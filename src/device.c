/*
 *  device.c - PG2 host library, device discovery and lifecycle API
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

#include <pthread.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#include "internal/lib.h"

static int build_device(pg2sdr_context *ctx, libusb_device *lu_device, char *serial, char *ports, pg2sdr_device **out)
{
    int error, usb_error;
    pg2sdr_device *dev = NULL;

    if (!serial || !ports) {
        error = PG2SDR_ERROR_NO_MEMORY;
        goto cleanup_nomutex;
    }

    if (!(dev = calloc(1, sizeof(*dev)))) {
        error = PG2SDR_ERROR_NO_MEMORY;
        goto cleanup_nomutex;
    }

    pthread_mutexattr_t attrs;
    if (pthread_mutexattr_init(&attrs) < 0 || pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE) < 0 || pthread_mutex_init(&dev->mutex, &attrs) < 0) {
        error = pg2sdr__translate_errno(errno);
        goto cleanup_nomutex;
    }

    dev->magic = MAGIC_DEV;
    dev->ctx = ctx;
    dev->serial = serial;
    dev->ports = ports;

    LOGDEBUG(dev, "opening device with serial %s on port %s", serial, ports);

    if ((usb_error = libusb_open(lu_device, &dev->usb_handle)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto cleanup;
    }

    /* set configuration 1 always (this does a soft reset of USB state) */
    if ((usb_error = libusb_set_configuration(dev->usb_handle, 1)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto cleanup;
    }

    /* claim interface 0, the main data-streaming interface; only one thing can
     * have that claimed */
    if ((usb_error = libusb_claim_interface(dev->usb_handle, 0)) < 0) {
        error = pg2sdr__translate_libusb_error(usb_error);
        goto cleanup;
    }

    /* verify we can talk to the device, grab metadata, check versions */
    if ((error = pg2sdr__ctrl_comms_check(dev->usb_handle, /* timeout_ms */ 0)) < 0) {
        LOGERROR(dev, "basic USB communications check failed, is this device really a PG2SDR?");
        goto cleanup;
    }

    firmware_metadata_t meta;
    if ((error = pg2sdr__ctrl_get_metadata(dev->usb_handle, &meta, /* timeout_ms */ 0)) < 0)
        goto cleanup;

    LOGDEBUG(dev, "library version: %u.%u.%u.%u firmware version: %u.%u.%u.%u firmware compat: %u.%u.%u.%u",
             (PG2_CURRENT_VERSION >> 24) & 255,
             (PG2_CURRENT_VERSION >> 16) & 255,
             (PG2_CURRENT_VERSION >> 8) & 255,
             (PG2_CURRENT_VERSION >> 0) & 255,
             (meta.version >> 24) & 255,
             (meta.version >> 16) & 255,
             (meta.version >> 8) & 255,
             (meta.version >> 0) & 255,
             (meta.compat >> 24) & 255,
             (meta.compat >> 16) & 255,
             (meta.compat >> 8) & 255,
             (meta.compat >> 0) & 255);

    if (PG2_CURRENT_VERSION < meta.compat) {
        LOGERROR(dev,
                 "host library protocol %u.%u.%u.%u < firmware minimum compatible protocol %u.%u.%u.%u, upgrade host library",
                 (PG2_CURRENT_VERSION >> 24) & 255,
                 (PG2_CURRENT_VERSION >> 16) & 255,
                 (PG2_CURRENT_VERSION >> 8) & 255,
                 (PG2_CURRENT_VERSION >> 0) & 255,
                 (meta.compat >> 24) & 255,
                 (meta.compat >> 16) & 255,
                 (meta.compat >> 8) & 255,
                 (meta.compat >> 0) & 255);
        error = PG2SDR_ERROR_FIRMWARE_MISMATCH;
        goto cleanup;
    }

    if (meta.version < PG2_COMPAT_VERSION) {
        LOGERROR(dev,
                 "firmware protocol %u.%u.%u.%u < host library minimum compatible protocol %u.%u.%u.%u, upgrade firmware",
                 (meta.version >> 24) & 255,
                 (meta.version >> 16) & 255,
                 (meta.version >> 8) & 255,
                 (meta.version >> 0) & 255,
                 (PG2_COMPAT_VERSION >> 24) & 255,
                 (PG2_COMPAT_VERSION >> 16) & 255,
                 (PG2_COMPAT_VERSION >> 8) & 255,
                 (PG2_COMPAT_VERSION >> 0) & 255);
        error = PG2SDR_ERROR_FIRMWARE_MISMATCH;
        goto cleanup;
    }

    dev->fw_version = meta.version;
    dev->control_timeout_ms = meta.control_timeout_ms;

    ep0_in_board_status_t status;
    if ((error = pg2sdr__ctrl_get_status(dev->usb_handle, &status, dev->control_timeout_ms)) < 0)
        goto cleanup;

    dev->conversion_mode = PG2SDR_MODE_BASEBAND;

    dev->adc_limit = 28e6;

    dev->requested_sample_rate = 0;
    dev->adc_pll_config.valid = false;
    dev->changing_rate = false;

    dev->requested_frequency = 0;
    dev->sideband_mode = PG2SDR_SIDEBAND_LOWER;
    dev->tuner_pll_config.valid = false;
    dev->changing_freq = false;

    dev->requested_bandpass_low = -100e6;
    dev->requested_bandpass_high = 100e6;
    dev->changing_bandpass = false;

    dev->decimation_mode = PG2SDR_DECIMATION_AUTO;
    dev->undersampling_mode = 1;

    dev->usb_bytes_per_block = status.usb_bytes_per_block;
    dev->usb_samples_per_block = status.usb_samples_per_block;
    dev->tuner_xtal = status.tuner_xtal;
    dev->buffer_size = 262144; /* default 128k (complex) */

    if ((error = pg2sdr__init_tuner(dev)) < 0)
        goto cleanup;

    /* set default gain tables */
    if ((error = pg2sdr_set_gain_tables(dev,
                                        pg2sdr__default_gain_table, pg2sdr__default_gain_table_size,
                                        pg2sdr__default_lna_table,
                                        pg2sdr__default_mix_table,
                                        pg2sdr__default_vga_table)) < 0)
        goto cleanup;

    /* set default bandpass table */
    if ((error = pg2sdr_set_bandpass_table(dev, pg2sdr__default_bandpass_table, pg2sdr__default_bandpass_table_size)) < 0)
        goto cleanup;

    *out = dev;
    return PG2SDR_SUCCESS;

cleanup:
    free(dev->bandpass_table);
    free(dev->gain_table);
    if (dev->usb_handle)
        libusb_close(dev->usb_handle);
    pthread_mutex_destroy(&dev->mutex);
    dev->magic = MAGIC_FREE;

cleanup_nomutex:
    free(dev);
    free(serial);
    free(ports);
    return error;
}

ssize_t pg2sdr_discover_devices(pg2sdr_context *ctx,
                                const char *match_serial_prefix,
                                const char *match_ports,
                                pg2sdr_usb_device ***device_list)
{
    return pg2sdr__discover_matching(ctx,
                                     match_serial_prefix,
                                     match_ports,
                                     DEVTYPE_PG2SDR,
                                     device_list);
}

int pg2sdr_open_single_device(pg2sdr_context *ctx, const char *match_serial_prefix, const char *match_ports, pg2sdr_device **device)
{
    int error = PG2SDR_SUCCESS;
    pg2sdr_usb_device **devices;

    ssize_t device_count = pg2sdr_discover_devices(ctx, match_serial_prefix, match_ports, &devices);
    if (device_count < 0)
        return device_count;

    if (device_count == 0) {
        error = PG2SDR_ERROR_NOT_FOUND;
        goto cleanup;
    }

    if (device_count > 1) {
        error = PG2SDR_ERROR_MULTIPLE_DEVICES;
        goto cleanup;
    }

    error = pg2sdr_open_device(ctx, devices[0], device);

cleanup:
    pg2sdr_free_device_list(devices);
    return error;
}

int pg2sdr_open_device(pg2sdr_context *ctx, pg2sdr_usb_device *usb_device, pg2sdr_device **device)
{
    CHECK_CTX(ctx);

    if (!usb_device || !device)
        return PG2SDR_ERROR_BAD_ARGUMENT;

    return build_device(ctx, usb_device->lu_device, strdup(usb_device->serial), strdup(usb_device->ports), device);
}

int pg2sdr_open_libusb_device(pg2sdr_context *ctx, libusb_device *lu_device, pg2sdr_device **device)
{
    CHECK_CTX(ctx);

    if (!lu_device || !device)
        return PG2SDR_ERROR_BAD_ARGUMENT;

    return build_device(ctx, lu_device, pg2sdr__strdup_serial(ctx, lu_device), pg2sdr__strdup_ports(ctx, lu_device), device);
}

int pg2sdr_close_device(pg2sdr_device *dev)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    if (dev->streaming) {
        pthread_mutex_unlock(&dev->mutex);
        return PG2SDR_ERROR_BAD_STATE;
    }

    int error = pg2sdr__ctrl_set_rf_power(dev->usb_handle, RF_POWER_OFF, dev->control_timeout_ms);
    if (error < 0) {
        char buf[1024];
        LOGERROR(dev, "warning: could not disable RF power on device close: %s", pg2sdr_strerror_r(error, buf, sizeof(buf)));
        /* continue anyway */
    }

    free(dev->gain_table);
    free(dev->bandpass_table);

    libusb_close(dev->usb_handle);
    dev->magic = MAGIC_FREE;
    pthread_mutex_unlock(&dev->mutex);
    pthread_mutex_destroy(&dev->mutex);

    free(dev);

    return PG2SDR_SUCCESS;
}

const char *pg2sdr_get_serial(pg2sdr_device *dev)
{
    if (!dev || !dev->ctx || dev->magic != MAGIC_DEV || dev->ctx->magic != MAGIC_CTX)
        return NULL;

    return dev->serial;
}

const char *pg2sdr_get_ports(pg2sdr_device *dev)
{
    if (!dev || !dev->ctx || dev->magic != MAGIC_DEV || dev->ctx->magic != MAGIC_CTX)
        return NULL;

    return dev->ports;
}

uint32_t pg2sdr_get_firmware_version(pg2sdr_device *dev)
{
    if (!dev || !dev->ctx || dev->magic != MAGIC_DEV || dev->ctx->magic != MAGIC_CTX)
        return 0 ;

    return dev->fw_version;
}
