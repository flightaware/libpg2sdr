/*
 *  blink.c - "pg2-util blink" subcommand
 *
 *  Copyright (c) 2026 FlightAware All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "internal/core.h"
#include "log.h"
#include "device.h"

static void show_blink_help();
int subcommand_blink(int argc, char * const argv[]);

static bool do_blink(const char *match_serial_prefix, const char *match_ports, uint32_t pattern);

static void show_blink_help()
{
    log_verbose("Usage: %s [OPTIONS] [PATTERN]\n"
                "Blink LED patterns on a ProStick Gen 2 device.\n"
                "\n"
                "By default, this will blink all LEDs at 2Hz so a device can be visually\n"
                "identified. Optionally an alternative patterns can be provided.\n"
                "The provided pattern overrides normal use of the LEDs until the pattern is\n"
                "disabled via --off or the device is reset.\n"
                "\n"
                "Pattern strings are [briefly!] up to 6, 4-character values which are cycled\n"
                "through in turn at 4Hz. The first characters represent the state of each of\n"
                "the 3 LEDs:\n"
                "\n"
                "  r - red LED on (bicolor LEDs only)\n"
                "  g - green LED on (bicolor LEDs only)\n"
                "  y - yellow LED on (red+green for bicolor LEDs)\n"
                "  anything else - LED off\n"
                "\n"
                "The 4th character is just for visual spacing and is ignored.\n"
                "The default 2Hz blink pattern is equivalent to 'yyy-000-'\n"
                "\n"
                "Available options:\n"
                "\n"
                " -h, --help             show this help\n"
                " -s, --serial <prefix>  specify serial number prefix of ProStick to affect\n"
                " -p, --port <bus-n.n.n> specify connected USB port of ProStick to affect\n"
                " -q, --quiet            suppress informational logging, show errors only\n"
                " -o, --off              disable any existing blink pattern\n",
                argv0);
}

static uint32_t parse_pattern(const char *pattern)
{
    if (strlen(pattern) % 4 != 0) {
        log_error("bad pattern: should be exactly a multiple of 4 characters long");
        return 0;
    }

    if (strlen(pattern) > 24) {
        log_error("bad pattern: should be no more than 24 characters long");
        return 0;
    }

    int len = strlen(pattern) / 4;
    uint32_t result = 0;
    for (int i = len-1; i >= 0; --i) {
        result <<= 5;

        /* RF LED */
        switch (pattern[i*4]) {
        case 'y': case 'Y':
            result |= 0x10;
            break;
        default:
            break;
        }

        /* Bicolor 1 */
        switch (pattern[i*4 + 1]) {
        case 'y': case 'Y':
            result |= 0x0C;
            break;
        case 'g': case 'G':
            result |= 0x08;
            break;
        case 'r': case 'R':
            result |= 0x04;
            break;
        default:
            break;
        }

        /* Bicolor 2 */
        switch (pattern[i*4 + 2]) {
        case 'y': case 'Y':
            result |= 0x03;
            break;
        case 'g': case 'G':
            result |= 0x02;
            break;
        case 'r': case 'R':
            result |= 0x01;
            break;
        default:
            break;
        }

        /* 4th character ignored */
    }

    if (!result) {
        log_error("bad pattern: must have at least one LED on (use --off to disable pattern entirely)");
        return 0;
    }

    /* rotate result so the topmost bits we care about, aren't zero */
    unsigned shift = (len-1)*5;
    uint32_t top_mask = 0x1F << shift;
    while (!(result & top_mask))
        result = (result >> 5) | ((result & 0x1F) << shift);

    return result;
}

int subcommand_blink(int argc, char * const argv[])
{
    struct option opts[] = {
        { "serial",  required_argument, 0, 's' },
        { "port",    required_argument, 0, 'p' },
        { "help",    no_argument,       0, 'h' },
        { "quiet",   no_argument,       0, 'q' },
        { "off",     no_argument,       0, 'o' },
        { 0, 0, 0, 0 }
    };

    const char *serial_prefix = NULL;
    const char *port_path = NULL;

    int opt;
    uint32_t pattern = parse_pattern("yyy-000-");
    while ((opt = getopt_long(argc, argv, "s:p:hwqo", opts, NULL)) != -1) {
        switch (opt) {
        case 's':
            serial_prefix = optarg;
            break;

        case 'p':
            port_path = optarg;
            break;

        case 'h':
            show_blink_help(argv[0]);
            return EXIT_SUCCESS;

        case 'q':
            verbose_logging = false;
            break;

        case 'o':
            pattern = 0;
            break;

        case '?':
            return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        if (!(pattern = parse_pattern(argv[optind])))
            return EXIT_FAILURE;
    }

    if (optind + 1 < argc) {
        log_error("unexpected trailing arguments");
        return EXIT_FAILURE;
    }

    return do_blink(serial_prefix, port_path, pattern) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool do_blink(const char *match_serial_prefix, const char *match_ports, uint32_t pattern)
{
    libusb_device *dev = NULL;
    bool success = false;
    libusb_device_handle *handle = NULL;

    dev = device_search(match_serial_prefix, match_ports, SEARCH_PG2SDR);
    if (!dev) {
        log_error("no suitable USB device found");
        goto cleanup;
    }

    if (!(handle = device_open(dev, false)))
        goto cleanup;

    log_verbose("Setting blink pattern to 0x%08x", pattern);

    int pg2_error;
    if ((pg2_error = pg2sdr__ctrl_led_pattern(handle, pattern, 00)) < 0) {
        log_perror_pg2sdr(pg2_error, "Failed to send LED_PATTERN command");
        goto cleanup;
    }

    success = true;

 cleanup:
    if (handle)
        device_close(handle);
    if (dev)
        libusb_unref_device(dev);

    return success;
}
