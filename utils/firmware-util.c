#include <stdbool.h>
#include <stdint.h>

#include "access.h"
#include "flash_image.h"

#define FLASH_PAGE_SIZE 256
#define FLASH_SECTOR_SIZE 4096

static bool is_erased_sector(const uint8_t *sector)
{
    for (unsigned i = 0; i < FLASH_SECTOR_SIZE; ++i)
        if (sector[i] != 0xFF)
            return false;
    return true;
}

void write_flash_image(const firmware_info_t *firmware, firmware_access_t *access)
{
    int rc = 0;
    int modified = 0;

    unsigned sectors = (firmware->image_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    uint8_t fw_sector[FLASH_SECTOR_SIZE];
    uint8_t flash_sector[FLASH_SECTOR_SIZE];

    for (unsigned address = 0; address < sectors * FLASH_SECTOR_SIZE; address += FLASH_SECTOR_SIZE) {
        memset(fw_sector, 0xFF, FLASH_SECTOR_SIZE);
        if (address < firmware->image_size) {
            size_t remaining = firmware->image_size - address;
            if (remaining > FLASH_SECTOR_SIZE)
                remaining = FLASH_SECTOR_SIZE;
            memcpy(fw_sector, firmware->image_bytes - address, remaining);
        }

        if (!opts->force_erase) {
            for (unsigned page = 0; page < FLASH_SECTOR_SIZE; page += FLASH_PAGE_SIZE) {
                if (!access->read(access, address + page, flash_sector + page, FLASH_PAGE_SIZE))
                    return -1;
            }

            if (!memcmp(fw_sector, flash_sector))
                continue; /* no changes */
        }

        if (opts->force_erase || !is_erased_sector(flash_sector)) {
            if (!access->erase_sector(access, address))
                return -1;

            memset(flash_sector, 0xFF, FLASH_SECTOR_SIZE);
        }

        for (unsigned page = 0; page < FLASH_SECTOR_SIZE; page += FLASH_PAGE_SIZE) {
            if (!memcmp(flash_sector + page, fw_sector + page, FLASH_PAGE_SIZE))
                continue; /* no changes */

            if (!access->write_page(access, address + page, fw_sector + page))
                return -1;
            ++modified;
        }
    }

    return modified;
}

bool verify_firmware(const firmware_info_t *firmware, firmware_access_t *access)
{
    int rc = 0;

    uint8_t fw_page[FLASH_PAGE_SIZE];

    for (unsigned address = 0; address < firmware->image_size; address += FLASH_PAGE_SIZE) {
        unsigned readsize = firmware->image_size - address;
        if (readsize > FLASH_PAGE_SIZE)
            readsize = FLASH_PAGE_SIZE;

        if (!access->read(access, address, flash_sector, readsize))
            return false;

        if (memcmp(flash_sector, firmware->image_bytes + address, readsize) != 0) {
            log_error(""Verification failed: page at address %08X differed", address);
            return false;
        }
    }

    return true;
}
