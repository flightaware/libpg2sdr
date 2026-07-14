/*
 *  load.c - "pg2-util load-firmware" subcommand
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

#include "internal/core.h"
#include "log.h"
#include "device.h"
#include "io.h"
#include "image.h"
#include "dfu_load.h"
#include "mem_load.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>

static bool do_load(const char *image_path, const char *serial_prefix, const char *port_path);
static void show_load_help();
int subcommand_load(int argc, char * const argv[]);

static void show_load_help()
{
    log_verbose("Usage: %s [OPTIONS] FIRMWARE-IMAGE\n"
                "This subcommand will load and start a new firmware image on a ProStick Gen 2.\n"
                "Firmware can be loaded in recovery mode, or while running normal firmware.\n"
                "Loading new firmware does not affect the firmware image stored in flash,\n"
                "and when the device is next reset or disconnected, it will return to using\n"
                "the firmware stored on flash (or entering recovery mode).\n"
                "\n"
                "Available options:\n"
                "\n"
                " -h, --help             show this help\n"
                " -s, --serial <prefix>  specify serial number prefix of ProStick to affect\n"
                " -p, --port <bus-n.n.n> specify connected USB port of ProStick to affect\n"
                " -q, --quiet            suppress informational logging, show errors only",
                argv0);
}

int subcommand_load(int argc, char * const argv[])
{
    struct option opts[] = {
        { "serial", required_argument, 0, 's' },
        { "port",   required_argument, 0, 'p' },
        { "help",   no_argument,       0, 'h' },
        { "quiet",  no_argument,       0, 'q' },
        { 0, 0, 0, 0 }
    };

    const char *serial_prefix = NULL;
    const char *port_path = NULL;

    int opt;
    while ((opt = getopt_long(argc, argv, "s:p:hq", opts, NULL)) != -1) {
        switch (opt) {
        case 's':
            serial_prefix = optarg;
            break;

        case 'p':
            port_path = optarg;
            break;

        case 'h':
            show_load_help(argv[0]);
            return EXIT_SUCCESS;

        case 'q':
            verbose_logging = false;
            break;

        case '?':
            return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        log_error("a firmware image filename is required");
        return EXIT_FAILURE;
    }

    if (optind + 1 < argc) {
        log_error("only one firmware image filename is expected");
        return EXIT_FAILURE;
    }

    return do_load(argv[optind], serial_prefix, port_path) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool do_load(const char *image_path, const char *serial_prefix, const char *port_path)
{
    firmware_io_t *io = NULL;
    firmware_image_t *image = NULL;
    libusb_device *dev = NULL;
    bool success = false;

    if (!(io = io_open_file(image_path)))
        goto cleanup;

    if (!(image = image_read(io)))
        goto cleanup;

    dev = device_search(serial_prefix, port_path, SEARCH_PG2SDR | SEARCH_RECOVERY);
    if (!dev) {
        log_error("no suitable USB device found");
        goto cleanup;
    }

    log_verbose("Loading firmware to device on port %s: %s", device_ports(dev), device_string(dev));

    switch (pg2sdr__identify_device(dev)) {
    case DEVTYPE_RECOVERY:
        /* patch boot_mode to indicate use of DFU / recovery mode */
        image_patch_boot_mode(image, BOOT_MODE_RECOVERY);
        success = dfu_load(image, dev, NULL);
        break;
    case DEVTYPE_PG2SDR:
    case DEVTYPE_AIRSPYMINI:
    case DEVTYPE_PROTOTYPE:
        /* patch boot_mode to indicate use of LOAD_IMAGE */
        image_patch_boot_mode(image, BOOT_MODE_LOAD_IMAGE);
        success = mem_load(image, dev, NULL);
        break;
    default:
        log_error("device does not seem to be a ProStick Gen 2");
        success = false;
        break;
    }

 cleanup:
    if (dev)
        libusb_unref_device(dev);
    if (image)
        image_free(image);
    if (io)
        io->close(io);

    return success;
}

