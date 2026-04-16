#include <stdatomic.h>
#include <pthread.h>
#include <assert.h>

#include "internal/lib.h"
#include "starch.h"

#define memory_barrier() atomic_thread_fence(memory_order_acq_rel)

static size_t convert_adc_blocks(pg2sdr_device *dev, const uint8_t *data, size_t length, int16_t *out);
static void dispatch_contiguous_blocks(pg2sdr_device *dev, const uint8_t *data, size_t length, pg2sdr_stream_callback callback, void *user_data);

static pg2sdr_sample_buffer *get_buffer(pg2sdr_device *dev);

static int allocate_transfers(pg2sdr_device *dev);
static void cancel_transfers(pg2sdr_device *dev);
static void free_transfers(pg2sdr__transfer_state *transfers, unsigned transfer_count);
static void free_dev_transfers(pg2sdr_device *dev);
static int dispatch_transfer(pg2sdr_device *dev, pg2sdr__transfer_state *dev_transfer, pg2sdr_stream_callback callback, void *user_data);
static int drain_transfers(pg2sdr_device *dev);
static int submit_one_transfer(struct pg2sdr__transfer_state *dev_transfer);
static int submit_transfers(pg2sdr_device *dev);
static void transfer_callback(struct libusb_transfer *xfer);
static bool check_any_transfer_busy(pg2sdr_device *dev);

static size_t user_sample_size(pg2sdr_device *dev)
{
    return (dev->conversion_mode == PG2SDR_MODE_BASEBAND ? sizeof(cs16_t) : sizeof(int16_t));
}

/* Unpack (and byteswap if needed) a block header at "in"
 * and store the result in "out"
 */
static void unpack_header(const uint8_t *in, ep1_header_t* out)
{
    const ep1_header_t *h = (ep1_header_t *)in;
    out->magic = le32toh(h->magic);
    out->block_len = le32toh(h->block_len);
    out->samples = le32toh(h->samples);
    out->sequence = le32toh(h->sequence);
    out->status = le32toh(h->status);
}

/* Get a fresh pg2sdr_sample_buffer that can be used to fill with data and pass to user code */
static pg2sdr_sample_buffer *get_buffer(pg2sdr_device *dev)
{
    /* For now, we just allocate on demand and let libc's malloc implementation deal with it.
     * Later, may be worth revisiting to do our own free-list if malloc doesn't perform well.
     */
    pg2sdr_sample_buffer *buf = malloc(sizeof(pg2sdr_sample_buffer) + dev->buffer_size * user_sample_size(dev));

    buf->samples = (int16_t*) (buf + 1); /* sample data immediately follows the header */
    buf->count = 0;
    buf->timestamp = 0;

    return buf;
}

/* Release a pg2sdr_sample_buffer that is no longer used by user code */
void pg2sdr_release_buffer(pg2sdr_sample_buffer *buf)
{
    free(buf);
}

/* Convert one or more ADC blocks (with headers) starting at "data", total byte length "length", storing converted
 * samples in "out"
 *
 * Caller should ensure that:
 *   the provided data is an exact number of ADC blocks (i.e. length is a multiple of dev->usb_bytes_per_block)
 *   the total converted data length produced will fit into "out"
 *
 * Returns the total number of output samples produced
 */
static size_t convert_adc_blocks(pg2sdr_device *dev, const uint8_t *data, size_t length, int16_t *out)
{
    assert (length % dev->usb_bytes_per_block == 0);

    const uint32_t bpb = dev->usb_bytes_per_block;
    const uint32_t spb = dev->usb_samples_per_block;
    const uint32_t swpb = spb / 8 * 3; /* "sample words per block", number of uint32_t words of sample data per block */

    /* We might need to un-invert the spectrum while converting ADC data.
     *
     * Lower sideband tuning causes spectrum inversion;
     * even-mode undersampling (N=2,4,...) also causes spectrum inversion;
     * if both apply, they cancel out.
     */
    const bool inverted_spectrum = (dev->sideband_mode == PG2SDR_SIDEBAND_LOWER) ^ ((dev->undersampling_mode & 1) == 0);

    switch(dev->conversion_mode) {
    case PG2SDR_MODE_LOWIF_REAL:
        {
            /* unpack ADC data into out directly */
            unsigned count = 0;
            pg2sdr__profile_start(&dev->profile_unpack, 0);
            for (unsigned i = 0; i < length; i += bpb, count += spb)
                pg2sdr__starch_unpack_raw_adc_data((const uint32_t*) (data + i + sizeof(ep1_header_t)), swpb, out + count);
            pg2sdr__profile_end(&dev->profile_unpack, count);
            return count;
        }

    case PG2SDR_MODE_BASEBAND:
        {
            /* unpack ADC data into work_buffer[0] */
            unsigned count = 0;
            pg2sdr__profile_start(&dev->profile_unpack, 0);
            if (!inverted_spectrum) {
                for (unsigned i = 0; i < length; i += bpb, count += spb)
                    pg2sdr__starch_unpack_raw_adc_data((const uint32_t*) (data + i + sizeof(ep1_header_t)), swpb, (int16_t*)dev->work_buffer[0] + count);
            } else {
                for (unsigned i = 0; i < length; i += bpb, count += spb)
                    pg2sdr__starch_unpack_raw_adc_data_invert((const uint32_t*) (data + i + sizeof(ep1_header_t)), swpb, (int16_t*)dev->work_buffer[0] + count);
            }
            pg2sdr__profile_end(&dev->profile_unpack, count);

            /* work_buffer[0] now contains uninverted-spectrum ADC data, with the signal we want centered at Fs/4 */

            if (dev->post_decimation) {
                /* do downconversion from work_buffer[0] -> work_buffer[1] */
                pg2sdr__profile_start(&dev->profile_downconverter, count);
                count = pg2sdr__dsp_downconvert_process(dev->downconverter, (int16_t*)dev->work_buffer[0], count, (cs16_t*)dev->work_buffer[1]);
                pg2sdr__profile_end(&dev->profile_downconverter, 0);

                /* work_buffer[1] now contains complex baseband data, with the signal we want centered at 0Hz,
                 * but at a higher sampling rate than the user requested. Decimate to bring the sampling rate
                 * down to what was requested.
                 */

                /* do all but the final decimate-by-2 step, alternating work buffers */
                unsigned work_in = 1;
                for (unsigned i = 0; i < (dev->post_decimation - 1); ++i) {
                    unsigned work_out = !work_in;
                    pg2sdr__profile_start(&dev->profile_decimator[i], count);
                    count = pg2sdr__dsp_halfband_decimate_process(dev->post_decimators[i], (cs16_t*)dev->work_buffer[work_in], count, (cs16_t*)dev->work_buffer[work_out]);
                    pg2sdr__profile_end(&dev->profile_decimator[i], 0);
                    work_in = work_out;
                }

                /* do final decimate-by-2 step directly to out */
                pg2sdr__profile_start(&dev->profile_decimator[dev->post_decimation - 1], count);
                count = pg2sdr__dsp_halfband_decimate_process(dev->post_decimators[dev->post_decimation - 1], (cs16_t*)dev->work_buffer[work_in], count, (cs16_t*)out);
                pg2sdr__profile_end(&dev->profile_decimator[dev->post_decimation - 1], 0);
            } else {
                /* No extra decimation, do downconversion from work_buffer[0] -> out */
                pg2sdr__profile_start(&dev->profile_downconverter, count);
                count = pg2sdr__dsp_downconvert_process(dev->downconverter, (int16_t*)dev->work_buffer[0], count, (cs16_t*)out);
                pg2sdr__profile_end(&dev->profile_downconverter, 0);
            }

            /* out now contains complex baseband data, with the signal we want centered at 0Hz, at the user's requested sample rate */
            return count;
        }

    default:
        /* not implemented */
        return 0;
    }
}

/* Convert and dispatch one or more ADC blocks (with headers) starting at "data", total byte length "length", to the user
 * callback "callback".
 *
 * Caller should ensure that:
 *  the provided data is an exact number of ADC blocks (i.e. length is a multiple of dev->usb_bytes_per_block)
 *  the ADC blocks have contiguous sequence numbers (i.e. only a single user callback with a single starting timestamp needs to be made)
 *  the total converted data length produced will fit into a sample buffer
 */
static void dispatch_contiguous_blocks(pg2sdr_device *dev, const uint8_t *data, size_t length, pg2sdr_stream_callback callback, void *user_data)
{
    assert (length % dev->usb_bytes_per_block == 0);

    /* todo: this is only true for PG2SDR_MODE_LOWIF_REAL with int16, update once we support other versions */
    const unsigned adc_samples = length / dev->usb_bytes_per_block * dev->usb_samples_per_block;
    const unsigned user_samples = adc_samples / dev->adc_samples_per_user_sample;
    assert (user_samples <= dev->buffer_size);

    pg2sdr_sample_buffer *buffer = get_buffer(dev);
    if (!buffer)
        return; /* no buffers available, drop this data on the floor */

    unsigned count = convert_adc_blocks(dev, data, length, (int16_t*) buffer->samples);

    ep1_header_t h;
    unpack_header(data, &h);

    buffer->timestamp = (h.sequence * dev->usb_samples_per_block - dev->partial_samples) / dev->adc_samples_per_user_sample;
    buffer->count = count;
    dev->partial_samples = (dev->partial_samples + adc_samples) % dev->adc_samples_per_user_sample;

    bool result = callback(dev, buffer, user_data);
    if (result) {
        pg2sdr_release_buffer(buffer);
    }
}

int pg2sdr_stream_data(pg2sdr_device *dev, pg2sdr_stream_callback callback, void *user_data)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    if (dev->streaming) {
        LOGDEBUG(dev, "pg2sdr_stream_data: already streaming");
        pthread_mutex_unlock(&dev->mutex);
        return PG2SDR_ERROR_BAD_STATE;
    }

    int error, usb_error;

    /* ensure tuner/ADC configuration is up to date */
    if ((error = pg2sdr_apply_changes(dev)) < 0) {
        LOGDEBUG(dev, "pg2sdr_stream_data: apply_changes failed");
        goto done;
    }

    if (!dev->adc_pll_config.valid) {
        /* sample rate not set */
        LOGDEBUG(dev, "pg2sdr_stream_data: sample rate not set");
        error = PG2SDR_ERROR_BAD_STATE;
        goto done;
    }

    /* estimate time for one transfer to complete, plus control transfer timeout */
    double fill_time_ms = 1000.0 * dev->adc_samples_per_transfer / dev->adc_pll_config.actual_frequency;
    unsigned timeout_ms = (unsigned) (fill_time_ms + dev->control_timeout_ms);

    if ((error = allocate_transfers(dev)) < 0) {
        LOGDEBUG(dev, "pg2sdr_stream_data: allocate_transfers failed");
        goto done;
    }

    /* if we're downconverting, set up the DSP state & work buffers */
    if (dev->conversion_mode == PG2SDR_MODE_BASEBAND) {
        if (!(dev->downconverter = pg2sdr__dsp_downconvert_create(/* ntaps */ pg2sdr__dsp_default_halfband_ntaps,
                                                                  /* taps */ pg2sdr__dsp_default_halfband_taps,
                                                                  /* max_in_length */ dev->adc_samples_per_transfer))) {
            LOGDEBUG(dev, "pg2sdr_stream_data: dsp_create_downconvert failed");
            error = PG2SDR_ERROR_NO_MEMORY;
            goto cleanup;
        }

        for (unsigned i = 0; i < 2; ++i) {
            if (!(dev->work_buffer[i] = malloc(dev->adc_samples_per_transfer * sizeof(int16_t)))) {
                LOGDEBUG(dev, "pg2sdr_stream_data: work buffer allocation failed");
                error = PG2SDR_ERROR_NO_MEMORY;
                goto cleanup;
            }
        }

        for (unsigned i = 0; i < dev->post_decimation; ++i) {
            if (!(dev->post_decimators[i] = pg2sdr__dsp_halfband_decimate_create(/* ntaps */ pg2sdr__dsp_default_halfband_ntaps,
                                                                                 /* taps */ pg2sdr__dsp_default_halfband_taps))) {
                LOGDEBUG(dev, "pg2sdr_stream_data: dsp_halfband_decimate_create failed");
                error = PG2SDR_ERROR_NO_MEMORY;
                goto cleanup;
            }
        }
    }

    pg2sdr__profile_reset(&dev->profile_unpack);
    pg2sdr__profile_reset(&dev->profile_downconverter);
    for (unsigned i = 0; i < PG2SDR_DECIMATION_MAX; ++i)
        pg2sdr__profile_reset(&dev->profile_decimator[i]);

    dev->partial_samples = 0;

    /* clear any endpoint halt first */
    if ((usb_error = libusb_clear_halt(dev->usb_handle, 0x81)) < 0) {
        LOGDEBUG(dev, "pg2sdr_stream_data: libusb_clear_halt failed");
        error = pg2sdr__translate_libusb_error(usb_error);
        goto cleanup;
    }

    LOGDEBUG(dev,
             "starting ADC transfers with:\n"
             "  N: %u\n"
             "  M: %.5f\n"
             "  P: %u\n"
             "  I: %u\n"
             "  fCCO: %.2f MHz\n"
             "  fADC: %.2f MHz",
             dev->adc_pll_config.n,
             dev->adc_pll_config.m,
             dev->adc_pll_config.p,
             dev->adc_pll_config.i,
             dev->adc_pll_config.actual_fcco / 1e6,
             dev->adc_pll_config.actual_frequency / 1e6);


    ep0_out_start_transfer_t start_params = {
        .n_divisor = dev->adc_pll_config.n,
        .m_divisor = round(dev->adc_pll_config.m * 32768.0),  // M divisor is Q15 fixed-point
        .p_divisor = dev->adc_pll_config.p,
        .idiv_divisor = dev->adc_pll_config.i
    };

    if ((error = pg2sdr__ctrl_start_transfer(dev->usb_handle, &start_params, dev->control_timeout_ms)) < 0) {
        LOGDEBUG(dev, "pg2sdr_stream_data: start_transfer failed");
        goto cleanup;
    }

    if ((error = submit_transfers(dev)) < 0) {
        LOGDEBUG(dev, "pg2sdr_stream_data: submit_transfers failed");
        goto drain;
    }

    dev->streaming = true;

    while (true) {
        dev->completion_flag = 0;
        memory_barrier();

        if (dev->draining) {
            LOGDEBUG(dev, "pg2sdr_stream_data: something set the draining flag, stopping");
            break;
        }

        if (dev->active_transfers_head->state == XFER_BUSY) {
            struct timeval timeout = {
                .tv_sec = timeout_ms / 1000,
                .tv_usec = (timeout_ms % 1000) * 1000
            };
            pthread_mutex_unlock(&dev->mutex);
            usb_error = libusb_handle_events_timeout_completed(dev->ctx->libusb_ctx, &timeout, &dev->completion_flag);
            pthread_mutex_lock(&dev->mutex);
            if (usb_error < 0 && usb_error != LIBUSB_ERROR_INTERRUPTED) {
                LOGDEBUG(dev, "pg2sdr_stream_data: got a libusb error in the main loop: %s", libusb_strerror(usb_error));
                error = pg2sdr__translate_libusb_error(usb_error);
                goto drain;
            }
            continue;
        }

        pg2sdr__transfer_state *current = dev->active_transfers_head;

        if (current->state != XFER_COMPLETED) {
            LOGERROR(dev, "pg2sdr_stream_data: active transfer has state %d", (int)current->state);
            error = PG2SDR_ERROR_BAD_STATE;
            goto drain;
        }

        if (current->transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            LOGERROR(dev, "pg2sdr_stream_data: bulk transfer failed with status %d", (int)current->transfer->status);
            error = pg2sdr__translate_libusb_transfer_status(current->transfer->status);
            goto drain;
        }

        if ((error = dispatch_transfer(dev, current, callback, user_data)) < 0) {
            goto drain;
        }

        current->state = XFER_IDLE;
        dev->active_transfers_head = current->next;
        if (!dev->active_transfers_head)
            dev->active_transfers_tail = NULL;

        if ((error = submit_one_transfer(current)) < 0) {
            goto drain;
        }
    }

 drain:
    dev->draining = true;
    /* return value ignored */ pg2sdr__ctrl_stop_transfer(dev->usb_handle, dev->control_timeout_ms);

    int cleanup_error = drain_transfers(dev);
    if (cleanup_error < 0) {
        goto done;
    }

 cleanup:
    pg2sdr__profile_log(dev, &dev->profile_unpack, "profile: unpack");
    pg2sdr__profile_log(dev, &dev->profile_downconverter, "profile: downconverter");
    for (unsigned i = 0; i < PG2SDR_DECIMATION_MAX; ++i) {
        if (dev->post_decimators[i])
            pg2sdr__profile_log(dev, &dev->profile_decimator[i], "profile: decimator[%u]", i);
    }

    for (unsigned i = 0; i < PG2SDR_DECIMATION_MAX; ++i) {
        pg2sdr__dsp_halfband_decimate_free(dev->post_decimators[i]);
        dev->post_decimators[i] = NULL;
    }

    pg2sdr__dsp_downconvert_free(dev->downconverter);
    dev->downconverter = NULL;

    for (unsigned i = 0; i < 2; ++i) {
        free(dev->work_buffer[i]);
        dev->work_buffer[i] = NULL;
    }

    free_dev_transfers(dev);

 done:
    dev->streaming = false;
    dev->draining = false;

    pthread_mutex_unlock(&dev->mutex);
    return error;
}

int pg2sdr_stop_streaming(pg2sdr_device *dev)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    if (!dev->streaming) {
        pthread_mutex_unlock(&dev->mutex);
        return PG2SDR_ERROR_BAD_STATE;
    }

    dev->draining = true;

    /* wake up the streaming thread */
    memory_barrier();
    dev->completion_flag = 1;
    libusb_interrupt_event_handler(dev->ctx->libusb_ctx);

    pthread_mutex_unlock(&dev->mutex);
    return PG2SDR_SUCCESS;
}

//Transfers

/* callback from libusb when a transfer completes.
 * We do no real processing here and just mark the transfer as completed,
 * then set the completion flag to avoid multithread event handler races
 * (see libusb docs)
 */
static void transfer_callback(struct libusb_transfer *xfer)
{
    pg2sdr__transfer_state *dev_transfer = (pg2sdr__transfer_state *)xfer->user_data;
    dev_transfer->state = XFER_COMPLETED;
    memory_barrier();
    dev_transfer->dev->completion_flag = 1;
}

/* (re)allocate libusb transfers and associated buffers for the current buffer size */
static int allocate_transfers(pg2sdr_device *dev)
{
    free_dev_transfers(dev);

    int error;

    /* allocate enough transfers for ~ 250ms, within reason */
    unsigned transfer_count = dev->adc_pll_config.actual_frequency / 4 / dev->adc_samples_per_transfer;
    if (transfer_count < 4)
        transfer_count = 4;
    if (transfer_count > 32)
        transfer_count = 32;

    /*
     * set our transfer timeout conservatively, based on the expected time to fill all our transfers
     * at the current sample rate
     */
    float fill_time_ms = 1000.0f * dev->adc_samples_per_transfer * transfer_count / dev->adc_pll_config.actual_frequency;
    unsigned transfer_timeout_ms = (unsigned) (fill_time_ms + dev->control_timeout_ms);

    LOGDEBUG(dev,
             "allocate_transfers: \n"
             "  buffer_size              %zu\n"
             "  usb_transfer_size        %u\n"
             "  adc_samples_per_transfer %u\n"
             "  transfer_count           %u\n"
             "  transfer_timeout_ms      %u",
             dev->buffer_size,
             dev->usb_transfer_size,
             dev->adc_samples_per_transfer,
             transfer_count,
             transfer_timeout_ms);

    pg2sdr__transfer_state *transfers = calloc(transfer_count, sizeof(pg2sdr__transfer_state));
    if (!transfers) {
        error = PG2SDR_ERROR_NO_MEMORY;
        goto failed;
    }

    for (unsigned i = 0; i < transfer_count; ++i) {
        /* We could use libusb_dev_mem_alloc here, but in practice zerocopy is flaky enough
         * that it's not worth it:
         *
         *  - some kernels have a bug where they'll just return garbage in zerocopy buffers
         *  - some kernels have a different bug where attempting to access a zerocopy buffer will SIGBUS the process
         *  - even with working kernels, the cache characteristics of the buffer mean that it can be slow to access
         *    (cf. how dump1090 has to use bounce buffers to get reasonable performance)
         */
        if (!(transfers[i].buffer = malloc(dev->usb_transfer_size))) {
            error = PG2SDR_ERROR_NO_MEMORY;
            goto failed;
        }
    }

    for (unsigned i = 0; i < transfer_count; ++i) {
        /* set up the transfer */
        if (!(transfers[i].transfer = libusb_alloc_transfer(0))) {
            error = PG2SDR_ERROR_NO_MEMORY;
            goto failed;
        }

        libusb_fill_bulk_transfer(transfers[i].transfer,  /* transfer to populate */
                                  dev->usb_handle,        /* usb device */
                                  0x81,                   /* endpoint number, EP 1 IN */
                                  transfers[i].buffer,    /* buffer to fill */
                                  dev->usb_transfer_size, /* max bytes to receive */
                                  transfer_callback,      /* callback on completion */
                                  (void *)&transfers[i],  /* callback user data */
                                  transfer_timeout_ms);   /* timeout */
        transfers[i].transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

        /* rest of the metadata */
        transfers[i].dev = dev;
        transfers[i].state = XFER_IDLE;
        transfers[i].next = NULL;
    }

    dev->transfers = transfers;
    dev->transfer_count = transfer_count;

    return PG2SDR_SUCCESS;

failed:
    free_transfers(transfers, transfer_count);
    return error;
}

/* free the contents of "transfers", and the array itself */
static void free_transfers(pg2sdr__transfer_state *transfers, unsigned transfer_count)
{
    if (!transfers)
        return;

    for (unsigned i = 0; i < transfer_count; ++i) {
        if (transfers[i].transfer)
            libusb_free_transfer(transfers[i].transfer);

        if (transfers[i].buffer) {
            free(transfers[i].buffer);
        }
    }
    free(transfers);
}

/* free any transfers currently allocated by "dev" */
static void free_dev_transfers(pg2sdr_device *dev)
{
    free_transfers(dev->transfers, dev->transfer_count);

    dev->transfers = NULL;
    dev->transfer_count = 0;
}

/* dispatch one completed USB transfer to user code */
static int dispatch_transfer(pg2sdr_device *dev, pg2sdr__transfer_state *dev_transfer, pg2sdr_stream_callback callback, void *user_data)
{
    unsigned bytelength = dev_transfer->transfer->actual_length;

    if (bytelength % dev->usb_bytes_per_block != 0) {
        return PG2SDR_ERROR_TRANSFER_FORMAT;
    }

    /* scan through the received data, validating headers and looking
     * for sequence discontinuities.  We give each user callback only
     * a single sample timestamp for the start of samples given to the
     * callback, so if there are any discontinuities, we need to do a
     * separate callback for each range of contiguous sequence numbers
     * so the user code can see the gaps.
     */
    ep1_header_t h;
    uint32_t expected_sequence = 0;
    unsigned start = 0;
    for (unsigned int offset = 0; offset < bytelength; offset += dev->usb_bytes_per_block) {
        unpack_header(dev_transfer->buffer + offset, &h);

        if (h.magic != BLOCK_MAGIC || h.block_len != dev->usb_bytes_per_block || h.samples != dev->usb_samples_per_block)
            return PG2SDR_ERROR_TRANSFER_FORMAT;

        if (h.status & BLOCK_STATUS_ADC_OVERRUN)
            LOGDEBUG(dev, "ADC overrun");
        if (h.status & BLOCK_STATUS_DMA_ERROR)
            LOGDEBUG(dev, "DMA error");
        if (h.status & BLOCK_STATUS_PACKING_OVERRUN)
            LOGDEBUG(dev, "Packing overrun");
        if (h.status & BLOCK_STATUS_USB_OVERRUN)
            LOGDEBUG(dev, "USB overrun");

        if (offset != 0 && h.sequence != expected_sequence) {
            /* Sequence discontinuity, dispatch up to the end of the previous block */
            dispatch_contiguous_blocks(dev, dev_transfer->buffer + start, offset - start, callback, user_data);
            start = offset;
        }

        expected_sequence = h.sequence + 1;
    }

    /* Dispatch final block range (in the normal case, this will be the whole transfer) */
    dispatch_contiguous_blocks(dev, dev_transfer->buffer + start, bytelength - start, callback, user_data);
    return PG2SDR_SUCCESS;
}

/* submit one currently-idle transfer, link it into the active list */
static int submit_one_transfer(pg2sdr__transfer_state *dev_transfer)
{
    if (dev_transfer->state != XFER_IDLE)
        return PG2SDR_ERROR_BAD_STATE;

    pg2sdr_device *dev = dev_transfer->dev;

    dev_transfer->state = XFER_BUSY;
    int usb_error = libusb_submit_transfer(dev_transfer->transfer);
    if (usb_error < 0) {
        LOGERROR(dev, "failed to submit bulk transfer: %s", libusb_strerror(usb_error));
        dev_transfer->state = XFER_IDLE;
        return pg2sdr__translate_libusb_error(usb_error);
    }

    /* append this one to the active list */
    dev_transfer->next = NULL;
    if (!dev->active_transfers_tail) {
        dev->active_transfers_head = dev_transfer;
    } else {
        dev->active_transfers_tail->next = dev_transfer;
    }
    dev->active_transfers_tail = dev_transfer;

    return PG2SDR_SUCCESS;
}

/* submit all idle transfers */
static int submit_transfers(pg2sdr_device *dev)
{
    for (unsigned i = 0; i < dev->transfer_count; ++i) {
        if (dev->transfers[i].state == XFER_IDLE) {
            int error = submit_one_transfer(&dev->transfers[i]);
            if (error < 0)
                return error;
        }
    }

    return PG2SDR_SUCCESS;
}

/* cancel all submitted transfers; wait for all submitted transfers to complete */
static int drain_transfers(pg2sdr_device *dev)
{
    int error = PG2SDR_SUCCESS;
    dev->draining = true; /* ensure that blocks returned by the user are not resubmitted */

    cancel_transfers(dev);

    while (true) {
        dev->completion_flag = 0;
        memory_barrier();

        if (!check_any_transfer_busy(dev))
            break;

        /* still waiting for some transfers to complete */
        pthread_mutex_unlock(&dev->mutex);
        int usb_error = libusb_handle_events_completed(dev->ctx->libusb_ctx, &dev->completion_flag);
        pthread_mutex_lock(&dev->mutex);
        if (usb_error < 0 && usb_error != LIBUSB_ERROR_INTERRUPTED) {
            error = pg2sdr__translate_libusb_error(usb_error);
            goto cleanup;
        }
    }

    pg2sdr__transfer_state *prev = NULL;
    while (dev->active_transfers_head) {
        dev->active_transfers_head->state = XFER_IDLE;
        prev = dev->active_transfers_head;
        dev->active_transfers_head = dev->active_transfers_head->next;
        prev->next = NULL;
    }
    dev->active_transfers_tail = NULL;

cleanup:
    dev->draining = false;
    return error;
}

/* cancel all outstanding transfers */
static void cancel_transfers(pg2sdr_device *dev)
{
    for (unsigned i = 0; i < dev->transfer_count; ++i) {
        if (dev->transfers[i].state == XFER_BUSY) {
            libusb_cancel_transfer(dev->transfers[i].transfer);
        }
    }
}

/* return true if we have any submitted transfers that are still busy
 * (i.e. they are still owned by libusb and can't be freed yet)
 */
static bool check_any_transfer_busy(pg2sdr_device *dev)
{
    for (unsigned i = 0; i < dev->transfer_count; ++i) {
        if (dev->transfers[i].state == XFER_BUSY)
            return true;
    }

    return false;
}
