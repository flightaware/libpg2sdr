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
typedef enum { LPCDR_LOG_DEBUG, LPCSDR_LOG_INFO, LPCSDR_LOG_ERROR } lpcsdr_log_level;
typedef void (*lpcsdr_log_callback)(lpcsdr_context *context, lpcsdr_log_level level, const char *message);



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
    LPCSDR_BT_BLOCKLENGTH_MISMATCH = -202       /* Block Length mismatch*/
};

typedef enum {
    LPCSDR_DEVICE_MODE_NORMAL = 0,        
    LPCSDR_DEVICE_MODE_DFU_BOOTLOADER = 1, 
} lpcsdr_device_mode;

typedef struct lpc_device lpc_device;

struct lpc_device {
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
};

typedef struct dfu_status dfu_status;

struct dfu_status {
    int bStatus;
    int bwPollTimeout;
    int bState;
    int iString;
};

struct hotplug_callback_state {
    int completed;
    libusb_device *device;
};

typedef struct lpcsdr_device_handle lpcsdr_device_handle;

struct lpcsdr_device_handle {
    unsigned magic;
    pthread_mutex_t mutex;
    lpcsdr_context *ctx;

    libusb_device_handle *usb_handle;
    // pxsdr_device_info info;

    uint16_t adc_per_frame;     /* firmware reported ADC samples per frame */
    uint16_t frame_size;        /* computed frame size including header, in bytes */
    uint16_t if_filter_low_min; /* firmware reported smallest available if_filter_low */
    // pv2_adc_mode_t adc_mode;    /* firmware reported ADC mode */

    // pxsdr_sampling_mode mode;
    // pxsdr_sample_format format;
    // pxsdr_conversion_mode conversion_mode;
    // pv2_board_type_t board_type;
    uint32_t sample_rate;
    uint32_t frequency;
    int32_t bandpass_low;
    int32_t bandpass_high;
    uint8_t gain[3];

    /* raw values, not transformed for mode: */
    /* last successful requested values (to avoid unnecessary reconfiguration) */
    uint32_t requested_sample_rate;
    uint32_t requested_lo_frequency;
    uint16_t requested_if_filter_low;
    uint16_t requested_if_filter_high;

    /* raw values, not transformed for mode: */
    /* last firmware reported values (might not exactly match what was requested due to hardware limits) */
    uint32_t actual_sample_rate;
    uint32_t actual_lo_frequency;
    uint16_t actual_if_filter_low;
    uint16_t actual_if_filter_high;

    /* baseband filter */
    // struct pxsdr_ifir *baseband_filter;

    /* decimation filters */
    // struct pxsdr_decimate *decimation_filter; /* prototype decimation filter used for each stage */
    int decimation_setting;                   /* if <0: select decimation automatically. if >=0: decimate by 2**N */
    unsigned decimation_log2;                 /* actual decimation in use, 2**N */
    // struct pxsdr_decimate **decimation_stage; /* decimation filter stages */

    /* DC removal */
    // cf32_t dc_bias;

    /* streaming config */
    unsigned buffer_size;  /* requested baseband buffer size, in samples */
    unsigned buffer_count; /* requested number of baseband buffers */

    unsigned hw_bytes_per_sample;    /* Bytes per input sample */
    unsigned hw_elements_per_sample; /* Input elements for one sample */
    unsigned hw_samples_per_frame;   /* Input samples per frame */
    unsigned hw_elements_per_frame;  /* Input elements per frame */
    unsigned hw_frames_per_block;    /* ADC frames needed for one output block */

    unsigned bb_total_decimation; /* Total decimation factor from hw sample rate to bb sample rate */
    unsigned bb_bytes_per_sample; /* Bytes per output sample */

    bool enable_zerocopy;                 /* should we try to allocate zerocopy USB transfers? */
    // pxsdr_internal_sample_block **blocks; /* array of allocated sample blocks */
    unsigned block_count;                 /* length of blocks array */
    unsigned block_size;                  /* allocated size of each sample block, in bytes */

    /* workspace */
    unsigned work_size; /* size of work_0 and work_1, in bytes */
    void *work_A;       /* work areas for conversion */
    void *work_B;

    /* streaming state */
    bool streaming;               /* is pxsdr_stream_data active? */
    bool draining;                /* is drain_transfers active? */
    bool stopping;                /* is there an outstanding pxsdr_stop_streaming request pending? */
    bool overrun;                 /* is there an outstanding stream overrun to report? */
    bool transfer_size_changed;   /* did the required transfer size change mid-streaming due to decimation/buffering change? */
    int completion_flag;          /* libusb wakeup flag, set by the transfer callback */
    uint32_t last_raw_timestamp;  /* last raw timestamp seen */
    uint32_t next_raw_timestamp;  /* expected timestamp of next received frame */
    uint32_t timestamp_rollovers; /* how many times have we seen the raw timestamp roll over? */

    // pxsdr_transfer_state *transfers; /* array of allocated USB transfers */
    unsigned transfer_count;         /* length of transfers array */
    unsigned transfer_size;          /* allocated size of each transfer buffer, in bytes */
    bool using_zerocopy;             /* were the transfer buffers allocated using libusb_dev_mem_alloc? */

    // pxsdr_transfer_state *transfer_head; /* linked list of active transfers */
    // pxsdr_transfer_state *transfer_tail;
};

typedef struct pll_divisors pll_divisors;
struct pll_divisors {
    uint32_t error;
    uint32_t n;
    uint32_t m;
    uint32_t p;
    uint32_t i;
};

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

// Setting various peripherals
int lpcsdr_capture(lpcsdr_device_handle* device_handle, uint32_t num_samples, uint32_t target_frequency);
int lpcsdr_start_transfer(lpcsdr_device_handle *handle, uint32_t target_frequency);
int lpcsdr_stop_transfer(lpcsdr_device_handle *handle);
int calculate_adc_clock_divisors(uint32_t target_frequency, pll_divisors **int_divisors, pll_divisors **frac_divisors);

#if defined(__cplusplus)
}
#endif


#endif /* LPCSDR_H */