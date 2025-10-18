#include "internal.h"

const float lpcsdr__standard_filter_taps[] = {
    -0.00105091, 0, 0.00250767, 0, -0.0048923, 0, 0.00855213, 0, -0.0139827, 0, 0.0220117,  0, -0.0343224, 0, 0.0552597,  0, -0.100878,   0, 0.316537,
    0.5,
    0.316537, 0, -0.100878,   0, 0.0552597,  0, -0.0343224, 0, 0.0220117,  0, -0.0139827, 0, 0.00855213, 0, -0.0048923, 0, 0.00250767, 0, -0.00105091,
};

const unsigned lpcsdr__standard_filter_ntaps = sizeof(lpcsdr__standard_filter_taps) / sizeof(lpcsdr__standard_filter_taps[0]);

#ifdef HAVE_NEON

#ifndef __ARM_NEON
#  error HAVE_NEON was set, but Neon does not seem to be available
#endif
#include <arm_neon.h>

__attribute__((optimize("no-tree-vectorize"))) /* turn off auto-vectorization */
static uint32_t halfband_decimate_block(const dsp_halfband_decimate_state_t *state, const cs16_t * restrict in, uint32_t count, cs16_t * restrict out)
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

#else

static uint32_t halfband_decimate_block(const dsp_halfband_decimate_state_t *state, const cs16_t * restrict in, uint32_t count, cs16_t * restrict out)
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

#endif

/* Build the history-processing wrapper */
#define PROCESS_FN lpcsdr__dsp_halfband_decimate_process
#define BLOCK_FN halfband_decimate_block
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

int lpcsdr__dsp_halfband_decimate_create(unsigned halfband_ntaps, const float *halfband_taps, dsp_halfband_decimate_state_t **result)
{
    if (halfband_ntaps % 2 != 1)
        return LPCSDR_ERROR_BAD_ARGUMENT; /* must have an odd number of taps */

    float center_tap = halfband_taps[(halfband_ntaps - 1) / 2];
    if (center_tap == 0)
        return LPCSDR_ERROR_BAD_ARGUMENT; /* center tap must be nonzero */

    float sum_taps = 0; /* sum of absolute tap values; used to scale coefficients to avoid overflow */
    for (unsigned i = 0; i < halfband_ntaps / 2; ++i) {
        if ((halfband_ntaps / 2 - i) % 2 == 0 && halfband_taps[i] != 0.0)
            return LPCSDR_ERROR_BAD_ARGUMENT; /* doesn't follow the expected halfband filter structure */
        if (halfband_taps[i] != halfband_taps[halfband_ntaps - i - 1])
            return LPCSDR_ERROR_BAD_ARGUMENT; /* must be symmetric */
        if (fabs(halfband_taps[i]) > fabs(center_tap))
            return LPCSDR_ERROR_BAD_ARGUMENT; /* no tap should be larger than the center tap */
        sum_taps += fabs(halfband_taps[i]) * 2;
    }

    sum_taps += center_tap;

    int error = LPCSDR_SUCCESS;
    dsp_halfband_decimate_state_t *state = calloc(1, sizeof(*state));
    if (!state)
        return LPCSDR_ERROR_NO_MEMORY;

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
        error = LPCSDR_ERROR_NO_MEMORY;
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
    *result = state;
    return LPCSDR_SUCCESS;

 fail:
    lpcsdr__dsp_halfband_decimate_free(state);
    return error;
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

int lpcsdr__dsp_downconvert_create(unsigned halfband_ntaps, const float *halfband_taps, uint32_t max_in_length, dsp_downconvert_state_t **result)
{
    int error = LPCSDR_SUCCESS;
    dsp_downconvert_state_t *state = calloc(1, sizeof(*state));
    if (!state)
        return LPCSDR_ERROR_NO_MEMORY;

    if ((error = lpcsdr__dsp_halfband_decimate_create(halfband_ntaps, halfband_taps, &state->decimate)) < 0)
        goto fail;

    state->max_in_length = max_in_length;
    if (!(state->buffer = malloc(state->max_in_length * sizeof(cs16_t)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto fail;
    }

    lpcsdr__dsp_downconvert_reset(state);
    *result = state;
    return LPCSDR_SUCCESS;

 fail:
    lpcsdr__dsp_downconvert_free(state);
    return error;
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
