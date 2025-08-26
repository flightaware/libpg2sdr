#include "internal.h"
#include <pthread.h>

int lpcsdr_init(struct lpcsdr_context **ctx) {

    if (!ctx)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    lpcsdr_context *newctx;
    if (!(newctx = malloc(sizeof(*newctx))))
        return LPCSDR_ERROR_NO_MEMORY;

    newctx->magic = MAGIC_CTX;
    newctx->firmware_path = NULL;

    int usb_error;
    if ((usb_error = libusb_init(&newctx->libusb_ctx)) < 0) {
        free(newctx);
        return lpcsdr__translate_libusb_error(usb_error);
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

int lpcsdr_set_firmware_path(struct lpcsdr_context *ctx, const char *firmware_path) {
    CHECK_CTX(ctx);

    char *dup_path;
    if (!(dup_path = strdup(firmware_path)))
        return LPCSDR_ERROR_NO_MEMORY;

    ctx->firmware_path = dup_path;
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
