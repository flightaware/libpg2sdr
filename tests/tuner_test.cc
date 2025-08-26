#include "util.h"

TEST(lpf_settings_for, Success) {
    int max = 3900e3;
    lpf_settings lpf = lpf_settings_for(3000e3, &max);
    ASSERT_EQ(lpf.cutoff, (float) 3177e3);

    lpf = lpf_settings_for(2600e3, NULL);
    ASSERT_EQ(lpf.cutoff, (float) 2601e3);

    lpf = lpf_settings_for(11196e3, NULL);
    ASSERT_EQ(lpf.cutoff, (float) 11196e3);

    max = 6000e3;
    lpf = lpf_settings_for(6555e3, &max);
    ASSERT_EQ(lpf.cutoff, (float) 5920e3);
}

TEST(hpf_settings_for, Success) {
    hpf_settings s = hpf_settings_for(527e3);
    ASSERT_EQ(s.cutoff, 527e3);

    s = hpf_settings_for(2200e3);
    ASSERT_EQ(s.cutoff, 2138e3);

    s = hpf_settings_for(3724e3);
    ASSERT_EQ(s.cutoff, 3724e3);

    s = hpf_settings_for(4000e3);
    ASSERT_EQ(s.cutoff, 3724e3);
}

TEST(bitrange_sanity_test, Success) {
    uint8_t mask = 1;
    for (int i = 0 ; i < 8; i++) {
        ASSERT_EQ(bitrange(i, 0), mask);
        mask = (mask << 1) | 1;
    }
}

TEST(set_tuner_value_in_change_set, Success) {
    change_set cs = {};

    set_tuner_reg(&cs, LNA_GAIN, 8);
    set_tuner_reg(&cs, LNA_GAIN_MODE, 1);
    set_tuner_reg(&cs, MIX_GAIN, 2);

    ASSERT_EQ(cs.entries[TunerR5].current_value, 0b11000);
    ASSERT_EQ(cs.entries[TunerR5].current_mask, LNA_GAIN_MASK | LNA_GAIN_MODE_MASK);
    ASSERT_EQ(cs.entries[TunerR7].current_value, 2);
    ASSERT_EQ(cs.entries[TunerR7].current_mask, MIX_GAIN_MASK);
}

TEST(Test_find_pll_parameters, Success) {
    pll_parameters p = {};

    ASSERT_EQ(find_pll_parameters(100e6, 28800000, &p), LPCSDR_SUCCESS);
    ASSERT_EQ(p.refdiv, true);
    ASSERT_EQ(p.seldiv, 32);
    ASSERT_EQ(p.feedback_n, 111);
    ASSERT_EQ(p.feedback_sdm, 7281);
    ASSERT_LT(abs(p.vco - 3199999768.0664062), .001);
    ASSERT_LT(abs(p.freq - 99999992.7520752), .001);
}

TEST(Test_FOR_Dev, create_tuner_r5)
{
    Context ctx;
    DeviceHandle handle(ctx);

    change_set cs = {};
    set_tuner_reg(&cs, LNA_GAIN, 8);
    set_tuner_reg(&cs, LNA_GAIN_MODE, 0);
    set_tuner_reg(&cs, MIX_GAIN, 2);

}
