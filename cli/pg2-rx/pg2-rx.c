#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include <pg2sdr.h>

static const char *argv0;

static int pg2sdr_perror(const char *what, int error)
{
    fprintf(stderr, "%s: %s: %s\n", argv0, what, pg2sdr_strerror(error));
    return EXIT_FAILURE;
}

static volatile bool stop_flag = false;
static int outfile = -1;
static uint64_t next_timestamp = 0;
static uint64_t total_samples = 0;
static uint64_t total_dropped = 0;
static uint64_t sample_limit = 0;
static struct timespec time_limit_ts;
static bool quiet = false;
static bool verbose = false;
static bool write_error = false;

typedef enum {
    FMT_UINT8,
    FMT_INT16,
    FMT_INT32,
    FMT_FLOAT32,
    FMT_FLOAT64
} output_format_t;

static output_format_t output_format = FMT_INT16;

static void log_callback(pg2sdr_context *context,
                         pg2sdr_log_level level,
                         const char *message)
{
    const char *level_str;
    switch (level) {
    case PG2SDR_LOG_ERROR:
        level_str = "error";
        break;
    case PG2SDR_LOG_INFO:
        if (quiet)
            return;
        level_str = "info";
        break;
    case PG2SDR_LOG_DEBUG:
        if (!verbose)
            return;
        level_str = "debug";
        break;
    default:
        return;
    }
    fprintf(stderr, "libpg2sdr (%s): %s\n", level_str, message);
}

static void stop_streaming(int sig)
{
    /* pg2sdr_stop_streaming() is thread-safe but _not_ async-signal-safe,
     * so only set a flag here and handle it the next time stream_callback is called
     */
    stop_flag = true;
}

/*
 * Convert samples in "buffer" to "format", returning a newly-allocated buffer with the
 * converted data. Store the size (in bytes) of the newly-allocated buffer in *bytecount.
 */
static uint8_t *convert_samples(pg2sdr_sample_buffer *buffer, output_format_t format, size_t *bytecount)
{
    size_t int16_count;
    if (buffer->mode == PG2SDR_MODE_BASEBAND)
        int16_count = buffer->count * 2;
    else
        int16_count = buffer->count;

    size_t bytes_per_value;
    switch (format) {
    case FMT_UINT8: bytes_per_value = sizeof(uint8_t); break;
    case FMT_INT16: bytes_per_value = sizeof(int16_t); break;
    case FMT_INT32: bytes_per_value = sizeof(int32_t); break;
    case FMT_FLOAT32: bytes_per_value = sizeof(_Float32); break;
    case FMT_FLOAT64: bytes_per_value = sizeof(_Float64); break;
    default: abort();
    }

    size_t outbuf_size = int16_count * bytes_per_value;
    *bytecount = outbuf_size;

    if (format == FMT_INT16) {
        /* no conversion necessary, reuse sample buffer directly */
        return (uint8_t*) buffer->samples;
    }

    uint8_t *outbuf = malloc(outbuf_size);
    if (!outbuf) {
        perror("malloc");
        abort();
    }

    switch (format) {
    case FMT_UINT8:
        for (size_t i = 0; i < int16_count; ++i)
            outbuf[i] = (buffer->samples[i] / 256) + 128;
        break;

    case FMT_INT32:
        {
            int32_t *out_int32 = (int32_t *)outbuf;
            for (size_t i = 0; i < int16_count; ++i)
                out_int32[i] = buffer->samples[i];
            break;
        }

    case FMT_FLOAT32:
        {
            _Float32 *out_float = (_Float32 *)outbuf;
            for (size_t i = 0; i < int16_count; ++i)
                out_float[i] = buffer->samples[i] / 32768.0;
            break;
        }

    case FMT_FLOAT64:
        {
            _Float64 *out_double = (_Float64 *)outbuf;
            for (size_t i = 0; i < int16_count; ++i)
                out_double[i] = buffer->samples[i] / 32768.0;
            break;
        }

    default:
        abort();
    }

    return outbuf;
}

static void free_samples(uint8_t *buf, output_format_t format)
{
    if (format != FMT_INT16)
        free(buf);
}

static bool stream_callback(pg2sdr_device *dev, pg2sdr_sample_buffer *buffer, void *user_data)
{
    /* unused */ (void) dev;

    if (next_timestamp != 0 && buffer->timestamp > next_timestamp) {
        uint64_t dropped = buffer->timestamp - next_timestamp;
        if (!quiet)
            fprintf(stderr, "dropped %" PRIu64 " samples\n", dropped);
        total_dropped += dropped;
    }
    next_timestamp = buffer->timestamp + buffer->count;
    total_samples += buffer->count;

    size_t remaining;
    uint8_t *converted = convert_samples(buffer, output_format, &remaining);
    uint8_t *data = converted;
    while (remaining > 0) {
        ssize_t count = write(outfile, data, remaining);
        if (count < 0) {
            perror("write");
            write_error = true;
            stop_flag = true;
            break;
        }
        remaining -= count;
        data += count;
    }

    free_samples(converted, output_format);

    if (sample_limit > 0 && (total_samples + total_dropped) >= sample_limit)
        stop_flag = true;

    if (time_limit_ts.tv_sec) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > time_limit_ts.tv_sec ||
            (now.tv_sec == time_limit_ts.tv_nsec && now.tv_nsec >= time_limit_ts.tv_nsec))
            stop_flag = true;
    }

    if (stop_flag) {
        (void) pg2sdr_stop_streaming(dev);
    }

    return true;
}

static bool parse_optarg_frequency(const char *opt, double *out)
{
    char *end;
    double result = strtod(optarg, &end);

    switch (*end) {
    case 'k':
    case 'K':
        result *= 1e3;
        ++end;
        break;
    case 'm':
    case 'M':
        result *= 1e6;
        ++end;
        break;
    }

    if (*end || result <=  0) {
        /* unrecognized suffix or trailing garbage or not a positive value */
        fprintf(stderr, "%s: %s option expects a positive frequency, but got '%s'\n", argv0, opt, optarg);
        return false;
    }

    *out = result;
    return true;
}

static bool parse_optarg_double(const char *opt, double *out)
{
    char *end;
    double result = strtod(optarg, &end);
    if (*end) {
        fprintf(stderr, "%s: %s option expects a number, but got '%s'\n", argv0, opt, optarg);
        return false;
    }

    *out = result;
    return true;
}

static void usage()
{
    fprintf(stderr,
            "Usage: %s -f FREQ -r RATE [OPTION]... FILE\n"
            "Receive sample data from a PG2SDR device, write converted data to a file\n"
            "\n"
            "When FILE is -, write to stdout\n"
            "\n"
            "Options that take frequencies understand k/M suffixes for kHz and MHz\n"
            "\n"
            " -s, --serial=PREFIX   select PG2SDR device by serial prefix\n"
            " -p, --port=PORT       select PG2SDR device by USB port path\n"
            "\n"
            " -f, --frequency=FREQ  set tuned center frequency\n"
            " -r, --rate=RATE       set user sampling rate\n"
            " -g, --gain=GAIN       set total gain in dB\n"
            " -b, --bandwidth=BW    set tuner bandpass filter bandwidth\n"
            " -d, --decimation=N    set decimation to N steps, or 'auto', or 'max'\n"
            " -u, --undersampling=N set undersampling mode to N\n"
            " -a, --adc-limit=FREQ  set maximum ADC frequency\n"
            " -m, --mode=MODE       set sample conversion mode ('baseband' or 'lowif')\n"
            " -i, --sideband=BAND   set tuner sideband ('upper' or 'lower')\n"
            "\n"
            " -o, --format=FMT      set output format ('uint8', 'int16', 'int32', 'float32', 'float64')\n"
            "\n"
            " -t, --time-limit=T    stop capture after T seconds\n"
            " -n, --sample-limit=N  stop capture after N user samples\n"
            "\n"
            " -q, --quiet           suppress informational messages, show errors only\n"
            " -v, --verbose         enable extra debugging messages\n"
            " -h, --help            show this help and exit\n",
            argv0);
}

int main(int argc, char **argv)
{
    argv0 = argv[0];

    struct option opts[] = {
        { "help",          no_argument,       0, 'h' },
        { "quiet",         no_argument,       0, 'q' },
        { "verbose",       no_argument,       0, 'v' },
        { "serial",        required_argument, 0, 's' },
        { "port",          required_argument, 0, 'p' },
        { "frequency",     required_argument, 0, 'f' },
        { "rate",          required_argument, 0, 'r' },
        { "gain",          required_argument, 0, 'g' },
        { "bandwidth",     required_argument, 0, 'b' },
        { "decimation",    required_argument, 0, 'd' },
        { "undersampling", required_argument, 0, 'u' },
        { "adc-limit",     required_argument, 0, 'a' },
        { "mode",          required_argument, 0, 'm' },
        { "sideband",      required_argument, 0, 'i' },
        { "format",        required_argument, 0, 'o' },
        { "time-limit",    required_argument, 0, 't' },
        { "sample-limit",  required_argument, 0, 'n' },
        { 0, 0, 0, 0 }
    };

    const char *serial_prefix = NULL;
    const char *port = NULL;
    pg2sdr_conversion_mode_t conversion_mode = PG2SDR_MODE_BASEBAND;
    double frequency = -1;
    double rate = -1;
    double gain = 200;
    double bandwidth = -1;
    int decimation = PG2SDR_DECIMATION_AUTO;
    unsigned undersampling = 1;
    double adc_limit = -1;
    double time_limit = -1;
    char *end;
    pg2sdr_sideband_mode_t sideband = PG2SDR_SIDEBAND_LOWER;

    int opt;
    while ((opt = getopt_long(argc, argv, "hqvs:p:f:r:b:g:d:u:a:t:n:m:o:i:", opts, NULL)) != -1) {
        switch (opt) {
        case 'h':
            usage();
            return EXIT_SUCCESS;

        case 'q':
            quiet = true;
            verbose = false;
            break;

        case 'v':
            verbose = true;
            quiet = false;
            break;

        case 's':
            serial_prefix = optarg;
            break;

        case 'p':
            port = optarg;
            break;

        case 'f':
            if (!parse_optarg_frequency("-f", &frequency))
                return EXIT_FAILURE;
            break;

        case 'r':
            if (!parse_optarg_frequency("-r", &rate))
                return EXIT_FAILURE;
            break;

        case 'b':
            if (!parse_optarg_frequency("-b", &bandwidth))
                return EXIT_FAILURE;
            break;

        case 'g':
            if (!parse_optarg_double("-g", &gain))
                return EXIT_FAILURE;
            break;

        case 'a':
            if (!parse_optarg_frequency("-a", &adc_limit))
                return EXIT_FAILURE;
            break;

        case 'd':
            if (!strcasecmp(optarg, "auto"))
                decimation = PG2SDR_DECIMATION_AUTO;
            else if (!strcasecmp(optarg, "max"))
                decimation = PG2SDR_DECIMATION_AUTO_MAX;
            else {
                long l = strtol(optarg, &end, 10);
                if (*end || l < 0) {
                    fprintf(stderr, "%s: -d option expects a non-negative integer or 'auto' or 'max', but got '%s'\n", argv0, optarg);
                    return EXIT_FAILURE;
                }
                decimation = (int)l;
            }
            break;

        case 'u': {
            unsigned long ul = strtoul(optarg, &end, 10);
            if (*end || ul < 0 || ul > UINT_MAX) {
                fprintf(stderr, "%s: -u option expects a non-negative integer, but got '%s'\n", argv0, optarg);
                return EXIT_FAILURE;
            }
            undersampling = (unsigned) ul;
            break;
        }

        case 't':
            if (!parse_optarg_double("-t", &time_limit))
                return EXIT_FAILURE;
            break;

        case 'n': {
            unsigned long ul = strtoul(optarg, &end, 10);
            if (*end || ul <= 0 || ul > UINT_MAX) {
                fprintf(stderr, "%s: -n option expects a positive integer, but got '%s'\n", argv0, optarg);
                return EXIT_FAILURE;
            }
            sample_limit = (uint64_t) ul;
            break;
        }

        case 'm':
            if (!strcasecmp(optarg, "baseband")) {
                conversion_mode = PG2SDR_MODE_BASEBAND;
            } else if (!strcasecmp(optarg, "lowif")) {
                conversion_mode = PG2SDR_MODE_LOWIF_REAL;
            } else {
                fprintf(stderr, "%s: -m option expects 'baseband' or 'lowif', but got '%s'\n", argv0, optarg);
                return EXIT_FAILURE;
            }
            break;

        case 'o':
            if (!strcasecmp(optarg, "uint8")) {
                output_format = FMT_UINT8;
            } else if (!strcasecmp(optarg, "int16")) {
                output_format = FMT_INT16;
            } else if (!strcasecmp(optarg, "int32")) {
                output_format = FMT_INT32;
            } else if (!strcasecmp(optarg, "float32")) {
                output_format = FMT_FLOAT32;
            } else if (!strcasecmp(optarg, "float64")) {
                output_format = FMT_FLOAT64;
            } else {
                fprintf(stderr, "%s: unrecognized output format '%s'\n", argv0, optarg);
                fprintf(stderr, "possible output formats: uint8, int16, int32, float32, float64\n");
                return EXIT_FAILURE;
            }
            break;

        case 'i':
            if (!strcasecmp(optarg, "upper")) {
                sideband = PG2SDR_SIDEBAND_UPPER;
            } else if (!strcasecmp(optarg, "lower")) {
                sideband = PG2SDR_SIDEBAND_LOWER;
            } else {
                fprintf(stderr, "%s: -i option expects 'upper' or 'lower', but got '%s'\n", argv0, optarg);
                return EXIT_FAILURE;
            }
            break;

        case '?':
            return EXIT_FAILURE;
        }
    }

    if (frequency < 0) {
        fprintf(stderr, "%s: the -f option is required\n\n", argv0);
        return EXIT_FAILURE;
    }

    if (rate < 0) {
        fprintf(stderr, "%s: the -r option is required\n\n", argv0);
        return EXIT_FAILURE;
    }

    if (optind >= argc) {
        fprintf(stderr, "%s: an output filename (or - for stdout) is required\n\n", argv0);
        return EXIT_FAILURE;
    }

    pg2sdr_context *ctx = NULL;
    pg2sdr_device *dev = NULL;
    int error;
    int status = EXIT_FAILURE;
    const char *outfilename;

    outfilename = argv[optind];
    if (!strcmp(outfilename, "-")) {
        outfile = STDOUT_FILENO;
        outfilename = "stdout";
    } else {
        if ((outfile = creat(outfilename, 0666)) < 0) {
            perror(outfilename);
            goto cleanup;
        }
    }

    if ((error = pg2sdr_init(&ctx)) < 0) {
        pg2sdr_perror("pg2sdr_init", error);
        goto cleanup;
    }

    if ((error = pg2sdr_set_log_callback(ctx, log_callback)) < 0) {
        pg2sdr_perror("pg2sdr_set_log_callback", error);
        goto cleanup;
    }

    if ((error = pg2sdr_open_single_device(ctx,
                                           serial_prefix,
                                           port,
                                           &dev)) < 0)
        return pg2sdr_perror("pg2sdr_open_single_device", error);

    if (!quiet) {
        fprintf(stderr, "Opened device on port %s with serial %s\n",
                pg2sdr_get_ports(dev),
                pg2sdr_get_serial(dev));
    }

    if ((error = pg2sdr_set_conversion_mode(dev, conversion_mode)) < 0) {
        pg2sdr_perror("pg2sdr_set_conversion_mode", error);
        goto cleanup;
    }

    if ((error = pg2sdr_set_sideband(dev, sideband)) < 0) {
        pg2sdr_perror("pg2sdr_set_sideband", error);
        goto cleanup;
    }

    if ((error = pg2sdr_set_sample_rate(dev, rate)) < 0) {
        pg2sdr_perror("pg2sdr_set_sample_rate", error);
        goto cleanup;
    }
    if ((error = pg2sdr_set_frequency(dev, frequency)) < 0) {
        pg2sdr_perror("pg2sdr_set_frequency", error);
        goto cleanup;
    }
    if (bandwidth > 0) {
        if (conversion_mode == PG2SDR_MODE_BASEBAND) {
            error = pg2sdr_set_bandpass(dev, -bandwidth/2.0, bandwidth/2.0);
        } else {
            if (sideband)
                error = pg2sdr_set_bandpass(dev, 0, bandwidth);
            else
                error = pg2sdr_set_bandpass(dev, -bandwidth, 0);
        }
        if (error < 0) {
            pg2sdr_perror("pg2sdr_set_bandpass", error);
            goto cleanup;
        }
    }
    if ((error = pg2sdr_set_total_gain_db(dev, gain)) < 0) {
        pg2sdr_perror("pg2sdr_set_total_gain_db", error);
        goto cleanup;
    }
    if (adc_limit > 0 && ((error = pg2sdr_set_adc_limit(dev, adc_limit)) < 0)) {
        pg2sdr_perror("pg2sdr_set_adc_limit", error);
        goto cleanup;
    }
    if ((error = pg2sdr_set_decimation_mode(dev, decimation)) < 0) {
        pg2sdr_perror("pg2sdr_set_decimation_mode", error);
        goto cleanup;
    }
    if ((error = pg2sdr_set_undersampling_mode(dev, undersampling)) < 0) {
        pg2sdr_perror("pg2sdr_set_undersampling_mode", error);
        goto cleanup;
    }

    if ((error = pg2sdr_apply_changes(dev)) < 0) {
        pg2sdr_perror("pg2sdr_apply_changes", error);
        goto cleanup;
    }

    if (!quiet) {
        /* for simplicity we ignore errors here (the only possible errors are API usage errors) */
        double actual_freq = 0;
        double actual_rate = 0;
        double actual_bp_lo = 0, actual_bp_hi = 0;
        double actual_gain = 0;

        (void) pg2sdr_get_frequency(dev, NULL, &actual_freq);
        (void) pg2sdr_get_sample_rate(dev, NULL, &actual_rate);
        (void) pg2sdr_get_bandpass(dev, NULL, NULL, &actual_bp_lo, &actual_bp_hi);
        (void) pg2sdr_get_total_gain_db(dev, &actual_gain);

        fprintf(stderr, "Configured with:\n");
        fprintf(stderr, "  center frequency: %.6f MHz\n", actual_freq/1e6);
        fprintf(stderr, "  sampling rate:    %.6f MSPS %s\n",
                actual_rate/1e6,
                (conversion_mode == PG2SDR_MODE_BASEBAND ? "complex" : "real"));
        fprintf(stderr, "  bandpass:         %.3f MHz .. %.3f MHz (%.0f kHz bandwidth)\n",
                (actual_freq+actual_bp_lo)/1e6,
                (actual_freq+actual_bp_hi)/1e6,
                (actual_bp_hi - actual_bp_lo)/1e3);
        fprintf(stderr, "  gain:             %.1f dB\n", actual_gain);

        if (time_limit > 0)
            fprintf(stderr, "Capturing for about %.1f seconds\n", time_limit);
        if (sample_limit > 0)
            fprintf(stderr, "Capturing about %" PRIu64 " samples total\n", sample_limit);

        fprintf(stderr, "Writing sample data to %s, ^C to stop early\n", outfilename);
    }

    signal(SIGINT, stop_streaming);

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    if (time_limit > 0) {
        /* calculate deadline when we should stop capturing, time_limit seconds after ts_start */
        double sec, nsec;
        nsec = modf(time_limit, &sec) * 1e9;

        time_limit_ts.tv_sec = ts_start.tv_sec + sec;
        time_limit_ts.tv_nsec = ts_start.tv_nsec + nsec;

        time_limit_ts.tv_sec += time_limit_ts.tv_nsec / 1000000000U;
        time_limit_ts.tv_nsec %= 1000000000U;
    }

    if ((error = pg2sdr_stream_data(dev, &stream_callback, NULL)) < 0) {
        pg2sdr_perror("pg2sdr_stream_data", error);
        goto cleanup;
    }
    clock_gettime(CLOCK_MONOTONIC, &ts_end);

    if (!quiet) {
        fprintf(stderr, "Finished capturing data.\n");

        uint64_t elapsed_ns = ((uint64_t)ts_end.tv_sec - (uint64_t)ts_start.tv_sec)*1000000000ULL + (uint64_t)ts_end.tv_nsec - (uint64_t)ts_start.tv_nsec;
        double elapsed_sec = elapsed_ns / 1e9;
        double effective_rate = (total_samples + total_dropped) / elapsed_sec;

        fprintf(stderr, "Received %" PRIu64 " samples (+ %" PRIu64 " dropped) in %.3fs\n",
                total_samples, total_dropped, elapsed_sec);
        fprintf(stderr, "Effective sampling rate: %.3f MSPS\n", effective_rate/1e6);
    }

    if (!write_error)
        status = EXIT_SUCCESS;

 cleanup:
    if (dev && (error = pg2sdr_close_device(dev)) < 0)
        pg2sdr_perror("pg2sdr_close_device", error);
    if (ctx && (error = pg2sdr_exit(ctx)) < 0)
        pg2sdr_perror("pg2sdr_exit", error);
    if (outfile >= 0)
        close(outfile);

    return status;
}
