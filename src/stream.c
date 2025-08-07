#include "internal.h"
#include <stdatomic.h>
#include <pthread.h>

#define memory_barrier() atomic_thread_fence(4)

static void free_block(lpcsdr_internal_sample_block *block)
{
    free(block->public_block.samples);
    free(block);
}

static void free_or_orphan_block(lpcsdr_internal_sample_block *block)
{
    if (block->busy) {
        block->orphan = true;
    } else {
        free_block(block);
    }
}

/* callback from libusb when a transfer completes.
 * We do no real processing here and just mark the transfer as completed,
 * then set the completion flag to avoid multithread event handler races
 * (see libusb docs)
 */
static void transfer_callback(struct libusb_transfer *xfer)
{
    printf("In callback\n");
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

/* submit one currently-idle transfer, link it into the active list */
static int submit_one_transfer(lpcsdr_transfer_state *dev_transfer)
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

static int find_unused_block(lpcsdr_device_handle *dev, lpcsdr_internal_sample_block **out) {
    lpcsdr_internal_sample_block *block = NULL;
    for (unsigned i = 0; i < dev->block_count; ++i) {
        if (!dev->blocks[i]->busy) {
            block = dev->blocks[i];
            break;
        }
    }

    if (!block) {
        return LPCSDR_SUCCESS;
    }

    *out = block;
    return LPCSDR_SUCCESS;
}

static void dispatch_block(lpcsdr_device_handle *dev, void *data, unsigned length, lpcsdr_stream_callback callback, void *user_data) {
    int error;
    lpcsdr_internal_sample_block *block = NULL;

    find_unused_block(dev, &block);
    
    if (block == NULL)
        return;

    switch(dev->conversion_mode) {
        case LPCSDR_LOWIF_REAL:
            if ((error = unpack_raw_adc_data(dev, (int16_t *) data, length, block->public_block.samples, 0, "stream_test_unpack.tsv")) < 0){
            }
            break;

        case LPCSDR_LOWIF_COMPLEX:
            //decimate?
            break;
    }
}

static int dispatch_transfer(lpcsdr_device_handle *dev, int stream_format, lpcsdr_transfer_state *dev_transfer, lpcsdr_stream_callback callback, void *user_data)
{
    int error;
    unsigned bytelength = dev_transfer->transfer->actual_length;
    
    if (bytelength % dev->usb_bytes_per_block_multiple != 0) {
        printf("lenght mismatch");
        return LPCSDR_BT_BLOCKLENGTH_MISMATCH;
    }

    // unsigned sequence = 1;

    ep1_header_t h = {};
    for (unsigned int offset = 0; offset < bytelength; offset += USB_BLOCK_SIZE) {
        unpack_header(offset, dev_transfer->buffer, &h);

        uint32_t block_len = h.block_len;
        if (h.magic != EXPECTED_BLOCK_HEADER_MAGIC || block_len % dev->usb_bytes_per_block_multiple) {
            printf("length mismatch at block %u\n", offset);
            return LPCSDR_ERROR_FIRMWARE_MISMATCH;
        }
        // if (h.sequence != sequence) {
        //     printf("sequence mismatch. Expected %u , got %u\n", sequence, h.sequence);
        //     return LPCSDR_ERROR_BAD_STATE;
        // }
        int16_t *out = calloc(h.samples, sizeof(int16_t));
        dispatch_block(dev, dev_transfer->buffer + offset, block_len, callback, user_data);

        // ++sequence;
    }
    return LPCSDR_SUCCESS;
}

int lpcsdr_stream_data(lpcsdr_device_handle *dev, lpcsdr_stream_callback callback, void *user_data, unsigned timeout_ms) {
    int error;

    if ((error = allocate_transfers(dev)) < 0)
        return error;

    if ((error = submit_transfers(dev)) < 0)
        return error;

    bool run = true;
    while (run) {

        dev->completion_flag = 0;

        if (dev->active_transfers_head->state == XFER_BUSY) {
            printf("Servicing...\n");
            struct timeval timeout = {/* tv_sec */ timeout_ms / 1000,
                                      /* tv_usec */ (timeout_ms % 1000) * 1000};
            error = libusb_handle_events_timeout_completed(dev->ctx->libusb_ctx, &timeout, &dev->completion_flag);
            if (error < 0 && error != LIBUSB_ERROR_INTERRUPTED) {
                return -1;
            }
            continue;
        }
        
        lpcsdr_transfer_state *current = dev->active_transfers_head;
        

        if (current->state != XFER_COMPLETED) {
            printf("failed\n");
            goto cleanup;
        }

        if (current->transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            error = -1;
            goto cleanup;
        }

        if ((error = dispatch_transfer(dev, -1, current, callback, user_data)) < 0) {
            goto cleanup;
        }

        current->state = XFER_IDLE;
        dev->active_transfers_head = current->next;
        if (!dev->active_transfers_head)
            dev->active_transfers_tail = NULL;

        if ((error = submit_one_transfer(current)) < 0) {
            printf("Submitting one transfer in loop \n");
            return error;
        }
    }

    return LPCSDR_SUCCESS;

cleanup:
    // drain and free stuff
    return error;
}

int lpcsdr_get_buffering(lpcsdr_device_handle *dev, unsigned *buffer_count, unsigned *buffer_size) {
    *buffer_count = dev->block_count;
    *buffer_size = dev->block_size;

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
    if ((error = lpcsdr_realloc_blocks(dev, buffer_count, buffer_size)) < 0) {
        goto done;
    }

    dev->block_count = buffer_count;
    dev->block_size = buffer_size;

done:
    pthread_mutex_unlock(&dev->mutex);
    return error;
}

int lpcsdr_realloc_blocks(lpcsdr_device_handle *dev, unsigned block_count, unsigned block_size_bytes) {

    int error;
    lpcsdr_internal_sample_block **blocks = NULL;

    dev->blocks = NULL;
    dev->block_count = 0;
    dev->block_size = 0;

    /* allocate user buffers */
    if (!(blocks = calloc(block_count, sizeof(*blocks)))) {
        error = LPCSDR_ERROR_NO_MEMORY;
        goto failed;
    }

    for (unsigned i = 0; i < block_count; ++i) {
        if (!(blocks[i] = calloc(1, sizeof(*blocks[i]))) || !(blocks[i]->public_block.samples = malloc(block_size_bytes))) {
            error = LPCSDR_ERROR_NO_MEMORY;
            goto failed;
        }

        blocks[i]->public_block.dev = dev;
        blocks[i]->busy = false;
        blocks[i]->orphan = false;
    }

    dev->blocks = blocks;
    return LPCSDR_SUCCESS;

failed:
    if (blocks) {
        for (unsigned i = 0; i < block_count; ++i) {
            free(blocks[i]->public_block.samples);
        }
        free(blocks);
    }
    return error;
}

void lpcsdr_free_blocks(lpcsdr_device_handle *dev)
{
    if (dev->blocks) {
        for (unsigned i = 0; i < dev->block_count; ++i) {
            free_or_orphan_block(dev->blocks[i]);
        }
    }

    free(dev->blocks);

    dev->blocks = NULL;
    dev->block_count = 0;
    dev->block_size = 0;
}