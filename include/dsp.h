#include "dsp-types.h"
#include <complex.h>
#include <math.h>

typedef struct lpcsdr_decimate {
    unsigned int ntaps;
    int16_t *taps;
    cs16_t *history;
    unsigned int history_max;
    unsigned int history_len;
} lpcsdr_decimate;

int process_cs16(lpcsdr_decimate *decimate, cs16_t *in, cs16_t *out, uint32_t count);
int decimate_cs16(const unsigned ntaps, const int16_t *taps, cs16_t *in, cs16_t *out, uint32_t count);
int lpcsdr_dsp_decimate_create(unsigned halfband_ntaps, const float *halfband_taps, struct lpcsdr_decimate **out);
int lpcsdr_decimate_complex_baseband(lpcsdr_decimate *decimate, uint32_t samples_per_block, int16_t *in, uint32_t in_length, cs16_t **out, uint32_t required_samples, const char *output_file_path);
int downmix_samples(uint32_t samples_per_block, int16_t *in, cs16_t *out, uint32_t in_length);
void lpcsdr_dsp_decimate_reset(lpcsdr_decimate *decimate);
extern unsigned lpcsdr_standard_filter_ntaps;
extern float lpcsdr_standard_filter_taps[];