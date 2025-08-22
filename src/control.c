#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include "internal.h"
#include <endian.h>

typedef enum {
    IN,
    OUT
} CONTROL_TRANSFER_DIR;

static int control_transfer(libusb_device_handle *usb_handle, CONTROL_TRANSFER_DIR dir,
    uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, 
    uint16_t wLength, unsigned int timeout) {
    
    uint8_t rt = LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;

    if (dir == OUT)
        rt |= LIBUSB_ENDPOINT_OUT;
    else
        rt |= LIBUSB_ENDPOINT_IN;

    int error = libusb_control_transfer(
                                        usb_handle, 
                                        rt,
                                        bRequest,
                                        wValue,
                                        wIndex,
                                        data,
                                        wLength,
                                        timeout
    );
    
    if (error < 0)
        return error;

    return LPCSDR_SUCCESS;
}

int lpcsdr_start_transfer(lpcsdr_device_handle *dev, uint32_t target_frequency){

    pll_divisors *divisors = NULL;
    int error = LPCSDR_SUCCESS;

    if ((error = calculate_adc_clock_divisors(target_frequency, &divisors, false, false, NULL)) < 0) {
        goto cleanup;
    }

    ep0_out_start_transfer_t buffer = {
        .n_divisor = divisors->n,
        // Shift M divisor by 15
        .m_divisor = round(divisors->m * LPCSDR_FIXED_POINT_SCALE_FACTOR),
        .p_divisor = divisors->p,
        .idiv_divisor = divisors->i
    };

    error = control_transfer(
                            dev->usb_handle,
                            OUT, 
                            EP0_OUT_START_TRANSFER,
                            0,
                            0,
                            (unsigned char *)&buffer,
                            sizeof(buffer),
                            1000
    );

    if (error < 0) {
        printf("error starting adc transfer and setting dividers %d", error);
        return error;
    }

cleanup:
    if (divisors)
        free(divisors);
    return error;
}

int lpcsdr_stop_transfer(lpcsdr_device_handle *dev) {
    int error = control_transfer(
                                dev->usb_handle,
                                OUT,
                                EP0_OUT_STOP_TRANSFER,
                                0,
                                0,
                                NULL,
                                0,
                                1000
    );

    if (error < 0) {
        return error;
    }
    return LPCSDR_SUCCESS;
}

int lpcsdr_get_status(lpcsdr_device_handle *dev, ep0_in_board_status_t *status) {


    int error = control_transfer(
                                dev->usb_handle, 
                                IN,
                                EP0_IN_BOARD_STATUS,
                                0,
                                0,
                                (unsigned char *) status,
                                sizeof(ep0_in_board_status_t),
                                1000
    );

    if (error < 0) {
        return error;
    }


    return LPCSDR_SUCCESS;
}

int lpcsdr_comms_check(libusb_device_handle *usb_handle)
{
    uint32_t buffer[1];
    int error = control_transfer(
                                usb_handle, 
                                IN,
                                EP0_IN_COMMS_CHECK,
                                0,
                                0,
                                (unsigned char *)buffer,
                                sizeof(buffer),
                                1000
    );

    if (error < 0) {
        return error;
    }

    if (buffer[0] == 0xDEADBEEF)
        return LPCSDR_SUCCESS;
    return -1;
}

int lpcsdr_tuner_update(lpcsdr_device_handle *dev, uint16_t first, uint8_t *payload, uint16_t payload_size)
{   
    int error = control_transfer(
                                dev->usb_handle,
                                OUT,
                                EP0_OUT_TUNER_UPDATE,
                                first,
                                0,
                                (unsigned char *) payload,
                                payload_size,
                                1000
    );

    if (error < 0) {
        return error;
    }

    return LPCSDR_SUCCESS;
}

int lpcsdr_set_rf_power(lpcsdr_device_handle *dev, uint16_t mode) {
    int error = control_transfer(
                                dev->usb_handle,
                                OUT,
                                EP0_OUT_SET_RF_POWER,
                                mode,
                                0,
                                NULL,
                                0,
                                1000
    );

    if (error < 0) {
        return error;
    }

    return LPCSDR_SUCCESS;
}

int lpcsdr_read_tuner_register(lpcsdr_device_handle *dev, tuner_reg_num first_reg, uint16_t cache, uint8_t *buffer, uint16_t buffer_size) {
    int error = control_transfer(
                                dev->usb_handle,
                                IN,
                                EP0_IN_TUNER_READ,
                                (uint16_t) first_reg,
                                cache,
                                (unsigned char *) buffer,
                                buffer_size,
                                1000
    );

    if (error < 0) {
        return error;
    }

    return LPCSDR_SUCCESS;
}
