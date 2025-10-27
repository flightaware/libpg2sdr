#include "util.h"

#include "tuner-regs.h"

using namespace testing;

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
    pg2sdr__prepare_tuner_payload_from_change_set(&cs, &first, payload, &payload_size);
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

    ASSERT_EQ(pg2sdr__find_pll_parameters(target, 28.8e6, &p), PG2SDR_SUCCESS);

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
    pg2sdr_device_handle *h = handle();

    tuner_pll_config_t p;
    ASSERT_EQ(pg2sdr__find_pll_parameters(100e6, 28800000, &p), PG2SDR_SUCCESS);
    ASSERT_EQ(pg2sdr__start_pll(h, &p), PG2SDR_SUCCESS);
    ASSERT_EQ(pg2sdr__has_pll_lock(h), 1);
}

TEST(gain_sanity_check, Success) {
    Context ctx;
    DeviceHandle handle(ctx);

    pg2sdr_device_handle *h = handle();
    ASSERT_EQ(pg2sdr_set_lna_gain(h, 1), PG2SDR_SUCCESS);
    ASSERT_EQ(pg2sdr_set_mix_gain(h, 2), PG2SDR_SUCCESS);
    ASSERT_EQ(pg2sdr_set_vga_gain(h, 3), PG2SDR_SUCCESS);

    EXPECT_EQ(read_tuner_bits(h, LNA_GAIN), 1);
    EXPECT_EQ(read_tuner_bits(h, MIX_GAIN), 2);
    EXPECT_EQ(read_tuner_bits(h, VGA_GAIN), 3);
}

TEST(filter_sanity_check, Success) {
    Context ctx;
    DeviceHandle handle(ctx);

    pg2sdr_device_handle *h = handle();

    auto settings = pg2sdr__select_bandpass_filter(h, 659e3, 3177e3, 0, 4000e3);
    ASSERT_NE(settings, nullptr);
    EXPECT_LE(settings->lower_corner, 659e3);
    EXPECT_GE(settings->upper_corner, 3177e3);
    EXPECT_GE(settings->lower_corner, 0);
    EXPECT_LE(settings->upper_corner, 4000e3);

    EXPECT_EQ(pg2sdr__tuner_set_bandpass(h, settings), PG2SDR_SUCCESS);

    // verify correct settings were written
    EXPECT_EQ(read_tuner_bits(h, IFFILT_HPF_CORNER), settings->hpf_corner);
    EXPECT_EQ(read_tuner_bits(h, IFFILT_Q), settings->lpf_q);
    EXPECT_EQ(read_tuner_bits(h, IFFILT_FINE_LPF), settings->lpf_fine);
    EXPECT_EQ(read_tuner_bits(h, IFFILT_NARROW), settings->lpf_narrow);
    EXPECT_EQ(read_tuner_bits(h, IFFILT_COARSE_LPF), settings->lpf_coarse);
}

typedef double PLLRangeTestParam;
class Test_find_pll_parameters_for_range : public testing::TestWithParam<PLLRangeTestParam> {};

TEST_P(Test_find_pll_parameters_for_range, FindParameters) {

    double target = GetParam();
    tuner_pll_config_t p = {};
    ASSERT_EQ(pg2sdr__find_pll_parameters(target, 28.8e6, &p), PG2SDR_SUCCESS);

    ASSERT_EQ(p.refdiv, true);
    ASSERT_NEAR(p.actual_frequency, target, target * 1e-6);
    ASSERT_LT(p.seldiv, 64);
    ASSERT_GE(p.actual_vco, 1750e6);
    ASSERT_LE(p.actual_vco, 3700e6);
    ASSERT_FLOAT_EQ(p.actual_vco / p.seldiv, p.actual_frequency);
}

INSTANTIATE_TEST_SUITE_P(, Test_find_pll_parameters_for_range, Range(55e6, 1850e6, 10e6));
