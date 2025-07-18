#include "test.h"

int read_unpacked_file(const char *file_path, int16_t **test_data, uint32_t num_lines) {

    if (file_path == NULL) {
        return LPCSDR_ERROR_BAD_ARGUMENT;
    }

    int16_t *buffer = calloc(num_lines, sizeof(int16_t));
    uint32_t cursor = 0;
    char line[100];

    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        return LPCSDR_ERROR_BAD_ARGUMENT;
    }

    while (fgets(line, sizeof(line), file) && cursor < num_lines) {
        
        uint16_t index = 0;
        while (index < 30 && line[index] != '\t')
            index++;
        index++;

        if (line[index] == '-') {
            uint16_t end = index;
            char slicedFoo[7];
            uint16_t f= 0;
            while (line[index] != '\0') {
                slicedFoo[f++] = line[index++];
            }
            buffer[cursor] = atoi(slicedFoo);
        } else {

            buffer[cursor] = (line[index] - '0');
        }
        cursor++;
    }

    *test_data = buffer;

    fclose(file);
    return LPCSDR_SUCCESS;
}

int test_complex_baseband_decimation_with_files() {
    int error = LPCSDR_SUCCESS;

    lpcsdr_decimate *default_filter;
    assert(lpcsdr_dsp_decimate_create(lpcsdr_standard_filter_ntaps, lpcsdr_standard_filter_taps, &default_filter) == LPCSDR_SUCCESS);
    baseband_decimation_test_case_with_file test_cases[] = {
        {
            .name = "Default Test Case",
            .decimate = default_filter,
            .num_lines = 1926663,
            .usb_samples_per_block = 13616/2,
            .required_samples = 960000,
            .input_file_path = "../test_files/inputs/default-test-case-input.tsv",
            .output_file_path = "../test_files/outputs/default-test-case-output.tsv",
        }
    };

    for (uint32_t current_test_case = 0; current_test_case < sizeof(test_cases)/sizeof(test_cases[0]); current_test_case++) {
        printf("Complex Baseband Decimation Tests \n");

        char *name = test_cases[current_test_case].name;
        lpcsdr_decimate *decimate = test_cases[current_test_case].decimate;
        uint32_t num_lines = test_cases[current_test_case].num_lines;
        uint32_t usb_samples_per_block = test_cases[current_test_case].usb_samples_per_block;
        uint32_t required_samples = test_cases[current_test_case].required_samples;
        const char *input_file_path = test_cases[current_test_case].input_file_path;
        const char *output_file_path = test_cases[current_test_case].output_file_path;
        cs16_t *test_data;
        cs16_t *out;

        if (input_file_path != NULL && (error = read_unpacked_file(input_file_path, &test_data, num_lines)) < 0) {
            printf("Could not get data for test case %s. Got error %s\n", name, error);
            return error;
        }

        if ((error = lpcsdr_decimate_complex_baseband(decimate, usb_samples_per_block, test_data, num_lines, &out, required_samples, output_file_path)) < 0) {
            printf("Could not complete decimation for %s. Got error  %s\n", name, error);
            return error;
        }
    }
    return error;
}

int initialize_handle(int argc, char **argv, lpcsdr_device_handle **handle) {
    lpcsdr_context *ctx;
    int error = LPCSDR_SUCCESS;
    if ((error = lpcsdr_init(&ctx) < 0)) {
        printf("Error initing lpc_context\n");
        return -1;
    }

    if ((error = lpcsdr_set_log_callback(ctx, &debug_logger)) < 0) {
        fprintf(stderr, "lpcsdr_set_log_callback: %s\n", lpcsdr_strerror(NULL, error));
        return -1;
    }

    if (argc > 1) {
        if ((error = lpcsdr_set_firmware_path(ctx, argv[1])) < 0) {
            fprintf(stderr, "lpcsdr_set_firmware_path: %s\n", lpcsdr_strerror(ctx, error));
            return 1;
        }
    }

    lpcsdr_device_handle *h;
    if ((error = lpcsdr_open_single_device(ctx, &h)) < 0) {
        fprintf(stderr, "lpcsdr_open_single_device: %s\n", lpcsdr_strerror(ctx, error));
        return -1;
    }

    *handle = h;
   
    return error;
}

int close_handle(lpcsdr_device_handle **handle) {
    int error = LPCSDR_SUCCESS;
    if ((error = lpcsdr_close_device(*handle)) < 0){
        fprintf(stderr, "lpcsdr_close_single_device: %s\n", lpcsdr_strerror(NULL, error));
        return error;
    }
    return error;
}

int test_calculate_adc_clock_divisors() {
    uint32_t target_frequency = 5.2e6; //mhz

    pll_divisors *int_divisors;
    pll_divisors *frac_divisors;
    int error;

    if ((error = calculate_adc_clock_divisors(target_frequency, &int_divisors, false, false, NULL)) < 0)
        return -1;
    if ((error = calculate_adc_clock_divisors(target_frequency, &frac_divisors, false, true, NULL)) < 0)
        return -1;

    printf("Target Frequency %u\n", target_frequency );
    printf("Int Frequency N %u, M %f, P %u, I %u. \n", int_divisors->n, int_divisors->m, int_divisors->p, int_divisors->i);
    printf("Frac Frequency N %u, M %f, P %u, I %u. \n", frac_divisors->n, frac_divisors->m, frac_divisors->p, frac_divisors->i);

    if (int_divisors->error != 0 && int_divisors->i != 0 && int_divisors->m != 13 && int_divisors->n != 0 && int_divisors->p != 30)
        return -1;

    if (frac_divisors->error != 0 && frac_divisors->i != 0 && frac_divisors->m != 13 && frac_divisors->n != 0 && frac_divisors->p != 30)
        return -1;

    return LPCSDR_SUCCESS;
}

int test_adc() {
    assert(init_adc_divisors() == LPCSDR_SUCCESS);
    printf("yayyy\n");
    assert(test_calculate_adc_clock_divisors() == LPCSDR_SUCCESS);

    return LPCSDR_SUCCESS;
}

int main(int argc, char **argv) {

    lpcsdr_device_handle *handle;
    // assert(initialize_handle(argc, argv, &handle) == LPCSDR_SUCCESS);

    // test_read(handle);
    // lpcsdr_capture(handle, 960000, 9600000, 8);
    assert(test_complex_baseband_decimation_with_files() == LPCSDR_SUCCESS);
    assert(test_adc() == LPCSDR_SUCCESS);

    // close_handle(&handle);
}