#include "lpcsdr.h"
#include "lpcsdr_protocol.h"

#define MAGIC_CTX 0x18273645
#define MAGIC_DEV 0xABCD
#define MAGIC_FREE 0xFEEE
#define VID_ROM 0x1fc9
#define PID_ROM 0x000c

#define VID_LPCSDR 0xDEAD
#define PID_LPCSDR 0xBEEF

#define DFU_DOWNLOAD_REQUEST 0x1
#define DFU_GET_STATUS_REQUEST 0x3

#define CHECK_CTX(ctx)                       \
    do {                                     \
        if (!ctx)                            \
            return LPCSDR_ERROR_BAD_ARGUMENT;\
        if (ctx->magic != MAGIC_CTX)         \
            return LPCSDR_ERROR_CORRUPTION;   \
    } while (0)

#define CHECK_DEV(dev)                                                            \
    do {                                                                          \
        if (!dev)                                                                 \
            return LPCSDR_ERROR_BAD_ARGUMENT;                                      \
        if (!dev->ctx || dev->magic != MAGIC_DEV || dev->ctx->magic != MAGIC_CTX) \
            return LPCSDR_ERROR_CORRUPTION;                                        \
    } while (0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))


struct lpcsdr_context {
    int magic;
    libusb_context *libusb_ctx;
    int last_libusb_error;
    int last_errno;
    char *firmware_path;
    lpcsdr_log_callback log_cb;
};

struct match_tuple {
    const char *serial;
    int bus;
    int address;
    int index;
};

enum dfu_state {
    DFU_DOWNLOAD_IDLE = 0x05 // dfuDNLOAD-IDLE state for when downloading firmware onto lpcsdr device
};

enum dfu_error {
    DFU_ERROR_TARGET = -200, // File is not targeted for use by this device.
    DFU_ERROR_FILE = -201, // File is for this device but fails some vendor-specific verification test.
    DFU_ERROR_WRITE = -202, // Device is unable to write memory.
    DFU_ERROR_ERASE = -203, // Memory erase function failed.
    DFU_ERROR_CHECK_ERASED = -204, // Memory erase check failed.
    DFU_ERROR_PROG = -205, // Program memory function failed.
    DFU_ERROR_VERIFY = -206, // Programmed memory failed verification.
    DFU_ERROR_ADDRESS = -207, // Cannot program memory due to received address that is out of range.
    DFU_ERROR_NOTDONE = -208, // Received DFU_DNLOAD with wLength = 0, but device does not think it has all of the data yet.
    DFU_ERROR_FIRMWARE = -209, // Device’s firmware is corrupt. It cannot return to run-time (non-DFU) operations.
    DFU_ERROR_VENDOR = -210, // iString indicates a vendor-specific ERROR_or.
    DFU_ERROR_USBR = -211, // Device detected unexpected USB reset signaling.
    DFU_ERROR_POR = -212, // Device detected unexpected power on reset.
    DFU_ERROR_UNKNOWN = -213,
    DFU_ERROR_STALLEDPKT  = -214, // Device stalled an unexpected request.
    DFU_ERROR_NON_IDLE_STATE = -215, // During DFU download after GET_STATUS request, state is NOT in dfuDNLOAD-IDLE  state.
};

int lpcsdr_comms_check(libusb_device_handle *device_handle);
int dfu_download_firmware(libusb_device_handle *handle, int block , u_int32_t *buffer, int count);
int dfu_get_status(libusb_device_handle *dev, dfu_status **status);
int lpcsdr_upload_firmware(lpcsdr_context *ctx, libusb_device_handle *handle);
int lpcsdr_handle_rom_bootloader(lpcsdr_context *ctx, libusb_device *original_dev, libusb_device **reenumerated_dev);
int translate_dfu_status(int dfu_status);

int lpcsdr_translate_libusb_error(struct lpcsdr_context *ctx, int error);
int lpcsdr_translate_errno(lpcsdr_context *ctx, int error);
int lpcsdr_get_status(lpcsdr_device_handle *device_handle, ep0_in_board_status_t **status);


int n_value(uint32_t n);
int i_value(uint32_t n);
int p_value(uint32_t n);
int divider_comparator(int error, int n, pll_divisors *b);

int build_lpc_device(lpcsdr_context *ctx, lpcsdr_device_handle **d);
int get_initial_device_from_list(lpcsdr_context *ctx, libusb_device **usb_list, int device_count, libusb_device **device);