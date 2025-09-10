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

typedef enum { LPCSDR_MODE_LOWIF_REAL, LPCSDR_MODE_BASEBAND } lpcsdr_conversion_mode_t;

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
    LPCSDR_TUNER_PLL_DIV_OUT_OF_RANGE = -302,      /* Requested PLL Divisor was our of range */
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
    uint8_t usb_bus;      /* bus number this device is connected to */
    uint8_t usb_address;  /* address within usb_bus */
    uint8_t usb_ports[8]; /* ports (from root) of this device, 0-terminated */
    void *libusb_device;
} lpc_device;

struct hotplug_callback_state {
    int completed;
    libusb_device *device;
};

typedef struct {
    /* Handle for the device producing this sample block. */
    lpcsdr_device_handle *dev;

    /* Sample data.
     * In LOWIF_REAL mode, each sample is a single 16-bit value.
     * In BASEBAND mode, each sample is two 16-bit values representing the I and Q channels respectively.
     */
    int16_t *samples;

    /* Number of samples available. Note that in BASEBAND mode, there will be count*2
     * individual int16_t values in total, as each sample consists of two int16_t values.
     *
     * lpcsdr_set_buffer_size controls the maximum number of samples provided per callback
     * i.e. the maximum value of "count".
     */
    unsigned count;

    /* Sample timestamp (cumulative number of received samples), at the start of the buffer.
     * This counter may not initially start at zero, and may be discontinuous between
     * callbacks if an overrun causes data to be dropped.
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

/*
 * Conversion mode, sample rate, center frequency, sideband, bandpass, decimation mode:
 * changes made to these parameters can interact, and need a call to lpcsdr_apply_changes to take effect
 * after you've completed all the changes you want.
 */

/* Set the current conversion mode to "mode", controlling the format of data returned (low-IF versus baseband).
 *
 * If mode is LPCSDR_MODE_BASEBAND, then user samples are complex baseband, with two int16_t values (I/Q, or
 * real/imaginary) per sample. The resulting signal, centered around 0Hz, corresponds to the RF signal centered
 * around the configured center frequency. This is the mode that most SDR clients will want to use.
 *
 * If mode is LPCSDR_MODE_LOWIF_REAL, then user samples are the real-valued output of the ADC, with one int16_t
 * value per sample. The resulting signal corresponds to one sideband of the RF spectrum, either above or
 * below the configured frequency depending on the configured sideband mode. The configured RF frequency
 * maps to 0Hz (though there will not be anything useful there, due to both the limits of the tuner bandpass
 * filter, and LO leakage). This mode is mostly for lower-level debugging of the LPCSDR hardware or software
 * itself, where direct inspection of the ADC data is useful.
 *
 * May not be called while streaming; will return LPCSDR_ERROR_BAD_STATE if this is attempted.
 *
 * Call lpcsdr_apply_changes to complete the configuration change.
 */
int lpcsdr_set_conversion_mode(lpcsdr_device_handle *dev, lpcsdr_conversion_mode_t mode);

/* Get the current conversion mode and store it in *mode. */
int lpcsdr_get_conversion_mode(lpcsdr_device_handle *dev, lpcsdr_conversion_mode_t *mode);

/* Set the user buffer size to "buffer_size" samples. This controls the maximum number of samples contained
 * in each lpcsdr_sample_buffer passed to the user callback while lpcsdr_stream_data is running.
 *
 * May not be called while streaming; will return LPCSDR_ERROR_BAD_STATE if this is attempted.
 */
int lpcsdr_set_buffer_size(lpcsdr_device_handle *dev, size_t buffer_size);

/* Get the current user buffer size and store it in *buffer_size */
int lpcsdr_get_buffer_size(lpcsdr_device_handle *dev, size_t *buffer_size);

/* Set the current requested sample rate to "rate".
 *
 * May be called at any time; does not affect the sample rate of any currently active stream.
 * Call lpcsdr_apply_changes to complete the configuration change.
 */
int lpcsdr_set_sample_rate(lpcsdr_device_handle *dev, double rate);

/* Get current sample rate configuration.
 *
 * The currently requested sample rate is stored in *requested (if not NULL)
 *
 * The actual configured sample rate is stored in *actual (if not NULL), or 0 if sample
 * rate configuration is still waiting to be applied. The actual rate reflects the effective
 * rate that the hardware is configured for, and may differ slightly from the requested rate
 * due to limitations of the hardware.
 */
int lpcsdr_get_sample_rate(lpcsdr_device_handle *dev, double *requested, double *actual);

/* Set the current decimation mode to "decimation_mode". This controls additional, transparent,
 * ADC sample rate scaling and decimation performed in the receive path. In all cases, the user
 * callback still receives data at the requested sample rate.
 *
 * Extra decimation can provide lower noise or better bandpass filtering characteristics, at the
 * cost of needing to run the ADC at a higher sample rate and transfer more data over the USB
 * bus.
 *
 * decimation_mode may be one of:
 *
 *   0:    do not do any extra decimation of the received signal;
 *
 *   1..LPCSDR_DECIMATION_MAX: scale the ADC sample rate by 2**N. Decimate received data by 2**N.
 *
 *   LPCSDR_DECIMATION_AUTO: use power-of-two ADC scaling and decimation to move the
 *    intermediate frequency range used by the received signal away from 0Hz. This avoids
 *    problems with the effective bandwidth being limited by tuner filtering near 0Hz,
 *    an issue that mostly affects lower sample rates. Won't increase the ADC frequency past
 *    20MHz. This is the default setting.
 *
 *   LPCSDR_DECIMATION_AUTO_MAX: use the largest possible power-of-two ADC scaling
 *    and decimation. Won't increase the ADC frequency past 20MHz.
 *
 * May be called at any time; does not affect the configuration of any currently active stream.
 * Call lpcsdr_apply_changes to complete the configuration change.
 */
#define LPCSDR_DECIMATION_MAX (8)
#define LPCSDR_DECIMATION_AUTO (-1)
#define LPCSDR_DECIMATION_AUTO_MAX (-2)
int lpcsdr_set_decimation_mode(lpcsdr_device_handle *dev, int decimation_mode);

/* Get the currently requested decimation mode and store it in *decimation_mode */
int lpcsdr_get_decimation_mode(lpcsdr_device_handle *dev, int *decimation_mode);

/* Set the current sideband tuning mode.
 *
 * If upper_sideband is true, the tuner LO will be tuned below the requested frequency, and
 * frequencies above the LO will be received.
 *
 * If upper_sideband is false, the tuner LO will be tuned above the requested frequency, and
 * frequencies below the LO will be received.
 *
 * Setting upper_sideband=true near the top of the available tuner range, or
 * setting upper_sideband=false near the bottom of the available range, will slightly extend
 * the range of tunable frequencies.
 *
 * May be called at any time; does not affect the configuration of any currently active stream.
 * Call lpcsdr_apply_changes to complete the configuration change.
 */
int lpcsdr_set_sideband(lpcsdr_device_handle *dev, bool upper_sideband);

/* Get the currently requested sideband tuning mode and store it in *upper_sideband */
int lpcsdr_get_sideband(lpcsdr_device_handle *dev, bool *upper_sideband);

/* Set the center frequency for received data. This is the RF frequency that will be
 * downconverted to 0Hz in samples provided to user callbacks, in both baseband and low-IF modes.
 *
 * May be called at any time; does not affect the configuration of any currently active stream.
 * Call lpcsdr_apply_changes to complete the configuration change.
 */
int lpcsdr_set_frequency(lpcsdr_device_handle *dev, double frequency);

/* Get current center frequency configuration.
 *
 * The currently requested center frequency is stored in *requested (if not NULL)
 *
 * The actual configured center frequency is stored in *actual (if not NULL), or 0 if center
 * frequency configuration is still waiting to be applied. The actual frequency reflects the
 * effective frequency that the hardware is configured for, and may differ slightly from the
 * requested frequency due to limitations of the hardware.
 */
int lpcsdr_get_frequency(lpcsdr_device_handle *dev, double *requested, double *actual);

/* Set bandpass filter limits.
 *
 * This controls the cutoffs of the analog tuner bandpass filter that shapes the incoming signal
 * before the ADC. The low and high limits are relative to the RF frequency set by
 * lpcsdr_set_frequency. Signals between the limits are retained, other signals are attenuated.
 *
 * In baseband mode, usually the low limit will be negative and the high limit will be positive,
 * as you want to receive a signal that is on both sides of the center frequency. For a simple
 * bandwidth setting, pass low = -bandwidth/2 and high = +bandwidth/2.
 *
 * In low-IF mode, both will either be positive (in upper-sideband mode) or negative (in low-
 * sideband mode) as all of the RF signal captures is on one side of the LO.
 *
 * The actual bandpass limits used may be significantly different to what is requested, for two
 * reasons:
 *
 *   a) the hardware is quite limited in terms of the available filtering options;
 *   b) the sampling rate chosen influences the maximum cutoff on one side of the bandpass region,
 *      as we must pick a cutoff below the ADC's Nyquist frequency to avoid aliasing in the ADC.
 *
 * May be called at any time; does not affect the configuration of any currently active stream.
 * Call lpcsdr_apply_changes to complete the configuration change.
 */
int lpcsdr_set_bandpass(lpcsdr_device_handle *dev, double low, double high);

/* Get current bandpass filter configuration.
 *
 * The currently requested limits are stored in *req_low and *req_high (if not NULL)
 *
 * The actual configured limits are stored in *actual_low and *actual_high (if not NULL), or
 * 0 if bandpass configuration is still waiting to be applied. The actual bandpass reflects the
 * effective bandpass cutoffs that the hardware is configured for, and may differ from the
 * requested limits due to limitations of the hardware.
 *
 * All limits are relative to the configured center frequency, i.e. a limit of +1MHz corresponds
 * to a cutoff of +1MHz at baseband.
 */
int lpcsdr_get_bandpass(lpcsdr_device_handle *dev, double *req_low, double *req_high, double *actual_low, double *actual_high);

/* Attempt to apply any outstanding configuration changes to conversion mode, sampling rate,
 * decimation mode, center frequency, sideband mode, and bandpass limits.
 *
 * This function should be called after all the corresponding lpcsdr_set_... functions for a
 * batch of changes have been completed. These settings interact, so it is possible that
 * the hardware can support the final configuration successfully but not every intermediate
 * configuration. Batching the changes together with a single "apply" call allows you to
 * jump to the final configuration in one step.
 *
 * Not all configuration changes can be applied while streaming is active. Either stop
 * streaming before calling lpcsdr_apply_changes, or interpret returned LPCSDR_ERROR_BAD_STATE
 * errors as "some changes could not be applied because streaming is active". If BAD_STATE is
 * returned, the device remains in a consistent state, and any unapplied changes remain
 * pending and can be applied later by a futher call to lpcsdr_apply_changes.
 *
 * It is safe to call lpcsdr_apply_changes if no changes are pending, doing so is a no-op.
 *
 * Calls to lpcsdr_stream_data implicit call lpcsdr_apply_changes before starting streaming.
 */
int lpcsdr_apply_changes(lpcsdr_device_handle *dev);

// Gain
int lpcsdr_set_lna_gain(lpcsdr_device_handle *dev, uint16_t gain);
int lpcsdr_set_mix_gain(lpcsdr_device_handle *dev, uint16_t gain);
int lpcsdr_set_vga_gain(lpcsdr_device_handle *dev, uint16_t gain);

// Streaming
int lpcsdr_stream_data(lpcsdr_device_handle *dev, lpcsdr_stream_callback callback, void *user_data, unsigned timeout_ms);
int lpcsdr_stop_streaming(lpcsdr_device_handle *dev);
void lpcsdr_release_buffer(lpcsdr_sample_buffer *buffer);

#if defined(__cplusplus)
}
#endif


#endif /* LPCSDR_H */
