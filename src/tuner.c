#include <pthread.h>
#include <math.h>

#include "internal.h"
#include "tuner-regs.h"

static int apply_tuner_changeset(pg2sdr_device *dev, change_set *cs)
{
    uint16_t first;
    uint8_t payload[TUNER_REG_MAX_PAYLOAD_SIZE] = {0};
    uint16_t payload_size;

    pg2sdr__prepare_tuner_payload_from_change_set(cs, &first, payload, &payload_size);
    if (!payload_size)
        return PG2SDR_SUCCESS; /* no work to do */

    return pg2sdr__ctrl_tuner_update(dev, first, payload, payload_size);
}

int pg2sdr__read_tuner_bits(pg2sdr_device *dev, uint8_t reg, uint8_t mask, unsigned offset)
{
    int error;
    uint8_t value;
    if ((error = pg2sdr__ctrl_read_tuner_register(dev, reg, CACHE_NORMAL, &value, sizeof(value))) < 0)
        return error;
    return (value & mask) >> offset;
}

int pg2sdr__tuner_set_lna(pg2sdr_device *dev, unsigned lna)
{
    if (lna > 15)
        return PG2SDR_ERROR_BAD_ARGUMENT;
    return pg2sdr__tuner_set_gains(dev, lna, -1, -1);
}

int pg2sdr__tuner_set_mix(pg2sdr_device *dev, unsigned mix)
{
    if (mix > 15)
        return PG2SDR_ERROR_BAD_ARGUMENT;
    return pg2sdr__tuner_set_gains(dev, -1, mix, -1);
}

int pg2sdr__tuner_set_vga(pg2sdr_device *dev, unsigned vga)
{
    if (vga > 15)
        return PG2SDR_ERROR_BAD_ARGUMENT;
    return pg2sdr__tuner_set_gains(dev, -1, -1, vga);
}

int pg2sdr__tuner_set_gains(pg2sdr_device *dev, int lna, int mix, int vga)
{
    assert (lna <= 15);
    assert (mix <= 15);
    assert (vga <= 15);

    change_set cs = {0};
    if (lna >= 0 && lna != dev->lna_gain)
        set_tuner_bits(&cs, LNA_GAIN, lna);
    if (mix >= 0 && mix != dev->mix_gain)
        set_tuner_bits(&cs, MIX_GAIN, mix);
    if (vga >= 0 && vga != dev->vga_gain)
        set_tuner_bits(&cs, VGA_GAIN, vga);

    int error;
    if ((error = apply_tuner_changeset(dev, &cs)) < 0)
        return error;

    if (lna >= 0)
        dev->lna_gain = lna;
    if (mix >= 0)
        dev->mix_gain = mix;
    if (vga >= 0)
        dev->vga_gain = vga;

    return PG2SDR_SUCCESS;
}


int pg2sdr__tuner_set_bandpass(pg2sdr_device *dev, const pg2sdr_bandpass_table_t *settings)
{
    assert(settings != NULL);

    change_set cs = {0};
    set_tuner_bits(&cs, IFFILT_HPF_CORNER, settings->hpf_corner);
    set_tuner_bits(&cs, IFFILT_Q, settings->lpf_q);
    set_tuner_bits(&cs, IFFILT_NARROW, settings->lpf_narrow);
    set_tuner_bits(&cs, IFFILT_FINE_LPF, settings->lpf_fine);
    set_tuner_bits(&cs, IFFILT_COARSE_LPF, settings->lpf_coarse);
    return apply_tuner_changeset(dev, &cs);
}

int pg2sdr__init_tuner(pg2sdr_device *dev)
{
    int error = PG2SDR_SUCCESS;

    if ((error = pg2sdr__ctrl_set_rf_power(dev, RF_POWER_RESET)) < 0)
        return error;

    int tuner_id = read_tuner_bits(dev, TUNER_ID);
    if (tuner_id < 0)    /* read failed */
        return tuner_id;
    if (tuner_id != 0x96)
        return PG2SDR_ERROR_TUNER_DETECT;

    /* Populate fixed initial register values.
     *
     * We express all the fields separately for readability, but
     * the compiler has enough visibility into the operation of
     * set_tuner_bits that it can optimize this down to direct
     * initialization of the changeset elements with constant
     * values.
     */
    change_set cs = {0};

    // TunerR5
    set_tuner_bits(&cs, PWD_LT, 1);
    set_tuner_bits(&cs, R5_RESERVED_6_0, 0);
    set_tuner_bits(&cs, PWD_LNA1, 0);
    set_tuner_bits(&cs, LNA_GAIN_MODE, 1);
    set_tuner_bits(&cs, LNA_GAIN, 0); dev->lna_gain = 0;

    // TunerR6
    set_tuner_bits(&cs, PWD_PDET1, 0);
    set_tuner_bits(&cs, PWD_PDET2, 0);
    set_tuner_bits(&cs, FILT_3DB, 1);
    set_tuner_bits(&cs, R6_RESERVED_4_1, 1);
    set_tuner_bits(&cs, R6_RESERVED_3_0, 0);
    set_tuner_bits(&cs, PW_LNA, 2);

    // TunerR7
    set_tuner_bits(&cs, IMG_R, 0);
    set_tuner_bits(&cs, PW_MIX, 1);
    set_tuner_bits(&cs, PW0_MIX, 1);
    set_tuner_bits(&cs, MIXGAIN_MODE, 0);
    set_tuner_bits(&cs, MIX_GAIN, 0); dev->mix_gain = 0;

    // TunerR8
    set_tuner_bits(&cs, PW_AMP, 1);
    set_tuner_bits(&cs, PW0_AMP, 1);
    set_tuner_bits(&cs, IMR_G_PATH, 0);
    set_tuner_bits(&cs, IMR_G, 0);

    // TunerR9
    set_tuner_bits(&cs, PWD_IFFILT, 0);
    set_tuner_bits(&cs, PW1_IFFILT, 1);
    set_tuner_bits(&cs, IMR_P_PATH, 0);
    set_tuner_bits(&cs, IMR_P, 0);

    // TunerR10
    set_tuner_bits(&cs, PW_FILT, 1);
    set_tuner_bits(&cs, FILTER_CUR, 3);
    set_tuner_bits(&cs, IFFILT_Q, 0);
    set_tuner_bits(&cs, IFFILT_FINE_LPF, 0);

    // TunerR11
    set_tuner_bits(&cs, IFFILT_NARROW, 0);
    set_tuner_bits(&cs, IFFILT_COARSE_LPF, 0);
    set_tuner_bits(&cs, CALIBRATION_TRIGGER, 0);
    set_tuner_bits(&cs, IFFILT_HPF_CORNER, 15);

    // TunerR12
    set_tuner_bits(&cs, PWD_ADC, 1);
    set_tuner_bits(&cs, PW_VGA, 1);
    set_tuner_bits(&cs, R12_RESERVED_5_1, 1);
    set_tuner_bits(&cs, VGA_GAIN_MODE, 0);
    set_tuner_bits(&cs, VGA_GAIN, 0); dev->vga_gain = 0;

    // TunerR13
    set_tuner_bits(&cs, LNA_VTH_H, 5);
    set_tuner_bits(&cs, LNA_VTH_L, 3);

    // TunerR14
    set_tuner_bits(&cs, MIX_VTH_H, 7);
    set_tuner_bits(&cs, MIX_VTH_L, 5);

    // TunerR15
    set_tuner_bits(&cs, FLT_EXT_WIDEST, 0);
    set_tuner_bits(&cs, R15_RESERVED_6_0, 0);
    set_tuner_bits(&cs, R15_RESERVED_5_1, 1);
    set_tuner_bits(&cs, CLK_OUT_DIS, 1);
    set_tuner_bits(&cs, RING_DISABLE, 1);
    set_tuner_bits(&cs, R15_RESERVED_2_0, 0);
    set_tuner_bits(&cs, CLK_AGC_DIS, 0);
    set_tuner_bits(&cs, R15_RESERVED_0_0, 0);

    // TunerR16
    set_tuner_bits(&cs, SEL_DIV, 0);
    set_tuner_bits(&cs, REF_DIV2, 1);
    set_tuner_bits(&cs, XTAL_DRIVE, 0);
    set_tuner_bits(&cs, DET1_CAP, 1);
    set_tuner_bits(&cs, CAPX, 0);

    // TunerR17
    set_tuner_bits(&cs, PW_LDO_A, 0);
    set_tuner_bits(&cs, CP_CURRENT, 0);
    set_tuner_bits(&cs, R17_RESERVED_2_0, 0);
    set_tuner_bits(&cs, R17_RESERVED_1_0, 0);
    set_tuner_bits(&cs, R17_RESERVED_0_0, 0);

    // TunerR18
    set_tuner_bits(&cs, VCO_CURRENT, 4);
    set_tuner_bits(&cs, SDM_DITHER_DIS, 0);
    set_tuner_bits(&cs, PWD_SDM, 1);
    set_tuner_bits(&cs, R18_RESERVED_2_0, 0);
    set_tuner_bits(&cs, R18_RESERVED_1_0, 0);
    set_tuner_bits(&cs, R18_RESERVED_0_0, 0);

    // TunerR19
    set_tuner_bits(&cs, R18_RESERVED_7_0, 0);
    set_tuner_bits(&cs, VCO_MODE, 0);
    set_tuner_bits(&cs, VCO_DAC, 0);

    // TunerR20
    set_tuner_bits(&cs, S_I2C, 0);
    set_tuner_bits(&cs, N_I2C, 0);

    // TunerR21
    set_tuner_bits(&cs, SDM_IN_LSB, 0);

    // TunerR22
    set_tuner_bits(&cs, SDM_IN_MSB, 0);

    // TunerR23
    set_tuner_bits(&cs, PW_LDO_D, 0);
    set_tuner_bits(&cs, DIV_BUF_CUR, 3);
    set_tuner_bits(&cs, OPEN_D, 0);
    set_tuner_bits(&cs, R23_RESERVED_2_1, 1);
    set_tuner_bits(&cs, R23_RESERVED_1_0, 0);
    set_tuner_bits(&cs, R23_RESERVED_0_0, 0);

    // TunerR24
    set_tuner_bits(&cs, R24_RESERVED_7_0, 0);
    set_tuner_bits(&cs, R24_RESERVED_6_1, 1);
    set_tuner_bits(&cs, RING_SE23, 0);
    set_tuner_bits(&cs, PW_RING, 0);
    set_tuner_bits(&cs, RING_N, 0);

    // TunerR25
    set_tuner_bits(&cs, PW_RFFILT, 1);
    set_tuner_bits(&cs, RFFILT_CURRENT, 2);
    set_tuner_bits(&cs, SW_AGC, 0);
    set_tuner_bits(&cs, R25_RESERVED_3_1, 1);
    set_tuner_bits(&cs, R25_RESERVED_2_1, 1);
    set_tuner_bits(&cs, RING_SELDIV, 0);

    // TunerR26
    set_tuner_bits(&cs, RFMUX, 1);
    set_tuner_bits(&cs, AGC_CLOCK, 2);
    set_tuner_bits(&cs, PLL_AUTO_CLK, 0);
    set_tuner_bits(&cs, RFFILT, 0);

    // TunerR27
    set_tuner_bits(&cs, TF_NCH, 0);
    set_tuner_bits(&cs, TF_LP, 0);

    // TunerR28
    set_tuner_bits(&cs, PDET3_GAIN, 5);
    set_tuner_bits(&cs, R28_RESERVED_3_0, 0);
    set_tuner_bits(&cs, DISCHARGE_MODE, 1);
    set_tuner_bits(&cs, RF_SOURCE, 0);
    set_tuner_bits(&cs, R28_RESERVED_0_0, 0);

    // TunerR29
    set_tuner_bits(&cs, DETECT_BW, 2);
    set_tuner_bits(&cs, PDET1_GAIN, 4);
    set_tuner_bits(&cs, PDET2_GAIN, 6);

    // TunerR30
    set_tuner_bits(&cs, SW_PDET, 0);
    set_tuner_bits(&cs, FILTER_EXT, 0);
    set_tuner_bits(&cs, PDET_CLK, 0x0A);

    // TunerR31
    set_tuner_bits(&cs, LT_ATT, 1);
    set_tuner_bits(&cs, R31_RESERVED_6_1, 1);
    set_tuner_bits(&cs, R31_RESERVED_5_0, 0);
    set_tuner_bits(&cs, R31_RESERVED_4_0, 0);
    set_tuner_bits(&cs, R31_RESERVED_3_0, 0);
    set_tuner_bits(&cs, R31_RESERVED_2_0, 0);
    set_tuner_bits(&cs, RING_ATT, 0);

    return apply_tuner_changeset(dev, &cs);
}

int pg2sdr__find_pll_parameters(double requested, double xtal, tuner_pll_config_t *out) {
    const double VCO_MIN = 1750e6;
    /* const double VCO_MAX = 3700e6; */

    bool refdiv = (xtal > 24e6);
    double pll_ref = refdiv ? xtal/2 : xtal;

    double seldiv = 2;
    while ((requested * seldiv < VCO_MIN) && (seldiv < 64))
        seldiv *= 2;

    double required_vco = requested * seldiv;

    /* required_vco might actually be out of range here (<VCO_MIN or >VCO_MAX)
     * but continue anyway; maybe it can lock slightly out of range. We'll report
     * a PLL lock failure later if necessary.
     */

    double pll_feedback = (required_vco / 2) / pll_ref;
    if (pll_feedback < 13 || pll_feedback >= 269) {
        return PG2SDR_ERROR_TUNER_PLL_RANGE;
    }

    double pll_feedback_int_part;
    double pll_feedback_frac = modf(pll_feedback, &pll_feedback_int_part);

    unsigned pll_feedback_int = (unsigned) pll_feedback_int_part;
    unsigned sdm_numerator = (unsigned) (pll_feedback_frac * (1<<18) + 1);
    sdm_numerator = (sdm_numerator & ~3) | 1;

    if (sdm_numerator < 8)
        sdm_numerator = 0;
    else if (sdm_numerator > ((1<<18) - 8)) {
        sdm_numerator = 0;
        pll_feedback_int += 1;
    } else if (sdm_numerator > ((1<<17) - 8) && sdm_numerator <= (1<<17)) {
        sdm_numerator = (1<<17) - 8;
    } else if (sdm_numerator > (1<<17) && sdm_numerator < ((1<<17) + 8)) {
        sdm_numerator = (1<<17) + 8;
    }

    double actual_vco = pll_ref * 2 * (pll_feedback_int + (double) sdm_numerator / (1<<18));
    double actual_out = actual_vco / seldiv;

    out->valid = true;
    out->refdiv = refdiv;
    out->seldiv = seldiv;
    out->feedback_n = pll_feedback_int;
    out->feedback_sdm = sdm_numerator >> 2;
    out->actual_vco = actual_vco;
    out->actual_frequency = actual_out;

    return PG2SDR_SUCCESS;
}

int pg2sdr__start_pll(pg2sdr_device *dev, tuner_pll_config_t *params) {
    int error = PG2SDR_SUCCESS;
    int resp = 0;

    if ((error = pg2sdr__configure_pll_settings(dev, params)) < 0)
        return error;

    uint16_t vco_currents[5] = {4, 3, 2, 1, 0};

    for (unsigned i = 0; i < sizeof(vco_currents)/sizeof(vco_currents[0]); i++) {
        resp = pg2sdr__ctrl_update_tuner_lock(dev, vco_currents[i], 50);

        if (resp < 0)
            return resp;
        else if (resp)
            break;
    }
    if (resp != 1)
        return PG2SDR_ERROR_TUNER_PLL_LOCK;

    change_set cs = {0};
    set_tuner_bits(&cs, PLL_AUTO_CLK, 2);
    return apply_tuner_changeset(dev, &cs);
}

int pg2sdr__configure_pll_settings(pg2sdr_device *dev, tuner_pll_config_t *params) {
    LOGDEBUG(dev, "configuring PLL with:\n"
             "  SDM:    %u\n"
             "  SELDIV: %u\n"
             "  REFDIV: %u\n"
             "  N:      %u\n"
             "  VCO:    %.3fMHz\n"
             "  PLL:    %.3fMHz",
             params->feedback_sdm,
             params->seldiv,
             params->refdiv ? 2 : 1,
             params->feedback_n,
             params->actual_vco / 1e6,
             params->actual_frequency / 1e6);

    uint16_t sdm_disable = ( params->feedback_sdm == 0 ) ? 1 : 0;
    uint8_t sdm_lsb = params->feedback_sdm & 0xff;
    uint8_t sdm_msb = (params->feedback_sdm >> 8) & 0xff;

    uint8_t ni2c = (params->feedback_n - 13) / 4;
    uint8_t si2c = (params->feedback_n - 13) & 3;

    uint8_t seldiv_lut[6] = {2, 4, 8, 16, 32, 64};
    uint8_t seldiv = 255;
    for (unsigned i = 0; i < sizeof(seldiv_lut)/ sizeof(seldiv_lut[0]); i++)
        if (seldiv_lut[i] == params->seldiv)
            seldiv = i;
    assert (seldiv != 255);

    uint8_t refdiv = params->refdiv ? 1 : 0;

    uint8_t vco_dac = round(params->actual_vco * 0.0318e-6 - 49.0);
    vco_dac = MAX(0, vco_dac);
    vco_dac = MIN(63, vco_dac);

    change_set cs = {0};

    set_tuner_bits(&cs, IMG_R, dev->upper_sideband ? 1 : 0);
    set_tuner_bits(&cs, PW_LDO_A, 1);
    set_tuner_bits(&cs, PW_LDO_D, 2);
    set_tuner_bits(&cs, PWD_SDM, sdm_disable);
    set_tuner_bits(&cs, SEL_DIV, seldiv);
    set_tuner_bits(&cs, REF_DIV2, refdiv);
    set_tuner_bits(&cs, S_I2C, si2c);
    set_tuner_bits(&cs, N_I2C, ni2c);
    set_tuner_bits(&cs, SDM_IN_LSB, sdm_lsb);
    set_tuner_bits(&cs, SDM_IN_MSB, sdm_msb);
    set_tuner_bits(&cs, PLL_AUTO_CLK, 0);
    set_tuner_bits(&cs, VCO_CURRENT, 4);
    set_tuner_bits(&cs, VCO_MODE, 1);
    set_tuner_bits(&cs, VCO_DAC, vco_dac);

    return apply_tuner_changeset(dev, &cs);
}

int pg2sdr__has_pll_lock(pg2sdr_device *dev) {
    int value;
    if ((value = read_tuner_bits(dev, PLL_LOCK)) < 0)
        return value;

    return (value != 0);
}


/*
    Create a payload of bytes from the changeset.
 */
void pg2sdr__prepare_tuner_payload_from_change_set(change_set *cs, uint16_t *first, uint8_t *out, uint16_t *out_size)
{
    uint16_t num_entries = 0;
    uint16_t first_entry = TUNER_REG_COUNT;
    uint16_t last_entry = 5;

    for (uint16_t i = 5; i < TUNER_REG_COUNT; i++) {
        if (cs->entries[i].current_mask != 0) {
            num_entries++;
            if (first_entry > i)
                first_entry = i;
            if (last_entry < i)
                last_entry = i;
        }
    }

    if (!num_entries) {
        *out_size = 0;
        return;
    }

    unsigned count = last_entry - first_entry + 1;

    for (int i = first_entry; i <= last_entry; i++) {
        /* Read and clear value + mask */
        unsigned offset = i - first_entry;
        out[offset] = cs->entries[i].current_value;
        out[offset + count] = cs->entries[i].current_mask;

        cs->entries[i].current_mask = 0;
        cs->entries[i].current_value = 0;
    }

    *first = first_entry;
    *out_size = count * 2;
}
