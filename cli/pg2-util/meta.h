/*
 *  meta.h - pg2-util firmware metadata manipulation, declarations
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

#ifndef PG2_META_H
#define PG2_META_H

#include "firmware/pg2sdr_protocol.h"

#include "image.h"

typedef struct {
    const char *port;   /* strdup'd bus-port[.port[.port ...]] */
    int device_type;    /* DEVTYPE_* */
    const char *serial; /* strdup'd PG2 serial number, or NULL if unavailable */

    bool active_firmware_valid;
    firmware_metadata_t active_firmware;

    bool status_valid;        /* are following fields valid? */
    bool recovery_switch_on;  /* boot switch state (true = recovery mode) */
    bool rf_power_on;         /* RF power state */
    bool adc_on;              /* ADC streaming state */
    const char *hw_type;      /* target hardware type */

    firmware_image_t *flash_image;
} port_metadata_t;

port_metadata_t *meta_query(libusb_device *dev);
void meta_free(port_metadata_t *meta);

#endif
