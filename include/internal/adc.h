#ifndef PG2SDR_ADC_H
#define PG2SDR_ADC_H

#include <stdint.h>
#include <assert.h>

/* PG2SDR internal header, ADC module */

/* valid range of CCO frequencies for PLL0AUDIO */
static const uint32_t adc_min_fcco = 275e6;
static const uint32_t adc_max_fcco = 550e6;

/* reference frequency used by PLL0AUDIO (= crystal frequency) */
static const double adc_reference_frequency = 12e6;

/* maximum P/I/N divisor values accepted by the hardware
 * for PLL0AUDIO
 *
 * nb: these are the values given to start_transfer; for
 * the actual effective divide-by-X values, see
 * `adc_effective_{n,p,i}_divisor`
 */
static const uint32_t adc_n_max_divisor = 256;
static const uint32_t adc_p_max_divisor = 32;
static const uint32_t adc_i_max_divisor = 256;

/* for a given N value, return the effective divide-by-X value */
static inline unsigned adc_effective_n_divisor(uint32_t n)
{
    /* The N value combines two components of the PLL0AUDIO hardware (see UM10503, Fig 34, pg. 84):
     *
     *  - N-DIVIDER, controlled by NP_DIV[21:12];
     *  - the "Direct Input" multiplexer following N-DIVIDER, controlled by CTRL[2]
     *
     * We can either use the N divider as a divide-by-N, N>=1
     * or (when N=0) bypass it entirely using the Direct Input multiplexer.
     *
     * N=1 is legal but mostly pointless (just bypass the divider with N=0 instead)
     */
    assert (n <= adc_n_max_divisor);
    return (n > 0) ? n : 1;
}

/* for a given P value, return the effective divide-by-X value */
static inline unsigned adc_effective_p_divisor(uint32_t p)
{
    /* The P value combines three components of the PLL0AUDIO hardware (see UM10503, Fig 34, pg. 184):
     *
     *  - P-DIVIDER, controlled by NP_DIV[6:0];
     *  - the fixed divide-by-2 divider following P-DIVIDER;
     *  - the "Direct Output" multiplexer following P-DIVIDER, controlled by CTRL[3]
     *
     * We can either use the P divider as a divide-by-P, P>=1, followed by a fixed divide-by-2;
     * or (when P=0) we can bypass both dividers entirely using the Direct Output multiplexer.
     */
    assert (p <= adc_p_max_divisor);
    return (p > 0) ? p * 2 : 1;
}

/* for a given I value, return the effective divide-by-X value */
static inline unsigned adc_effective_i_divisor(uint32_t i)
{
    /* The I value combines several configuration options of the CGU (see UM10503, Fig 32, pg. 154):
     *
     *  - The input chosen for BASE_ADCHS_CLK, either PLL0AUDIO directly, or the IDIVE configurable divider;
     *  - If IDIVE is used, then the divide-by-I configuration of IDIVE
     *
     * We can either use PLL0AUDIO directly as the ADC clock (I=0), or add a configurable divide-by-I between
     * PLL0AUDIO and the ADC (I>=2).
     *
     * Note that I=1 is illegal, as IDIVE can't be configured as a divide-by-1 (passthrough) divider. For
     * divide-by-1, use I=0 and bypass IDIVE entirely.
     */
    assert (i <= adc_i_max_divisor);
    assert (i != 1);
    return (i > 0) ? i : 1;
}

/* scale a floating-point M divisor to a fixed-point value suitable for passing to the hardware */
static inline uint32_t adc_fixed_point_m(double m)
{
    return (uint32_t)(round(m * 32768));
}

typedef struct {
    uint32_t p;
    uint32_t i;
} adc_p_i_tuple_t;

/* precompute a table of (effective P * effective I) -> (P,I); caller takes ownership.
 * the returned table is of size `adc_p_i_table_size`
 */
static const unsigned adc_p_i_table_size = adc_p_max_divisor * 2 * adc_i_max_divisor + 1;
adc_p_i_tuple_t *pg2sdr__adc_make_p_i_table();

/* Complete configuration settings for a particular ADC clock frequency */
typedef struct {
    bool valid;

    bool fractional;
    uint32_t n;
    double m;
    uint32_t p;
    uint32_t i;

    double error;
    double actual_fcco;
    double actual_frequency;
} adc_pll_config_t;

/* internal comparator for configuration candidates, exposed for test access */
int pg2sdr__adc_candidate_is_better(adc_pll_config_t *current_best, adc_pll_config_t *candidate, bool minimize_error, float error_threshold);

/* Find suitable ADC PLL settings for a sample rate of `target_frequency` Hz
 *
 * If `minimize_error` is true, prefer settings with lower absolute frequency error (difference between requested & configured frequency) always
 * If `minimize_error` is false, prefer settings with lower phase noise, even if this means increasing the absolute frequency error
 *
 * If `enable_fractional` is true, allow settings that use a fractional M divider.
 * If `enable_no_pll` is true, allow setttings that bypass PLL0AUDIO i.e. m == 0 (requires firmware >= 0.9.6.0)
 *
 * If `epsilon` > 0, only consider settings where the total frequency error is less than (epsilon * target_frequency)
 * If `epsilon` <= 0, behave as if epsilon = 1e-6 (i.e. 1PPM max error)
 *
 * Store results in `*divisors` and return PG2SDR_SUCCESS on success;
 * return a negative error code on failure.
 */
int pg2sdr__adc_find_divisors(double target_frequency, adc_pll_config_t *divisors, bool minimize_error, bool enable_fractional,
                              bool enable_no_pll, double epsilon);

#endif /* PG2SDR_ADC_H */
