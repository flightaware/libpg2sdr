#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "internal/core.h"
#include "log.h"
#include "device.h"
#include "hotplug.h"

static void show_reset_help();
int subcommand_reset(int argc, char * const argv[]);

static bool do_reset(const char *match_serial_prefix, const char *match_ports, bool wait);

static void show_reset_help()
{
    log_verbose("Usage: %s [OPTIONS]\n"
                "Reset and reinitialize a ProStick Gen 2 device. The ProStick will attempt\n"
                "to load firmware from flash, or enter recovery mode.\n"
                "\n"
                "Available options:\n"
                "\n"
                " -h, --help             show this help\n"
                " -s, --serial <prefix>  specify serial number prefix of ProStick to affect\n"
                " -p, --port <bus-n.n.n> specify connected USB port of ProStick to affect\n"
                " -q, --quiet            suppress informational logging, show errors only\n"
                " -w, --wait             wait for reset process to complete before returning",
                argv0);
}

int subcommand_reset(int argc, char * const argv[])
{
    struct option opts[] = {
        { "serial", required_argument, 0, 's' },
        { "port",   required_argument, 0, 'p' },
        { "help",   no_argument,       0, 'h' },
        { "quiet",  no_argument,       0, 'q' },
        { "wait",   no_argument,       0, 'w' },
        { 0, 0, 0, 0 }
    };

    const char *serial_prefix = NULL;
    const char *port_path = NULL;
    bool wait = false;

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
            show_reset_help(argv[0]);
            return EXIT_SUCCESS;

        case 'q':
            verbose_logging = false;
            break;

        case 'w':
            wait = true;
            break;

        case '?':
            return EXIT_FAILURE;
        }
    }

    if (optind + 1 < argc) {
        log_error("unexpected trailing arguments");
        return EXIT_FAILURE;
    }

    return do_reset(serial_prefix, port_path, wait) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool do_reset(const char *match_serial_prefix, const char *match_ports, bool wait)
{
    libusb_device *dev = NULL;
    bool success = false;
    firmware_hotplug_state *hotplug_state = NULL;
    libusb_device_handle *handle = NULL;
    libusb_device *post_hotplug = NULL;

    dev = device_search(match_serial_prefix, match_ports, SEARCH_PG2SDR);
    if (!dev) {
        log_error("no suitable USB device found");
        goto cleanup;
    }

    if (!(handle = device_open(dev, false)))
        goto cleanup;

    if (wait) {
        /* set up the hotplug callback now, before triggering the new firmware */
        if (!(hotplug_state = hotplug_prepare(dev)))
            goto cleanup;
    }

    /* send reset command */
    log_verbose("Resetting device on port %s: %s", device_ports(dev), device_string(dev));

    int pg2_error;
    if ((pg2_error = pg2sdr__ctrl_reset(handle, 0)) < 0) {
        log_perror_pg2sdr(pg2_error, "Failed to send RESET command");
        goto cleanup;
    }

    /* done with the old device now */
    libusb_close(handle);
    handle = NULL;

    if (wait) {
        /* wait for the new device to appear */
        post_hotplug = hotplug_await(hotplug_state);
        success = (post_hotplug != NULL);
    } else {
        /* don't wait, just assume success */
        success = true;
    }

 cleanup:
    if (handle)
        device_close(handle);
    if (post_hotplug)
        libusb_unref_device(post_hotplug);
    if (hotplug_state)
        hotplug_cleanup(hotplug_state);
    if (dev)
        libusb_unref_device(dev);

    return success;
}
