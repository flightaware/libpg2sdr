#ifndef PG2_LOG_H
#define PG2_LOG_H

#include <stdbool.h>

extern const char *argv0;
extern bool verbose_logging;

void log_error(const char *fmt, ...) __attribute__((format (printf, 1, 2)));
void log_perror(const char *fmt, ...) __attribute__((format (printf, 1, 2)));
void log_perror_libusb(int usb_error, const char *fmt, ...)  __attribute__((format (printf, 2, 3)));
void log_perror_pg2sdr(int pg2_error, const char *fmt, ...)  __attribute__((format (printf, 2, 3)));
void log_verbose(const char *fmt, ...) __attribute__((format (printf, 1, 2)));

#endif
