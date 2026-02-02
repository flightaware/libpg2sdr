#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <libusb-1.0/libusb.h>

const char *argv0 = NULL;
bool verbose_logging = true;

//
// Logging
//

void log_error(const char *fmt, ...)
{
    fprintf(stderr, "%s: ", argv0);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
}

void log_perror(const char *fmt, ...)
{
    int e = errno;

    fprintf(stderr, "%s: ", argv0);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, ": %s\n", strerror(e));
}

void log_perror_libusb(int usb_error, const char *fmt, ...)
{
    fprintf(stderr, "%s: ", argv0);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, ": %s\n", libusb_strerror(usb_error));
}

void log_verbose(const char *fmt, ...)
{
    if (!verbose_logging)
        return;

    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
}


