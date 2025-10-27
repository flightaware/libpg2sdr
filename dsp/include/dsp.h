#ifndef PG2SDR_DSP_H
#define PG2SDR_DSP_H

#include "dsp-types.h"
#include <stdbool.h>
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
    unsigned int context_len;
} dsp_halfband_decimate_state_t;

bool pg2sdr__dsp_halfband_taps_valid(unsigned halfband_ntaps, const float *halfband_taps);
dsp_halfband_decimate_state_t *pg2sdr__dsp_halfband_decimate_create(unsigned halfband_ntaps, const float *halfband_taps);
uint32_t pg2sdr__dsp_halfband_decimate_process(dsp_halfband_decimate_state_t *state, const cs16_t *in, uint32_t in_length, cs16_t *out);
void pg2sdr__dsp_halfband_decimate_free(dsp_halfband_decimate_state_t *state);
void pg2sdr__dsp_halfband_decimate_reset(dsp_halfband_decimate_state_t *state);

typedef struct {
    dsp_halfband_decimate_state_t *decimate;

    cs16_t *buffer;
    uint32_t max_in_length;
} dsp_downconvert_state_t;

dsp_downconvert_state_t *pg2sdr__dsp_downconvert_create(unsigned halfband_ntaps, const float *halfband_taps, uint32_t max_in_length);
uint32_t pg2sdr__dsp_downconvert_process(dsp_downconvert_state_t *state, const int16_t *in, uint32_t in_length, cs16_t *out);
void pg2sdr__dsp_downconvert_free(dsp_downconvert_state_t *state);
void pg2sdr__dsp_downconvert_reset(dsp_downconvert_state_t *state);

extern const unsigned pg2sdr__dsp_default_halfband_ntaps;
extern const float pg2sdr__dsp_default_halfband_taps[];

#if defined(__cplusplus)
}
#endif

#endif /* PG2SDR_DSP_H */
