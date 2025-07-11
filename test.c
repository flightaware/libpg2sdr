#include "test.h"

int test_read(lpcsdr_device_handle *handle) {

    ep0_in_board_status_t *status;

    lpcsdr_start_transfer(handle, 9600000);

    lpcsdr_get_status(handle, &status);

    uint32_t num_samples = 960000 * 2;
    uint8_t *out = calloc(sizeof(uint8_t), num_samples);
    uint32_t out_length;

    int error = lpcsdr_read(handle, status, num_samples, &out, &out_length, "test_read");
    if (error < 0) {
        printf("%d \n", error);
    }
}

int get_unpacked_adc_data_for_baseband_decimation(const char *file_path, int16_t **test_data, uint32_t num_lines) {

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

int test_complex_baseband_decimation(lpcsdr_device_handle *handle) {
    int error = LPCSDR_SUCCESS;

    baseband_decimation_test_case test_cases[] = {
        {
            .name = "Complex Baseband Decimation: Default Test Case",
            .decimate = handle->decimation_filter,
            .num_lines = 1926663,
            .usb_samples_per_block = 13616/2,
            .required_samples = 960000,
            .input_file_path = "./python-capture.tsv",
            .output_file_path = "test_data.tsv"
        }
    };

    for (uint32_t current_test_case = 0; current_test_case < sizeof(test_cases)/sizeof(test_cases[0]); current_test_case++) {

        lpcsdr_decimate *decimate = test_cases[current_test_case].decimate;
        uint32_t num_lines = test_cases[current_test_case].num_lines;
        uint32_t usb_samples_per_block = test_cases[current_test_case].usb_samples_per_block;
        uint32_t required_samples = test_cases[current_test_case].required_samples;
        const char *input_file_path = test_cases[current_test_case].input_file_path;
        const char *output_file_path = test_cases[current_test_case].output_file_path;
        int16_t *test_data;
        cs16_t *out;

        if ((error = get_unpacked_adc_data_for_baseband_decimation(input_file_path, &test_data, num_lines)) < 0) {
            printf("Baseband decimation: Could not get data for %s", error);
            return error;
        }   

        if ((error = lpcsdr_decimate_complex_baseband(decimate, usb_samples_per_block, test_data, num_lines, &out, required_samples, output_file_path)) < 0) {
            return error;
        }
    }
    return error;
}

int initialize_handle(int argc, char **argv, lpcsdr_device_handle **handle) {
    lpcsdr_context *ctx;
    int error;
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
   
    return 1;
}

int close_handle(lpcsdr_device_handle **handle) {
    int error;
    if ((error = lpcsdr_close_device(*handle)) < 0){
        fprintf(stderr, "lpcsdr_close_single_device: %s\n", lpcsdr_strerror(NULL, error));
        return -1;
    }
    return 1;
}

int test_calculate_adc_clock_divisors() {
    uint32_t target_frequency = 5.2e6; //mhz

    pll_divisors *int_divisors;
    pll_divisors *frac_divisors;
    int error;
    if ((error = calculate_adc_clock_divisors(target_frequency, &int_divisors, &frac_divisors)) < 0)
        return -1;


    printf("Target Frequency %u\n", target_frequency );
    printf("Int Frequency N %u, M %u, P %u, I %u. \n", int_divisors->n, int_divisors->m, int_divisors->p, int_divisors->i);
    printf("Freq Frequency N %u, M %u, P %u, I %u. \n", frac_divisors->n, frac_divisors->m, frac_divisors->p, frac_divisors->i);

    if (int_divisors->error != 0 && int_divisors->i != 0 && int_divisors->m != 13 && int_divisors->n != 0 && int_divisors->p != 30)
        return -1;

    if (frac_divisors->error != 0 && frac_divisors->i != 0 && frac_divisors->m != 13 && frac_divisors->n != 0 && frac_divisors->p != 30)
        return -1;

    return LPCSDR_SUCCESS;
}

int main(int argc, char **argv) {
    if (test_calculate_adc_clock_divisors() < 0)
        printf("Calculating target frequency dividers failed\n");
    lpcsdr_device_handle *handle;
    if (initialize_handle(argc, argv, &handle) < 0)
        printf("Initialize handle failed\n");

    test_complex_baseband_decimation(handle);

    // test_transfer_start_and_capture(handle);
    close_handle(&handle);
}