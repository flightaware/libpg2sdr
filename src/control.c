#include "internal/control.h"

#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <endian.h>

#include "pg2sdr.h"
#include "internal/errors.h"

#define DEFAULT_CONTROL_TIMEOUT 1000

static int control_in(libusb_device_handle *usb_handle,
                      uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data,
                      uint16_t wLength,
                      unsigned timeout_ms)
{
    int count = libusb_control_transfer(usb_handle,
                                        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
                                        bRequest,
                                        wValue,
                                        wIndex,
                                        data,
                                        wLength,
                                        timeout_ms ? timeout_ms : DEFAULT_CONTROL_TIMEOUT);
    if (count < 0)
        return pg2sdr__translate_libusb_error(count);
    if (count != wLength)
        return PG2SDR_ERROR_FIRMWARE_MISMATCH;
    return PG2SDR_SUCCESS;
}

static int control_out(libusb_device_handle *usb_handle,
                       uint8_t bRequest, uint16_t wValue, uint16_t wIndex, const unsigned char *data,
                       uint16_t wLength,
                       unsigned timeout_ms)
{
    int count = libusb_control_transfer(usb_handle,
                                        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
                                        bRequest,
                                        wValue,
                                        wIndex,
                                        (unsigned char *)data,
                                        wLength,
                                        timeout_ms ? timeout_ms : DEFAULT_CONTROL_TIMEOUT);
    if (count < 0)
        return pg2sdr__translate_libusb_error(count);
    if (count != wLength)
        return PG2SDR_ERROR_FIRMWARE_MISMATCH;
    return PG2SDR_SUCCESS;
}

/* For tuner-specific control transfers, try to refine LIBUSB_ERROR_PIPE */

static int tuner_control_in(libusb_device_handle *usb_handle,
                            uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data,
                            uint16_t wLength,
                            unsigned timeout_ms)
{
    int error = control_in(usb_handle, bRequest, wValue, wIndex, data, wLength, timeout_ms);
    if (error == (PG2SDR_ERROR_LIBUSB_MIN - LIBUSB_ERROR_PIPE)) {
        /* check if an I2C error was seen */
        ep0_in_board_status_t status;
        if (pg2sdr__ctrl_get_status(usb_handle, &status, timeout_ms) >= 0 && (status.flags & STATUS_TUNER_I2C_ERROR) != 0)
            return PG2SDR_ERROR_TUNER_I2C;
    }

    return error;
}

static int tuner_control_out(libusb_device_handle *usb_handle,
                             uint8_t bRequest, uint16_t wValue, uint16_t wIndex, const unsigned char *data,
                             uint16_t wLength,
                             unsigned timeout_ms)
{
    int error = control_out(usb_handle, bRequest, wValue, wIndex, data, wLength, timeout_ms);
    if (error == (PG2SDR_ERROR_LIBUSB_MIN - LIBUSB_ERROR_PIPE)) {
        /* check if an I2C error was seen */
        ep0_in_board_status_t status;
        if (pg2sdr__ctrl_get_status(usb_handle, &status, timeout_ms) >= 0 && (status.flags & STATUS_TUNER_I2C_ERROR) != 0)
            return PG2SDR_ERROR_TUNER_I2C;
    }

    return error;
}

int pg2sdr__ctrl_start_transfer(libusb_device_handle *dev, const ep0_out_start_transfer_t *config, unsigned timeout_ms)
{
    /* byteswap all the things */
    ep0_out_start_transfer_t swapped = {
        .n_divisor = htole32(config->n_divisor),
        .m_divisor = htole32(config->m_divisor),
        .p_divisor = htole32(config->p_divisor),
        .idiv_divisor = htole32(config->idiv_divisor)
    };

    return control_out(dev,
                       EP0_OUT_START_TRANSFER,
                       0,
                       0,
                       (unsigned char *)&swapped,
                       sizeof(swapped),
                       timeout_ms);
}

int pg2sdr__ctrl_stop_transfer(libusb_device_handle *dev, unsigned timeout_ms)
{
    return control_out(dev,
                       EP0_OUT_STOP_TRANSFER,
                       0,
                       0,
                       NULL,
                       0,
                       timeout_ms);
}

int pg2sdr__ctrl_get_status(libusb_device_handle *dev, ep0_in_board_status_t *out, unsigned timeout_ms)
{
    ep0_in_board_status_t status;
    int error = control_in(dev,
                           EP0_IN_BOARD_STATUS,
                           0,
                           0,
                           (unsigned char *)&status,
                           sizeof(status),
                           timeout_ms);
    if (error < 0) {
        return error;
    }

    /* byteswap all the things, if needed
     * (these are probably all no-ops, but anyway)
     */
    out->flags = le32toh(status.flags);
    out->hsadc_frequency = le32toh(status.hsadc_frequency);
    out->pll_stat = le32toh(status.pll_stat);
    out->pll_ctrl = le32toh(status.pll_ctrl);
    out->pll_mdiv = le32toh(status.pll_mdiv);
    out->pll_np_div = le32toh(status.pll_np_div);
    out->pll_frac = le32toh(status.pll_frac);
    out->idiv_e_ctrl = le32toh(status.idiv_e_ctrl);
    out->adchs_fifo_cfg = le32toh(status.adchs_fifo_cfg);
    out->adchs_config = le32toh(status.adchs_config);
    out->adchs_adc_speed = le32toh(status.adchs_adc_speed);
    out->adchs_power_control = le32toh(status.adchs_power_control);
    out->adchs_int0_status = le32toh(status.adchs_int0_status);
    out->adchs_fifo_sts = le32toh(status.adchs_fifo_sts);
    out->adchs_dscr_sts = le32toh(status.adchs_dscr_sts);
    out->gpdma_config = le32toh(status.gpdma_config);
    out->gpdma_enbldchns = le32toh(status.gpdma_enbldchns);
    out->gpdma_rawinttcstat = le32toh(status.gpdma_rawinttcstat);
    out->gpdma_rawinterrstat = le32toh(status.gpdma_rawinterrstat);
    out->gpdma0_config = le32toh(status.gpdma0_config);
    out->gpdma0_control = le32toh(status.gpdma0_control);
    out->gpdma0_srcaddr = le32toh(status.gpdma0_srcaddr);
    out->gpdma0_destaddr = le32toh(status.gpdma0_destaddr);
    out->gpdma0_lli = le32toh(status.gpdma0_lli);
    out->current_lli = le32toh(status.current_lli);
    out->next_sequence = le32toh(status.next_sequence);
    /* tuner_regs is a uint8_t array and doesn't need modification */
    out->usb_free_buffers = le32toh(status.usb_free_buffers);
    out->usb_filled_buffers = le32toh(status.usb_filled_buffers);
    out->usb_samples_per_block = le32toh(status.usb_samples_per_block);
    out->usb_bytes_per_block = le32toh(status.usb_bytes_per_block);
    out->m4_freq = le32toh(status.m4_freq);
    out->m4_mean_idle = le32toh(status.m4_mean_idle);
    out->m4_mean_idle_scale = le32toh(status.m4_mean_idle_scale);
    out->m4_min_idle = le32toh(status.m4_min_idle);
    out->m4_min_idle_scale = le32toh(status.m4_min_idle_scale);
    out->clock_32k = le32toh(status.clock_32k);
    out->clock_irc = le32toh(status.clock_irc);
    out->clock_pll0usb = le32toh(status.clock_pll0usb);
    out->clock_pll0audio = le32toh(status.clock_pll0audio);
    out->clock_pll1 = le32toh(status.clock_pll1);
    out->clock_idiv_a = le32toh(status.clock_idiv_a);
    out->clock_idiv_b = le32toh(status.clock_idiv_b);
    out->clock_idiv_c = le32toh(status.clock_idiv_c);
    out->clock_idiv_d = le32toh(status.clock_idiv_d);
    out->clock_idiv_e = le32toh(status.clock_idiv_e);
    out->tuner_xtal = le32toh(status.tuner_xtal);
    out->serial_number = le64toh(status.serial_number);

    return PG2SDR_SUCCESS;
}

int pg2sdr__ctrl_get_metadata(libusb_device_handle *dev, firmware_metadata_t *out, unsigned timeout_ms)
{
    firmware_metadata_t meta;
    int error = control_in(dev,
                           EP0_IN_METADATA,
                           0,
                           0,
                           (unsigned char *)&meta,
                           sizeof(meta),
                           timeout_ms);
    if (error < 0) {
        return error;
    }

    /* byteswap all the things */
    out->version = le32toh(meta.version);
    out->compat = le32toh(meta.compat);
    out->max_control_transfer = le16toh(meta.max_control_transfer);
    out->control_timeout_ms = le16toh(meta.control_timeout_ms);
    memcpy(out->build_type, meta.build_type, sizeof(meta.build_type));
    out->build_type[sizeof(out->build_type)-1] = 0;

    return PG2SDR_SUCCESS;
}

int pg2sdr__ctrl_comms_check(libusb_device_handle *dev, unsigned timeout_ms)
{
    int error;
    ep0_in_comms_check_t in_check;

    if ((error = control_in(dev,
                            EP0_IN_COMMS_CHECK,
                            0,
                            0,
                            (unsigned char *)&in_check,
                            sizeof(in_check),
                            timeout_ms)) < 0) {
        return error;
    }

    if (le32toh(in_check.magic) != COMMS_CHECK_MAGIC)
        return PG2SDR_ERROR_FIRMWARE_MISMATCH;

    ep0_out_comms_check_t out_check;
    out_check.magic = htole32(COMMS_CHECK_MAGIC);
    if ((error = control_out(dev,
                             EP0_OUT_COMMS_CHECK,
                             0,
                             0,
                             (unsigned char *)&out_check,
                             sizeof(out_check),
                             timeout_ms)) < 0) {
        return error;
    }

    return PG2SDR_SUCCESS;
}

int pg2sdr__ctrl_tuner_update(libusb_device_handle *dev, uint16_t first, uint8_t *payload, uint16_t payload_size, unsigned timeout_ms)
{
    return tuner_control_out(dev,
                             EP0_OUT_TUNER_UPDATE,
                             first,
                             0,
                             (unsigned char *) payload,
                             payload_size,
                             timeout_ms);
}

int pg2sdr__ctrl_set_rf_power(libusb_device_handle *dev, rf_power_mode_t mode, unsigned timeout_ms)
{
    return control_out(dev,
                       EP0_OUT_SET_RF_POWER,
                       (uint16_t) mode,
                       0,
                       NULL,
                       0,
                       timeout_ms);
}

int pg2sdr__ctrl_read_tuner_register(libusb_device_handle *dev, uint16_t first_reg, tuner_cache_mode_t cache_mode, uint8_t *buffer, uint16_t buffer_size, unsigned timeout_ms)
{
    return tuner_control_in(dev,
                            EP0_IN_TUNER_READ,
                            first_reg,
                            (uint16_t) cache_mode,
                            buffer,
                            buffer_size,
                            timeout_ms);
}

int pg2sdr__ctrl_update_tuner_lock(libusb_device_handle *dev, uint16_t vco_current, uint16_t lock_timeout, unsigned timeout_ms) {
    ep0_in_tuner_lock_t out;
    int error = tuner_control_in(dev,
                                 EP0_IN_TUNER_LOCK,
                                 vco_current,
                                 lock_timeout,
                                 (unsigned char *) &out,
                                 sizeof(out),
                                 timeout_ms);

    if (error < 0)
        return error;

    return out.pll_locked;
}

int pg2sdr__ctrl_flash_read_quad(libusb_device_handle *dev, uint32_t address, uint8_t *buf, uint16_t len, unsigned timeout_ms)
{
    return control_in(dev,
                      EP0_IN_FLASH_READ_QUAD,
                      address & 0xFFFF,          /* wValue */
                      address >> 16,             /* wIndex */
                      buf,
                      len,
                      timeout_ms);
}

int pg2sdr__ctrl_flash_write(libusb_device_handle *dev, uint32_t address, const uint8_t *buf, uint16_t len, unsigned timeout_ms)
{
    return control_out(dev,
                       EP0_OUT_FLASH_WRITE,
                       address & 0xFFFF,          /* wValue */
                       address >> 16,             /* wIndex */
                       buf,
                       len,
                       timeout_ms);
}

int pg2sdr__ctrl_flash_erase(libusb_device_handle *dev, uint32_t address, unsigned timeout_ms)
{
    return control_out(dev,
                       EP0_OUT_FLASH_ERASE,
                       address & 0xFFFF,          /* wValue */
                       address >> 16,             /* wIndex */
                       NULL,                      /* buf */
                       0,                         /* wLength */
                       timeout_ms);
}

int pg2sdr__ctrl_load_image(libusb_device_handle *dev, uint32_t address, const uint8_t *buf, uint16_t len, unsigned timeout_ms)
{
    return control_out(dev,
                       EP0_OUT_LOAD_IMAGE,
                       address & 0xFFFF,          /* wValue */
                       address >> 16,             /* wIndex */
                       buf,
                       len,
                       timeout_ms);
}
