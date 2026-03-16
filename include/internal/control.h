#ifndef PG2SDR_CONTROL_H
#define PG2SDR_CONTROL_H

#include <stdint.h>
#include <libusb-1.0/libusb.h>

#include "firmware/pg2sdr_protocol.h"

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

#endif
