/*
 *  io.h - pg2-util firmware image I/O helpers, declarations
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

#ifndef PG2_IO_H
#define PG2_IO_H

#include <stdbool.h>
#include <stdint.h>

#include <libusb-1.0/libusb.h>

/* I/O abstraction over files and device flash.
 *
 * Each of these method-like function pointers expect
 * to be passed, as their first argument, a pointer to
 * the structure they're operating on. i.e. call them
 * like this:
 *
 *   firmware_io_t *io = io_open_file(...);
 *   io->read(io, ...);
 *
 */
typedef struct firmware_io_s {
    /* A brief description of what this I/O object accesses */
    const char *what;

    /* read(io, offset, buffer, length):
     *
     *   Read `length` bytes at offset `offset` into `buffer`.
     *   This does not need to be page-aligned, or limited to
     *   only one flash page - arbitrary ranges can be read.
     *
     *   Return true on success, false on error
     */
    bool (*read)(struct firmware_io_s *io,
                 unsigned offset,
                 uint8_t *buffer,
                 unsigned length);

    /* close(io):
     *
     *   Close and free this I/O object.
     */
    void (*close)(struct firmware_io_s *io);

    /* write_page(io, offset, buffer):
     *
     *   Write one 256-byte page to flash at address `offset`,
     *   using 256 bytes of data from `buffer`.
     *
     *   `offset` must be page-aligned (a multiple of 256)
     *   and the page should be empty before the write
     *   (i.e. all bytes 0xFF, see erase_sector)
     *
     *   Return true on success, false on error.
     */
    bool (*write_page)(struct firmware_io_s *io,
                       unsigned offset,
                       const uint8_t *buffer);

    /* erase_sector(io, offset):
     *
     *   Erase one 4096-byte flash sector at address `offset`.
     *   This erases the 16 256-byte pages that make up the sector,
     *   filling them with 0xFF bytes.
     *
     *   `offset` must be sector-aligned (a multiple of 4096).
     *
     *   Return true on success, false on error.
     */
    bool (*erase_sector)(struct firmware_io_s *io,
                         unsigned offset);
} firmware_io_t;

/*
 * Create an I/O object that provides read-only access
 * to the file at `path`. Return the new object, or NULL on error.
 */
firmware_io_t *io_open_file(const char *path);

/*
 * Create an I/O object that provides access
 * to the flash on the device at `dev`.
 *
 * If dryrun is true, access is read-only, and attempts to
 * write data will just log about it and not actually perform
 * the write.
 *
 * Return the new object, or NULL on error.
 */
firmware_io_t *io_open_flash(libusb_device *dev, bool dryrun);

/* TODO: this should probably be in firmware/pg2sdr_protocol.h? */
#define FLASH_PAGE_SIZE 256
#define FLASH_SECTOR_SIZE 4096

#endif
