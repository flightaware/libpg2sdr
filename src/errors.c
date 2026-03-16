#include "internal/lib.h"

int pg2sdr__translate_libusb_error(int error)
{
    /* handle some cases that map directly to our own error codes */
    switch (error) {
    case LIBUSB_SUCCESS:
        return PG2SDR_SUCCESS;
    case LIBUSB_ERROR_NO_DEVICE:
        return PG2SDR_ERROR_DISCONNECTED;
    case LIBUSB_ERROR_BUSY:
        return PG2SDR_ERROR_BUSY;
    case LIBUSB_ERROR_TIMEOUT:
        return PG2SDR_ERROR_TIMEOUT;
    case LIBUSB_ERROR_NO_MEM:
        return PG2SDR_ERROR_NO_MEMORY;
    case LIBUSB_ERROR_ACCESS:
        return PG2SDR_ERROR_ACCESS;
    }

    /* everything else, throw it into the generic "libusb error" range */
    int converted = PG2SDR_ERROR_LIBUSB_MIN - error; /* nb: error is negative */
    if (converted >= PG2SDR_ERROR_LIBUSB_MIN && converted <= PG2SDR_ERROR_LIBUSB_MAX)
        return converted;
    return PG2SDR_ERROR_LIBUSB_MIN;
}

int pg2sdr__translate_libusb_transfer_status(enum libusb_transfer_status status)
{
    switch (status) {
    case LIBUSB_TRANSFER_COMPLETED:
        return PG2SDR_SUCCESS;
    case LIBUSB_TRANSFER_TIMED_OUT:
        return PG2SDR_ERROR_TIMEOUT;
    case LIBUSB_TRANSFER_STALL:
        return PG2SDR_ERROR_TRANSFER_STALL;
    case LIBUSB_TRANSFER_OVERFLOW:
        return PG2SDR_ERROR_TRANSFER_OVERFLOW;
    case LIBUSB_TRANSFER_NO_DEVICE:
        return PG2SDR_ERROR_DISCONNECTED;
    default:
        return PG2SDR_ERROR_TRANSFER_OTHER;
    }
}

int pg2sdr__translate_errno(int error)
{
    switch (error) {
    case 0:
        return PG2SDR_SUCCESS;
    case ENOMEM:
        return PG2SDR_ERROR_NO_MEMORY;
    case EACCES:
        return PG2SDR_ERROR_ACCESS;
    }

    int converted = PG2SDR_ERROR_SYSTEM_MIN + error; /* nb: error (an errno value) is positive */
    if (converted >= PG2SDR_ERROR_SYSTEM_MIN && converted <= PG2SDR_ERROR_SYSTEM_MAX)
        return converted;

    return PG2SDR_ERROR_SYSTEM_MIN;
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

const char *pg2sdr_strerror(int error)
{
    static char buf[1024];
    return pg2sdr_strerror_r(error, buf, sizeof(buf));
}

const char *pg2sdr_strerror_r(int error, char *buf, size_t buflen)
{
    if (error > PG2SDR_ERROR_SYSTEM_MIN && error <= PG2SDR_ERROR_SYSTEM_MAX) {
        int sys_error = error - PG2SDR_ERROR_SYSTEM_MIN;
        char extrabuf[1024];
        snprintf(buf, buflen, "system error: %s", wrap_strerror(sys_error, extrabuf, sizeof(extrabuf)));
        return buf;
    }

    if (error > PG2SDR_ERROR_LIBUSB_MIN && error <= PG2SDR_ERROR_LIBUSB_MAX) {
        int usb_error = -(error - PG2SDR_ERROR_LIBUSB_MIN);
        snprintf(buf, buflen, "libusb error: %s", libusb_strerror(usb_error));
        return buf;
    }

    switch ((enum pg2sdr_error)error) {
    case PG2SDR_SUCCESS:
        return "no error";

    case PG2SDR_ERROR_NOT_FOUND:
        return "No matching device found";
    case PG2SDR_ERROR_DISCONNECTED:
        return "Device unexpectedly disconnected";
    case PG2SDR_ERROR_BAD_ARGUMENT:
        return "Bad argument to API call";
    case PG2SDR_ERROR_NO_MEMORY:
        return "Memory allocation failed";
    case PG2SDR_ERROR_NOT_IMPLEMENTED:
        return "Operation not implemented";
    case PG2SDR_ERROR_FIRMWARE_MISMATCH:
        return "Host/firmware version mismatch";
    case PG2SDR_ERROR_MULTIPLE_DEVICES:
        return "More than one matching device found";
    case PG2SDR_ERROR_BUSY:
        return "Device already in use";
    case PG2SDR_ERROR_BAD_STATE:
        return "Operation not possible in this state";
    case PG2SDR_ERROR_TIMEOUT:
        return "Operation timed out";
    case PG2SDR_ERROR_CORRUPTION:
        return "Heap corruption, double-free, or use-after-free detected";
    case PG2SDR_ERROR_ACCESS:
        return "Insufficient permissions to access device";

    case PG2SDR_ERROR_TRANSFER_OTHER:
        return "Unexpected libusb transfer status";
    case PG2SDR_ERROR_TRANSFER_STALL:
        return "Bulk endpoint stalled";
    case PG2SDR_ERROR_TRANSFER_OVERFLOW:
        return "Received unexpected data on bulk endpoint";
    case PG2SDR_ERROR_TRANSFER_FORMAT:
        return "Received malformed data on bulk endpoint";

    case PG2SDR_ERROR_TUNER_DETECT:
        return "Tuner not present on I2C bus";
    case PG2SDR_ERROR_TUNER_PLL_LOCK:
        return "Tuner LO PLL did not lock";
    case PG2SDR_ERROR_TUNER_PLL_RANGE:
        return "Required tuner PLL frequency out of range";
    case PG2SDR_ERROR_TUNER_I2C:
        return "Tuner I2C bus communication error";
    case PG2SDR_ERROR_ADC_RATE_RANGE:
        return "Required ADC sampling rate out of range for ADC hardware";

    case PG2SDR_ERROR_SYSTEM_MIN:
        return "Unknown system error";
    case PG2SDR_ERROR_LIBUSB_MIN:
        return "Unknown libusb error";

        /* these are just here so the compiler doesn't complain; they are
         * actually already handled earlier
         */
    case PG2SDR_ERROR_LIBUSB_MAX:
    case PG2SDR_ERROR_SYSTEM_MAX:
        break;
    }

    /* this is deliberately not a default case, so we get compiler
     * complaints if we missed an enum value in the switch above
     */
    snprintf(buf, buflen, "Unknown error code %d", error);
    return buf;
}
