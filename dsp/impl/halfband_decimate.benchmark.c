/*
 *  halfband_decimate.benchmark.c - DSP, halfband decimator, benchmark
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

#include <stdio.h>
#include "dsp/dsp.h"

void STARCH_BENCHMARK(halfband_decimate_block) (void)
{
    cs16_t *in = NULL, *out = NULL;
    dsp_halfband_decimate_state_t *state = NULL;
    const unsigned len = 65536;

    if (!(in = calloc(len, sizeof(*in))))
        goto done;
    if (!(out = calloc(len, sizeof(*out))))
        goto done;

    for (unsigned i = 0; i < len; ++i) {
        in[i].i = rand() % 65536 - 32768;
        in[i].q = rand() % 65536 - 32768;
    }

    if (!(state = pg2sdr__dsp_halfband_decimate_create(pg2sdr__dsp_default_halfband_ntaps, pg2sdr__dsp_default_halfband_taps)))
        goto done;

    STARCH_BENCHMARK_RUN(halfband_decimate_block, state, in, len, out);

 done:
    pg2sdr__dsp_halfband_decimate_free(state);
    free(in);
    free(out);
}
