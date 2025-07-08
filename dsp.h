#include "dsp-types.h"

typedef struct lpcsdr_decimate {
    unsigned ntaps;
    int16_t *taps;
    int16_t center_tap;
    cs16_t *history;
    unsigned history_max;
    unsigned history_len;
} lpcsdr_decimate;

int lpcsdr_dsp_decimate_create(unsigned halfband_ntaps, const float *halfband_taps, struct lpcsdr_decimate **decimate);
int lpcsdr_decimate_complex_baseband(lpcsdr_decimate *decimate, int16_t *adc_capture_data, uint32_t adc_capture_data_length, cs16_t **out, uint32_t required_samples, uint16_t *output_to_file);
extern unsigned lpcsdr_standard_filter_ntaps;
extern float lpcsdr_standard_filter_taps[];