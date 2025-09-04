#include "internal.h"

float lpcsdr__standard_filter_taps[] = {
    0, -0.00105091, 0, 0.00250767, 0, -0.0048923, 0, 0.00855213, 0, -0.0139827, 0, 0.0220117,  0, -0.0343224, 0, 0.0552597,  0, -0.100878,   0, 0.316537,
    0.5,
    0.316537, 0, -0.100878,   0, 0.0552597,  0, -0.0343224, 0, 0.0220117,  0, -0.0139827, 0, 0.00855213, 0, -0.0048923, 0, 0.00250767, 0, -0.00105091, 0,
};

unsigned lpcsdr__standard_filter_ntaps = sizeof(lpcsdr__standard_filter_taps) / sizeof(lpcsdr__standard_filter_taps[0]);

static uint32_t halfband_decimate_block(const unsigned int ntaps, const int16_t *taps, const cs16_t *in, uint32_t in_length, cs16_t *out)
{
    assert (in_length % 2 == 0);

    const unsigned center_tap_offset = (ntaps-1)/2;
    const int16_t center_tap = taps[center_tap_offset];

    uint32_t window_end = 0;
    uint32_t out_index = 0;
    for (; window_end < in_length; window_end += 2, out_index++) {
        int32_t q_sum = center_tap * in[window_end + center_tap_offset].q;
        int32_t i_sum = center_tap * in[window_end + center_tap_offset].i;
        uint32_t tap_pointer = 0;
        uint32_t current = window_end;

        while (tap_pointer < ntaps) {
            q_sum += (taps[tap_pointer] * in[current].q);
            i_sum += (taps[tap_pointer] * in[current].i); 
            tap_pointer += 2;
            current += 2;
        }

        out[out_index].i = (int16_t) (i_sum >> 15);
        out[out_index].q = (int16_t) (q_sum >> 15);
    }

    return out_index;
}

static uint32_t lpcsdr__dsp_halfband_decimate(dsp_halfband_decimate_state_t *state, const cs16_t *in, uint32_t in_length, cs16_t *out)
{
    const unsigned ntaps = state->ntaps;
    const int16_t *taps = state->taps;
    cs16_t *history = state->history;
    const unsigned history_len = state->history_len;
    const unsigned history_max = state->history_max;

    unsigned history_fill = history_max - history_len; /* elements required to completely fill the history buffer */
    if (history_fill > in_length)                      /* .. but we can't fill with more elements than we have available */
        history_fill = in_length;
    memcpy_elements(history + history_len, in, history_fill); /* fill the history buffer */

    const unsigned history_available = history_len + history_fill;             /* elements in the history buffer, including what we just copied */
    if (history_available < ntaps) {
        /* not enough history to do any useful processing, just retain what we have */
        state->history_len = history_available;
        return 0;
    }

    /* if history_available == ntaps, we can process 1 window
     * if history_available == ntaps + 1, we can process 2 windows
     * etc
     */
    const unsigned history_processed = (history_available - ntaps + 1) & ~1; /* number of windows we can process from the history buffer, multiple of 2 */
    if (!history_processed) {
        /* not enough history to do any useful processing, just retain what we have */
        state->history_len = history_available;
        return 0;
    }

    uint32_t out_produced = halfband_decimate_block(ntaps, taps, history, history_processed, out);

    /* First unprocessed window in history starts at history[history_processed].
     * We copied in[0] to history[history_len].
     * So the first unprocessed window in input data starts at in[history_processed - history_len]
     */
    if (history_processed < history_len) {
        /* no further data to process in the input, just shift history down */
        memmove_elements(history, history + history_processed, history_available - history_processed);
        state->history_len = history_available - history_processed;
        return out_produced;
    }

    const unsigned offset = history_processed - history_len;                  /* offset in input of first unprocessed window */
    const unsigned main_processed = ((in_length - offset) - ntaps + 1) & ~1;  /* number of windows we can process from the input buffer, multiple of 2 */

    /* process the main block */
    out_produced += halfband_decimate_block(ntaps, taps, in + offset, main_processed, out + out_produced);

    /* preserve history starting from the first unprocessed window in input data */
    const unsigned input_consumed = offset + main_processed;
    memcpy_elements(history, in + input_consumed, in_length - input_consumed);
    state->history_len = in_length - input_consumed;

    return out_produced;
}

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

    unsigned first_nonzero;
    if (halfband_ntaps % 4 == 1) {
        /* First nonzero tap is at index 1 */
        assert(halfband_taps[0] == 0.0);
        first_nonzero = 1;
        state->ntaps = halfband_ntaps - 2;
    } else {
        /* First nonzero tap must be at index 0 */
        assert(halfband_taps[0] != 0.0);
        first_nonzero = 0;
        state->ntaps = halfband_ntaps;
    }
    state->history_max = state->ntaps * 2 + 2;

    if (!(state->taps = malloc(state->ntaps * sizeof(int16_t))) ||
        !(state->history = malloc(state->history_max * sizeof(cs16_t)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto fail;
    }

    /* scale taps so that the output cannot ever overflow a Q15 representation */
    float scale = 32767 / sum_taps;
    for (unsigned i = 0; i < state->ntaps; ++i) {
        state->taps[i] = (int16_t)(halfband_taps[first_nonzero + i] * scale + 0.5);
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

    state->history_len = state->history_max / 2;
    memset_elements(state->history, 0, state->history_len);
}

static uint32_t fs4_mix(const int16_t *in, uint32_t in_length, cs16_t *out)
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
    uint32_t decimated_length = lpcsdr__dsp_halfband_decimate(state->decimate, state->buffer, mixed_length, out);

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
