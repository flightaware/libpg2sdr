#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "log.h"
#include "device.h"
#include "nanojson.h"
#include "meta.h"

static void show_device_info_help();
int subcommand_device_info(int argc, char * const argv[]);

static bool do_device_info(const char *serial_prefix, const char *port_path, bool json_output);

static bool show_device_info(libusb_device *dev, bool json_output);
static void show_port_metadata(port_metadata_t *meta);
static void show_firmware_metadata(firmware_metadata_t *meta);

static void json_port_metadata(port_metadata_t *meta);
static void json_firmware_metadata(firmware_metadata_t *metadata);

static void show_device_info_help()
{
    log_verbose("Usage: %s [OPTIONS]\n"
                "This subcommand gathers information on the active and loaded firmware for\n"
                "connected ProStick Gen 2 devices, and writes a summary to stdout.\n"
                "By default, information on all connected devices will be shown. To get info\n"
                "on a specific device only, use the '-s' or '-p' options.\n"
                "\n"
                "Available options:\n"
                "\n"
                " -h, --help             show this help\n"
                " -s, --serial <prefix>  specify serial number prefix of ProStick to affect\n"
                " -p, --port <bus-n.n.n> specify connected USB port of ProStick to affect\n"
                " -q, --quiet            suppress informational logging, show errors only"
                " -j, --json             output machine-readable json to stdout",
                argv0);
}

int subcommand_device_info(int argc, char * const argv[])
{
    struct option opts[] = {
        { "serial", required_argument, 0, 's' },
        { "port",   required_argument, 0, 'p' },
        { "help",   no_argument,       0, 'h' },
        { "quiet",  no_argument,       0, 'q' },
        { "json",   no_argument,       0, 'j' },
        { 0, 0, 0, 0 }
    };

    const char *serial_prefix = NULL;
    const char *port_path = NULL;
    bool json_output = false;

    int opt;
    while ((opt = getopt_long(argc, argv, "s:p:hqj", opts, NULL)) != -1) {
        switch (opt) {
        case 's':
            serial_prefix = optarg;
            break;

        case 'p':
            port_path = optarg;
            break;

        case 'h':
            show_device_info_help(argv[0]);
            return EXIT_SUCCESS;

        case 'q':
            verbose_logging = false;
            break;

        case 'j':
            json_output = true;
            break;

        case '?':
            return EXIT_FAILURE;
        }
    }

    if (optind + 1 < argc) {
        log_error("did not expect any non-option arguments");
        return EXIT_FAILURE;
    }

    return do_device_info(serial_prefix, port_path, json_output) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool do_device_info(const char *serial_prefix, const char *port_path, bool json_output)
{
    /* scan for suitable devices and call show_device_info on each */

    int usb_error;
    if ((usb_error = libusb_init(NULL)) < 0) {
        log_perror_libusb(usb_error, "libusb_init");
        return false;
    }

    libusb_device **devlist = NULL;
    ssize_t count = libusb_get_device_list(NULL, &devlist);
    if (count < 0) {
        log_perror_libusb(count, "libusb_get_device_list");
        return false;
    }

    if (json_output) {
        json_set_output(stdout);
        json_start_array();
    }

    unsigned found = 0;
    bool errors = false;
    for (size_t i = 0; i < count; ++i) {
        libusb_device *dev = devlist[i];

        struct libusb_device_descriptor ddesc;
        (void) libusb_get_device_descriptor(dev, &ddesc); /* always succeeds */

        if (port_path) {
            if (strcmp(port_path, device_ports(dev)) != 0) {
                /* port doesn't match */
                continue;
            }
        }

        if (serial_prefix) {
            /* only consider serial numbers on PG2 devices with loaded firmware */
            if (!device_is_pg2(dev))
                continue;

            const char *serial = device_serial(dev);
            if (!serial)
                continue;

            if (strlen(serial_prefix) > strlen(serial) ||
                strncmp(serial, serial_prefix, strlen(serial_prefix)) != 0) {
                /* serial prefix doesn't match */
                continue;
            }
        }

        /* if neither option was given, match anything that looks suitable */
        if (!port_path && !serial_prefix) {
            if (!device_is_pg2(dev) && !device_is_dfu(dev))
                continue;
        }

        /* suitable device, process it */
        if (!show_device_info(dev, json_output))
            errors = true;
        else
            ++found;
    }

    if (json_output) {
        json_end_array();
        fprintf(stdout, "\n");
    } else {
        if (!found)
            log_verbose("No matching devices found");
        else
            log_verbose("%u matching device%s found", found, (found == 1) ? "" : "s");
    }

    libusb_free_device_list(devlist, /* unref_devices */ 1);
    return !errors;
}

static bool show_device_info(libusb_device *dev, bool json_output)
{
    port_metadata_t *meta = meta_query(dev);
    if (!meta)
        return false;

    if (json_output) {
        json_port_metadata(meta);
    } else {
        show_port_metadata(meta);
    }

    meta_free(meta);
    return true;
}

static void show_port_metadata(port_metadata_t *meta)
{
    fprintf(stdout, "Port %s:\n", meta->port);
    fprintf(stdout, "  Device type:          %s\n",
            (meta->device_type == DEVICE_DFU) ? "ProStick Gen 2 (recovery mode)" :
            (meta->device_type == DEVICE_PG2) ? "ProStick Gen 2" :
            "Non-ProStick Gen 2 device");

    if (meta->serial)
        fprintf(stdout, "  Serial number:        %s\n", meta->serial);

    if (meta->active_firmware_valid) {
        fprintf(stdout, "  Active firmware:\n");
        show_firmware_metadata(&meta->active_firmware);
    }

    if (meta->flash_image) {
        fprintf(stdout, "  Flash firmware:\n");
        show_firmware_metadata(&meta->flash_image->metadata);
        fprintf(stdout,
                "    Total image size:   %u bytes\n"
                "    DFU release number: %04x\n"
                "    DFU CRC:            %08x\n",
                meta->flash_image->image_size,
                meta->flash_image->dfu_release,
                meta->flash_image->dfu_crc);
    }
}

static void show_firmware_metadata(firmware_metadata_t *metadata)
{
    if (!metadata->version)
        return;
    fprintf(stdout, "    Version:            %u.%u.%u.%u\n",
            ((metadata->version >> 24) & 0xFF),
            ((metadata->version >> 16) & 0xFF),
            ((metadata->version >> 8) & 0xFF),
            ((metadata->version >> 0) & 0xFF));
    fprintf(stdout, "    Compat:             %u.%u.%u.%u\n",
            ((metadata->compat >> 24) & 0xFF),
            ((metadata->compat >> 16) & 0xFF),
            ((metadata->compat >> 8) & 0xFF),
            ((metadata->compat >> 0) & 0xFF));
    fprintf(stdout, "    Max control xfer:   %u bytes\n", metadata->max_control_transfer);
    fprintf(stdout, "    Control timeout:    %u ms\n", metadata->control_timeout_ms);
    fprintf(stdout, "    Build type:         %s\n", metadata->build_type);
}

static void json_port_metadata(port_metadata_t *meta)
{
    json_start_object();
    json_key("port"); json_string(meta->port);
    json_key("type"); json_string((meta->device_type == DEVICE_DFU) ? "dfu" :
                                  (meta->device_type == DEVICE_PG2) ? "pg2sdr" :
                                  "other");
    if (meta->serial) {
        json_key("serial"); json_string(meta->serial);
    }
    if (meta->active_firmware_valid) {
        json_key("active");
        json_start_object();
        json_firmware_metadata(&meta->active_firmware);
        json_end_object();
    }
    if (meta->flash_image) {
        json_key("flash");
        json_start_object();
        json_firmware_metadata(&meta->flash_image->metadata);
        json_key("image_size"); json_number(meta->flash_image->image_size);
        json_key("dfu_release"); json_string_fmt("%04x", meta->flash_image->dfu_release);
        json_key("dfu_crc"); json_string_fmt("%08x", meta->flash_image->dfu_crc);
        json_end_object();
    }
    json_end_object();
}

static void json_firmware_metadata(firmware_metadata_t *metadata)
{
    if (!metadata->version)
        return;
    json_key("version"); json_string_fmt("%u.%u.%u.%u",
                                         ((metadata->version >> 24) & 0xFF),
                                         ((metadata->version >> 16) & 0xFF),
                                         ((metadata->version >> 8) & 0xFF),
                                         ((metadata->version >> 0) & 0xFF));
    json_key("compat"); json_string_fmt("%u.%u.%u.%u",
                                        ((metadata->compat >> 24) & 0xFF),
                                        ((metadata->compat >> 16) & 0xFF),
                                        ((metadata->compat >> 8) & 0xFF),
                                        ((metadata->compat >> 0) & 0xFF));
    json_key("max_control_transfer"); json_number(metadata->max_control_transfer);
    json_key("control_timeout_ms"); json_number(metadata->control_timeout_ms);
    json_key("build_type"); json_string(metadata->build_type);
}
