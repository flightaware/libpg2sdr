#include "internal.h"

int lpcsdr_translate_libusb_error(lpcsdr_context *ctx, int error)
{   
    switch (error) {
    case LIBUSB_SUCCESS:
        return LPCSDR_SUCCESS;
    case LIBUSB_ERROR_NO_MEM:
        return LPCSDR_ERROR_NO_MEMORY;
    case LIBUSB_ERROR_NO_DEVICE:
        return LPCSDR_ERROR_DISCONNECTED;
    case LIBUSB_ERROR_NOT_FOUND:
        return LPCSDR_ERROR_NOT_FOUND;
    case LIBUSB_ERROR_BUSY:
        return LPCSDR_ERROR_BUSY;
    default:
        if (ctx)
            ctx->last_libusb_error = error;
        return LPCSDR_ERROR_LIBUSB;
    }
}

int lpcsdr_translate_errno(lpcsdr_context *ctx, int error)
{
    switch (error) {
    case 0:
        return LPCSDR_SUCCESS;
    case ENOENT:
        return LPCSDR_ERROR_FWIMAGE_MISSING;
    case ENOMEM:
        return LPCSDR_ERROR_NO_MEMORY;
    default:
        if (ctx)
            ctx->last_errno = error;
        return LPCSDR_ERROR_SYSTEM;
    }
}

const char *lpcsdr_strerror(lpcsdr_context *ctx, int error)
{
    static char buf[512];

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
    case LPCSDR_ERROR_FWIMAGE_FORMAT:
        return "Firmware image format not recognized";
    case LPCSDR_ERROR_FWIMAGE_TRUNCATED:
        return "Firmware image truncated";
    case LPCSDR_ERROR_FWIMAGE_CHECKSUM:
        return "Firmware image checksum mismatch";
    case LPCSDR_ERROR_FWIMAGE_UPLOAD:
        return "Firmware image upload failed";
    case LPCSDR_BT_EXPECTED_LENGTH_MISMATCH:
        return "Bulk Transfer mismatch between expected and actual length";

    case LPCSDR_ERROR_LIBUSB:
        if (ctx && ctx->last_libusb_error) {
            snprintf(buf, sizeof(buf), "libusb: %s", libusb_strerror(ctx->last_libusb_error));
            buf[sizeof(buf) - 1] = 0;
            return buf;
        } else {
            return "libusb error (no detail available)";
        }

    case LPCSDR_ERROR_SYSTEM:
        if (ctx && ctx->last_errno) {
            snprintf(buf, sizeof(buf), "system: %s", strerror(ctx->last_errno));
            buf[sizeof(buf) - 1] = 0;
            return buf;
        } else {
            return "system error (no detail available)";
        }

    default:
        return "Unknown error";
    }
}
