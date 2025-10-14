#include "internal.h"

int lpcsdr__translate_libusb_error(int error)
{
    /* handle some cases that map directly to our own error codes */
    switch (error) {
    case LIBUSB_SUCCESS:
        return LPCSDR_SUCCESS;
    case LIBUSB_ERROR_NO_DEVICE:
        return LPCSDR_ERROR_DISCONNECTED;
    case LIBUSB_ERROR_BUSY:
        return LPCSDR_ERROR_BUSY;
    case LIBUSB_ERROR_TIMEOUT:
        return LPCSDR_ERROR_TIMEOUT;
    case LIBUSB_ERROR_NO_MEM:
        return LPCSDR_ERROR_NO_MEMORY;
    }

    /* everything else, throw it into the generic "libusb error" range */
    int converted = LPCSDR_ERROR_LIBUSB_MIN - error; /* nb: error is negative */
    if (converted >= LPCSDR_ERROR_LIBUSB_MIN && converted <= LPCSDR_ERROR_LIBUSB_MAX)
        return converted;
    return LPCSDR_ERROR_LIBUSB_MIN;
}

int lpcsdr__translate_libusb_transfer_status(enum libusb_transfer_status status)
{
    switch (status) {
    case LIBUSB_TRANSFER_COMPLETED:
        return LPCSDR_SUCCESS;
    case LIBUSB_TRANSFER_TIMED_OUT:
        return LPCSDR_ERROR_TIMEOUT;
    case LIBUSB_TRANSFER_STALL:
        return LPCSDR_ERROR_TRANSFER_STALL;
    case LIBUSB_TRANSFER_OVERFLOW:
        return LPCSDR_ERROR_TRANSFER_OVERFLOW;
    case LIBUSB_TRANSFER_NO_DEVICE:
        return LPCSDR_ERROR_DISCONNECTED;
    default:
        return LPCSDR_ERROR_TRANSFER_OTHER;
    }
}

int lpcsdr__translate_errno(int error)
{
    switch (error) {
    case 0:
        return LPCSDR_SUCCESS;
    case ENOMEM:
        return LPCSDR_ERROR_NO_MEMORY;
    }

    int converted = LPCSDR_ERROR_SYSTEM_MIN + error; /* nb: error (an errno value) is positive */
    if (converted >= LPCSDR_ERROR_SYSTEM_MIN && converted <= LPCSDR_ERROR_SYSTEM_MAX)
        return converted;

    return LPCSDR_ERROR_SYSTEM_MIN;
}

/* ow, strerror_r is just a giant ball of incompatible pain */

#if (_POSIX_C_SOURCE >= 200112L) && ! _GNU_SOURCE
/* we have the XSI strerror_r */
static char *wrap_strerror(int errnum, char *buf, size_t buflen)
{
    if (strerror_r(errnum, buf, buflen) != 0) {
        snprintf(buf, buflen, "Unknown error %d", errnum);
    }
    return buf;
}
#else
/* we have the GNU strerror_r */
static char *wrap_strerror(int errnum, char *buf, size_t buflen)
{
    return strerror_r(errnum, buf, buflen);
}
#endif

const char *lpcsdr_strerror(int error)
{
    static char buf[1024];
    return lpcsdr_strerror_r(error, buf, sizeof(buf));
}

const char *lpcsdr_strerror_r(int error, char *buf, size_t buflen)
{
    if (error > LPCSDR_ERROR_SYSTEM_MIN && error <= LPCSDR_ERROR_SYSTEM_MAX) {
        int sys_error = error - LPCSDR_ERROR_SYSTEM_MIN;
        char extrabuf[1024];
        snprintf(buf, buflen, "system error: %s", wrap_strerror(sys_error, extrabuf, sizeof(extrabuf)));
        return buf;
    }

    if (error > LPCSDR_ERROR_LIBUSB_MIN && error <= LPCSDR_ERROR_LIBUSB_MAX) {
        int usb_error = -(error - LPCSDR_ERROR_LIBUSB_MIN);
        snprintf(buf, buflen, "libusb error: %s", libusb_strerror(usb_error));
        return buf;
    }

    switch ((enum lpcsdr_error)error) {
    case LPCSDR_SUCCESS:
        return "no error";

    case LPCSDR_ERROR_NOT_FOUND:
        return "No matching device found";
    case LPCSDR_ERROR_DISCONNECTED:
        return "Device unexpectedly disconnected";
    case LPCSDR_ERROR_BAD_ARGUMENT:
        return "Bad argument to API call";
    case LPCSDR_ERROR_NO_MEMORY:
        return "Memory allocation failed";
    case LPCSDR_ERROR_NOT_IMPLEMENTED:
        return "Not implemented";
    case LPCSDR_ERROR_FIRMWARE_MISMATCH:
        return "Host/firmware version mismatch";
    case LPCSDR_ERROR_MULTIPLE_DEVICES:
        return "More than one device found";
    case LPCSDR_ERROR_BUSY:
        return "Device already in use";
    case LPCSDR_ERROR_BAD_STATE:
        return "Operation not possible in this state";
    case LPCSDR_ERROR_TIMEOUT:
        return "Operation timed out";
    case LPCSDR_ERROR_CORRUPTION:
        return "Heap corruption or double-free detected";

    case LPCSDR_ERROR_FWIMAGE_MISSING:
        return "Firmware image not found";
    case LPCSDR_ERROR_FWIMAGE_UPLOAD:
        return "Firmware image DFU upload failed";

    case LPCSDR_ERROR_TRANSFER_OTHER:
        return "Uncategorized bulk endpoint transfer error";
    case LPCSDR_ERROR_TRANSFER_STALL:
        return "Bulk endpoint stall condition";
    case LPCSDR_ERROR_TRANSFER_OVERFLOW:
        return "Bulk endpoint transfer returned more data that expected";
    case LPCSDR_ERROR_TRANSFER_FORMAT:
        return "Malformed bulk endpoint data received";

    case LPCSDR_ERROR_TUNER_DETECT:
        return "Could not detect tuner";
    case LPCSDR_ERROR_TUNER_PLL_LOCK:
        return "Tuner PLL did not lock";
    case LPCSDR_ERROR_TUNER_PLL_RANGE:
        return "Required tuner PLL frequency out of range";
    case LPCSDR_ERROR_TUNER_I2C:
        return "Tuner I2C communication error";
    case LPCSDR_ERROR_ADC_RATE_RANGE:
        return "Required ADC sampling rate out of range";

    case LPCSDR_ERROR_SYSTEM_MIN:
        return "Unknown system error";
    case LPCSDR_ERROR_LIBUSB_MIN:
        return "Unknown libusb error";

        /* these are just here so the compiler doesn't complain; they are
         * actually already handled earlier
         */
    case LPCSDR_ERROR_LIBUSB_MAX:
    case LPCSDR_ERROR_SYSTEM_MAX:
        break;
    }

    /* this is deliberately not a default case, so we get compiler
     * complaints if we missed an enum value in the switch above
     */
    return "Unknown error";
}
