/*
 *  hotplug.c - pg2-util hotplug device discovery helpers, implementation
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

#include "hotplug.h"

#include "internal/core.h"
#include "log.h"
#include "device.h"

#include <string.h>
#include <stdlib.h>


//
// This code handles the shared logic for waiting for newly started firmware
// to re-enumerate on the USB bus.
//
// The general flow of control is:
//
//   call hotplug_prepare() with the existing USB device
//   tell the existing device to start the new firmware via whatever mechanism
//   call hotplug_await(), which waits for re-enumeration and returns the re-enumerated device or NULL
//   call hotplug_cleanup() to clean up
//

/* our internal state preserved over hotplug_* */
struct firmware_hotplug_state_s {
    int completed;                              /* libusb completion flag */
    libusb_hotplug_callback_handle cb_handle;   /* libusb hotplug callback handle for registered hotplug handler */
    libusb_device *device;                      /* newly appeared USB device on the port we were monitoring (owns a device reference, call libusb_unref_device to free) */
    libusb_device *maybe_device;                /* newly appeared USB device on some other port (for the VM case) (owns a device reference, call libusb_unref_device to free) */

    uint8_t match_bus;                          /* bus number of the original pre-hotplug device */
    uint8_t match_ports[7];                     /* port path of the original pre-hotplug device */
    int match_ports_count;                      /* size of match_ports */
};

/* Callback from libusb when a new USB device enumerates. user_data points to our firmware_hotplug_state_t */
static int hotplug_callback(libusb_context *usb_ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data)
{
    firmware_hotplug_state *state = (firmware_hotplug_state *)user_data;

    /* get triggering device bus / ports */
    uint8_t bus = libusb_get_bus_number(dev);
    uint8_t ports[7];
    int count = libusb_get_port_numbers(dev, ports, sizeof(ports));
    if (count <= 0) {
        log_perror_libusb(count, "libusb_get_port_numbers");
        return 0;
    }

    if (bus == state->match_bus &&
        count == state->match_ports_count &&
        !memcmp(ports, state->match_ports, count)) {
        /* Activity on the port we're monitoring. Remember the latest
         * device seen, and set the completion flag.
         */
        if (state->device)
            libusb_unref_device(state->device);
        state->device = libusb_ref_device(dev);
        state->completed = 1;
        return 0;
    }

    /* Activity on some other port */
    switch (pg2sdr__identify_device(dev)) {
    case DEVTYPE_PG2SDR:
    case DEVTYPE_PROTOTYPE:
    case DEVTYPE_RECOVERY:
        /* PG2-like hardware enumerated on a different port, remember the first one as a "maybe match" */
        if (!state->maybe_device) {
            state->maybe_device = libusb_ref_device(dev);
        }
        return 0;
    default:
        /* Anything else, don't care */
        return 0;
    }
}

firmware_hotplug_state *hotplug_prepare(libusb_device *dev)
{
    firmware_hotplug_state *state;

    if (!(state = calloc(1, sizeof(*state)))) {
        log_perror("calloc");
        return NULL;
    }

    /* Record the bus/port of the original device for later matching in hotplug_callback */
    state->match_bus = libusb_get_bus_number(dev);
    int count = libusb_get_port_numbers(dev, state->match_ports, sizeof(state->match_ports));
    if (count < 0) {
        log_perror_libusb(count, "libusb_get_port_numbers");
        free(state);
        return NULL;
    }
    state->match_ports_count = count;

    /* register for hotplug events for all new devices connecting to the USB bus
     * (we cannot be more specific as we want to match >1 vendor/product ID)
     */
    int usb_error;
    if ((usb_error = libusb_hotplug_register_callback(NULL,
                                                      /* events */ LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
                                                      /* flags */ LIBUSB_HOTPLUG_NO_FLAGS,
                                                      /* vendor_id */ LIBUSB_HOTPLUG_MATCH_ANY,
                                                      /* product_id */ LIBUSB_HOTPLUG_MATCH_ANY,
                                                      /* dev_class */ LIBUSB_HOTPLUG_MATCH_ANY,
                                                      /* cb_fn */ hotplug_callback,
                                                      /* user_data */ state,
                                                      /* handle */ &state->cb_handle)) < 0) {
        log_perror_libusb(usb_error, "libusb_hotplug_register_callback");
        free(state);
        return NULL;
    }

    return state;
}

libusb_device *hotplug_await(firmware_hotplug_state *state)
{
    log_verbose("Waiting for new firmware to start");

    /* wait up to 3 seconds for hotplug notification of a new device
     * (normally, it takes around 1.5 seconds for the firmware to
     * re-enumerate and for libusb to notice)
     */
    const unsigned timeout_ms = 3000;

    struct timeval deadline;
    gettimeofday(&deadline, NULL);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_usec += timeout_ms % 1000;
    while (deadline.tv_usec > 1000000) {
        ++deadline.tv_sec;
        deadline.tv_usec -= 1000000;
    }

    while (!state->completed) {
        struct timeval now;
        gettimeofday(&now, NULL);

        if (now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_usec >= deadline.tv_usec))
            break; /* reached deadline, use whatever we've seen so far */

        /* set timeout to reflect the time remaining until our deadline */
        struct timeval timeout;
        timeout.tv_sec = deadline.tv_sec - now.tv_sec;
        if (deadline.tv_usec >= now.tv_usec)
            timeout.tv_usec = deadline.tv_usec - now.tv_usec;
        else {
            timeout.tv_sec--;
            timeout.tv_usec = deadline.tv_usec + 1000000 - now.tv_usec;
        }

        int usb_error;
        if ((usb_error = libusb_handle_events_timeout_completed(NULL, &timeout, &state->completed)) < 0) {
            log_perror_libusb(usb_error, "libusb_handle_events_timeout_completed");
            return NULL;
        }
    }

    /* either the completion flag got set (exact device match), or we timed out */

    libusb_device *post_hotplug;
    if (state->device) {
        /* exact match */
        post_hotplug = state->device;
    } else if (state->maybe_device) {
        /* timeout on the old port, but we saw a new PG2 appear on a different port */
        log_verbose("Device moved to different port %s (maybe this is a VM?)",
                    device_ports(state->maybe_device));
        post_hotplug = state->maybe_device;
    } else {
        /* timeout with no matches */
        log_error("Timed out waiting for firmware startup");
        return NULL;
    }

    switch (pg2sdr__identify_device(post_hotplug)) {
    case DEVTYPE_PG2SDR:
    case DEVTYPE_PROTOTYPE:
        log_verbose("Device re-enumerated on port %s", device_ports(post_hotplug));
        return libusb_ref_device(post_hotplug);

    case DEVTYPE_RECOVERY:
        log_error("Device re-enumerated on port %s in recovery mode (firmware startup failed)", device_ports(post_hotplug));
        return NULL;

    default:
        log_error("Device re-enumerated on port %s with an unexpected VID/PID", device_ports(post_hotplug));
        return NULL;
    }
}

void hotplug_cleanup(firmware_hotplug_state *state)
{
    if (!state)
        return;

    libusb_hotplug_deregister_callback(NULL, state->cb_handle);
    if (state->device) {
        libusb_unref_device(state->device);
        state->device = NULL;
    }
    if (state->maybe_device) {
        libusb_unref_device(state->maybe_device);
        state->maybe_device = NULL;
    }

    free(state);
}

