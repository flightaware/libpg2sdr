#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "dsp.h"
#include "starch.h"

#define memset_elements(_dst, _val, _count) memset((_dst), (_val), (_count) * sizeof((_dst)[0]))
#define memmove_elements(_dst, _src, _count) memmove((_dst), (_src), (_count) * sizeof((_dst)[0]))
#define memcpy_elements(_dst, _src, _count) memcpy((_dst), (_src), (_count) * sizeof((_dst)[0]))

const float lpcsdr__dsp_default_halfband_taps[] = {
    -0.00105091, 0, 0.00250767, 0, -0.0048923, 0, 0.00855213, 0, -0.0139827, 0, 0.0220117,  0, -0.0343224, 0, 0.0552597,  0, -0.100878,   0, 0.316537,
    0.5,
    0.316537, 0, -0.100878,   0, 0.0552597,  0, -0.0343224, 0, 0.0220117,  0, -0.0139827, 0, 0.00855213, 0, -0.0048923, 0, 0.00250767, 0, -0.00105091,
};

const unsigned lpcsdr__dsp_default_halfband_ntaps = sizeof(lpcsdr__dsp_default_halfband_taps) / sizeof(lpcsdr__dsp_default_halfband_taps[0]);


/* Build the history-processing wrapper */
#define PROCESS_FN lpcsdr__dsp_halfband_decimate_process
#define BLOCK_FN lpcsdr__starch_halfband_decimate_block
#define STATE_TYPE dsp_halfband_decimate_state_t
#define IN_TYPE cs16_t
#define OUT_TYPE cs16_t
#define IN_BLOCK_LEN 2
# include "dsp-history.inc"
#undef PROCESS_FN
#undef BLOCK_FN
#undef STATE_TYPE
#undef IN_TYPE
#undef OUT_TYPE
#undef IN_BLOCK_LEN

bool lpcsdr__dsp_halfband_taps_valid(unsigned halfband_ntaps, const float *halfband_taps)
{
    if (!halfband_taps)
        return false;

    if (halfband_ntaps % 2 != 1)
        return false; /* must have an odd number of taps */

    float center_tap = halfband_taps[(halfband_ntaps - 1) / 2];
    if (center_tap == 0)
        return false; /* center tap must be nonzero */

    for (unsigned i = 0; i < halfband_ntaps / 2; ++i) {
        if ((halfband_ntaps / 2 - i) % 2 == 0 && halfband_taps[i] != 0.0)
            return false; /* doesn't follow the expected halfband filter structure */
        if (halfband_taps[i] != halfband_taps[halfband_ntaps - i - 1])
            return false; /* must be symmetric */
        if (fabs(halfband_taps[i]) > fabs(center_tap))
            return false; /* no tap should be larger than the center tap */
    }

    return true;
}


dsp_halfband_decimate_state_t *lpcsdr__dsp_halfband_decimate_create(unsigned halfband_ntaps, const float *halfband_taps)
{
    if (!lpcsdr__dsp_halfband_taps_valid(halfband_ntaps, halfband_taps))
        return false;
    
    float center_tap = halfband_taps[(halfband_ntaps - 1) / 2];
    float sum_taps = 0; /* sum of absolute tap values; used to scale coefficients to avoid overflow */
    for (unsigned i = 0; i < halfband_ntaps / 2; ++i) {
        sum_taps += fabs(halfband_taps[i]) * 2;
    }
    sum_taps += center_tap;

    dsp_halfband_decimate_state_t *state = calloc(1, sizeof(*state));
    if (!state)
        return NULL;

    if (halfband_ntaps % 4 == 1) {
        /* First nonzero tap is at index 1 */
        assert(halfband_taps[0] == 0.0);
        halfband_taps += 1;
        halfband_ntaps -= 2;
    } else {
        /* First nonzero tap must be at index 0 */
        assert(halfband_taps[0] != 0.0);
    }

    state->ntaps = halfband_ntaps;
    state->context_len = state->ntaps;
    state->history_max = state->context_len * 2;

    /* pad out actually allocated taps space to a multiple of 4 for the NEON implementation */
    unsigned ntapspad = state->ntaps + (4 - state->ntaps % 4);

    if (!(state->taps = aligned_alloc(16, ntapspad * sizeof(int16_t))) ||
        !(state->history = aligned_alloc(16, state->history_max * sizeof(cs16_t)))) {
        goto fail;
    }

    /* scale taps so that the output cannot ever overflow a Q15 representation */
    float scale = 32767 / sum_taps;
    for (unsigned i = 0; i < ntapspad; ++i) {
        if (i < halfband_ntaps)
            state->taps[i] = (int16_t)(halfband_taps[i] * scale + 0.5);
        else
            state->taps[i] = 0;
    }

    lpcsdr__dsp_halfband_decimate_reset(state);
    return state;

 fail:
    lpcsdr__dsp_halfband_decimate_free(state);
    return NULL;
}

void lpcsdr__dsp_halfband_decimate_free(dsp_halfband_decimate_state_t *state)
{
    if (!state)
        return;

    free(state->taps);
    free(state->history);
    free(state);
}

void lpcsdr__dsp_halfband_decimate_reset(dsp_halfband_decimate_state_t *state)
{
    if (!state)
        return;

    state->history_len = state->context_len - 1;
    memset_elements(state->history, 0, state->history_len);
}

static uint32_t fs4_mix(const int16_t * restrict in, uint32_t in_length, cs16_t * restrict out)
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

uint32_t lpcsdr__dsp_downconvert_process(dsp_downconvert_state_t *state, const int16_t *in, uint32_t in_length, cs16_t *out)
{
    assert (in_length % 4 == 0);
    assert (in_length <= state->max_in_length);

    uint32_t mixed_length = fs4_mix(in, in_length, state->buffer);
    uint32_t decimated_length = lpcsdr__dsp_halfband_decimate_process(state->decimate, state->buffer, mixed_length, out);

    return decimated_length;
}

dsp_downconvert_state_t *lpcsdr__dsp_downconvert_create(unsigned halfband_ntaps, const float *halfband_taps, uint32_t max_in_length)
{
    dsp_downconvert_state_t *state = calloc(1, sizeof(*state));
    if (!state)
        return NULL;

    if (!(state->decimate = lpcsdr__dsp_halfband_decimate_create(halfband_ntaps, halfband_taps)))
        goto fail;

    state->max_in_length = max_in_length;
    if (!(state->buffer = malloc(state->max_in_length * sizeof(cs16_t)))) {
        goto fail;
    }

    lpcsdr__dsp_downconvert_reset(state);
    return state;

 fail:
    lpcsdr__dsp_downconvert_free(state);
    return NULL;
}

void lpcsdr__dsp_downconvert_free(dsp_downconvert_state_t *state)
{
    if (!state)
        return;

    lpcsdr__dsp_halfband_decimate_free(state->decimate);
    free(state->buffer);
    free(state);
}

void lpcsdr__dsp_downconvert_reset(dsp_downconvert_state_t *state)
{
    if (!state)
        return;

    lpcsdr__dsp_halfband_decimate_reset(state->decimate);
}
