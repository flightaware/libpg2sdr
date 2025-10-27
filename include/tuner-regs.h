#ifndef PG2SDR_TUNER_REGS_H
#define PG2SDR_TUNER_REGS_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

#define TUNER_REG_COUNT 32
#define TUNER_REG_MAX_PAYLOAD_SIZE (27 * 2) // We can only write to 27 registers.

typedef struct {
    uint8_t current_value;
    uint8_t current_mask;
} change_set_value;

typedef struct {
    change_set_value entries[TUNER_REG_COUNT];
} change_set;

#define REG_BIT(reg, name, mask) \
  static const uint8_t name ## _REG = (reg); \
  static const uint8_t name ## _MASK = (onebit(mask)); \
  static const unsigned name ## _OFFSET = (mask)

#define REG_BITS(reg, name, msb, lsb) \
  static const uint8_t name ## _REG = (reg); \
  static const uint8_t name ## _MASK = (bitrange(msb, lsb)); \
  static const unsigned name ## _OFFSET = (lsb)

#define onebit(x) (1 << x)
#define bitrange(msb, lsb) (((2<<msb)-1) ^ ((1<<lsb)-1))

#define set_tuner_bits(cs, name, value) _set_tuner_bits_helper((cs), name ## _REG, name ## _MASK, (value) << name ## _OFFSET)
#define read_tuner_bits(dev, name) pg2sdr__read_tuner_bits((dev), name ## _REG, name ## _MASK, name ## _OFFSET)

void pg2sdr__prepare_tuner_payload_from_change_set(change_set *cs, uint16_t *first, uint8_t *out, uint16_t *out_size);
int pg2sdr__read_tuner_bits(pg2sdr_device_handle *dev, uint8_t reg, uint8_t mask, unsigned offset);

/* Make this static-inline to ensure it's visible to the compiler at the point of use -- in most cases
 * it can be optimized down to some simple bit twiddling (or, in the extreme case of pg2sdr__set_initial_values,
 * constant initialization of the changeset entries)
 */
static inline void _set_tuner_bits_helper(change_set *cs, unsigned reg, unsigned mask, unsigned value)
{
    cs->entries[reg].current_mask |= mask;
    cs->entries[reg].current_value = (cs->entries[reg].current_value & ~mask) | (value & mask);
}


/* register definitions */

REG_BITS(0, TUNER_ID,             7, 0);

REG_BITS(1, DET3_ADC,             5, 0);

REG_BIT (2, PLL_LOCK,             6);
REG_BITS(2, VCO_ADC,              5, 0);

REG_BITS(3, MIX_ADC,              7, 4);
REG_BITS(3, LNA_ADC,              3, 0);

REG_BITS(4, VCO_FINE_TUNE,        5, 4);
REG_BITS(4, FILT_CAL_CODE,        3, 0);

REG_BIT (5, PWD_LT,               7);
REG_BIT (5, R5_RESERVED_6_0,      6);
REG_BIT (5, PWD_LNA1,             5);
REG_BIT (5, LNA_GAIN_MODE,        4);
REG_BITS(5, LNA_GAIN,             3, 0);

REG_BIT (6, PWD_PDET1,            7);
REG_BIT (6, PWD_PDET2,            6);
REG_BIT (6, FILT_3DB,             5);
REG_BIT (6, R6_RESERVED_4_1,      4);
REG_BIT (6, R6_RESERVED_3_0,      3);
REG_BITS(6, PW_LNA,               2, 0);

REG_BIT (7, IMG_R,                7);
REG_BIT (7, PW_MIX,               6);
REG_BIT (7, PW0_MIX,              5);
REG_BIT (7, MIXGAIN_MODE,         4);
REG_BITS(7, MIX_GAIN,             3, 0);

REG_BIT (8, PW_AMP,               7);
REG_BIT (8, PW0_AMP,              6);
REG_BIT (8, IMR_G_PATH,           5);
REG_BITS(8, IMR_G,                4, 0);

REG_BIT (9, PWD_IFFILT,           7);
REG_BIT (9, PW1_IFFILT,           6);
REG_BIT (9, IMR_P_PATH,           5);
REG_BITS(9, IMR_P,                4, 0);

REG_BIT (10, PW_FILT,             7);
REG_BITS(10, FILTER_CUR,          6, 5);
REG_BIT (10, IFFILT_Q,            4);
REG_BITS(10, IFFILT_FINE_LPF,     3, 0);

REG_BIT (11, IFFILT_NARROW,       7);
REG_BITS(11, IFFILT_COARSE_LPF,   6, 5);
REG_BIT (11, CALIBRATION_TRIGGER, 4);
REG_BITS(11, IFFILT_HPF_CORNER,   3, 0);

REG_BIT (12, PWD_ADC,             7);
REG_BIT (12, PW_VGA,              6);
REG_BIT (12, R12_RESERVED_5_1,    5);
REG_BIT (12, VGA_GAIN_MODE,       4);
REG_BITS(12, VGA_GAIN,            3, 0);

REG_BITS(13, LNA_VTH_H,           7, 4);
REG_BITS(13, LNA_VTH_L,           3, 0);

REG_BITS(14, MIX_VTH_H,           7, 4);
REG_BITS(14, MIX_VTH_L,           3, 0);

REG_BIT (15, FLT_EXT_WIDEST,      7);
REG_BIT (15, R15_RESERVED_6_0,    6);
REG_BIT (15, R15_RESERVED_5_1,    5);
REG_BIT (15, CLK_OUT_DIS,         4);
REG_BIT (15, RING_DISABLE,        3);
REG_BIT (15, R15_RESERVED_2_0,    2);
REG_BIT (15, CLK_AGC_DIS,         1);
REG_BIT (15, R15_RESERVED_0_0,    0);

REG_BITS(16, SEL_DIV,             7, 5);
REG_BIT (16, REF_DIV2,            4);
REG_BIT (16, XTAL_DRIVE,          3);
REG_BIT (16, DET1_CAP,            2);
REG_BITS(16, CAPX,                1, 0);

REG_BITS(17, PW_LDO_A,            7, 6);
REG_BITS(17, CP_CURRENT,          5, 3);
REG_BIT (17, R17_RESERVED_2_0,    2);
REG_BIT (17, R17_RESERVED_1_0,    1);
REG_BIT (17, R17_RESERVED_0_0,    0);

REG_BITS(18, VCO_CURRENT,         7, 5);
REG_BIT (18, SDM_DITHER_DIS,      4);
REG_BIT (18, PWD_SDM,             3);
REG_BIT (18, R18_RESERVED_2_0,    2);
REG_BIT (18, R18_RESERVED_1_0,    1);
REG_BIT (18, R18_RESERVED_0_0,    0);

REG_BIT (19, R18_RESERVED_7_0,    7);
REG_BIT (19, VCO_MODE,            6);
REG_BITS(19, VCO_DAC,             5, 0);

REG_BITS(20, S_I2C,               7, 6);
REG_BITS(20, N_I2C,               5, 0);

REG_BITS(21, SDM_IN_LSB,          7, 0);

REG_BITS(22, SDM_IN_MSB,          7, 0);

REG_BITS(23, PW_LDO_D,            7, 6);
REG_BITS(23, DIV_BUF_CUR,         5, 4);
REG_BIT (23, OPEN_D,              3);
REG_BIT (23, R23_RESERVED_2_1,    2);
REG_BIT (23, R23_RESERVED_1_0,    1);
REG_BIT (23, R23_RESERVED_0_0,    0);

REG_BIT (24, R24_RESERVED_7_0,    7);
REG_BIT (24, R24_RESERVED_6_1,    6);
REG_BIT (24, RING_SE23,           5);
REG_BIT (24, PW_RING,             4);
REG_BITS(24, RING_N,              3, 0);

REG_BIT (25, PW_RFFILT,           7);
REG_BITS(25, RFFILT_CURRENT,      6, 5);
REG_BIT (25, SW_AGC,              4);
REG_BIT (25, R25_RESERVED_3_1,    3);
REG_BIT (25, R25_RESERVED_2_1,    2);
REG_BITS(25, RING_SELDIV,         1, 0);

REG_BITS(26, RFMUX,               7, 6);
REG_BITS(26, AGC_CLOCK,           5, 4);
REG_BITS(26, PLL_AUTO_CLK,        3, 2);
REG_BITS(26, RFFILT,              1, 0);

REG_BITS(27, TF_NCH,              7, 4);
REG_BITS(27, TF_LP,               3, 0);

REG_BITS(28, PDET3_GAIN,          7, 4);
REG_BIT (28, R28_RESERVED_3_0,    3);
REG_BIT (28, DISCHARGE_MODE,      2);
REG_BIT (28, RF_SOURCE,           1);
REG_BIT (28, R28_RESERVED_0_0,    0);

REG_BITS(29, DETECT_BW,           7, 6);
REG_BITS(29, PDET1_GAIN,          5, 3);
REG_BITS(29, PDET2_GAIN,          2, 0);

REG_BIT (30, SW_PDET,             7);
REG_BIT (30, FILTER_EXT,          6);
REG_BITS(30, PDET_CLK,            5, 0);

REG_BIT (31, LT_ATT,              7);
REG_BIT (31, R31_RESERVED_6_1,    6);
REG_BIT (31, R31_RESERVED_5_0,    5);
REG_BIT (31, R31_RESERVED_4_0,    4);
REG_BIT (31, R31_RESERVED_3_0,    3);
REG_BIT (31, R31_RESERVED_2_0,    2);
REG_BITS(31, RING_ATT,            1, 0);

#if defined(__cplusplus)
}
#endif

#endif /* PG2SDR_TUNER_REGS_H */
