#ifndef LPCSDR_DSP_H
#define LPCSDR_DSP_H

#include "dsp-types.h"
#include <complex.h>
#include <math.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
    unsigned int ntaps;
    int16_t *taps;

    cs16_t *history;
    unsigned int history_max;
    unsigned int history_len;
} dsp_halfband_decimate_state_t;

int lpcsdr__dsp_halfband_decimate_create(unsigned halfband_ntaps, const float *halfband_taps, dsp_halfband_decimate_state_t **result);
uint32_t lpcsdr__dsp_halfband_decimate_process(dsp_halfband_decimate_state_t *state, const cs16_t *in, uint32_t in_length, cs16_t *out);
void lpcsdr__dsp_halfband_decimate_free(dsp_halfband_decimate_state_t *state);
void lpcsdr__dsp_halfband_decimate_reset(dsp_halfband_decimate_state_t *state);

typedef struct {
    dsp_halfband_decimate_state_t *decimate;

    cs16_t *buffer;
    uint32_t max_in_length;
} dsp_downconvert_state_t;

int lpcsdr__dsp_downconvert_create(unsigned halfband_ntaps, const float *halfband_taps, uint32_t max_in_length, dsp_downconvert_state_t **result);
uint32_t lpcsdr__dsp_downconvert_process(dsp_downconvert_state_t *state, const int16_t *in, uint32_t in_length, cs16_t *out);
void lpcsdr__dsp_downconvert_free(dsp_downconvert_state_t *state);
void lpcsdr__dsp_downconvert_reset(dsp_downconvert_state_t *state);

extern const unsigned lpcsdr__standard_filter_ntaps;
extern const float lpcsdr__standard_filter_taps[];

#if defined(__cplusplus)
}
#endif

#endif /* LPCSDR_DSP_H */
