#include "internal/core.h"
#include "log.h"
#include "device.h"
#include "nanojson.h"
#include "meta.h"

#include <stdlib.h>
#include <string.h>
#include <getopt.h>

static void show_device_info_help();
int subcommand_device_info(int argc, char * const argv[]);

static bool do_device_info(const char *serial_prefix, const char *port_path, bool json_output);

static bool show_device_info(libusb_device *dev, bool json_output);
static void show_port_metadata(port_metadata_t *meta);
static void json_port_metadata(port_metadata_t *meta);

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
                " -q, --quiet            suppress informational logging, show errors only\n"
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

    if (optind < argc) {
        log_error("did not expect any non-option arguments");
        return EXIT_FAILURE;
    }

    return do_device_info(serial_prefix, port_path, json_output) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool do_device_info(const char *match_serial_prefix, const char *match_ports, bool json_output)
{
    /* scan for suitable devices and call show_device_info on each */

    pg2sdr_usb_device **devices = NULL;
    ssize_t device_count;
    if ((device_count = pg2sdr__discover_matching(shared_pg2sdr_ctx, match_serial_prefix, match_ports,
                                                  DEVTYPE_PG2SDR|DEVTYPE_LEGACY|DEVTYPE_RECOVERY,
                                                  &devices)) < 0) {
        log_perror_pg2sdr(device_count, "could not enumerate USB devices");
        return false;
    }

    if (json_output) {
        json_set_output(stdout);
        json_start_array();
    }

    unsigned found = 0;
    bool errors = false;
    for (size_t i = 0; i < device_count; ++i) {
        if (!show_device_info(devices[i]->lu_device, json_output))
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

    pg2sdr_free_device_list(devices);
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

    const char *typestr;
    switch (meta->device_type) {
    case DEVTYPE_PG2SDR:   typestr = "ProStick Gen 2"; break;
    case DEVTYPE_LEGACY:   typestr = "ProStick Gen 2 (legacy VID/PID)"; break;
    case DEVTYPE_RECOVERY: typestr = "ProStick Gen 2 (recovery mode)"; break;
    default:               typestr = "Non-ProStick Gen 2 device"; break;
    }

    fprintf(stdout, "  Device type:          %s\n", typestr);

    if (meta->serial)
        fprintf(stdout, "  Serial number:        %s\n",
                meta->serial);

    if (meta->status_valid) {
        fprintf(stdout, "  Recovery switch:      %s\n",
                meta->recovery_switch_on ? "recovery mode" : "normal");
        fprintf(stdout, "  RF power:             %s\n",
                meta->rf_power_on ? "on" : "off");
        fprintf(stdout, "  ADC data stream:      %s\n",
                meta->adc_on ? "active" : "not active");
        fprintf(stdout, "  Hardware type:        %s\n",
                meta->hw_type);
    }

    if (meta->active_firmware_valid) {
        fprintf(stdout, "  Active firmware:\n");
        show_firmware_metadata("    ", &meta->active_firmware, stdout);
    }

    if (meta->flash_image) {
        fprintf(stdout, "  Flash firmware:\n");
        show_firmware_image("    ", meta->flash_image, stdout);
    }
}

static void json_port_metadata(port_metadata_t *meta)
{
    json_start_object();
    json_key("port"); json_string(meta->port);

    const char *typestr;
    switch (meta->device_type) {
    case DEVTYPE_PG2SDR:   typestr = "pg2sdr"; break;
    case DEVTYPE_LEGACY:   typestr = "legacy"; break;
    case DEVTYPE_RECOVERY: typestr = "recovery"; break;
    default:               typestr = "other"; break;
    }

    json_key("type"); json_string(typestr);

    if (meta->serial) {
        json_key("serial"); json_string(meta->serial);
    }

    if (meta->status_valid) {
        json_key("recovery_switch_on"); json_bool(meta->recovery_switch_on);
        json_key("rf_power_on"); json_bool(meta->rf_power_on);
        json_key("adc_on"); json_bool(meta->adc_on);
        json_key("hw_type"); json_string(meta->hw_type);
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
        json_firmware_image(meta->flash_image);
        json_end_object();
    }
    json_end_object();
}
