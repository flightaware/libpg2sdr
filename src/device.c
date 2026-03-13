#include <pthread.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#include "internal.h"

static int build_device(pg2sdr_context *ctx, libusb_device *lu_device, char *serial, char *ports, pg2sdr_device **out)
{
    int error, usb_error;
    pg2sdr_device *dev = NULL;

    if (!serial || !ports) {
        error = PG2SDR_ERROR_NO_MEMORY;
        goto cleanup_nomutex;
    }

    if (!(dev = calloc(1, sizeof(*dev)))) {
        error = PG2SDR_ERROR_NO_MEMORY;
        goto cleanup_nomutex;
    }

    pthread_mutexattr_t attrs;
    if (pthread_mutexattr_init(&attrs) < 0 || pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE) < 0 || pthread_mutex_init(&dev->mutex, &attrs) < 0) {
        error = pg2sdr__translate_errno(errno);
        goto cleanup_nomutex;
    }

    dev->magic = MAGIC_DEV;
    dev->ctx = ctx;
    dev->serial = serial;
    dev->ports = ports;

    if ((usb_error = libusb_open(lu_device, &dev->usb_handle)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto cleanup;
    }

    /* set configuration 1 always (this does a soft reset of USB state) */
    if ((usb_error = libusb_set_configuration(dev->usb_handle, 1)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto cleanup;
    }

    /* claim interface 0, the main data-streaming interface; only one thing can
     * have that claimed */
    if ((usb_error = libusb_claim_interface(dev->usb_handle, 0)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto cleanup;
    }

    /* verify we can talk to the device, grab metadata, check versions */
    if ((error = pg2sdr__ctrl_comms_check(dev->usb_handle, /* timeout_ms */ 0)) < 0) {
        LOGERROR(dev, "basic USB communications check failed, is this device really a PG2SDR?");
        goto cleanup;
    }

    firmware_metadata_t meta;
    if ((error = pg2sdr__ctrl_get_metadata(dev->usb_handle, &meta, /* timeout_ms */ 0)) < 0)
        goto cleanup;

    if (PG2_CURRENT_VERSION < meta.compat) {
        LOGERROR(dev,
                 "host library protocol %u.%u.%u.%u < firmware minimum compatible protocol %u.%u.%u.%u, upgrade host library",
                 (PG2_CURRENT_VERSION >> 24) & 255,
                 (PG2_CURRENT_VERSION >> 16) & 255,
                 (PG2_CURRENT_VERSION >> 8) & 255,
                 (PG2_CURRENT_VERSION >> 0) & 255,
                 (meta.compat >> 24) & 255,
                 (meta.compat >> 16) & 255,
                 (meta.compat >> 8) & 255,
                 (meta.compat >> 0) & 255);
        error = PG2SDR_ERROR_FIRMWARE_MISMATCH;
        goto cleanup;
    }

    if (meta.version < PG2_COMPAT_VERSION) {
        LOGERROR(dev,
                 "firmware protocol %u.%u.%u.%u < host library minimum compatible protocol %u.%u.%u.%u, upgrade firmware",
                 (meta.version >> 24) & 255,
                 (meta.version >> 16) & 255,
                 (meta.version >> 8) & 255,
                 (meta.version >> 0) & 255,
                 (PG2_COMPAT_VERSION >> 24) & 255,
                 (PG2_COMPAT_VERSION >> 16) & 255,
                 (PG2_COMPAT_VERSION >> 8) & 255,
                 (PG2_COMPAT_VERSION >> 0) & 255);
        error = PG2SDR_ERROR_FIRMWARE_MISMATCH;
        goto cleanup;
    }

    dev->control_timeout_ms = meta.control_timeout_ms;

    ep0_in_board_status_t status;
    if ((error = pg2sdr__ctrl_get_status(dev->usb_handle, &status, dev->control_timeout_ms)) < 0)
        goto cleanup;

    dev->conversion_mode = PG2SDR_MODE_LOWIF_REAL;

    dev->adc_limit = 28e6;

    dev->requested_sample_rate = 0;
    dev->adc_pll_config.valid = false;
    dev->changing_rate = false;

    dev->requested_frequency = 0;
    dev->upper_sideband = false;
    dev->tuner_pll_config.valid = false;
    dev->changing_freq = false;

    dev->requested_bandpass_low = -100e6;
    dev->requested_bandpass_high = 100e6;
    dev->changing_bandpass = false;

    dev->decimation_mode = PG2SDR_DECIMATION_AUTO;
    dev->undersampling_mode = 1;

    dev->usb_bytes_per_block = status.usb_bytes_per_block;
    dev->usb_samples_per_block = status.usb_samples_per_block;
    dev->tuner_xtal = status.tuner_xtal;
    dev->buffer_size = 262144; /* default 128k (complex) */

    if ((error = pg2sdr__init_tuner(dev)) < 0)
        goto cleanup;

    /* set default gain tables */
    if ((error = pg2sdr_set_gain_tables(dev,
                                        pg2sdr__default_gain_table, pg2sdr__default_gain_table_size,
                                        pg2sdr__default_lna_table,
                                        pg2sdr__default_mix_table,
                                        pg2sdr__default_vga_table)) < 0)
        goto cleanup;

    /* set default bandpass table */
    if ((error = pg2sdr_set_bandpass_table(dev, pg2sdr__default_bandpass_table, pg2sdr__default_bandpass_table_size)) < 0)
        goto cleanup;

    *out = dev;
    return PG2SDR_SUCCESS;

cleanup:
    free(dev->bandpass_table);
    free(dev->gain_table);
    if (dev->usb_handle)
        libusb_close(dev->usb_handle);
    pthread_mutex_destroy(&dev->mutex);
    dev->magic = MAGIC_FREE;

cleanup_nomutex:
    free(dev);
    free(serial);
    free(ports);
    return error;
}

void pg2sdr_free_device_list(pg2sdr_usb_device **device_list)
{
    if (!device_list)
        return;

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


/* sorting function for discovery */
static int sort_devices(const void *l, const void *r)
{
    pg2sdr_usb_device *left = *((pg2sdr_usb_device **)l);
    pg2sdr_usb_device *right = *((pg2sdr_usb_device **)r);

    /* just order by serial */
    return strcmp(left->serial, right->serial);
}

static char *get_serial(pg2sdr_context *ctx, libusb_device *usb_dev)
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
        pg2sdr__log(ctx, PG2SDR_LOG_ERROR, "warning: could not open device to fetch serial number: %s", libusb_strerror(usb_error));
        return strdup("");
    }

    if ((usb_error = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char *)serial, sizeof(serial))) < 0) {
        pg2sdr__log(ctx, PG2SDR_LOG_ERROR, "warning: could not fetch serial number descriptor: %s", libusb_strerror(usb_error));
        libusb_close(handle);
        return strdup("");
    }

    libusb_close(handle);
    return strdup(serial);
}

static char *get_ports(pg2sdr_context *ctx, libusb_device *usb_dev)
{
    char port_string[33];
    char *out = port_string;
    char *end = port_string + sizeof(port_string);

    uint8_t bus = libusb_get_bus_number(usb_dev);
    uint8_t ports[7];
    int port_count = libusb_get_port_numbers(usb_dev, ports, sizeof(ports));
    if (port_count < 1) {
        pg2sdr__log(ctx, PG2SDR_LOG_ERROR, "warning: could not fetch USB ports: %s", libusb_strerror(port_count));
        return strdup("");
    }

    out += snprintf(out, end - out, "%u-%u", bus, ports[0]);
    for (int i = 1; i < port_count; ++i)
        out += snprintf(out, end - out, ".%u", ports[i]);

    return strdup(port_string);
}

ssize_t pg2sdr_discover_devices(pg2sdr_context *ctx, pg2sdr_usb_device ***device_list)
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
    if (!(matched_device_list = calloc(device_count + 1, sizeof(*matched_device_list)))) {
        error = PG2SDR_ERROR_NO_MEMORY;
        goto failed;
    }

    int matched = 0;
    for (ssize_t i = 0; i < device_count; ++i) {
        int usb_error;
        struct libusb_device_descriptor desc;
        libusb_device *lu_dev = lu_device_list[i];

        if ((usb_error = libusb_get_device_descriptor(lu_dev, &desc)) < 0) {
            pg2sdr__log(ctx, PG2SDR_LOG_ERROR, "error getting device descriptor for USB bus %d device %u: %s",
                        libusb_get_bus_number(lu_dev),
                        libusb_get_device_address(lu_dev),
                        libusb_strerror(usb_error));
            continue;
        }

        if (desc.idVendor != VID_PG2SDR || desc.idProduct != PID_PG2SDR) {
            continue; /* not a PG2SDR */
        }

        pg2sdr_usb_device *pg2_dev;
        if (!(pg2_dev = calloc(sizeof(*pg2_dev), 1))) {
            error = PG2SDR_ERROR_NO_MEMORY;
            goto failed;
        }

        matched_device_list[matched++] = pg2_dev;

        if (!(pg2_dev->serial = get_serial(ctx, lu_dev))) {
            error = PG2SDR_ERROR_NO_MEMORY;
            goto failed;
        }

        if (!(pg2_dev->ports = get_ports(ctx, lu_dev))) {
            error = PG2SDR_ERROR_NO_MEMORY;
            goto failed;
        }

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

int pg2sdr_open_single_device(pg2sdr_context *ctx, const char *serial_prefix, const char *ports, pg2sdr_device **device)
{
    int error = PG2SDR_SUCCESS;
    pg2sdr_usb_device **devices;

    ssize_t device_count = pg2sdr_discover_devices(ctx, &devices);
    if (device_count < 0)
        return device_count;

    int match_index = -1;
    for (int i = 0; i < device_count; ++i) {
        if (ports && strcmp(ports, devices[i]->ports) != 0)
            continue;
        if (serial_prefix &&
            (strlen(serial_prefix) > strlen(devices[i]->serial) ||
             strncmp(serial_prefix, devices[i]->serial, strlen(serial_prefix)) != 0))
            continue;

        if (match_index != -1) {
            error = PG2SDR_ERROR_MULTIPLE_DEVICES;
            goto cleanup;
        }
    }

    if (match_index == -1) {
        error = PG2SDR_ERROR_NOT_FOUND;
        goto cleanup;
    }

    if ((error = pg2sdr_open_device(ctx, devices[match_index], device)) < 0)
        goto cleanup;

cleanup:
    pg2sdr_free_device_list(devices);
    return error;
}

int pg2sdr_open_device(pg2sdr_context *ctx, pg2sdr_usb_device *usb_device, pg2sdr_device **device)
{
    CHECK_CTX(ctx);

    if (!usb_device || !device)
        return PG2SDR_ERROR_BAD_ARGUMENT;

    return build_device(ctx, usb_device->lu_device, strdup(usb_device->serial), strdup(usb_device->ports), device);
}

int pg2sdr_open_device_libusb(pg2sdr_context *ctx, libusb_device *lu_device, pg2sdr_device **device)
{
    CHECK_CTX(ctx);

    if (!lu_device || !device)
        return PG2SDR_ERROR_BAD_ARGUMENT;

    return build_device(ctx, lu_device, get_serial(ctx, lu_device), get_ports(ctx, lu_device), device);
}

int pg2sdr_close_device(pg2sdr_device *dev)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    if (dev->streaming) {
        pthread_mutex_unlock(&dev->mutex);
        return PG2SDR_ERROR_BUSY;
    }

    int error = pg2sdr__ctrl_set_rf_power(dev->usb_handle, RF_POWER_OFF, dev->control_timeout_ms);
    if (error < 0) {
        char buf[1024];
        LOGERROR(dev, "warning: could not disable RF power on device close: %s", pg2sdr_strerror_r(error, buf, sizeof(buf)));
        /* continue anyway */
    }

    free(dev->gain_table);
    free(dev->bandpass_table);

    libusb_close(dev->usb_handle);
    dev->magic = MAGIC_FREE;
    pthread_mutex_unlock(&dev->mutex);
    pthread_mutex_destroy(&dev->mutex);

    free(dev);

    return PG2SDR_SUCCESS;
}

const char *pg2sdr_get_serial(pg2sdr_device *dev)
{
    if (!dev || !dev->ctx || dev->magic != MAGIC_DEV || dev->ctx->magic != MAGIC_CTX)
        return NULL;

    return dev->serial;
}

const char *pg2sdr_get_ports(pg2sdr_device *dev)
{
    if (!dev || !dev->ctx || dev->magic != MAGIC_DEV || dev->ctx->magic != MAGIC_CTX)
        return NULL;

    return dev->ports;
}
