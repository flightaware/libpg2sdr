#ifndef PG2_IMAGE_H
#define PG2_IMAGE_H

#include <stdint.h>
#include <stdio.h>

#include "io.h"
#include "pg2sdr_protocol.h"

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

