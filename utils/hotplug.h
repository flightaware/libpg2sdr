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
