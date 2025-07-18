#include "internal.h"

float lpcsdr_standard_filter_taps[] = {
    0, -0.00105091, 0, 0.00250767, 0, -0.0048923, 0, 0.00855213, 0, -0.0139827, 0, 0.0220117,  0, -0.0343224, 0, 0.0552597,  0, -0.100878,   0, 0.316537, 0.5, 0.316537,
    0, -0.100878,   0, 0.0552597,  0, -0.0343224, 0, 0.0220117,  0, -0.0139827, 0, 0.00855213, 0, -0.0048923, 0, 0.00250767, 0, -0.00105091, 0,
};

unsigned lpcsdr_standard_filter_ntaps = sizeof(lpcsdr_standard_filter_taps) / sizeof(lpcsdr_standard_filter_taps[0]);

int decimate_cs16(const unsigned ntaps, const int16_t *taps, cs16_t *in, cs16_t *out, uint32_t count) {
    uint32_t window_end = 0;
    uint32_t out_index = 0;
    for (; window_end < count; window_end += 2, out_index++) {

        double q_sum =  0;
        double i_sum =  0;
        uint32_t tap_pointer = 0;
        uint32_t current = window_end;

        while (tap_pointer < ntaps) {
            q_sum += (taps[tap_pointer] * in[current].q);
            i_sum += (taps[tap_pointer] * in[current].i); 
            tap_pointer += 1;
            current += 1;
        }

        out[out_index].i = (i_sum);
        out[out_index].q = (q_sum);
    }

    return out_index;
}

int process_cs16(lpcsdr_decimate *decimate, cs16_t *in, cs16_t *out, uint32_t count) {
    int error = LPCSDR_SUCCESS;
    const unsigned ntaps = decimate->ntaps;
    const int16_t *taps = decimate->taps;
    cs16_t *history = decimate->history;
    const unsigned history_len = decimate->history_len;
    const unsigned history_max = decimate->history_max;

    // unsigned history_fill = history_max - history_len; /* elements required to completely fill the history buffer */
    // if (history_fill > count)                          /* .. but we can't fill with more elements than we have available */
    //     history_fill = count;
    // memcpy_elements(history + history_len, in, history_fill); /* fill the history buffer */

    // const unsigned history_available = history_len + history_fill;             /* elements in the history buffer, including what we just copied */
    // const unsigned history_processed = (history_available - (ntaps - 1)) & ~1; /* number of windows we can process from the history buffer, multiple of 2 */
    // if (history_available < (ntaps - 1) || history_processed == 0) {
    //     /* not enough history to do any useful processing, just retain what we have */
    //     decimate->history_len = history_available;
    //     return 0;
    // }


    uint32_t num_processed = decimate_cs16(ntaps, taps, in, out, count);
    return num_processed;
    /* First unprocessed window in history starts at history[history_processed].
     * We copied in[0] to history[history_len].
     * So the first unprocessed window in input data starts at in[history_processed - history_len]
     */
    // if (history_processed < history_len) {
    //     /* no further data to process in the input, just shift history down */
    //     memmove_elements(history, history + history_processed, history_available - history_processed);
    //     decimate->history_len = history_available - history_processed;
    //     return history_processed / 2;
    // }

    // const unsigned offset = history_processed - history_len;               /* offset in input of first unprocessed window */
    // const unsigned main_processed = ((count - offset) - (ntaps - 1)) & ~1; /* number of windows we can process from the input buffer, multiple of 2 */

    // /* process the main block */
    // decimate_cs16(taps, ntaps, in + offset, out + history_processed / 2, main_processed);

    // /* preserve history starting from the first unprocessed window in input data */
    // const unsigned input_consumed = offset + main_processed;
    // memcpy_elements(history, in + input_consumed, count - input_consumed);
    // decimate->history_len = count - input_consumed;

    // return (history_processed + main_processed) / 2;
}

int downmix_samples(uint32_t samples_per_block, int16_t *in, cs16_t *out, uint32_t in_length) {
    double complex c_signal[samples_per_block];

    double complex *mixed_intermediate = calloc(in_length, sizeof(double complex));
    for (uint32_t i = 0; i < samples_per_block; i+=4) {
        c_signal[i] =       1 + 0 * I;
        c_signal[i + 1] =   0 + 1 * I;
        c_signal[i + 2] =  -1 + 0 * I;
        c_signal[i + 3] =   0 - 1 * I;
    }

    // July 10, 2025. Currently ADC is outputting values that are 12 bit sign extended to 16 bit.
    for (uint32_t i = 0; i < samples_per_block; i++) {
        c_signal[i] = c_signal[i]/ 2048;
    }

    uint32_t mixed_offset = 0;
    for (uint32_t c_signal_index = 0; mixed_offset < in_length; mixed_offset++, c_signal_index++) {
        mixed_intermediate[mixed_offset] = in[mixed_offset] * c_signal[c_signal_index % samples_per_block];
    }

    for (uint32_t cur = 0; cur < mixed_offset; cur++) {
        // There should NOT be loss of precision? We're casting a double to int. But ADC integers should be scaled
        out[cur].i = creal(mixed_intermediate[cur]);
        out[cur].q = cimag(mixed_intermediate[cur]);
    }

    return LPCSDR_SUCCESS;
}

int lpcsdr_decimate_complex_baseband(lpcsdr_decimate *decimate, uint32_t samples_per_block, int16_t *in, uint32_t in_length, cs16_t **out, uint32_t required_samples, const char *output_file_path) {
    int error = LPCSDR_SUCCESS;

    uint32_t mixed_length =  required_samples * 2 + samples_per_block;
    cs16_t *mixed = calloc(in_length, sizeof(cs16_t));

    printf("mixed %u in %u\n", mixed_length, in_length);

    downmix_samples(samples_per_block, in, mixed, in_length);


    const unsigned ntaps = decimate->ntaps;
    uint32_t complex_baseband_length = in_length - ntaps + 1;
    cs16_t *values = calloc(complex_baseband_length, sizeof(cs16_t));
    uint32_t num_processed = process_cs16(decimate, mixed, values, complex_baseband_length);
    
    // Maybe make all of these function pointers
    if (output_file_path) {
        FILE *file = fopen(output_file_path, "w");
        if (file == NULL) {
            printf("Could not output to file %s\n", output_file_path);
        } else {
            for (int i = 0; i < num_processed; i++)
                fprintf(file, "%f,%f\n", (values[i].i), (values[i].q));
        }

        fclose(file);
    }

    *out = values;
    return error;
}

void lpcsdr_dsp_decimate_reset(lpcsdr_decimate *decimate)
{
    if (!decimate)
        return;

    memset_elements(decimate->history, 0, decimate->history_max);
    // decimate->history_len = decimate->ntaps - 1;
    decimate->history_len = 0;
}