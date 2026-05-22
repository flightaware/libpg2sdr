/*
 *  standby.c - "pg2-util standby" subcommand
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
#include "hotplug.h"

static void show_standby_help();
int subcommand_standby(int argc, char * const argv[]);

static bool do_standby(const char *match_serial_prefix, const char *match_ports);

static void show_standby_help()
{
    log_verbose("Usage: %s [OPTIONS]\n"
                "Put a ProStick Gen 2 device into standby mode. This will power off the\n"
                "RF section of the device, and halt any data streaming.\n"
                "\n"
                "Normally, a device goes into standby automatically when a SDR program\n"
                "on the host stops using the device. However, if the SDR program does not\n"
                "correctly close the device before exiting, the ProStick can be left in\n"
                "active mode. This 'standby' command can be used to clean up if that happens.\n"
                "\n"
                "Available options:\n"
                "\n"
                " -h, --help             show this help\n"
                " -s, --serial <prefix>  specify serial number prefix of ProStick to affect\n"
                " -p, --port <bus-n.n.n> specify connected USB port of ProStick to affect\n"
                " -q, --quiet            suppress informational logging, show errors only\n",
                argv0);
}

int subcommand_standby(int argc, char * const argv[])
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
    while ((opt = getopt_long(argc, argv, "s:p:hwq", opts, NULL)) != -1) {
        switch (opt) {
        case 's':
            serial_prefix = optarg;
            break;

        case 'p':
            port_path = optarg;
            break;

        case 'h':
            show_standby_help(argv[0]);
            return EXIT_SUCCESS;

        case 'q':
            verbose_logging = false;
            break;

        case '?':
            return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        log_error("did not expect any non-option arguments");
        return EXIT_FAILURE;
    }

    return do_standby(serial_prefix, port_path) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool do_standby(const char *match_serial_prefix, const char *match_ports)
{
    libusb_device *dev = NULL;
    bool success = false;
    libusb_device_handle *handle = NULL;

    dev = device_search(match_serial_prefix, match_ports, SEARCH_PG2SDR);
    if (!dev) {
        log_error("no suitable USB device found");
        goto cleanup;
    }

    /* Do claim the interface here, because we don't want to actually put anything into
     * standby if there's another process actively using the device.
     */
    if (!(handle = device_open(dev, true)))
        goto cleanup;

    /* stop transfers, power off RF stage */
    log_verbose("Putting device into standby on port %s: %s", device_ports(dev), device_string(dev));

    success = true;

    /* try both, even if one fails */

    int pg2_error;
    if ((pg2_error = pg2sdr__ctrl_stop_transfer(handle, 0)) < 0) {
        log_perror_pg2sdr(pg2_error, "Failed to send STOP_TRANSFER command");
        success = false;
    }

    if ((pg2_error = pg2sdr__ctrl_set_rf_power(handle, RF_POWER_OFF, 0)) < 0) {
        log_perror_pg2sdr(pg2_error, "Failed to send RF_POWER_OFF command");
        success = false;
    }

 cleanup:
    if (handle)
        device_close(handle);
    if (dev)
        libusb_unref_device(dev);

    return success;
}
