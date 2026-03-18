#include "internal/core.h"
#include "io.h"
#include "image.h"
#include "log.h"
#include "device.h"
#include "dfu_load.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>

int subcommand_write(int argc, char * const argv[]);
static void show_write_help();

int subcommand_verify(int argc, char * const argv[]);
static void show_verify_help();

static bool do_write_verify(const char *image_path, const char *serial_prefix, const char *port_path, bool dryrun, bool force_erase, bool verify_only);

static bool is_erased_page(const uint8_t *page);
static int write_image(const firmware_image_t *image, firmware_io_t *io, bool force_erase);
static bool verify_image(const firmware_image_t *image, firmware_io_t *io);

static void show_write_help()
{
    log_verbose("Usage: %s [OPTIONS] FIRMWARE-IMAGE\n"
                "This subcommand will write a new ProStick Gen 2 firmware image to flash\n"
                "storage, updating the firmware used when the ProStick resets or is\n"
                "disconnected.\n"
                "\n"
                "If the ProStick is currently running other firmware, that firmware is\n"
                "unaffected until the device is reset or disconnected.\n"
                "\n"
                "If the ProStick is currently in recovery mode, the provided firmware will\n"
                "first be downloaded to the device (as if 'pg2-util load' had been used)\n"
                "before writing the new firmware to flash storage.\n"
                "\n"
                "Available options:\n"
                "\n"
                " -h, --help             show this help\n"
                " -s, --serial PREFIX    specify serial number prefix of ProStick to affect\n"
                " -p, --port BUS-N[.N..] specify connected USB port of ProStick to affect\n"
                " -q, --quiet            suppress informational logging, show errors only\n"
                " -f, --force-erase      erase all flash sectors, not only changed sectors\n"
                " -n, --dry-run          run normal checks, but don't modify flash\n",
                argv0);
}

int subcommand_write(int argc, char * const argv[])
{
    struct option opts[] = {
        { "serial", required_argument, 0, 's' },
        { "port",   required_argument, 0, 'p' },
        { "help",   no_argument,       0, 'h' },
        { "quiet",  no_argument,       0, 'q' },
        { "dryrun", no_argument,       0, 'n' },
        { "force-erase", no_argument,  0, 'f' },
        { 0, 0, 0, 0 }
    };

    const char *serial_prefix = NULL;
    const char *port_path = NULL;
    bool dryrun = false;
    bool force_erase = false;

    int opt;
    while ((opt = getopt_long(argc, argv, "s:p:hqnf", opts, NULL)) != -1) {
        switch (opt) {
        case 's':
            serial_prefix = optarg;
            break;

        case 'p':
            port_path = optarg;
            break;

        case 'h':
            show_write_help(argv[0]);
            return EXIT_SUCCESS;

        case 'q':
            verbose_logging = false;
            break;

        case 'n':
            dryrun = true;
            break;

        case 'f':
            force_erase = true;
            break;

        case '?':
            return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        log_error("a firmware image filename is required");
        return EXIT_FAILURE;
    }

    if (optind + 1 < argc) {
        log_error("only one firmware image filename is expected");
        return EXIT_FAILURE;
    }

    return do_write_verify(argv[optind], serial_prefix, port_path, dryrun, force_erase, false) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void show_verify_help()
{
    log_verbose("Usage: %s [OPTIONS] FIRMWARE-IMAGE\n"
                "This subcommand compares the given FIRMWARE-IMAGE file to the firmware\n"
                "stored on a ProStick Gen 2's flash storage. If there are any differences\n"
                "an error will be reported, and the verify command will exit with a non-zero\n"
                "exit code.\n"
                "\n"
                "Available options:\n"
                "\n"
                " -h, --help             show this help\n"
                " -s, --serial PREFIX    specify serial number prefix of ProStick to affect\n"
                " -p, --port BUS-N[.N..] specify connected USB port of ProStick to affect\n"
                " -q, --quiet            suppress informational logging, show errors only",
                argv0);
}

int subcommand_verify(int argc, char * const argv[])
{
    struct option opts[] = {
        { "serial", required_argument, 0, 's' },
        { "port",   required_argument, 0, 'p' },
        { "help",   no_argument,       0, 'h' },
        { "quiet",  no_argument,       0, 'q' },
        { 0, 0, 0, 0 }
    };

    const char *serial_prefix = NULL;
    const char *port_path = NULL;

    int opt;
    while ((opt = getopt_long(argc, argv, "s:p:hq", opts, NULL)) != -1) {
        switch (opt) {
        case 's':
            serial_prefix = optarg;
            break;

        case 'p':
            port_path = optarg;
            break;

        case 'h':
            show_verify_help(argv[0]);
            return EXIT_SUCCESS;

        case 'q':
            verbose_logging = false;
            break;

        case '?':
            return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        log_error("a firmware image filename is required");
        return EXIT_FAILURE;
    }

    if (optind + 1 < argc) {
        log_error("only one firmware image filename is expected");
        return EXIT_FAILURE;
    }

    return do_write_verify(argv[optind], serial_prefix, port_path, false, false, true) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool do_write_verify(const char *image_path, const char *serial_prefix, const char *port_path, bool dryrun, bool force_erase, bool verify_only)
{
    firmware_io_t *file_io = NULL;
    firmware_io_t *flash_io = NULL;
    firmware_image_t *image = NULL;
    libusb_device *dev = NULL;
    bool success = false;

    if (!(file_io = io_open_file(image_path)))
        goto cleanup;

    if (!(image = image_read(file_io)))
        goto cleanup;

    dev = device_search(serial_prefix, port_path, SEARCH_PG2SDR | SEARCH_RECOVERY);
    if (!dev) {
        log_error("no suitable USB device found");
        goto cleanup;
    }

    switch (pg2sdr__identify_device(dev)) {
    case DEVTYPE_RECOVERY:
        {
            /* To write firmware to flash on a device in recovery mode, we need to get running firmware onto it via DFU first */
            log_verbose("Loading firmware to device on port %s: %s", device_ports(dev), device_string(dev));

            libusb_device *new_dev = NULL;
            if (!dfu_load(image, dev, &new_dev))
                goto cleanup;

            libusb_unref_device(dev);
            dev = new_dev;

            break;
        }

    case DEVTYPE_PG2SDR:
    case DEVTYPE_LEGACY:
        /* no extra work needed */
        break;

    default:
        log_error("device at %s does not seem to be a ProStick Gen 2", device_ports(dev));
        goto cleanup;
    }

    if (!(flash_io = io_open_flash(dev, dryrun)))
        goto cleanup;

    if (verify_only) {
        log_verbose("Verifying flash image on device on port %s: %s", device_ports(dev), device_string(dev));
        if (!verify_image(image, flash_io))
            goto cleanup;
        log_verbose("Flash content matches firmware image.");
    } else {
        log_verbose("Writing new flash image to device on port %s: %s", device_ports(dev), device_string(dev));
        int modified = write_image(image, flash_io, force_erase);
        if (modified < 0)
            goto cleanup;

        if (!modified) {
            log_verbose("Flash contents match the provided image, no changes were made");
        } else if (dryrun) {
            log_verbose("Dryrun mode, no changes were made, %d flash pages would have been updated", modified);
        } else {
            log_verbose("%d flash pages updated", modified);
            log_verbose("Verifying new flash contents");
            if (!verify_image(image, flash_io))
                goto cleanup;
            log_verbose("Firmware image successfully written");
        }
    }
    success = true;

 cleanup:
    if (flash_io)
        flash_io->close(flash_io);
    if (dev)
        libusb_unref_device(dev);
    if (image)
        image_free(image);
    if (file_io)
        file_io->close(file_io);

    return success;
}

/* Given a buffer of FLASH_PAGE_SIZE bytes at `page`,
 * return true iff the page is entirely erased (contains
 * only 0xFF bytes)
 */
static bool is_erased_page(const uint8_t *page)
{
    for (unsigned i = 0; i < FLASH_PAGE_SIZE; ++i)
        if (page[i] != 0xFF)
            return false;
    return true;
}

/* Write 'image' to flash using 'io'.
 *
 * If force_erase is false, only flash sectors that need changes will be erased first.
 * If force_erase is true, all affected flash sectors are erased.
 *
 * Returns the number of flash pages modified, or <0 if there was an error.
 *
 * Note that if force_erase is false and the image matches what's already stored on flash,
 * no flash pages are modified and write_flash_image will return 0.
 */
static int write_image(const firmware_image_t *image, firmware_io_t *io, bool force_erase)
{
    int modified = 0;

    unsigned sectors = (image->image_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    uint8_t fw_sector[FLASH_SECTOR_SIZE];
    uint8_t flash_sector[FLASH_SECTOR_SIZE];

    for (unsigned address = 0; address < sectors * FLASH_SECTOR_SIZE; address += FLASH_SECTOR_SIZE) {
        /* work out the desired contents of this sector
         * (usually just the bytes from the image, but slightly different
         * when we are past the end of the image - in that case we
         * want 0xFF padding)
         */
        memset(fw_sector, 0xFF, FLASH_SECTOR_SIZE);
        if (address < image->image_size) {
            size_t remaining = image->image_size - address;
            if (remaining > FLASH_SECTOR_SIZE)
                remaining = FLASH_SECTOR_SIZE;
            memcpy(fw_sector, image->image_bytes + address, remaining);
        }

        bool erase_sector = false;
        if (force_erase) {
            erase_sector = true;
        } else {
            /* Read existing flash sector contents, a page at a time.
             * Look for pages where we need to update the page, but the current flash page is not erased.
             * If there are any pages like that, we need to erase the whole sector.
             */
            for (unsigned page = 0; page < FLASH_SECTOR_SIZE; page += FLASH_PAGE_SIZE) {
                if (!io->read(io, address + page, flash_sector + page, FLASH_PAGE_SIZE))
                    return -1;

                if (memcmp(fw_sector + page, flash_sector + page, FLASH_PAGE_SIZE) != 0 && !is_erased_page(flash_sector + page)) {
                    erase_sector = true;
                }
            }
        }

        /* If needed, erase the whole sector and update our idea of the flash contents */
        if (erase_sector) {
            if (!io->erase_sector(io, address))
                return -1;

            memset(flash_sector, 0xFF, FLASH_SECTOR_SIZE);
        }

        /* Look for pages we need to update, and write them */
        for (unsigned page = 0; page < FLASH_SECTOR_SIZE; page += FLASH_PAGE_SIZE) {
            if (!memcmp(flash_sector + page, fw_sector + page, FLASH_PAGE_SIZE))
                continue; /* no changes to this page */

            assert(is_erased_page(flash_sector + page));

            if (!io->write_page(io, address + page, fw_sector + page))
                return -1;
            ++modified;
        }
    }

    return modified;
}

/* Compare 'image' to the contents of flash via 'io',
 * and complain if there are any differences found.
 *
 * Returns true if everything matches, false if there was a mismatch
 * or read error.
 */
static bool verify_image(const firmware_image_t *image, firmware_io_t *io)
{
    uint8_t flash_page[FLASH_PAGE_SIZE];

    for (unsigned address = 0; address < image->image_size; address += FLASH_PAGE_SIZE) {
        unsigned readsize = image->image_size - address;
        if (readsize > FLASH_PAGE_SIZE)
            readsize = FLASH_PAGE_SIZE;

        if (!io->read(io, address, flash_page, readsize))
            return false;

        for (unsigned i = 0; i < readsize; ++i) {
            if (flash_page[i] != image->image_bytes[address + i]) {
                log_error("Flash contents do not match image, first difference at offset 0x%04x", address + i);
                return false;
            }
        }
    }

    return true;
}
