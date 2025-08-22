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
#include "lpcsdr_protocol.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct lpcsdr_context lpcsdr_context;
typedef enum { LPCDR_LOG_DEBUG, LPCSDR_LOG_INFO, LPCSDR_LOG_ERROR } lpcsdr_log_level;
typedef void (*lpcsdr_log_callback)(lpcsdr_context *context, lpcsdr_log_level level, const char *message);
typedef struct lpcsdr_device_handle lpcsdr_device_handle;
typedef bool (*lpcsdr_stream_callback)(void *block, void *user_data);

typedef enum { LPCSDR_LOWIF_REAL, LPCSDR_LOWIF_COMPLEX } lpcsdr_conversion_mode;

enum lpcsdr_error {
    LPCSDR_SUCCESS = 0, /* no error */

    LPCSDR_ERROR_LIBUSB = -1, /* libusb error (see pxsdr_last_usb_error()) */
    LPCSDR_ERROR_SYSTEM = -2, /* system call error (see pxsdr_last_errno()) */

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
    LPCSDR_ERROR_FWIMAGE_FORMAT = -101,    /* firmware image format not recognized */
    LPCSDR_ERROR_FWIMAGE_TRUNCATED = -102, /* firmware image truncated */
    LPCSDR_ERROR_FWIMAGE_CHECKSUM = -103,  /* firmware image checksum mismatch */
    LPCSDR_ERROR_FWIMAGE_UPLOAD = -104,    /* firmware image upload failed */
    LPCSDR_ERROR_FWIMAGE_TIMEOUT = -105,   /* firmware did not re-enumerate within timeout after firmware upload */

    /* Bulk Transfer (BT) errors */
    LPCSDR_BT_EXPECTED_LENGTH_MISMATCH = -200,  /* Expected length and actual length of bytes read mismatch */
    LPCSDR_BT_MAGIC_MISMATCH = -201,            /* Magic number at header is wrong */
    LPCSDR_BT_BLOCKLENGTH_MISMATCH = -202,      /* Block Length mismatch*/
    LPCSDR_BT_SAMPLELENGTH_MISMATCH = -203,     /* Block Num Samples mismatch*/

    /* Tuner errors */
    LPCSDR_TUNER_REGISTER_SYMBOL_NOT_FOUND = -300, /* Could not find provided register symbol for a given register */
    LPCSDR_TUNER_INIT_FAILED = -301,               /* TUNER_ID was not correct value. Tuner init failed somehow */
};

typedef enum {
    LPCSDR_DEVICE_MODE_NORMAL = 0,        
    LPCSDR_DEVICE_MODE_DFU_BOOTLOADER = 1, 
} lpcsdr_device_mode;

typedef struct lpc_device {
    lpcsdr_context *context;
    lpcsdr_device_mode mode;
    char serial[9];               /* serial number string, ASCIIZ */
    unsigned index;
    /* USB connection details: */
    bool usb_superspeed; /* true if this device is connected via a USB3.0
                            superspeed connection */
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
     * In REAL_INVERTED mode, each sample is a single value.
     * In COMPLEX_BASEBAND mode, each sample is two values representing the I and Q channels respectively.
     * The type of each value is controled by the sample format - int16_t or float
     */
    void *samples;

    /* Number of samples available */
    unsigned count;

    /* Sample timestamp (cumulative number of received samples), at the start of the buffer.
     * This counter may not initially start at zero.
     */
    uint64_t timestamp;

} lpcsdr_sample_buffer;

typedef struct pll_divisors {
    bool fractional;

    uint32_t n;
    float m;
    uint32_t p;
    uint32_t i;

    float error;
    float actual_fcco;
    float actual_frequency;
} pll_divisors;

int lpcsdr_init(lpcsdr_context **ctx);
int lpcsdr_exit(lpcsdr_context *ctx);
int lpcsdr_open_device(lpc_device *device, lpcsdr_device_handle **device_handle);
int lpcsdr_set_log_callback(lpcsdr_context *ctx, lpcsdr_log_callback callback);
int lpcsdr_free_device_list(lpc_device **device_list);
int lpcsdr_discover_devices(lpcsdr_context *ctx, lpc_device ***lpc_device_list, bool allow_rom_bootloader);
const char *lpcsdr_strerror(lpcsdr_context *ctx, int error);
int lpcsdr_set_firmware_path(struct lpcsdr_context *ctx, const char *firmware_path);
int lpcsdr_open_single_device(lpcsdr_context *ctx, lpcsdr_device_handle **device_handle);
int lpcsdr_close_device(lpcsdr_device_handle *dev);

// Control Transfers
int lpcsdr_get_status(lpcsdr_device_handle *dev, ep0_in_board_status_t **status);
int lpcsdr_set_rf_power(lpcsdr_device_handle *dev, uint16_t mode);

// Open by Methods
// static int generic_match(lpc_device *dev, void *arg);
// static int generic_open_by(lpcsdr_context *ctx, struct match_tuple *match, lpcsdr_device_handle **device);
int lpcsdr_open_by_serial(lpcsdr_context *ctx, const char *serial, lpcsdr_device_handle **device);
int lpcsdr_open_by_address(lpcsdr_context *ctx, uint8_t bus, uint8_t address, lpcsdr_device_handle **device);
int lpcsdr_open_by_index(lpcsdr_context *ctx, unsigned index, lpcsdr_device_handle **device);
int lpcsdr_open_by_callback(lpcsdr_context *ctx, int (*callback)(lpc_device*, void *), void *callback_data, lpcsdr_device_handle **device);

//Streaming
int lpcsdr_set_buffering(lpcsdr_device_handle *dev, unsigned buffer_count, unsigned buffer_size);
int lpcsdr_get_buffering(lpcsdr_device_handle *dev, unsigned *buffer_count, unsigned *buffer_size);
int lpcsdr_stream_data(lpcsdr_device_handle *dev, lpcsdr_stream_callback callback, void *user_data, unsigned timeout_ms);
int lpcsdr_stop_streaming(lpcsdr_device_handle *dev);

#if defined(__cplusplus)
}
#endif


#endif /* LPCSDR_H */
