#include "util.h"

TEST(lpf_settings_for, Success) {
    int max = 3900e3;
    lpf_settings lpf = lpf_settings_for(3000e3, &max);
    EXPECT_EQ(lpf.cutoff, (float) 3177e3);

    lpf = lpf_settings_for(2600e3, NULL);
    EXPECT_EQ(lpf.cutoff, (float) 2601e3);

    lpf = lpf_settings_for(11196e3, NULL);
    EXPECT_EQ(lpf.cutoff, (float) 11196e3);

    max = 6000e3;
    lpf = lpf_settings_for(6555e3, &max);
    EXPECT_EQ(lpf.cutoff, (float) 5920e3);
}

TEST(hpf_settings_for, Success) {
    hpf_settings s = hpf_settings_for(527e3);
    EXPECT_EQ(s.cutoff, 527e3);

    s = hpf_settings_for(2200e3);
    EXPECT_EQ(s.cutoff, 2138e3);

    s = hpf_settings_for(3724e3);
    EXPECT_EQ(s.cutoff, 3724e3);

    s = hpf_settings_for(4000e3);
    EXPECT_EQ(s.cutoff, 3724e3);
}

TEST(init_tuner_regs, Success) {
    lpcsdr_device_handle handle = {
        .registers_count = TUNER_REG_COUNT,
    };
    EXPECT_EQ(init_tuner_registers(&handle.registers), LPCSDR_SUCCESS);
}

TEST(Test_FOR_Dev, create_tuner_r5)
{
    Context ctx;
    DeviceHandle handle(ctx);

    handle()->registers_count = TUNER_REG_COUNT;
    EXPECT_EQ(init_tuner_registers(&handle()->registers), LPCSDR_SUCCESS);
    change_set *cs;

    create_change_set(&cs);

    set_tuner_value_in_change_set(handle(), cs, TunerR5, LNA_GAIN, 8);
    set_tuner_value_in_change_set(handle(), cs, TunerR5, LNA_GAIN_MODE, 0);
    set_tuner_value_in_change_set(handle(), cs, TunerR7, MIX_GAIN, 2);

    uint16_t first;
    uint8_t *payload;
    uint16_t payload_size;
    EXPECT_EQ(prepare_tuner_payload_from_change_set(cs, &first, &payload, &payload_size), LPCSDR_SUCCESS);

    EXPECT_EQ(lpcsdr_tuner_update(handle(), first, payload, payload_size), LPCSDR_SUCCESS);
}
