#include <gtest/gtest.h>
using namespace std;
#include <tuple> 
#include <iostream>
#include <fstream>

extern "C" {
    #include "internal.h"
}


struct candidate_is_better_test_case {
    string name;
    pll_divisors *current_best;
    pll_divisors *candidate;
    uint32_t min_fcco;
    uint32_t max_fcco;
    bool minimize_error;
    float error_threshold;
    bool expected;
} typedef candidate_is_better_test_case;

TEST(ADCTEST, Test_candidate_is_better) {
    candidate_is_better_test_case test_cases[10] = {
        {
            .name = "candidate actual_fcco below min",
            .current_best = NULL,
            .candidate = new pll_divisors{
                .actual_fcco = 0,
            },
            .min_fcco = 1,
            .max_fcco = 5,
            .minimize_error = false,
            .error_threshold = 0,
            .expected = false
        },
        {
            .name = "candidate actual_fcco above max",
            .current_best = NULL,
            .candidate = new pll_divisors{
                .actual_fcco = 7,
            },
            .min_fcco = 1,
            .max_fcco = 5,
            .minimize_error = false,
            .error_threshold = 0,
            .expected = false
        },
        {
            .name = "candidate not fractional, m < 1",
            .current_best = NULL,
            .candidate = new pll_divisors{
                .fractional = false,
                .m = -1,
                .actual_fcco = 3,
            },
            .min_fcco = 1,
            .max_fcco = 5,
            .minimize_error = false,
            .error_threshold = 0,
            .expected = false
        },
        {
            .name = "candidate fractional, m < fixed_point",
            .current_best = NULL,
            .candidate = new pll_divisors{
                .fractional = true,
                .m = .00000000000000001,
                .actual_fcco = 3,
            },
            .min_fcco = 1,
            .max_fcco = 5,
            .minimize_error = false,
            .error_threshold = 0,
            .expected = false
        },
        {
            .name = "error > error_threshold",
            .current_best = NULL,
            .candidate = new pll_divisors{
                .fractional = false,
                .m = 2,
                .error = 5,
                .actual_fcco = 3,
            },
            .min_fcco = 1,
            .max_fcco = 5,
            .minimize_error = false,
            .error_threshold = 1,
            .expected = false
        },
        {
            .name = "candidate is good, current is null",
            .current_best = NULL,
            .candidate = new pll_divisors{
                .fractional = false,
                .m = 2,
                .error = 1,
                .actual_fcco = 3,
            },
            .min_fcco = 1,
            .max_fcco = 5,
            .minimize_error = false,
            .error_threshold = 2,
            .expected = true
        },
        {
            .name = "candidate error < current best error",
            .current_best = new pll_divisors{
                .error = 5,
            },
            .candidate = new pll_divisors{
                .fractional = false,
                .m = 2,
                .error = 1,
                .actual_fcco = 3,
            },
            .min_fcco = 1,
            .max_fcco = 5,
            .minimize_error = true,
            .error_threshold = 2,
            .expected = true
        },
        {
            .name = "same type, candidate m < current best m",
            .current_best = new pll_divisors{
                .fractional = true,
                .m = 10,
                .error = 5,
            },
            .candidate = new pll_divisors{
                .fractional = false,
                .m = 2,
                .error = 1,
                .actual_fcco = 3,
            },
            .min_fcco = 1,
            .max_fcco = 5,
            .minimize_error = false,
            .error_threshold = 2,
            .expected = true
        },
        {
            .name = "current_best fractional, candidate m <= current best m * 4",
            .current_best = new pll_divisors{
                .fractional = true,
                .m = 4,
                .error = 5,
            },
            .candidate = new pll_divisors{
                .fractional = false,
                .m = 1,
                .error = 1,
                .actual_fcco = 3,
            },
            .min_fcco = 1,
            .max_fcco = 5,
            .minimize_error = false,
            .error_threshold = 2,
            .expected = true
        }, 
        {
            .name = "candidate integer, not candidate m <= current best m * 4",
            .current_best = new pll_divisors{
                .fractional = false,
                .m = 4,
                .error = 5,
            },
            .candidate = new pll_divisors{
                .fractional = true,
                .m = 1,
                .error = 1,
                .actual_fcco = 3,
            },
            .min_fcco = 1,
            .max_fcco = 5,
            .minimize_error = false,
            .error_threshold = 2,
            .expected = false
        }
    };
    
    for (uint16_t cur = 0; cur < sizeof(test_cases)/sizeof(test_cases[0]); cur++) {
        candidate_is_better_test_case t = test_cases[cur];
        printf("Tests %s\n",t.name.c_str());
        EXPECT_EQ(candidate_is_better(t.current_best, t.candidate, t.min_fcco, t.max_fcco, t.minimize_error, t.error_threshold), t.expected);
    }
}

TEST(ADCTEST, Test_calculate_adc_divisor_tables) {

    uint32_t *n_divisors;
    uint32_t *p_divisors;
    uint32_t *i_divisors;
    uint32_t **p_i_divisors_map;
    uint32_t p_i_divisors_map_length;

    uint32_t expected_n_i_divisor_length = 256;
    uint32_t expected_p_divisor_length = 33;
    uint32_t expected_p_i_map_length = 16385; // (32 * 2 * 256) + 1
    uint32_t expected_n_i_divisors[expected_n_i_divisor_length] = {0};
    
    for (uint32_t n = 1; n < expected_n_i_divisor_length; n++) {
        expected_n_i_divisors[n] = n + 1;
    }
    uint32_t expected_p_dividers[expected_p_divisor_length] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 
        18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
    };

    map<uint32_t, tuple<uint32_t,uint32_t>> expected_p_i_divisors_map = {};

    for (uint32_t p = 0; p < expected_p_divisor_length; p++) {
        for (uint32_t i = 0; i < expected_n_i_divisor_length; i++) {
            uint32_t d = effective_p_divisor(p) * effective_i_divisor(i);

            if (expected_p_i_divisors_map.find(d) != expected_p_i_divisors_map.end()) {
                tuple<uint32_t,uint32_t> pair = expected_p_i_divisors_map[d];
                if (i < get<1>(pair)) {
                    expected_p_i_divisors_map[d] = tuple<uint32_t,uint32_t> (p, i);
                }
            } else {
                expected_p_i_divisors_map[d] = tuple<uint32_t,uint32_t> (p, i);
            }
        }
    }

    EXPECT_EQ(calculate_adc_divisor_tables(&n_divisors, &p_divisors, &i_divisors, &p_i_divisors_map, &p_i_divisors_map_length), LPCSDR_SUCCESS);
    EXPECT_EQ(p_i_divisors_map_length, expected_p_i_map_length);
    for (uint32_t n = 0; n < expected_p_divisor_length; n++){
        EXPECT_EQ(p_divisors[n], expected_p_dividers[n]);
    }
    for (uint32_t index = 0; index < expected_n_i_divisor_length; index++){
        EXPECT_EQ(n_divisors[index], expected_n_i_divisors[index]);
        EXPECT_EQ(i_divisors[index], expected_n_i_divisors[index]);
    }
    for (uint32_t index = 0; index < expected_p_i_map_length; index++) {
        uint32_t *pair = p_i_divisors_map[index];
         if (expected_p_i_divisors_map.find(index) != expected_p_i_divisors_map.end()) {
            tuple<uint32_t, uint32_t> expected_pair = expected_p_i_divisors_map[index];
            EXPECT_EQ(get<0>(expected_pair), pair[0]);
            EXPECT_EQ(get<1>(expected_pair), pair[1]);
         }
    }
}

TEST(ADCTEST, Test_calculate_adc_clock_divisors) {
    
    EXPECT_EQ(init_global_adc_divisor_tables(), LPCSDR_SUCCESS);
    uint32_t target_frequency = 5200000; //hz

    pll_divisors *int_divisors;
    EXPECT_EQ(calculate_adc_clock_divisors(target_frequency, &int_divisors, false, false, NULL), LPCSDR_SUCCESS);
    EXPECT_EQ(int_divisors->error, 0);
    EXPECT_EQ(int_divisors->i, 0);
    EXPECT_EQ(int_divisors->m, 13);
    EXPECT_EQ(int_divisors->n, 0);
    EXPECT_EQ(int_divisors->p, 30);
    EXPECT_EQ(int_divisors->actual_frequency, (float) target_frequency);

    pll_divisors *frac_divisors;
    EXPECT_EQ(calculate_adc_clock_divisors(target_frequency, &frac_divisors, false, true, NULL), LPCSDR_SUCCESS);
    EXPECT_EQ(frac_divisors->error, 0);
    EXPECT_EQ(frac_divisors->i, 0);
    EXPECT_EQ(frac_divisors->m, 13);
    EXPECT_EQ(frac_divisors->n, 0);
    EXPECT_EQ(frac_divisors->p, 30);
    EXPECT_EQ(frac_divisors->actual_frequency, (float) target_frequency);
}

TEST(Test_populate_new_current_best, Successful) {
    pll_divisors *b = NULL;
    pll_divisors c = {
        .fractional = true,

        .n = 5,
        .m = 3,
        .p = 4,
        .i = 2,

        .error = 1,
        .actual_fcco = 200,
        .actual_frequency = 300,

    };
    EXPECT_EQ(populate_new_current_best(&b, &c), LPCSDR_SUCCESS);
    EXPECT_EQ(b->error, c.error);
    EXPECT_EQ(b->actual_fcco, c.actual_fcco);
    EXPECT_EQ(b->actual_frequency, c.actual_frequency);
    EXPECT_EQ(b->fractional, c.fractional);
    EXPECT_EQ(b->i, c.i);
    EXPECT_EQ(b->m, c.m);
    EXPECT_EQ(b->p, c.p);
    EXPECT_EQ(b->n, c.n);
}

TEST(Test_unpack_raw_adc_data, Successful) {
    uint16_t buffer_length = 32;
    uint8_t buffer[buffer_length] = {
        // header data in little endian
        // Magic
        239, 190, 173, 222, 
        //block len
        32, 0, 0, 0, 
        // num samples per block
        8, 0, 0, 0, 
        // sequence
        1, 0, 0, 0, 
        // status
        1, 0, 0, 0, 
        // 8 12 bit samples spread over 12 bytes (1 word = 32 bits = 4 bytes)
        2, 0, 2, 0,
        4, 0, 5, 0,
        3,96, 3,80
    };

    lpcsdr_device_handle h = {
        .usb_samples_per_block_multiple = 8,
        .usb_bytes_per_block_multiple = 32,
        .individual_sample_bit_size = 12,
    };

    int16_t *out = (int16_t *) calloc(buffer_length, sizeof(uint16_t));
    uint32_t out_length;

    int return_status = unpack_raw_adc_data(&h, buffer, buffer_length, out, 0, NULL);
    int16_t expected_unpacked_samples_length = 8;
    int16_t expected_unpacked_samples[expected_unpacked_samples_length] = {32, 32, 64, 80, 48, 48, 96, 80, 48};
    EXPECT_EQ(return_status, 8);

    for (int i = 0; i < expected_unpacked_samples_length; i++) {
        EXPECT_EQ(out[i], expected_unpacked_samples[i]);
    }
}
