#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include "internal.h"
#include <endian.h>

int n_value(uint32_t n) {
    return (n > 0) ? n: 1;
}

int p_value(uint32_t p) {
    return (p > 0) ? p * 2 : 1;
}

int i_value(uint32_t i) {
    return (i > 0) ? i : 1;
}

int divider_comparator(int error, int n, pll_divisors *b) {

    if (error < b->error)
        return true;

    if (error == b->error && n < b->n)
        return true;

    return false;
}

int calculate_adc_clock_divisors(uint32_t target_frequency, pll_divisors **int_divisors, pll_divisors **frac_divisors) {

    uint32_t min_fcco = 275e6;
    uint32_t max_fcco = 550e6;
    uint32_t reference_frequency = 12e6;

    uint32_t range_min = ceil(min_fcco / target_frequency);
    uint32_t range_max= floor(max_fcco/ target_frequency);

    uint32_t length = 16834;
    length += 1;
    uint32_t p_i_dividers[length][2];
    memset(p_i_dividers, UINT32_MAX, length * sizeof(p_i_dividers[0]));

    for (uint32_t p = 0; p < 33; p++) {
        for (uint32_t i = 0; i < 257; i++) {
            uint32_t d = p_value(p) * i_value(i);

            if (i < p_i_dividers[d][1]) {
                p_i_dividers[d][0] = p;
                p_i_dividers[d][1] = i;
            }
        }
    }

    pll_divisors *best_frac = calloc(1, sizeof(pll_divisors));
    pll_divisors *best_int = calloc(1, sizeof(pll_divisors));
    best_frac->error = UINT32_MAX;
    best_int->error = UINT32_MAX;


    for (uint32_t s = range_min; s < range_max; s++) {
        if (p_i_dividers[s][0] == UINT32_MAX)
            continue;

        uint32_t p = p_i_dividers[s][0];
        uint32_t i = p_i_dividers[s][1];

        uint32_t desired_fcco = target_frequency * s;

        uint32_t scaled_m = round(desired_fcco / reference_frequency / 2 * (1 << 15));

        uint32_t fractional_m =  scaled_m / (1<<15);

        uint32_t actual_fcco = 2 * fractional_m * reference_frequency;
        uint32_t actual_frequency = actual_fcco / s;
        uint32_t error = round(abs(actual_frequency - target_frequency));

        if (error < best_frac->error) {
            best_frac->error = error;
            best_frac->n = 0;
            best_frac->m = fractional_m;
            best_frac->p = p;
            best_frac->i = i;
        }

        for (uint32_t n = 0; n < 257; n++) {
            uint32_t n_reference = reference_frequency / n_value(n);
            uint32_t integer_m = round(desired_fcco / n_reference / 2);
            actual_fcco = 2 * integer_m * n_reference;
            actual_frequency = actual_fcco / s;
            error = round(abs(actual_frequency - target_frequency));

            if (divider_comparator(error, n, best_int)) {
                best_int->error = error;
                best_int->n = n;
                best_int->m = integer_m;
                best_int->p = p;
                best_int->i = i;
            }
        }
    }

    *int_divisors = best_int;
    *frac_divisors = best_frac;

    return LPCSDR_SUCCESS;
}

int lpcsdr_start_transfer(lpcsdr_device_handle *handle, uint32_t target_frequency){

    pll_divisors *int_divisors;
    pll_divisors *frac_divisors;
    pll_divisors *smallest_error_divisors;
    int error = LPCSDR_SUCCESS;

    if ((error = calculate_adc_clock_divisors(target_frequency, &int_divisors, &frac_divisors)) < 0) {
        goto cleanup;
    }

    if (int_divisors->error < frac_divisors->error)
        smallest_error_divisors = int_divisors;
    else
        smallest_error_divisors = frac_divisors;

    ep0_out_start_transfer_t buffer = {
        .n_divisor = smallest_error_divisors->n,
        // Shift M divisor by 15
        .m_divisor = round(smallest_error_divisors->m * 32768),
        .p_divisor = smallest_error_divisors->p,
        .idiv_divisor = smallest_error_divisors->i
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
    if (int_divisors)
        free(int_divisors);
    if (frac_divisors)
        free(frac_divisors);
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

int lpcsdr_capture_adc_output(lpcsdr_device_handle* device_handle, uint32_t num_samples) {

    ep0_in_board_status_t *status = NULL;
    uint8_t *data = NULL;
    uint32_t *num_bytes_actually_read = NULL;
    int error = LPCSDR_SUCCESS;

    if ((error = lpcsdr_get_status(device_handle, &status)) < 0) {
        goto cleanup;
    }

    uint32_t num_blocks = ceil((double)num_samples / (double)status->usb_samples_per_block) + 8;
    uint32_t total = num_blocks * status->usb_bytes_per_block;
    uint32_t blocks_per_chunk = 128;

    uint32_t chunk_size =  blocks_per_chunk * status->usb_bytes_per_block;
    uint32_t chunk_timeout = 500 + floor(1100 * blocks_per_chunk * status->usb_samples_per_block / status->hsadc_frequency);

    uint32_t captured_bytes = 0;
    num_bytes_actually_read = calloc(1, sizeof(uint32_t));
    data = calloc(total, sizeof(uint8_t));

    while (captured_bytes < total) {

        uint32_t length = MIN(chunk_size, total - captured_bytes);
        uint8_t *buffer = calloc(length, sizeof(uint8_t));

        if ((error = libusb_bulk_transfer(device_handle->usb_handle, 0x81, (unsigned char*) buffer, length, (int *) num_bytes_actually_read, chunk_timeout)) < 0) {
            goto cleanup;
        }

        if (!num_bytes_actually_read || *num_bytes_actually_read != length) {
            error = LPCSDR_BT_EXPECTED_LENGTH_MISMATCH;
            goto cleanup;
        }

        for (uint32_t i = captured_bytes, x = 0; i < captured_bytes + length; i++, x++) {
            data[i] = buffer[x];
        }
        captured_bytes += length;
        free(buffer);
    }
    
    lpcsdr_stop_transfer(device_handle);

    FILE *file = fopen("adc_capture_output.tsv", "w");
    int index = 0;
    for(uint32_t current_block = 8 * status->usb_bytes_per_block; current_block < total; current_block+=status->usb_bytes_per_block) {
        uint8_t header[sizeof(ep1_header_t)];
        for (uint32_t hx = 0; hx < sizeof(ep1_header_t); hx++) {
            header[hx] = data[current_block + hx];
        }

        ep1_header_t *h = (ep1_header_t *) header;

        h->magic = le32toh(h->magic);
        h->block_len = le32toh(h->block_len);
        h->samples = le32toh(h->samples);
        h->sequence = le32toh(h->sequence);
        h->status = le32toh(h->status);

        if (h->magic != 0xDEADBEEF) {
            fprintf(stderr, "Magic mismatch at block %d", current_block);
            error = LPCSDR_BT_MAGIC_MISMATCH;
            goto cleanup; 
        }
        if (h->block_len!=status->usb_bytes_per_block) {
            fprintf(stderr, "Block length mismatch at block %d", current_block);
            error = LPCSDR_BT_BLOCKLENGTH_MISMATCH;
            goto cleanup;
        }

        uint8_t block_body[status->usb_bytes_per_block];
        for (uint32_t bx = current_block + sizeof(ep1_header_t), x = 0; bx < current_block + sizeof(ep1_header_t)+ status->usb_bytes_per_block; bx++, x++){
            block_body[x] = data[bx];
        }

        uint32_t total_samples_size_in_bytes = (uint32_t)floor(status->usb_samples_per_block * 12 / 8);
        uint32_t packed[total_samples_size_in_bytes / 4];
            
        for (uint32_t bx = 0, packed_index = 0; bx < total_samples_size_in_bytes; bx+=4, packed_index++){
            packed[packed_index] = (block_body[bx + 3] << 24) | (block_body[bx + 2] << 16) | (block_body[bx + 1] << 8) | block_body[bx];
        }

        int16_t unpacked[status->usb_samples_per_block];
        for (uint32_t i = 0, unpacked_index = 0; i < (total_samples_size_in_bytes/4); i += 3, unpacked_index += 8) {
            uint32_t first = packed[i], second = packed[i + 1], third = packed[i + 2];
            unpacked[unpacked_index]        =   (first  &   0x00000FFF);
            unpacked[unpacked_index + 1]    =   (first  &   0x0FFF0000) >> 16;
            unpacked[unpacked_index + 2]    =   (second &   0x00000FFF);
            unpacked[unpacked_index + 3]    =   (second &   0x0FFF0000) >> 16;

            unpacked[unpacked_index + 4]    =   (third  &   0x00000FFF);
            unpacked[unpacked_index + 5]    =   (third  &   0x0FFF0000) >> 16;
            unpacked[unpacked_index + 6]    =   ((first &   0x0000F000) >> 4)  | ((second & 0x0000F000) >> 8)   | ((third & 0x0000F000) >> 12);
            unpacked[unpacked_index + 7]    =   ((first &   0xF0000000) >> 20) | ((second & 0xF0000000) >> 24)  | ((third & 0xF0000000) >> 28);

            for (int z = unpacked_index; z < unpacked_index + 8; z++) {
                int16_t extended = unpacked[z] & 0b0000111111111111;
                if (unpacked[z] & 0x800) {
                    extended = extended | 0xF800;
                }
                // uint16_t extended = (unpacked[z] & 0x7FF) - (unpacked[z] & 0x800);

                fprintf(file, "%d\t%x\n", index, extended);
                index += 1;
            }
        }
    }

cleanup:
    if (num_bytes_actually_read)
        free(num_bytes_actually_read);
    if (data)
        free(data);
    if (status)
        free(status);
    return error;
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