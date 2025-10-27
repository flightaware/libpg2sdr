#include <stdatomic.h>
#include <pthread.h>
#include <assert.h>

#include "internal.h"

static double actual_sample_rate(lpcsdr_device_handle *dev);
static double actual_frequency(lpcsdr_device_handle *dev);
static double actual_bandpass_low(lpcsdr_device_handle *dev);
static double actual_bandpass_high(lpcsdr_device_handle *dev);

static double lo_offset(lpcsdr_device_handle *dev);
static double center_if_frequency(lpcsdr_device_handle *dev);

static int apply_rate_change(lpcsdr_device_handle *dev);
static int apply_freq_change(lpcsdr_device_handle *dev);
static int apply_bandpass_change(lpcsdr_device_handle *dev);

static bool tuner_pll_config_equal(const tuner_pll_config_t *left, const tuner_pll_config_t *right);
static bool tuner_bandpass_equal(const lpcsdr_bandpass_table_t *left, const lpcsdr_bandpass_table_t *right);

int lpcsdr_set_conversion_mode(lpcsdr_device_handle *dev, lpcsdr_conversion_mode_t mode)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    int error = LPCSDR_SUCCESS;

    if (dev->streaming) {
        error = LPCSDR_ERROR_BAD_STATE;
        goto done;
    }

    switch (mode) {
    case LPCSDR_MODE_LOWIF_REAL:
    case LPCSDR_MODE_BASEBAND:
        /* okay */
        break;

    default:
        error = LPCSDR_ERROR_BAD_ARGUMENT;
        goto done;
    }

    dev->conversion_mode = mode;
    dev->changing_rate = true;
    dev->changing_freq = true;
    dev->changing_bandpass = true;

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

int lpcsdr_get_conversion_mode(lpcsdr_device_handle *dev, lpcsdr_conversion_mode_t *mode)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    *mode = dev->conversion_mode;
    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_set_buffer_size(lpcsdr_device_handle *dev, size_t buffer_size)
{
    CHECK_DEV(dev);
    if (buffer_size < 1)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    pthread_mutex_lock(&dev->mutex);
    int error = LPCSDR_SUCCESS;

    if (dev->streaming) {
        error = LPCSDR_ERROR_BAD_STATE;
        goto done;
    }

    if (dev->buffer_size != buffer_size) {
        dev->buffer_size = buffer_size;
        dev->changing_rate = true; // we need to recalculate buffer sizes that depend on sample rate + buffer size
    }

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

int lpcsdr_get_buffer_size(lpcsdr_device_handle *dev, size_t *buffer_size)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    *buffer_size = dev->buffer_size;
    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_set_sample_rate(lpcsdr_device_handle *dev, double rate)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);

    if (rate != dev->requested_sample_rate) {
        dev->requested_sample_rate = rate;
        dev->changing_rate = true;
    }

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_get_sample_rate(lpcsdr_device_handle *dev, double *requested, double *actual)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);

    if (requested)
        *requested = dev->requested_sample_rate;
    if (actual)
        *actual = actual_sample_rate(dev);

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_set_decimation_mode(lpcsdr_device_handle *dev, int decimation_mode)
{
    CHECK_DEV(dev);

    if (decimation_mode > LPCSDR_DECIMATION_MAX || (decimation_mode < 0 && decimation_mode != LPCSDR_DECIMATION_AUTO && decimation_mode != LPCSDR_DECIMATION_AUTO_MAX))
        return LPCSDR_ERROR_BAD_ARGUMENT;

    pthread_mutex_lock(&dev->mutex);
    if (dev->decimation_mode != decimation_mode) {
        dev->decimation_mode = decimation_mode;
        dev->changing_rate = true;
    }

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_get_decimation_mode(lpcsdr_device_handle *dev, int *decimation_mode)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);
    if (decimation_mode)
        *decimation_mode = dev->decimation_mode;
    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_set_undersampling_mode(lpcsdr_device_handle *dev, int undersampling_mode)
{
    CHECK_DEV(dev);

    if (undersampling_mode <= 0)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    pthread_mutex_lock(&dev->mutex);
    if (dev->undersampling_mode != undersampling_mode) {
        dev->undersampling_mode = undersampling_mode;
        dev->changing_rate = true;
    }

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_get_undersampling_mode(lpcsdr_device_handle *dev, int *undersampling_mode)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);
    if (undersampling_mode)
        *undersampling_mode = dev->undersampling_mode;
    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_set_adc_limit(lpcsdr_device_handle *dev, double adc_limit)
{
    CHECK_DEV(dev);

    if (adc_limit <= 0 || adc_limit > 80e6)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    pthread_mutex_lock(&dev->mutex);
    if (dev->adc_limit != adc_limit) {
        dev->adc_limit = adc_limit;
        dev->changing_rate = true;
    }

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_get_adc_limit(lpcsdr_device_handle *dev, double *adc_limit)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);
    if (adc_limit)
        *adc_limit = dev->adc_limit;
    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_set_sideband(lpcsdr_device_handle *dev, bool upper_sideband)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);

    if (upper_sideband != dev->upper_sideband) {
        dev->upper_sideband = upper_sideband;
        dev->changing_freq = true;
        dev->changing_bandpass = true;
    }

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_get_sideband(lpcsdr_device_handle *dev, bool *upper_sideband)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    *upper_sideband = dev->upper_sideband;
    pthread_mutex_unlock(&dev->mutex);

    return LPCSDR_SUCCESS;
}

int lpcsdr_set_frequency(lpcsdr_device_handle *dev, double freq)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    if (freq != dev->requested_frequency) {
        dev->requested_frequency = freq;
        dev->changing_freq = true;
    }
    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_get_frequency(lpcsdr_device_handle *dev, double *requested, double *actual)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);

    if (requested)
        *requested = dev->requested_frequency;
    if (actual)
        *actual = actual_frequency(dev);

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_set_bandpass(lpcsdr_device_handle *dev, double low, double high)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);

    if (low > high) {
        double t = low;
        low = high;
        high = t;
    }

    if (low != dev->requested_bandpass_low || high != dev->requested_bandpass_high) {
        dev->requested_bandpass_low = low;
        dev->requested_bandpass_high = high;
        dev->changing_bandpass = true;
    }

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_get_bandpass(lpcsdr_device_handle *dev, double *low, double *high, double *actual_low, double *actual_high)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);

    if (low)
        *low = dev->requested_bandpass_low;
    if (high)
        *high = dev->requested_bandpass_high;
    if (actual_low)
        *actual_low = actual_bandpass_low(dev);
    if (actual_high)
        *actual_high = actual_bandpass_high(dev);

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_apply_changes(lpcsdr_device_handle *dev)
{
    CHECK_DEV(dev);

    int error = LPCSDR_SUCCESS;
    pthread_mutex_lock(&dev->mutex);

    if (dev->changing_rate) {
        if ((error = apply_rate_change(dev)) < 0)
            goto done;
        dev->changing_rate = false;    /* we are done reconfiguring the ADC .. */
        dev->changing_freq = true;     /* but we may need to reconfigure the tuner LO too (because the LO offset may have changed) */
        dev->changing_bandpass = true; /* .. and the bandpass filter too (because the IF center frequency & Nyquist frequency may have changed) */
    }

    if (dev->changing_freq) {
        if ((error = apply_freq_change(dev)) < 0)
            goto done;
        dev->changing_freq = false;    /* we are done reconfiguring the tuner LO */
    }

    if (dev->changing_bandpass) {
        if ((error = apply_bandpass_change(dev)) < 0)
            goto done;
        dev->changing_bandpass = false;    /* we are done reconfiguring the bandpass filter */
    }

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

static double actual_sample_rate(lpcsdr_device_handle *dev)
{
    if (!dev->adc_pll_config.valid || dev->changing_rate || !dev->adc_samples_per_user_sample)
        return 0;

    return dev->adc_pll_config.actual_frequency / dev->adc_samples_per_user_sample;
}

static double actual_frequency(lpcsdr_device_handle *dev)
{
    if (!dev->tuner_pll_config.valid || !dev->adc_pll_config.valid || dev->changing_rate || dev->changing_freq) {
        return 0;
    }

    return dev->tuner_pll_config.actual_frequency - lo_offset(dev);
}

static double actual_bandpass_low(lpcsdr_device_handle *dev)
{
    if (!dev->adc_pll_config.valid || !dev->current_bandpass_entry || dev->changing_rate || dev->changing_bandpass)
        return 0;

    const double center = center_if_frequency(dev);
    if (dev->upper_sideband)
        return (dev->current_bandpass_entry->lower_corner - center);
    else
        return (center - dev->current_bandpass_entry->lower_corner);
}

static double actual_bandpass_high(lpcsdr_device_handle *dev)
{
    if (!dev->adc_pll_config.valid || !dev->current_bandpass_entry || dev->changing_rate || dev->changing_bandpass)
        return 0;

    const double center = center_if_frequency(dev);
    if (dev->upper_sideband)
        return (dev->current_bandpass_entry->upper_corner - center);
    else
        return (center - dev->current_bandpass_entry->upper_corner);
}

static double undersampling_offset(lpcsdr_device_handle *dev)
{
    return (dev->undersampling_mode - 1) * dev->adc_pll_config.actual_frequency / 2.0;
}

static double lo_offset(lpcsdr_device_handle *dev)
{
    switch (dev->conversion_mode) {
    case LPCSDR_MODE_LOWIF_REAL:
        return undersampling_offset(dev) * (dev->upper_sideband ? -1.0 : 1.0);

    case LPCSDR_MODE_BASEBAND:
        /* lower sideband case: LO = freq + Fs/4
         * upper sideband case: LO = freq - Fs/4
         */
        return (dev->adc_pll_config.actual_frequency / 4.0 + undersampling_offset(dev)) * (dev->upper_sideband ? -1.0 : 1.0);

    default:
        return 0;
    }
}

static double center_if_frequency(lpcsdr_device_handle *dev)
{
    switch (dev->conversion_mode) {
    case LPCSDR_MODE_LOWIF_REAL:
        return 0;

    case LPCSDR_MODE_BASEBAND:
        return fabs(lo_offset(dev));

    default:
        return 0;
    }
}

static bool tuner_pll_config_equal(const tuner_pll_config_t *left, const tuner_pll_config_t *right)
{
    return
        left->valid &&
        right->valid &&
        left->refdiv == right->refdiv &&
        left->seldiv == right->seldiv &&
        left->feedback_n == right->feedback_n &&
        left->feedback_sdm == right->feedback_sdm;
}

static bool tuner_bandpass_equal(const lpcsdr_bandpass_table_t *left, const lpcsdr_bandpass_table_t *right)
{
    return
        left->hpf_corner == right->hpf_corner &&
        left->lpf_narrow == right->lpf_narrow &&
        left->lpf_coarse == right->lpf_coarse &&
        left->lpf_fine == right->lpf_fine &&
        left->lpf_q == right->lpf_q;
}

static int apply_rate_change(lpcsdr_device_handle *dev)
{
    /* can't change sample rate while streaming data */
    if (dev->streaming)
        return LPCSDR_ERROR_BAD_STATE;

    if (!dev->requested_sample_rate)
        return LPCSDR_SUCCESS;         /* no frequency configured yet */

    unsigned adc_samples_per_user_sample;
    unsigned post_decimation;
    switch (dev->conversion_mode) {
    case LPCSDR_MODE_LOWIF_REAL:
        adc_samples_per_user_sample = 1;
        post_decimation = 0;
        break;

    case LPCSDR_MODE_BASEBAND:
        if (dev->decimation_mode >= 0 && dev->decimation_mode <= LPCSDR_DECIMATION_MAX) {
            /* Explicit decimation setting */
            post_decimation = dev->decimation_mode;
        } else if (dev->decimation_mode == LPCSDR_DECIMATION_AUTO) {
            /* Scale up sample rate until it avoids the low end of the IF
             * range where the tuner IF filter will eat the bandwidth we
             * want to receive
             */
            double scaled = dev->requested_sample_rate;
            post_decimation = 0;
            while (scaled * 4 <= dev->adc_limit && post_decimation < LPCSDR_DECIMATION_MAX && (scaled - dev->requested_sample_rate) < 0.5e6) {
                ++post_decimation;
                scaled *= 2;
            }
        } else if (dev->decimation_mode == LPCSDR_DECIMATION_AUTO_MAX) {
            /* Scale up sample rate as far as possible (given fADC <= 20MHz) */
            double scaled = dev->requested_sample_rate;
            post_decimation = 0;
            while (scaled * 4 <= dev->adc_limit && post_decimation < LPCSDR_DECIMATION_MAX) {
                ++post_decimation;
                scaled *= 2;
            }
        } else {
            /* bad decimation mode */
            return LPCSDR_ERROR_CORRUPTION;
        }

        adc_samples_per_user_sample = 2 * (1 << post_decimation);
        break;

    default:
        return LPCSDR_ERROR_CORRUPTION;
    }

    double target = dev->requested_sample_rate * adc_samples_per_user_sample;
    if (target > dev->adc_limit)
        return LPCSDR_ERROR_ADC_RATE_RANGE;

    /* work out the PLL config so we know it's possible & what the exact
     * ADC rate is. The actual PLL programming only happens when we start streaming data
     */
    adc_pll_config_t new_config;
    int error;
    if ((error = pg2sdr__adc_find_divisors(target, &new_config, /* minimize_error= */ false, /* allow_fractional= */ true, /* epsilon= */ 0)) < 0)
        return error;

    LOGDEBUG(dev, "ADC sample rate changes to %.6f MHz with %u post-decimation steps (divide-by-%u)", new_config.actual_frequency/1e6, post_decimation, 1<<post_decimation);
    dev->adc_pll_config = new_config;

    /* recompute transfer sizes */
    unsigned adc_samples_per_buffer = dev->buffer_size * adc_samples_per_user_sample;        /* # of ADC samples that will fill the user buffer (rounded down) */
    unsigned blocks_per_buffer = adc_samples_per_buffer / dev->usb_samples_per_block;        /* # of ADC blocks that will fill the user buffer (rounded down) */
    dev->usb_transfer_size = blocks_per_buffer * dev->usb_bytes_per_block;                   /* Exact transfer size for that many ADC blocks */
    dev->adc_samples_per_transfer = blocks_per_buffer * dev->usb_samples_per_block;          /* # of ADC samples that we receive per USB transfer */
    dev->adc_samples_per_user_sample = adc_samples_per_user_sample;                          /* # of ADC samples per user sample (i.e. ADC sampling rate / user sampling rate) */
    dev->post_decimation = post_decimation;                                                  /* Extra decimation steps after downconversion */

    return LPCSDR_SUCCESS;
}

static int apply_freq_change(lpcsdr_device_handle *dev)
{
    if (!dev->requested_frequency)
        return LPCSDR_SUCCESS;         /* no frequency configured yet */

    if (!dev->adc_pll_config.valid) {
        /* no sampling rate configured yet, so no work to do right now. Later, when
         * the sample rate is eventually set (and lpcsdr_apply_changes is called
         * again), we'll configure both the sampling rate & tuner LO
         */
        return LPCSDR_SUCCESS;
    }

    double target = dev->requested_frequency + lo_offset(dev);

    int error;
    tuner_pll_config_t new_config;
    if ((error = pg2sdr__find_pll_parameters(target, dev->tuner_xtal, &new_config)) < 0) {
        /* new value is out of range */
        return error;
    }

    if (tuner_pll_config_equal(&new_config, &dev->tuner_pll_config)) {
        /* no work to do */
        return LPCSDR_SUCCESS;
    }

    LOGDEBUG(dev, "tuner LO frequency changes to %.6f MHz", new_config.actual_frequency/1e6);
    if ((error = pg2sdr__start_pll(dev, &new_config)) < 0) {
        /* we failed while configuring the tuner, so the LO state is uncertain.
         * Mark the current config as invalid as it probably no longer matches
         * the hardware state.
         */
        dev->tuner_pll_config.valid = false;
        return error;
    }

    dev->tuner_pll_config = new_config;

    return LPCSDR_SUCCESS;
}

static int apply_bandpass_change(lpcsdr_device_handle *dev)
{
    if (!dev->adc_pll_config.valid || dev->changing_rate)
        return LPCSDR_SUCCESS; /* will apply when rate change is applied */

    /* Clients specify low and high bandpass cutoffs relative to the zero
     * frequency in the samples they receive (i.e. relative to the RF
     * frequency they tuned to)
     *
     * e.g. if a client asks for complex baseband with:
     *
     *    sample rate:   10Msps complex
     *    frequency:     1090MHz
     *    low bandpass:  -1MHz
     *    high bandpass: 2MHz
     *
     * then it is interested in the spectrum between 1090-1 = 1089MHz
     * and 1090+2 = 1092MHz, expecting that to to appear as -1Mhz .. +2MHz
     * in the complex baseband output, and we should try to set our
     * tuner filters to match that.
     *
     * This requires that we map those requested frequencies to the
     * equivalent IF frequencies, since the tuner bandpass filter is
     * operating on the mixer output / IF signal. The details of this
     * depend on sample rate and choice of sideband.
     *
     * With high-sideband tuning, we will configure something like:
     *   ADC sample rate Fs = 20MHz
     *   Tuner LO = 1090MHz - Fs/4 = 1085MHz
     *   RF input @ 1090MHz -> IF signal at 5MHz -> baseband at 0MHz
     *   RF input @ 1089MHz -> IF signal at 4MHz -> baseband at -1MHz
     *   RF input @ 1092MHz -> IF signal at 7MHz -> baseband at +2Mhz
     *   tuner bandpass = 4MHz .. 7MHz
     *
     * With low-sideband tuning, we will configure something like:
     *   ADC sample rate Fs = 20MHz
     *   Tuner LO = 1090MHz + Fs/4 = 1095MHz
     *   RF input @ 1090MHz -> IF signal at 5MHz -> baseband at 0MHz
     *   RF input @ 1089MHz -> IF signal at 6MHz -> baseband at -1MHz
     *   RF input @ 1092MHz -> IF signal at 3MHz -> baseband at +2MHz
     *   tuner bandpass = 3MHz .. 6MHz
     *
     * (low-IF case is similar, but we tune the LO directly
     *  to the requested frequency)
     */
    const double center = center_if_frequency(dev);   /* IF frequency where the tuned RF frequency appears; this will end up at 0Hz in user samples */
    const double nyquist_low = undersampling_offset(dev);                                 /* low edge of unaliased bandpass sampling range */
    const double nyquist_high = nyquist_low + dev->adc_pll_config.actual_frequency / 2.0; /* high edge of unaliased bandpass sampling range */

    double l, h;
    if (dev->upper_sideband) {
        l = center + dev->requested_bandpass_low;
        h = center + dev->requested_bandpass_high;
    } else {
        l = center - dev->requested_bandpass_high;
        h = center - dev->requested_bandpass_low;
    }


    if (l < nyquist_low)
        l = nyquist_low;
    if (h > nyquist_high)
        h = nyquist_high;

    LOGDEBUG(dev, "IF bandpass filter constraints: signal (%.3f .. %.3f), Nyquist (%.3f .. %.3f) MHz",
             l/1e6, h/1e6, nyquist_low/1e6, nyquist_high/1e6);

    const lpcsdr_bandpass_table_t *settings = pg2sdr__select_bandpass_filter(dev,
                                                                             /* wanted signal bandpass */ l, h,
                                                                             /* Nyquist limits */ nyquist_low, nyquist_high);

    if (dev->current_bandpass_entry && tuner_bandpass_equal(settings, dev->current_bandpass_entry)) {
        /* no work to do */
        return LPCSDR_SUCCESS;
    }

    LOGDEBUG(dev, "set IF bandpass = %.3fMHz .. %.3fMHz (%u/%u/%u/%u/%u)",
             settings->lower_corner/1e6, settings->upper_corner/1e6,
             settings->hpf_corner,
             settings->lpf_narrow,
             settings->lpf_coarse,
             settings->lpf_fine,
             settings->lpf_q);

    int error;
    if ((error = pg2sdr__tuner_set_bandpass(dev, settings)) < 0) {
        /* tuner state is indeterminate now */
        dev->current_bandpass_entry = NULL;
        return error;
    }

    dev->current_bandpass_entry = settings;
    return LPCSDR_SUCCESS;
}
