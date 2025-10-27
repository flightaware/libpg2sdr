#include <stdio.h>
#include "dsp.h"

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
