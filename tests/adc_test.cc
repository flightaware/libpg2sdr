#include <gtest/gtest.h>
#include <tuple> 
#include <iostream>
#include <fstream>
using namespace testing;
using namespace std;

extern "C" {
    #include "internal/lib.h"
}


struct candidate_is_better_test_case {
    string name;
    adc_pll_config_t current_best;
    adc_pll_config_t candidate;
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
            .current_best = {},
            .candidate = {
                .valid = true,
                .actual_fcco = adc_min_fcco - 1e6,
            },
            .minimize_error = false,
            .error_threshold = 0,
            .expected = false
        },
        {
            .name = "candidate actual_fcco above max",
            .current_best = {},
            .candidate = {
                .valid = true,
                .actual_fcco = adc_max_fcco + 1e6,
            },
            .minimize_error = false,
            .error_threshold = 0,
            .expected = false
        },
        {
            .name = "candidate not fractional, m < 1",
            .current_best = {},
            .candidate = {
                .valid = true,
                .fractional = false,
                .m = -1,
                .actual_fcco = adc_min_fcco,
            },
            .minimize_error = false,
            .error_threshold = 0,
            .expected = false
        },
        {
            .name = "candidate fractional, m < fixed_point",
            .current_best = {},
            .candidate = {
                .valid = true,
                .fractional = true,
                .m = .00000000000000001,
                .actual_fcco = adc_min_fcco,
            },
            .minimize_error = false,
            .error_threshold = 0,
            .expected = false
        },
        {
            .name = "error > error_threshold",
            .current_best = {},
            .candidate = {
                .valid = true,
                .fractional = false,
                .m = 2,
                .error = 5,
                .actual_fcco = adc_min_fcco,
            },
            .minimize_error = false,
            .error_threshold = 1,
            .expected = false
        },
        {
            .name = "candidate is good, current is null",
            .current_best = {},
            .candidate = {
                .valid = true,
                .fractional = false,
                .m = 2,
                .error = 1,
                .actual_fcco = adc_min_fcco,
            },
            .minimize_error = false,
            .error_threshold = 2,
            .expected = true
        },
        {
            .name = "candidate error < current best error",
            .current_best = {
                .valid = true,
                .error = 5,
            },
            .candidate = {
                .valid = true,
                .fractional = false,
                .m = 2,
                .error = 1,
                .actual_fcco = adc_min_fcco,
            },
            .minimize_error = true,
            .error_threshold = 2,
            .expected = true
        },
        {
            .name = "same type, candidate m < current best m",
            .current_best = {
                .valid = true,
                .fractional = true,
                .m = 10,
                .error = 5,
            },
            .candidate = {
                .valid = true,
                .fractional = false,
                .m = 2,
                .error = 1,
                .actual_fcco = adc_min_fcco,
            },
            .minimize_error = false,
            .error_threshold = 2,
            .expected = true
        },
        {
            .name = "current_best fractional, candidate m <= current best m * 4",
            .current_best = {
                .valid = true,
                .fractional = true,
                .m = 4,
                .error = 5,
            },
            .candidate = {
                .valid = true,
                .fractional = false,
                .m = 1,
                .error = 1,
                .actual_fcco = adc_min_fcco,
            },
            .minimize_error = false,
            .error_threshold = 2,
            .expected = true
        }, 
        {
            .name = "candidate integer, not candidate m <= current best m * 4",
            .current_best = {
                .valid = true,
                .fractional = false,
                .m = 4,
                .error = 5,
            },
            .candidate = {
                .valid = true,
                .fractional = true,
                .m = 1,
                .error = 1,
                .actual_fcco = adc_min_fcco,
            },
            .minimize_error = false,
            .error_threshold = 2,
            .expected = false
        }
    };
    
    for (unsigned cur = 0; cur < sizeof(test_cases)/sizeof(test_cases[0]); cur++) {
        candidate_is_better_test_case t = test_cases[cur];
        ASSERT_EQ(pg2sdr__adc_candidate_is_better(&t.current_best, &t.candidate, t.minimize_error, t.error_threshold), t.expected);
    }
}

TEST(ADCTEST, Test_p_i_table_correct) {

    adc_p_i_tuple_t *table = pg2sdr__adc_make_p_i_table();
    ASSERT_NE(table, nullptr);

    for (unsigned p_i = 0; p_i < adc_p_i_table_size; ++p_i) {
        // For every table entry, either:
        if (table[p_i].i == UINT32_MAX) {
            // the entry is not filled
            EXPECT_EQ(table[p_i].i, UINT32_MAX);
            EXPECT_EQ(table[p_i].p, UINT32_MAX);
        } else {
            // the P/I values are consistent with the table index
            EXPECT_EQ(adc_effective_p_divisor(table[p_i].p) * adc_effective_i_divisor(table[p_i].i), p_i);
            // the P/I values are in range
            EXPECT_LE(table[p_i].p, adc_p_max_divisor);
            EXPECT_LE(table[p_i].i, adc_i_max_divisor);
            EXPECT_NE(table[p_i].i, 1);
        }
    }

    for (unsigned p = 0; p <= adc_p_max_divisor; ++p) {
        for (unsigned i = 0; i <= adc_i_max_divisor; ++i) {
            // I=1 is illegal
            if (i == 1)
                continue;

            // For every possible legal P/I combination:
            unsigned p_i = adc_effective_p_divisor(p) * adc_effective_i_divisor(i);

            // the corresponding entry index must be in range
            ASSERT_LT(p_i, adc_p_i_table_size);

            // either:
            if (table[p_i].i == i) {
                // this entry exactly matches this P/I
                EXPECT_EQ(table[p_i].i, i);
                EXPECT_EQ(table[p_i].p, p);
            } else {
                // or, the entry is "better" (smaller I / larger P)
                EXPECT_LT(table[p_i].i, i);
                EXPECT_GT(table[p_i].p, p);
            }
        }
    }
}

TEST(ADCTEST, Test_calculate_adc_clock_divisors) {
    
    uint32_t target_frequency = 5200000; //hz

    adc_pll_config_t int_divisors;
    ASSERT_EQ(pg2sdr__adc_find_divisors(target_frequency, &int_divisors, false, false, true, 0), PG2SDR_SUCCESS);
    ASSERT_EQ(int_divisors.valid, true);
    ASSERT_EQ(int_divisors.error, 0);
    ASSERT_EQ(int_divisors.i, 0);
    ASSERT_EQ(int_divisors.m, 13);
    ASSERT_EQ(int_divisors.n, 0);
    ASSERT_EQ(int_divisors.p, 30);
    ASSERT_EQ(int_divisors.actual_frequency, (float) target_frequency);

    adc_pll_config_t frac_divisors;
    ASSERT_EQ(pg2sdr__adc_find_divisors(target_frequency, &frac_divisors, false, true, true, 0), PG2SDR_SUCCESS);
    ASSERT_EQ(frac_divisors.valid, true);
    ASSERT_EQ(frac_divisors.error, 0);
    ASSERT_EQ(frac_divisors.i, 0);
    ASSERT_EQ(frac_divisors.m, 13);
    ASSERT_EQ(frac_divisors.n, 0);
    ASSERT_EQ(frac_divisors.p, 30);
    ASSERT_EQ(frac_divisors.actual_frequency, (float) target_frequency);
}

TEST(ADCTEST, Test_N_divisor) {
    const double target = 5.234e4;
    adc_pll_config_t divisors;
    ASSERT_EQ(pg2sdr__adc_find_divisors(target, &divisors, false, false, true, 0), PG2SDR_SUCCESS);

    // Solution should produce a frequency within 1ppm of the requested frequency
    EXPECT_LT(abs(divisors.actual_frequency / target - 1.0), 1e-6);

    // actual_fcco and actual_frequency should be consistent with what the other settings imply
    double expected_fcco = 2 * (adc_reference_frequency / adc_effective_n_divisor(divisors.n)) * divisors.m;
    double expected_fadc = expected_fcco / adc_effective_p_divisor(divisors.p) / adc_effective_i_divisor(divisors.i);
    EXPECT_FLOAT_EQ(divisors.actual_frequency, expected_fadc) << divisors.n << ' ' << divisors.m << ' ' << divisors.i << ' ' << divisors.p;
    EXPECT_FLOAT_EQ(divisors.actual_fcco, expected_fcco) << divisors.n << ' ' << divisors.m << ' ' << divisors.i << ' ' << divisors.p;

    // fcco should be in range
    EXPECT_GE(expected_fcco, 275e6);
    EXPECT_LE(expected_fcco, 550e6);
}

typedef std::tuple<double,bool,bool,bool,double> ADCTestParam;
class ADCParameterizedTest : public testing::TestWithParam<ADCTestParam> {};

TEST_P(ADCParameterizedTest, CanTune)
{
    double target, epsilon;
    bool minimize_error, allow_fractional, allow_no_pll;
    std::tie(target, minimize_error, allow_fractional, allow_no_pll, epsilon) = GetParam();

    adc_pll_config_t divisors;
    ASSERT_EQ(pg2sdr__adc_find_divisors(target, &divisors, minimize_error, allow_fractional, allow_no_pll, epsilon), PG2SDR_SUCCESS);

    // Solution should respect allow_fractional
    EXPECT_TRUE(allow_fractional || !divisors.fractional);

    // Solution should respect allow_no_pll
    EXPECT_TRUE(allow_no_pll || divisors.m != 0);

    // If no_pll is enabled, we should use a no_pll solution for target frequencies close
    // to exact factors of adc_reference_frequency
    double divisor = round(adc_reference_frequency / target);
    double no_pll_error = fabs(adc_reference_frequency / divisor - target);
    bool expect_no_pll = allow_no_pll && no_pll_error <= target * epsilon;
    EXPECT_TRUE(!expect_no_pll || divisors.m == 0);

    // Solution should produce a frequency within epsilon of the requested frequency
    EXPECT_NEAR(divisors.actual_frequency, target, target * epsilon);

    // actual_fcco and actual_frequency should be consistent with what the other settings imply
    double expected_fcco, expected_fadc;
    if (divisors.m == 0) {
        expected_fcco = 0;
        expected_fadc = adc_reference_frequency / adc_effective_i_divisor(divisors.i);
    } else {
        expected_fcco = 2 * (adc_reference_frequency / adc_effective_n_divisor(divisors.n)) * divisors.m;
        expected_fadc = expected_fcco / adc_effective_p_divisor(divisors.p) / adc_effective_i_divisor(divisors.i);

        // fcco should be in range
        EXPECT_GE(expected_fcco, 275e6);
        EXPECT_LE(expected_fcco, 550e6);
    }

    EXPECT_FLOAT_EQ(divisors.actual_frequency, expected_fadc);
    EXPECT_FLOAT_EQ(divisors.actual_fcco, expected_fcco);
}

/* target frequencies which are not particularly "round" numbers */
INSTANTIATE_TEST_SUITE_P(ADCTargetFloats, ADCParameterizedTest, Combine( /* target */ Range(0.5e6, 25e6, (25e6-0.5e6)/97),
                                                                         /* minimize_error */ Bool(),
                                                                         /* allow_fractional */ Bool(),
                                                                         /* allow_no_pll */ Bool(),
                                                                         /* epsilon */ Values(1e-6) ) );

/* target frequencies which are round numbers which should provoke the no_pll case */
INSTANTIATE_TEST_SUITE_P(ADCTargetExact, ADCParameterizedTest, Combine( /* target */ Range(1.0e6, 30.0e6, 1.0e6),
                                                                        /* minimize_error */ Bool(),
                                                                        /* allow_fractional */ Bool(),
                                                                        /* allow_no_pll */ Bool(),
                                                                        /* epsilon */ Values(1e-6) ) );


/* target freqs which can't be satisfied by integer solutions */
INSTANTIATE_TEST_SUITE_P(ADCTargetNeedsFractional, ADCParameterizedTest, Combine( /* target */ Values(9.0001e6),
                                                                                  /* minimize_error */ Bool(),
                                                                                  /* allow_fractional */ Values(true),
                                                                                  /* allow_no_pll */ Bool(),
                                                                                  /* epsilon */ Values(1e-6) ) );
