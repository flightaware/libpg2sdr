/*
 *  fs4_mix.benchmark.c - DSP, Fs/4 mixer, benchmark impl
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

void STARCH_BENCHMARK(fs4_mix) (void)
{
    int16_t *in = NULL;
    cs16_t *out = NULL;
    const unsigned len = 65536 - 4;

    if (!(in = calloc(len, sizeof(*in))))
        goto done;
    if (!(out = calloc(len, sizeof(*out))))
        goto done;

    for (unsigned i = 0; i < len; ++i) {
        in[i] = rand() % 65536;
    }

    STARCH_BENCHMARK_RUN(fs4_mix, in, len, out);

 done:
    free(in);
    free(out);
}

bool STARCH_BENCHMARK_VERIFY(fs4_mix)(const int16_t *in, unsigned len, cs16_t *out)
{
    bool okay = true;

    for (unsigned i = 0; i < len; i += 4, in += 4, out += 4) {
        if (out[0].i != in[0] ||
            out[0].q != 0 ||
            out[1].i != 0 ||
            out[1].q != -in[1] ||
            out[2].i != -in[2] ||
            out[2].q != 0 ||
            out[3].i != 0 ||
            out[3].q != in[3]) {
            fprintf(stderr,
                    "verification failed: \n"
                    "  in[%u..]  = { %+d,%+d,%+d,%+d }\n"
                    "  out[%u..] = { %+d+%di,%+d+%di%+d+%di%+d+%di }\n",
                    i, in[0], in[1], in[2], in[3],
                    i, out[0].i, out[0].q, out[1].i, out[1].q, out[2].i, out[2].q, out[3].i, out[3].q);
            okay = false;
        }
    }

    return okay;
}
