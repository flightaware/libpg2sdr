#include <pthread.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#include "internal.h"

static int build_pg2sdr_usb_device(pg2sdr_context *ctx, libusb_device_handle *usb_handle, pg2sdr_device **out)
{
    int error;

    pg2sdr_device *dev = calloc(1, sizeof(pg2sdr_device));
    if (!dev)
        return PG2SDR_ERROR_NO_MEMORY;
    dev->usb_handle = usb_handle;
    dev->magic = MAGIC_DEV;
    dev->ctx = ctx;

    pthread_mutexattr_t attrs;
    if (pthread_mutexattr_init(&attrs) < 0 || pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE) < 0 || pthread_mutex_init(&dev->mutex, &attrs) < 0) {
        error = pg2sdr__translate_errno(errno);
        goto cleanup_nomutex;
    }

    ep0_in_board_status_t status;
    if ((error = pg2sdr__ctrl_get_status(dev, &status)) < 0)
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
    dev->buffer_size = 262144; /* default 1MB (complex) */

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
    pthread_mutex_destroy(&dev->mutex);
    dev->magic = MAGIC_FREE;

cleanup_nomutex:
    free(dev);
    return error;
}

void pg2sdr_free_device_list(pg2sdr_usb_device **device_list)
{
    if (!device_list)
        return;

    for (int i = 0; device_list[i]; ++i) {
        if (!device_list[i])
            continue;
        if (device_list[i]->libusb_device)
            libusb_unref_device((libusb_device *)device_list[i]->libusb_device);
        free(device_list[i]);
        device_list[i] = NULL;
    }

    free(device_list);
}


/* sorting function for discovery */
static int rank_devices(const void *l, const void *r)
{
    pg2sdr_usb_device *left = *((pg2sdr_usb_device **)l);
    pg2sdr_usb_device *right = *((pg2sdr_usb_device **)r);

    /* order by mode (ROM bootloaders last) */
    if (left->mode != right->mode)
        return (int)left->mode - (int)right->mode;

    /* same mode; order by serial */
    int serial_order = strcmp(left->serial, right->serial);
    if (serial_order != 0)
        return serial_order;

    /* same mode and serial; order by USB bus */
    if (left->usb_bus != right->usb_bus)
        return (int)left->usb_bus - (int)right->usb_bus;

    /* same mode, serial, USB bus; order by port path */
    return memcmp(left->usb_ports, right->usb_ports, sizeof(left->usb_ports));
}

/* Fill in "serial" with the device serial number for "usb_dev". Returns a libusb error. */
static int get_serial(libusb_device *usb_dev, unsigned char *serial, size_t length)
{
    memset(serial, 0, length);

    int usb_error;
    struct libusb_device_descriptor desc;
    if ((usb_error = libusb_get_device_descriptor(usb_dev, &desc)) < 0)
        return usb_error;

    if (!desc.iSerialNumber) {
        /* no serial number to get */
        return LIBUSB_SUCCESS;
    }

    /* unfortunately, current libusb lacks a way to get string descriptors without opening the device
     * (though there seems to be some slow movement on an API for that upstream)
     */
    libusb_device_handle *handle = NULL;
    if ((usb_error = libusb_open(usb_dev, &handle)) != LIBUSB_SUCCESS)
        return usb_error;

    usb_error = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, serial, length);
    libusb_close(handle);
    return usb_error;
}

ssize_t pg2sdr_discover_devices(pg2sdr_context *ctx, pg2sdr_usb_device ***pg2sdr_usb_device_list, bool allow_rom_bootloader)
{
    libusb_device **lu_device_list = NULL;
    ssize_t device_count = libusb_get_device_list(ctx->libusb_ctx, &lu_device_list);
    if (device_count < 0)
        return pg2sdr__translate_libusb_error(device_count);

    int error = PG2SDR_SUCCESS;
    pg2sdr_usb_device **pg2sdr_usb_devices_to_return;
    if (!(pg2sdr_usb_devices_to_return = calloc(device_count + 1, sizeof(*pg2sdr_usb_devices_to_return)))) {
        error = PG2SDR_ERROR_NO_MEMORY;
        goto failed;
    }

    int matched = 0;
    for (ssize_t i = 0; i < device_count; ++i) {
        int usb_error;
        struct libusb_device_descriptor desc;
        pg2sdr_device_mode mode;
        libusb_device *usb_dev = lu_device_list[i];

        if ((usb_error = libusb_get_device_descriptor(usb_dev, &desc)) < 0) {
            pg2sdr__log(ctx, PG2SDR_LOG_ERROR, "error getting device descriptor for USB bus %d device %d: %s",
                        libusb_get_bus_number(usb_dev),
                        libusb_get_port_number(usb_dev),
                        libusb_strerror(usb_error));
            continue;
        }

        unsigned char serial[17];
        if (desc.idVendor == VID_ROM && desc.idProduct == PID_ROM && allow_rom_bootloader) {
            /* DFU bootloader */
            mode = PG2SDR_DEVICE_MODE_DFU_BOOTLOADER;
            memset(serial, 0, sizeof(serial));
        } else if (desc.idVendor == VID_PG2SDR && desc.idProduct == PID_PG2SDR) {
            /* PG2SDR firmware */
            mode = PG2SDR_DEVICE_MODE_NORMAL;
            if ((usb_error = get_serial(usb_dev, serial, sizeof(serial))) < 0) {
                /* warn (but still use the device) */
                pg2sdr__log(ctx, PG2SDR_LOG_ERROR, "error getting serial number for USB bus %d device %d: %s",
                            libusb_get_bus_number(usb_dev),
                            libusb_get_port_number(usb_dev),
                            libusb_strerror(usb_error));
            }
        } else {
            /* not an interesting device */
            continue;
        }

        if (!(pg2sdr_usb_devices_to_return[matched] = calloc(sizeof(pg2sdr_usb_device), 1))) {
            error = PG2SDR_ERROR_NO_MEMORY;
            goto failed;
        }

        pg2sdr_usb_devices_to_return[matched]->context = ctx;
        pg2sdr_usb_devices_to_return[matched]->mode = mode;
        static_assert(sizeof(pg2sdr_usb_devices_to_return[matched]->serial) == sizeof(serial), "serial field size mismatch");
        memcpy(pg2sdr_usb_devices_to_return[matched]->serial, serial, sizeof(serial));
        pg2sdr_usb_devices_to_return[matched]->usb_bus = libusb_get_bus_number(usb_dev);
        pg2sdr_usb_devices_to_return[matched]->usb_address = libusb_get_device_address(usb_dev);
        pg2sdr_usb_devices_to_return[matched]->libusb_device = (void *)libusb_ref_device(usb_dev);
        usb_error = libusb_get_port_numbers(usb_dev, pg2sdr_usb_devices_to_return[matched]->usb_ports, sizeof(pg2sdr_usb_devices_to_return[matched]->usb_ports) - 1);
        if (usb_error < 0) {
            pg2sdr__log(ctx, PG2SDR_LOG_ERROR, "error getting port path for USB bus %d device %d: %s",
                        libusb_get_bus_number(usb_dev),
                        libusb_get_port_number(usb_dev),
                        libusb_strerror(usb_error));
            pg2sdr_usb_devices_to_return[matched]->usb_ports[0] = 0;
        }

        matched++;
    }

    if (matched > 1) {
        qsort(pg2sdr_usb_devices_to_return, matched, sizeof(*pg2sdr_usb_devices_to_return), rank_devices);
    }

    pg2sdr_usb_devices_to_return[matched] = NULL;
    *pg2sdr_usb_device_list = pg2sdr_usb_devices_to_return;
    libusb_free_device_list(lu_device_list, /* unref devices */ 1);
    return matched;

failed:
    pg2sdr_free_device_list(pg2sdr_usb_devices_to_return);
    libusb_free_device_list(lu_device_list, /* unref devices */ 1);
    return error;
}

int pg2sdr_open_single_device(pg2sdr_context *ctx, pg2sdr_device **device_handle)
{
    int error = PG2SDR_SUCCESS;
    pg2sdr_usb_device **devices;
    
    int device_count = pg2sdr_discover_devices(ctx, &devices, true);
    if (device_count < 0)
      return device_count;
    
    if (device_count == 0) {
        error = PG2SDR_ERROR_NOT_FOUND;
        goto cleanup;
    }

    if (device_count > 1) {
        error = PG2SDR_ERROR_MULTIPLE_DEVICES;
        goto cleanup;
    }

    if ((error = pg2sdr_open_device(devices[0], device_handle)) < 0)
        goto cleanup;

    return error;

cleanup:
    pg2sdr_free_device_list(devices);
    return error;
}

int pg2sdr_open_device(pg2sdr_usb_device *device, pg2sdr_device **device_handle)
{
    if (!device || !device_handle)
        return PG2SDR_ERROR_BAD_ARGUMENT;

    int error, usb_error;

    pg2sdr_context *ctx = device->context;
    libusb_device *original_dev = (libusb_device *) device->libusb_device;
    libusb_device *reenumerated_dev = NULL;
    libusb_device_handle *usb_handle = NULL;

    if (device->mode == PG2SDR_DEVICE_MODE_DFU_BOOTLOADER) {
        if ((error = pg2sdr__boot_firmware(ctx, original_dev, &reenumerated_dev)) < 0) {
            goto failed;
        }
    }

    if ((usb_error = libusb_open(reenumerated_dev ? reenumerated_dev : original_dev, &usb_handle)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto failed;
    }

    if (reenumerated_dev) {
        /* we're done with this now */
        libusb_unref_device(reenumerated_dev);
        reenumerated_dev = NULL;
    }

    /* set configuration 1 always (this does a soft reset of USB state) */
    if ((usb_error = libusb_set_configuration(usb_handle, 1)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto failed;
    }

    /* claim interface 0, the main data-streaming interface; only one thing can
     * have that claimed */
    if ((usb_error = libusb_claim_interface(usb_handle, 0)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto failed;
    }


    //build pg2sdr_device
    pg2sdr_device *handle = NULL;
    if ((error = build_pg2sdr_usb_device(ctx, usb_handle, &handle)) < 0) {
        goto failed;
    }

    *device_handle = handle;
    return PG2SDR_SUCCESS;

failed:
    if (usb_handle) {
        libusb_close(usb_handle);
    }

    if (reenumerated_dev) {
        libusb_unref_device(reenumerated_dev);
    }

    return error;
}

struct match_tuple {
    const char *serial;
    int bus;
    int address;
    int index;
};

static int generic_match(pg2sdr_usb_device *dev, void *arg)
{
    struct match_tuple *match = (struct match_tuple *)arg;

    if (match->serial && strcmp(dev->serial, match->serial))
        return 0;

    if (match->bus >= 0 && dev->usb_bus != match->bus)
        return 0;

    if (match->address >= 0 && dev->usb_address != match->address)
        return 0;

    if (match->index >= 0 && dev->index != match->index)
        return 0;

    return 1;
}

static int generic_open_by(pg2sdr_context *ctx, struct match_tuple *match, pg2sdr_device **device)
{
    CHECK_CTX(ctx);
    if (!device)
        return PG2SDR_ERROR_BAD_ARGUMENT;
    return pg2sdr_open_by_callback(ctx, generic_match, (void *)match, device);
}

int pg2sdr_open_by_serial(pg2sdr_context *ctx, const char *serial, pg2sdr_device **device)
{
    struct match_tuple match = {
        .serial = serial,
        .bus = -1,
        .address = -1,
        .index = -1,
    };

    return generic_open_by(ctx, &match, device);
}

int pg2sdr_open_by_address(pg2sdr_context *ctx, uint8_t bus, uint8_t address, pg2sdr_device **device)
{
    struct match_tuple match = {
        .serial = NULL,
        .bus = bus,
        .address = address,
        .index = -1,
    };

    return generic_open_by(ctx, &match, device);
}

int pg2sdr_open_by_index(pg2sdr_context *ctx, unsigned index, pg2sdr_device **device)
{
    struct match_tuple match = {
        .serial = NULL,
        .bus = -1,
        .address = -1,
        .index = index,
    };

    return generic_open_by(ctx, &match, device);
}

int pg2sdr_open_by_callback(pg2sdr_context *ctx, int (*callback)(pg2sdr_usb_device*, void *), void *callback_data, pg2sdr_device **device)
{
    CHECK_CTX(ctx);
    if (!callback || !device)
        return PG2SDR_ERROR_BAD_ARGUMENT;

    pg2sdr_usb_device **device_list;
    int count = pg2sdr_discover_devices(ctx, &device_list, true); /* include ROM bootloader always, the callback can filter */
    if (count < 0)
        return count;

    pg2sdr_usb_device *match = NULL;
    for (int i = 0; i < count; ++i) {
        if (callback(device_list[i], callback_data)) {
            match = device_list[i];
            break;
        }
    }

    int error;
    if (!match)
        error = PG2SDR_ERROR_NOT_FOUND;
    else
        error = pg2sdr_open_device(match, device);

    pg2sdr_free_device_list(device_list);
    return error;
}

int pg2sdr_close_device(pg2sdr_device *dev)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    if (dev->streaming) {
        pthread_mutex_unlock(&dev->mutex);
        return PG2SDR_ERROR_BUSY;
    }

    int error = pg2sdr__ctrl_set_rf_power(dev, RF_POWER_OFF);
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

int pg2sdr_get_serial(pg2sdr_device *dev, char *serial, size_t length)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);

    ep0_in_board_status_t status;
    int error = pg2sdr__ctrl_get_status(dev, &status);
    if (error < 0)
        goto done;

    snprintf(serial, length, "%" PRIX64, status.serial_number);
    error = PG2SDR_SUCCESS;

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}
