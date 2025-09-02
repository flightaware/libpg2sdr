#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <endian.h>

#include "internal.h"

#define CONTROL_TIMEOUT 1000

static int control_in(libusb_device_handle *usb_handle,
                      uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data,
                      uint16_t wLength)
{
    int count = libusb_control_transfer(usb_handle,
                                        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
                                        bRequest,
                                        wValue,
                                        wIndex,
                                        data,
                                        wLength,
                                        CONTROL_TIMEOUT);
    if (count < 0)
        return lpcsdr__translate_libusb_error(count);
    if (count != wLength)
        return LPCSDR_ERROR_FIRMWARE_MISMATCH;
    return LPCSDR_SUCCESS;
}

static int control_out(libusb_device_handle *usb_handle,
                       uint8_t bRequest, uint16_t wValue, uint16_t wIndex, const unsigned char *data,
                       uint16_t wLength)
{
    int count = libusb_control_transfer(usb_handle,
                                        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
                                        bRequest,
                                        wValue,
                                        wIndex,
                                        (unsigned char *)data,
                                        wLength,
                                        CONTROL_TIMEOUT);
    if (count < 0)
        return lpcsdr__translate_libusb_error(count);
    if (count != wLength)
        return LPCSDR_ERROR_FIRMWARE_MISMATCH;
    return LPCSDR_SUCCESS;
}

int lpcsdr__ctrl_start_transfer(lpcsdr_device_handle *dev, const adc_pll_config_t *config)
{
    ep0_out_start_transfer_t buffer = {
        .n_divisor = htole32(config->n),
        // Shift M divisor by 15
        .m_divisor = htole32(round(config->m * 32768.0)),
        .p_divisor = htole32(config->p),
        .idiv_divisor = htole32(config->i)
    };

    return control_out(dev->usb_handle,
                       EP0_OUT_START_TRANSFER,
                       0,
                       0,
                       (unsigned char *)&buffer,
                       sizeof(buffer));
}

int lpcsdr__ctrl_stop_transfer(lpcsdr_device_handle *dev)
{
    return control_out(dev->usb_handle,
                       EP0_OUT_STOP_TRANSFER,
                       0,
                       0,
                       NULL,
                       0);
}

int lpcsdr__ctrl_get_status(lpcsdr_device_handle *dev, ep0_in_board_status_t *out)
{
    ep0_in_board_status_t status;
    int error = control_in(dev->usb_handle, 
                           EP0_IN_BOARD_STATUS,
                           0,
                           0,
                           (unsigned char *)&status,
                           sizeof(status));
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

    return LPCSDR_SUCCESS;
}

/* This one takes a libusb handle, since we'll be calling it during
 * device discovery/setup before we have a full lpcsdr_device_handle
 * prepared
 */
int lpcsdr__ctrl_comms_check(libusb_device_handle *usb_handle)
{
    int error;
    ep0_in_comms_check_t in_check;

    if ((error = control_in(usb_handle, 
                            EP0_IN_COMMS_CHECK,
                            0,
                            0,
                            (unsigned char *)&in_check,
                            sizeof(in_check))) < 0) {
        return error;
    }
    
    if (le32toh(in_check.magic) != COMMS_CHECK_MAGIC)
        return LPCSDR_ERROR_FIRMWARE_MISMATCH;

    ep0_out_comms_check_t out_check;
    out_check.magic = htole32(COMMS_CHECK_MAGIC);
    if ((error = control_out(usb_handle,
                             EP0_OUT_COMMS_CHECK,
                             0,
                             0,
                             (unsigned char *)&out_check,
                             sizeof(out_check))) < 0) {
        return error;
    }

    return LPCSDR_SUCCESS;
}

int lpcsdr__ctrl_tuner_update(lpcsdr_device_handle *dev, uint16_t first, uint8_t *payload, uint16_t payload_size)
{   
    return control_out(dev->usb_handle,
                       EP0_OUT_TUNER_UPDATE,
                       first,
                       0,
                       (unsigned char *) payload,
                       payload_size);
}

int lpcsdr__ctrl_set_rf_power(lpcsdr_device_handle *dev, rf_power_mode_t mode)
{
    return control_out(dev->usb_handle,
                       EP0_OUT_SET_RF_POWER,
                       (uint16_t) mode,
                       0,
                       NULL,
                       0);
}

int lpcsdr__ctrl_read_tuner_register(lpcsdr_device_handle *dev, tuner_reg_num first_reg, uint16_t cache, uint8_t *buffer, uint16_t buffer_size)
{
    return control_in(dev->usb_handle,
                      EP0_IN_TUNER_READ,
                      (uint16_t) first_reg,
                      cache,
                      (unsigned char *) buffer,
                      buffer_size);
}

int lpcsdr__ctrl_update_tuner_lock(lpcsdr_device_handle *dev, uint16_t vco_current, uint16_t timeout) {
    ep0_in_tuner_lock_t out;
    int error =  control_in(dev->usb_handle,
                       EP0_IN_TUNER_LOCK,
                       vco_current,
                       timeout,
                       (unsigned char *) &out,
                       sizeof(out));

    if (error < 0)
        return error;

    return out.pll_locked;
}
