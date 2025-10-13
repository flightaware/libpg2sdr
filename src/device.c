#include <pthread.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#include "internal.h"

static int build_lpc_device(lpcsdr_context *ctx, libusb_device_handle *usb_handle, lpcsdr_device_handle **out)
{
    int error;

    lpcsdr_device_handle *dev = calloc(1, sizeof(lpcsdr_device_handle));
    if (!dev)
        return LPCSDR_ERROR_NO_MEMORY;
    dev->usb_handle = usb_handle;
    dev->magic = MAGIC_DEV;
    dev->ctx = ctx;

    pthread_mutexattr_t attrs;
    if (pthread_mutexattr_init(&attrs) < 0 || pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE) < 0 || pthread_mutex_init(&dev->mutex, &attrs) < 0) {
        error = lpcsdr__translate_errno(errno);
        goto cleanup_nomutex;
    }

    ep0_in_board_status_t status;
    if ((error = lpcsdr__ctrl_get_status(dev, &status)) < 0)
        goto cleanup;

    dev->conversion_mode = LPCSDR_MODE_LOWIF_REAL;

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

    dev->decimation_mode = LPCSDR_DECIMATION_AUTO;

    dev->usb_bytes_per_block = status.usb_bytes_per_block;
    dev->usb_samples_per_block = status.usb_samples_per_block;
    dev->tuner_xtal = status.tuner_xtal;
    dev->buffer_size = 262144; /* default 1MB (complex) */

    if ((error = lpcsdr__init_tuner(dev)) < 0)
        goto cleanup;

    /* set default gain tables */
    if ((error = lpcsdr_set_gain_tables(dev,
                                        lpcsdr__default_gain_table, lpcsdr__default_gain_table_size,
                                        lpcsdr__default_lna_table,
                                        lpcsdr__default_mix_table,
                                        lpcsdr__default_vga_table)) < 0)
        goto cleanup;

    /* set default bandpass table */
    if ((error = lpcsdr_set_bandpass_table(dev, lpcsdr__default_bandpass_table, lpcsdr__default_bandpass_table_size)) < 0)
        goto cleanup;

    *out = dev;
    return LPCSDR_SUCCESS;

cleanup:
    pthread_mutex_destroy(&dev->mutex);

cleanup_nomutex:
    free(dev);
    return error;
}

void lpcsdr_free_device_list(lpc_device **device_list)
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
    lpc_device *left = *((lpc_device **)l);
    lpc_device *right = *((lpc_device **)r);

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

ssize_t lpcsdr_discover_devices(lpcsdr_context *ctx, lpc_device ***lpc_device_list, bool allow_rom_bootloader)
{
    libusb_device **lu_device_list = NULL;
    ssize_t device_count = libusb_get_device_list(ctx->libusb_ctx, &lu_device_list);
    if (device_count < 0)
        return lpcsdr__translate_libusb_error(device_count);

    int error = LPCSDR_SUCCESS;
    lpc_device **lpc_devices_to_return;
    if (!(lpc_devices_to_return = calloc(device_count + 1, sizeof(*lpc_devices_to_return)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto failed;
    }

    int matched = 0;
    for (ssize_t i = 0; i < device_count; ++i) {
        int usb_error;
        struct libusb_device_descriptor desc;
        lpcsdr_device_mode mode;
        libusb_device *usb_dev = lu_device_list[i];

        if ((usb_error = libusb_get_device_descriptor(usb_dev, &desc)) < 0) {
            lpcsdr__log(ctx, LPCSDR_LOG_ERROR, "error getting device descriptor for USB bus %d device %d: %s",
                        libusb_get_bus_number(usb_dev),
                        libusb_get_port_number(usb_dev),
                        libusb_strerror(usb_error));
            continue;
        }

        unsigned char serial[17];
        if (desc.idVendor == VID_ROM && desc.idProduct == PID_ROM && allow_rom_bootloader) {
            /* DFU bootloader */
            mode = LPCSDR_DEVICE_MODE_DFU_BOOTLOADER;
            memset(serial, 0, sizeof(serial));
        } else if (desc.idVendor == VID_LPCSDR && desc.idProduct == PID_LPCSDR) {
            /* LPCSDR firmware */
            mode = LPCSDR_DEVICE_MODE_NORMAL;
            if ((usb_error = get_serial(usb_dev, serial, sizeof(serial))) < 0) {
                /* warn (but still use the device) */
                lpcsdr__log(ctx, LPCSDR_LOG_ERROR, "error getting serial number for USB bus %d device %d: %s",
                            libusb_get_bus_number(usb_dev),
                            libusb_get_port_number(usb_dev),
                            libusb_strerror(usb_error));
            }
        } else {
            /* not an interesting device */
            continue;
        }

        if (!(lpc_devices_to_return[matched] = calloc(sizeof(lpc_device), 1))) {
            error = LPCSDR_ERROR_NO_MEMORY;
            goto failed;
        }

        lpc_devices_to_return[matched]->context = ctx;
        lpc_devices_to_return[matched]->mode = mode;
        static_assert(sizeof(lpc_devices_to_return[matched]->serial) == sizeof(serial));
        memcpy(lpc_devices_to_return[matched]->serial, serial, sizeof(serial));
        lpc_devices_to_return[matched]->usb_bus = libusb_get_bus_number(usb_dev);
        lpc_devices_to_return[matched]->usb_address = libusb_get_device_address(usb_dev);
        lpc_devices_to_return[matched]->libusb_device = (void *)libusb_ref_device(usb_dev);
        usb_error = libusb_get_port_numbers(usb_dev, lpc_devices_to_return[matched]->usb_ports, sizeof(lpc_devices_to_return[matched]->usb_ports) - 1);
        if (usb_error < 0) {
            lpcsdr__log(ctx, LPCSDR_LOG_ERROR, "error getting port path for USB bus %d device %d: %s",
                        libusb_get_bus_number(usb_dev),
                        libusb_get_port_number(usb_dev),
                        libusb_strerror(usb_error));
            lpc_devices_to_return[matched]->usb_ports[0] = 0;
        }

        matched++;
    }

    if (matched > 1) {
        qsort(lpc_devices_to_return, matched, sizeof(*lpc_devices_to_return), rank_devices);
    }

    lpc_devices_to_return[matched] = NULL;
    *lpc_device_list = lpc_devices_to_return;
    libusb_free_device_list(lu_device_list, /* unref devices */ 1);
    return matched;

failed:
    lpcsdr_free_device_list(lpc_devices_to_return);
    libusb_free_device_list(lu_device_list, /* unref devices */ 1);
    return error;
}

int lpcsdr_open_single_device(lpcsdr_context *ctx, lpcsdr_device_handle **device_handle)
{
    int error = LPCSDR_SUCCESS;
    lpc_device **devices;
    
    int device_count = lpcsdr_discover_devices(ctx, &devices, true);
    if (device_count < 0)
      return device_count;
    
    if (device_count == 0) {
        error = LPCSDR_ERROR_NOT_FOUND;
        goto cleanup;
    }

    if (device_count > 1) {
        error = LPCSDR_ERROR_MULTIPLE_DEVICES;
        goto cleanup;
    }

    if ((error = lpcsdr_open_device(devices[0], device_handle)) < 0)
        goto cleanup;

    return error;

cleanup:
    lpcsdr_free_device_list(devices);
    return error;
}

int lpcsdr_open_device(lpc_device *device, lpcsdr_device_handle **device_handle)
{
    if (!device || !device_handle)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    int error, usb_error;

    lpcsdr_context *ctx = device->context;
    libusb_device *original_dev = (libusb_device *) device->libusb_device;
    libusb_device *reenumerated_dev = NULL;
    libusb_device_handle *usb_handle = NULL;

    if (device->mode == LPCSDR_DEVICE_MODE_DFU_BOOTLOADER) {
        if ((error = lpcsdr__boot_firmware(ctx, original_dev, &reenumerated_dev)) < 0) {
            goto failed;
        }
    }

    if ((usb_error = libusb_open(reenumerated_dev ? reenumerated_dev : original_dev, &usb_handle)) < 0) {
        error = lpcsdr__translate_libusb_error(usb_error);
        goto failed;
    }

    if (reenumerated_dev) {
        /* we're done with this now */
        libusb_unref_device(reenumerated_dev);
        reenumerated_dev = NULL;
    }

    /* set configuration 1 always (this does a soft reset of USB state) */
    if ((usb_error = libusb_set_configuration(usb_handle, 1)) < 0) {
        error = lpcsdr__translate_libusb_error(usb_error);
        goto failed;
    }

    /* claim interface 0, the main data-streaming interface; only one thing can
     * have that claimed */
    if ((usb_error = libusb_claim_interface(usb_handle, 0)) < 0) {
        error = lpcsdr__translate_libusb_error(usb_error);
        goto failed;
    }


    //build lpcsdr_device_handle
    lpcsdr_device_handle *handle = NULL;
    if ((error = build_lpc_device(ctx, usb_handle, &handle)) < 0) {
        goto failed;
    }

    *device_handle = handle;
    return LPCSDR_SUCCESS;

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

static int generic_match(lpc_device *dev, void *arg)
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

static int generic_open_by(lpcsdr_context *ctx, struct match_tuple *match, lpcsdr_device_handle **device)
{
    CHECK_CTX(ctx);
    if (!device)
        return LPCSDR_ERROR_BAD_ARGUMENT;
    return lpcsdr_open_by_callback(ctx, generic_match, (void *)match, device);
}

int lpcsdr_open_by_serial(lpcsdr_context *ctx, const char *serial, lpcsdr_device_handle **device)
{
    struct match_tuple match = {
        .serial = serial,
        .bus = -1,
        .address = -1,
        .index = -1,
    };

    return generic_open_by(ctx, &match, device);
}

int lpcsdr_open_by_address(lpcsdr_context *ctx, uint8_t bus, uint8_t address, lpcsdr_device_handle **device)
{
    struct match_tuple match = {
        .serial = NULL,
        .bus = bus,
        .address = address,
        .index = -1,
    };

    return generic_open_by(ctx, &match, device);
}

int lpcsdr_open_by_index(lpcsdr_context *ctx, unsigned index, lpcsdr_device_handle **device)
{
    struct match_tuple match = {
        .serial = NULL,
        .bus = -1,
        .address = -1,
        .index = index,
    };

    return generic_open_by(ctx, &match, device);
}

int lpcsdr_open_by_callback(lpcsdr_context *ctx, int (*callback)(lpc_device*, void *), void *callback_data, lpcsdr_device_handle **device)
{
    CHECK_CTX(ctx);
    if (!callback || !device)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    lpc_device **device_list;
    int count = lpcsdr_discover_devices(ctx, &device_list, true); /* include ROM bootloader always, the callback can filter */
    if (count < 0)
        return count;

    lpc_device *match = NULL;
    for (int i = 0; i < count; ++i) {
        if (callback(device_list[i], callback_data)) {
            match = device_list[i];
            break;
        }
    }

    int error;
    if (!match)
        error = LPCSDR_ERROR_NOT_FOUND;
    else
        error = lpcsdr_open_device(match, device);

    lpcsdr_free_device_list(device_list);
    return error;
}

int lpcsdr_close_device(lpcsdr_device_handle *dev)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    if (dev->streaming) {
        pthread_mutex_unlock(&dev->mutex);
        return LPCSDR_ERROR_BUSY;
    }

    int error = lpcsdr__ctrl_set_rf_power(dev, RF_POWER_OFF);
    if (error < 0) {
        char buf[1024];
        LOGERROR(dev, "warning: could not disable RF power on device close: %s", lpcsdr_strerror_r(error, buf, sizeof(buf)));
        /* continue anyway */
    }

    free(dev->gain_table);

    libusb_close(dev->usb_handle);
    dev->magic = MAGIC_FREE;
    pthread_mutex_unlock(&dev->mutex);
    pthread_mutex_destroy(&dev->mutex);

    free(dev);

    return LPCSDR_SUCCESS;
}

int lpcsdr_get_serial(lpcsdr_device_handle *dev, char *serial, size_t length)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);

    ep0_in_board_status_t status;
    int error = lpcsdr__ctrl_get_status(dev, &status);
    if (error < 0)
        goto done;

    snprintf(serial, length, "%" PRIX64, status.serial_number);
    error = LPCSDR_SUCCESS;

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}
