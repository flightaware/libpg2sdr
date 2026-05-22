/*
 *  io.c - pg2-util firmware image I/O helpers, implementation
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

#include "io.h"

#include "internal/core.h"
#include "log.h"
#include "device.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static void flash_close(firmware_io_t *io);
static bool flash_read(firmware_io_t *io, unsigned start, uint8_t *buf, unsigned len);
static bool flash_write_page(firmware_io_t *io, unsigned start, const uint8_t *buf);
static bool flash_erase_sector(firmware_io_t *io, unsigned start);
static bool dryrun_write_page(firmware_io_t *io, unsigned start, const uint8_t *buf);
static bool dryrun_erase_sector(firmware_io_t *io, unsigned start);

//
// File I/O object
//

typedef struct {
    firmware_io_t common;
    int fd; /* file descriptor for opened image file */
} io_file_t;

static void file_close(firmware_io_t *io);
static bool file_read(firmware_io_t *io, unsigned start, uint8_t *buf, unsigned len);
static bool file_write_page(firmware_io_t *io, unsigned start, const uint8_t *buf);
static bool file_erase_sector(firmware_io_t *io, unsigned start);

firmware_io_t *io_open_file(const char *path)
{
    io_file_t *io = calloc(1, sizeof(*io));
    if (!io) {
        log_perror("calloc");
        return NULL;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        log_perror("%s: open", path);
        return NULL;
    }

    io->common.what = strdup(path);
    io->common.read = file_read;
    io->common.close = file_close;
    io->common.write_page = file_write_page;
    io->common.erase_sector = file_erase_sector;
    io->fd = fd;

    return &io->common;
}

static void file_close(firmware_io_t *io)
{
    if (!io)
        return;

    io_file_t *file = (io_file_t*) io;
    if (file->fd >= 0)
        close(file->fd);

    free((char*)file->common.what);
    free(file);
}

static bool file_read(firmware_io_t *io, unsigned start, uint8_t *buf, unsigned len)
{
    io_file_t *file = (io_file_t*) io;

    ssize_t count;
    if ((count = pread(file->fd, buf, len, start)) < 0) {
        log_perror("%s: pread", io->what);
        return false;
    }

    if (count < len) {
        log_error("%s: short read (read %d bytes, expected %u bytes), image is truncated", io->what, (int)count, len);
        return false;
    }

    return true;
}

static bool file_write_page(firmware_io_t *io, unsigned start, const uint8_t *buf)
{
    log_error("%s: attempt to write_page on a file-backed image", io->what);
    return false;
}

static bool file_erase_sector(firmware_io_t *io, unsigned start)
{
    log_error("%s: attempt to erase_sector on a file-backed image", io->what);
    return false;
}

//
// Device flash I/O object
//

typedef struct {
    firmware_io_t common;
    libusb_device_handle *handle; /* handle of opened USB device */
} io_flash_t;

firmware_io_t *io_open_flash(libusb_device *dev, bool readonly)
{
    io_flash_t *io = NULL;
    libusb_device_handle *handle = NULL;;

    if (!(io = calloc(1, sizeof(*io)))) {
        log_perror("calloc");
        return NULL;
    }

    /* claim interface only if we are going to do writes */
    if (!(handle = device_open(dev, readonly ? false : true))) {
        goto error;
    }

    io->common.what = "SPI flash";
    io->common.read = flash_read;
    io->common.close = flash_close;
    io->common.write_page = readonly ? dryrun_write_page : flash_write_page;
    io->common.erase_sector = readonly ? dryrun_erase_sector : flash_erase_sector;
    io->handle = handle;
    return &io->common;

 error:
    if (handle)
        device_close(handle);
    free(io);
    return NULL;
}

static void flash_close(firmware_io_t *io)
{
    if (!io)
        return;

    io_flash_t *flash = (io_flash_t*) io;
    if (flash->handle) {
        device_close(flash->handle);
        flash->handle = NULL;
    }
    free(io);
}

static bool flash_read(firmware_io_t *io, unsigned start, uint8_t *buf, unsigned len)
{
    io_flash_t *flash = (io_flash_t*) io;
    while (len > 0) {
        unsigned page_len = FLASH_PAGE_SIZE - (start % FLASH_PAGE_SIZE); /* bytes after 'start' to the end of the page it's in */
        if (page_len > len)
            page_len = len;

        /* todo: timeout from firmware */
        int pg2_error = pg2sdr__ctrl_flash_read_quad(flash->handle, start, buf, page_len, /* timeout */ 0);
        if (pg2_error < 0) {
            log_perror_pg2sdr(pg2_error, "%s: FLASH_READ_QUAD", io->what);
            return false;
        }

        start += page_len;
        buf += page_len;
        len -= page_len;
    }

    return true;
}

static bool flash_write_page(firmware_io_t *io, unsigned start, const uint8_t *buf)
{
    if (start % FLASH_PAGE_SIZE != 0) {
        log_error("%s: attempt to write unaligned flash page at 0x%08x",
                  io->what, start);
        return false;
    }

    io_flash_t *flash = (io_flash_t*) io;

    /* todo: timeout from firmware */
    int pg2_error = pg2sdr__ctrl_flash_write(flash->handle,
                                             start,
                                             buf,
                                             FLASH_PAGE_SIZE,         /* wLength */
                                             0);                      /* timeout */
    if (pg2_error < 0) {
        log_perror_pg2sdr(pg2_error, "%s: FLASH_WRITE", io->what);
        return false;
    }

    return true;
}

static bool dryrun_write_page(firmware_io_t *io, unsigned start, const uint8_t *buf)
{
    log_verbose("dryrun: would have written flash page at address 0x%08x\n", start);
    return true;
}

static bool flash_erase_sector(firmware_io_t *io, unsigned start)
{
    if (start % FLASH_SECTOR_SIZE != 0) {
        log_error("%s: attempt to erase unaligned flash sector at 0x%08x",
                  io->what, start);
        return false;
    }

    io_flash_t *flash = (io_flash_t*) io;

    /* todo: timeout from firmware */
    int pg2_error = pg2sdr__ctrl_flash_erase(flash->handle, start, /* timeout */ 0);
    if (pg2_error < 0) {
        log_perror_pg2sdr(pg2_error, "%s: FLASH_ERASE", io->what);
        return false;
    }

    return true;
}

static bool dryrun_erase_sector(firmware_io_t *io, unsigned start)
{
    log_verbose("dryrun: would have erased flash sector at address 0x%08x\n", start);
    return true;
}
