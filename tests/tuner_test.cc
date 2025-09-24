#include "util.h"

#include "tuner-regs.h"

using namespace testing;

TEST(lpf_settings_for, Success) {
    const lpf_settings *lpf = lpcsdr__lpf_settings_for(3000e3, 3900e3);
    ASSERT_EQ(lpf->cutoff, (float) 3177e3);

    lpf = lpcsdr__lpf_settings_for(2026e3, 3000e3);
    ASSERT_EQ(lpf->cutoff, (float) 2027e3);

    lpf = lpcsdr__lpf_settings_for(2600e3, 11196e3);
    ASSERT_EQ(lpf->cutoff, (float) 2601e3);

    lpf = lpcsdr__lpf_settings_for(11196e3, 11196e3);
    ASSERT_EQ(lpf->cutoff, (float) 11196e3);

    lpf = lpcsdr__lpf_settings_for(6555e3, 6000e3);
    ASSERT_EQ(lpf->cutoff, (float) 5920e3);

    lpf = lpcsdr__lpf_settings_for(3100e3, 4000e3);
    ASSERT_EQ(lpf->cutoff, (float) 3177e3);

    lpf = lpcsdr__lpf_settings_for(11190e3, 11196e3);
    ASSERT_EQ(lpf->cutoff, (float) 11196e3);
}

TEST(hpf_settings_for, Success) {
    const hpf_settings *s = lpcsdr__hpf_settings_for(527e3);
    ASSERT_EQ(s->cutoff, 527e3);

    s = lpcsdr__hpf_settings_for(520e3);
    ASSERT_EQ(s->cutoff, 527e3);

    s = lpcsdr__hpf_settings_for(2200e3);
    ASSERT_EQ(s->cutoff, 2138e3);

    s = lpcsdr__hpf_settings_for(3724e3);
    ASSERT_EQ(s->cutoff, 3724e3);

    s = lpcsdr__hpf_settings_for(3700e3);
    ASSERT_EQ(s->cutoff, 3563e3);

    s = lpcsdr__hpf_settings_for(4000e3);
    ASSERT_EQ(s->cutoff, 3724e3);
}

TEST(bitrange_sanity_test, Success) {
    uint8_t mask = 1;
    for (int i = 0 ; i < 8; i++) {
        ASSERT_EQ(bitrange(i, 0), mask);
        mask = (mask << 1) | 1;
    }
}

TEST(set_tuner_value_in_change_set, Success)
{
    change_set cs = {0};

    set_tuner_bits(&cs, LNA_GAIN, 8);
    set_tuner_bits(&cs, LNA_GAIN_MODE, 1);
    set_tuner_bits(&cs, MIX_GAIN, 2);

    EXPECT_EQ(cs.entries[LNA_GAIN_REG].current_value & LNA_GAIN_MASK, (8 << LNA_GAIN_OFFSET));
    EXPECT_EQ(cs.entries[LNA_GAIN_REG].current_mask & LNA_GAIN_MASK, LNA_GAIN_MASK);
    EXPECT_EQ(cs.entries[LNA_GAIN_MODE_REG].current_value & LNA_GAIN_MODE_MASK, (1 << LNA_GAIN_MODE_OFFSET));
    EXPECT_EQ(cs.entries[LNA_GAIN_MODE_REG].current_mask & LNA_GAIN_MODE_MASK, LNA_GAIN_MODE_MASK);
    EXPECT_EQ(cs.entries[MIX_GAIN_REG].current_value & MIX_GAIN_MASK, (2 << MIX_GAIN_OFFSET));
    EXPECT_EQ(cs.entries[MIX_GAIN_REG].current_mask & MIX_GAIN_MASK, MIX_GAIN_MASK);
}

TEST(prepare_tuner_payload_from_change_set, Success) {
    change_set cs = {0};

    set_tuner_bits(&cs, PW_LDO_A, 1); //17
    set_tuner_bits(&cs, PW_LDO_D, 2); //23
    set_tuner_bits(&cs, PWD_SDM, 1); //18
    set_tuner_bits(&cs, SEL_DIV, 1); //16
    set_tuner_bits(&cs, REF_DIV2, 1); //16
    set_tuner_bits(&cs, S_I2C, 1); //20
    set_tuner_bits(&cs, N_I2C, 1); //20
    set_tuner_bits(&cs, SDM_IN_LSB, 1); //21
    set_tuner_bits(&cs, SDM_IN_MSB, 1); //22
    set_tuner_bits(&cs, PLL_AUTO_CLK, 0); //26
    set_tuner_bits(&cs, VCO_CURRENT, 4); //18
    set_tuner_bits(&cs, VCO_MODE, 1);    //19
    set_tuner_bits(&cs, VCO_DAC, 1); //19

    uint16_t first;
    uint8_t payload[TUNER_REG_MAX_PAYLOAD_SIZE]= {};
    uint16_t payload_size;
    lpcsdr__prepare_tuner_payload_from_change_set(&cs, &first, payload, &payload_size);
    ASSERT_EQ(first, 16);
    ASSERT_EQ(payload_size, 22);

    for (unsigned i = 0; i < TUNER_REG_COUNT; i++) {
        ASSERT_EQ(cs.entries[i].current_mask, 0);
        ASSERT_EQ(cs.entries[i].current_value, 0);
    }
}

TEST(Test_find_pll_parameters, Success) {
    tuner_pll_config_t p = {};

    const double target = 100e6;

    ASSERT_EQ(lpcsdr__find_pll_parameters(target, 28.8e6, &p), LPCSDR_SUCCESS);

    // these are very prescriptive white-box tests, needed?
    EXPECT_EQ(p.refdiv, true);
    EXPECT_EQ(p.seldiv, 32);
    EXPECT_EQ(p.feedback_n, 111);
    EXPECT_EQ(p.feedback_sdm, 7282);

    // actual freq should be within 1ppm of requested freq
    ASSERT_NEAR(p.actual_frequency, target, target * 1e-6);
    // actual vco should be consistent with output freq & seldiv
    ASSERT_FLOAT_EQ(p.actual_vco / p.seldiv, p.actual_frequency);
}

TEST(tuner_sanity_check, Success)
{
    Context ctx;
    DeviceHandle handle(ctx);
    lpcsdr_device_handle *h = handle();

    tuner_pll_config_t p;
    ASSERT_EQ(lpcsdr__find_pll_parameters(100e6, 28800000, &p), LPCSDR_SUCCESS);
    ASSERT_EQ(lpcsdr__start_pll(h, &p), LPCSDR_SUCCESS);
    ASSERT_EQ(lpcsdr__has_pll_lock(h), 1);
}

TEST(gain_sanity_check, Success) {
    Context ctx;
    DeviceHandle handle(ctx);

    lpcsdr_device_handle *h = handle();
    ASSERT_EQ(lpcsdr_set_lna_gain(h, 1), LPCSDR_SUCCESS);
    ASSERT_EQ(lpcsdr_set_mix_gain(h, 2), LPCSDR_SUCCESS);
    ASSERT_EQ(lpcsdr_set_vga_gain(h, 3), LPCSDR_SUCCESS);

    EXPECT_EQ(read_tuner_bits(h, LNA_GAIN), 1);
    EXPECT_EQ(read_tuner_bits(h, MIX_GAIN), 2);
    EXPECT_EQ(read_tuner_bits(h, VGA_GAIN), 3);
}

TEST(filter_sanity_check, Success) {
    Context ctx;
    DeviceHandle handle(ctx);

    lpcsdr_device_handle *h = handle();

    auto lpf = lpcsdr__lpf_settings_for(3177e3, 0);
    ASSERT_NE(lpf, nullptr);
    EXPECT_GE(lpf->cutoff, 3177e3);

    auto hpf = lpcsdr__hpf_settings_for(659e3);
    ASSERT_NE(hpf, nullptr);
    EXPECT_LE(hpf->cutoff, 659e3);

    EXPECT_EQ(lpcsdr__tuner_set_bandpass(h, hpf, lpf), LPCSDR_SUCCESS);

    // Check High-Pass HPF_CORNER set to 14
    EXPECT_EQ(read_tuner_bits(h, IFFILT_HPF_CORNER), hpf->hpf_corner);

    // Check TunerR11 Low-Pass
    EXPECT_EQ(read_tuner_bits(h, IFFILT_Q), lpf->lpf_q);
    EXPECT_EQ(read_tuner_bits(h, IFFILT_FINE_LPF), lpf->lpf_fine);
    EXPECT_EQ(read_tuner_bits(h, IFFILT_NARROW), lpf->lpf_narrow);
    EXPECT_EQ(read_tuner_bits(h, IFFILT_COARSE_LPF), lpf->lpf_coarse);
}

typedef double PLLRangeTestParam;
class Test_find_pll_parameters_for_range : public testing::TestWithParam<PLLRangeTestParam> {};

TEST_P(Test_find_pll_parameters_for_range, FindParameters) {

    double target = GetParam();
    tuner_pll_config_t p = {};
    ASSERT_EQ(lpcsdr__find_pll_parameters(target, 28.8e6, &p), LPCSDR_SUCCESS);

    ASSERT_EQ(p.refdiv, true);
    ASSERT_NEAR(p.actual_frequency, target, target * 1e-6);
    ASSERT_LT(p.seldiv, 64);
    ASSERT_GE(p.actual_vco, 1750e6);
    ASSERT_LE(p.actual_vco, 3700e6);
    ASSERT_FLOAT_EQ(p.actual_vco / p.seldiv, p.actual_frequency);
}

INSTANTIATE_TEST_SUITE_P(, Test_find_pll_parameters_for_range, Range(55e6, 1850e6, 10e6));
