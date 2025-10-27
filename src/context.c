#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include "internal.h"

static void default_logger(lpcsdr_context *ctx, lpcsdr_log_level level, const char *message)
{
    if (level >= LPCSDR_LOG_INFO)
        fprintf(stderr, "liblpcsdr: %s\n", message);
}

static void debug_logger(lpcsdr_context *ctx, lpcsdr_log_level level, const char *message)
{
    fprintf(stderr, "liblpcsdr: %s\n", message);
}

int lpcsdr_init(lpcsdr_context **ctx) {

    if (!ctx)
        return LPCSDR_ERROR_BAD_ARGUMENT;

    lpcsdr_context *newctx;
    if (!(newctx = malloc(sizeof(*newctx))))
        return LPCSDR_ERROR_NO_MEMORY;

    newctx->magic = MAGIC_CTX;
    newctx->firmware_path = NULL;

    if (getenv("LPCSDR_DEBUG"))
        newctx->log_cb = debug_logger;
    else
        newctx->log_cb = default_logger;

    int usb_error;
    if ((usb_error = libusb_init(&newctx->libusb_ctx)) < 0) {
        free(newctx);
        return pg2sdr__translate_libusb_error(usb_error);
    }

    char *firmware_env = getenv("LPCSDR_FIRMWARE");
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

int lpcsdr_set_firmware_path(lpcsdr_context *ctx, const char *firmware_path) {
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

void pg2sdr__log(lpcsdr_context *ctx, lpcsdr_log_level level, const char *format, ...)
{
    if (!ctx || !ctx->log_cb)
        return;

    char buf[512];

    va_list ap;
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    ctx->log_cb(ctx, level, buf);
}
