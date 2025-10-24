#include "dsp.h"

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
