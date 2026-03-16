#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include "internal/lib.h"

static void default_logger(pg2sdr_context *ctx, pg2sdr_log_level level, const char *message)
{
    if (level >= PG2SDR_LOG_INFO)
        fprintf(stderr, "libpg2sdr: %s\n", message);
}

static void debug_logger(pg2sdr_context *ctx, pg2sdr_log_level level, const char *message)
{
    fprintf(stderr, "libpg2sdr: %s\n", message);
}

int pg2sdr_init(pg2sdr_context **ctx) {

    if (!ctx)
        return PG2SDR_ERROR_BAD_ARGUMENT;

    pg2sdr_context *newctx;
    if (!(newctx = malloc(sizeof(*newctx))))
        return PG2SDR_ERROR_NO_MEMORY;

    newctx->magic = MAGIC_CTX;

    if (getenv("PG2SDR_DEBUG"))
        newctx->log_cb = debug_logger;
    else
        newctx->log_cb = default_logger;

    int usb_error;
    if ((usb_error = libusb_init(&newctx->libusb_ctx)) < 0) {
        free(newctx);
        return pg2sdr__translate_libusb_error(usb_error);
    }

    *ctx = newctx;
    return PG2SDR_SUCCESS;
}

int pg2sdr_set_log_callback(pg2sdr_context *ctx, pg2sdr_log_callback callback)
{
    CHECK_CTX(ctx);
    ctx->log_cb = callback;
    return PG2SDR_SUCCESS;
}


int pg2sdr_exit(pg2sdr_context *ctx)
{
    CHECK_CTX(ctx);

    libusb_exit(ctx->libusb_ctx);
    ctx->magic = MAGIC_FREE; /* try to detect use-after-free */
    free(ctx);
    return PG2SDR_SUCCESS;
}

void pg2sdr__log(pg2sdr_context *ctx, pg2sdr_log_level level, const char *format, ...)
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
