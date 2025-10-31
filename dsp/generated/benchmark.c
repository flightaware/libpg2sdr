
/* starch generated code. Do not edit. */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "starch.h"

typedef struct timespec starch_benchmark_time;

uint64_t starch_benchmark_elapsed(starch_benchmark_time *start, starch_benchmark_time *end);
void starch_benchmark_get_time(starch_benchmark_time *t);

const unsigned starch_benchmark_warmup_loops = 10;

typedef struct {
    const char *name;
    const char *impl;
    const char *flavor;
    uint64_t ns;
} starch_benchmark_result;

static starch_benchmark_result *starch_benchmark_results = NULL;
static unsigned starch_benchmark_result_size = 0;
static unsigned starch_benchmark_result_count = 0;

typedef struct benchmark_flavor_list_node {
    const char *flavor;
    struct benchmark_flavor_list_node *next;
} starch_benchmark_flavor_list;

static starch_benchmark_flavor_list *starch_benchmark_flavor_whitelist = NULL;
static starch_benchmark_flavor_list *starch_benchmark_flavor_blacklist = NULL;

static bool starch_benchmark_list_only = false;
static bool starch_benchmark_validate_only = false;
static bool starch_benchmark_validation_failed = false;
static bool starch_benchmark_top_only = false;
static unsigned starch_benchmark_iterations = 1;

typedef struct timespec starch_benchmark_time;
void starch_benchmark_get_time(starch_benchmark_time *t)
{
#ifdef CLOCK_THREAD_CPUTIME_ID
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, t);
#else
    clock_gettime(CLOCK_MONOTONIC, t);
#endif
}

uint64_t starch_benchmark_elapsed(starch_benchmark_time *start, starch_benchmark_time *end)
{
    return ((uint64_t)end->tv_sec - (uint64_t)start->tv_sec) * 1000000000U + (uint64_t)end->tv_nsec - (uint64_t)start->tv_nsec;
}

static bool starch_benchmark_flavor_in_list(const char *flavor, const starch_benchmark_flavor_list *list)
{
    for (; list; list = list->next) {
        if (!strcmp(flavor, list->flavor))
            return true;
    }
    return false;
}


/* prototypes for benchmark helpers provided by user code */
void pg2sdr__starch_fs4_mix_benchmark (void);
bool pg2sdr__starch_fs4_mix_benchmark_verify ( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void pg2sdr__starch_fs4_mix_benchmark(void);

static void starch_benchmark_one_fs4_mix( pg2sdr__starch_fs4_mix_regentry * _entry, const int16_t * arg0, uint32_t arg1, cs16_t * arg2 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2 );

    /* verify correctness of the output */
    if (! pg2sdr__starch_fs4_mix_benchmark_verify ( arg0, arg1, arg2 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "fs4_mix";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    starch_benchmark_results[starch_benchmark_result_count].flavor = _entry->flavor;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_fs4_mix( const int16_t * arg0, uint32_t arg1, cs16_t * arg2 )
{
    for (pg2sdr__starch_fs4_mix_regentry *_entry = pg2sdr__starch_fs4_mix_registry; _entry->name; ++_entry) {
        starch_benchmark_one_fs4_mix( _entry, arg0, arg1, arg2 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void pg2sdr__starch_halfband_decimate_block_benchmark (void);

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void pg2sdr__starch_halfband_decimate_block_benchmark(void);

static void starch_benchmark_one_halfband_decimate_block( pg2sdr__starch_halfband_decimate_block_regentry * _entry, const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2, arg3 );

    if (starch_benchmark_validate_only) {
        fprintf(stderr, "no validator defined\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "halfband_decimate_block";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    starch_benchmark_results[starch_benchmark_result_count].flavor = _entry->flavor;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_halfband_decimate_block( const dsp_halfband_decimate_state_t * arg0, const cs16_t * arg1, uint32_t arg2, cs16_t * arg3 )
{
    for (pg2sdr__starch_halfband_decimate_block_regentry *_entry = pg2sdr__starch_halfband_decimate_block_registry; _entry->name; ++_entry) {
        starch_benchmark_one_halfband_decimate_block( _entry, arg0, arg1, arg2, arg3 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void pg2sdr__starch_unpack_raw_adc_data_benchmark (void);
bool pg2sdr__starch_unpack_raw_adc_data_benchmark_verify ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void pg2sdr__starch_unpack_raw_adc_data_benchmark(void);

static void starch_benchmark_one_unpack_raw_adc_data( pg2sdr__starch_unpack_raw_adc_data_regentry * _entry, const uint32_t * arg0, uint32_t arg1, int16_t * arg2 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2 );

    /* verify correctness of the output */
    if (! pg2sdr__starch_unpack_raw_adc_data_benchmark_verify ( arg0, arg1, arg2 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "unpack_raw_adc_data";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    starch_benchmark_results[starch_benchmark_result_count].flavor = _entry->flavor;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_unpack_raw_adc_data( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 )
{
    for (pg2sdr__starch_unpack_raw_adc_data_regentry *_entry = pg2sdr__starch_unpack_raw_adc_data_registry; _entry->name; ++_entry) {
        starch_benchmark_one_unpack_raw_adc_data( _entry, arg0, arg1, arg2 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void pg2sdr__starch_unpack_raw_adc_data_invert_benchmark (void);
bool pg2sdr__starch_unpack_raw_adc_data_invert_benchmark_verify ( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void pg2sdr__starch_unpack_raw_adc_data_invert_benchmark(void);

static void starch_benchmark_one_unpack_raw_adc_data_invert( pg2sdr__starch_unpack_raw_adc_data_invert_regentry * _entry, const uint32_t * arg0, uint32_t arg1, int16_t * arg2 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2 );

    /* verify correctness of the output */
    if (! pg2sdr__starch_unpack_raw_adc_data_invert_benchmark_verify ( arg0, arg1, arg2 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "unpack_raw_adc_data_invert";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    starch_benchmark_results[starch_benchmark_result_count].flavor = _entry->flavor;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_unpack_raw_adc_data_invert( const uint32_t * arg0, uint32_t arg1, int16_t * arg2 )
{
    for (pg2sdr__starch_unpack_raw_adc_data_invert_regentry *_entry = pg2sdr__starch_unpack_raw_adc_data_invert_registry; _entry->name; ++_entry) {
        starch_benchmark_one_unpack_raw_adc_data_invert( _entry, arg0, arg1, arg2 );
    }
}


#define STARCH_SYMBOL(_name) pg2sdr__starch_ ## _name ## _benchmark_sym
#define STARCH_BENCHMARK(_function) pg2sdr__starch_ ## _function ## _benchmark
#define STARCH_BENCHMARK_VERIFY(_function) pg2sdr__starch_ ## _function ## _benchmark_verify
#define STARCH_BENCHMARK_RUN(_function, ...) starch_benchmark_run_ ## _function ( __VA_ARGS__ )

#include "../impl/fs4_mix.benchmark.c"
#include "../impl/halfband_decimate.benchmark.c"
#include "../impl/unpack_raw_adc_data.benchmark.c"

#undef STARCH_SYMBOL
#undef STARCH_BENCHMARK
#undef STARCH_BENCHMARK_VERIFY
#undef STARCH_BENCHMARK_RUN

static void starch_benchmark_all_fs4_mix(void)
{
    fprintf(stderr, "==== fs4_mix ===\n");
    pg2sdr__starch_fs4_mix_benchmark ();
}
static void starch_benchmark_all_halfband_decimate_block(void)
{
    fprintf(stderr, "==== halfband_decimate_block ===\n");
    pg2sdr__starch_halfband_decimate_block_benchmark ();
}
static void starch_benchmark_all_unpack_raw_adc_data(void)
{
    fprintf(stderr, "==== unpack_raw_adc_data ===\n");
    pg2sdr__starch_unpack_raw_adc_data_benchmark ();
}
static void starch_benchmark_all_unpack_raw_adc_data_invert(void)
{
    fprintf(stderr, "==== unpack_raw_adc_data_invert ===\n");
    pg2sdr__starch_unpack_raw_adc_data_invert_benchmark ();
}

static int starch_benchmark_compare_result(const void *a, const void *b)
{
    const starch_benchmark_result *left = (const starch_benchmark_result *) a;
    const starch_benchmark_result *right = (const starch_benchmark_result *) b;

    int name_cmp = strcmp(left->name, right->name);
    if (name_cmp)
        return name_cmp;

    if (left->ns < right->ns)
        return -1;
    if (left->ns > right->ns)
        return 1;
    return 0;
}

static void starch_benchmark_usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [OPTION ...] [FUNCTION ...]\n"
            "Benchmarks starch functions and optionally writes a sorted wisdom file.\n"
            "\n"
            "  -r FILE          Read initial wisdom from FILE\n"
            "  -o FILE          Write sorted wisdom to FILE\n"
            "  -F FLAVOR        Add FLAVOR to whitelist\n"
            "                     (default: no whitelist, run all runtime-supported flavors)\n"
            "  -N FLAVOR        Add FLAVOR to blacklist\n"
            "                     (default: no blacklist, run all runtime-supported flavors)\n"
            "  -l               List compiled-in implementations but don't benchmark them\n"
            "  -V               Run validation tests, but don't run benchmarks\n"
            "  -t               Include only the top candidate per function, per flavor, in wisdom output\n"
            "  -i ITERS         Run benchmark ITERS times and use the mean. If ITERS > 2, ignore\n"
            "                   the smallest and largest runs when calculating the mean.\n"
            "                     (default: 1 iteration)\n"
            "  FUNCTION         Run benchmarks for these functions only\n"
            "                     (default: benchmark all functions)\n"
            "\n", argv0);

    fprintf(stderr,
            "Mix: %s\n\n"
            "Flavors:\n", STARCH_MIX_NAME);
#ifdef STARCH_FLAVOR_ARMV7A_SIMD
    fprintf(stderr,
            "  %-15s  %s\n",
            "armv7a_simd",
            pg2sdr__starch_supports_arm_neon() ? "supported" : "not supported");
#endif
#ifdef STARCH_FLAVOR_ARMV8A_SIMD
    fprintf(stderr,
            "  %-15s  %s\n",
            "armv8a_simd",
            pg2sdr__starch_supports_aarch64_asimd() ? "supported" : "not supported");
#endif
#ifdef STARCH_FLAVOR_DEFAULT
    fprintf(stderr,
            "  %-15s  %s\n",
            "default",
            1 ? "supported" : "not supported");
#endif
#ifdef STARCH_FLAVOR_X86_64_AVX
    fprintf(stderr,
            "  %-15s  %s\n",
            "x86_64_avx",
            pg2sdr__starch_supports_x86_avx() ? "supported" : "not supported");
#endif
#ifdef STARCH_FLAVOR_X86_64_AVX2
    fprintf(stderr,
            "  %-15s  %s\n",
            "x86_64_avx2",
            pg2sdr__starch_supports_x86_avx2() ? "supported" : "not supported");
#endif

      fprintf(stderr,
              "\nFunctions: "
              "fs4_mix "
              "halfband_decimate_block "
              "unpack_raw_adc_data "
              "unpack_raw_adc_data_invert "
              "\n");
}

static void starch_benchmark_append_flavor(const char *flavor, starch_benchmark_flavor_list **list)
{
    starch_benchmark_flavor_list *newnode = malloc(sizeof(*newnode));
    if (!newnode) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        exit(1);
    }

    newnode->flavor = flavor;
    newnode->next = *list;
    *list = newnode;
}

int main(int argc, char **argv)
{
    int specific = 0;
    const char *output_path = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "r:o:F:N:i:lhtV")) != -1) {
        switch (opt) {
        case 'r':
            if (pg2sdr__starch_read_wisdom(optarg) < 0) {
                fprintf(stderr, "%s: cannot read %s: %s\n", argv[0], optarg, strerror(errno));
                return 1;
            }
            fprintf(stderr, "%s: loaded wisdom file %s\n", argv[0], optarg);
            break;

        case 'o':
            output_path = optarg;
            break;

        case 'F':
            if (starch_benchmark_flavor_in_list(optarg, starch_benchmark_flavor_blacklist)) {
                fprintf(stderr, "%s: conflicting -F and -N options for flavor %s\n", argv[0], optarg);
                return 2;
            }
            starch_benchmark_append_flavor(optarg, &starch_benchmark_flavor_whitelist);
            break;

        case 'N':
            if (starch_benchmark_flavor_in_list(optarg, starch_benchmark_flavor_whitelist)) {
                fprintf(stderr, "%s: conflicting -F and -N options for flavor %s\n", argv[0], optarg);
                return 2;
            }
            starch_benchmark_append_flavor(optarg, &starch_benchmark_flavor_blacklist);
            break;

        case 'l':
            starch_benchmark_list_only = true;
            break;

        case 't':
            starch_benchmark_top_only = true;
            break;

        case 'i':
            starch_benchmark_iterations = atoi(optarg);
            break;

        case 'V':
            starch_benchmark_validate_only = true;
            break;

        case 'h':
            starch_benchmark_usage(argv[0]);
            return 0;

        case '?':
        default:
            starch_benchmark_usage(argv[0]);
            return 2;
        }
    }

    if (starch_benchmark_list_only && output_path) {
        fprintf(stderr, "%s: -o and -l options cannot be specified together\n", argv[0]);
        return 2;
    }

    for (int i = optind; i < argc; ++i) {
        if (!strcmp(argv[i], "fs4_mix")) {
            specific = 1;
            starch_benchmark_all_fs4_mix();
            continue;
        }
        if (!strcmp(argv[i], "halfband_decimate_block")) {
            specific = 1;
            starch_benchmark_all_halfband_decimate_block();
            continue;
        }
        if (!strcmp(argv[i], "unpack_raw_adc_data")) {
            specific = 1;
            starch_benchmark_all_unpack_raw_adc_data();
            continue;
        }
        if (!strcmp(argv[i], "unpack_raw_adc_data_invert")) {
            specific = 1;
            starch_benchmark_all_unpack_raw_adc_data_invert();
            continue;
        }

        fprintf(stderr, "%s: unrecognized function name: %s\n", argv[0], argv[i]);
        return 2;
    }

    if (!specific) {
        starch_benchmark_all_fs4_mix();
        starch_benchmark_all_halfband_decimate_block();
        starch_benchmark_all_unpack_raw_adc_data();
        starch_benchmark_all_unpack_raw_adc_data_invert();
    }

    if (output_path) {
        FILE *out = fopen(output_path, "w");
        if (!out) {
            fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], output_path, strerror(errno));
            return 1;
        }

        fprintf(out, "# generated by ");
        for (int i = 0; i < argc; ++i)
            fprintf(out, "%s ", argv[i]);
        fprintf(out, "\n\n");

        qsort(starch_benchmark_results, starch_benchmark_result_count, sizeof(*starch_benchmark_results), starch_benchmark_compare_result);

        unsigned last_name_change = 0;
        const char *last_name = NULL;
        for (unsigned i = 0; i < starch_benchmark_result_count; ++i) {
            starch_benchmark_result *result = &starch_benchmark_results[i];
            if (last_name && strcmp(last_name, result->name) != 0) {
                fprintf(out, "\n");
                last_name_change = i;
            }
            last_name = result->name;
            if (starch_benchmark_top_only) {
                bool flavor_seen = false;
                for (unsigned j = last_name_change; j < i; ++j) {
                    if (!strcmp(result->flavor, starch_benchmark_results[j].flavor)) {
                        flavor_seen = true;
                        break;
                    }
                }
                if (flavor_seen)
                    continue;
            }
            fprintf(out, "%-40s %-40s  # %" PRIu64 " ns/call\n", result->name, result->impl, result->ns);
        }

        fclose(out);
        fprintf(stderr, "%s: wrote sorted wisdom to %s\n", argv[0], output_path);
    }

    return starch_benchmark_validation_failed ? 1 : 0;
}
