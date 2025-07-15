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
int lpcsdr_decimate_complex_baseband(lpcsdr_decimate *decimate, uint32_t samples_per_block, int16_t *in, uint32_t in_length, cs16_t **out, uint32_t required_samples, const char *output_file_path);
extern unsigned lpcsdr_standard_filter_ntaps;
extern float lpcsdr_standard_filter_taps[];