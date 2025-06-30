#include "internal.h"
#include <complex.h>

float standard_filter_taps[] = {
    0, -0.00105091, 0, 0.00250767, 0, -0.0048923, 0, 0.00855213, 0, -0.0139827, 0, 0.0220117,  0, -0.0343224, 0, 0.0552597,  0, -0.100878,   0, 0.316537, 0.5, 0.316537,
    0, -0.100878,   0, 0.0552597,  0, -0.0343224, 0, 0.0220117,  0, -0.0139827, 0, 0.00855213, 0, -0.0048923, 0, 0.00250767, 0, -0.00105091, 0,
};

int lpcsdr_convert_adc_capture_to_complex_baseband(int16_t *adc_capture_data, uint32_t adc_capture_data_length, uint32_t required_samples) {
    int error = LPCSDR_SUCCESS;
    // quarter_sample_frequency_complex signal
    
    // samples per block
    uint32_t samples_per_block = 13616/2;
    // uint32_t num_samples = 1024;

    double complex c_signal[samples_per_block];

    for (uint32_t i = 0; i < samples_per_block; i+=4) {
        c_signal[i] =       1 + 0 * I;
        c_signal[i + 1] =   0 + 1 * I;
        c_signal[i + 2] =  -1 + 0 * I;
        c_signal[i + 3] =   0 - 1 * I;
    }

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
        printf("mixed real %f img %f, adc %d, %d\n", creal(mixed[mixed_offset]), cimag(mixed[mixed_offset]),  adc_capture_data[mixed_offset], c_signal[c_signal_index % samples_per_block]);
    }

    printf("mixed offset %d, required_samples * 2 %d\n", mixed_offset, required_samples * 2);

    
    // for (uint32_t mixed_offset = 0; mixed_offset < ; mixed_offset += samples_per_block) {

    //     // uint32_t sample_index = mixed_offset, c_signal_index = 0
    //     for (uint32_t sample_index = mixed_offset, c_signal_index = 0; sample_index < mixed_offset + samples_per_block; sample_index++, c_signal_index++) {
    //         // printf("sample index %d\n", sample_index);
    //         mixed[sample_index] = adc_capture_data[sample_index] * c_signal[c_signal_index];
    //     }
    // }

    //Decimate
    // uint32_t filter_length = sizeof(standard_filter_taps)/sizeof(standard_filter_taps[0]);
    uint32_t filter_length = 3;
    // minus filter length?
    double complex first_step_dec[adc_capture_data_length - filter_length];


    for (uint32_t index = 0; index < adc_capture_data_length - filter_length; index++) {
    // printf("adc %d, fl %d, index %d\n", adc_capture_data_length, filter_length, index);

        double complex result = 0;
        for (uint32_t filter_offset = 0; filter_offset < filter_length; filter_offset++) {
            result += standard_filter_taps[filter_offset] * mixed[index + filter_offset];
        }
        first_step_dec[index] = result;
    }
    
    for (int i = 0; i < sizeof(first_step_dec)/sizeof(first_step_dec[0]); i++) {
        printf("%f %f\n", creal(first_step_dec[i]), cimag(first_step_dec[i]));
    }

    return error;
}


//0, -0.00105091, 0,
// 0         ,1      ,2,3,4,5,6,7,8,9,10,11

// 0.000000 -0.000001
// 0.000001 0.000000
// 0.000000 0.000002
// -0.000002 0.000000
// 0.000000 -0.000003
// 0.000003 0.000000
// 0.000000 0.000004
// -0.000004 0.000000
// 0.000000 -0.000005
