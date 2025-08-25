#include <cinttypes>

#include "util.h"

bool stub_callback(lpcsdr_sample_buffer *buffer, void *user_data) {
    static unsigned count = 0;
    static uint64_t expected = 0;

    fprintf(stderr, "%3u: %7u samples, timestamp: %10" PRIu64 ", expected: %10" PRIu64, count, buffer->count, buffer->timestamp, expected);
    if (count > 0 && buffer->timestamp != expected)
        fprintf(stderr, "  GAP! %" PRIu64 " samples dropped", buffer->timestamp - expected);
    fprintf(stderr, "\n");
    expected = buffer->timestamp + buffer->count;
    if (count++ == 1000) {
        lpcsdr_stop_streaming(buffer->dev);
    }

    return true;
}

TEST(STREAM_TEST, dev)
{
    Context ctx;
    DeviceHandle handle(ctx);

    int error;

#if 0
    error = lpcsdr_set_buffering(handle(), 10 * 13616);
    ASSERT_EQ(error, LPCSDR_SUCCESS) << "lpcsdr_set_buffering: " << lpcsdr_strerror(error);
#endif

    error = lpcsdr_set_sample_rate(handle(), 20000000);
    ASSERT_EQ(error, LPCSDR_SUCCESS) << "lpcsdr_set_sample_rate: " << lpcsdr_strerror(error);

    error = lpcsdr_stream_data(handle(), &stub_callback, NULL, 1000);
    ASSERT_EQ(error, LPCSDR_SUCCESS) << "lpcsdr_stream_data: " << lpcsdr_strerror(error);
}
