/*
 *  halfband_decimate.neon.c - DSP, halfband decimator, ARM NEON/SIMD impl
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

#ifndef __ARM_NEON
#  error Neon intrinsics are not available
#endif
#include <arm_neon.h>

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("no-tree-vectorize"))) /* turn off auto-vectorization as it over-optimizes the epilogue */
#endif
uint32_t STARCH_IMPL(halfband_decimate_block, neon_intrinsics) (const dsp_halfband_decimate_state_t *state, const cs16_t * restrict in, uint32_t count, cs16_t * restrict out)
{
    const unsigned ntaps = state->ntaps;
    const int16_t * restrict taps = state->taps;

    assert(count % 2 == 0);

    /* Process 4 complex samples at a time (producing 2 output samples)
     *
     * Strictly, we should only read data up to in[i + ntaps].
     * The vectorized loop can readahead up to 6 samples beyond that in
     * the worst case, so handle the final part of the input buffer separately.
     */
    uint32_t i = 0, out_i = 0;
    for (; i + 6 < count; i += 4, out_i += 2) {
        int32x4_t sum0 = vdupq_n_s32(0);
        int32x4_t sum1 = vdupq_n_s32(0);
        int32x4_t sum2 = vdupq_n_s32(0);
        int32x4_t sum3 = vdupq_n_s32(0);

        const uint32_t * restrict inp = (const uint32_t *)(in + i);
        uint32x4_t in01 = vld1q_u32(inp);                        /* 16x8 { q[3] i[3] q[2] i[2] q[1] i[1] q[0] i[0] } */
        inp += 4;

        /* taps is allocated beyond ntaps to the next multiple of 4, with zero padding,
         * so we don't need to handle the trailing case specially here
         */
        for (uint32_t j = 0; j < ntaps; j += 4) {
            /* load taps */
            int16x4_t t = vld1_s16(taps + j);                    /* 16x4 { t[3] t[2] t[1] t[0] } */

            /* load more samples */
            uint32x4_t in23 = vld1q_u32(inp);                    /* 16x8 { q[7] i[7] q[6] i[6] q[5] i[5] q[4] i[4] } */
            inp += 4;

            /* set up some convenient aliases (doesn't directly generate code) */
            uint32x2_t in0 = vget_low_u32(in01);                 /* 16x4 { q[1] i[1] q[0] i[0] } */
            uint32x2_t in1 = vget_high_u32(in01);                /* 16x4 { q[3] i[3] q[2] i[2] } */
            uint32x2_t in2 = vget_low_u32(in23);                 /* 16x4 { q[5] i[5] q[4] i[4] } */

            /* rearrange the loaded samples into the order we want */
            uint32x2x2_t zip01 = vzip_u32(in0, in1);             /* 16x4x2 { q[2] i[2] q[0] i[0] }, { q[3] i[3] q[1] i[1] } */
            uint32x2x2_t zip12 = vzip_u32(in1, in2);             /* 16x4x2 { q[4] i[4] q[2] i[2] }, { q[5] i[5] q[3] i[3] } */

            /* tell the compiler they're really int16_t (doesn't directly generate code) */
            int16x4_t mul0 = vreinterpret_s16_u32(zip01.val[0]); /* 16x4 { q[2] i[2] q[0] i[0] } */
            int16x4_t mul1 = vreinterpret_s16_u32(zip01.val[1]); /* 16x4 { q[3] i[3] q[1] i[1] } */
            int16x4_t mul2 = vreinterpret_s16_u32(zip12.val[0]); /* 16x4 { q[4] i[4] q[2] i[2] } */
            int16x4_t mul3 = vreinterpret_s16_u32(zip12.val[1]); /* 16x4 { q[5] i[5] q[3] i[3] } */

            /* multiply and accumulate */
            sum0 = vqdmlal_lane_s16(sum0, mul0, t, 0);           /* 32x4 { i[0]*taps[0]+..., q[0]*taps[0]+..., i[2]*taps[0]+..., q[2]*taps[0]+... } */
            sum1 = vqdmlal_lane_s16(sum1, mul1, t, 1);           /* 32x4 { i[1]*taps[1]+..., q[1]*taps[1]+..., i[3]*taps[1]+..., q[3]*taps[1]+... } */
            sum2 = vqdmlal_lane_s16(sum2, mul2, t, 2);           /* 32x4 { i[2]*taps[2]+..., q[2]*taps[2]+..., i[4]*taps[2]+..., q[4]*taps[2]+... } */
            sum3 = vqdmlal_lane_s16(sum3, mul3, t, 3);           /* 32x4 { i[3]*taps[3]+..., q[3]*taps[3]+..., i[5]*taps[3]+..., q[5]*taps[3]+... } */

            /* shift input samples down */
            in01 = in23;
        }

        /* total up sum0..sum3 by lane */
        int32x4_t sum01 = vqaddq_s32(sum0, sum1);                    /* 32x4 { i[0]*taps[0] + i[1]*taps[1] + ..., ... } */
        int32x4_t sum23 = vqaddq_s32(sum2, sum3);                    /* 32x4 { i[2]*taps[2] + i[3]*taps[3] + ..., ... } */
        int16x4_t iq_out = vaddhn_s32(sum01, sum23);                 /* 16x4 { (i[0]*taps[0] + i[1]*taps[1] + i[2]*taps[2] + i[3]*taps[3] + ...) >> 15,
                                                                               (q[0]*taps[0] + q[1]*taps[1] + q[2]*taps[2] + q[3]*taps[3] + ...) >> 15,
                                                                               (i[2]*taps[0] + i[3]*taps[1] + i[4]*taps[2] + i[5]*taps[3] + ...) >> 15,
                                                                               (q[2]*taps[0] + q[3]*taps[1] + q[4]*taps[2] + q[5]*taps[3] + ...) >> 15 } */

        /* store the 4 results */
        vst1_s16((int16_t *) (out + out_i), iq_out);
    }

    /* process the final trailing samples exactly,
     * without the slight lookahead that the vectorized loop does
     */
    for (; i < count; i += 2, out_i += 1) {
        int32_t sum_i = 0;
        int32_t sum_q = 0;
        const cs16_t * restrict inp = in + i;
        for (uint32_t j = 0; j < ntaps; j += 1, inp += 1) {
            sum_i += inp->i * taps[j];
            sum_q += inp->q * taps[j];
        }

        out[out_i].i = (int16_t) (sum_i >> 15);
        out[out_i].q = (int16_t) (sum_q >> 15);
    }

    return out_i;
}
