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

int lpcsdr_capture(lpcsdr_device_handle* device_handle, uint32_t num_samples, uint32_t target_frequency, uint32_t skip) {
    int error = LPCSDR_SUCCESS;

    if ((error = lpcsdr_start_transfer(device_handle, target_frequency)) < 0)
        goto cleanup;

    ep0_in_board_status_t *status = NULL;
    if ((error = lpcsdr_get_status(device_handle, &status)) < 0)
        goto cleanup;

     
    uint32_t num_blocks = ceil((double) (2 * num_samples + skip * status->usb_samples_per_block) / (double)status->usb_samples_per_block);
    uint32_t total = num_blocks * status->usb_bytes_per_block;

    uint8_t *adc_capture = calloc(total, sizeof(uint8_t));
    if ((error = lpcsdr_read_raw_adc_data(device_handle, status, adc_capture, total, "raw_adc_capture.tsv")) < 0)
        goto cleanup;

    int16_t *unpacked = NULL;
    uint32_t unpacked_length;
    if ((error = unpack_raw_adc_data(device_handle, status, adc_capture, total, &unpacked, &unpacked_length, skip, "./test_unpacked.tsv")) < 0)
        goto cleanup;

    cs16_t *out = NULL;
    if ((error = lpcsdr_decimate_complex_baseband(device_handle->decimation_filter, status->usb_samples_per_block, unpacked, unpacked_length, &out, num_samples, "./test_files/outputs/default-test-case-output.tsv")) < 0)
        goto cleanup;

cleanup:
    if (status)
        free(status);
    if (adc_capture)
        free(adc_capture);
    if (unpacked)
        free(unpacked);

    return error;
}

int lpcsdr_read_raw_adc_data(lpcsdr_device_handle* device_handle, ep0_in_board_status_t *status, uint8_t *out, uint32_t total, const char *output_file_path) {
    int error = LPCSDR_SUCCESS;
    uint32_t blocks_per_chunk = 128;

    uint32_t chunk_size =  blocks_per_chunk * status->usb_bytes_per_block;
    uint32_t chunk_timeout = 500 + floor(1100 * blocks_per_chunk * status->usb_samples_per_block / status->hsadc_frequency);

    uint32_t captured_bytes = 0;
    // uint8_t *data = calloc(total, sizeof(uint8_t));
    uint32_t *num_bytes_actually_read = calloc(1, sizeof(uint32_t));


    if (num_bytes_actually_read == NULL)
        return LPCSDR_ERROR_NO_MEMORY;

    printf("total %u chunk_size %u samples_per_block %u frequency %d bytes_per_block %u\n", total, chunk_size, status->usb_samples_per_block, status->hsadc_frequency, status->usb_bytes_per_block);

    while (captured_bytes < total) {
        printf("capture byte %u \n", captured_bytes);
        uint32_t length = MIN(chunk_size, total - captured_bytes);
        uint8_t *buffer = calloc(length, sizeof(uint8_t));

        if (buffer == NULL)
            return LPCSDR_ERROR_NO_MEMORY;

        if ((error = libusb_bulk_transfer(device_handle->usb_handle, 0x81, (unsigned char*) buffer, length, (int *) num_bytes_actually_read, chunk_timeout)) < 0) {
            goto cleanup;
        }

        if (!num_bytes_actually_read || *num_bytes_actually_read != length) {
            error = LPCSDR_BT_EXPECTED_LENGTH_MISMATCH;
            goto cleanup;
        }

        for (uint32_t i = captured_bytes, x = 0; i < captured_bytes + length; i++, x++) {
            out[i] = buffer[x];
        }
        captured_bytes += length;
        free(buffer);
    }
    
    lpcsdr_stop_transfer(device_handle);

    if (captured_bytes != total) {
        return LPCSDR_ERROR_BAD_STATE;
    }

    FILE *file;
    if (output_file_path && (file = fopen(output_file_path, "w")) != NULL) {
        fprintf(file, "total %d captured_bytes %d\n", total, captured_bytes);
        for (uint32_t cursor = 0; cursor < total; cursor++) {
            fprintf(file, "%d\n", out[cursor]);
        }
        fclose(file);
    }

    // *out = data;
    // *out_length = captured_bytes;
    
cleanup:
    if (num_bytes_actually_read)
        free(num_bytes_actually_read);
    return error;
}

int unpack_raw_adc_data(lpcsdr_device_handle *handle, ep0_in_board_status_t *status, uint8_t *in, uint32_t in_length, int16_t **out, uint32_t *out_length, uint32_t skip, const char *output_file) {
    int error = LPCSDR_SUCCESS;

    int16_t *extended_unpacked = calloc(in_length, sizeof(int16_t));
    if (extended_unpacked == NULL) {
        return LPCSDR_ERROR_NO_MEMORY;
    }
    
    uint32_t out_index = 0;

    for(uint32_t current_block = skip * status->usb_bytes_per_block; current_block < in_length; current_block+=status->usb_bytes_per_block) {
        uint8_t header[sizeof(ep1_header_t)];
        for (uint32_t hx = 0; hx < sizeof(ep1_header_t); hx++) {
            header[hx] = in[current_block + hx];
        }

        ep1_header_t *h = (ep1_header_t *) header;

        h->magic = le32toh(h->magic);
        h->block_len = le32toh(h->block_len);
        h->samples = le32toh(h->samples);
        h->sequence = le32toh(h->sequence);
        h->status = le32toh(h->status);

        if (h->magic != 0xDEADBEEF) {
            fprintf(stderr, "Magic mismatch at block %d", current_block);
            return LPCSDR_BT_MAGIC_MISMATCH;
        }
        if (h->block_len!=status->usb_bytes_per_block) {
            fprintf(stderr, "Block length mismatch at block %d", current_block);
            return LPCSDR_BT_BLOCKLENGTH_MISMATCH;
        }

        uint8_t block_body[status->usb_bytes_per_block];
        uint32_t current_offset = current_block + sizeof(ep1_header_t);
        for (uint32_t bx = current_offset, new_block_body_offset = 0; bx < current_offset + status->usb_bytes_per_block; bx++, new_block_body_offset++){
            block_body[new_block_body_offset] = in[bx];
        }

        uint32_t total_samples_size_in_bytes = (uint32_t)floor(status->usb_samples_per_block * 12 / 8);
        uint32_t packed_size = total_samples_size_in_bytes / 4;
        uint32_t packed[packed_size];

        for (uint32_t bx = 0, packed_index = 0; bx < total_samples_size_in_bytes; bx+=4, packed_index++){
            packed[packed_index] = (block_body[bx + 3] << 24) | (block_body[bx + 2] << 16) | (block_body[bx + 1] << 8) | block_body[bx];
        }

        int16_t unpacked[status->usb_samples_per_block];
        for (uint32_t i = 0, unpacked_index = 0; i < packed_size; i += 3, unpacked_index += 8) {
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
                uint16_t extended_value = (unpacked[z] & 0x7FF) - (unpacked[z] & 0x800);
                extended_unpacked[out_index] = extended_value;
                out_index += 1;
            }
        }
    }

    FILE *file;
    if (output_file != NULL && (file = fopen(output_file, "w")) != NULL) {
        for (uint32_t i = 0; i < out_index; i++)
            fprintf(file, "%d\t%x\n", i, extended_unpacked[i]);
        fclose(file);
    } else {
        fprintf(stderr, "Unpack Raw ADC data: Cannot open file %s\n", output_file);
    }

    
    printf("length of unpacked %d \n", out_index);
    *out = extended_unpacked;
    *out_length = out_index;

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