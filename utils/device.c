#include "device.h"

#include <string.h>
#include <stdio.h>
#include "log.h"

#include "pg2sdr.h"
#include "internal/device.h"

//
// USB device helpers
//

/* a one-entry cache, so we don't keep calling libusb_open */
static libusb_device *cache_dev = NULL;            /* device we're caching for (has a reference) */
static libusb_device_handle *cache_handle = NULL;  /* opened/configured handle for cache_dev */
static bool cache_handle_in_use = false;           /* true if cache_handle is currently in use (i.e. still waiting for a call to device_close) */

/* a shared pg2sdr_context */
pg2sdr_context *shared_ctx = NULL;

static void log_callback(pg2sdr_context *context,
                         pg2sdr_log_level level,
                         const char *message)
{
    if (level <= PG2SDR_LOG_DEBUG)
        return;

    if (level <= PG2SDR_LOG_INFO)
        log_verbose("libpg2sdr: %s", message);
    else
        log_error("libpg2sdr: %s", message);
}

int setup_shared_ctx()
{
    if (!shared_ctx) {
        int error;
        pg2sdr_context *ctx = NULL;
        if ((error = pg2sdr_init(&ctx)) < 0) {
            log_perror_pg2sdr(error, "pg2sdr_init");
            return error;
        }

        if ((error = pg2sdr_set_log_callback(ctx, log_callback)) < 0) {
            log_perror_pg2sdr(error, "pg2sdr_set_log_callback");
            pg2sdr_exit(ctx);
            return error;
        }

        shared_ctx = ctx;
    }

    return PG2SDR_SUCCESS;
}

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

const char *device_ports(libusb_device *dev)
{
    static char *ports = NULL;

    if (setup_shared_ctx() < 0)
        return "<error>";

    free(ports);
    ports = pg2sdr__strdup_ports(shared_ctx, dev);
    return ports;
}

const char *device_serial(libusb_device *dev)
{
    static char *serial = NULL;

    if (setup_shared_ctx() < 0)
        return "<error>";

    free(serial);
    serial = pg2sdr__strdup_serial(shared_ctx, dev);
    return serial;
}

const char *device_string(libusb_device *dev)
{
    static char buf[128];

    int type = pg2sdr__identify_device(dev);
    switch (type) {
    case DEVTYPE_PG2SDR:
        {
            char *serial = pg2sdr__strdup_serial(NULL, dev);
            snprintf(buf, sizeof(buf), "ProStick Gen 2 serial %s",
                     serial ? serial : "<unknown>");
            free(serial);
            return buf;
        }

    case DEVTYPE_LEGACY:
        {
            char *serial = pg2sdr__strdup_serial(NULL, dev);
            snprintf(buf, sizeof(buf), "ProStick Gen 2 (legacy VID/PID) serial %s",
                     serial ? serial : "<unknown>");
            free(serial);
            return buf;
        }

    case DEVTYPE_RECOVERY:
        return "ProStick Gen 2 or other LPC device in recovery mode";

    default:
        {
            struct libusb_device_descriptor ddesc;
            (void) libusb_get_device_descriptor(dev, &ddesc); /* always succeeds */

            snprintf(buf, sizeof(buf), "unrecognized device %04x:%04x",
                     ddesc.idVendor, ddesc.idProduct);
            return buf;
        }
    }
}

libusb_device *device_search(const char *match_serial_prefix, const char *match_ports, unsigned flags)
{
    libusb_device *found = NULL;
    pg2sdr_context *ctx = NULL;     /* temporary context just for the duration of this function */
    pg2sdr_usb_device **devices = NULL;
    int error;

    if ((error = setup_shared_ctx()) < 0)
        goto cleanup;

    /* bitmask of DEVTYPE_* device types we are interested in */
    const int typemask =
        ((flags & SEARCH_PG2SDR) ? (DEVTYPE_PG2SDR | DEVTYPE_LEGACY) : 0) |
        ((flags & SEARCH_RECOVERY) ? DEVTYPE_RECOVERY : 0);

    ssize_t device_count;
    if ((device_count = pg2sdr__discover_matching(shared_ctx, match_serial_prefix, match_ports, typemask, &devices)) < 0) {
        log_perror_pg2sdr(device_count, "could not enumerate USB devices");
        goto cleanup;
    }

    if (device_count == 0)
        goto cleanup; /* no matching device */

    if (device_count > 1) {
        /* multiple matching devices, bail out */
        log_error("More than one suitable device was found, select exactly one with the -p or -s options");
        for (size_t i = 0; i < device_count; ++i) {
            char *ports = pg2sdr__strdup_ports(ctx, devices[i]->lu_device);
            log_verbose("  port %s: %s", ports, device_string(devices[i]->lu_device));
            free(ports);
        }

        goto cleanup;
    }

    /* exactly one matching device, success */
    found = libusb_ref_device(devices[0]->lu_device);

 cleanup:
    if (devices)
        pg2sdr_free_device_list(devices);
    if (ctx)
        pg2sdr_exit(ctx);

    return found;
}
