#include <stdint.h>

typedef struct lpf_settings {
    float cutoff;
    int lpf_coarse;
    int lpf_fine;
    int lpf_q;
    int lpf_narrow;
} lpf_settings;

typedef struct hpf_settings {
    float cutoff;
    int hpf_corner;
} hpf_settings;

#define TUNER_REG_COUNT 32

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

typedef enum {
    // TunerR5 
    PWD_LT,
    reserved_6_0,
    PWD_LNA1,
    LNA_GAIN_MODE,
    LNA_GAIN,

    // TunerR6
    PWD_PDET1,
    PWD_PDET2,
    FILT_3DB,
    reserved_4_1,
    reserved_3_0,
    PW_LNA,

    // TunerR7
    img_r,
    PW_MIX,
    PW0_MIX,
    MIXGAIN_MODE,
    MIX_GAIN
} REGISTER_SYMBOL;

typedef struct {
    REGISTER_SYMBOL symbol;
    uint32_t bit_mask;
} symbol_bit_mask_tuple;

typedef struct bit_flag {
    tuner_reg_num num;
    unsigned symbol_count;
    symbol_bit_mask_tuple **symbols;
} bit_flag;

typedef struct bit_flag_metadata {
    tuner_reg_num num;
    unsigned symbol_count;
    symbol_bit_mask_tuple *symbols;
} bit_flag_metadata;

typedef struct change_set_value {
    uint8_t current_value;
    uint8_t current_mask;
} change_set_value;

typedef struct change_set {
    change_set_value **entries;
    unsigned int entries_count;
} change_set;

lpf_settings lpf_settings_for(int target, int *max);
hpf_settings hpf_settings_for(int target);
uint32_t onebit(uint32_t x);
uint32_t bitrange(uint32_t x, uint32_t y);

// Tuner Register
int init_tuner(lpcsdr_device_handle *handle);
int init_tuner_registers(bit_flag ***out);
int create_tuner_register(tuner_reg_num reg_num, unsigned symbol_count, symbol_bit_mask_tuple *symbols, bit_flag **out);
int set_tuner_value(lpcsdr_device_handle *handle, change_set *cs, tuner_reg_num reg, REGISTER_SYMBOL symbol, unsigned int value);
int prepare_tuner_payload_from_change_set(change_set *cs, uint16_t *first, uint8_t **payload, uint16_t *payload_size);
int free_registers(bit_flag **registers, unsigned registers_count);

// change_set
int create_change_set(change_set **out);
int free_change_set(change_set *cs);