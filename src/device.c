#include "internal.h"
#include <pthread.h>
#include <math.h>
#include <string.h>

int get_initial_device_from_list(lpcsdr_context *ctx, libusb_device **usb_list, int device_count, libusb_device **device) {
    
    int error = LPCSDR_SUCCESS;
    struct libusb_device_descriptor desc;
    for (int i = 0; i < device_count; i++) {
        libusb_device *usb_dev = usb_list[i];
        if ((error = libusb_get_device_descriptor(usb_dev, &desc)) < 0) {
            return lpcsdr_translate_libusb_error(ctx, error);
        }
        
        if (desc.idVendor == VID_ROM && desc.idProduct == PID_ROM) {
            *device = usb_dev;
            return LPCSDR_SUCCESS;
        }
    }

    return LPCSDR_ERROR_NOT_FOUND;
}


int lpcsdr_dsp_decimate_create(unsigned halfband_ntaps, const float *halfband_taps, lpcsdr_decimate **result)
{

    if (halfband_ntaps % 2 != 1)
        return LPCSDR_ERROR_BAD_ARGUMENT; /* must have an odd number of taps */

    float center_tap = halfband_taps[(halfband_ntaps - 1) / 2];
    if (center_tap == 0)
        return LPCSDR_ERROR_BAD_ARGUMENT; /* center tap must be nonzero */
    

    float sum_taps = 0; /* sum of absolute tap values; used to scale coefficients to avoid overflow */
    for (unsigned i = 0; i < halfband_ntaps / 2; ++i) {
        if ((halfband_ntaps / 2 - i) % 2 == 0 && halfband_taps[i] != 0.0)
            return LPCSDR_ERROR_BAD_ARGUMENT; /* doesn't follow the expected halfband filter structure */
        if (halfband_taps[i] != halfband_taps[halfband_ntaps - i - 1])
            return LPCSDR_ERROR_BAD_ARGUMENT; /* must be symmetric */
        if (fabs(halfband_taps[i]) > fabs(center_tap))
            return LPCSDR_ERROR_BAD_ARGUMENT; /* no tap should be larger than the center tap */
        sum_taps += fabs(halfband_taps[i]) * 2;
    }

    sum_taps += center_tap;

    struct lpcsdr_decimate *decimate;
    if (!(decimate = calloc(1, sizeof(*decimate))))
        return LPCSDR_ERROR_NO_MEMORY;

    decimate->ntaps = halfband_ntaps;
    decimate->history_max = decimate->ntaps * 2 + 2;

    if (!(decimate->taps = malloc(decimate->ntaps * sizeof(int16_t))) || !(decimate->history = malloc(decimate->history_max * sizeof(cs16_t)))) {
        free(decimate);
        return LPCSDR_ERROR_NO_MEMORY;
    }

    /* scale taps so that the output cannot ever overflow a Q15 representation */
    float scale = 32767 / sum_taps;
    for (unsigned i = 0; i < decimate->ntaps; ++i) {
        decimate->taps[i] = (int16_t)(halfband_taps[i] * scale + 0.5);
    }

    lpcsdr_dsp_decimate_reset(decimate);
    *result = decimate;
    return LPCSDR_SUCCESS;
}

int build_lpc_device(lpcsdr_context *ctx, lpcsdr_device_handle **d) {

    lpcsdr_device_handle *dev = calloc(1, sizeof(lpcsdr_device_handle));
    if (!dev)
        return LPCSDR_ERROR_NO_MEMORY;

    dev->magic = MAGIC_DEV;
    dev->ctx = ctx;

    pthread_mutexattr_t attrs;
    int error;
    if (pthread_mutexattr_init(&attrs) < 0 || pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE) < 0 || pthread_mutex_init(&dev->mutex, &attrs) < 0) {
        error = lpcsdr_translate_errno(ctx, errno);
        goto cleanup_nomutex;
    }

    if ((error = lpcsdr_dsp_decimate_create(lpcsdr_standard_filter_ntaps, lpcsdr_standard_filter_taps, &dev->decimation_filter)) < 0)
        goto cleanup;

    *d = dev;
    return LPCSDR_SUCCESS;

cleanup:
    // if (dev->baseband_filter)
        // lpcsdr_dsp_ifir_free(dev->baseband_filter);

    if (dev->decimation_filter)
        lpcsdr_dsp_decimate_free(dev->decimation_filter);

    pthread_mutex_destroy(&dev->mutex);

cleanup_nomutex:
    free(dev);

    return error;

}

void lpcsdr_dsp_decimate_free(struct lpcsdr_decimate *decimate)
{
    if (!decimate)
        return;

    free(decimate->taps);
    free(decimate->history);
    free(decimate);
}

int lpcsdr_free_device_list(lpc_device **device_list)
{
    if (!device_list)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    for (int i = 0; device_list[i]; ++i) {
        if (device_list[i]->libusb_device)
            libusb_unref_device((libusb_device *)device_list[i]->libusb_device);
        free(device_list[i]);
        device_list[i] = NULL;
    }

    free(device_list);
    return LPCSDR_SUCCESS;
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

    /* same mode, serial, USB bus; order by device address */
    return (int)left->usb_address - (int)right->usb_address;
}

int lpcsdr_discover_devices(lpcsdr_context *ctx, lpc_device ***lpc_device_list, bool allow_rom_bootloader) {

    libusb_device **libusb_device_list;
    ssize_t device_count = libusb_get_device_list(ctx->libusb_ctx, &libusb_device_list);
    if (device_count < 0) {
        return lpcsdr_translate_libusb_error(ctx, device_count);
    }

    lpc_device **lpc_devices_to_return;
    if (!(lpc_devices_to_return = calloc(device_count, sizeof(*lpc_devices_to_return))))
        return LPCSDR_ERROR_NO_MEMORY;

    
    int error = LPCSDR_SUCCESS;
    int matched = 0;

    for (ssize_t i = 0; i < device_count; ++i) {
        struct libusb_device_descriptor desc;
        lpcsdr_device_mode mode;
        libusb_device *usb_dev = libusb_device_list[i];
        if ((error = libusb_get_device_descriptor(usb_dev, &desc)) < 0) {
            return lpcsdr_translate_libusb_error(ctx, error);
        }

        if (desc.idVendor == VID_ROM && desc.idProduct == PID_ROM && allow_rom_bootloader) {
            /* DFU bootloader */
            mode = LPCSDR_DEVICE_MODE_DFU_BOOTLOADER;
        } else if (desc.idVendor == VID_LPCSDR && desc.idProduct == PID_LPCSDR) {
            /* LPCSDR firmware */
            mode = LPCSDR_DEVICE_MODE_NORMAL;
        } else {
            /* not an interesting device */
            continue;
        }

        if (mode == LPCSDR_DEVICE_MODE_NORMAL) {
            libusb_device_handle *handle;
            if ((error = libusb_open(usb_dev, &handle)) != LIBUSB_SUCCESS) {
                printf("uh oh\n");
                continue;
            }

            printf("Opened handle for lpcsdr\n");

            if ((error = lpcsdr_comms_check(handle)) < 0) {
                printf("error %d", error);
                continue;
            };
        }
            

        if (!(lpc_devices_to_return[matched] = calloc(sizeof(lpc_device), 1))) {
            error = LPCSDR_ERROR_NO_MEMORY;
            goto failed;
        }

        lpc_devices_to_return[matched]->context = ctx;
        lpc_devices_to_return[matched]->mode = mode;
        lpc_devices_to_return[matched]->usb_superspeed = (libusb_get_device_speed(usb_dev) >= LIBUSB_SPEED_SUPER);
        lpc_devices_to_return[matched]->usb_bus = libusb_get_bus_number(usb_dev);
        lpc_devices_to_return[matched]->usb_address = libusb_get_device_address(usb_dev);
        lpc_devices_to_return[matched]->libusb_device = (void *)libusb_ref_device(usb_dev);

        matched++;
    }

    if (matched > 1) {
        qsort(lpc_devices_to_return, matched, sizeof(*lpc_devices_to_return), rank_devices);
    }

    lpc_devices_to_return[matched] = NULL;

    *lpc_device_list = lpc_devices_to_return;
    return matched;

failed:
    //cleanup list
    lpcsdr_free_device_list(lpc_devices_to_return);
    return error;
}

int lpcsdr_open_single_device(lpcsdr_context *ctx, lpcsdr_device_handle **device_handle)
{
    int error = LPCSDR_SUCCESS;
    lpc_device **devices;
    
    int device_count = lpcsdr_discover_devices(ctx, &devices, true);
    
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

cleanup:
    lpcsdr_free_device_list(devices);
    return error;
}

int lpcsdr_open_device(lpc_device *device, lpcsdr_device_handle **device_handle) {
    int error;

    lpcsdr_context *ctx = device->context;
    libusb_device *original_dev = (libusb_device *) device->libusb_device;
    libusb_device *reenumerated_dev = NULL;
    libusb_device_handle *usb_handle = NULL;


    if (device->mode == LPCSDR_DEVICE_MODE_DFU_BOOTLOADER) {
        if ((error = lpcsdr_handle_rom_bootloader(ctx, original_dev, &reenumerated_dev)) < 0) {
            error = lpcsdr_translate_libusb_error(ctx, error);
            goto failed;
        }
    }

    if ((error = libusb_open(reenumerated_dev ? reenumerated_dev : original_dev, &usb_handle)) < 0) {
        error = lpcsdr_translate_libusb_error(ctx, error);
        goto failed;
    }

    if (reenumerated_dev) {
        /* we're done with this now */
        libusb_unref_device(reenumerated_dev);
        reenumerated_dev = NULL;
    }

    /* try to set configuration 1 if not already set */
    int config;
    if ((error = libusb_get_configuration(usb_handle, &config)) < 0) {
        error = lpcsdr_translate_libusb_error(ctx, error);
        goto failed;
    }

    if (config != 1) {
        if ((error = libusb_set_configuration(usb_handle, 1)) < 0) {
            error = lpcsdr_translate_libusb_error(ctx, error);
            goto failed;
        }
    }

    /* claim interface 0, the main data-streaming interface; only one thing can
     * have that claimed */
    if ((error = libusb_claim_interface(usb_handle, 0)) < 0) {
        error = lpcsdr_translate_libusb_error(ctx, error);
        goto failed;
    }


    //build lpcsdr_device_handle
    lpcsdr_device_handle *handle;
    build_lpc_device(ctx, &handle);
    handle->usb_handle = usb_handle;


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


static int generic_match(lpc_device *dev, void *arg)
{
    struct match_tuple *match = (struct match_tuple *)arg;

    if (match->serial && (dev->mode == LPCSDR_DEVICE_MODE_DFU_BOOTLOADER || strcmp(dev->serial, match->serial)))
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