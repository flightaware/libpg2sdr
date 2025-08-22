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

TEST(STREAM_TEST, dev)
{
    Context ctx;
    DeviceHandle handle(ctx);

    int error = lpcsdr_set_buffering(handle(), 10, 13616);
    EXPECT_EQ(error, LPCSDR_SUCCESS) << "lpcsdr_set_buffering: " << lpcsdr_strerror(ctx(), error);

    error = lpcsdr_start_transfer(handle(), 5000000);
    ASSERT_EQ(error, LPCSDR_SUCCESS) << "lpcsdr_start_transfer: " << lpcsdr_strerror(ctx(), error);

    error = lpcsdr_stream_data(handle(), &stub_callback, NULL, 1000);
    EXPECT_EQ(error, LPCSDR_SUCCESS) << "lpcsdr_stream_data returned " << lpcsdr_strerror(ctx(), error);

    error = lpcsdr_stop_transfer(handle());
    EXPECT_EQ(error, LPCSDR_SUCCESS) << "lpcsdr_stop_transfer: " << lpcsdr_strerror(ctx(), error);
}
