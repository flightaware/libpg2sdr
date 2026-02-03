#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>

#include "log.h"
#include "device.h"
#include "io.h"
#include "image.h"
#include "dfu_load.h"
#include "mem_load.h"

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

    dev = device_search(serial_prefix, port_path, SEARCH_DFU | SEARCH_PG2);
    if (!dev) {
        log_error("no suitable USB device found");
        goto cleanup;
    }

    log_verbose("Loading firmware to device on port %s: %s", device_ports(dev), device_string(dev));

    if (device_is_dfu(dev)) {
        success = dfu_load(image, dev, NULL);
    } else if (device_is_pg2(dev)) {
        success = mem_load(image, dev, NULL);
    } else {
        log_error("device at %s does not seem to be a ProStick Gen 2",
                  device_ports(dev));
        success = false;
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

