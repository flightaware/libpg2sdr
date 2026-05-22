/*
 *  dsp.h - PG2 host library internal DSP API
 *
 *  Copyright (c) 2026 FlightAware All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
