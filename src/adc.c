#include <math.h>
#include <stdlib.h>
#include "internal.h"

uint32_t n_divisors_length = 256;
uint32_t *n_divisors;

uint32_t p_divisors_length = 33;
uint32_t *p_divisors;

uint32_t i_divisors_length = 256;
uint32_t *i_divisors;

uint32_t **p_i_divisors_map;
uint32_t p_i_divisors_map_length;

// Scale factor to use 16 bits.
const uint16_t LPCSDR_FIXED_POINT_SCALE_FACTOR = 32768;

// ADC is outputting values that are 12 bit signed.
const uint16_t ADC_OUTPUT_VALUE_BIT_LENGTH = 2048;

int effective_n_divisor(uint32_t n) {
    return (n > 0) ? n: 1;
}

int effective_p_divisor(uint32_t p) {
    return (p > 0) ? p * 2 : 1;
}

int effective_i_divisor(uint32_t i) {
    return (i > 0) ? i : 1;
}

int fixed_point_m(pll_divisors *divisors) {
    return (uint64_t)(round(divisors->m * 32768));
}

int candidate_is_better(pll_divisors *current_best, pll_divisors *candidate, uint32_t min_fcco, uint32_t max_fcco, bool minimize_error, float error_threshold) {

    if (candidate->actual_fcco < min_fcco || candidate->actual_fcco > max_fcco)
        return false;
    
    if (!candidate->fractional && (candidate->m < 1 || candidate->m > 1 << 15))
        return false;

    if (candidate->fractional && (fixed_point_m(candidate) < 1 || fixed_point_m(candidate) >= 1 << 22))
        return false;

    if (candidate->error > error_threshold)
        return false;

    if (current_best == NULL)
        return true;

    if (minimize_error)
        return candidate->error < current_best->error;

    if (current_best->fractional == candidate->fractional)
        return candidate->m < current_best->m;

    if (current_best->fractional)
        return candidate->m <= current_best->m * 4;
    else
        return !(current_best->m <= candidate->m * 4);
}

int init_global_adc_divisor_tables() {
    return calculate_adc_divisor_tables(&n_divisors, &p_divisors, &i_divisors, &p_i_divisors_map, &p_i_divisors_map_length);
}

int calculate_adc_divisor_tables(uint32_t **n_out, uint32_t **p_out, uint32_t **i_out, uint32_t ***p_i_divisors_out, uint32_t *p_i_divisors_out_length) {
    uint32_t *n_divisors = calloc(n_divisors_length, sizeof(uint32_t));
    uint32_t *p_divisors = calloc(p_divisors_length, sizeof(uint32_t));
    uint32_t *i_divisors = calloc(i_divisors_length, sizeof(uint32_t));

    n_divisors[0] = 0;
    p_divisors[0] = 0;
    i_divisors[0] = 0;

    for (uint32_t n = 1; n < n_divisors_length; n++)
        n_divisors[n] = n + 1;

    for (uint32_t p = 1; p < p_divisors_length; p++)
        p_divisors[p] = p;

    for (uint32_t i = 1; i < i_divisors_length; i++)
        i_divisors[i] = i + 1;

    uint32_t largest_p_divisor = p_divisors[p_divisors_length - 1];
    uint32_t largest_i_divisor = i_divisors[i_divisors_length - 1];
    uint32_t num_divisors = effective_p_divisor(largest_p_divisor) * effective_i_divisor(largest_i_divisor);
    num_divisors += 1;

    uint32_t **p_i_divisors_map = (uint32_t **) calloc(num_divisors, sizeof(uint32_t *));

    for (uint32_t current_pair = 0; current_pair < num_divisors; current_pair++) {
        p_i_divisors_map[current_pair] = calloc(2, sizeof(uint32_t));
        memset(p_i_divisors_map[current_pair], UINT32_MAX, 2 * sizeof(uint32_t));
    }

    for (uint16_t p = 0; p < p_divisors_length; p++) {
        for (uint16_t i = 0; i < i_divisors_length; i++) {
            uint32_t p_divisor = p_divisors[p];
            uint32_t i_divisor = i_divisors[i];

            uint32_t d = effective_p_divisor(p_divisors[p]) * effective_i_divisor(i_divisors[i]);
            if (i < p_i_divisors_map[d][1]) {
                p_i_divisors_map[d][0] = p_divisor;
                p_i_divisors_map[d][1] = i_divisor;
            }
        }
    }

    *n_out = n_divisors;
    *p_out = p_divisors;
    *i_out = i_divisors;
    *p_i_divisors_out = p_i_divisors_map;
    *p_i_divisors_out_length = num_divisors;

    return LPCSDR_SUCCESS;
}

int populate_new_current_best(pll_divisors **current_best, pll_divisors *candidate) {
    if (*current_best == NULL)
        *current_best = calloc(1, sizeof(pll_divisors));

    pll_divisors *b = *current_best;

    b->error = candidate->error;
    b->actual_fcco = candidate->actual_fcco;
    b->actual_frequency = candidate->actual_frequency;
    b->fractional = candidate->fractional;
    b->i = candidate->i;
    b->m = candidate->m;
    b->p = candidate->p;
    b->n = candidate->n;
    return LPCSDR_SUCCESS;
}

int calculate_adc_clock_divisors(uint32_t target_frequency, pll_divisors **divisors, bool minimize_error, bool enable_fractional, double *optional_epsilon) {
    double epsilon;
    if (optional_epsilon != NULL)
        epsilon = *optional_epsilon;
    else
        epsilon = 1e-6;

    uint32_t min_fcco = 275e6;
    uint32_t max_fcco = 550e6;
    uint32_t reference_frequency = 12e6;
    double error_threshold = target_frequency * epsilon;

    uint32_t range_min = ceil(min_fcco / target_frequency);
    uint32_t range_max= floor(max_fcco/ target_frequency);

    pll_divisors *current_best = NULL;
    for (uint32_t s = range_min; s < range_max; s++) {
        if (p_i_divisors_map[s][0] == UINT32_MAX || s > p_i_divisors_map_length) {
            continue;
        }

        uint32_t p = p_i_divisors_map[s][0];
        uint32_t i = p_i_divisors_map[s][1];

        uint32_t desired_fcco = target_frequency * s;

        desired_fcco = MIN(desired_fcco, max_fcco);
        desired_fcco = MAX(desired_fcco, min_fcco);

        if (enable_fractional) {
            uint32_t scaled_m = round(desired_fcco / reference_frequency / 2 * (1 << 15));

            uint32_t test_fcco = 2 * scaled_m / (1<<15) * reference_frequency;
            if (test_fcco < min_fcco)
                scaled_m += 1;
            else if (test_fcco > max_fcco)
                scaled_m -= 1;

            uint32_t fractional_m =  scaled_m / (1<<15);
            uint32_t actual_fcco = 2 * fractional_m * reference_frequency;
            uint32_t actual_frequency = actual_fcco / s;
            double error = (abs(actual_frequency - target_frequency));

            pll_divisors candidate = {
                .fractional = true,
                .n = 0,
                .m = fractional_m,
                .p = p,
                .i = i,
                .error = error,
                .actual_fcco = actual_fcco,
                .actual_frequency = actual_frequency,
            };
            if (candidate_is_better(current_best, &candidate, min_fcco, max_fcco, minimize_error, error_threshold)) {
                populate_new_current_best(&current_best, &candidate);
            }
        }

        for (uint32_t n = 0; n < n_divisors_length; n++) {
            uint32_t n_reference = reference_frequency / effective_n_divisor(n_divisors[n]);
            uint32_t integer_m = round(desired_fcco / n_reference / 2);

            uint32_t actual_fcco = 2 * integer_m * n_reference;
            uint32_t actual_frequency = actual_fcco / s;
            double error = abs(actual_frequency - target_frequency);

            pll_divisors candidate = {
                .fractional = false,
                .n = n,
                .m = integer_m,
                .p = p,
                .i = i,
                .error = error,
                .actual_fcco = actual_fcco,
                .actual_frequency = actual_frequency,
            };
            if (candidate_is_better(current_best, &candidate, min_fcco, max_fcco, minimize_error, error_threshold)) {
                populate_new_current_best(&current_best, &candidate);
            }
        }
    }
    *divisors = current_best;
    return LPCSDR_SUCCESS;
}

int unpack_header(uint32_t offset, uint8_t *in, ep1_header_t* out) {
    uint8_t header[sizeof(ep1_header_t)];
    for (uint32_t hx = 0; hx < sizeof(ep1_header_t); hx++) {
        header[hx] = in[offset + hx];
    }

    ep1_header_t *h = (ep1_header_t *) header;

    out->magic = le32toh(h->magic);
    out->block_len = le32toh(h->block_len);
    out->samples = le32toh(h->samples);
    out->sequence = le32toh(h->sequence);
    out->status = le32toh(h->status);

    return LPCSDR_SUCCESS;
}

int unpack_raw_adc_data(lpcsdr_device_handle *handle, uint8_t *in, uint32_t in_length, int16_t *out, uint32_t skip, const char *output_file) {
    int error = LPCSDR_SUCCESS;

    ep1_header_t h = {};
    unpack_header(0, in, &h);
    
    const uint32_t usb_bytes_per_block = h.block_len;
    const uint32_t usb_samples_per_block = h.samples;
    uint32_t out_index = 0;

    for(uint32_t current_block = skip * usb_bytes_per_block; current_block < in_length; current_block+=usb_bytes_per_block) {
        unpack_header(current_block, in, &h);

        if (out_index == 6807) {
            int a = 1;
        }

        if (h.magic != EXPECTED_BLOCK_HEADER_MAGIC) {
            fprintf(stderr, "Magic mismatch at block %d", current_block);
            return LPCSDR_BT_MAGIC_MISMATCH;
        }
        if (h.block_len % handle->usb_bytes_per_block_multiple != 0) {
            fprintf(stderr, "Block length %d not a multiple of %d", h.block_len, handle->usb_bytes_per_block_multiple);
            return LPCSDR_BT_BLOCKLENGTH_MISMATCH;
        }
        if (h.samples % handle->usb_samples_per_block_multiple != 0) {
            fprintf(stderr, "Block sample length of %d not a multiple of %d", h.samples, handle->usb_samples_per_block_multiple);
            return LPCSDR_BT_SAMPLELENGTH_MISMATCH;
        }

        uint8_t block_body[usb_bytes_per_block];
        uint32_t current_offset = current_block + sizeof(ep1_header_t);
        for (uint32_t bx = current_offset, new_block_body_offset = 0; bx < current_offset + usb_bytes_per_block; bx++, new_block_body_offset++){
            block_body[new_block_body_offset] = in[bx];
        }

        uint32_t total_samples_size_in_bytes = (uint32_t)floor(usb_samples_per_block * handle->individual_sample_bit_size / 8);
        uint32_t packed_size = total_samples_size_in_bytes / 4;
        uint32_t packed[packed_size];

        for (uint32_t bx = 0, packed_index = 0; bx < total_samples_size_in_bytes; bx+=4, packed_index++){
            packed[packed_index] = (block_body[bx + 3] << 24) | (block_body[bx + 2] << 16) | (block_body[bx + 1] << 8) | block_body[bx];
        }

        int16_t unpacked[usb_samples_per_block];
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
                out[out_index] = unpacked[z] * LPCSDR_FIXED_POINT_SCALE_FACTOR / ADC_OUTPUT_VALUE_BIT_LENGTH;
                out_index += 1;
            }
        }
    }

    FILE *file;
    if (output_file != NULL && (file = fopen(output_file, "a")) != NULL) {
        for (uint32_t i = 0; i < out_index; i++)
            fprintf(file, "%d\t%d\n", i, out[i]);
        fclose(file);
    } else {
        fprintf(stderr, "Unpack Raw ADC data: Cannot open file %s\n", output_file);
    }

    printf("length of unpacked %d \n", out_index);

    return error;
}

int lpcsdr_read_raw_adc_data(lpcsdr_device_handle* device_handle, ep0_in_board_status_t *status, uint8_t *out, uint32_t total, const char *output_file_path) {
    int error = LPCSDR_SUCCESS;
    const uint32_t usb_bytes_per_block = status->usb_bytes_per_block;
    const uint32_t usb_samples_per_block = status->usb_samples_per_block;
    const uint32_t hsadc_frequency = device_handle->hsadc_frequency;
    const uint32_t blocks_per_chunk = device_handle->blocks_per_chunk;

    uint32_t chunk_size =  blocks_per_chunk * usb_bytes_per_block;
    uint32_t chunk_timeout = 500 + floor(1100 * blocks_per_chunk * usb_samples_per_block / hsadc_frequency);

    uint32_t captured_bytes = 0;
    uint32_t *num_bytes_actually_read = calloc(1, sizeof(uint32_t));


    if (num_bytes_actually_read == NULL)
        return LPCSDR_ERROR_NO_MEMORY;

    printf("total %u chunk_size %u samples_per_block %u frequency %d bytes_per_block %u\n", total, chunk_size, usb_samples_per_block, hsadc_frequency, usb_bytes_per_block);

    while (captured_bytes < total) {
        printf("capture byte %u \n", captured_bytes);
        uint32_t length = MIN(chunk_size, total - captured_bytes);
        uint8_t *buffer = calloc(length, sizeof(uint8_t));

        if (buffer == NULL)
            return LPCSDR_ERROR_NO_MEMORY;

        if ((error = device_handle->vtable->bulk_transfer(device_handle->usb_handle, 0x81, (unsigned char *) buffer, length, (int *) num_bytes_actually_read, chunk_timeout)) < 0) {
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

cleanup:
    if (num_bytes_actually_read)
        free(num_bytes_actually_read);
    return error;
}

int lpcsdr_capture_toy_example(lpcsdr_device_handle* device_handle, uint32_t num_samples, uint32_t target_frequency, uint32_t skip) {
    int error = LPCSDR_SUCCESS;
    uint8_t *adc_capture = NULL;
    int16_t *unpacked = NULL;

    if ((error = lpcsdr_start_transfer(device_handle, target_frequency)) < 0)
        goto cleanup;

    ep0_in_board_status_t *status = NULL;
    if ((error = lpcsdr_get_status(device_handle->usb_handle, &status)) < 0)
        goto cleanup;
     
    uint32_t num_blocks = ceil((double) (2 * num_samples + skip * status->usb_samples_per_block) / (double)status->usb_samples_per_block);
    uint32_t total = num_blocks * status->usb_bytes_per_block;

    adc_capture = calloc(total, sizeof(uint8_t));
    if ((error = lpcsdr_read_raw_adc_data(device_handle, status, adc_capture, total, "raw_adc_capture.tsv")) < 0)
        goto cleanup;

    unpacked = calloc(num_blocks * status->usb_samples_per_block, sizeof(int16_t));
    if ((error = unpack_raw_adc_data(device_handle, adc_capture, total, unpacked, skip, "./test_unpacked.tsv")) < 0)
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
