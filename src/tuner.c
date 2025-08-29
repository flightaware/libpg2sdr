#include "internal.h"
#include "math.h"

lpf_settings lpf_calibration[16] = {
    {
        .cutoff = 2027e3,
        .lpf_coarse = 3,
        .lpf_fine = 15,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .cutoff = 2093e3,
        .lpf_coarse = 3,
        .lpf_fine = 13,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .cutoff = 2320e3,
        .lpf_coarse = 1,
        .lpf_fine = 15,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .cutoff = 2601e3,
        .lpf_coarse = 1,
        .lpf_fine = 9,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .cutoff = 2891e3,
        .lpf_coarse = 0,
        .lpf_fine = 12,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .cutoff = 3177e3,
        .lpf_coarse = 0,
        .lpf_fine = 8,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .cutoff = 3525e3,
        .lpf_coarse = 0,
        .lpf_fine = 4,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .cutoff = 3960e3,
        .lpf_coarse = 0,
        .lpf_fine = 0,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .cutoff = 5733e3,
        .lpf_coarse = 3,
        .lpf_fine = 15,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .cutoff = 5920e3,
        .lpf_coarse = 3,
        .lpf_fine = 13,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .cutoff = 6555e3,
        .lpf_coarse = 1,
        .lpf_fine = 15,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .cutoff = 7345e3,
        .lpf_coarse = 1,
        .lpf_fine = 9,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .cutoff = 8168e3,
        .lpf_coarse = 0,
        .lpf_fine = 12,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .cutoff = 8975e3,
        .lpf_coarse = 0,
        .lpf_fine = 8,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .cutoff = 9955e3,
        .lpf_coarse = 0,
        .lpf_fine = 4,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .cutoff = 11196e3,
        .lpf_coarse = 0,
        .lpf_fine = 0,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
};

hpf_settings hpf_calibration[16] = {
    {
        .cutoff = 527e3,
        .hpf_corner = 15,
    },
    {
        .cutoff = 659e3,
        .hpf_corner = 14,
    },
    {
        .cutoff = 774e3,
        .hpf_corner = 13,
    },
    {
        .cutoff = 863e3,
        .hpf_corner = 12,
    },
    {
        .cutoff = 1096e3,
        .hpf_corner = 11,
    },
    {
        .cutoff = 1374e3,
        .hpf_corner = 10,
    },
    {
        .cutoff = 1522e3,
        .hpf_corner = 9,
    },
    {
        .cutoff = 1665e3,
        .hpf_corner = 8,
    },
    {
        .cutoff = 1914e3,
        .hpf_corner = 7,
    },
    {
        .cutoff = 2138e3,
        .hpf_corner = 6,
    },
    {
        .cutoff = 2342e3,
        .hpf_corner = 5,
    },
    {
        .cutoff = 2458e3,
        .hpf_corner = 4,
    },
    {
        .cutoff = 2733e3,
        .hpf_corner = 3,
    },
    {
        .cutoff = 3005e3,
        .hpf_corner = 2,
    },
    {
        .cutoff = 3563e3,
        .hpf_corner = 1,
    },
    {
        .cutoff = 3724e3,
        .hpf_corner = 0,
    },

};

static int update_tuner_regs(lpcsdr_device_handle *dev, change_set *cs) {
    uint16_t first;
    uint8_t payload[TUNER_REG_MAX_PAYLOAD_SIZE] = {0};
    uint16_t payload_size;

    lpcsdr__prepare_tuner_payload_from_change_set(cs, &first, payload, &payload_size);
    return lpcsdr__ctrl_tuner_update(dev, first, payload, payload_size);
}

int lpcsdr_tune_pll(lpcsdr_device_handle *dev, double requested_frequency) {
    int error = LPCSDR_SUCCESS;

    pll_parameters params = {};

    if ((error = lpcsdr__find_pll_parameters(requested_frequency, dev->tuner_xtal, &params)) < 0)
        return error;

    if ((error = lpcsdr__start_pll(dev, &params)) < 0)
        return error;

    return error;
}

int lpcsdr_set_lna_gain(lpcsdr_device_handle *dev, uint16_t gain) {
    change_set cs = {0};
    set_tuner_reg(&cs, LNA_GAIN, gain);
    return update_tuner_regs(dev, &cs);
}

int lpcsdr_set_mix_gain(lpcsdr_device_handle *dev, uint16_t gain) {
    change_set cs = {0};
    set_tuner_reg(&cs, MIX_GAIN, gain);
    return update_tuner_regs(dev, &cs);
}

int lpcsdr_set_vga_gain(lpcsdr_device_handle *dev, uint16_t gain) {
    change_set cs = {0};
    set_tuner_reg(&cs, VGA_GAIN, gain);
    return update_tuner_regs(dev, &cs);
}

int lpcsdr_set_bandwidth_highend_cutoff(lpcsdr_device_handle *dev, int cutoff, int *not_above) {
    if (not_above && (*not_above < cutoff))
        return LPCSDR_TUNER_LPF_INVALID_ARG;

    lpf_settings s = lpcsdr__lpf_settings_for(cutoff, not_above);
    change_set cs = {0};

    LOGDEBUG(dev, "set LPF cutoff = %.3fMHz", s.cutoff/1e6);

    set_tuner_reg(&cs, IFFILT_Q, s.lpf_q);
    set_tuner_reg(&cs, IFFILT_NARROW, s.lpf_narrow);
    set_tuner_reg(&cs, IFFILT_FINE_LPF, s.lpf_fine);
    set_tuner_reg(&cs, IFFILT_COARSE_LPF, s.lpf_coarse);
    return update_tuner_regs(dev, &cs);
}

int lpcsdr_set_bandwidth_lowend_cutoff(lpcsdr_device_handle *dev, int cutoff) {
    hpf_settings s = lpcsdr__hpf_settings_for(cutoff);
    change_set cs = {0};

    LOGDEBUG(dev, "set HPF cutoff = %.3fMHz", s.cutoff/1e6);

    set_tuner_reg(&cs, IFFILT_HPF_CORNER, s.hpf_corner);
    return update_tuner_regs(dev, &cs);
}

int lpcsdr_set_center_frequency_bandwidth(lpcsdr_device_handle *dev, int low, int high, int *max) {
    int error = LPCSDR_SUCCESS;

    if ((error = lpcsdr_set_bandwidth_lowend_cutoff(dev, MIN(low, high))) < 0)
        return error;
    if ((error = lpcsdr_set_bandwidth_highend_cutoff(dev, MAX(low, high), max)) < 0)
        return error;

    return error;
}

/* 
    Find the lowest setting with cutoff >= target, but never above max.
    Assumes max >= target.
*/
lpf_settings lpcsdr__lpf_settings_for(int target, int *max) {
    int limit = sizeof(lpf_calibration)/sizeof(lpf_calibration[0]);
    if (max != NULL) {
        for (int i = 0; i < sizeof(lpf_calibration)/sizeof(lpf_calibration[0]); i++) {
            if (lpf_calibration[i].cutoff > *max) {
                limit = i;
                break;
            }
        }
    }

    int r = 0;
    while (r < limit) {
        if (lpf_calibration[r].cutoff >= target)
            break;
        r++;
    }

    return lpf_calibration[MAX(0, MIN(r, limit - 1))];
}

/* Find largest setting with cutoff <= target */
hpf_settings lpcsdr__hpf_settings_for(int target) {
    int p = 0;
    while (p < sizeof(hpf_calibration)/sizeof(hpf_calibration[0])) {
        if (hpf_calibration[p].cutoff > target)
            break;
        p++;
    }
    return hpf_calibration[MAX(0, p - 1)];
}

int lpcsdr__init_tuner(lpcsdr_device_handle *dev) {
    int error = LPCSDR_SUCCESS;

    if ((error = lpcsdr__ctrl_set_rf_power(dev, RF_POWER_RESET)) < 0)
        return error;

    uint8_t buffer;
    if ((error = lpcsdr__ctrl_read_tuner_register(dev, TunerR0, 0, &buffer, sizeof(uint8_t))) < 0)
        return error;

    uint8_t tuner_id = extract_tuner_val(buffer, TUNER_ID);
    if (tuner_id != 0x96)
        return LPCSDR_TUNER_INIT_FAILED;

    return lpcsdr__set_initial_values(dev);
}

int lpcsdr__set_initial_values(lpcsdr_device_handle *dev) {
    change_set cs = {0};

    // TunerR5
    set_tuner_reg(&cs, PWD_LT, 1);
    set_tuner_reg(&cs, R5_RESERVED_6_0, 0);
    set_tuner_reg(&cs, PWD_LNA1, 0);
    set_tuner_reg(&cs, LNA_GAIN_MODE, 1);
    set_tuner_reg(&cs, LNA_GAIN, 0);

    // TunerR6
    set_tuner_reg(&cs, PWD_PDET1, 0);
    set_tuner_reg(&cs, PWD_PDET2, 0);
    set_tuner_reg(&cs, FILT_3DB, 1);
    set_tuner_reg(&cs, R6_RESERVED_4_1, 1);
    set_tuner_reg(&cs, R6_RESERVED_3_0, 0);
    set_tuner_reg(&cs, PW_LNA, 2);

    // TunerR7
    set_tuner_reg(&cs, IMG_R, 0);
    set_tuner_reg(&cs, PW_MIX, 1);
    set_tuner_reg(&cs, PW0_MIX, 1);
    set_tuner_reg(&cs, MIXGAIN_MODE, 0);
    set_tuner_reg(&cs, MIX_GAIN, 0);


    // TunerR8
    set_tuner_reg(&cs, PW_AMP, 1);
    set_tuner_reg(&cs, PW0_AMP, 1);
    set_tuner_reg(&cs, IMR_G_PATH, 0);
    set_tuner_reg(&cs, IMR_G, 0);

    // TunerR9
    set_tuner_reg(&cs, PWD_IFFILT, 0);
    set_tuner_reg(&cs, PW1_IFFILT, 1);
    set_tuner_reg(&cs, IMR_P_PATH, 0);
    set_tuner_reg(&cs, IMR_P, 0);

    // TunerR10
    set_tuner_reg(&cs, PW_FILT, 1);
    set_tuner_reg(&cs, FILTER_CUR, 3);
    set_tuner_reg(&cs, IFFILT_Q, 0);
    set_tuner_reg(&cs, IFFILT_FINE_LPF, 0);

    // TunerR11
    set_tuner_reg(&cs, IFFILT_NARROW, 0);
    set_tuner_reg(&cs, IFFILT_COARSE_LPF, 0);
    set_tuner_reg(&cs, CALIBRATION_TRIGGER, 0);
    set_tuner_reg(&cs, IFFILT_HPF_CORNER, 15);

    // TunerR12
    set_tuner_reg(&cs, PWD_ADC, 1);
    set_tuner_reg(&cs, PW_VGA, 1);
    set_tuner_reg(&cs, R12_RESERVED_5_1, 1);
    set_tuner_reg(&cs, VGA_GAIN_MODE, 0);
    set_tuner_reg(&cs, VGA_GAIN, 0);

    // TunerR13
    set_tuner_reg(&cs, LNA_VTH_H, 5);
    set_tuner_reg(&cs, LNA_VTH_L, 3);

    // TunerR14
    set_tuner_reg(&cs, MIX_VTH_H, 7);
    set_tuner_reg(&cs, MIX_VTH_L, 5);

    // TunerR15
    set_tuner_reg(&cs, FLT_EXT_WIDEST, 0);
    set_tuner_reg(&cs, R15_RESERVED_6_0, 0);
    set_tuner_reg(&cs, R15_RESERVED_5_1, 1);
    set_tuner_reg(&cs, CLK_OUT_DIS, 1);
    set_tuner_reg(&cs, RING_DISABLE, 1);
    set_tuner_reg(&cs, R15_RESERVED_2_0, 0);
    set_tuner_reg(&cs, CLK_AGC_DIS, 0);
    set_tuner_reg(&cs, R15_RESERVED_0_0, 0);

    // TunerR16
    set_tuner_reg(&cs, SEL_DIV, 0);
    set_tuner_reg(&cs, REF_DIV2, 1);
    set_tuner_reg(&cs, XTAL_DRIVE, 0);
    set_tuner_reg(&cs, DET1_CAP, 1);
    set_tuner_reg(&cs, CAPX, 0);

    // TunerR17
    set_tuner_reg(&cs, PW_LDO_A, 0);
    set_tuner_reg(&cs, CP_CURRENT, 0);
    set_tuner_reg(&cs, R17_RESERVED_2_0, 0);
    set_tuner_reg(&cs, R17_RESERVED_1_0, 0);
    set_tuner_reg(&cs, R17_RESERVED_0_0, 0);

    // TunerR18
    set_tuner_reg(&cs, VCO_CURRENT, 4);
    set_tuner_reg(&cs, SDM_DITHER_DIS, 0);
    set_tuner_reg(&cs, PWD_SDM, 1);
    set_tuner_reg(&cs, R18_RESERVED_2_0, 0);
    set_tuner_reg(&cs, R18_RESERVED_1_0, 0);
    set_tuner_reg(&cs, R18_RESERVED_0_0, 0);

    // TunerR19
    set_tuner_reg(&cs, R18_RESERVED_7_0, 0);
    set_tuner_reg(&cs, VCO_MODE, 0);
    set_tuner_reg(&cs, VCO_DAC, 0);

    // TunerR20
    set_tuner_reg(&cs, S_I2C, 0);
    set_tuner_reg(&cs, N_I2C, 0);

    // TunerR21
    set_tuner_reg(&cs, SDM_IN_LSB, 0);

    // TunerR22
    set_tuner_reg(&cs, SDM_IN_MSB, 0);

    // TunerR23
    set_tuner_reg(&cs, PW_LDO_D, 0);
    set_tuner_reg(&cs, DIV_BUF_CUR, 3);
    set_tuner_reg(&cs, OPEN_D, 0);
    set_tuner_reg(&cs, R23_RESERVED_2_1, 1);
    set_tuner_reg(&cs, R23_RESERVED_1_0, 0);
    set_tuner_reg(&cs, R23_RESERVED_0_0, 0);

    // TunerR24
    set_tuner_reg(&cs, R24_RESERVED_7_0, 0);
    set_tuner_reg(&cs, R24_RESERVED_6_1, 1);
    set_tuner_reg(&cs, RING_SE23, 0);
    set_tuner_reg(&cs, PW_RING, 0);
    set_tuner_reg(&cs, RING_N, 0);

    // TunerR25
    set_tuner_reg(&cs, PW_RFFILT, 1);
    set_tuner_reg(&cs, RFFILT_CURRENT, 2);
    set_tuner_reg(&cs, SW_AGC, 0);
    set_tuner_reg(&cs, R25_RESERVED_3_1, 1);
    set_tuner_reg(&cs, R25_RESERVED_2_1, 1);
    set_tuner_reg(&cs, RING_SELDIV, 0);

    // TunerR26
    set_tuner_reg(&cs, RFMUX, 1);
    set_tuner_reg(&cs, AGC_CLOCK, 2);
    set_tuner_reg(&cs, PLL_AUTO_CLK, 0);
    set_tuner_reg(&cs, RFFILT, 0);

    // TunerR27
    set_tuner_reg(&cs, TF_NCH, 0);
    set_tuner_reg(&cs, TF_LP, 0);

    // TunerR28
    set_tuner_reg(&cs, PDET3_GAIN, 5);
    set_tuner_reg(&cs, R28_RESERVED_3_0, 0);
    set_tuner_reg(&cs, DISCHARGE_MODE, 1);
    set_tuner_reg(&cs, RF_SOURCE, 0);
    set_tuner_reg(&cs, R28_RESERVED_0_0, 0);

    // TunerR29
    set_tuner_reg(&cs, DETECT_BW, 2);
    set_tuner_reg(&cs, PDET1_GAIN, 4);
    set_tuner_reg(&cs, PDET2_GAIN, 6);

    // TunerR30
    set_tuner_reg(&cs, SW_PDET, 0);
    set_tuner_reg(&cs, FILTER_EXT, 0);
    set_tuner_reg(&cs, PDET_CLK, 0x0A);

    // TunerR31
    set_tuner_reg(&cs, LT_ATT, 1);
    set_tuner_reg(&cs, R31_RESERVED_6_1, 1);
    set_tuner_reg(&cs, R31_RESERVED_5_0, 0);
    set_tuner_reg(&cs, R31_RESERVED_4_0, 0);
    set_tuner_reg(&cs, R31_RESERVED_3_0, 0);
    set_tuner_reg(&cs, R31_RESERVED_2_0, 0);
    set_tuner_reg(&cs, RING_ATT, 0);

    return update_tuner_regs(dev, &cs);
}

int lpcsdr__find_pll_parameters(double requested, double xtal, pll_parameters *out) {
    bool refdiv = false;
    double pll_ref = xtal;

    if (xtal > 24e6) {
        refdiv = true;
        pll_ref = xtal / 2;
    }

    double VCO_MIN = 1750e6;
    double VCO_MAX = 3700e6;
    double seldiv = 2;
    while ((requested * seldiv < VCO_MIN) && (seldiv < 64))
        seldiv *= 2;

    double required_vco = requested * seldiv;
    if (required_vco < VCO_MIN || required_vco > VCO_MAX) {
        // todo: we could continue here, since sometimes the VCO is okay
        // running somewhat outside the nominal limits,
        // and just report any PLL lock failure
        return LPCSDR_TUNER_PLL_DIV_OUT_OF_RANGE;
    }

    double pll_feedback = (required_vco / 2) / pll_ref;
    if (pll_feedback < 13 || pll_feedback >= 269) {
        return LPCSDR_TUNER_PLL_DIV_OUT_OF_RANGE;
    }

    double pll_feedback_int_part;
    double pll_feedback_frac = modf(pll_feedback, &pll_feedback_int_part);

    unsigned pll_feedback_int = (unsigned) pll_feedback_int_part;
    unsigned sdm_numerator = (unsigned) round(pll_feedback_frac * (1<<18));

    sdm_numerator = (sdm_numerator & ~3) | 1;

    if (sdm_numerator < 32)
        sdm_numerator = 0;
    else if (sdm_numerator > ((1<<18) - 32)) {
        sdm_numerator = 0;
        pll_feedback_int += 1;
    } else if (sdm_numerator > ((1<<17) - 32) && sdm_numerator <= (1<<17)) {
        sdm_numerator = (1<<17) - 32;
    } else if (sdm_numerator > (1<<17) && sdm_numerator < ((1<<17) + 32)) {
        sdm_numerator = (1<<17) + 32;
    }

    double actual_vco = pll_ref * 2 * (pll_feedback_int + (double) sdm_numerator / (1<<18));
    double actual_out = actual_vco / seldiv;

    out->refdiv = refdiv;
    out->seldiv = seldiv;
    out->feedback_n = pll_feedback_int;
    out->feedback_sdm = sdm_numerator >> 2;
    out->vco = actual_vco;
    out->freq = actual_out;

    return LPCSDR_SUCCESS;
}

int lpcsdr__start_pll(lpcsdr_device_handle *dev, pll_parameters *params) {
    int error = LPCSDR_SUCCESS;
    int resp = 0;
    
    if ((error = lpcsdr__configure_pll_settings(dev, params)) < 0)
        return error;

    uint16_t vco_currents[5] = {4, 3, 2, 1, 0};

    for (unsigned i = 0; i < sizeof(vco_currents)/sizeof(vco_currents[0]); i++) {
        resp = lpcsdr__ctrl_update_tuner_lock(dev, vco_currents[i], 50);
        
        if (resp < 0)
            return resp;
        else if (resp)
            break;
    }
    if (resp != 1)
        return LPCSDR_TUNER_LOCK_ERR;

    change_set cs = {0};
    set_tuner_reg(&cs, PLL_AUTO_CLK, 2);
    error = update_tuner_regs(dev, &cs);

    return error;
}

int lpcsdr__configure_pll_settings(lpcsdr_device_handle *dev, pll_parameters *params) {
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
             params->vco / 1e6,
             params->freq / 1e6);

    uint16_t sdm_disable = ( params->feedback_sdm == 0 ) ? 1 : 0;
    uint8_t sdm_lsb = params->feedback_sdm & 0xff;
    uint8_t sdm_msb = (params->feedback_sdm >> 8) & 0xff;

    uint8_t ni2c = (params->feedback_n - 13) / 4;
    uint8_t si2c = (params->feedback_n - 13) & 3;
    
    uint8_t seldiv_lut[6] = {2, 4, 8, 16, 32, 64};
    uint8_t seldiv = -1;
    for (unsigned i = 0; i < sizeof(seldiv_lut)/ sizeof(seldiv_lut[0]); i++)
        if (seldiv_lut[i] == params->seldiv)
            seldiv = i;


    uint8_t refdiv = (params->refdiv == true) ? 1: 0;

    uint8_t vco_dac = round(params->vco * 0.0318e-6 - 49.0);
    vco_dac = MAX(0, vco_dac);
    vco_dac = MIN(63, vco_dac);

    change_set cs = {0};

    set_tuner_reg(&cs, PW_LDO_A, 1);
    set_tuner_reg(&cs, PW_LDO_D, 2);
    set_tuner_reg(&cs, PWD_SDM, sdm_disable);
    set_tuner_reg(&cs, SEL_DIV, seldiv);
    set_tuner_reg(&cs, REF_DIV2, refdiv);
    set_tuner_reg(&cs, S_I2C, si2c);
    set_tuner_reg(&cs, N_I2C, ni2c);
    set_tuner_reg(&cs, SDM_IN_LSB, sdm_lsb);
    set_tuner_reg(&cs, SDM_IN_MSB, sdm_msb);
    set_tuner_reg(&cs, PLL_AUTO_CLK, 0);
    set_tuner_reg(&cs, VCO_CURRENT, 4);
    set_tuner_reg(&cs, VCO_MODE, 1);
    set_tuner_reg(&cs, VCO_DAC, vco_dac);

    return update_tuner_regs(dev, &cs);
}

int lpcsdr__has_pll_lock(lpcsdr_device_handle *dev) {
    int error = LPCSDR_SUCCESS;
    uint8_t buffer;
    
    if ((error = lpcsdr__ctrl_read_tuner_register(dev, TunerR2, 0, &buffer, sizeof(uint8_t))) < 0)
        return error;

    return extract_tuner_val(buffer, PLL_LOCK);
}

int lpcsdr__set_tuner_value_in_change_set(change_set *cs, uint8_t reg, uint8_t mask, uint8_t value) {
    cs->entries[reg].current_mask |= mask;
    cs->entries[reg].current_value = (cs->entries[reg].current_value & ~mask) | value;
    return LPCSDR_SUCCESS;
}

/*
    Create a payload of bytes from the changeset.
 */
void lpcsdr__prepare_tuner_payload_from_change_set(change_set *cs, uint16_t *first, uint8_t *out, uint16_t *out_size) {
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
