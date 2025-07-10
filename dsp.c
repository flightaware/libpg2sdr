#include "internal.h"
#include <complex.h>
#include <math.h>

float lpcsdr_standard_filter_taps[] = {
    0, -0.00105091, 0, 0.00250767, 0, -0.0048923, 0, 0.00855213, 0, -0.0139827, 0, 0.0220117,  0, -0.0343224, 0, 0.0552597,  0, -0.100878,   0, 0.316537, 0.5, 0.316537,
    0, -0.100878,   0, 0.0552597,  0, -0.0343224, 0, 0.0220117,  0, -0.0139827, 0, 0.00855213, 0, -0.0048923, 0, 0.00250767, 0, -0.00105091, 0,
};

unsigned lpcsdr_standard_filter_ntaps = sizeof(lpcsdr_standard_filter_taps) / sizeof(lpcsdr_standard_filter_taps[0]);

int lpcsdr_decimate_complex_baseband(lpcsdr_decimate *decimate, int16_t *adc_capture_data, uint32_t adc_capture_data_length, cs16_t **out, uint32_t required_samples, uint16_t *output_to_file) {
    int error = LPCSDR_SUCCESS;

    const unsigned ntaps = decimate->ntaps;
    const int16_t *taps = decimate->taps;
    cs16_t *history = decimate->history;
    const unsigned history_len = decimate->history_len;
    const unsigned history_max = decimate->history_max;
    const int16_t center_tap = decimate->center_tap;

    uint32_t samples_per_block = 13616/2;

    double complex c_signal[samples_per_block];

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

    uint32_t mixed_length =  required_samples * 2 + samples_per_block;
    double complex *mixed = calloc(mixed_length, sizeof(*mixed));

    uint32_t mixed_offset = 0;
    for (uint32_t c_signal_index = 0; mixed_offset < adc_capture_data_length; mixed_offset++, c_signal_index++) {
        mixed[mixed_offset] = adc_capture_data[mixed_offset] * c_signal[c_signal_index % samples_per_block];
    }

    uint32_t complex_baseband_length = adc_capture_data_length - ntaps + 1;
    cs16_t *values = calloc(complex_baseband_length, sizeof(cs16_t));

    uint32_t window_end = 0;
    uint32_t out_index = 0;
    for (; window_end < complex_baseband_length; window_end += 2, out_index++) {

        double q_sum =  0;
        double i_sum =  0;
        uint32_t tap_pointer = 0;
        uint32_t current = window_end;

        while (tap_pointer < ntaps) {
            q_sum += taps[tap_pointer] * cimag(mixed[current]);
            i_sum += taps[tap_pointer] * creal(mixed[current]); 
            tap_pointer += 1;
            current += 1;
        }

        values[out_index].i = (i_sum);
        values[out_index].q = (q_sum);

    }
    
    if (output_to_file != NULL && *output_to_file == 1) {
        FILE *file = fopen("liblpcsdr-decimate-output.tsv", "w");
        for (int i = 0; i < out_index; i++) {
            fprintf(file, "%f,%f\n", (values[i].i), (values[i].q));
        }
    }

    *out = values;
    return error;
}


