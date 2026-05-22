/*
 *  hotplug.h - pg2-util hotplug device discovery helpers, declarations
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

#ifndef PG2_HOTPLUG_H
#define PG2_HOTPLUG_H

#include <libusb-1.0/libusb.h>

/* Common helper for handling starting new firmware
 * and discovering the re-enumerated device over the
 * USB reset triggered when the new firmware is started
 */

/* opaque type that holds state over the hotplug process */
typedef struct firmware_hotplug_state_s firmware_hotplug_state;

/* Prepare to handle a firmware reset on device `dev`.
 * This sets up a hotplug handler to watch for new USB
 * devices, and should be called immediately before
 * triggering the reset.
 *
 * Returns a new hotplug-state object that should be passed
 * to hotplug_await, or NULL on error. hotplug_cleanup should
 * eventually be called to free this object.
 */
firmware_hotplug_state *hotplug_prepare(libusb_device *dev);

/* Wait for a previously prepared device to complete
 * firmware reset and re-enumerate. This should be called
 * immediately after triggering the firmware reset.
 *
 * Returns the newly enumerated device (caller should
 * eventually call libusb_unref_device to free this device),
 * or NULL on error (including when no suitable device
 * re-enumerated within the timeout)
 */
libusb_device *hotplug_await(firmware_hotplug_state *state);

/* Finish using a hotplug-state object previously created
 * by hotplug_prepare, unregistering the hotplug handler
 * and releasing resources.
 *
 * It's safe to pass a NULL state.
 */
void hotplug_cleanup(firmware_hotplug_state *state);

#endif
