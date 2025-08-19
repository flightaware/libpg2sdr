#include "util.h"

int initialize_handle(lpcsdr_device_handle **handle) {
    lpcsdr_context *ctx;
    int error = LPCSDR_SUCCESS;
    if ((error = lpcsdr_init(&ctx) < 0)) {
        printf("Error initing lpc_context\n");
        return -1;
    }

    if ((error = lpcsdr_set_firmware_path(ctx, "/media/psf/soapy_shared_folder/liblpcsdr/lpcsdr_firmware/images/lpcsdr.bin")) < 0) {
        fprintf(stderr, "lpcsdr_set_firmware_path: %s\n", lpcsdr_strerror(ctx, error));
        return -1;
    }

    lpcsdr_device_handle *h;
    if ((error = lpcsdr_open_single_device(ctx, &h)) < 0) {
        fprintf(stderr, "lpcsdr_open_single_device: %s\n", lpcsdr_strerror(ctx, error));
        return -1;
    }

    *handle = h;
    return error;
}