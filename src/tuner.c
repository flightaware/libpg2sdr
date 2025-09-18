#include <pthread.h>

#include "internal.h"
#include "math.h"

static const lpf_settings lpf_calibration[] = {
    {
        .valid = true,
        .cutoff = 2027e3,
        .lpf_coarse = 3,
        .lpf_fine = 15,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .valid = true,
        .cutoff = 2093e3,
        .lpf_coarse = 3,
        .lpf_fine = 13,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .valid = true,
        .cutoff = 2320e3,
        .lpf_coarse = 1,
        .lpf_fine = 15,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .valid = true,
        .cutoff = 2601e3,
        .lpf_coarse = 1,
        .lpf_fine = 9,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .valid = true,
        .cutoff = 2891e3,
        .lpf_coarse = 0,
        .lpf_fine = 12,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .valid = true,
        .cutoff = 3177e3,
        .lpf_coarse = 0,
        .lpf_fine = 8,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .valid = true,
        .cutoff = 3525e3,
        .lpf_coarse = 0,
        .lpf_fine = 4,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .valid = true,
        .cutoff = 3960e3,
        .lpf_coarse = 0,
        .lpf_fine = 0,
        .lpf_q = 0,
        .lpf_narrow = 1,
    },
    {
        .valid = true,
        .cutoff = 5733e3,
        .lpf_coarse = 3,
        .lpf_fine = 15,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .valid = true,
        .cutoff = 5920e3,
        .lpf_coarse = 3,
        .lpf_fine = 13,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .valid = true,
        .cutoff = 6555e3,
        .lpf_coarse = 1,
        .lpf_fine = 15,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .valid = true,
        .cutoff = 7345e3,
        .lpf_coarse = 1,
        .lpf_fine = 9,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .valid = true,
        .cutoff = 8168e3,
        .lpf_coarse = 0,
        .lpf_fine = 12,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .valid = true,
        .cutoff = 8975e3,
        .lpf_coarse = 0,
        .lpf_fine = 8,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .valid = true,
        .cutoff = 9955e3,
        .lpf_coarse = 0,
        .lpf_fine = 4,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
    {
        .valid = true,
        .cutoff = 11196e3,
        .lpf_coarse = 0,
        .lpf_fine = 0,
        .lpf_q = 0,
        .lpf_narrow = 0,
    },
};

static const hpf_settings hpf_calibration[] = {
    {
        .valid = true,
        .cutoff = 527e3,
        .hpf_corner = 15,
    },
    {
        .valid = true,
        .cutoff = 659e3,
        .hpf_corner = 14,
    },
    {
        .valid = true,
        .cutoff = 774e3,
        .hpf_corner = 13,
    },
    {
        .valid = true,
        .cutoff = 863e3,
        .hpf_corner = 12,
    },
    {
        .valid = true,
        .cutoff = 1096e3,
        .hpf_corner = 11,
    },
    {
        .valid = true,
        .cutoff = 1374e3,
        .hpf_corner = 10,
    },
    {
        .valid = true,
        .cutoff = 1522e3,
        .hpf_corner = 9,
    },
    {
        .valid = true,
        .cutoff = 1665e3,
        .hpf_corner = 8,
    },
    {
        .valid = true,
        .cutoff = 1914e3,
        .hpf_corner = 7,
    },
    {
        .valid = true,
        .cutoff = 2138e3,
        .hpf_corner = 6,
    },
    {
        .valid = true,
        .cutoff = 2342e3,
        .hpf_corner = 5,
    },
    {
        .valid = true,
        .cutoff = 2458e3,
        .hpf_corner = 4,
    },
    {
        .valid = true,
        .cutoff = 2733e3,
        .hpf_corner = 3,
    },
    {
        .valid = true,
        .cutoff = 3005e3,
        .hpf_corner = 2,
    },
    {
        .valid = true,
        .cutoff = 3563e3,
        .hpf_corner = 1,
    },
    {
        .valid = true,
        .cutoff = 3724e3,
        .hpf_corner = 0,
    },

};

static int update_tuner_regs(lpcsdr_device_handle *dev, change_set *cs) {
    uint16_t first;
    uint8_t payload[TUNER_REG_MAX_PAYLOAD_SIZE] = {0};
    uint16_t payload_size;

    lpcsdr__prepare_tuner_payload_from_change_set(cs, &first, payload, &payload_size);
    if (!payload_size)
        return LPCSDR_SUCCESS; /* no work to do */

    return lpcsdr__ctrl_tuner_update(dev, first, payload, payload_size);
}

int lpcsdr__tuner_set_lna(lpcsdr_device_handle *dev, unsigned lna)
{
    if (lna > 15)
        return LPCSDR_ERROR_BAD_ARGUMENT;
    return lpcsdr__tuner_set_gains(dev, lna, -1, -1);
}

int lpcsdr__tuner_set_mix(lpcsdr_device_handle *dev, unsigned mix)
{
    if (mix > 15)
        return LPCSDR_ERROR_BAD_ARGUMENT;
    return lpcsdr__tuner_set_gains(dev, -1, mix, -1);
}

int lpcsdr__tuner_set_vga(lpcsdr_device_handle *dev, unsigned vga)
{
    if (vga > 15)
        return LPCSDR_ERROR_BAD_ARGUMENT;
    return lpcsdr__tuner_set_gains(dev, -1, -1, vga);
}

int lpcsdr__tuner_set_gains(lpcsdr_device_handle *dev, int lna, int mix, int vga)
{
    assert (lna <= 15);
    assert (mix <= 15);
    assert (vga <= 15);

    change_set cs = {0};
    if (lna >= 0 && lna != dev->lna_gain)
        set_tuner_reg(&cs, LNA_GAIN, lna);
    if (mix >= 0 && mix != dev->mix_gain)
        set_tuner_reg(&cs, MIX_GAIN, mix);
    if (vga >= 0 && vga != dev->vga_gain)
        set_tuner_reg(&cs, VGA_GAIN, vga);

    int error;
    if ((error = update_tuner_regs(dev, &cs)) < 0)
        return error;

    if (lna >= 0)
        dev->lna_gain = lna;
    if (mix >= 0)
        dev->mix_gain = mix;
    if (vga >= 0)
        dev->vga_gain = vga;

    return LPCSDR_SUCCESS;
}

/*
    Set the low pass filter values.
    Derive the absolute maximum from the requested sampling rate.
*/
int lpcsdr__tuner_set_bandpass(lpcsdr_device_handle *dev, const hpf_settings *hpf, const lpf_settings *lpf)
{
    assert(hpf && hpf->valid);
    assert(lpf && lpf->valid);

    change_set cs = {0};
    set_tuner_reg(&cs, IFFILT_HPF_CORNER, hpf->hpf_corner);
    set_tuner_reg(&cs, IFFILT_Q, lpf->lpf_q);
    set_tuner_reg(&cs, IFFILT_NARROW, lpf->lpf_narrow);
    set_tuner_reg(&cs, IFFILT_FINE_LPF, lpf->lpf_fine);
    set_tuner_reg(&cs, IFFILT_COARSE_LPF, lpf->lpf_coarse);
    return update_tuner_regs(dev, &cs);
}

/* 
    Find the lowest setting with cutoff >= target, but never above max.
    Max should be >= target.
    If max isn't 0, max should be >= lpf_calibration[0].cutoff.
*/
const lpf_settings *lpcsdr__lpf_settings_for(double target, double max)
{
    size_t limit = sizeof(lpf_calibration)/sizeof(lpf_calibration[0]);
    if (max > 0) {
        for (size_t i = 0; i < limit; ++i) {
            if (lpf_calibration[i].cutoff > max) {
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

    return &lpf_calibration[MAX(0, MIN(r, limit - 1))];
}

/* Find largest setting with cutoff <= target */
const hpf_settings *lpcsdr__hpf_settings_for(double target)
{
    const size_t limit = sizeof(hpf_calibration)/sizeof(hpf_calibration[0]);
    int p = 0;
    while (p < limit) {
        if (hpf_calibration[p].cutoff > target)
            break;
        p++;
    }
    return &hpf_calibration[MAX(0, p - 1)];
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
    set_tuner_reg(&cs, LNA_GAIN, 0); dev->lna_gain = 0;

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
    set_tuner_reg(&cs, MIX_GAIN, 0); dev->mix_gain = 0;

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
    set_tuner_reg(&cs, VGA_GAIN, 0); dev->vga_gain = 0;

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

int lpcsdr__find_pll_parameters(double requested, double xtal, tuner_pll_config_t *out) {
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

    return LPCSDR_SUCCESS;
}

int lpcsdr__start_pll(lpcsdr_device_handle *dev, tuner_pll_config_t *params) {
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

int lpcsdr__configure_pll_settings(lpcsdr_device_handle *dev, tuner_pll_config_t *params) {
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
    uint8_t seldiv = -1;
    for (unsigned i = 0; i < sizeof(seldiv_lut)/ sizeof(seldiv_lut[0]); i++)
        if (seldiv_lut[i] == params->seldiv)
            seldiv = i;


    uint8_t refdiv = (params->refdiv == true) ? 1: 0;

    uint8_t vco_dac = round(params->actual_vco * 0.0318e-6 - 49.0);
    vco_dac = MAX(0, vco_dac);
    vco_dac = MIN(63, vco_dac);

    change_set cs = {0};

    set_tuner_reg(&cs, IMG_R, dev->upper_sideband ? 1 : 0);
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
void lpcsdr__prepare_tuner_payload_from_change_set(change_set *cs, uint16_t *first, uint8_t *out, uint16_t *out_size)
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
