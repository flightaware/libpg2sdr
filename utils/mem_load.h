#ifndef PG2_MEM_LOAD_H
#define PG2_MEM_LOAD_H

#include "image.h"

#include <stdbool.h>
#include <libusb-1.0/libusb.h>

/*
 * Download the firmware in `image` using the
 * LOAD_IMAGE protocol to to the device `dev`,
 * and start the new firmware. The device should be
 * already running existing firmware (not in recovery mode).
 *
 * Wait for the new firmware to re-enumerate on the
 * USB bus. If `loaded_device` is not NULL, return
 * the newly enumerated device in `*loaded_device`,
 * with refcount passing to the caller (caller should
 * call `libusb_unref_device` when they're done with
 * the device)
 *
 * Return true on success
 */
bool mem_load(const firmware_image_t *image, libusb_device *dev, libusb_device **loaded_device);

#endif
