/* -*- mode: c -*- */

/*
 *  dsp-history.inc - helper for DSP operations requiring history
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

/* Intended to be included with various macros defined:
 *
 *  PROCESS_FN:   process function to define
 *  BLOCK_FN:     block function to call
 *  STATE_TYPE:   state datatype
 *    -> should have members "history_len", "history_max", "history", "context_len"
 *  IN_TYPE:      input datatype
 *  OUT_TYPE:     output datatype
 *  IN_BLOCK_LEN: BLOCK_FN requires input samples in multiples of IN_SAMPLES length
 */

uint32_t PROCESS_FN(STATE_TYPE *state,
                    const IN_TYPE * restrict in, uint32_t in_length,
                    OUT_TYPE * restrict out)
{
    IN_TYPE * const history = state->history;
    const unsigned history_len = state->history_len;
    const unsigned history_max = state->history_max;
    const unsigned context_len = state->context_len;

    unsigned history_fill = history_max - history_len; /* elements required to completely fill the history buffer */
    if (history_fill > in_length)                      /* .. but we can't fill with more elements than we have available */
        history_fill = in_length;
    memcpy_elements(history + history_len, in, history_fill); /* fill the history buffer */

    const unsigned history_available = history_len + history_fill;             /* elements in the history buffer, including what we just copied */
    if (history_available < context_len) {
        /* not enough history to do any useful processing, just retain what we have */
        state->history_len = history_available;
        return 0;
    }

    /* We have enough history to process (history_available - context_len + 1) input samples
     * .. but we can only process them in multiples of IN_BLOCK_LEN
     */
    const unsigned history_blocks_to_process = (history_available - context_len + 1) / IN_BLOCK_LEN;
    const unsigned history_samples_to_process = history_blocks_to_process * IN_BLOCK_LEN;
    if (!history_blocks_to_process) {
        /* not enough history to do any useful processing, just retain what we have */
        state->history_len = history_available;
        return 0;
    }

    /* Process blocks from history */
    uint32_t out_produced = BLOCK_FN(state, history, history_samples_to_process, out);

    /* First unprocessed block in history starts at history[history_samples_to_process].
     * We copied in[0] to history[history_len].
     * So the first unprocessed block in input data starts at in[history_samples_to_process - history_len]
     */
    if (history_samples_to_process < history_len) {
        /* no further data to process in the input, just shift history down */
        memmove_elements(history, history + history_samples_to_process, history_available - history_samples_to_process);
        state->history_len = history_available - history_samples_to_process;
        return out_produced;
    }

    /* There is more data in the input buffer that we can process */
    const unsigned offset = history_samples_to_process - history_len;         /* offset in input of first unprocessed block */
    const unsigned main_blocks_to_process = ((in_length - offset) - context_len + 1) / IN_BLOCK_LEN;
    const unsigned main_samples_to_process = main_blocks_to_process * IN_BLOCK_LEN;

    /* process data from the input buffer directly */
    out_produced += BLOCK_FN(state, in + offset, main_samples_to_process, out + out_produced);

    /* preserve history starting from the first unprocessed block in input data */
    const unsigned input_consumed = offset + main_samples_to_process;
    memcpy_elements(history, in + input_consumed, in_length - input_consumed);
    state->history_len = in_length - input_consumed;

    return out_produced;
}
