#ifndef PG2_RESET_H
#define PG2_RESET_H

#include <libusb-1.0/libusb.h>

/* Common helper for handling starting new firmware
 * and discovering the re-enumerated device over the
 * USB reset triggered when the new firmware is started
 */

/* opaque type that holds state over the reset process */
typedef struct firmware_reset_state_s firmware_reset_state;

/* Prepare to handle a firmware reset on device `dev`.
 * This sets up a hotplug handler to watch for new USB
 * devices, and should be called immediately before
 * triggering the reset.
 *
 * Returns a new reset-state object that should be passed
 * to reset_await, or NULL on error. reset_cleanup should
 * eventually be called to free this object.
 */
firmware_reset_state *reset_prepare(libusb_device *dev);

/* Wait for a previously prepared device to complete
 * firmware reset and re-enumerate. This should be called
 * immediately after triggering the firmware reset.
 *
 * Returns the newly enumerated device (caller should
 * eventually call libusb_unref_device to free this device),
 * or NULL on error (including when no suitable device
 * re-enumerated within the timeout)
 */
libusb_device *reset_await(firmware_reset_state *state);

/* Finish using a reset-state object previously created
 * by reset_prepare, unregistering the hotplug handler
 * and releasing resources.
 *
 * It's safe to pass a NULL state.
 */
void reset_cleanup(firmware_reset_state *state);

#endif
