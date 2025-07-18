#include "internal.h"
#include <stdlib.h>
#include <endian.h>
#include <stdio.h>
#include <assert.h>


typedef struct baseband_decimation_test_case_with_file {
    char *name;
    lpcsdr_decimate *decimate;
    uint32_t num_lines;
    uint32_t usb_samples_per_block; 
    uint32_t required_samples;
    int16_t *test_data;
    char *input_file_path;
    char *output_file_path;
    char *expected_file_path;

} baseband_decimation_test_case_with_file;

int test_read(lpcsdr_device_handle *handle);
int test_adc();
int init_adc_divisors();
int test_calculate_adc_clock_divisors();
int read_unpacked_file(const char *file_path, int16_t **test_data, uint32_t num_lines);
int close_handle(lpcsdr_device_handle **handle);
int initialize_handle(int argc, char **argv, lpcsdr_device_handle **handle);
int test_transfer_start_and_capture();
int test_complex_baseband_decimation_with_files();

static void debug_logger(lpcsdr_context *ctx, lpcsdr_log_level level, const char *message) { fprintf(stderr, "lpcsdr: %s\n", message); }
