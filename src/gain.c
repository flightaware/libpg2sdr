#include <pthread.h>

#include "internal.h"

/* Generic helpers, to reduce duplication needed for code that's the same shape for lna/mix/vga */

static int generic_set_gain(lpcsdr_device_handle *dev, unsigned gain, int (*tuner_set_gain)(lpcsdr_device_handle *, unsigned))
{
    int error = LPCSDR_SUCCESS;
    pthread_mutex_lock(&dev->mutex);
    if ((error = tuner_set_gain(dev, gain)) < 0)
        goto done;
    dev->current_gain_entry = NULL;

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

static int generic_set_gain_db(lpcsdr_device_handle *dev, double gain_db, double *table, int (*tuner_set_gain)(lpcsdr_device_handle *, unsigned))
{
    int error = LPCSDR_SUCCESS;
    pthread_mutex_lock(&dev->mutex);

    /* these tables are not necessarily sorted, do a full scan */
    unsigned best = 0;
    for (unsigned i = 1; i < 16; ++i) {
        if (fabs(table[i] - gain_db) < fabs(table[best] - gain_db))
            best = i;
    }

    if ((error = tuner_set_gain(dev, best)) < 0)
        goto done;
    dev->current_gain_entry = NULL;

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

/* -- LNA -- */

int lpcsdr_set_lna_gain(lpcsdr_device_handle *dev, unsigned gain)
{
    CHECK_DEV(dev);
    return generic_set_gain(dev, gain, lpcsdr__tuner_set_lna);
}

int lpcsdr_set_lna_gain_db(lpcsdr_device_handle *dev, double gain_db)
{
    CHECK_DEV(dev);
    return generic_set_gain_db(dev, gain_db, dev->lna_table, lpcsdr__tuner_set_lna);
}

/* -- MIX -- */

int lpcsdr_set_mix_gain(lpcsdr_device_handle *dev, unsigned gain)
{
    CHECK_DEV(dev);
    return generic_set_gain(dev, gain, lpcsdr__tuner_set_mix);
}

int lpcsdr_set_mix_gain_db(lpcsdr_device_handle *dev, double gain_db)
{
    CHECK_DEV(dev);
    return generic_set_gain_db(dev, gain_db, dev->mix_table, lpcsdr__tuner_set_mix);
}

/* -- VGA -- */

int lpcsdr_set_vga_gain(lpcsdr_device_handle *dev, unsigned gain)
{
    CHECK_DEV(dev);
    return generic_set_gain(dev, gain, lpcsdr__tuner_set_vga);
}

int lpcsdr_set_vga_gain_db(lpcsdr_device_handle *dev, double gain_db)
{
    CHECK_DEV(dev);
    return generic_set_gain_db(dev, gain_db, dev->vga_table, lpcsdr__tuner_set_vga);
}

/* Getters that operate on any/all three stages at once */

int lpcsdr_get_stage_gains(lpcsdr_device_handle *dev, unsigned *lna, unsigned *mix, unsigned *vga)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);
    if (lna)
        *lna = dev->lna_gain;
    if (mix)
        *mix = dev->mix_gain;
    if (vga)
        *vga = dev->vga_gain;
    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_get_stage_gains_db(lpcsdr_device_handle *dev, double *lna_db, double *mix_db, double *vga_db)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);
    if (lna_db)
        *lna_db = dev->lna_table[dev->lna_gain];
    if (mix_db)
        *mix_db = dev->mix_table[dev->mix_gain];
    if (vga_db)
        *vga_db = dev->vga_table[dev->vga_gain];
    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

/* Get/set gain as a single combined dB value, using a custom gain curve to select gain settings */

static int compare_gain_entry(const void *left, const void *right)
{
    const lpcsdr_gain_table_t *left_entry = (const lpcsdr_gain_table_t *) left;
    const lpcsdr_gain_table_t *right_entry = (const lpcsdr_gain_table_t *) right;

    if (left_entry->gain_db < right_entry->gain_db)
        return -1;
    if (left_entry->gain_db > right_entry->gain_db)
        return 1;
    return 0;
}

int lpcsdr_get_total_gain_db(lpcsdr_device_handle *dev, double *gain_db)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);
    if (gain_db) {
        if (dev->current_gain_entry) {
            /* We have an exact db value from the gain curve */
            *gain_db = dev->current_gain_entry->gain_db;
        } else {
            /* We don't have an exact value; try to derive something sensible from gains of individual stages */
            *gain_db =
                dev->lna_table[dev->lna_gain] +
                dev->mix_table[dev->mix_gain] +
                dev->vga_table[dev->vga_gain];
        }
    }
    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

int lpcsdr_set_total_gain_db(lpcsdr_device_handle *dev, double gain_db)
{
    CHECK_DEV(dev);
    pthread_mutex_lock(&dev->mutex);

    /* could do a binary search here, but that's a lot of code to write to search a small table
     * (libc bsearch doesn't help here, because we want an inexact match)
     */
    lpcsdr_gain_table_t *nearest = &dev->gain_table[0];
    for (size_t i = 1; i < dev->gain_table_size; ++i) {
        if (fabs(dev->gain_table[i].gain_db - gain_db) < fabs(nearest->gain_db - gain_db)) {
            /* entry i is nearer than our old attempt */
            nearest = &dev->gain_table[i];
        }
        if (dev->gain_table[i].gain_db > gain_db) {
            /* the table is sorted; the last possible candidate is the first entry with gain > our target */
            break;
        }
    }

    int error = LPCSDR_SUCCESS;
    if (dev->current_gain_entry != nearest) {
        if ((error = lpcsdr__tuner_set_gains(dev, nearest->lna_gain, nearest->mix_gain, nearest->vga_gain)) < 0)
            goto done;
        dev->current_gain_entry = nearest;
    }

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

/* -- Gain table access -- */

int lpcsdr_set_gain_tables(lpcsdr_device_handle *dev,
                           const lpcsdr_gain_table_t *gain_table, size_t gain_table_size,
                           const double *lna_table,
                           const double *mix_table,
                           const double *vga_table)
{
    CHECK_DEV(dev);
    if (gain_table != NULL && gain_table_size == 0)
        return LPCSDR_ERROR_BAD_ARGUMENT; /* if changing the gain table, it must have at least one entry */

    int error = LPCSDR_SUCCESS;
    pthread_mutex_lock(&dev->mutex);

    if (lna_table)
        memcpy(dev->lna_table, lna_table, sizeof(dev->lna_table));
    if (mix_table)
        memcpy(dev->mix_table, mix_table, sizeof(dev->mix_table));
    if (vga_table)
        memcpy(dev->vga_table, vga_table, sizeof(dev->vga_table));

    if (gain_table) {
        /* take a copy of the provided data, sort it, use the sorted copy */

        lpcsdr_gain_table_t *new_table;
        if (!(new_table = calloc(gain_table_size, sizeof(lpcsdr_gain_table_t)))) {
            error = LPCSDR_ERROR_NO_MEMORY;
            goto done;
        }

        memcpy(new_table, gain_table, gain_table_size * sizeof(lpcsdr_gain_table_t));
        qsort(new_table, gain_table_size, sizeof(lpcsdr_gain_table_t), compare_gain_entry);

        /* swap in the new copy */
        free(dev->gain_table);
        dev->gain_table = new_table;
        dev->gain_table_size = gain_table_size;
        dev->current_gain_entry = NULL; /* this might be pointing into the old table */
    }

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

int lpcsdr_get_gain_tables(lpcsdr_device_handle *dev,
                           lpcsdr_gain_table_t **gain_table,
                           size_t *gain_table_size,
                           double *lna_table,
                           double *mix_table,
                           double *vga_table)
{
    CHECK_DEV(dev);
    if ((gain_table && !gain_table_size) || (!gain_table && gain_table_size))
        return LPCSDR_ERROR_BAD_ARGUMENT;

    int error = LPCSDR_SUCCESS;
    pthread_mutex_lock(&dev->mutex);

    if (gain_table) {
        /* allocate a copy of the table, return a pointer; it is the caller's responsibility
         * to free() this memory when they are done with it.
         *
         * (we do this mostly to avoid complications caused if we returned a
         * pointer to the internal table, which can be reallocated if lpcsdr_set_gain_tables()
         * is called, perhaps even from a separate thread)
         */
        lpcsdr_gain_table_t *clone;
        if (!(clone = calloc(dev->gain_table_size, sizeof(lpcsdr_gain_table_t)))) {
            error = LPCSDR_ERROR_NO_MEMORY;
            goto done;
        }

        memcpy(clone, dev->gain_table, dev->gain_table_size * sizeof(lpcsdr_gain_table_t));
        *gain_table = clone;
        *gain_table_size = dev->gain_table_size;
    }


    if (lna_table)
        memcpy(lna_table, dev->lna_table, sizeof(dev->lna_table));
    if (mix_table)
        memcpy(mix_table, dev->mix_table, sizeof(dev->mix_table));
    if (vga_table)
        memcpy(vga_table, dev->vga_table, sizeof(dev->vga_table));

 done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}
