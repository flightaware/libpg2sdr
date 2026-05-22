/*
 *  image.h - pg2-util firmware image helpers, declarations
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

#ifndef PG2_IMAGE_H
#define PG2_IMAGE_H

#include <stdint.h>
#include <stdio.h>

#include "io.h"
#include "firmware/pg2sdr_protocol.h"

typedef struct {
    /* total image size size, including header/suffix */
    uint32_t image_size;
    /* complete image contents, including header/suffix */
    uint8_t *image_bytes;
    /* bcdDevice from DFU suffix (firmware release number) */
    uint16_t dfu_release;
    /* CRC from DFU suffix */
    uint32_t dfu_crc;

    /* start of data to load over DFU / LOAD_IMAGE */
    uint8_t *load_bytes;
    /* number of bytes to load over DFU / LOAD_IMAGE */
    uint32_t load_size;

    /* embedded image metadata */
    firmware_metadata_t metadata;
} firmware_image_t;

/*
 * Load and verify a firmware image using `io` to
 * access the image data.
 *
 * Returns the loaded image, or NULL on error.
 *
 * Caller should eventually call `image_free`
 * when done with the loaded image.
 */
firmware_image_t *image_read(firmware_io_t *io);

/*
 * Free `image` previously returned from `image_read`.
 * If `image` is NULL, does nothing.
 */
void image_free(firmware_image_t *image);

void show_firmware_image(const char *indent, firmware_image_t *image, FILE *out);
void show_firmware_metadata(const char *indent, firmware_metadata_t *meta, FILE *out);

void json_firmware_image(firmware_image_t *image);
void json_firmware_metadata(firmware_metadata_t *meta);

#endif

