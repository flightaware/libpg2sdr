#ifndef PG2SDR_CORE_H
#define PG2SDR_CORE_H

/* internal/core.h has the core functions needed by pg2-firmware only,
 * without any of the ADC / tuner / streaming / DSP code.
 *
 * Nothing implemented here should end up with symbol references to other
 * non-core parts of libpg2sdr, as we want to be able to link pg2-firmware
 * against the core code only.
 */

#include "pg2sdr.h"
#include "firmware/pg2sdr_protocol.h"

#define MAGIC_CTX 0x18273645
#define MAGIC_FREE 0xFEEE

struct pg2sdr__context {
    int magic;
    libusb_context *libusb_ctx;
    pg2sdr_log_callback log_cb;
};

#define CHECK_CTX(ctx)                       \
    do {                                     \
        if (!ctx)                            \
            return PG2SDR_ERROR_BAD_ARGUMENT;\
        if (ctx->magic != MAGIC_CTX)         \
            return PG2SDR_ERROR_CORRUPTION;  \
    } while (0)

/* context.c */
void pg2sdr__log(pg2sdr_context *ctx, pg2sdr_log_level level, const char *format, ...) __attribute__((format(printf, 3, 4)));

/* errors.c */
int pg2sdr__translate_libusb_error(int error);
int pg2sdr__translate_libusb_transfer_status(enum libusb_transfer_status status);
int pg2sdr__translate_errno(int error);

/* discovery.c */

/* ctx may be NULL, at the cost of no error logging */
char *pg2sdr__strdup_serial(pg2sdr_context *ctx, libusb_device *usb_dev);

/* ctx may be NULL, at the cost of no error logging */
char *pg2sdr__strdup_ports(pg2sdr_context *ctx, libusb_device *usb_dev);

/* this is also used as a bitset */
typedef enum {
    DEVTYPE_PG2SDR = 1,
    DEVTYPE_LEGACY = 2,
    DEVTYPE_RECOVERY = 4,
    DEVTYPE_OTHER = 8,
} device_type_t;

device_type_t pg2sdr__identify_device(libusb_device *lu_device);
ssize_t pg2sdr__discover_matching(pg2sdr_context *ctx,
                                  const char *match_serial_prefix,
                                  const char *match_ports,
                                  device_type_t match_types,
                                  pg2sdr_usb_device ***device_list);

/* control.c */
int pg2sdr__ctrl_get_status(libusb_device_handle *dev, ep0_in_board_status_t *status, unsigned timeout_ms);
int pg2sdr__ctrl_set_rf_power(libusb_device_handle *dev, rf_power_mode_t mode, unsigned timeout_ms);
int pg2sdr__ctrl_comms_check(libusb_device_handle *usb_handle, unsigned timeout_ms);
int pg2sdr__ctrl_start_transfer(libusb_device_handle *dev, const ep0_out_start_transfer_t *config, unsigned timeout_ms);
int pg2sdr__ctrl_stop_transfer(libusb_device_handle *dev, unsigned timeout_ms);
int pg2sdr__ctrl_tuner_update(libusb_device_handle *dev, uint16_t first, uint8_t *payload, uint16_t payload_size, unsigned timeout_ms);
int pg2sdr__ctrl_read_tuner_register(libusb_device_handle *dev, uint16_t first_reg, tuner_cache_mode_t cache_mode, uint8_t *buffer, uint16_t buffer_size, unsigned timeout_ms);
int pg2sdr__ctrl_update_tuner_lock(libusb_device_handle *dev, uint16_t vco_current, uint16_t lock_timeout_ms, unsigned timeout_ms);
int pg2sdr__ctrl_get_metadata(libusb_device_handle *dev, firmware_metadata_t *out, unsigned timeout_ms);
int pg2sdr__ctrl_flash_read_quad(libusb_device_handle *dev, uint32_t address, uint8_t *buf, uint16_t len, unsigned timeout_ms);
int pg2sdr__ctrl_flash_write(libusb_device_handle *dev, uint32_t address, const uint8_t *buf, uint16_t len, unsigned timeout_ms);
int pg2sdr__ctrl_flash_erase(libusb_device_handle *dev, uint32_t address, unsigned timeout_ms);
int pg2sdr__ctrl_load_image(libusb_device_handle *dev, uint32_t address, const uint8_t *buf, uint16_t len, unsigned timeout_ms);
int pg2sdr__ctrl_reset(libusb_device_handle *dev, unsigned timeout_ms);
int pg2sdr__ctrl_led_pattern(libusb_device_handle *dev, uint32_t pattern, unsigned timeout_ms);

#endif
