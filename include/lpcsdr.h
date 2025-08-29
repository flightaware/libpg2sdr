#ifndef LPCSDR_H
#define LPCSDR_H

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <endian.h>
#include <string.h>
#include <errno.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct lpcsdr_context lpcsdr_context;
typedef enum { LPCSDR_LOG_DEBUG, LPCSDR_LOG_INFO, LPCSDR_LOG_ERROR } lpcsdr_log_level;
typedef void (*lpcsdr_log_callback)(lpcsdr_context *context, lpcsdr_log_level level, const char *message);
typedef struct lpcsdr_device_handle lpcsdr_device_handle;

typedef enum { LPCSDR_MODE_LOWIF_REAL, LPCSDR_MODE_BASEBAND } lpcsdr_conversion_mode;

enum lpcsdr_error {
    LPCSDR_SUCCESS = 0, /* no error */

    LPCSDR_ERROR_NOT_FOUND = -3,         /* no matching device found */
    LPCSDR_ERROR_DISCONNECTED = -4,      /* device unexpectedly disconnected */
    LPCSDR_ERROR_BAD_ARGUMENT = -5,      /* bad argument to API call */
    LPCSDR_ERROR_NO_MEMORY = -6,         /* memory allocation failed */
    LPCSDR_ERROR_NOT_IMPLEMENTED = -7,   /* not implemented */
    LPCSDR_ERROR_FIRMWARE_MISMATCH = -8, /* host/firmware version mismatch */
    LPCSDR_ERROR_MULTIPLE_DEVICES = -9,  /* more than one device found */
    LPCSDR_ERROR_BUSY = -10,             /* device already in use */
    LPCSDR_ERROR_BAD_STATE = -11,        /* operation not possible in this state */
    LPCSDR_ERROR_TIMEOUT = -12,          /* timed out waiting for data */
    LPCSDR_ERROR_USB_IO = -13,           /* Bulk transfer I/O error */
    LPCSDR_ERROR_CORRUPTION = -14,       /* Heap corruption or double-free detected */

    /* firmware-generated errors */
    LPCSDR_ERROR_FIRMWARE_FAILURE = -50, /* generic firmware error */
    LPCSDR_ERROR_TUNER_I2C = -51,        /* tuner I2C communication error */
    LPCSDR_ERROR_TUNER_NO_LOCK = -52,    /* tuner PLL lock failure */
    LPCSDR_ERROR_CLOCK_I2C = -53,        /* clock I2C communication error */
    LPCSDR_ERROR_CLOCK_NO_LOCK = -54,    /* clock PLL lock failure */
    LPCSDR_ERROR_OUT_OF_RANGE = -55,     /* parameter value out of range */

    /* firmware image errors */
    LPCSDR_ERROR_FWIMAGE_MISSING = -100,   /* firmware image not found */
    LPCSDR_ERROR_FWIMAGE_UPLOAD = -104,    /* firmware image DFU upload failed */
    LPCSDR_ERROR_FWIMAGE_TIMEOUT = -105,   /* firmware did not re-enumerate within timeout after firmware upload */

    LPCSDR_ERROR_TRANSFER_ERROR = -200,         /* libusb transfer status not COMPLETED and not otherwise handled */
    LPCSDR_ERROR_TRANSFER_STALL = -201,         /* libusb transfer status LIBUSB_TRANSFER_STALL, endpoint stalled */
    LPCSDR_ERROR_TRANSFER_OVERFLOW = -202,      /* libusb transfer status LIBUSB_TRANSFER_OVERFLOW, device sent more data than requested */
    LPCSDR_ERROR_TRANSFER_FORMAT = -203,        /* malformed bulk transfer data */

    /* Tuner errors */
    LPCSDR_TUNER_REGISTER_SYMBOL_NOT_FOUND = -300, /* Could not find provided register symbol for a given register */
    LPCSDR_TUNER_INIT_FAILED = -301,               /* TUNER_ID was not correct value. Tuner init failed somehow */
    LCPSDR_TUNER_PLL_DIV_OUT_OF_RANGE = -302,      /* Requested PLL Divisor was our of range */
    LPCSDR_TUNER_LOCK_ERR = -303,                   /* Tuner PLL could not get lock */
    LPCSDR_TUNER_LPF_INVALID_ARG = -304,

    /* system call error range */
    LPCSDR_ERROR_SYSTEM_MAX = -1000,
    LPCSDR_ERROR_SYSTEM_MIN = -1999,

    /* libusb error range */
    LPCSDR_ERROR_LIBUSB_MAX = -2000,
    LPCSDR_ERROR_LIBUSB_MIN = -2999,
};

typedef enum {
    LPCSDR_DEVICE_MODE_NORMAL = 0,        
    LPCSDR_DEVICE_MODE_DFU_BOOTLOADER = 1, 
} lpcsdr_device_mode;

typedef struct lpc_device {
    lpcsdr_context *context;
    lpcsdr_device_mode mode;
    char serial[17];               /* serial number string, ASCIIZ */
    unsigned index;
    /* USB connection details: */
    uint8_t usb_bus;     /* bus number this device is connected to */
    uint8_t usb_address; /* address within usb_bus */
    void *libusb_device;
} lpc_device;

struct hotplug_callback_state {
    int completed;
    libusb_device *device;
};

struct lpcsdr_ifir {
    unsigned ntaps;
    int16_t *taps;
    int16_t qtap;
    int16_t *history_i;
    int16_t *history_q;
};

typedef struct {
    /* Handle for the device producing this sample block. */
    lpcsdr_device_handle *dev;

    /* Sample data.
     * In LOWIF_REAL mode, each sample is a single 16-bit value.
     * In BASEBAND mode, each sample is two 16-bit values representing the I and Q channels respectively.
     */
    int16_t *samples;

    /* Number of samples available */
    unsigned count;

    /* Sample timestamp (cumulative number of received samples), at the start of the buffer.
     * This counter may not initially start at zero.
     */
    uint64_t timestamp;

} lpcsdr_sample_buffer;

typedef bool (*lpcsdr_stream_callback)(lpcsdr_sample_buffer *buffer, void *user_data);

const char *lpcsdr_strerror(int error);
const char *lpcsdr_strerror_r(int error, char *buf, size_t buflen);

/* context.c */
int lpcsdr_init(lpcsdr_context **ctx);
int lpcsdr_exit(lpcsdr_context *ctx);
int lpcsdr_set_firmware_path(lpcsdr_context *ctx, const char *firmware_path);
int lpcsdr_set_log_callback(lpcsdr_context *ctx, lpcsdr_log_callback callback);

/* device.c */
int lpcsdr_open_device(lpc_device *device, lpcsdr_device_handle **device_handle);
void lpcsdr_free_device_list(lpc_device **device_list);
ssize_t lpcsdr_discover_devices(lpcsdr_context *ctx, lpc_device ***lpc_device_list, bool allow_rom_bootloader);
int lpcsdr_open_single_device(lpcsdr_context *ctx, lpcsdr_device_handle **device_handle);
int lpcsdr_close_device(lpcsdr_device_handle *dev);
int lpcsdr_get_serial(lpcsdr_device_handle *dev, char *serial, size_t length);

// Open by Methods
// static int generic_match(lpc_device *dev, void *arg);
// static int generic_open_by(lpcsdr_context *ctx, struct match_tuple *match, lpcsdr_device_handle **device);
int lpcsdr_open_by_serial(lpcsdr_context *ctx, const char *serial, lpcsdr_device_handle **device);
int lpcsdr_open_by_address(lpcsdr_context *ctx, uint8_t bus, uint8_t address, lpcsdr_device_handle **device);
int lpcsdr_open_by_index(lpcsdr_context *ctx, unsigned index, lpcsdr_device_handle **device);
int lpcsdr_open_by_callback(lpcsdr_context *ctx, int (*callback)(lpc_device*, void *), void *callback_data, lpcsdr_device_handle **device);

// Device configuration
int lpcsdr_set_buffer_size(lpcsdr_device_handle *dev, size_t buffer_size);
int lpcsdr_get_buffer_size(lpcsdr_device_handle *dev, size_t *buffer_size);
int lpcsdr_set_sample_rate(lpcsdr_device_handle *dev, uint32_t rate);
int lpcsdr_get_sample_rate(lpcsdr_device_handle *dev, uint32_t *rate);
int lpcsdr_tune_pll(lpcsdr_device_handle *dev, double requested_frequency);
int lpcsdr_set_lna_gain(lpcsdr_device_handle *dev, uint16_t gain);
int lpcsdr_set_mix_gain(lpcsdr_device_handle *dev, uint16_t gain);
int lpcsdr_set_vga_gain(lpcsdr_device_handle *dev, uint16_t gain);
int lpcsdr_set_bandwidth_highend_cutoff(lpcsdr_device_handle *dev, int cutoff, int *not_above);
int lpcsdr_set_bandwidth_lowend_cutoff(lpcsdr_device_handle *dev, int cutoff);
int lpcsdr_set_center_frequency_bandwidth(lpcsdr_device_handle *dev, int low, int high, int *max);

// Streaming
int lpcsdr_stream_data(lpcsdr_device_handle *dev, lpcsdr_stream_callback callback, void *user_data, unsigned timeout_ms);
int lpcsdr_stop_streaming(lpcsdr_device_handle *dev);
void lpcsdr_release_buffer(lpcsdr_sample_buffer *buffer);

#if defined(__cplusplus)
}
#endif


#endif /* LPCSDR_H */
