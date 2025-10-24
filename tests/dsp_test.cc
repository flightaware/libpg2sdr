#include <gtest/gtest.h>
using namespace testing;

#include "dsp.h"

TEST(DSPTests, DataTypes)
{
    static_assert(alignof(cs16_t) == alignof(int16_t));
    static_assert(sizeof(cs16_t) == 2 * alignof(int16_t));
    static_assert(offsetof(cs16_t, i) == 0);
    static_assert(offsetof(cs16_t, q) == sizeof(int16_t));
}

TEST(DSPDecimateTests, ImpulseResponse)
{
    // Feed an impulse to the decimator and check that the impulse response
    // matches the configured taps. Since there are two possible options for
    // which output sample gets discarded during decimation, put an impulse
    // in the I and Q parts of the input at a 1-sample offset.

    size_t in_len = lpcsdr__dsp_default_halfband_ntaps;
    if (in_len % 2 == 1)
        ++in_len;

    cs16_t *in = new cs16_t[in_len];
    for (auto i = 0; i < in_len; ++i) {
        in[i].i = 0;
        in[i].q = 0;
    }

    in[0].i = 32767;
    in[1].q = 32767;

    size_t out_len = in_len / 2;
    cs16_t *out = new cs16_t[out_len + 2];

    dsp_halfband_decimate_state_t *state = lpcsdr__dsp_halfband_decimate_create(lpcsdr__dsp_default_halfband_ntaps, lpcsdr__dsp_default_halfband_taps);
    ASSERT_NE(state, nullptr);
    
    auto produced = lpcsdr__dsp_halfband_decimate_process(state, in, in_len, out);
    ASSERT_EQ(produced, out_len);
    auto center_tap_offset = state->ntaps/2;
    auto center_tap_value = state->taps[center_tap_offset];

    EXPECT_NEAR(out[center_tap_offset/2 + 1].q, ((int32_t)in[1].q * center_tap_value) >> 15, 1);
    
    for (unsigned i = 0; i < state->ntaps/2; ++i) {
        int16_t tap_value = state->taps[state->ntaps - i*2 - 1];
        EXPECT_NEAR(out[i].i, ((int32_t)in[0].i * tap_value) >> 15, 1);
    }

    lpcsdr__dsp_halfband_decimate_free(state);
    delete[] out;
    delete[] in;
}

static cs16_t * make_random_input(uint32_t len)    
{
    // return random input with input components between -16384..+16383
    // (i.e. we can add two of them without overflowing an int16_t)
    
    cs16_t *out = new cs16_t[len];
    for (auto i = 0; i < len; ++i) {
        out[i].i = (int16_t) (rand() % 32768) - 16384;
        out[i].q = (int16_t) (rand() % 32768) - 16384;
    }
    return out;
}

static std::pair<cs16_t *,uint32_t> decimate(dsp_halfband_decimate_state_t *state, cs16_t *in, uint32_t in_len)
{
    uint32_t out_len = in_len/2;
    cs16_t *out = new cs16_t[out_len];
    lpcsdr__dsp_halfband_decimate_reset(state);
    uint32_t produced = lpcsdr__dsp_halfband_decimate_process(state, in, in_len, out);
    assert(produced <= out_len);
    return {out,produced};
}
    

TEST(DSPDecimateTests, LinearResponse)
{
    // Generate two random inputs, and compute the decimator output for each.
    // i.e.
    //
    //    out_1 = decimate(random_in_1)
    //    out_2 = decimate(random_in_2)
    //
    // Then verify that
    //
    //    out_1 + out_2 = decimate(random_in_1 + random_in_2)
    //
    // which is what we expect for a linear time-invariant system

    srand(1);
    
    size_t in_len = 1024;
    cs16_t *in_1 = make_random_input(in_len);
    cs16_t *in_2 = make_random_input(in_len);

    cs16_t *in_sum = new cs16_t[in_len];
    for (auto i = 0; i < in_len; ++i) {
        in_sum[i].i = in_1[i].i + in_2[i].i;
        in_sum[i].q = in_1[i].q + in_2[i].q;
    }

    dsp_halfband_decimate_state_t *state = lpcsdr__dsp_halfband_decimate_create(lpcsdr__dsp_default_halfband_ntaps, lpcsdr__dsp_default_halfband_taps);
    ASSERT_NE(state, nullptr);

    auto out_1 = decimate(state, in_1, in_len);
    auto out_2 = decimate(state, in_2, in_len);
    auto out_sum = decimate(state, in_sum, in_len);

    // all outputs should have the same length
    ASSERT_EQ(out_1.second, out_2.second);
    ASSERT_EQ(out_1.second, out_sum.second);

    for (auto i = 0; i < out_1.second; ++i) {
        EXPECT_NEAR(out_sum.first[i].i, out_1.first[i].i + out_2.first[i].i, 1) << "at index " << i;
        EXPECT_NEAR(out_sum.first[i].q, out_1.first[i].q + out_2.first[i].q, 1) << "at index " << i;
    }

    lpcsdr__dsp_halfband_decimate_free(state);
    delete[] in_1;
    delete[] in_2;
    delete[] in_sum;
    delete[] out_1.first;
    delete[] out_2.first;
    delete[] out_sum.first;
}

TEST(DSPDecimateTests, AvoidOverflow)
{
    // Build an input vector that, when multiplied by the decimator taps, produces the largest possible output.
    // Verify that the output doesn't overflow an int16.

    dsp_halfband_decimate_state_t *state = lpcsdr__dsp_halfband_decimate_create(lpcsdr__dsp_default_halfband_ntaps, lpcsdr__dsp_default_halfband_taps);
    ASSERT_NE(state, nullptr);

    size_t in_len = state->ntaps;
    if (in_len % 2 == 1)
        ++in_len; // Pad to even length

    cs16_t *in = new cs16_t[in_len];
    for (auto i = 0; i < state->ntaps; ++i) {
        if (i >= state->ntaps) {
            in[i].i = 0;
            in[i].q = 0;
        } else if (state->taps[i] < 0) {
            in[i].i = -32768;
            in[i].q = -32768;
        } else if (state->taps[i] > 0) {
            in[i].i = 32767;
            in[i].q = 32767;
        } else {
            in[i].i = 0;
            in[i].q = 0;
        }
    }

    size_t out_len = in_len / 2;
    cs16_t *out = new cs16_t[out_len];

    uint32_t produced = lpcsdr__dsp_halfband_decimate_process(state, in, in_len, out);
    ASSERT_EQ(produced, out_len);

    // Look at the final produced output, which should correspond to when our input
    // is exactly lined up with the full set of taps.
    
    // We expect the result to be near the max possible value without overflowing.
    // On overflow, the sign will flip.
    EXPECT_NEAR(out[out_len-1].i, 32767, 100);
    EXPECT_NEAR(out[out_len-1].q, 32767, 100);
    
    delete[] out;
    delete[] in;
    lpcsdr__dsp_halfband_decimate_free(state);
}

static std::pair<cs16_t *,uint32_t> decimate_in_chunks(dsp_halfband_decimate_state_t *state, cs16_t *in, uint32_t in_len, uint32_t chunk_len)
{
    uint32_t out_len = in_len/2;
    cs16_t *out = new cs16_t[out_len];
    lpcsdr__dsp_halfband_decimate_reset(state);

    uint32_t produced = 0;
    for (uint32_t i = 0; i < in_len; i += chunk_len) {
        produced += lpcsdr__dsp_halfband_decimate_process(state, in + i, std::min(in_len - i, chunk_len), out + produced);
        assert(produced <= out_len);
    }

    return {out,produced};
}

TEST(DSPDecimateTests, ChunkProcessing)
{
    // Build large random input, and process it through the decimator in a single call.
    // Then try the same input, but process it in many smaller calls.
    // Verify that the outputs are the same.

    srand(1);

    uint32_t in_len = 65536;    
    cs16_t *in = make_random_input(in_len);

    dsp_halfband_decimate_state_t *state = lpcsdr__dsp_halfband_decimate_create(lpcsdr__dsp_default_halfband_ntaps, lpcsdr__dsp_default_halfband_taps);
    ASSERT_NE(state, nullptr);

    auto out_1 = decimate(state, in, in_len);
    auto out_2 = decimate_in_chunks(state, in, in_len, 20);
    auto out_3 = decimate_in_chunks(state, in, in_len, 200);

    ASSERT_EQ(out_1.second, out_2.second);
    ASSERT_EQ(out_1.second, out_3.second);

    for (auto i = 0; i < out_1.second; ++i) {
        EXPECT_EQ(out_1.first[i].i, out_2.first[i].i) << "at index " << i;
        EXPECT_EQ(out_1.first[i].q, out_2.first[i].q) << "at index " << i;
        EXPECT_EQ(out_1.first[i].i, out_3.first[i].i) << "at index " << i;
        EXPECT_EQ(out_1.first[i].q, out_3.first[i].q) << "at index " << i;
    }

    delete[] in;
    delete[] out_1.first;
    delete[] out_2.first;
    delete[] out_3.first;
}
