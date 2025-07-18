#include "internal.h"
#include <pthread.h>

int lpcsdr_set_firmware_path(struct lpcsdr_context *ctx, char *firmware_path) {
    CHECK_CTX(ctx);

    char *dup_path;
    if (!(dup_path = strdup(firmware_path)))
        return LPCSDR_ERROR_NO_MEMORY;

    ctx->firmware_path = dup_path;
    return LPCSDR_SUCCESS;
}

int lpcsdr_init(struct lpcsdr_context **ctx) {

    if (!ctx)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    lpcsdr_context *newctx;
    if (!(newctx = malloc(sizeof(*newctx))))
        return LPCSDR_ERROR_NO_MEMORY;

    newctx->magic = MAGIC_CTX;
    newctx->last_libusb_error = 0;
    newctx->last_errno = 0;
    newctx->firmware_path = NULL;

    if (libusb_init(&newctx->libusb_ctx) < 0) {
        free(newctx);
        return LPCSDR_ERROR_LIBUSB;
    }

    char *firmware_env = getenv("LPCSDR_FIRMWARE_PATH");
    if (firmware_env) {
        int error = lpcsdr_set_firmware_path(newctx, firmware_env);
        if (error < 0) {
            free(newctx->firmware_path);
            free(newctx);
            return error;
        }
    }
    
    *ctx = newctx;
    return LPCSDR_SUCCESS;
}

int lpcsdr_close_device(lpcsdr_device_handle *dev)
{
    CHECK_DEV(dev);

    if (!dev) {
        return LPCSDR_ERROR_BAD_ARGUMENT;
    }

    pthread_mutex_lock(&dev->mutex);
    if (dev->streaming) {
        pthread_mutex_unlock(&dev->mutex);
        return LPCSDR_ERROR_BUSY;
    }

    // pxsdr_free_blocks(dev);
    // pxsdr_free_decimators(dev);
    // lpcsdr_dsp_ifir_free(dev->baseband_filter);
    // pxsdr_dsp_decimate_free(dev->decimation_filter);
    libusb_close(dev->usb_handle);
    dev->magic = MAGIC_FREE;
    pthread_mutex_unlock(&dev->mutex);
    pthread_mutex_destroy(&dev->mutex);

    free(dev->ctx->firmware_path);
    free(dev->ctx);
    free(dev);

    return LPCSDR_SUCCESS;
}

int lpcsdr_set_log_callback(lpcsdr_context *ctx, lpcsdr_log_callback callback)
{
    CHECK_CTX(ctx);
    ctx->log_cb = callback;
    return LPCSDR_SUCCESS;
}


int lpcsdr_exit(lpcsdr_context *ctx)
{
    CHECK_CTX(ctx);

    libusb_exit(ctx->libusb_ctx);
    free(ctx->firmware_path);
    ctx->magic = MAGIC_FREE; /* try to detect use-after-free */
    free(ctx);
    return LPCSDR_SUCCESS;
}