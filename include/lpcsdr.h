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

    // Bulk Transfer (BT) errors
    LPCSDR_BT_EXPECTED_LENGTH_MISMATCH = -200,  /* Expected length and actual length of bytes read mismatch */
    LPCSDR_BT_MAGIC_MISMATCH = -201,            /* Magic number at header is wrong */
    LPCSDR_BT_BLOCKLENGTH_MISMATCH = -202,      /* Block Length mismatch*/
    LPCSDR_BT_SAMPLELENGTH_MISMATCH = -203      /* Block Num Samples mismatch*/
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

typedef struct dfu_status {
    int bStatus;
    int bwPollTimeout;
    int bState;
    int iString;
} dfu_status;

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

struct lpcsdr_device_handle {
    unsigned magic;
    pthread_mutex_t mutex;
    lpcsdr_context *ctx;

    uint32_t usb_samples_per_block_multiple;
    uint32_t usb_bytes_per_block_multiple;
    uint16_t individual_sample_bit_size;
    uint32_t hsadc_frequency;
    uint32_t blocks_per_chunk;

    libusb_device_handle *usb_handle;

    bool streaming; 
    lpcsdr_conversion_mode conversion_mode;

    /* decimation filters */
    struct lpcsdr_decimate *decimation_filter; /* prototype decimation filter used for each stage */
};

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
int lpcsdr_set_firmware_path(struct lpcsdr_context *ctx, char *firmware_path);
int lpcsdr_open_single_device(lpcsdr_context *ctx, lpcsdr_device_handle **device_handle);
int lpcsdr_close_device(lpcsdr_device_handle *dev);

// Open by Methods
// static int generic_match(lpc_device *dev, void *arg);
// static int generic_open_by(lpcsdr_context *ctx, struct match_tuple *match, lpcsdr_device_handle **device);
int lpcsdr_open_by_serial(lpcsdr_context *ctx, const char *serial, lpcsdr_device_handle **device);
int lpcsdr_open_by_address(lpcsdr_context *ctx, uint8_t bus, uint8_t address, lpcsdr_device_handle **device);
int lpcsdr_open_by_index(lpcsdr_context *ctx, unsigned index, lpcsdr_device_handle **device);
int lpcsdr_open_by_callback(lpcsdr_context *ctx, int (*callback)(lpc_device*, void *), void *callback_data, lpcsdr_device_handle **device);

int lpcsdr_start_transfer(lpcsdr_device_handle *handle, uint32_t target_frequency);
int lpcsdr_stop_transfer(lpcsdr_device_handle *handle);
int lpcsdr_read_raw_adc_data(lpcsdr_device_handle* device_handle, ep0_in_board_status_t *status, uint8_t *out, uint32_t total, const char *output_file_path);
// int unpack_raw_adc_data(lpcsdr_device_handle *handle, uint8_t *in, uint32_t in_length, int16_t **out, uint32_t *out_length, uint32_t skip, const char *output_file);
int unpack_raw_adc_data(lpcsdr_device_handle *handle, uint8_t *in, uint32_t in_length, int16_t *out, uint32_t skip, const char *output_file);
int lpcsdr_capture_toy_example(lpcsdr_device_handle* device_handle, uint32_t num_samples, uint32_t target_frequency, uint32_t skip);


#if defined(__cplusplus)
}
#endif


#endif /* LPCSDR_H */