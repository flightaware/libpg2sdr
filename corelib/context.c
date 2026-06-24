/*
 *  context.c - PG2 host library, library context management
 *
 *  Copyright (c) 2026 FlightAware All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "internal/core.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

static void default_logger(pg2sdr_context *ctx, pg2sdr_log_level level, const char *message)
{
    if (level >= PG2SDR_LOG_INFO)
        fprintf(stderr, "libpg2sdr: %s\n", message);
}

static void debug_logger(pg2sdr_context *ctx, pg2sdr_log_level level, const char *message)
{
    fprintf(stderr, "libpg2sdr: %s\n", message);
}

uint32_t pg2sdr_get_api_version(void)
{
    return PG2SDR_API_VERSION;
}

int pg2sdr__init_version(pg2sdr_context **ctx, uint32_t min_api_version)
{
    if (!ctx)
        return PG2SDR_ERROR_BAD_ARGUMENT;

    if (PG2SDR_API_VERSION < min_api_version)
        return PG2SDR_ERROR_LIBRARY_VERSION;

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
