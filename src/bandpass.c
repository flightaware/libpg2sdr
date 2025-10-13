#include <pthread.h>

#include "internal.h"

/* -- Bandpass table access -- */

int lpcsdr_set_bandpass_table(lpcsdr_device_handle *dev,
                              const lpcsdr_bandpass_table_t *bandpass_table, size_t bandpass_table_size)
{
    CHECK_DEV(dev);
    if (bandpass_table == NULL || bandpass_table_size == 0)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    int error = LPCSDR_SUCCESS;
    pthread_mutex_lock(&dev->mutex);

    /* take a copy of the provided data */
    lpcsdr_bandpass_table_t *new_table;
    if (!(new_table = calloc(bandpass_table_size, sizeof(lpcsdr_bandpass_table_t)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto done;
    }
    memcpy(new_table, bandpass_table, bandpass_table_size * sizeof(lpcsdr_bandpass_table_t));

    /* swap in the new copy */
    free(dev->bandpass_table);
    dev->bandpass_table = new_table;
    dev->bandpass_table_size = bandpass_table_size;
    dev->current_bandpass_entry = NULL; /* this might be pointing into the old table */

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

int lpcsdr_get_bandpass_table(lpcsdr_device_handle *dev,
                              lpcsdr_bandpass_table_t **bandpass_table,
                              size_t *bandpass_table_size)
{
    CHECK_DEV(dev);
    if (!bandpass_table || !bandpass_table_size)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    int error = LPCSDR_SUCCESS;
    pthread_mutex_lock(&dev->mutex);

    /* allocate a copy of the table, return a pointer; it is the caller's responsibility
     * to free() this memory when they are done with it.
     *
     * (we do this mostly to avoid complications caused if we returned a
     * pointer to the internal table, which can be reallocated if lpcsdr_set_bandpass_tables()
     * is called, perhaps even from a separate thread)
     */
    lpcsdr_bandpass_table_t *clone;
    if (!(clone = calloc(dev->bandpass_table_size, sizeof(lpcsdr_bandpass_table_t)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto done;
    }

    memcpy(clone, dev->bandpass_table, dev->bandpass_table_size * sizeof(lpcsdr_bandpass_table_t));
    *bandpass_table = clone;
    *bandpass_table_size = dev->bandpass_table_size;

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

static double filter_penalty(const lpcsdr_bandpass_table_t *entry,
                             double low_signal,
                             double high_signal,
                             double low_nyquist,
                             double high_nyquist)
{
    /* If this bandpass filter exceeds the Nyquist frequency band, then
     * treat the folded frequencies as interference and reduce the effective
     * bandwidth
     */
    double lower_corner = entry->lower_corner;
    if (lower_corner < low_nyquist)
        lower_corner = low_nyquist + (low_nyquist - lower_corner);
    double upper_corner = entry->upper_corner;
    if (upper_corner > high_nyquist)
        upper_corner = high_nyquist - (upper_corner - high_nyquist);

    /* penalize filters that are too narrow; smaller penalty for filters that are too wide */
    double lower_penalty = (lower_corner <= low_signal) ? 0.25*(low_signal - lower_corner) : (lower_corner - low_signal);
    double upper_penalty = (upper_corner >= high_signal) ? 0.25*(upper_corner - high_signal) : (high_signal - upper_corner);
    /* normalize the penalty by bandwidth, so we can compare consistently to ripple */
    double bw_penalty = (lower_penalty + upper_penalty) / (high_signal - low_signal);

    /* penalize filters with more passband ripple */
    double ripple_penalty = entry->ripple * 0.05; /* make 1dB ripple worth about 5% signal bandwidth */

    return bw_penalty + ripple_penalty;
}

const lpcsdr_bandpass_table_t *lpcsdr__select_bandpass_filter(lpcsdr_device_handle *dev,
                                                              double low_signal,
                                                              double high_signal,
                                                              double low_nyquist,
                                                              double high_nyquist)
{
    /* find the filter entry with the lowest penalty (see filter_penalty()) */

    if (high_signal - low_signal < 1000) {
        /* <1kHz or inverted high/low, untangle it and ensure it's at least 1kHz wide */
        double ll = (low_signal < high_signal ? low_signal : high_signal) - 500;
        double hh = (low_signal > high_signal ? low_signal : high_signal) + 500;
        low_signal = ll;
        high_signal = hh;
    }

    const lpcsdr_bandpass_table_t *best = &dev->bandpass_table[0];
    double best_penalty = filter_penalty(best, low_signal, high_signal, low_nyquist, high_nyquist);
    for (size_t i = 1; i < dev->bandpass_table_size; ++i) {
        const lpcsdr_bandpass_table_t *candidate = &dev->bandpass_table[i];
        double candidate_penalty = filter_penalty(candidate, low_signal, high_signal, low_nyquist, high_nyquist);
        if (candidate_penalty < best_penalty) {
            best = candidate;
            best_penalty = candidate_penalty;
        }
    }
    return best;
}
