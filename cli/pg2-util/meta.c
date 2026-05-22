/*
 *  meta.c - pg2-util firmware metadata manipulation, implementation
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

#include "meta.h"

#include "internal/core.h"
#include "log.h"
#include "device.h"

#include <stdlib.h>
#include <string.h>

static void populate_active_meta(libusb_device *dev, port_metadata_t *meta)
{
    meta->active_firmware_valid = false;

    libusb_device_handle *handle = device_open(dev, false);
    if (!handle)
        return;

    int pg2_error;
    if ((pg2_error = pg2sdr__ctrl_get_metadata(handle, &meta->active_firmware, 0)) < 0) {
        log_perror_pg2sdr(pg2_error, "fetching firmware metadata failed");
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

static void populate_status(libusb_device *dev, port_metadata_t *meta)
{
    libusb_device_handle *handle = device_open(dev, false);
    if (!handle)
        return;

    ep0_in_board_status_t status;
    int pg2_error;
    if ((pg2_error = pg2sdr__ctrl_get_status(handle, &status, 0)) < 0) {
        log_perror_pg2sdr(pg2_error, "fetching board status failed");
        return;
    }

    meta->status_valid = true;
    meta->recovery_switch_on = (status.flags & STATUS_SW1_USBBOOT) != 0;
    meta->rf_power_on = (status.flags & STATUS_RF_POWER_ON) != 0;
    meta->adc_on = (status.flags & STATUS_HSADC_RUN) != 0;

    if (status.flags & STATUS_IS_PG2SDR)
        meta->hw_type = "pg2sdr";
    else if (status.flags & STATUS_IS_PG2SDR)
        meta->hw_type = "airspy";
    else
        meta->hw_type = "unknown";
}

port_metadata_t *meta_query(libusb_device *dev)
{
    port_metadata_t *meta;

    if (!(meta = calloc(1, sizeof(*meta)))) {
        log_perror("calloc");
        return NULL;
    }

    meta->port = pg2sdr__strdup_ports(NULL, dev);
    meta->device_type = pg2sdr__identify_device(dev);
    switch (meta->device_type) {
    case DEVTYPE_PG2SDR:
    case DEVTYPE_LEGACY:
        meta->serial = pg2sdr__strdup_serial(NULL, dev);
        populate_status(dev, meta);
        populate_active_meta(dev, meta);
        populate_flash_meta(dev, meta);
        break;

    default:
        /* nothing extra */
        break;
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
