#include <stdint.h>

#define onebit(x) (1 << x)
#define bitrange(msb, lsb) (((2<<msb)-1) ^ ((1<<lsb)-1))
#define TUNER_REG_COUNT 32
#define TUNER_REG_MAX_PAYLOAD_SIZE 27 * 2 // We can only write to 27 registers.

#define REG_BIT(reg, name, mask) \
  static const uint8_t name ## _REG = (reg); \
  static const uint8_t name ## _MASK = (onebit(mask)); \
  static const uint8_t name ## _OFFSET = (mask);

#define REG_BITS(reg, name, msb, lsb) \
  static const uint8_t name ## _REG = (reg); \
  static const uint8_t name ## _MASK = (bitrange(msb, lsb)); \
  static const uint8_t name ## _OFFSET = (lsb);

#define set_tuner_reg(cs, name, value) \
    lpcsdr__set_tuner_value_in_change_set(cs, \
    name ## _REG, \
    name ## _MASK, \
    (value) << name ## _OFFSET)

#define extract_tuner_val_helper(buffer, mask, offset) ((buffer & mask) >> offset)
#define extract_tuner_val(buffer, name) extract_tuner_val_helper(buffer, name ## _MASK, name ## _OFFSET)

typedef struct {
    bool valid;
    float cutoff;
    int lpf_coarse;
    int lpf_fine;
    int lpf_q;
    int lpf_narrow;
} lpf_settings;

typedef struct {
    bool valid;
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
    bool valid;

    bool refdiv;
    int seldiv;
    int feedback_n;
    int feedback_sdm;

    double actual_vco;
    double actual_frequency;
} tuner_pll_config_t;

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

const lpf_settings *lpcsdr__lpf_settings_for(double target, double max);
const hpf_settings *lpcsdr__hpf_settings_for(double target);
int lpcsdr__tuner_set_bandpass(lpcsdr_device_handle *dev, const hpf_settings *hpf, const lpf_settings *lpf);

int lpcsdr__init_tuner(lpcsdr_device_handle *dev);
int lpcsdr__set_initial_values(lpcsdr_device_handle *dev);
int lpcsdr__start_pll(lpcsdr_device_handle *dev, tuner_pll_config_t *params);
int lpcsdr__find_pll_parameters(double requested, double xtal, tuner_pll_config_t *out);
int lpcsdr__has_pll_lock(lpcsdr_device_handle *dev);
int lpcsdr__configure_pll_settings(lpcsdr_device_handle *dev, tuner_pll_config_t *params);

void lpcsdr__prepare_tuner_payload_from_change_set(change_set *cs, uint16_t *first, uint8_t *out, uint16_t *out_size);
int lpcsdr__set_tuner_value_in_change_set(change_set *cs, uint8_t reg, uint8_t mask, uint8_t value);
int lpcsdr__vco_scan(lpcsdr_device_handle *dev);

REG_BITS(TunerR0, TUNER_ID,             7, 0)

REG_BITS(TunerR1, DET3_ADC,             5, 0)

REG_BIT (TunerR2, PLL_LOCK,             6)
REG_BITS(TunerR2, VCO_ADC,              5, 0)

REG_BITS(TunerR3, MIX_ADC,              7, 4)
REG_BITS(TunerR3, LNA_ADC,              3, 0)

REG_BITS(TunerR4, VCO_FINE_TUNE,        5, 4)
REG_BITS(TunerR4, FILT_CAL_CODE,        3, 0)

REG_BIT (TunerR5, PWD_LT,               7)
REG_BIT (TunerR5, R5_RESERVED_6_0,      6)
REG_BIT (TunerR5, PWD_LNA1,             5)
REG_BIT (TunerR5, LNA_GAIN_MODE,        4)
REG_BITS(TunerR5, LNA_GAIN,             3, 0)

REG_BIT (TunerR6, PWD_PDET1,            7)
REG_BIT (TunerR6, PWD_PDET2,            6)
REG_BIT (TunerR6, FILT_3DB,             5)
REG_BIT (TunerR6, R6_RESERVED_4_1,      4)
REG_BIT (TunerR6, R6_RESERVED_3_0,      3)
REG_BITS(TunerR6, PW_LNA,               2, 0)

REG_BIT (TunerR7, IMG_R,                7)
REG_BIT (TunerR7, PW_MIX,               6)
REG_BIT (TunerR7, PW0_MIX,              5)
REG_BIT (TunerR7, MIXGAIN_MODE,         4)
REG_BITS(TunerR7, MIX_GAIN,             3, 0)

REG_BIT (TunerR8, PW_AMP,               7)
REG_BIT (TunerR8, PW0_AMP,              6)
REG_BIT (TunerR8, IMR_G_PATH,           5)
REG_BITS(TunerR8, IMR_G,                4, 0)

REG_BIT (TunerR9, PWD_IFFILT,           7)
REG_BIT (TunerR9, PW1_IFFILT,           6)
REG_BIT (TunerR9, IMR_P_PATH,           5)
REG_BITS(TunerR9, IMR_P,                4, 0)

REG_BIT (TunerR10, PW_FILT,             7)
REG_BITS(TunerR10, FILTER_CUR,          6, 5)
REG_BIT (TunerR10, IFFILT_Q,            4)
REG_BITS(TunerR10, IFFILT_FINE_LPF,     3, 0)

REG_BIT (TunerR11, IFFILT_NARROW,       7)
REG_BITS(TunerR11, IFFILT_COARSE_LPF,   6, 5)
REG_BIT (TunerR11, CALIBRATION_TRIGGER, 4)
REG_BITS(TunerR11, IFFILT_HPF_CORNER,   3, 0)

REG_BIT (TunerR12, PWD_ADC,             7)
REG_BIT (TunerR12, PW_VGA,              6)
REG_BIT (TunerR12, R12_RESERVED_5_1,    5)
REG_BIT (TunerR12, VGA_GAIN_MODE,       4)
REG_BITS(TunerR12, VGA_GAIN,            3, 0)

REG_BITS(TunerR13, LNA_VTH_H,           7, 4)
REG_BITS(TunerR13, LNA_VTH_L,           3, 0)

REG_BITS(TunerR14, MIX_VTH_H,           7, 4)
REG_BITS(TunerR14, MIX_VTH_L,           3, 0)

REG_BIT (TunerR15, FLT_EXT_WIDEST,      7)
REG_BIT (TunerR15, R15_RESERVED_6_0,    6)
REG_BIT (TunerR15, R15_RESERVED_5_1,    5)
REG_BIT (TunerR15, CLK_OUT_DIS,         4)
REG_BIT (TunerR15, RING_DISABLE,        3)
REG_BIT (TunerR15, R15_RESERVED_2_0,    2)
REG_BIT (TunerR15, CLK_AGC_DIS,         1)
REG_BIT (TunerR15, R15_RESERVED_0_0,    0)

REG_BITS(TunerR16, SEL_DIV,             7, 5)
REG_BIT (TunerR16, REF_DIV2,            4)
REG_BIT (TunerR16, XTAL_DRIVE,          3)
REG_BIT (TunerR16, DET1_CAP,            2)
REG_BITS(TunerR16, CAPX,                1, 0)

REG_BITS(TunerR17, PW_LDO_A,            7, 6)
REG_BITS(TunerR17, CP_CURRENT,          5, 3)
REG_BIT (TunerR17, R17_RESERVED_2_0,    2)
REG_BIT (TunerR17, R17_RESERVED_1_0,    1)
REG_BIT (TunerR17, R17_RESERVED_0_0,    0)

REG_BITS(TunerR18, VCO_CURRENT,         7, 5)
REG_BIT (TunerR18, SDM_DITHER_DIS,      4)
REG_BIT (TunerR18, PWD_SDM,             3)
REG_BIT (TunerR18, R18_RESERVED_2_0,    2)
REG_BIT (TunerR18, R18_RESERVED_1_0,    1)
REG_BIT (TunerR18, R18_RESERVED_0_0,    0)

REG_BIT (TunerR19, R18_RESERVED_7_0,    7)
REG_BIT (TunerR19, VCO_MODE,            6)
REG_BITS(TunerR19, VCO_DAC,             5, 0)

REG_BITS(TunerR20, S_I2C,               7, 6)
REG_BITS(TunerR20, N_I2C,               5, 0)

REG_BITS(TunerR21, SDM_IN_LSB,          7, 0)

REG_BITS(TunerR22, SDM_IN_MSB,          7, 0)

REG_BITS(TunerR23, PW_LDO_D,            7, 6)
REG_BITS(TunerR23, DIV_BUF_CUR,         5, 4)
REG_BIT (TunerR23, OPEN_D,              3)
REG_BIT (TunerR23, R23_RESERVED_2_1,    2)
REG_BIT (TunerR23, R23_RESERVED_1_0,    1)
REG_BIT (TunerR23, R23_RESERVED_0_0,    0)

REG_BIT (TunerR24, R24_RESERVED_7_0,    7)
REG_BIT (TunerR24, R24_RESERVED_6_1,    6)
REG_BIT (TunerR24, RING_SE23,           5)
REG_BIT (TunerR24, PW_RING,             4)
REG_BITS(TunerR24, RING_N,              3, 0)

REG_BIT (TunerR25, PW_RFFILT,           7)
REG_BITS(TunerR25, RFFILT_CURRENT,      6, 5)
REG_BIT (TunerR25, SW_AGC,              4)
REG_BIT (TunerR25, R25_RESERVED_3_1,    3)
REG_BIT (TunerR25, R25_RESERVED_2_1,    2)
REG_BITS(TunerR25, RING_SELDIV,         1, 0)

REG_BITS(TunerR26, RFMUX,               7, 6)
REG_BITS(TunerR26, AGC_CLOCK,           5, 4)
REG_BITS(TunerR26, PLL_AUTO_CLK,        3, 2)
REG_BITS(TunerR26, RFFILT,              1, 0)

REG_BITS(TunerR27, TF_NCH,              7, 4)
REG_BITS(TunerR27, TF_LP,               3, 0)

REG_BITS(TunerR28, PDET3_GAIN,          7, 4)
REG_BIT (TunerR28, R28_RESERVED_3_0,    3)
REG_BIT (TunerR28, DISCHARGE_MODE,      2)
REG_BIT (TunerR28, RF_SOURCE,           1)
REG_BIT (TunerR28, R28_RESERVED_0_0,    0)

REG_BITS(TunerR29, DETECT_BW,           7, 6)
REG_BITS(TunerR29, PDET1_GAIN,          5, 3)
REG_BITS(TunerR29, PDET2_GAIN,          2, 0)

REG_BIT (TunerR30, SW_PDET,             7)
REG_BIT (TunerR30, FILTER_EXT,          6)
REG_BITS(TunerR30, PDET_CLK,            5, 0)

REG_BIT (TunerR31, LT_ATT,              7)
REG_BIT (TunerR31, R31_RESERVED_6_1,    6)
REG_BIT (TunerR31, R31_RESERVED_5_0,    5)
REG_BIT (TunerR31, R31_RESERVED_4_0,    4)
REG_BIT (TunerR31, R31_RESERVED_3_0,    3)
REG_BIT (TunerR31, R31_RESERVED_2_0,    2)
REG_BITS(TunerR31, RING_ATT,            1, 0)