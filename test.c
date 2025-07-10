#include "internal.h"
#include <stdlib.h>
#include <endian.h>
#include <stdio.h>

int close_handle(lpcsdr_device_handle **handle);
int initialize_handle(int argc, char **argv, lpcsdr_device_handle **handle);
int test_calculate_adc_clock_divisors();
int test_transfer_start_and_capture(lpcsdr_device_handle *handle);

static void debug_logger(lpcsdr_context *ctx, lpcsdr_log_level level, const char *message) { fprintf(stderr, "lpcsdr: %s\n", message); }

int test_transfer_start_and_capture(lpcsdr_device_handle *handle) {
    // lpcsdr_capture(handle, 10, (uint32_t) 9.6e6);

    FILE* file = fopen("./python-capture.tsv", "r");
    if (file == NULL) {
        printf("file is null");
        return -1;
    }
    char line[100];
    int16_t test[1926663];
    uint64_t x = 0;
    // fgets(line, 100, file);
    while (fgets(line, sizeof(line), file)) {
        
        uint16_t index = 0;
        while (index < 30 && line[index] != '\t')
            index+=1;
        index++;
        // printf("fill line %s %d \n", line, index);
        if (line[index] == '-') {
            uint16_t end = index;
            char slicedFoo[7];
            uint16_t f= 0;
            while (line[index] != '\0') {
                slicedFoo[f++] = line[index++];
            }

            // printf("negative %s \n", slicedFoo);
            test[x] = atoi(slicedFoo);
        }else {

            test[x] = (line[index] - '0');
        }
        // printf("test %d %d %c\n", x, test[x] ,line[index]);
        x++;
    }
    // printf("%d\n", test[0]);
    // return -1;
    fclose(file);
    cs16_t *out;
    uint16_t output_to_file = 1;
    lpcsdr_decimate_complex_baseband(handle->decimation_filter, test, 1926663, &out, 960000, &output_to_file);
    return -1;
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

    test_transfer_start_and_capture(handle);
    close_handle(&handle);
}