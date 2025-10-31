#include <stdio.h>
#include "dsp.h"

void STARCH_BENCHMARK(unpack_raw_adc_data) (void)
{
    uint32_t *in = NULL;
    int16_t *out = NULL;
    const unsigned samples = 6808;
    const unsigned in_words = samples * 12 / 32;

    if (!(in = calloc(in_words, sizeof(*in))))
        goto done;
    if (!(out = calloc(samples, sizeof(*out))))
        goto done;

    for (unsigned i = 0; i < in_words; ++i) {
        in[i] = rand();
    }

    STARCH_BENCHMARK_RUN(unpack_raw_adc_data, in, in_words, out);

 done:
    free(in);
    free(out);
}


bool STARCH_BENCHMARK_VERIFY(unpack_raw_adc_data) (const uint32_t *in, unsigned words, int16_t *out)
{
    bool okay = true;

    for (unsigned i = 0; i < words; i += 3, in += 3, out += 8) {
        uint32_t first = in[0];
        uint32_t second = in[1];
        uint32_t third = in[2];

        /* 12->16 bit scaling is baked into the bitshifts here */
        int16_t out0 = (int16_t) ((first  & 0x00000FFF) << 4);
        int16_t out1 = (int16_t) ((first  & 0x0FFF0000) >> 12);
        int16_t out2 = (int16_t) ((second & 0x00000FFF) << 4);
        int16_t out3 = (int16_t) ((second & 0x0FFF0000) >> 12);
        int16_t out4 = (int16_t) ((third  & 0x00000FFF) << 4);
        int16_t out5 = (int16_t) ((third  & 0x0FFF0000) >> 12);
        int16_t out6 = (int16_t) (((first & 0x0000F000)) | ((second & 0x0000F000) >> 4) | ((third & 0x0000F000) >> 8));
        int16_t out7 = (int16_t) (((first & 0xF0000000) >> 16) | ((second & 0xF0000000) >> 20)  | ((third & 0xF0000000) >> 24));

        if (out[0] != out0 ||
            out[1] != out1 ||
            out[2] != out2 ||
            out[3] != out3 ||
            out[4] != out4 ||
            out[5] != out5 ||
            out[6] != out6 ||
            out[7] != out7) {
            fprintf(stderr,
                    "verification failed: \n"
                    "  in[%u..]  = { %08x %08x %08x }\n"
                    "  out = { %+d %+d %+d %+d %+d %+d %+d %+d }\n"
                    "  expected =  { %+d %+d %+d %+d %+d %+d %+d %+d }\n",                    
                    i, in[0], in[1], in[2],
                    out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7],
                    out0, out1, out2, out3, out4, out5, out6, out7);
        }
    }

    return okay;
}

void STARCH_BENCHMARK(unpack_raw_adc_data_invert) (void)
{
    uint32_t *in = NULL;
    int16_t *out = NULL;
    const unsigned samples = 6808;
    const unsigned in_words = samples * 12 / 32;

    if (!(in = calloc(in_words, sizeof(*in))))
        goto done;
    if (!(out = calloc(samples, sizeof(*out))))
        goto done;

    for (unsigned i = 0; i < in_words; ++i) {
        in[i] = rand();
    }

    STARCH_BENCHMARK_RUN(unpack_raw_adc_data_invert, in, in_words, out);

 done:
    free(in);
    free(out);
}

bool STARCH_BENCHMARK_VERIFY(unpack_raw_adc_data_invert) (const uint32_t *in, unsigned words, int16_t *out)
{
    bool okay = true;

    for (unsigned i = 0; i < words; i += 3, in += 3, out += 8) {
        uint32_t first = in[0];
        uint32_t second = in[1];
        uint32_t third = in[2];

        /* 12->16 bit scaling is baked into the bitshifts here */
        int16_t out0 = (int16_t) ((first  & 0x00000FFF) << 4);
        int16_t out1 = -(int16_t) ((first  & 0x0FFF0000) >> 12);
        int16_t out2 = (int16_t) ((second & 0x00000FFF) << 4);
        int16_t out3 = -(int16_t) ((second & 0x0FFF0000) >> 12);
        int16_t out4 = (int16_t) ((third  & 0x00000FFF) << 4);
        int16_t out5 = -(int16_t) ((third  & 0x0FFF0000) >> 12);
        int16_t out6 = (int16_t) (((first & 0x0000F000)) | ((second & 0x0000F000) >> 4) | ((third & 0x0000F000) >> 8));
        int16_t out7 = -(int16_t) (((first & 0xF0000000) >> 16) | ((second & 0xF0000000) >> 20)  | ((third & 0xF0000000) >> 24));

        if (out[0] != out0 ||
            out[1] != out1 ||
            out[2] != out2 ||
            out[3] != out3 ||
            out[4] != out4 ||
            out[5] != out5 ||
            out[6] != out6 ||
            out[7] != out7) {
            fprintf(stderr,
                    "verification failed: \n"
                    "  in[%u..]  = { %08x %08x %08x }\n"
                    "  out = { %+d %+d %+d %+d %+d %+d %+d %+d }\n"
                    "  expected =  { %+d %+d %+d %+d %+d %+d %+d %+d }\n",                    
                    i, in[0], in[1], in[2],
                    out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7],
                    out0, out1, out2, out3, out4, out5, out6, out7);
            okay = false;
        }
    }

    return okay;
}
