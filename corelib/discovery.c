#include "internal/core.h"

#include <pthread.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

char *pg2sdr__strdup_serial(pg2sdr_context *ctx, libusb_device *usb_dev)
{
    char serial[33];
    memset(serial, 0, sizeof(serial));

    int usb_error;
    struct libusb_device_descriptor desc;
    (void) libusb_get_device_descriptor(usb_dev, &desc); /* always succeeds */

    if (!desc.iSerialNumber) {
        /* no serial number to get */
        return strdup("");
    }

    /* unfortunately, current libusb lacks a way to get string descriptors without opening the device
     * (though there seems to be some slow movement on an API for that upstream)
     */
    libusb_device_handle *handle = NULL;
    if ((usb_error = libusb_open(usb_dev, &handle)) != LIBUSB_SUCCESS) {
        if (ctx)
            pg2sdr__log(ctx, PG2SDR_LOG_ERROR, "warning: could not open device to fetch serial number: %s", libusb_strerror(usb_error));
        return strdup("");
    }

    if ((usb_error = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char *)serial, sizeof(serial))) < 0) {
        if (ctx)
            pg2sdr__log(ctx, PG2SDR_LOG_ERROR, "warning: could not fetch serial number descriptor: %s", libusb_strerror(usb_error));
        libusb_close(handle);
        return strdup("");
    }

    libusb_close(handle);
    return strdup(serial);
}

char *pg2sdr__strdup_ports(pg2sdr_context *ctx, libusb_device *usb_dev)
{
    char port_string[33];
    char *out = port_string;
    char *end = port_string + sizeof(port_string);

    uint8_t bus = libusb_get_bus_number(usb_dev);
    uint8_t ports[7];
    int port_count = libusb_get_port_numbers(usb_dev, ports, sizeof(ports));
    if (port_count < 1) {
        if (ctx)
            pg2sdr__log(ctx, PG2SDR_LOG_ERROR, "warning: could not fetch USB ports: %s", libusb_strerror(port_count));
        return strdup("");
    }

    out += snprintf(out, end - out, "%u-%u", bus, ports[0]);
    for (int i = 1; i < port_count; ++i)
        out += snprintf(out, end - out, ".%u", ports[i]);

    return strdup(port_string);
}

device_type_t pg2sdr__identify_device(libusb_device *lu_dev)
{
    struct libusb_device_descriptor desc;

    /* libusb docs: "Note since libusb-1.0.16, LIBUSBX_API_VERSION >= 0x01000102, this function always succeeds." */
    static_assert(LIBUSBX_API_VERSION >= 0x01000102);
    (void) libusb_get_device_descriptor(lu_dev, &desc);

    /* todo: once we have settled on a new, stable, VID/PID, make the old values
     * return DEVTYPE_LEGACY
     */
    if (desc.idVendor == VID_PG2SDR && desc.idProduct == PID_PG2SDR) {
        return DEVTYPE_PG2SDR;
    } else if (desc.idVendor == VID_ROM && desc.idProduct == PID_ROM) {
        return DEVTYPE_RECOVERY;
    } else {
        return DEVTYPE_OTHER;
    }
}

static bool prefix_match(const char *prefix, const char *candidate)
{
    size_t len = strlen(prefix);
    if (strlen(candidate) < len)
        return false;
    return !strncmp(prefix, candidate, len);
}

/* sorting function for discovery */
static int sort_devices(const void *l, const void *r)
{
    pg2sdr_usb_device *left = *((pg2sdr_usb_device **)l);
    pg2sdr_usb_device *right = *((pg2sdr_usb_device **)r);

    /* just order by serial */
    return strcmp(left->serial, right->serial);
}

ssize_t pg2sdr__discover_matching(pg2sdr_context *ctx,
                                  const char *match_serial_prefix,
                                  const char *match_ports,
                                  device_type_t match_types,
                                  pg2sdr_usb_device ***device_list)
{
    CHECK_CTX(ctx);

    if (!device_list)
        return PG2SDR_ERROR_BAD_ARGUMENT;

    libusb_device **lu_device_list = NULL;
    ssize_t device_count = libusb_get_device_list(ctx->libusb_ctx, &lu_device_list);
    if (device_count < 0)
        return pg2sdr__translate_libusb_error(device_count);

    int error = PG2SDR_SUCCESS;
    pg2sdr_usb_device **matched_device_list = NULL;

    /* Allocate +1 entry, pg2sdr_free_device_list expects a sentinel NULL at the end of the list */
    if (!(matched_device_list = calloc(device_count + 1, sizeof(*matched_device_list)))) {
        error = PG2SDR_ERROR_NO_MEMORY;
        goto failed;
    }

    int matched = 0;
    for (ssize_t i = 0; i < device_count; ++i) {
        libusb_device *lu_dev = lu_device_list[i];

        device_type_t type = pg2sdr__identify_device(lu_dev);
        if (!(type & match_types))
            continue;

        char *serial = pg2sdr__strdup_serial(ctx, lu_dev);
        char *ports = pg2sdr__strdup_ports(ctx, lu_dev);
        if (!serial || !ports) {
            free(serial);
            free(ports);
            error = PG2SDR_ERROR_NO_MEMORY;
            goto failed;
        }

        if ( (match_serial_prefix && !prefix_match(match_serial_prefix, serial)) ||
             (match_ports && strcmp(ports, match_ports) != 0) ) {
            free(serial);
            free(ports);
            continue;
        }

        pg2sdr_usb_device *pg2_dev;
        if (!(pg2_dev = calloc(sizeof(*pg2_dev), 1))) {
            free(serial);
            free(ports);
            error = PG2SDR_ERROR_NO_MEMORY;
            goto failed;
        }

        matched_device_list[matched++] = pg2_dev;
        pg2_dev->serial = serial;
        pg2_dev->ports = ports;
        pg2_dev->lu_device = libusb_ref_device(lu_dev);
    }

    if (matched > 1) {
        qsort(matched_device_list, matched, sizeof(*matched_device_list), sort_devices);
    }

    *device_list = matched_device_list;
    libusb_free_device_list(lu_device_list, /* unref devices */ 1);
    return matched;

failed:
    pg2sdr_free_device_list(matched_device_list);
    free(matched_device_list);
    libusb_free_device_list(lu_device_list, /* unref devices */ 1);
    return error;
}

void pg2sdr_free_device_list(pg2sdr_usb_device **device_list)
{
    if (!device_list)
        return;

    /* device_list has a NULL sentinel at the end (see pg2sdr__discover_matching) */
    for (int i = 0; device_list[i]; ++i) {
        if (!device_list[i])
            continue;
        if (device_list[i]->lu_device)
            libusb_unref_device(device_list[i]->lu_device);
        free((char*) device_list[i]->serial);
        free((char*) device_list[i]->ports);
        free(device_list[i]);
        device_list[i] = NULL;
    }

    free(device_list);
}
