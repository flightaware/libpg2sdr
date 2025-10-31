
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

