#include "lpcsdr.h"
#include <stdlib.h>
#include <endian.h>

int close_handle(lpcsdr_device_handle **handle);
int initialize_handle(int argc, char **argv, lpcsdr_device_handle **handle);
int test_calculate_adc_clock_divisors();
int test_transfer_start_and_capture(lpcsdr_device_handle *handle);

static void debug_logger(lpcsdr_context *ctx, lpcsdr_log_level level, const char *message) { fprintf(stderr, "lpcsdr: %s\n", message); }

int test_transfer_start_and_capture(lpcsdr_device_handle *handle) {
    int error = 1;
    if ((error = lpcsdr_start_transfer(handle, (uint32_t) 9.6e6)) < 0)
        return error;
    if ((error = lpcsdr_capture_adc_output(handle, (uint32_t) 9.6e6)) < 0)
        return error;

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

    test_transfer_start_and_capture(handle);
    close_handle(&handle);
}