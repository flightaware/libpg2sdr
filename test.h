#include "internal.h"
#include <stdlib.h>
#include <endian.h>
#include <stdio.h>


typedef struct baseband_decimation_test_case {
    char *name;
    lpcsdr_decimate *decimate;
    uint32_t num_lines;
    uint32_t usb_samples_per_block; 
    uint32_t required_samples; 
    char *input_file_path;
    char *output_file_path;

} baseband_decimation_test_case;

int test_read(lpcsdr_device_handle *handle);
int get_unpacked_adc_data_for_baseband_decimation(const char *file_path, int16_t **test_data, uint32_t num_lines);
int close_handle(lpcsdr_device_handle **handle);
int initialize_handle(int argc, char **argv, lpcsdr_device_handle **handle);
int test_calculate_adc_clock_divisors();
int test_transfer_start_and_capture(lpcsdr_device_handle *handle);

static void debug_logger(lpcsdr_context *ctx, lpcsdr_log_level level, const char *message) { fprintf(stderr, "lpcsdr: %s\n", message); }
