#include "mem_load.h"
#include "log.h"
#include "reset.h"
#include "device.h"
#include "pg2sdr_protocol.h"

/* TODO: get this from the firmware */
#define CONTROL_TIMEOUT 1000
#define BLOCK_SIZE 512

/* send a LOAD_IMAGE control transfer for a given address & payload, return any libusb error */
static int ctrl_load_image(libusb_device_handle *handle, unsigned address, const uint8_t *buffer, int count)
{
    return libusb_control_transfer(handle,
                                   LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
                                   EP0_OUT_LOAD_IMAGE,
                                   address & 0xFFFF,
                                   address >> 16,
                                   (unsigned char *)buffer,
                                   count,
                                   CONTROL_TIMEOUT);
}

/* Send the image at `load_bytes` with size `load_size` to the USB device `handle`, but
 * don't start the new image yet.
 *
 * The image should include the LPC header, but does not need to include the DFU suffix.
 */
static bool load_image_bytes(libusb_device_handle *handle, const uint8_t *load_bytes, unsigned load_size)
{
    int usb_error;

    for (unsigned addr = 0; addr < load_size; addr += BLOCK_SIZE) {
        unsigned transfer_size = load_size - addr;
        if (transfer_size > BLOCK_SIZE)
            transfer_size = BLOCK_SIZE;

        if ((usb_error = ctrl_load_image(handle, addr, load_bytes + addr, transfer_size)) < 0) {
            log_perror_libusb(usb_error, "LOAD_IMAGE(0x%04x .. 0x%04x)", addr, addr + transfer_size - 1);
            return false;
        }
    }

    return true;
}

static bool load_image_reset(libusb_device_handle *handle, unsigned load_size)
{
    int usb_error;
    if ((usb_error = ctrl_load_image(handle, load_size, NULL, 0)) < 0) {
        log_perror_libusb(usb_error, "LOAD_IMAGE(%0x04x, reset)", load_size);
        return false;
    }

    return true;
}

bool mem_load(const firmware_image_t *image, libusb_device *dev, libusb_device **loaded_device)
{
    libusb_device_handle *handle = NULL;
    firmware_reset_state *reset_state = NULL;
    libusb_device *post_reset = NULL;

    if (!(handle = device_open(dev, true)))
        goto cleanup;

    log_verbose("Loading %u bytes to device RAM", image->load_size);

    /* send the new image, but don't trigger the reset yet */
    if (!load_image_bytes(handle, image->load_bytes, image->load_size))
        goto cleanup;

    /* set up the hotplug callback now, before triggering the new firmware */
    if (!(reset_state = reset_prepare(dev)))
        goto cleanup;

    /* relocate and start the new firmware */
    log_verbose("Booting new firmware image");
    if (!load_image_reset(handle, image->load_size))
        goto cleanup;

    /* done with the old device now */
    libusb_close(handle);
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
