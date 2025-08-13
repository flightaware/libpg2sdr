#include <libusb-1.0/libusb.h>
#include "internal.h"

int populate_libusb_vtable(libusb_vtable **out) {
    libusb_vtable *vtable = calloc(1, sizeof(libusb_vtable));

    if (!vtable)
        return LPCSDR_ERROR_NO_MEMORY;
    
    vtable->bulk_transfer = libusb_bulk_transfer;
    vtable->control_transfer = libusb_control_transfer;
    *out = vtable;

    return LPCSDR_SUCCESS;
}

void free_libusb_vtable(libusb_vtable *vtable) {
    if (!vtable)
        return;

    free(vtable);
    return;
}