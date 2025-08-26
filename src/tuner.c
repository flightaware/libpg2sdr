#include "internal.h"
#include "math.h"

static int update_tuner_regs(lpcsdr_device_handle *handle, change_set *cs) {
    uint16_t first;
    uint8_t payload[64] = {0};
    uint16_t payload_size;

    prepare_tuner_payload_from_change_set(cs, &first, &payload[0], &payload_size);
    return lpcsdr__ctrl_tuner_update(handle, first, &payload[0], payload_size);
}

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

lpf_settings lpf_settings_for(int target, int *max) {
    int limit = sizeof(lpf_calibration)/sizeof(lpf_calibration[0]);
    if (max != NULL) {
        while (limit - 1 > 0) {
            if (lpf_calibration[limit - 1].cutoff < *max) {
                break;
            }
            limit--;
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

hpf_settings hpf_settings_for(int target) {
    int p = 0;
    while (p < sizeof(hpf_calibration)/sizeof(hpf_calibration[0])) {
        if (hpf_calibration[p].cutoff > target)
            break;
        p++;
    }
    return hpf_calibration[MAX(0, p - 1)];
}

int set_tuner_value_in_change_set(change_set *cs, uint8_t reg, uint8_t mask, uint8_t value) {
    cs->entries[reg].current_mask |= mask;
    cs->entries[reg].current_value = (cs->entries[reg].current_value & ~mask) | value;
    return LPCSDR_SUCCESS;
}

int find_pll_parameters(double requested, double xtal, pll_parameters *out) {
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
        return LPCSDR_ERROR_BAD_ARGUMENT;
    }

    double pll_feedback = (required_vco / 2) / pll_ref;
    if (pll_feedback < 13 || pll_feedback >= 269) {
        return LPCSDR_ERROR_BAD_ARGUMENT;
    }

    double pll_feedback_int_part;
    double pll_feedback_frac;
    if (!(pll_feedback_frac = modf(pll_feedback, &pll_feedback_int_part))) {
        return LCPSDR_TUNER_PLL_DIV_OUT_OF_RANGE;
    }

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

int has_pll_lock(lpcsdr_device_handle *handle) {
    int error = LPCSDR_SUCCESS;
    uint8_t buffer;
    
    if ((error = lpcsdr__ctrl_read_tuner_register(handle, TunerR2, 0, &buffer, sizeof(uint8_t))) < 0)
        return error;

    return extract_tuner_val(buffer, PLL_LOCK);
}

int configure_pll_settings(lpcsdr_device_handle *handle, pll_parameters *params) {
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

    change_set cs = {};

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

    return update_tuner_regs(handle, &cs);
}

int tune_pll(lpcsdr_device_handle *handle, double requested_frequency) {
    int error = LPCSDR_SUCCESS;

    pll_parameters params = {};

    if ((error = find_pll_parameters(requested_frequency, handle->tuner_xtal, &params)) < 0)
        return error;

    if ((error = start_pll(handle, &params)) < 0)
        return error;

    return error;
}

int start_pll(lpcsdr_device_handle *handle, pll_parameters *params) {
    int error = LPCSDR_SUCCESS;
    
    if ((error = configure_pll_settings(handle, params)) < 0)
        return 0;
    
    return error;
}

int init_tuner(lpcsdr_device_handle *handle) {
    int error = LPCSDR_SUCCESS;

    if ((error = lpcsdr__ctrl_set_rf_power(handle, 1)) < 0)
        return error;

    uint8_t buffer;
    if ((error = lpcsdr__ctrl_read_tuner_register(handle, TunerR0, 0, &buffer, sizeof(uint8_t))) < 0)
        return error;

    uint8_t tuner_id = extract_tuner_val(buffer, TUNER_ID);
    if (tuner_id != 0x96)
        return LPCSDR_TUNER_INIT_FAILED;

    return error;
}

/*
    Create a payload of bytes from the changeset.
 */
void prepare_tuner_payload_from_change_set(change_set *cs, uint16_t *first, uint8_t *out, uint16_t *out_size) {
    uint16_t num_entries = 0;
    uint16_t first_entry = 5;
    uint16_t last_entry = 5;

    for (uint16_t i = 5; i < TUNER_REG_COUNT; i++) {
        if (cs->entries[i].current_mask != 0) {
            num_entries++;
            if (first_entry >= i)
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
