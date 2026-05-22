/*
 *  halfband_decimate.generic.c - DSP, halfband decimator, portable impl
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

#include "dsp/dsp.h"

#include <assert.h>

uint32_t STARCH_IMPL(halfband_decimate_block, generic) (const dsp_halfband_decimate_state_t *state, const cs16_t * restrict in, uint32_t count, cs16_t * restrict out)
{
    const unsigned ntaps = state->ntaps;
    const int16_t * restrict taps = state->taps;
    const unsigned center_tap_offset = (ntaps-1)/2;
    const int16_t center_tap = taps[center_tap_offset];

    assert(count % 2 == 0);

    uint32_t out_i = 0;
    for (uint32_t i = 0; i < count; i += 2, out_i += 1) {
        int32_t sum_i = center_tap * in[i + center_tap_offset].i;
        int32_t sum_q = center_tap * in[i + center_tap_offset].q;
        const cs16_t * restrict inp = in + i;
        for (uint32_t j = 0; j < ntaps; j += 2, inp += 2) {
            sum_i += inp->i * taps[j];
            sum_q += inp->q * taps[j];
        }

        out[out_i].i = (int16_t) (sum_i >> 15);
        out[out_i].q = (int16_t) (sum_q >> 15);
    }

    return out_i;
}
