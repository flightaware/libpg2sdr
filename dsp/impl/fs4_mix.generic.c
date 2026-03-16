#include "dsp/dsp.h"

#include <assert.h>

uint32_t STARCH_IMPL(fs4_mix, generic) (const int16_t * restrict in, uint32_t in_length, cs16_t * restrict out)
{
    assert (in_length % 4 == 0);
    for (uint32_t i = 0; i < in_length; i += 4, in += 4, out += 4) {
        out[0].i = in[0];
        out[0].q = 0;
        out[1].i = 0;
        out[1].q = -in[1];
        out[2].i = -in[2];
        out[2].q = 0;
        out[3].i = 0;
        out[3].q = in[3];
    }
    return in_length;
}

