#include <stdint.h>

#define onebit(x) (1 << x)
#define bitrange(msb, lsb) (((2<<msb)-1) ^ ((1<<lsb)-1))
#define TUNER_REG_COUNT 32

#define REG_FLAG(reg, name, mask) \
  static const uint8_t name ## _REG = (reg); \
  static const uint8_t name ## _MASK = (mask); \
  static const uint8_t name ## _OFFSET = (__builtin_ctz(mask));

#define set_tuner_reg(cs, name, value) \
    set_tuner_value_in_change_set(cs, \
    name ## _REG, \
    name ## _MASK, \
    (value) << name ## _OFFSET)

#define extract_tuner_val_helper(buffer, mask, offset) ((buffer & mask) >> offset)
#define extract_tuner_val(buffer, name) extract_tuner_val_helper(buffer, name ## _MASK, name ## _OFFSET)

typedef struct {
    float cutoff;
    int lpf_coarse;
    int lpf_fine;
    int lpf_q;
    int lpf_narrow;
} lpf_settings;

typedef struct {
    float cutoff;
    int hpf_corner;
} hpf_settings;

typedef struct {
    uint8_t current_value;
    uint8_t current_mask;
} change_set_value;

typedef struct {
    change_set_value entries[TUNER_REG_COUNT];
} change_set;

typedef struct {
    bool refdiv;
    int seldiv;
    int feedback_n;
    int feedback_sdm;
    double vco;
    double freq;
} pll_parameters;

typedef enum { 
    TunerR0 = 0,
    TunerR1 = 1,
    TunerR2 = 2,
    TunerR3 = 3,
    TunerR4 = 4,
    TunerR5 = 5,
    TunerR6 = 6,
    TunerR7 = 7,
    TunerR8 = 8,
    TunerR9 = 9,
    TunerR10 = 10,
    TunerR11 = 11,
    TunerR12 = 12,
    TunerR13 = 13,
    TunerR14 = 14,
    TunerR15 = 15,
    TunerR16 = 16,
    TunerR17 = 17,
    TunerR18 = 18,
    TunerR19 = 19,
    TunerR20 = 20,
    TunerR21 = 21,
    TunerR22 = 22,
    TunerR23 = 23,
    TunerR24 = 24,
    TunerR25 = 25,
    TunerR26 = 26,
    TunerR27 = 27,
    TunerR28 = 28,
    TunerR29 = 29,
    TunerR30 = 30,
    TunerR31 = 31,
} tuner_reg_num;

lpf_settings lpf_settings_for(int target, int *max);
hpf_settings hpf_settings_for(int target);

// Tuner
int init_tuner(lpcsdr_device_handle *handle);
int start_pll(lpcsdr_device_handle *handle, pll_parameters *params);
int find_pll_parameters(double requested, double xtal, pll_parameters *out);
int has_pll_lock(lpcsdr_device_handle *handle);
int configure_pll_settings(lpcsdr_device_handle *handle, pll_parameters *params);
int tune_pll(lpcsdr_device_handle *handle, double requested_frequency);

// Change
void prepare_tuner_payload_from_change_set(change_set *cs, uint16_t *first, uint8_t *out, uint16_t *out_size);
int set_tuner_value_in_change_set(change_set *cs, uint8_t reg, uint8_t mask, uint8_t value);

REG_FLAG(TunerR0, TUNER_ID,             bitrange(7, 0))

REG_FLAG(TunerR1, DET3_ADC,             bitrange(5, 0))

REG_FLAG(TunerR2, PLL_LOCK,             onebit(6))
REG_FLAG(TunerR2, VCO_ADC,              bitrange(5, 0))

REG_FLAG(TunerR3, MIX_ADC,              bitrange(7, 4))
REG_FLAG(TunerR3, LNA_ADC,              bitrange(3, 0))

REG_FLAG(TunerR4, vco_fine_tune,        bitrange(5, 4))
REG_FLAG(TunerR4, filt_cal_code,        bitrange(3, 0))

REG_FLAG(TunerR5, PWD_LT,               onebit(7))
REG_FLAG(TunerR5, R5_reserved_6_0,      onebit(6))
REG_FLAG(TunerR5, PWD_LNA1,             onebit(5))
REG_FLAG(TunerR5, LNA_GAIN_MODE,        onebit(4))
REG_FLAG(TunerR5, LNA_GAIN,             bitrange(3, 0))

REG_FLAG(TunerR6, PWD_PDET1,            onebit(7))
REG_FLAG(TunerR6, PWD_PDET2,            onebit(6))
REG_FLAG(TunerR6, FILT_3DB,             onebit(5))
REG_FLAG(TunerR6, R6_reserved_4_1,      onebit(4))
REG_FLAG(TunerR6, R6_reserved_3_0,      onebit(3))
REG_FLAG(TunerR6, PW_LNA,               bitrange(2, 0))

REG_FLAG(TunerR7, img_r,                onebit(7))
REG_FLAG(TunerR7, PW_MIX,               onebit(6))
REG_FLAG(TunerR7, PW0_MIX,              onebit(5))
REG_FLAG(TunerR7, MIXGAIN_MODE,         onebit(4))
REG_FLAG(TunerR7, MIX_GAIN,             bitrange(3, 0))

REG_FLAG(TunerR8, PW_AMP,               onebit(7))
REG_FLAG(TunerR8, PW0_AMP,              onebit(6))
REG_FLAG(TunerR8, imr_g_path,           onebit(5))
REG_FLAG(TunerR8, IMR_G,                bitrange(4, 0))

REG_FLAG(TunerR9, PWD_IFFILT,           onebit(7))
REG_FLAG(TunerR9, PW1_IFFILT,           onebit(6))
REG_FLAG(TunerR9, imr_p_path,           onebit(5))
REG_FLAG(TunerR9, IMR_P,                bitrange(4, 0))

REG_FLAG(TunerR10, PW_FILT,             onebit(7))
REG_FLAG(TunerR10, filter_cur,          bitrange(6, 5))
REG_FLAG(TunerR10, iffilt_q,            onebit(4))
REG_FLAG(TunerR10, iffilt_fine_lpf,     bitrange(3, 0))

REG_FLAG(TunerR11, iffilt_narrow,       onebit(7))
REG_FLAG(TunerR11, iffilt_coarse_lpf,   bitrange(6, 5))
REG_FLAG(TunerR11, calibration_trigger, onebit(4))
REG_FLAG(TunerR11, iffilt_hpf_corner,   bitrange(3, 0))

REG_FLAG(TunerR12, pwd_adc,             onebit(7))
REG_FLAG(TunerR12, PW_VGA,              onebit(6))
REG_FLAG(TunerR12, R12_reserved_5_1,    onebit(5))
REG_FLAG(TunerR12, VGA_GAIN_MODE,       onebit(4))
REG_FLAG(TunerR12, VGA_GAIN,            bitrange(3, 0))

REG_FLAG(TunerR13, LNA_VTH_H,           bitrange(7, 4))
REG_FLAG(TunerR13, LNA_VTH_L,           bitrange(3, 0))

REG_FLAG(TunerR14, MIX_VTH_H,           bitrange(7, 4))
REG_FLAG(TunerR14, MIX_VTH_L,           bitrange(3, 0))

REG_FLAG(TunerR15, flt_ext_widest,      onebit(7))
REG_FLAG(TunerR15, R15_reserved_6_0,    onebit(6))
REG_FLAG(TunerR15, R15_reserved_5_1,    onebit(5))
REG_FLAG(TunerR15, clk_out_dis,         onebit(4))
REG_FLAG(TunerR15, ring_disable,        onebit(3))
REG_FLAG(TunerR15, R15_reserved_2_0,    onebit(2))
REG_FLAG(TunerR15, clk_agc_dis,         onebit(1))
REG_FLAG(TunerR15, R15_reserved_0_0,    onebit(0))

REG_FLAG(TunerR16, SEL_DIV,             bitrange(7, 5))
REG_FLAG(TunerR16, REF_DIV2,            onebit(4))
REG_FLAG(TunerR16, xtal_drive,          onebit(3))
REG_FLAG(TunerR16, det1_cap,            onebit(2))
REG_FLAG(TunerR16, CAPX,                bitrange(1, 0))

REG_FLAG(TunerR17, PW_LDO_A,            bitrange(7, 6))
REG_FLAG(TunerR17, cp_current,          bitrange(5, 3))
REG_FLAG(TunerR17, R17_reserved_2_0,    onebit(2))
REG_FLAG(TunerR17, R17_reserved_1_0,    onebit(1))
REG_FLAG(TunerR17, R17_reserved_0_0,    onebit(0))

REG_FLAG(TunerR18, VCO_CURRENT,         bitrange(7, 5))
REG_FLAG(TunerR18, sdm_dither_dis,      onebit(4))
REG_FLAG(TunerR18, PWD_SDM,             onebit(3))
REG_FLAG(TunerR18, R18_reserved_2_0,    onebit(2))
REG_FLAG(TunerR18, R18_reserved_1_0,    onebit(1))
REG_FLAG(TunerR18, R18_reserved_0_0,    onebit(0))

REG_FLAG(TunerR19, R18_reserved_7_0,    onebit(7))
REG_FLAG(TunerR19, VCO_MODE,            onebit(6))
REG_FLAG(TunerR19, VCO_DAC,             bitrange(5, 0))

REG_FLAG(TunerR20, S_I2C,               bitrange(7, 6))
REG_FLAG(TunerR20, N_I2C,               bitrange(5, 0))

REG_FLAG(TunerR21, SDM_IN_LSB,          bitrange(7, 0))

REG_FLAG(TunerR22, SDM_IN_MSB,          bitrange(7, 0))

REG_FLAG(TunerR23, PW_LDO_D,            bitrange(7, 6))
REG_FLAG(TunerR23, div_buf_cur,         bitrange(5, 4))
REG_FLAG(TunerR23, OPEN_D,              onebit(3))
REG_FLAG(TunerR23, R23_reserved_2_1,    onebit(2))
REG_FLAG(TunerR23, R23_reserved_1_0,    onebit(1))
REG_FLAG(TunerR23, R23_reserved_0_0,    onebit(0))

REG_FLAG(TunerR24, R24_reserved_7_0,    onebit(7))
REG_FLAG(TunerR24, R24_reserved_6_1,    onebit(6))
REG_FLAG(TunerR24, ring_se23,           onebit(5))
REG_FLAG(TunerR24, pw_ring,             onebit(4))
REG_FLAG(TunerR24, ring_n,              bitrange(3, 0))

REG_FLAG(TunerR25, PW_RFFILT,           onebit(7))
REG_FLAG(TunerR25, rffilt_current,      bitrange(6, 5))
REG_FLAG(TunerR25, SW_AGC,              onebit(4))
REG_FLAG(TunerR25, R25_reserved_3_1,    onebit(3))
REG_FLAG(TunerR25, R25_reserved_2_1,    onebit(2))
REG_FLAG(TunerR25, ring_seldiv,         bitrange(1, 0))

REG_FLAG(TunerR26, RFMUX,               bitrange(7, 6))
REG_FLAG(TunerR26, agc_clock,           bitrange(5, 4))
REG_FLAG(TunerR26, PLL_AUTO_CLK,        bitrange(3, 2))
REG_FLAG(TunerR26, RFFILT,              bitrange(1, 0))

REG_FLAG(TunerR27, TF_NCH,              bitrange(7, 4))
REG_FLAG(TunerR27, TF_LP,               bitrange(3, 0))

REG_FLAG(TunerR28, PDET3_GAIN,          bitrange(7, 4))
REG_FLAG(TunerR28, R28_reserved_3_0,    onebit(3))
REG_FLAG(TunerR28, discharge_mode,      onebit(2))
REG_FLAG(TunerR28, rf_source,           onebit(1))
REG_FLAG(TunerR28, R28_reserved_0_0,    onebit(0))

REG_FLAG(TunerR29, detect_bw,           bitrange(7, 6))
REG_FLAG(TunerR29, PDET1_GAIN,          bitrange(5, 3))
REG_FLAG(TunerR29, PDET2_GAIN,          bitrange(2, 0))

REG_FLAG(TunerR30, sw_pdet,             onebit(7))
REG_FLAG(TunerR30, FILTER_EXT,          onebit(6))
REG_FLAG(TunerR30, PDET_CLK,            bitrange(5, 0))

REG_FLAG(TunerR31, lt_att,              onebit(7))
REG_FLAG(TunerR31, R31_reserved_6_1,    onebit(6))
REG_FLAG(TunerR31, R31_reserved_5_0,    onebit(5))
REG_FLAG(TunerR31, R31_reserved_4_0,    onebit(4))
REG_FLAG(TunerR31, R31_reserved_3_0,    onebit(3))
REG_FLAG(TunerR31, R31_reserved_2_0,    onebit(2))
REG_FLAG(TunerR31, ring_att,            bitrange(1, 0))