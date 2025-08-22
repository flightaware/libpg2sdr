#include "internal.h"

int lpcsdr_translate_libusb_error(int error)
{
    if (error == LIBUSB_SUCCESS)
        return LPCSDR_SUCCESS;

    int converted = LPCSDR_ERROR_LIBUSB_MIN - error; /* nb: error is negative */
    if (converted >= LPCSDR_ERROR_LIBUSB_MIN && converted <= LPCSDR_ERROR_LIBUSB_MAX)
        return converted;

    return LPCSDR_ERROR_LIBUSB_MIN;
}

int lpcsdr_translate_libusb_transfer_status(enum libusb_transfer_status status)
{
    switch (status) {
    case LIBUSB_TRANSFER_COMPLETED:
        return LPCSDR_SUCCESS;
    case LIBUSB_TRANSFER_STALL:
        return LPCSDR_BT_STALL;
    case LIBUSB_TRANSFER_OVERFLOW:
        return LPCSDR_BT_OVERFLOW;
    default:
        return LPCSDR_BT_GENERIC_ERROR;
    }
}

int lpcsdr_translate_errno(int error)
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
        return "Timed out waiting for data";
    case LPCSDR_ERROR_USB_IO:
        return "Bulk transfer I/O error";
    case LPCSDR_ERROR_CORRUPTION:
        return "Heap corruption or double-free detected";
    case LPCSDR_ERROR_FIRMWARE_FAILURE:
        return "Generic firmware error";
    case LPCSDR_ERROR_TUNER_I2C:
        return "Tuner I2C communication error";
    case LPCSDR_ERROR_TUNER_NO_LOCK:
        return "Tuner PLL lock failure";
    case LPCSDR_ERROR_CLOCK_I2C:
        return "Clock I2C communication error";
    case LPCSDR_ERROR_CLOCK_NO_LOCK:
        return "Clock PLL lock failure";
    case LPCSDR_ERROR_OUT_OF_RANGE:
        return "Parameter value out of range";
    case LPCSDR_ERROR_FWIMAGE_MISSING:
        return "Firmware image not found";
    case LPCSDR_ERROR_FWIMAGE_UPLOAD:
        return "Firmware image upload failed";
    case LPCSDR_ERROR_FWIMAGE_TIMEOUT:
        return "Timeout after uploading firmware image";
    case LPCSDR_BT_MAGIC_MISMATCH:
        return "LPCSDR_BT_MAGIC_MISMATCH needs a description";
    case LPCSDR_BT_SAMPLELENGTH_MISMATCH:
        return "LPCSDR_BT_SAMPLELENGTH_MISMATCH needs a description";
    case LPCSDR_BT_BLOCKLENGTH_MISMATCH:
         return "LPCSDR_BT_BLOCKLENGTH_MISMATCH needs a description";
    case LPCSDR_TUNER_REGISTER_SYMBOL_NOT_FOUND:
         return "LPCSDR_TUNER_REGISTER_SYMBOL_NOT_FOUND needs a description";
    case LPCSDR_TUNER_INIT_FAILED:
         return "LPCSDR_TUNER_INIT_FAILED needs a description";
    case LPCSDR_BT_EXPECTED_LENGTH_MISMATCH:
        return "Bulk Transfer mismatch between expected and actual length";
    case LPCSDR_BT_GENERIC_ERROR:
        return "generic bulk transfer failed";
    case LPCSDR_BT_STALL:
        return "bulk transfer endpoint stalled";
    case LPCSDR_BT_OVERFLOW:
        return "bulk transfer returned more data that expected";

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
