#include "internal.h"
#include <pthread.h>

typedef struct {
    int bStatus;
    int bwPollTimeout;
    int bState;
    int iString;
} dfu_status_t;

static int hotplug_callback(libusb_context *usb_ctx, libusb_device *device, libusb_hotplug_event event, void *user_data)
{
    /* stash the device where requested */
    struct hotplug_callback_state *state = (struct hotplug_callback_state *)user_data;
    state->completed = 1;
    if (state->device)
        libusb_unref_device(state->device);
    state->device = libusb_ref_device(device);
    return 0; /* stay registered; we will deregister explicitly */
}

/* Send DFU_GET_STATUS_REQUEST, populate *status with the returned status
 * Return 0 on success, or a negative libusb error code on failure
 */
static int dfu_get_status(libusb_device_handle *dev, dfu_status_t *status)
{
    uint8_t buffer[6];
    int error = libusb_control_transfer(dev, 
                                        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                        DFU_GET_STATUS_REQUEST,
                                        0,
                                        0,
                                        (unsigned char *)buffer,
                                        6,
                                        1000);
    if (error < 0) {
        return error;
    }

    status->bStatus = buffer[0];
    status->bwPollTimeout = ((buffer[3]) << 16) | ((buffer[2]) << 8) | (buffer[1]);
    status->bState = buffer[4];
    status->iString = buffer[5];
    return error;
}

/* Send DFU_DOWNLOAD_REQUEST for block index "block" with a payload of "count" bytes in "buffer"
 * Return 0 on success, or a negative libusb error code on failure
 */
static int dfu_download_firmware(libusb_device_handle *handle, int block, const uint8_t *buffer, int count)
{
    return libusb_control_transfer(handle, 
                                   LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                   DFU_DOWNLOAD_REQUEST,
                                   block,
                                   0,
                                   (unsigned char *) buffer,
                                   count,
                                   1000);
}

int lpcsdr_upload_firmware(lpcsdr_context *ctx, libusb_device_handle *handle)
{
    int fd = open(ctx->firmware_path, O_RDONLY);
    if (fd < 0) {
        return lpcsdr_translate_errno(ctx, fd);
    }
    
    uint32_t block = 0;
    dfu_status_t dfu_status;
    int error = LPCSDR_SUCCESS;
    
    while (true) {
        uint8_t buffer[2048];
        int count = read(fd, buffer, sizeof(buffer));

        if (count <= 0) {
            break;
        }
        if ((error = dfu_download_firmware(handle, block, buffer, count)) < 0) {
            goto cleanup;
        }

        if ((error = dfu_get_status(handle, &status)) < 0) {
            goto cleanup;
        }
        if (dfu_status.bState != DFU_DOWNLOAD_IDLE) {
            error = LPCSDR_ERROR_FWIMAGE_UPLOAD;
            goto cleanup;
        }
        block += 1;
    }

    if ((error = dfu_download_firmware(handle, block, NULL, 0)) < 0) {
        goto cleanup;
    };

    // Sending the dfu_get_status to trigger manifestaton phase will get a pipe error response. Even though nothing is wrong.
    int final_resp = dfu_get_status(handle, &status);
    if (final_resp < 0 && final_resp != -9)
        error = final_resp;


cleanup:
    return error;
}

int lpcsdr_handle_rom_bootloader(lpcsdr_context *ctx, libusb_device *original_dev, libusb_device **reenumerated_dev)
{
    int error;
    int usb_error;
    libusb_device_handle *handle = NULL;
    struct hotplug_callback_state cb_state = {0, NULL};
    libusb_hotplug_callback_handle cb_handle;

    /* register for hotplug events for the main firmware VID/PID */
    if ((usb_error = libusb_hotplug_register_callback(ctx->libusb_ctx,
                                                      /* events */ LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
                                                      /* flags */ LIBUSB_HOTPLUG_NO_FLAGS,
                                                      /* vendor_id */ VID_LPCSDR,
                                                      /* product_id */ PID_LPCSDR,
                                                      /* dev_class */ LIBUSB_HOTPLUG_MATCH_ANY,
                                                      /* cb_fn */ hotplug_callback,
                                                      /* user_data */ &cb_state,
                                                      /* handle */ &cb_handle)) < 0) {
        error = lpcsdr_translate_libusb_error(ctx, usb_error);
        goto failed;
    }

    /* open and configure the original bootloader device */
    if ((usb_error = libusb_open(original_dev, &handle)) < 0) {
        error = lpcsdr_translate_libusb_error(ctx, usb_error);
        goto failed;
    }

    if ((usb_error = libusb_set_configuration(handle, 1)) < 0) {
        error = lpcsdr_translate_libusb_error(ctx, usb_error);
        goto failed;
    }

    if ((usb_error = libusb_claim_interface(handle, 0)) < 0) {
        error = lpcsdr_translate_libusb_error(ctx, usb_error);
        goto failed;
    }

    /* upload the firmware to the ROM bootloader */
    if ((error = lpcsdr_upload_firmware(ctx, handle)) < 0) {
        goto failed;
    }

    /* done with the bootloader device now */
    libusb_close(handle);
    handle = NULL;


    /* wait up to 5 seconds for hotplug notification of a new device
     * (normally, it takes around 0.5-0.6 seconds for the firmware to
     * re-enumerate and for libusb to notice)
     */
    struct timeval deadline;
    gettimeofday(&deadline, NULL);
    deadline.tv_sec += 5;

    while (!cb_state.completed) {
        struct timeval now;
        gettimeofday(&now, NULL);

        if (now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_usec >= deadline.tv_usec)) {
            /* timed out */
            printf("now %ld, %ld deadline %ld, %ld \n", now.tv_sec, now.tv_usec, deadline.tv_sec, deadline.tv_usec);
            error = LPCSDR_ERROR_FWIMAGE_UPLOAD;
            goto failed;
        }

        struct timeval timeout;
        timeout.tv_sec = deadline.tv_sec - now.tv_sec;
        if (deadline.tv_usec >= now.tv_usec)
            timeout.tv_usec = deadline.tv_usec - now.tv_usec;
        else {
            timeout.tv_sec--;
            timeout.tv_usec = deadline.tv_usec + 1000000 - now.tv_usec;
        }

        if ((usb_error = libusb_handle_events_timeout_completed(ctx->libusb_ctx, &timeout, &cb_state.completed)) < 0) {
            error = lpcsdr_translate_libusb_error(ctx, usb_error);
            goto failed;
        }
    }

    /* hotplug callback seen, we have the new device */
    libusb_hotplug_deregister_callback(ctx->libusb_ctx, cb_handle);
    *reenumerated_dev = cb_state.device; /* caller should unref this when done */
    return LPCSDR_SUCCESS;

failed:
    if (cb_handle)
        libusb_hotplug_deregister_callback(ctx->libusb_ctx, cb_handle);
    if (handle)
        libusb_close(handle);
    if (cb_state.device)
        libusb_unref_device(cb_state.device);
    return error;
}



int translate_dfu_status(int dfu_status) {
    switch(dfu_status) {
        case 0x00:
            return LPCSDR_SUCCESS;
        case 0x01:
            return DFU_ERROR_TARGET;
        case 0x02:
            return DFU_ERROR_FILE;
        case 0x03:
            return DFU_ERROR_WRITE;
        case 0x04:
            return DFU_ERROR_ERASE;
        case 0x05:
            return DFU_ERROR_CHECK_ERASED;
        case 0x06:
            return DFU_ERROR_PROG;
        case 0x07:
            return DFU_ERROR_VERIFY;
        case 0x08:
            return DFU_ERROR_ADDRESS;
        case 0x09:
            return DFU_ERROR_NOTDONE;
        case 0x0A:
            return DFU_ERROR_FIRMWARE;
        case 0x0B:
            return DFU_ERROR_VENDOR;
        case 0x0C:
            return DFU_ERROR_USBR;
        case 0x0D:
            return DFU_ERROR_POR;
        case 0x0F:
            return DFU_ERROR_STALLEDPKT;
        case 0x0E:
        default:
            return DFU_ERROR_UNKNOWN;

    }
}
