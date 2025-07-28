#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include "internal.h"
#include <endian.h>


int lpcsdr_start_transfer(lpcsdr_device_handle *handle, uint32_t target_frequency){

    pll_divisors *divisors = NULL;
    int error = LPCSDR_SUCCESS;

    if ((error = calculate_adc_clock_divisors(target_frequency, &divisors, false, false, NULL)) < 0) {
        goto cleanup;
    }

    ep0_out_start_transfer_t buffer = {
        .n_divisor = divisors->n,
        // Shift M divisor by 15
        .m_divisor = round(divisors->m * 32768),
        .p_divisor = divisors->p,
        .idiv_divisor = divisors->i
    };

    error = libusb_control_transfer(
                                    handle->usb_handle, 
                                    LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                                    0x13,
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

int lpcsdr_stop_transfer(lpcsdr_device_handle *handle) {
    int error = libusb_control_transfer(
                                    handle->usb_handle, 
                                    LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                                    0x14,
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

int lpcsdr_get_status(lpcsdr_device_handle *device_handle, ep0_in_board_status_t **status) {

    ep0_in_board_status_t *buffer = calloc(1, sizeof(ep0_in_board_status_t));

    int error = libusb_control_transfer(
                                        device_handle->usb_handle, 
                                        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                                        EP0_IN_BOARD_STATUS,
                                        0,
                                        0,
                                        (unsigned char *)buffer,
                                        sizeof(ep0_in_board_status_t),
                                        1000
    );

    if (error < 0) {
        return error;
    }

    *status = buffer;

    return LPCSDR_SUCCESS;
}

int lpcsdr_comms_check(libusb_device_handle *device_handle)
{
    uint32_t buffer[1];
    int error = libusb_control_transfer(
                                        device_handle, 
                                        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_ENDPOINT,
                                        0x01,
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