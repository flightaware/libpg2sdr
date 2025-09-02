#include <math.h>
#include <stdlib.h>
#include "internal.h"

uint32_t n_divisors_length = 256;
uint32_t *n_divisors;

uint32_t p_divisors_length = 33;
uint32_t *p_divisors;

uint32_t i_divisors_length = 256;
uint32_t *i_divisors;

uint32_t **p_i_divisors_map;
uint32_t p_i_divisors_map_length;

// Scale factor to use 16 bits.
const uint16_t LPCSDR_FIXED_POINT_SCALE_FACTOR = 32768;

// ADC is outputting values that are 12 bit signed.
const uint16_t ADC_OUTPUT_VALUE_BIT_LENGTH = 2048;
const uint8_t SAMPLE_BIT_SIZE = 12;

int effective_n_divisor(uint32_t n) {
    return (n > 0) ? n: 1;
}

int effective_p_divisor(uint32_t p) {
    return (p > 0) ? p * 2 : 1;
}

int effective_i_divisor(uint32_t i) {
    return (i > 0) ? i : 1;
}

int fixed_point_m(adc_pll_config_t *divisors) {
    return (uint64_t)(round(divisors->m * 32768));
}

int candidate_is_better(adc_pll_config_t *current_best, adc_pll_config_t *candidate, uint32_t min_fcco, uint32_t max_fcco, bool minimize_error, float error_threshold) {
    if (!candidate->valid)
        return false;

    if (candidate->actual_fcco < min_fcco || candidate->actual_fcco > max_fcco)
        return false;
    
    if (!candidate->fractional && (candidate->m < 1 || candidate->m > 1 << 15))
        return false;

    if (candidate->fractional && (fixed_point_m(candidate) < 1 || fixed_point_m(candidate) >= 1 << 22))
        return false;

    if (candidate->error > error_threshold)
        return false;

    if (current_best == NULL || !current_best->valid)
        return true;

    if (minimize_error)
        return candidate->error < current_best->error;

    if (current_best->fractional == candidate->fractional)
        return candidate->m < current_best->m;

    if (current_best->fractional)
        return candidate->m <= current_best->m * 4;
    else
        return !(current_best->m <= candidate->m * 4);
}

int init_global_adc_divisor_tables() {
    return calculate_adc_divisor_tables(&n_divisors, &p_divisors, &i_divisors, &p_i_divisors_map, &p_i_divisors_map_length);
}

int calculate_adc_divisor_tables(uint32_t **n_out, uint32_t **p_out, uint32_t **i_out, uint32_t ***p_i_divisors_out, uint32_t *p_i_divisors_out_length) {
    uint32_t *n_divisors = calloc(n_divisors_length, sizeof(uint32_t));
    uint32_t *p_divisors = calloc(p_divisors_length, sizeof(uint32_t));
    uint32_t *i_divisors = calloc(i_divisors_length, sizeof(uint32_t));

    n_divisors[0] = 0;
    p_divisors[0] = 0;
    i_divisors[0] = 0;

    for (uint32_t n = 1; n < n_divisors_length; n++)
        n_divisors[n] = n + 1;

    for (uint32_t p = 1; p < p_divisors_length; p++)
        p_divisors[p] = p;

    for (uint32_t i = 1; i < i_divisors_length; i++)
        i_divisors[i] = i + 1;

    uint32_t largest_p_divisor = p_divisors[p_divisors_length - 1];
    uint32_t largest_i_divisor = i_divisors[i_divisors_length - 1];
    uint32_t num_divisors = effective_p_divisor(largest_p_divisor) * effective_i_divisor(largest_i_divisor);
    num_divisors += 1;

    uint32_t **p_i_divisors_map = (uint32_t **) calloc(num_divisors, sizeof(uint32_t *));

    for (uint32_t current_pair = 0; current_pair < num_divisors; current_pair++) {
        p_i_divisors_map[current_pair] = calloc(2, sizeof(uint32_t));
        memset(p_i_divisors_map[current_pair], UINT32_MAX, 2 * sizeof(uint32_t));
    }

    for (uint16_t p = 0; p < p_divisors_length; p++) {
        for (uint16_t i = 0; i < i_divisors_length; i++) {
            uint32_t p_divisor = p_divisors[p];
            uint32_t i_divisor = i_divisors[i];

            uint32_t d = effective_p_divisor(p_divisors[p]) * effective_i_divisor(i_divisors[i]);
            if (i < p_i_divisors_map[d][1]) {
                p_i_divisors_map[d][0] = p_divisor;
                p_i_divisors_map[d][1] = i_divisor;
            }
        }
    }

    *n_out = n_divisors;
    *p_out = p_divisors;
    *i_out = i_divisors;
    *p_i_divisors_out = p_i_divisors_map;
    *p_i_divisors_out_length = num_divisors;

    return LPCSDR_SUCCESS;
}

int calculate_adc_clock_divisors(uint32_t target_frequency, adc_pll_config_t *divisors, bool minimize_error, bool enable_fractional, double epsilon) {
    if (epsilon <= 0)
        epsilon = 1e-6;

    uint32_t min_fcco = 275e6;
    uint32_t max_fcco = 550e6;
    uint32_t reference_frequency = 12e6;
    double error_threshold = target_frequency * epsilon;

    uint32_t range_min = ceil(min_fcco / target_frequency);
    uint32_t range_max= floor(max_fcco/ target_frequency);

    adc_pll_config_t current_best = { .valid = false };
    for (uint32_t s = range_min; s < range_max; s++) {
        if (p_i_divisors_map[s][0] == UINT32_MAX || s > p_i_divisors_map_length) {
            continue;
        }

        uint32_t p = p_i_divisors_map[s][0];
        uint32_t i = p_i_divisors_map[s][1];

        uint32_t desired_fcco = target_frequency * s;

        desired_fcco = MIN(desired_fcco, max_fcco);
        desired_fcco = MAX(desired_fcco, min_fcco);

        if (enable_fractional) {
            uint32_t scaled_m = round(desired_fcco / reference_frequency / 2 * (1 << 15));

            uint32_t test_fcco = 2 * scaled_m / (1<<15) * reference_frequency;
            if (test_fcco < min_fcco)
                scaled_m += 1;
            else if (test_fcco > max_fcco)
                scaled_m -= 1;

            uint32_t fractional_m =  scaled_m / (1<<15);
            uint32_t actual_fcco = 2 * fractional_m * reference_frequency;
            uint32_t actual_frequency = actual_fcco / s;
            double error = (abs(actual_frequency - target_frequency));

            adc_pll_config_t candidate = {
                .fractional = true,
                .n = 0,
                .m = fractional_m,
                .p = p,
                .i = i,
                .error = error,
                .actual_fcco = actual_fcco,
                .actual_frequency = actual_frequency,
            };
            if (candidate_is_better(&current_best, &candidate, min_fcco, max_fcco, minimize_error, error_threshold)) {
                current_best = candidate;
            }
        }

        for (uint32_t n = 0; n < n_divisors_length; n++) {
            uint32_t n_reference = reference_frequency / effective_n_divisor(n_divisors[n]);
            uint32_t integer_m = round(desired_fcco / n_reference / 2);

            uint32_t actual_fcco = 2 * integer_m * n_reference;
            uint32_t actual_frequency = actual_fcco / s;
            double error = abs(actual_frequency - target_frequency);

            adc_pll_config_t candidate = {
                .valid = true,
                .fractional = false,
                .n = n,
                .m = integer_m,
                .p = p,
                .i = i,
                .error = error,
                .actual_fcco = actual_fcco,
                .actual_frequency = actual_frequency,
            };
            if (candidate_is_better(&current_best, &candidate, min_fcco, max_fcco, minimize_error, error_threshold)) {
                current_best = candidate;
            }
        }
    }
    *divisors = current_best;
    return LPCSDR_SUCCESS;
}
