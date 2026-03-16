#ifndef PG2SDR_ERRORS_H
#define PG2SDR_ERRORS_H

#include <libusb-1.0/libusb.h>

/* errors.c */
int pg2sdr__translate_libusb_error(int error);
int pg2sdr__translate_libusb_transfer_status(enum libusb_transfer_status status);
int pg2sdr__translate_errno(int error);

#endif
