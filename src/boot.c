#include <pthread.h>

#include "internal.h"

typedef struct {
    int bStatus;
    int bwPollTimeout;
    int bState;
    int iString;
} dfu_status_t;

/* DFU state machine values for dfu_status_t.bState; we only really care about one, DFU_DOWNLOAD_IDLE */
#define DFU_DOWNLOAD_IDLE 0x05

/* DFU protocol control transfer values */
#define DFU_DOWNLOAD_REQUEST 0x01
#define DFU_GET_STATUS_REQUEST 0x03

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
static int dfu_ctrl_get_status(libusb_device_handle *dev, dfu_status_t *status)
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
static int dfu_ctrl_download(libusb_device_handle *handle, int block, const uint8_t *buffer, int count)
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

/* Send a firmware image to the device (which must be in DFU bootloader mode)
 * using the DFU protocol.
 *
 * (nb: DFU uses "download" to mean "transfer from host to device", but we call that "upload"
 * elsewhere, so there's some contradictory terminology in here)
 *
 * Return 0 on success, or a negative liblpcsdr error code on failure
 */
static int dfu_download_image(pg2sdr_context *ctx, libusb_device_handle *handle)
{
    if (!ctx->firmware_path) {
        /* We want to upload firmware, but no path has been configured */
        return LPCSDR_ERROR_FWIMAGE_MISSING;
    }

    int fd = open(ctx->firmware_path, O_RDONLY);
    if (fd < 0) {
        char buf[1024];
        pg2sdr__log(ctx,
                    LPCSDR_LOG_ERROR,
                    "Could not read firmware image at %s: %s",
                    ctx->firmware_path,
                    pg2sdr_strerror_r(pg2sdr__translate_errno(errno), buf, sizeof(buf)));

        if (errno == ENOENT)
            return LPCSDR_ERROR_FWIMAGE_MISSING;
        else
            return pg2sdr__translate_errno(errno);
    }
    
    uint32_t block = 0;
    dfu_status_t dfu_status;
    int error = LPCSDR_SUCCESS;
    int usb_error;
    
    while (true) {
        uint8_t buffer[2048];
        int count = read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            /* read error */
            error = pg2sdr__translate_errno(errno);
            goto cleanup;
        }

        if (!count) {
            /* EOF, we are done */
            break;
        }

        if ((usb_error = dfu_ctrl_download(handle, block, buffer, count)) < 0) {
            error = pg2sdr__translate_libusb_error(usb_error);
            goto cleanup;
        }

        if ((usb_error = dfu_ctrl_get_status(handle, &dfu_status)) < 0) {
            error = pg2sdr__translate_libusb_error(usb_error);
            goto cleanup;
        }
        if (dfu_status.bState != DFU_DOWNLOAD_IDLE) {
            error = LPCSDR_ERROR_FWIMAGE_UPLOAD;
            goto cleanup;
        }

        ++block;
    }

    /* end of download */
    if ((usb_error = dfu_ctrl_download(handle, block, NULL, 0)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto cleanup;
    }

    /* Sending the dfu_get_status to trigger manifestaton phase will get a pipe error response. Even though nothing is wrong. */
    if ((usb_error = dfu_ctrl_get_status(handle, &dfu_status)) < 0 && usb_error != LIBUSB_ERROR_PIPE) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto cleanup;
    }

    /* success */

cleanup:
    close(fd);
    return error;
}

/* Given a device "orginal_dev" in DFU bootloader mode:
 *   use DFU to send a firmware image to the device
 *   wait for the device to restart with the new firmware image and re-enumerate as a LPCSDR device
 *   clean up the old device handle (as that device has gone away)
 *   discover the new LPCSDR device, and put the handle in *reenumerated_dev
 * Return 0 on success, or a negative liblpcsdr error on failure
 */
int pg2sdr__boot_firmware(pg2sdr_context *ctx, libusb_device *original_dev, libusb_device **reenumerated_dev)
{
    int error;
    int usb_error;
    libusb_device_handle *handle = NULL;
    struct hotplug_callback_state cb_state = {0, NULL};
    libusb_hotplug_callback_handle cb_handle;

    pg2sdr__log(ctx,
                LPCSDR_LOG_INFO,
                "Downloading firmware image to device at bus %u port %u",
                libusb_get_bus_number(original_dev),
                libusb_get_port_number(original_dev));

    /* register for hotplug events for the main firmware VID/PID */
    if ((usb_error = libusb_hotplug_register_callback(ctx->libusb_ctx,
                                                      /* events */ LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
                                                      /* flags */ LIBUSB_HOTPLUG_NO_FLAGS,
                                                      /* vendor_id */ VID_PG2SDR,
                                                      /* product_id */ PID_PG2SDR,
                                                      /* dev_class */ LIBUSB_HOTPLUG_MATCH_ANY,
                                                      /* cb_fn */ hotplug_callback,
                                                      /* user_data */ &cb_state,
                                                      /* handle */ &cb_handle)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto failed;
    }

    /* open and configure the original bootloader device */
    if ((usb_error = libusb_open(original_dev, &handle)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto failed;
    }

    if ((usb_error = libusb_set_configuration(handle, 1)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto failed;
    }

    if ((usb_error = libusb_claim_interface(handle, 0)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto failed;
    }

    /* upload the firmware to the ROM bootloader */
    if ((error = dfu_download_image(ctx, handle)) < 0) {
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
            error = LPCSDR_ERROR_TIMEOUT;
            goto failed;
        }

        /* set timeout to reflect the time remaining until our deadline */
        struct timeval timeout;
        timeout.tv_sec = deadline.tv_sec - now.tv_sec;
        if (deadline.tv_usec >= now.tv_usec)
            timeout.tv_usec = deadline.tv_usec - now.tv_usec;
        else {
            timeout.tv_sec--;
            timeout.tv_usec = deadline.tv_usec + 1000000 - now.tv_usec;
        }

        if ((usb_error = libusb_handle_events_timeout_completed(ctx->libusb_ctx, &timeout, &cb_state.completed)) < 0) {
            error = pg2sdr__translate_libusb_error(usb_error);
            goto failed;
        }
    }

    /* hotplug callback seen, we have the new device */
    pg2sdr__log(ctx,
                LPCSDR_LOG_INFO,
                "New LPCSDR device discovered at bus %u port %u",
                libusb_get_bus_number(cb_state.device),
                libusb_get_port_number(cb_state.device));
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
