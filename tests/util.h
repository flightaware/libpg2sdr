#ifndef TESTS_UTIL_H
#define TESTS_UTIL_H

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

extern "C" {
    #include "internal.h"
}

class Context {
public:
    Context() : ctx(nullptr) {
        int error;
        if ((error = pg2sdr_init(&ctx)) < 0)
            throw std::runtime_error(std::string("context init failed: pg2sdr_init: ") + pg2sdr_strerror(error));
    }

    Context(const Context&) = delete;
    Context& operator= (const Context&) = delete;

    ~Context() {
        pg2sdr_exit(ctx);
    }

    pg2sdr_context *operator()(void) {
        return ctx;
    }

private:
    pg2sdr_context *ctx;
};

class DeviceHandle {
public:
    DeviceHandle(Context &ctx) : handle(nullptr) {
        int error;
        if ((error = pg2sdr_open_single_device(ctx(), NULL, NULL, &handle)) < 0)
            throw std::runtime_error(std::string("device setup failed: pg2sdr_open_single_device: ") + pg2sdr_strerror(error));
    }

    DeviceHandle(const DeviceHandle&) = delete;
    DeviceHandle& operator= (const DeviceHandle&) = delete;

    ~DeviceHandle() {
        pg2sdr_close_device(handle);
    }

    pg2sdr_device *operator()(void) {
        return handle;
    }

private:
    pg2sdr_device  *handle;
};

#endif
