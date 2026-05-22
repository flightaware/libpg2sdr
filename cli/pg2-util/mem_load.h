/*
 *  mem_load.h - pg2-util load-firmware-to-RAM helpers, declarations
 *
 *  Copyright (c) 2026 FlightAware All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
