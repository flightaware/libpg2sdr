/*
 *  unpack_raw_adc_data.generic.c - DSP, ADC unpacking, generic impl
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


/* Convert "words" uint32_t words of packed ADC data stored at "in":
 *   1. unpack the sample values from the packed representation
 *   2. scale from 12-bit twos-complement to 16-bit twos-complement
 *   3. write results to "out"
 *
 * caller must ensure:
 *   "words" is a multiple of 3
 *   "out" has at least enough space for (words * 8 / 3) int16_t values
 */

void STARCH_IMPL(unpack_raw_adc_data, generic) (const uint32_t * restrict in, uint32_t words, int16_t * restrict out)
{
    assert(words % 3 == 0);

    for (unsigned i = 0; i < words; i += 3, in += 3, out += 8) {
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

/* Like `unpack_raw_adc_data`, but also invert the spectrum by negating every second sample */
void STARCH_IMPL(unpack_raw_adc_data_invert, generic) (const uint32_t * restrict in, uint32_t words, int16_t * restrict out)
{
    assert(words % 3 == 0);

    for (unsigned i = 0; i < words; i += 3, in += 3, out += 8) {
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

