#include "internal.h"

uint32_t onebit(uint32_t x) {
    return (1<<x);
}

uint32_t bitrange(uint32_t x, uint32_t y) {
    if (x > y) {
        uint32_t t = x;
        x = y;
        y = t;
    }
    return ((2<<y)-1) ^ ((1<<x)-1);
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

int init_tuner(lpcsdr_device_handle *handle) {
    int error = LPCSDR_SUCCESS;
    handle->registers_count = TUNER_REG_COUNT;

    if ((error = init_tuner_registers(&handle->registers)) < 0)
        goto failed;

    if ((error = lpcsdr_set_rf_power(handle, 1)) < 0)
        goto failed;

    return error;

failed:
    return error;
}

int init_tuner_registers(bit_flag ***out) {
    symbol_bit_mask_tuple tuner_r5_symbols[5] = {
        {
            .symbol = PWD_LT,
            .bit_mask = onebit(7)
        },
        {
            .symbol = reserved_6_0,
            .bit_mask= onebit(6)
        },
        {
            .symbol = PWD_LNA1,
            .bit_mask = onebit(5)
        },
        {
            .symbol = LNA_GAIN_MODE,
            .bit_mask = onebit(4)
        },
        {
            .symbol = LNA_GAIN,
            .bit_mask = bitrange(3,0)
        }
    };

    symbol_bit_mask_tuple tuner_r6_symbols[6] = {
        {
            .symbol = PWD_PDET1,
            .bit_mask = onebit(7)
        },
        {
            .symbol = PWD_PDET2,
            .bit_mask= onebit(6)
        },
        {
            .symbol = FILT_3DB,
            .bit_mask = onebit(5)
        },
        {
            .symbol = reserved_4_1,
            .bit_mask = onebit(4)
        },
        {
            .symbol = reserved_3_0,
            .bit_mask = onebit(3)
        },
        {
            .symbol = PW_LNA,
            .bit_mask = bitrange(2,0)
        }
    };

    symbol_bit_mask_tuple tuner_r7_symbols[5] = {
        {
            .symbol = img_r,
            .bit_mask = onebit(7)
        },
        {
            .symbol = PW_MIX,
            .bit_mask= onebit(6)
        },
        {
            .symbol = PW0_MIX,
            .bit_mask = onebit(5)
        },
        {
            .symbol = MIXGAIN_MODE,
            .bit_mask = onebit(4)
        },
        {
            .symbol = MIX_GAIN,
            .bit_mask = bitrange(3,0)
        }
    };

    bit_flag_metadata register_arr[3] = {
        {   
            .num = TunerR5,
            .symbol_count = sizeof(tuner_r5_symbols)/sizeof(tuner_r5_symbols[0]),
            .symbols = tuner_r5_symbols,
        },
        {   
            .num = TunerR6,
            .symbol_count = sizeof(tuner_r6_symbols)/sizeof(tuner_r6_symbols[0]),
            .symbols = tuner_r6_symbols,
        },
        {   
            .num = TunerR7,
            .symbol_count = sizeof(tuner_r7_symbols)/sizeof(tuner_r7_symbols[0]),
            .symbols = tuner_r7_symbols,
        }
    };

    int error = LPCSDR_SUCCESS;
    bit_flag **registers = NULL;

    if (!(registers = calloc(TUNER_REG_COUNT, sizeof(bit_flag*)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto failed;
    }

    for (int i = 0; i < sizeof(register_arr)/sizeof(register_arr[0]);i++) {
        bit_flag *reg;
        if ((error = create_tuner_register(register_arr[i].num, register_arr[i].symbol_count, register_arr[i].symbols, &reg)) < 0) {
            goto failed;
        }
        registers[register_arr[i].num] = reg;
    }

    *out = registers;
    return error;

failed:
    if (registers) {
        for (int i = 0; i < TUNER_REG_COUNT; i++) {
            if (registers[i])
                free(registers[i]);
        }
        free(registers);
    }
    return error;
}

int create_tuner_register(tuner_reg_num reg_num, unsigned symbol_count, symbol_bit_mask_tuple *symbols, bit_flag **out) {
    int error = LPCSDR_SUCCESS;
    bit_flag *reg = NULL;
    symbol_bit_mask_tuple **s = NULL;

    if (!(reg = calloc(1, sizeof(bit_flag)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto failed;
    }
    reg->num = reg_num;
    reg->symbol_count = symbol_count;

    if (!(s = calloc(reg->symbol_count, sizeof(*s)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto failed;
    }

    for (int i = 0; i < reg->symbol_count; i++) {
        if (!(s[i] = calloc(1, sizeof(*s[i])))) {
            error = LPCSDR_ERROR_NO_MEMORY;
            goto failed;
        }
        s[i]->symbol = symbols->symbol;
        s[i]->bit_mask = symbols->bit_mask;
        symbols++;
    }

    reg->symbols = s;
    *out = reg;

    return error;
failed:
    if (reg)
        free(reg);
    if (s) {
        for (int i = 0; i < symbol_count; i++) {
            if (s[i])
                free(s[i]);
        }
        free(s);
    }

    return error;
}

int free_registers(bit_flag **registers, unsigned registers_count) {

    if (!registers)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    for (unsigned i = 0; i < registers_count; i++) {
        for (unsigned x = 0; x < registers[i]->symbol_count; x++) {
            free(registers[i]->symbols[x]);
        }
        free(registers[i]->symbols);
        free(registers[i]);
    }
    free(registers);

    return LPCSDR_SUCCESS;
}

/*
    Use reg to index into the handles->register array.
    Search for the desired REGISTER_SYMBOL and update the change_set value + mask.
*/
int set_tuner_value(lpcsdr_device_handle *handle, change_set *cs, tuner_reg_num reg, REGISTER_SYMBOL symbol, unsigned int value) {
    bit_flag *r = handle->registers[reg];
    for (int i = 0; i < r->symbol_count; i++) {
        if (r->symbols[i]->symbol == symbol) {
            uint8_t mask = cs->entries[reg]->current_mask;
            cs->entries[reg]->current_mask = mask | r->symbols[i]->bit_mask;
            cs->entries[reg]->current_value = (cs->entries[reg]->current_value & ~mask) | value;
            return LPCSDR_SUCCESS;
        }
    }

    return LPCSDR_TUNER_REGISTER_SYMBOL_NOT_FOUND;
}

/*
    Create a payload of bytes from the changeset.
 */
int prepare_tuner_payload_from_change_set(change_set *cs, uint16_t *first, uint8_t **out, uint16_t *out_size) {
    uint16_t num_entries = 0;
    uint16_t first_entry = 5;
    uint16_t last_entry = 5;
    
    for (uint16_t i = 5; i < cs->entries_count; i++) {
        if (cs->entries[i]->current_mask != 0) {
            num_entries++;
            if (first_entry >= i)
                first_entry = i;
            if (last_entry < i)
                last_entry = i;
        }
    }

    if (num_entries == 0) {
        return LPCSDR_SUCCESS;
    }

    unsigned count = last_entry - first_entry + 1;
    uint8_t *payload = calloc(count * 2, sizeof(uint8_t));

    for (int i = first_entry; i <= last_entry; i++) {
        /* Read and clear value + mask */
        unsigned offset = i - first_entry;
        payload[offset] = cs->entries[i]->current_value;
        payload[offset + count] = cs->entries[i]->current_mask;

        cs->entries[i]->current_mask = 0;
        cs->entries[i]->current_value = 0;
    }

    *first = first_entry;
    *out = payload;
    *out_size = count * 2;

    return LPCSDR_SUCCESS;
}

int create_change_set(change_set **out) {
    int error = LPCSDR_SUCCESS;
    change_set *cs = NULL;
    change_set_value **entries = NULL;
    
    if (!(cs = calloc(1, sizeof(change_set)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto failed;
    }
    cs->entries_count = TUNER_REG_COUNT;
    if (!(entries = calloc(TUNER_REG_COUNT, sizeof(*entries)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto failed;
    }

    for (int i = 0; i < TUNER_REG_COUNT; i++) {
        if (!(entries[i] = calloc(1, sizeof(change_set_value)))){
            error = LPCSDR_ERROR_NO_MEMORY;
            goto failed;
        }
    }

    cs->entries = entries;
    *out = cs;

    return LPCSDR_SUCCESS;

failed:
    if (cs)
        free(cs);
    if (entries) {
        for (int i = 0; i < TUNER_REG_COUNT; i++) {
            if (entries[i])
                free(entries[i]);
        }
    }
    return error;
}

void free_change_set(change_set *cs) {
    for (int i = 0; i < TUNER_REG_COUNT; i++) {
        free(cs->entries[i]);
    }
    free(cs->entries);
    free(cs);
}
