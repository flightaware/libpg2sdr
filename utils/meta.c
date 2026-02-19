#include "meta.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "device.h"

/* retrieve firmware metadata from active firmware */
static int ctrl_get_metadata(libusb_device_handle *handle, firmware_metadata_t *metadata)
{
    int rc = libusb_control_transfer(handle,
                                     LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
                                     EP0_IN_METADATA,
                                     0, /* wValue */
                                     0, /* wIndex */
                                     (unsigned char *)metadata,
                                     sizeof(*metadata),
                                     1000); /* timeout */
    if (rc < 0)
        return rc;

    metadata->version = le32toh(metadata->version);
    metadata->compat = le32toh(metadata->compat);
    metadata->max_control_transfer = le16toh(metadata->max_control_transfer);
    metadata->control_timeout_ms = le16toh(metadata->control_timeout_ms);
    metadata->build_type[sizeof(metadata->build_type)-1] = 0;

    return 0;
}

static void populate_active_meta(libusb_device *dev, port_metadata_t *meta)
{
    meta->active_firmware_valid = false;

    libusb_device_handle *handle = device_open(dev, true);
    if (!handle)
        return;

    int usb_error;
    if ((usb_error = ctrl_get_metadata(handle, &meta->active_firmware)) < 0) {
        log_perror_libusb(usb_error, "fetching firmware metadata failed");
        return;
    }

    meta->active_firmware_valid = true;
}

/* return true if it looks like the data accessed by 'io' is empty
 * (erased) flash -- i.e. if the first flash page is all 0xFF.
 */
static bool is_empty_flash(firmware_io_t *io)
{
    uint8_t page[256];

    if (!io->read(io, 0, page, sizeof(page)))
        return false;

    for (size_t i = 0; i < sizeof(page); ++i)
        if (page[i] != 0xFF)
            return false;

    return true;
}

static void populate_flash_meta(libusb_device *dev, port_metadata_t *meta)
{
    firmware_io_t *io = io_open_flash(dev, /* readonly */ true);
    if (!io)
        return;

    /* If flash is entirely empty, avoid the call to image_read as it
     * will complain if asked to read empty flash
     */
    if (!is_empty_flash(io))
        meta->flash_image = image_read(io);

    io->close(io);
}

port_metadata_t *meta_query(libusb_device *dev)
{
    port_metadata_t *meta;

    if (!(meta = calloc(1, sizeof(*meta)))) {
        log_perror("calloc");
        return NULL;
    }

    meta->port = strdup(device_ports(dev));

    if (device_is_dfu(dev)) {
        meta->device_type = DEVICE_DFU;
    } else if (device_is_pg2(dev)) {
        meta->device_type = DEVICE_PG2;

        const char *serial = device_serial(dev);
        if (serial)
            meta->serial = strdup(serial);

        populate_active_meta(dev, meta);
        populate_flash_meta(dev, meta);
    } else {
        meta->device_type = DEVICE_OTHER;
    }

    return meta;
}

void meta_free(port_metadata_t *meta)
{
    if (!meta)
        return;

    free((char*)meta->port);
    free((char*)meta->serial);
    image_free(meta->flash_image);
}
