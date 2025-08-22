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
        if ((error = lpcsdr_init(&ctx)) < 0)
            throw std::runtime_error(std::string("context init failed: lpcsdr_init: ") + lpcsdr_strerror(error));
    }

    Context(const Context&) = delete;
    Context& operator= (const Context&) = delete;

    ~Context() {
        lpcsdr_exit(ctx);
    }

    lpcsdr_context *operator()(void) {
        return ctx;
    }

private:
    lpcsdr_context *ctx;
};

class DeviceHandle {
public:
    DeviceHandle(Context &ctx) : handle(nullptr) {
        int error;

        if (::getenv("LPCSDR_FIRMWARE_PATH") == nullptr) {
            if ((error = lpcsdr_set_firmware_path(ctx(), "/media/psf/soapy_shared_folder/liblpcsdr/lpcsdr_firmware/images/lpcsdr.bin")) < 0) {
                throw std::runtime_error(std::string("device setup failed: lpcsdr_set_firmware_path: ") + lpcsdr_strerror(error));
            }
        }

        if ((error = lpcsdr_open_single_device(ctx(), &handle)) < 0)
            throw std::runtime_error(std::string("device setup failed: lpcsdr_open_single_device: ") + lpcsdr_strerror(error));
    }

    DeviceHandle(const DeviceHandle&) = delete;
    DeviceHandle& operator= (const DeviceHandle&) = delete;

    ~DeviceHandle() {
        lpcsdr_close_device(handle);
    }

    lpcsdr_device_handle *operator()(void) {
        return handle;
    }

private:
    lpcsdr_device_handle  *handle;
};

#endif
