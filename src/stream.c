#include "internal.h"
#include <stdatomic.h>
#include <pthread.h>

#define memory_barrier() atomic_thread_fence(4)

static int allocate_transfers(lpcsdr_device_handle *dev);
static void cancel_transfers(lpcsdr_device_handle *dev);
static int dispatch_transfer(lpcsdr_device_handle *dev, lpcsdr_transfer_state *dev_transfer, lpcsdr_stream_callback callback, void *user_data);
static int drain_transfers(lpcsdr_device_handle *dev);
static int submit_one_transfer(struct lpcsdr_transfer_state *dev_transfer);
static int submit_transfers(lpcsdr_device_handle *dev);
static void transfer_callback(struct libusb_transfer *xfer);
static bool check_any_transfer_busy(lpcsdr_device_handle *dev);

static lpcsdr_internal_sample_buffer *find_unused_buffer(lpcsdr_buffer_manager *bm);
static void free_or_orphan_buffer(lpcsdr_internal_sample_buffer *buffer);
static void release_buffer(lpcsdr_buffer_manager *m, lpcsdr_internal_sample_buffer *buffer);

static void dispatch_block(lpcsdr_device_handle *dev, void *data, unsigned length, lpcsdr_stream_callback callback, void *user_data);

static void dispatch_block(lpcsdr_device_handle *dev, void *data, unsigned length, lpcsdr_stream_callback callback, void *user_data) {
    lpcsdr_internal_sample_buffer *buffer = find_unused_buffer(dev->buffer_manager);
    ep1_header_t h;
    unpack_header(0, data, &h);
    if (buffer == NULL)
        return;

    switch(dev->conversion_mode) {
        case LPCSDR_LOWIF_REAL:
            buffer->public_buffer.count = unpack_raw_adc_data(dev, (uint8_t *) data, length, (int16_t *) buffer->public_buffer.samples, 0, "stream_test_unpack.tsv");
            break;

        case LPCSDR_LOWIF_COMPLEX:
            // Not implemented
            break;
    }
    buffer->public_buffer.timestamp = h.sequence;
    bool result = callback((lpcsdr_sample_buffer *)buffer, user_data);

    if (result) {
        release_buffer(dev->buffer_manager, buffer);
    }
}

int lpcsdr_stream_data(lpcsdr_device_handle *dev, lpcsdr_stream_callback callback, void *user_data, unsigned timeout_ms) {
    int error = LPCSDR_SUCCESS;
    int usb_error;

    if ((error = allocate_transfers(dev)) < 0)
        return error;

    if ((error = submit_transfers(dev)) < 0)
        return error;

    dev->streaming = true;

    while (!dev->draining) {
        dev->completion_flag = 0;

        if (dev->active_transfers_head->state == XFER_BUSY) {
            struct timeval timeout = {/* tv_sec */ timeout_ms / 1000,
                                      /* tv_usec */ (timeout_ms % 1000) * 1000};
            usb_error = libusb_handle_events_timeout_completed(dev->ctx->libusb_ctx, &timeout, &dev->completion_flag);
            if (usb_error < 0 && usb_error != LIBUSB_ERROR_INTERRUPTED) {
                /* no cleanup? */
                return lpcsdr_translate_libusb_error(dev->ctx, usb_error);
            }
            continue;
        }
        
        lpcsdr_transfer_state *current = dev->active_transfers_head;

        if (current->state != XFER_COMPLETED) {
            error = LPCSDR_ERROR_BAD_STATE;
            goto cleanup;
        }

        if (current->transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            error = lpcsdr_translate_libusb_transfer_status(dev->ctx, current->transfer->status);
            goto cleanup;
        }

        if ((error = dispatch_transfer(dev, current, callback, user_data)) < 0) {
            goto cleanup;
        }

        current->state = XFER_IDLE;
        dev->active_transfers_head = current->next;
        if (!dev->active_transfers_head)
            dev->active_transfers_tail = NULL;

        if ((error = submit_one_transfer(current)) < 0) {
            goto cleanup;
        }
    }

cleanup:
    int cleanup_error = drain_transfers(dev);
    if (cleanup_error < 0) {
        goto done;
    }

done:
    dev->streaming = false;
    dev->draining = false;
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
    dev->completion_flag = 1;
    memory_barrier();
    libusb_interrupt_event_handler(dev->ctx->libusb_ctx);

    pthread_mutex_unlock(&dev->mutex);
    return LPCSDR_SUCCESS;
}

//Buffer

static void free_buffer(lpcsdr_internal_sample_buffer *buffer)
{
    free(buffer->public_buffer.samples);
    free(buffer);
}

static void free_or_orphan_buffer(lpcsdr_internal_sample_buffer *buffer)
{
    if (buffer->busy) {
        buffer->orphan = true;
    } else {
        free_buffer(buffer);
    }
}

static lpcsdr_internal_sample_buffer *find_unused_buffer(lpcsdr_buffer_manager *bm) 
{
    if (bm->available_head) {
        lpcsdr_internal_sample_buffer *available_buffer = bm->available_head;
        bm->available_head = bm->available_head->next_available;
        available_buffer->next_available = NULL;
        available_buffer->busy = true;
        return available_buffer;
    }

    return NULL;
}

int lpcsdr_get_buffering(lpcsdr_device_handle *dev, unsigned *buffer_count, unsigned *buffer_size) {
    *buffer_count = dev->buffer_manager->buffer_count;
    *buffer_size = dev->buffer_manager->buffer_size;

    return LPCSDR_SUCCESS;
}

int lpcsdr_set_buffering(lpcsdr_device_handle *dev, unsigned buffer_count, unsigned buffer_size)
{
    CHECK_DEV(dev);

    int error;
    pthread_mutex_lock(&dev->mutex);

    /* if we fail to reallocate here, don't update the size/count; the old buffers are probably freed,
     * but we'll try to reallocate using the old values when we next start streaming
     */
    if ((error = lpcsdr_realloc_buffers(dev, buffer_count, buffer_size)) < 0) {
        goto done;
    }
    pthread_mutex_unlock(&dev->mutex);

    dev->buffer_manager->buffer_count = buffer_count;
    dev->buffer_manager->buffer_size = buffer_size;

done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

int lpcsdr_realloc_buffers(lpcsdr_device_handle *dev, unsigned buffer_count, unsigned buffer_size_bytes) {

    int error;
    lpcsdr_buffer_manager *m = calloc(1, sizeof(lpcsdr_buffer_manager));

    m->buffers = NULL;
    m->buffer_count = 0;
    m->buffer_size = 0;

    lpcsdr_internal_sample_buffer **buffers = NULL;
    lpcsdr_internal_sample_buffer *prev = NULL;

    /* allocate user buffers */
    if (!(buffers = calloc(buffer_count, sizeof(*buffers)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto failed;
    }

    for (unsigned i = 0; i < buffer_count; ++i) {
        if (!(buffers[i] = calloc(1, sizeof(*buffers[i]))) || !(buffers[i]->public_buffer.samples = malloc(buffer_size_bytes))) {
            error = LPCSDR_ERROR_NO_MEMORY;
            goto failed;
        }

        buffers[i]->public_buffer.dev = dev;
        buffers[i]->busy = false;
        buffers[i]->orphan = false;

        if (prev)
            prev->next_available = buffers[i];
        prev = buffers[i];
    }
    m->available_head = buffers[0];
    m->available_tail = buffers[buffer_count - 1];
    dev->buffer_manager = m;
    dev->buffer_manager->buffers = buffers;
    return LPCSDR_SUCCESS;

failed:
    if (buffers) {
        for (unsigned i = 0; i < buffer_count; ++i) {
            free(buffers[i]->public_buffer.samples);
            free(buffers[i]);
        }
        free(buffers);
    }
    return error;
}

static void release_buffer(lpcsdr_buffer_manager *m, lpcsdr_internal_sample_buffer *buffer)
{
    if (buffer->orphan) {
        /* we have reallocated while this block was busy; just free directly, don't reuse */
        free_buffer(buffer);
        return;
    }

    buffer->next_available = NULL;
    buffer->busy = false;

    if (!m->available_tail) {
        m->available_head = buffer;
    } else {
        m->available_tail->next_available = buffer;
    }
    m->available_tail = buffer;
}

void lpcsdr_free_buffers(lpcsdr_buffer_manager *bm)
{
    if (bm->buffers) {
        for (unsigned i = 0; i < bm->buffer_count; ++i) {
            free_or_orphan_buffer(bm->buffers[i]);
        }
    }

    free(bm->buffers);
    free(bm);
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
    dev_transfer->dev->completion_flag = 1;
    memory_barrier();
    dev_transfer->state = XFER_COMPLETED;
}

static int allocate_transfers(lpcsdr_device_handle *dev) {
    int error;
    unsigned transfer_size = USB_BLOCK_SIZE; /* hardcoded for now. */
    unsigned transfer_count = 4;

    lpcsdr_transfer_state *transfers = calloc(transfer_count, sizeof(lpcsdr_transfer_state));
    if (!transfers) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto failed;
    }

    for (unsigned i = 0; i < transfer_count; ++i) {
        if (!(transfers[i].buffer = malloc(transfer_size))) {
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

        libusb_fill_bulk_transfer(transfers[i].transfer, /* transfer to populate */
                                  dev->usb_handle,       /* usb device */
                                  0x81,                  /* endpoint number, EP 1 IN */
                                  transfers[i].buffer,   /* buffer to fill */
                                  transfer_size,         /* max bytes to receive */
                                  transfer_callback,     /* callback on completion */
                                  (void *)&transfers[i], /* callback user data */
                                  500);
        transfers[i].transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

        /* rest of the metadata */
        transfers[i].dev = dev;
        transfers[i].state = XFER_IDLE;
        transfers[i].next = NULL;
    }

    dev->transfers = transfers;
    dev->transfer_count = transfer_count;
    dev->transfer_size = transfer_size;

    return LPCSDR_SUCCESS;

failed:
    if (transfers) {
        for (unsigned i = 0; i < transfer_count; ++i) {
            if (transfers[i].transfer)
                libusb_free_transfer(transfers[i].transfer);

            if (transfers[i].buffer) {
                free(transfers[i].buffer);
            }
        }
        free(transfers);
    }
    return error;
}

static int dispatch_transfer(lpcsdr_device_handle *dev, lpcsdr_transfer_state *dev_transfer, lpcsdr_stream_callback callback, void *user_data)
{
    unsigned bytelength = dev_transfer->transfer->actual_length;
    
    if (bytelength % dev->usb_bytes_per_block_multiple != 0) {
        return LPCSDR_BT_BLOCKLENGTH_MISMATCH;
    }

    ep1_header_t h = {};
    for (unsigned int offset = 0; offset < bytelength; offset += USB_BLOCK_SIZE) {
        unpack_header(offset, dev_transfer->buffer, &h);

        uint32_t block_len = h.block_len;
        if (h.magic != EXPECTED_BLOCK_HEADER_MAGIC) {
            return LPCSDR_BT_MAGIC_MISMATCH;
        }
        if (h.block_len % dev->usb_bytes_per_block_multiple != 0) {
            return LPCSDR_BT_BLOCKLENGTH_MISMATCH;
        }
        if (h.samples % dev->usb_samples_per_block_multiple != 0) {
            return LPCSDR_BT_SAMPLELENGTH_MISMATCH;
        }

        printf("block seq %u\n", h.sequence);
        dispatch_block(dev, dev_transfer->buffer + offset, block_len, callback, user_data);
    }
    return LPCSDR_SUCCESS;

}

/* submit one currently-idle transfer, link it into the active list */
static int submit_one_transfer(struct lpcsdr_transfer_state *dev_transfer)
{
    if (dev_transfer->state != XFER_IDLE)
        return LPCSDR_ERROR_BAD_STATE;

    lpcsdr_device_handle *dev = dev_transfer->dev;

    dev_transfer->state = XFER_BUSY;
    int error = libusb_submit_transfer(dev_transfer->transfer);
    if (error < 0) {
        dev_transfer->state = XFER_IDLE;
        return lpcsdr_translate_libusb_error(dev->ctx, error);
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

static void cancel_transfers(lpcsdr_device_handle *dev)
{
    /* cancel all outstanding transfers */
    for (unsigned i = 0; i < dev->transfer_count; ++i) {
        if (dev->transfers[i].state == XFER_BUSY) {
            libusb_cancel_transfer(dev->transfers[i].transfer);
        }
    }
}

static bool check_any_transfer_busy(lpcsdr_device_handle *dev)
{
    for (unsigned i = 0; i < dev->transfer_count; ++i) {
        if (dev->transfers[i].state == XFER_BUSY)
            return true;
    }

    return false;
}