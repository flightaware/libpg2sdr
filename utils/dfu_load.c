#include "dfu_load.h"
#include "reset.h"
#include "log.h"
#include "device.h"

#include <stdlib.h>
#include <string.h>

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

/* timeout for DFU control transfer, ms */
#define CONTROL_TIMEOUT 1000

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
                                        CONTROL_TIMEOUT);
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
                                   CONTROL_TIMEOUT);
}

/* Download a firmware image at `fw_bytes` with length `fw_size` to the device at `handle` via DFU,
 * but don't manifest the new firmware yet. Return true on success.
 */
static bool dfu_download_bytes(libusb_device_handle *handle, const uint8_t *fw_bytes, unsigned fw_size)
{
    dfu_status_t dfu_status;
    int usb_error;
    const unsigned block_size = 2048;

    unsigned block = 0;
    unsigned offset = 0;
    while (offset < fw_size) {
        const uint8_t *bytes = fw_bytes + offset;
        unsigned send_size = fw_size - offset;
        if (send_size > block_size)
            send_size = block_size;

        if ((usb_error = dfu_ctrl_download(handle, block, bytes, send_size)) < 0) {
            log_perror_libusb(usb_error, "DFU_DOWNLOAD (block %u)", block);
            return false;
        }

        if ((usb_error = dfu_ctrl_get_status(handle, &dfu_status)) < 0) {
            log_perror_libusb(usb_error, "DFU_GET_STATUS (block %u)", block);
            return false;
        }

        if (dfu_status.bState != DFU_DOWNLOAD_IDLE) {
            log_error("DFU_GET_STATUS (block %u): got DFU state %u, not IDLE",
                      block,
                      dfu_status.bState);
            return false;
        }

        ++block;
        offset += send_size;
    }

    /* end of download */
    if ((usb_error = dfu_ctrl_download(handle, block, NULL, 0)) < 0) {
        log_perror_libusb(usb_error, "DFU_DOWNLOAD (block %u, end of image)", block);
        return false;
    }

    return true;
}

/*
 * Manifest (i.e. start firmware) a previously-uploaded firmware image.
 *
 * return true on (probable) success -- i.e. sending the GET_STATUS
 * control transfer to start the firmware either suceeded, or
 * returned an expected pipe error. This doesn't mean the new firmware
 * is actually working!
 */
static bool dfu_manifest(libusb_device_handle *handle)
{
    dfu_status_t dfu_status;
    int usb_error;

    /* Sending the dfu_get_status to trigger manifestaton phase will get a pipe error response. Even though nothing is wrong. */
    if ((usb_error = dfu_ctrl_get_status(handle, &dfu_status)) < 0 && usb_error != LIBUSB_ERROR_PIPE) {
        log_perror_libusb(usb_error, "DFU_GET_STATUS (manifest)");
        return false;
    }

    /* success */
    return true;
}

bool dfu_load(const firmware_image_t *image, libusb_device *dev, libusb_device **loaded_device)
{
    libusb_device_handle *handle = NULL;
    firmware_reset_state *reset_state = NULL;
    libusb_device *post_reset = NULL;

    /* open and configure the original bootloader device */
    if (!(handle = device_open(dev, true))) {
        goto cleanup;
    }

    log_verbose("Loading %u bytes via DFU", image->load_size);

    /* send the firmware to the ROM bootloader, but don't trigger the reset yet */
    if (!dfu_download_bytes(handle, image->load_bytes, image->load_size))
        goto cleanup;

    /* set up the hotplug callback now, before triggering the new firmware */
    if (!(reset_state = reset_prepare(dev)))
        goto cleanup;

    /* trigger manifestation of the uploaded image (i.e. reset into the new firmware) */
    log_verbose("Booting new firmware");
    if (!dfu_manifest(handle))
        goto cleanup;

    /* done with the bootloader device */
    device_close(handle);
    handle = NULL;

    /* wait for the new device to appear */
    post_reset = reset_await(reset_state);
    if (loaded_device)
        *loaded_device = libusb_ref_device(post_reset);

 cleanup:
    if (handle)
        device_close(handle);
    if (post_reset)
        libusb_unref_device(post_reset);
    if (reset_state)
        reset_cleanup(reset_state);

    return (post_reset != NULL);
}
