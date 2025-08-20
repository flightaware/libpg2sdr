#include <cinttypes>

#include "util.h"

bool stub_callback(void *buffer, void *user_data) {
    static unsigned count = 0;
    lpcsdr_sample_buffer *b = (lpcsdr_sample_buffer *) buffer;
    printf("seq: %" PRIu64 ", count: %u, our counter %u\n", b->timestamp, b->count, count);
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
