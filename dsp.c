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

    const unsigned ntaps = decimate->ntaps & ~3; /* always a multiple of 4 */
    const int16_t *taps = decimate->taps;
    cs16_t *history = decimate->history;
    const unsigned history_len = decimate->history_len;
    const unsigned history_max = decimate->history_max;
    const int16_t center_tap = decimate->center_tap;

    printf("decimate  %d  %d ", center_tap, ntaps);
    
    // samples per block
    uint32_t samples_per_block = 13616/2;

    double complex c_signal[samples_per_block];

    for (uint32_t i = 0; i < samples_per_block; i+=4) {
        c_signal[i] =       1 + 0 * I;
        c_signal[i + 1] =   0 + 1 * I;
        c_signal[i + 2] =  -1 + 0 * I;
        c_signal[i + 3] =   0 - 1 * I;
    }

    // THIS is important for scaling the ADC values? I'm not sure if the ADC values are getting scale currently
    for (uint32_t i = 0; i < samples_per_block; i++) {
        c_signal[i] = c_signal[i]/ 2048;
    }

    uint32_t mixed_length =  required_samples * 2 + samples_per_block;
    double complex *mixed = calloc(mixed_length, sizeof(*mixed));

    // int16_t x = 5 ;
    double complex i = adc_capture_data[0] * c_signal[0];
    printf("first: %d,res real %f, res img: %f, mixed_length %d, adc_length %d\n", adc_capture_data[0], creal(i), cimag(i), mixed_length, adc_capture_data_length);

    uint32_t mixed_offset = 0;
    for (uint32_t c_signal_index = 0; mixed_offset < adc_capture_data_length; mixed_offset++, c_signal_index++) {
        mixed[mixed_offset] = adc_capture_data[mixed_offset] * c_signal[c_signal_index % samples_per_block];
        // printf("mixed real %f img %f, adc %d, %d\n", creal(mixed[mixed_offset]), cimag(mixed[mixed_offset]),  adc_capture_data[mixed_offset], c_signal[c_signal_index % samples_per_block]);
    }

    printf("mixed offset %d, required_samples * 2 %d\n", mixed_offset, required_samples * 2);

    //0	(-0.01953125+0j

    cs16_t *values = calloc(adc_capture_data_length - ntaps, sizeof(cs16_t));

    uint16_t center_delay = (ntaps / 2 - 1);
    for (uint32_t index = 0, out_index = 0; index < adc_capture_data_length - ntaps; index+=2, out_index++) {

        double bleh = cimag(mixed[center_delay]);

        double q_initial = cimag(mixed[center_delay]) * center_tap;
        double i_initial = creal(mixed[center_delay]) * center_tap;
        
        double q_sum =  q_initial;
        double i_sum =  i_initial;

        printf("initial img %f * %d\n", cimag(mixed[center_delay]), center_tap);
        printf("initial real %f * %d\n", creal(mixed[center_delay]), center_tap);

        for (uint32_t filter_offset = 0; filter_offset < ntaps; filter_offset++) {
            double q = cimag(mixed[index + filter_offset]);
            double i = creal(mixed[index + filter_offset]);
            q_sum +=   taps[filter_offset] * q;
            i_sum +=   taps[filter_offset] * i; 
            printf("q %f * %d \n", q , taps[filter_offset]);
            printf("i %f * %d \n", i , taps[filter_offset]);
        }
        printf("overall i %f, q %f, index %u\n", i_sum, q_sum, index);

        values[out_index].i = (i_sum);
        values[out_index].q = (q_sum);

        if (out_index == 0) {
            return -1;
        }
    }
    
    if (output_to_file != NULL && *output_to_file == 1) {
        FILE *file = fopen("liblpcsdr-decimate-output.tsv", "w");
        for (int i = 0; i < adc_capture_data_length/2; i++) {
            fprintf(file, "%d\t%d%dj\n", i, (values[i].i), (values[i].q));
        }
    }


    *out = values;
    return error;
}


