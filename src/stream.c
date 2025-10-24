#include <stdatomic.h>
#include <pthread.h>
#include <assert.h>

#include "internal.h"

#define memory_barrier() atomic_thread_fence(memory_order_acq_rel)

static void unpack_raw_adc_data(const uint32_t *in, uint32_t words, int16_t *out);
static size_t convert_adc_blocks(lpcsdr_device_handle *dev, const uint8_t *data, size_t length, int16_t *out);
static void dispatch_contiguous_blocks(lpcsdr_device_handle *dev, const uint8_t *data, size_t length, lpcsdr_stream_callback callback, void *user_data);

static lpcsdr_sample_buffer *get_buffer(lpcsdr_device_handle *dev);

static int allocate_transfers(lpcsdr_device_handle *dev);
static void cancel_transfers(lpcsdr_device_handle *dev);
static void free_transfers(lpcsdr_transfer_state *transfers, unsigned transfer_count);
static void free_dev_transfers(lpcsdr_device_handle *dev);
static int dispatch_transfer(lpcsdr_device_handle *dev, lpcsdr_transfer_state *dev_transfer, lpcsdr_stream_callback callback, void *user_data);
static int drain_transfers(lpcsdr_device_handle *dev);
static int submit_one_transfer(struct lpcsdr_transfer_state *dev_transfer);
static int submit_transfers(lpcsdr_device_handle *dev);
static void transfer_callback(struct libusb_transfer *xfer);
static bool check_any_transfer_busy(lpcsdr_device_handle *dev);

static size_t user_sample_size(lpcsdr_device_handle *dev)
{
    return (dev->conversion_mode == LPCSDR_MODE_BASEBAND ? sizeof(cs16_t) : sizeof(int16_t));
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

/* Convert "words" uint32_t words of packed ADC data stored at "in":
 *   1. unpack the sample values from the packed representation
 *   2. scale from 12-bit twos-complement to 16-bit twos-complement
 *   3. write results to "out"
 *
 * caller must ensure:
 *   "words" is a multiple of 3
 *   "out" has at least enough space for (words * 8 / 3) int16_t values
 */
static void unpack_raw_adc_data(const uint32_t *in, uint32_t words, int16_t *out)
{
    /* This function is a candidate for use of starch */

    assert(words % 3 == 0);

    for (unsigned i = 0; i < words; i += 3, in += 3, out += 8) {
        uint32_t first = in[0];
        uint32_t second = in[1];
        uint32_t third = in[2];

        /* 12->16 bit scaling is baked into the bitshifts here */
        out[0] = (int16_t) ((first  & 0x00000FFF) << 4);
        out[1] = (int16_t) ((first  & 0x0FFF0000) >> 12);
        out[2] = (int16_t) ((second & 0x00000FFF) << 4);
        out[3] = (int16_t) ((second & 0x0FFF0000) >> 12);
        out[4] = (int16_t) ((third  & 0x00000FFF) << 4);
        out[5] = (int16_t) ((third  & 0x0FFF0000) >> 12);
        out[6] = (int16_t) (((first & 0x0000F000)) | ((second & 0x0000F000) >> 4) | ((third & 0x0000F000) >> 8));
        out[7] = (int16_t) (((first & 0xF0000000) >> 16) | ((second & 0xF0000000) >> 20)  | ((third & 0xF0000000) >> 24));
    }
}

/* Like `unpack_raw_adc_data`, but also invert the spectrum */
static void unpack_raw_adc_data_invert(const uint32_t *in, uint32_t words, int16_t *out)
{
    /* This function is a candidate for use of starch */

    assert(words % 3 == 0);

    for (unsigned i = 0; i < words; i += 3, in += 3, out += 8) {
        uint32_t first = in[0];
        uint32_t second = in[1];
        uint32_t third = in[2];

        /* 12->16 bit scaling is baked into the bitshifts here */
        out[0] = (int16_t) ((first  & 0x00000FFF) << 4);
        out[1] = (int16_t) -((first  & 0x0FFF0000) >> 12);
        out[2] = (int16_t) ((second & 0x00000FFF) << 4);
        out[3] = (int16_t) -((second & 0x0FFF0000) >> 12);
        out[4] = (int16_t) ((third  & 0x00000FFF) << 4);
        out[5] = (int16_t) -((third  & 0x0FFF0000) >> 12);
        out[6] = (int16_t) (((first & 0x0000F000)) | ((second & 0x0000F000) >> 4) | ((third & 0x0000F000) >> 8));
        out[7] = (int16_t) -(((first & 0xF0000000) >> 16) | ((second & 0xF0000000) >> 20)  | ((third & 0xF0000000) >> 24));
    }
}

/* Get a fresh lpcsdr_sample_buffer that can be used to fill with data and pass to user code */
static lpcsdr_sample_buffer *get_buffer(lpcsdr_device_handle *dev)
{
    /* For now, we just allocate on demand and let libc's malloc implementation deal with it.
     * Later, may be worth revisiting to do our own free-list if malloc doesn't perform well.
     */
    lpcsdr_sample_buffer *buf = malloc(sizeof(lpcsdr_sample_buffer) + dev->buffer_size * user_sample_size(dev));

    buf->dev = dev;
    buf->samples = (int16_t*) (buf + 1); /* sample data immediately follows the header */
    buf->count = 0;
    buf->timestamp = 0;

    return buf;
}

/* Release a lpcsdr_sample_buffer that is no longer used by user code */
void lpcsdr_release_buffer(lpcsdr_sample_buffer *buf)
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
static size_t convert_adc_blocks(lpcsdr_device_handle *dev, const uint8_t *data, size_t length, int16_t *out)
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
    const bool inverted_spectrum = (!dev->upper_sideband) ^ ((dev->undersampling_mode & 1) == 0);

    switch(dev->conversion_mode) {
    case LPCSDR_MODE_LOWIF_REAL:
        {
            /* unpack ADC data into out directly */
            unsigned count = 0;
            for (unsigned i = 0; i < length; i += bpb, count += spb)
                unpack_raw_adc_data((const uint32_t*) (data + i + sizeof(ep1_header_t)), swpb, out + count);
            return count;
        }

    case LPCSDR_MODE_BASEBAND:
        {
            /* unpack ADC data into work_buffer[0] */
            unsigned count = 0;
            if (!inverted_spectrum) {
                for (unsigned i = 0; i < length; i += bpb, count += spb)
                    unpack_raw_adc_data((const uint32_t*) (data + i + sizeof(ep1_header_t)), swpb, (int16_t*)dev->work_buffer[0] + count);
            } else {
                for (unsigned i = 0; i < length; i += bpb, count += spb)
                    unpack_raw_adc_data_invert((const uint32_t*) (data + i + sizeof(ep1_header_t)), swpb, (int16_t*)dev->work_buffer[0] + count);
            }

            /* work_buffer[0] now contains uninverted-spectrum ADC data, with the signal we want centered at Fs/4 */

            if (dev->post_decimation) {
                /* do downconversion from work_buffer[0] -> work_buffer[1] */
                count = lpcsdr__dsp_downconvert_process(dev->downconverter, (int16_t*)dev->work_buffer[0], count, (cs16_t*)dev->work_buffer[1]);

                /* work_buffer[1] now contains complex baseband data, with the signal we want centered at 0Hz,
                 * but at a higher sampling rate than the user requested. Decimate to bring the sampling rate
                 * down to what was requested.
                 */

                /* do all but the final decimate-by-2 step, alternating work buffers */
                unsigned work_in = 1;
                for (unsigned i = 0; i < (dev->post_decimation - 1); ++i) {
                    unsigned work_out = !work_in;
                    count = lpcsdr__dsp_halfband_decimate_process(dev->post_decimators[i], (cs16_t*)dev->work_buffer[work_in], count, (cs16_t*)dev->work_buffer[work_out]);
                    work_in = work_out;
                }

                /* do final decimate-by-2 step directly to out */
                count = lpcsdr__dsp_halfband_decimate_process(dev->post_decimators[dev->post_decimation - 1], (cs16_t*)dev->work_buffer[work_in], count, (cs16_t*)out);
            } else {
                /* No extra decimation, do downconversion from work_buffer[0] -> out */
                count = lpcsdr__dsp_downconvert_process(dev->downconverter, (int16_t*)dev->work_buffer[0], count, (cs16_t*)out);
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
static void dispatch_contiguous_blocks(lpcsdr_device_handle *dev, const uint8_t *data, size_t length, lpcsdr_stream_callback callback, void *user_data)
{
    assert (length % dev->usb_bytes_per_block == 0);

    /* todo: this is only true for LPCSDR_MODE_LOWIF_REAL with int16, update once we support other versions */
    const unsigned adc_samples = length / dev->usb_bytes_per_block * dev->usb_samples_per_block;
    const unsigned user_samples = adc_samples / dev->adc_samples_per_user_sample;
    assert (user_samples <= dev->buffer_size);

    lpcsdr_sample_buffer *buffer = get_buffer(dev);
    if (!buffer)
        return; /* no buffers available, drop this data on the floor */

    unsigned count = convert_adc_blocks(dev, data, length, (int16_t*) buffer->samples);

    ep1_header_t h;
    unpack_header(data, &h);

    buffer->timestamp = (h.sequence * dev->usb_samples_per_block - dev->partial_samples) / dev->adc_samples_per_user_sample;
    buffer->count = count;
    dev->partial_samples = (dev->partial_samples + adc_samples) % dev->adc_samples_per_user_sample;

    bool result = callback(buffer, user_data);
    if (result) {
        lpcsdr_release_buffer(buffer);
    }
}

int lpcsdr_stream_data(lpcsdr_device_handle *dev, lpcsdr_stream_callback callback, void *user_data, unsigned timeout_ms)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    if (dev->streaming) {
        LOGDEBUG(dev, "lpcsdr_stream_data: already streaming");
        pthread_mutex_unlock(&dev->mutex);
        return LPCSDR_ERROR_BAD_STATE;
    }

    int error, usb_error;

    /* ensure tuner/ADC configuration is up to date */
    if ((error = lpcsdr_apply_changes(dev)) < 0) {
        LOGDEBUG(dev, "lpcsdr_stream_data: apply_changes failed");
        goto done;
    }

    if (!dev->adc_pll_config.valid) {
        /* sample rate not set */
        LOGDEBUG(dev, "lpcsdr_stream_data: sample rate not set");
        error = LPCSDR_ERROR_BAD_STATE;
        goto done;
    }

    if (!timeout_ms) {
        /* estimate time for one transfer to complete, add an arbitrary 500ms */
        double fill_time_ms = 1000.0 * dev->adc_samples_per_transfer / dev->adc_pll_config.actual_frequency;
        timeout_ms = (unsigned) (fill_time_ms + 500);
    }

    if ((error = allocate_transfers(dev)) < 0) {
        LOGDEBUG(dev, "lpcsdr_stream_data: allocate_transfers failed");
        goto done;
    }

    /* if we're downconverting, set up the DSP state & work buffers */
    if (dev->conversion_mode == LPCSDR_MODE_BASEBAND) {
        if (!(dev->downconverter = lpcsdr__dsp_downconvert_create(/* ntaps */ lpcsdr__dsp_default_halfband_ntaps,
                                                                  /* taps */ lpcsdr__dsp_default_halfband_taps,
                                                                  /* max_in_length */ dev->adc_samples_per_transfer))) {
            LOGDEBUG(dev, "lpcsdr_stream_data: dsp_create_downconvert failed");
            error = LPCSDR_ERROR_NO_MEMORY;
            goto cleanup;
        }

        for (unsigned i = 0; i < 2; ++i) {
            if (!(dev->work_buffer[i] = malloc(dev->adc_samples_per_transfer * sizeof(int16_t)))) {
                LOGDEBUG(dev, "lpcsdr_stream_data: work buffer allocation failed");
                error = LPCSDR_ERROR_NO_MEMORY;
                goto cleanup;
            }
        }

        for (unsigned i = 0; i < dev->post_decimation; ++i) {
            if (!(dev->post_decimators[i] = lpcsdr__dsp_halfband_decimate_create(/* ntaps */ lpcsdr__dsp_default_halfband_ntaps,
                                                                                 /* taps */ lpcsdr__dsp_default_halfband_taps))) {
                LOGDEBUG(dev, "lpcsdr_stream_data: dsp_halfband_decimate_create failed");
                error = LPCSDR_ERROR_NO_MEMORY;
                goto cleanup;
            }
        }
    }

    dev->partial_samples = 0;

    /* clear any endpoint halt first */
    if ((usb_error = libusb_clear_halt(dev->usb_handle, 0x81)) < 0) {
        LOGDEBUG(dev, "lpcsdr_stream_data: libusb_clear_halt failed");
        error = lpcsdr__translate_libusb_error(usb_error);
        goto cleanup;
    }

    LOGDEBUG(dev,
             "starting ADC transfers with:\n"
             "  N: %u\n"
             "  M: %.5f\n"
             "  P: %u\n"
             "  I: %u\n"
             "  fCCO: %.2f MHz\n"
             "  fADC: %.2f MHz\n",
             dev->adc_pll_config.n,
             dev->adc_pll_config.m,
             dev->adc_pll_config.p,
             dev->adc_pll_config.i,
             dev->adc_pll_config.actual_fcco / 1e6,
             dev->adc_pll_config.actual_frequency / 1e6);

    if ((error = lpcsdr__ctrl_start_transfer(dev, &dev->adc_pll_config)) < 0) {        
        LOGDEBUG(dev, "lpcsdr_stream_data: start_transfer failed");
        goto cleanup;
    }

    if ((error = submit_transfers(dev)) < 0) {
        LOGDEBUG(dev, "lpcsdr_stream_data: submit_transfers failed");        
        goto drain;
    }

    dev->streaming = true;

    while (true) {
        dev->completion_flag = 0;
        memory_barrier();

        if (dev->draining) {
            LOGDEBUG(dev, "lpcsdr_stream_data: something set the draining flag, stopping");
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
                LOGDEBUG(dev, "lpcsdr_stream_data: got a libusb error in the main loop: %s", libusb_strerror(usb_error));
                error = lpcsdr__translate_libusb_error(usb_error);
                goto drain;
            }
            continue;
        }

        lpcsdr_transfer_state *current = dev->active_transfers_head;

        if (current->state != XFER_COMPLETED) {
            LOGERROR(dev, "lpcsdr_stream_data: active transfer has state %d", (int)current->state);
            error = LPCSDR_ERROR_BAD_STATE;
            goto drain;
        }

        if (current->transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            LOGERROR(dev, "lpcsdr_stream_data: bulk transfer failed with status %d", (int)current->transfer->status);
            error = lpcsdr__translate_libusb_transfer_status(current->transfer->status);
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
    /* return value ignored */ lpcsdr__ctrl_stop_transfer(dev);

    int cleanup_error = drain_transfers(dev);
    if (cleanup_error < 0) {
        goto done;
    }

 cleanup:
    for (unsigned i = 0; i < LPCSDR_DECIMATION_MAX; ++i) {
        lpcsdr__dsp_halfband_decimate_free(dev->post_decimators[i]);
        dev->post_decimators[i] = NULL;
    }

    lpcsdr__dsp_downconvert_free(dev->downconverter);
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

int lpcsdr_stop_streaming(lpcsdr_device_handle *dev)
{
    CHECK_DEV(dev);

    pthread_mutex_lock(&dev->mutex);
    if (!dev->streaming) {
        pthread_mutex_unlock(&dev->mutex);
        return LPCSDR_ERROR_BAD_STATE;
    }

    dev->draining = true;

    /* wake up the streaming thread */
    memory_barrier();
    dev->completion_flag = 1;
    libusb_interrupt_event_handler(dev->ctx->libusb_ctx);

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

//Transfers

/* callback from libusb when a transfer completes.
 * We do no real processing here and just mark the transfer as completed,
 * then set the completion flag to avoid multithread event handler races
 * (see libusb docs)
 */
static void transfer_callback(struct libusb_transfer *xfer)
{
    lpcsdr_transfer_state *dev_transfer = (lpcsdr_transfer_state *)xfer->user_data;
    dev_transfer->state = XFER_COMPLETED;
    memory_barrier();
    dev_transfer->dev->completion_flag = 1;
}

/* (re)allocate libusb transfers and associated buffers for the current buffer size */
static int allocate_transfers(lpcsdr_device_handle *dev)
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
    unsigned transfer_timeout_ms = (unsigned) (fill_time_ms + 500); /* half a second of slop */

    LOGDEBUG(dev,
             "allocate_transfers: \n"
             "  buffer_size              %zu\n"
             "  usb_transfer_size        %u\n"
             "  adc_samples_per_transfer %u\n"
             "  transfer_count           %u\n"
             "  transfer_timeout_ms      %u\n",
             dev->buffer_size,
             dev->usb_transfer_size,
             dev->adc_samples_per_transfer,
             transfer_count,
             transfer_timeout_ms);

    lpcsdr_transfer_state *transfers = calloc(transfer_count, sizeof(lpcsdr_transfer_state));
    if (!transfers) {
        error = LPCSDR_ERROR_NO_MEMORY;
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
            error = LPCSDR_ERROR_NO_MEMORY;
            goto failed;
        }
    }

    for (unsigned i = 0; i < transfer_count; ++i) {
        /* set up the transfer */
        if (!(transfers[i].transfer = libusb_alloc_transfer(0))) {
            error = LPCSDR_ERROR_NO_MEMORY;
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

    return LPCSDR_SUCCESS;

failed:
    free_transfers(transfers, transfer_count);
    return error;
}

/* free the contents of "transfers", and the array itself */
static void free_transfers(lpcsdr_transfer_state *transfers, unsigned transfer_count)
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
static void free_dev_transfers(lpcsdr_device_handle *dev)
{
    free_transfers(dev->transfers, dev->transfer_count);

    dev->transfers = NULL;
    dev->transfer_count = 0;
}

/* dispatch one completed USB transfer to user code */
static int dispatch_transfer(lpcsdr_device_handle *dev, lpcsdr_transfer_state *dev_transfer, lpcsdr_stream_callback callback, void *user_data)
{
    unsigned bytelength = dev_transfer->transfer->actual_length;

    if (bytelength % dev->usb_bytes_per_block != 0) {
        return LPCSDR_ERROR_TRANSFER_FORMAT;
    }

    /* scan through the received data, validating headers and looking for sequence discontinuities.
     * Each user callback provides only a single sample timestamp, so if there are any discontinuities,
     * we need to do a separate callback for each range of contiguous sequence numbers so the user code
     * can see the gaps.
     */
    ep1_header_t h;
    uint32_t expected_sequence = 0;
    unsigned start = 0;
    for (unsigned int offset = 0; offset < bytelength; offset += dev->usb_bytes_per_block) {
        unpack_header(dev_transfer->buffer + offset, &h);

        if (h.magic != BLOCK_MAGIC || h.block_len != dev->usb_bytes_per_block || h.samples != dev->usb_samples_per_block)
            return LPCSDR_ERROR_TRANSFER_FORMAT;

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
    return LPCSDR_SUCCESS;
}

/* submit one currently-idle transfer, link it into the active list */
static int submit_one_transfer(struct lpcsdr_transfer_state *dev_transfer)
{
    if (dev_transfer->state != XFER_IDLE)
        return LPCSDR_ERROR_BAD_STATE;

    lpcsdr_device_handle *dev = dev_transfer->dev;

    dev_transfer->state = XFER_BUSY;
    int usb_error = libusb_submit_transfer(dev_transfer->transfer);
    if (usb_error < 0) {
        LOGERROR(dev, "failed to submit bulk transfer: %s", libusb_strerror(usb_error));
        dev_transfer->state = XFER_IDLE;
        return lpcsdr__translate_libusb_error(usb_error);
    }

    /* append this one to the active list */
    dev_transfer->next = NULL;
    if (!dev->active_transfers_tail) {
        dev->active_transfers_head = dev_transfer;
    } else {
        dev->active_transfers_tail->next = dev_transfer;
    }
    dev->active_transfers_tail = dev_transfer;

    return LPCSDR_SUCCESS;
}

/* submit all idle transfers */
static int submit_transfers(lpcsdr_device_handle *dev)
{
    for (unsigned i = 0; i < dev->transfer_count; ++i) {
        if (dev->transfers[i].state == XFER_IDLE) {
            int error = submit_one_transfer(&dev->transfers[i]);
            if (error < 0)
                return error;
        }
    }

    return LPCSDR_SUCCESS;
}

/* cancel all submitted transfers; wait for all submitted transfers to complete */
static int drain_transfers(lpcsdr_device_handle *dev)
{
    int error = LPCSDR_SUCCESS;
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
            error = lpcsdr__translate_libusb_error(usb_error);
            goto cleanup;
        }
    }

    lpcsdr_transfer_state *prev = NULL;
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
static void cancel_transfers(lpcsdr_device_handle *dev)
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
static bool check_any_transfer_busy(lpcsdr_device_handle *dev)
{
    for (unsigned i = 0; i < dev->transfer_count; ++i) {
        if (dev->transfers[i].state == XFER_BUSY)
            return true;
    }

    return false;
}
