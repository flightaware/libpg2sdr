#include "device.h"

#include <string.h>
#include <stdio.h>
#include "log.h"

//
// USB device helpers
//

/* a one-entry cache, so we don't keep calling libusb_open */
static libusb_device *cache_dev = NULL;            /* device we're caching for (has a reference) */
static libusb_device_handle *cache_handle = NULL;  /* opened/configured handle for cache_dev */
static bool cache_handle_in_use = false;           /* true if cache_handle is currently in use (i.e. still waiting for a call to device_close) */

libusb_device_handle *device_open(libusb_device *dev, bool claim_interface)
{
    int usb_error;
    libusb_device_handle *handle = NULL;

    if (cache_handle != NULL && !cache_handle_in_use && cache_dev == dev) {
        cache_handle_in_use = true;
        handle = cache_handle;
    } else {
        if ((usb_error = libusb_open(dev, &handle)) < 0) {
            log_perror_libusb(usb_error, "libusb_open");
            goto error;
        }
    }

    int config;
    if ((usb_error = libusb_get_configuration(handle, &config)) < 0) {
        log_perror_libusb(usb_error, "libusb_get_configuration");
        goto error;
    }

    if (config == 0) {
        if ((usb_error = libusb_set_configuration(handle, 1) < 0)) {
            log_perror_libusb(usb_error, "libusb_set_configuration(1)");
            goto error;
        }
    }

    if (claim_interface) {
        if ((usb_error = libusb_claim_interface(handle, 0)) < 0) {
            log_perror_libusb(usb_error, "libusb_claim_interface(0)");
            goto error;
        }
    }

    if (cache_handle != NULL) {
        if (!cache_handle_in_use) /* cache entry is idle */
            libusb_close(cache_handle);

        /* we discard the cache regardless. If the cached handle was actually
         * in use, it stays open and will be closed by device_close() later.
         */
        cache_handle_in_use = false;
        cache_handle = NULL;
    }

    if (cache_dev) {
        libusb_unref_device(cache_dev);
        cache_dev = NULL;
    }

    if (!cache_handle) {
        cache_dev = libusb_ref_device(dev);
        cache_handle = handle;
        cache_handle_in_use = true;
    }

    return handle;

 error:
    if (handle)
        libusb_close(handle);
    return NULL;
}

void device_close(libusb_device_handle *handle)
{
    if (cache_handle_in_use && handle == cache_handle) {
        cache_handle_in_use = false;
        return;
    }

    libusb_close(handle);
}

const char *device_serial(libusb_device *dev)
{
    static char buf[33];

    struct libusb_device_descriptor ddesc;
    (void) libusb_get_device_descriptor(dev, &ddesc); /* always succeeds */

    if (!ddesc.iSerialNumber) {
        /* no serial number to get */
        return NULL;
    }

    memset(buf, 0, sizeof(buf));

    /* unfortunately, current libusb lacks a way to get string descriptors without opening the device
     * (though there seems to be some slow movement on an API for that upstream)
     */
    libusb_device_handle *handle = device_open(dev, false);
    if (!handle)
        return NULL;

    int usb_error;
    if ((usb_error = libusb_get_string_descriptor_ascii(handle, ddesc.iSerialNumber, (unsigned char *)buf, sizeof(buf))) < 0) {
        libusb_close(handle);
        log_perror_libusb(usb_error, "failed to read device serial number: libusb_get_string_descriptor_ascii(%d)", ddesc.iSerialNumber);
        return NULL;
    }

    libusb_close(handle);
    return buf;
}

bool device_is_pg2(libusb_device *dev)
{
    struct libusb_device_descriptor ddesc;
    (void) libusb_get_device_descriptor(dev, &ddesc); /* always succeeds */
    return (ddesc.idVendor == VID_PG2SDR && ddesc.idProduct == PID_PG2SDR);
}

bool device_is_dfu(libusb_device *dev)
{
    struct libusb_device_descriptor ddesc;
    (void) libusb_get_device_descriptor(dev, &ddesc); /* always succeeds */
    return (ddesc.idVendor == VID_LPC_ROM && ddesc.idProduct == PID_LPC_ROM);
}

const char *device_string(libusb_device *dev)
{
    static char buf[128];

    if (device_is_pg2(dev)) {
        const char *serial = device_serial(dev);
        snprintf(buf, sizeof(buf), "ProStick Gen 2 serial %s",
                 serial ? serial : "<unknown>");
    } else if (device_is_dfu(dev)) {
        snprintf(buf, sizeof(buf), "ProStick Gen 2 or other LPC device in recovery mode");
    } else {
        struct libusb_device_descriptor ddesc;
        (void) libusb_get_device_descriptor(dev, &ddesc); /* always succeeds */

        snprintf(buf, sizeof(buf), "unrecognized device %04x:%04x",
                 ddesc.idVendor, ddesc.idProduct);
    }

    return buf;
}

libusb_device *device_search(const char *serial_prefix, const char *port_path, unsigned flags)
{
    const bool wants_pg2 = (flags & SEARCH_PG2);
    const bool wants_dfu = (flags & SEARCH_DFU);

    int usb_error;
    if ((usb_error = libusb_init(NULL)) < 0) {
        log_perror_libusb(usb_error, "libusb_init");
        return NULL;
    }

    libusb_device **devlist = NULL;
    ssize_t count = libusb_get_device_list(NULL, &devlist);
    if (count < 0) {
        log_perror_libusb(count, "libusb_get_device_list");
        return NULL;
    }

    libusb_device *found = NULL;
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
            if (!(device_is_pg2(dev) && wants_pg2) &&
                !(device_is_dfu(dev) && wants_dfu))
                continue;
        }

        if (found) {
            /* >1 match, bail out */
            log_error("more than one suitable device was found:");
            log_verbose("  port %s: %s", device_ports(found), device_string(found));
            log_verbose("  port %s: %s", device_ports(dev), device_string(dev));
            log_verbose("Use -s and -p options to select a single device");
            libusb_unref_device(found);
            found = NULL;
            break;
        }

        found = libusb_ref_device(dev);
    }

    /* if we found something, verify that it's the right device type */
    if (found) {
        if (!(device_is_pg2(found) && wants_pg2) &&
            !(device_is_dfu(found) && wants_dfu)) {
            const char *expected =
                (wants_pg2 && wants_dfu) ? "ProStick Gen 2" :
                (wants_pg2) ? "ProStick Gen 2 with active firmware" :
                (wants_dfu) ? "ProStick Gen 2 in recovery mode" :
                "nothing at all (this is a bug)";

            log_error("Expected a %s on port %s, but found: %s",
                      expected,
                      device_ports(found),
                      device_string(found));

            libusb_unref_device(found);
            found = NULL;
        }
    }

    /* cleanup */
    libusb_free_device_list(devlist, /* unref_devices */ 1);
    return found;
}

const char *device_ports(libusb_device *dev)
{
    static char buf[64];

    uint8_t bus = libusb_get_bus_number(dev);

    uint8_t ports[7];
    int count = libusb_get_port_numbers(dev, ports, sizeof(ports));
    if (count <= 0)
        return "<error>";

    char *out = buf;
    char *end = buf + sizeof(buf);
    out += snprintf(out, end-out, "%u-%u", bus, ports[0]);
    for (size_t i = 1; i < count; ++i)
        out += snprintf(out, end-out, ".%u", ports[i]);

    return buf;
}

