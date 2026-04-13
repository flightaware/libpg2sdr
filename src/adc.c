#include <math.h>
#include <stdlib.h>
#include <pthread.h>

#include "internal/lib.h"
#include "internal/adc.h"

static adc_p_i_tuple_t *p_i_table = NULL; /* Call init_p_i_table before use */
static void init_p_i_table_once()
{
    /* called via pthread_once exactly once */
    p_i_table = pg2sdr__adc_make_p_i_table();
}

static bool init_p_i_table()
{
    static pthread_once_t guard = PTHREAD_ONCE_INIT;
    pthread_once(&guard, init_p_i_table_once);
    return (p_i_table != NULL);
}

int pg2sdr__adc_candidate_is_better(adc_pll_config_t *current_best, adc_pll_config_t *candidate, bool minimize_error, float error_threshold)
{
    if (!candidate->valid)
        return false;

    if (candidate->actual_fcco < adc_min_fcco || candidate->actual_fcco > adc_max_fcco)
        return false;

    if (!candidate->fractional && (candidate->m < 1 || candidate->m > 1 << 15))
        return false;

    if (candidate->fractional && (adc_fixed_point_m(candidate->m) < 1 || adc_fixed_point_m(candidate->m) >= 1 << 22))
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

adc_p_i_tuple_t *pg2sdr__adc_make_p_i_table()
{
    adc_p_i_tuple_t *table = malloc(adc_p_i_table_size * sizeof(adc_p_i_tuple_t));
    if (!table)
        return NULL;

    for (uint32_t p_i = 0; p_i < adc_p_i_table_size; ++p_i) {
        table[p_i].i = UINT32_MAX;
        table[p_i].p = UINT32_MAX;
    }

    for (uint32_t p = 0; p <= adc_p_max_divisor; p++) {
        for (uint32_t i = 0; i <= adc_i_max_divisor; i++) {
            if (i == 1) /* I=1 is not a legal divisor value */
                continue;

            uint32_t p_i = adc_effective_p_divisor(p) * adc_effective_i_divisor(i);
            assert (p_i < adc_p_i_table_size);
            if (i < table[p_i].i) {
                table[p_i].p = p;
                table[p_i].i = i;
            }
        }
    }

    return table;
}

int pg2sdr__adc_find_divisors(double target_frequency, adc_pll_config_t *divisors, bool minimize_error, bool enable_fractional,
                              bool enable_no_pll, double epsilon)
{
    if (target_frequency > 80e6)
        return PG2SDR_ERROR_ADC_RATE_RANGE;

    if (!init_p_i_table())
        return PG2SDR_ERROR_NO_MEMORY;

    if (epsilon <= 0)
        epsilon = 1e-6;

    double error_threshold = target_frequency * epsilon;

    /* consider no-PLL case first */
    if (enable_no_pll) {
        unsigned xtal_divisor = round(adc_reference_frequency / target_frequency);
        if (xtal_divisor >= 1 && xtal_divisor <= 256) {
            double actual_frequency = adc_reference_frequency / xtal_divisor;
            double error = fabs(actual_frequency - target_frequency);
            if (error <= error_threshold) {
                divisors->valid = true;
                divisors->fractional = false;
                divisors->n = 0;
                divisors->m = 0;
                divisors->p = 0;
                divisors->i = (xtal_divisor == 1 ? 0 : xtal_divisor);
                divisors->error = error;
                divisors->actual_fcco = 0;
                divisors->actual_frequency = actual_frequency;
                return PG2SDR_SUCCESS;
            }
        }
    }

    /* consider PLL cases */

    uint32_t range_min = ceil(adc_min_fcco / target_frequency);
    uint32_t range_max = floor(adc_max_fcco / target_frequency);

    adc_pll_config_t current_best = { .valid = false };
    for (uint32_t s = range_min; s < range_max; s++) {
        if (s >= adc_p_i_table_size || p_i_table[s].i == UINT32_MAX) {
            continue;
        }

        uint32_t p = p_i_table[s].p;
        uint32_t i = p_i_table[s].i;

        uint32_t desired_fcco = target_frequency * s;

        desired_fcco = MIN(desired_fcco, adc_max_fcco);
        desired_fcco = MAX(desired_fcco, adc_min_fcco);

        if (enable_fractional) {
            uint32_t scaled_m = round(desired_fcco / adc_reference_frequency / 2 * (1 << 15));

            double test_fcco = 2 * scaled_m / (1<<15) * adc_reference_frequency;
            if (test_fcco < adc_min_fcco)
                scaled_m += 1;
            else if (test_fcco > adc_max_fcco)
                scaled_m -= 1;

            double fractional_m =  scaled_m / (1<<15);
            double actual_fcco = 2 * fractional_m * adc_reference_frequency;
            double actual_frequency = actual_fcco / s;
            double error = fabs(actual_frequency - target_frequency);

            adc_pll_config_t candidate = {
                .valid = true,
                .fractional = true,
                .n = 0,
                .m = fractional_m,
                .p = p,
                .i = i,
                .error = error,
                .actual_fcco = actual_fcco,
                .actual_frequency = actual_frequency,
            };
            if (pg2sdr__adc_candidate_is_better(&current_best, &candidate, minimize_error, error_threshold)) {
                current_best = candidate;
            }
        }

        for (uint32_t n = 0; n <= adc_n_max_divisor; n++) {
            double n_reference = adc_reference_frequency / adc_effective_n_divisor(n);
            uint32_t integer_m = round(desired_fcco / n_reference / 2);

            double actual_fcco = 2.0 * integer_m * n_reference;
            double actual_frequency = actual_fcco / s;
            double error = fabs(actual_frequency - target_frequency);

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
            if (pg2sdr__adc_candidate_is_better(&current_best, &candidate, minimize_error, error_threshold)) {
                current_best = candidate;
            }
        }
    }

    if (!current_best.valid) {
        /* All candidates were rejected */
        return PG2SDR_ERROR_ADC_RATE_RANGE;
    }

    *divisors = current_best;
    return PG2SDR_SUCCESS;
}
