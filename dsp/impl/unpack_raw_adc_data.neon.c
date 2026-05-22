/*
 *  unpack_raw_adc_data.neon.c - DSP, ADC unpacking, ARM NEON/SIMD impl
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

#ifndef __ARM_NEON
#  error Neon intrinsics are not available
#endif
#include <arm_neon.h>

void STARCH_IMPL(unpack_raw_adc_data, neon_intrinsics) (const uint32_t * restrict in, uint32_t words, int16_t * restrict out)
{
    assert(words % 3 == 0);

    const int16x8_t bitmask = vreinterpretq_s16_u16(vdupq_n_u16(0xFFF0));

    uint32_t * restrict out32 = (uint32_t *)out;
    unsigned i;
    for (i = 0; i+12 <= words; i += 12) {
        uint32x4x3_t indata = vld3q_u32(in);
        in += 12;

        int16x8_t in0 = vreinterpretq_s16_u32(indata.val[0]);
        int16x8_t in1 = vreinterpretq_s16_u32(indata.val[1]);
        int16x8_t in2 = vreinterpretq_s16_u32(indata.val[2]);

        /* first 3 are easy, take each 16-bit nibble and shift left 4 bits, discarding the top bits */
        int16x8_t out0 = vshlq_n_s16(in0, 4);
        int16x8_t out1 = vshlq_n_s16(in1, 4);
        int16x8_t out2 = vshlq_n_s16(in2, 4);

        /* final one is harder, we need to assemble it from those discarded top bits of the first 3 nibbles
         * assume we start with lanes containing these 16 bit values (x = dontcare)
         *   [0] = Axxx
         *   [1] = Bxxx
         *   [2] = Cxxx
         */
        int16x8_t out3 = in0;                   /* Axxx */
        out3 = vsriq_n_s16(out3, in1, 4);       /* A000 | 0Bxx -> ABxx */
        out3 = vsriq_n_s16(out3, in2, 8);       /* AB00 | 00Cx -> ABCx */
        out3 = vandq_s16(out3, bitmask);        /* ABCx -> ABC0 */

        uint32x4x4_t outdata;
        outdata.val[0] = vreinterpretq_u32_s16(out0);
        outdata.val[1] = vreinterpretq_u32_s16(out1);
        outdata.val[2] = vreinterpretq_u32_s16(out2);
        outdata.val[3] = vreinterpretq_u32_s16(out3);
        vst4q_u32(out32, outdata);
        out32 += 16;
    }

    /* handle trailing data */
    out = (int16_t*) out32;
    for (; i < words; i += 3, in += 3, out += 8) {
        uint32_t first = in[0];
        uint32_t second = in[1];
        uint32_t third = in[2];

        /* 12->16 bit scaling is baked into the bitshifts here */
        out[0] = (int16_t) ((first  & 0x00000FFF) << 4);
        out[1] = (int16_t) ((first  & 0x0FFF0000) >> 12);
        out[2] = (int16_t) ((second & 0x00000FFF) << 4);
        out[3] = (int16_t) ((second & 0x0FFF0000) >> 12);
        out[4] = (int16_t) ((third  & 0x00000FFF) << 4);
        out[5] = (int16_t) ((third  & 0x0FFF0000) >> 12);
        out[6] = (int16_t) (((first & 0x0000F000)) | ((second & 0x0000F000) >> 4) | ((third & 0x0000F000) >> 8));
        out[7] = (int16_t) (((first & 0xF0000000) >> 16) | ((second & 0xF0000000) >> 20)  | ((third & 0xF0000000) >> 24));
    }    
}

void STARCH_IMPL(unpack_raw_adc_data_invert, neon_intrinsics) (const uint32_t * restrict in, uint32_t words, int16_t * restrict out)
{
    assert(words % 3 == 0);

    /* This is the same as the unpack_raw_adc_data implementation above, except it also multiplies
     * by negate_q at the end to negate every second sample.
     */

    const int16x8_t bitmask = vreinterpretq_s16_u16(vdupq_n_u16(0xFFF0));
    const int16x4_t negate = vcreate_s16(0xFFFF0001FFFF0001); /* { -1, 1, -1, 1 } */
    const int16x8_t negate_q = vcombine_s16(negate, negate);  /* { -1, 1, -1, 1, -1, 1, -1, 1 } */

    uint32_t * restrict out32 = (uint32_t *)out;
    unsigned i;
    for (i = 0; i+12 <= words; i += 12) {
        uint32x4x3_t indata = vld3q_u32(in);
        in += 12;

        int16x8_t in0 = vreinterpretq_s16_u32(indata.val[0]);
        int16x8_t in1 = vreinterpretq_s16_u32(indata.val[1]);
        int16x8_t in2 = vreinterpretq_s16_u32(indata.val[2]);

        /* first 3 are easy, take each 16-bit nibble and shift left 4 bits, discarding the top bits */
        int16x8_t out0 = vshlq_n_s16(in0, 4);
        int16x8_t out1 = vshlq_n_s16(in1, 4);
        int16x8_t out2 = vshlq_n_s16(in2, 4);

        /* final one is harder, we need to assemble it from those discarded top bits of the first 3 nibbles
         * assume we start with lanes containing these 16 bit values (x = dontcare)
         *   [0] = Axxx
         *   [1] = Bxxx
         *   [2] = Cxxx
         */
        int16x8_t out3 = in0;                   /* Axxx */
        out3 = vsriq_n_s16(out3, in1, 4);       /* A000 | 0Bxx -> ABxx */
        out3 = vsriq_n_s16(out3, in2, 8);       /* AB00 | 00Cx -> ABCx */
        out3 = vandq_s16(out3, bitmask);        /* ABCx -> ABC0 */

        uint32x4x4_t outdata;
        outdata.val[0] = vreinterpretq_u32_s16(vmulq_s16(negate_q, out0));
        outdata.val[1] = vreinterpretq_u32_s16(vmulq_s16(negate_q, out1));
        outdata.val[2] = vreinterpretq_u32_s16(vmulq_s16(negate_q, out2));
        outdata.val[3] = vreinterpretq_u32_s16(vmulq_s16(negate_q, out3));
        vst4q_u32(out32, outdata);
        out32 += 16;
    }

    /* handle trailing data */
    out = (int16_t*) out32;
    for (; i < words; i += 3, in += 3, out += 8) {
        uint32_t first = in[0];
        uint32_t second = in[1];
        uint32_t third = in[2];

        /* 12->16 bit scaling is baked into the bitshifts here */
        out[0] =   (int16_t) ((first  & 0x00000FFF) << 4);
        out[1] = - (int16_t) ((first  & 0x0FFF0000) >> 12);
        out[2] =   (int16_t) ((second & 0x00000FFF) << 4);
        out[3] = - (int16_t) ((second & 0x0FFF0000) >> 12);
        out[4] =   (int16_t) ((third  & 0x00000FFF) << 4);
        out[5] = - (int16_t) ((third  & 0x0FFF0000) >> 12);
        out[6] =   (int16_t) (((first & 0x0000F000)) | ((second & 0x0000F000) >> 4) | ((third & 0x0000F000) >> 8));
        out[7] = - (int16_t) (((first & 0xF0000000) >> 16) | ((second & 0xF0000000) >> 20)  | ((third & 0xF0000000) >> 24));
    }    
}
