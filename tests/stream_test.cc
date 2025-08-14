#include <gtest/gtest.h>
using namespace std;
#include <tuple> 
#include <iostream>
#include <fstream>

extern "C" {
    #include "internal.h"
}

int initialize_handle(lpcsdr_device_handle **handle) {
    lpcsdr_context *ctx;
    int error = LPCSDR_SUCCESS;
    if ((error = lpcsdr_init(&ctx) < 0)) {
        printf("Error initing lpc_context\n");
        return -1;
    }

    if ((error = lpcsdr_set_firmware_path(ctx, "/media/psf/soapy_shared_folder/liblpcsdr/lpcsdr_firmware/images/lpcsdr.bin")) < 0) {
        fprintf(stderr, "lpcsdr_set_firmware_path: %s\n", lpcsdr_strerror(ctx, error));
        return -1;
    }

    lpcsdr_device_handle *h;
    if ((error = lpcsdr_open_single_device(ctx, &h)) < 0) {
        fprintf(stderr, "lpcsdr_open_single_device: %s\n", lpcsdr_strerror(ctx, error));
        return -1;
    }

    *handle = h;
    return error;
}

bool stub_callback(void *buffer, void *user_data) {
    static unsigned count = 0;
    lpcsdr_sample_buffer *b = (lpcsdr_sample_buffer *) buffer;
    printf("seq: %u, count: %u, our coutner %u\n", b->timestamp, b->count, count);
    if (count == 100) {
        lpcsdr_stop_streaming(b->dev);
    }

    count++;
    return true;
}

TEST(STREAM_TEST, dev) {
    lpcsdr_device_handle *handle;
    assert(initialize_handle(&handle) == LPCSDR_SUCCESS);
    lpcsdr_set_buffering(handle, 10, 13616);
    lpcsdr_start_transfer(handle, 100000);

    int r = lpcsdr_stream_data(handle, &stub_callback, NULL, 1000);
    lpcsdr_stop_transfer(handle);
    EXPECT_EQ(r, LPCSDR_SUCCESS);
}