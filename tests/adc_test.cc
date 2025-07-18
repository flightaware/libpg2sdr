#include <gtest/gtest.h>

extern "C" { /* Because our code is in C */
  #include "internal.h"
}

TEST (ADCTest, Test) {
    EXPECT_EQ(init_adc_divisors(), LPCSDR_SUCCESS);
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