#include "mem_load.h"
#include "log.h"
#include "reset.h"
#include "device.h"

#include "internal/control.h"

/* TODO: get this from the firmware */
#define CONTROL_TIMEOUT 1000
#define BLOCK_SIZE 512

/* Send the image at `load_bytes` with size `load_size` to the USB device `handle`, but
 * don't start the new image yet.
 *
 * The image should include the LPC header, but does not need to include the DFU suffix.
 */
static bool load_image_bytes(libusb_device_handle *handle, const uint8_t *load_bytes, unsigned load_size, unsigned timeout_ms)
{
    int pg2_error;

    for (unsigned addr = 0; addr < load_size; addr += BLOCK_SIZE) {
        unsigned transfer_size = load_size - addr;
        if (transfer_size > BLOCK_SIZE)
            transfer_size = BLOCK_SIZE;

        if ((pg2_error = pg2sdr__ctrl_load_image(handle, addr, load_bytes + addr, transfer_size, timeout_ms)) < 0) {
            log_perror_pg2sdr(pg2_error, "LOAD_IMAGE(0x%04x .. 0x%04x)", addr, addr + transfer_size - 1);
            return false;
        }
    }

    return true;
}

static bool load_image_end(libusb_device_handle *handle, unsigned load_size, unsigned timeout_ms)
{
    int pg2_error;
    if ((pg2_error = pg2sdr__ctrl_load_image(handle, load_size, /* buf */ NULL, /* length */ 0, timeout_ms)) < 0) {
        log_perror_pg2sdr(pg2_error, "LOAD_IMAGE(%0x04x, end)", load_size);
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
    if (!load_image_bytes(handle, image->load_bytes, image->load_size, /* timeout */ 0))
        goto cleanup;

    /* set up the hotplug callback now, before triggering the new firmware */
    if (!(reset_state = reset_prepare(dev)))
        goto cleanup;

    /* relocate and start the new firmware */
    log_verbose("Booting new firmware image");
    if (!load_image_end(handle, image->load_size, /* timeout */ 0))
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
