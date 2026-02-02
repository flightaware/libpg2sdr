#include "io.h"
#include "log.h"
#include "pg2sdr_protocol.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* TODO: get this from the firmware or pg2sdr_protocol.h */
#define CONTROL_TIMEOUT 1000

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

firmware_io_t *io_open_flash(libusb_device *dev, bool dryrun)
{
    io_flash_t *io = NULL;
    libusb_device_handle *handle = NULL;;
    int usb_error;

    if (!(io = calloc(1, sizeof(*io)))) {
        log_perror("calloc");
        return NULL;
    }

    if ((usb_error = libusb_open(dev, &handle)) < 0) {
        log_perror_libusb(usb_error, "libusb_open");
        goto error;
    }

    int config;
    if ((usb_error = libusb_get_configuration(handle, &config)) < 0) {
        log_perror_libusb(usb_error, "libusb_get_configuration");
        goto error;
    }

    if (config == 0) {
        if ((usb_error = libusb_set_configuration(handle, 1) < 0)) {
            log_perror_libusb(usb_error, "libusb_set_configuration(1)");
            goto error;
        }
    }

    io->common.what = "SPI flash";
    io->common.read = flash_read;
    io->common.close = flash_close;
    io->common.write_page = dryrun ? dryrun_write_page : flash_write_page;
    io->common.erase_sector = dryrun ? dryrun_erase_sector : flash_erase_sector;
    io->handle = handle;
    return &io->common;

 error:
    if (handle)
        libusb_close(handle);
    free(io);
    return NULL;
}

static void flash_close(firmware_io_t *io)
{
    if (!io)
        return;

    io_flash_t *flash = (io_flash_t*) io;
    if (flash->handle) {
        libusb_close(flash->handle);
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

        int count = libusb_control_transfer(flash->handle,
                                            LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
                                            EP0_IN_FLASH_READ_QUAD,  /* bRequest */
                                            start & 0xFFFF,          /* wValue */
                                            start >> 16,             /* wIndex */
                                            buf,
                                            page_len,                /* wLength */
                                            CONTROL_TIMEOUT);
        if (count < 0) {
            log_perror_libusb(count, "%s: libusb_control_transfer(EP0_IN_FLASH_READ_QUAD)", io->what);
            return false;
        }

        if (count < page_len) {
            log_error("%s: short read (read %d bytes, expected %u bytes)", io->what, count, page_len);
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
    int usb_error = libusb_control_transfer(flash->handle,
                                            LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
                                            EP0_OUT_FLASH_WRITE,     /* bRequest */
                                            start & 0xFFFF,          /* wValue */
                                            start >> 16,             /* wIndex */
                                            (uint8_t*)buf,
                                            FLASH_PAGE_SIZE,         /* wLength */
                                            CONTROL_TIMEOUT);
    if (usb_error < 0) {
        log_perror_libusb(usb_error, "%s: libusb_control_transfer(EP0_OUT_FLASH_WRITE)", io->what);
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
    int usb_error = libusb_control_transfer(flash->handle,
                                            LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
                                            EP0_OUT_FLASH_ERASE,     /* bRequest */
                                            start & 0xFFFF,          /* wValue */
                                            start >> 16,             /* wIndex */
                                            NULL,
                                            0,                       /* wLength */
                                            CONTROL_TIMEOUT);
    if (usb_error < 0) {
        log_perror_libusb(usb_error, "%s: EP0_OUT_FLASH_ERASE", io->what);
        return false;
    }

    return true;
}

static bool dryrun_erase_sector(firmware_io_t *io, unsigned start)
{
    log_verbose("dryrun: would have erased flash sector at address 0x%08x\n", start);
    return true;
}
