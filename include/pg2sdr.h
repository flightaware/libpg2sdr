#ifndef PG2SDR_H
#define PG2SDR_H

/**
 *  \file pg2sdr.h
 *
 *  \brief The main pg2sdr header.
 *
 *  The public pg2sdr API is entirely contained in a single header file.
 */

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

/**
 * \defgroup context Library context and init/deinit
 *
 * Library contexts allow for multiple users of the pg2sdr library in
 * the same process, without requiring coordination between the
 * different users. Each library user should allocate a separate
 * context, and pass it to the various API functions that expect a
 * context.
 *
 * There is a context associated with each ::pg2sdr_device instance.
 * APIs that accept ::pg2sdr_device arguments implicitly use the
 * associated context.
 *
 * The lifetime of contexts is managed by pg2sdr_init() and
 * pg2sdr_exit().
 *
 */

/**
 * \brief An opaque handle for library state.
 * \ingroup context
 *
 * Library context is represented as a pointer to an opaque
 * pg2sdr_context struct, where the details of the struct are
 * internal to the library.
 *
 * \sa pg2sdr_init()
 * \sa pg2sdr_exit()
 */
typedef struct pg2sdr__context pg2sdr_context;

/**
 * \brief Severity of log messages passed to ::pg2sdr_log_callback.
 * \ingroup context
 */
typedef enum {
    PG2SDR_LOG_DEBUG, /**< debug messages */
    PG2SDR_LOG_INFO,  /**< informational messages */
    PG2SDR_LOG_ERROR  /**< errors */
} pg2sdr_log_level;

/**
 * \brief Callback function passed to \ref pg2sdr_set_log_callback.
 * \ingroup context
 *
 * \param context the library context associated with this log message
 * \param level the log level of this message
 * \param message an ASCII message to log
 */
typedef void (*pg2sdr_log_callback)(pg2sdr_context *context,
                                    pg2sdr_log_level level,
                                    const char *message);

/**
 * \brief Allocate a new library context.
 * \ingroup context
 *
 * Context should be eventually freed by calling pg2sdr_exit()
 *
 * \param[out] ctx Storage for a pointer to the newly allocated context
 * \return ::PG2SDR_SUCCESS on success, negative error code on failure
 */
int pg2sdr_init(pg2sdr_context **ctx);

/**
 * \brief Free a library context.
 * \ingroup context
 *
 * Releases resources associated with a context previously allocated by
 * pg2sdr_init().
 *
 * Must not be called while there are in-progress API calls using the
 * context.
 *
 * After a call to pg2sdr_exit(), the context is no longer valid and
 * should not be used.
 *
 * \param[in] ctx The context to free
 * \return ::PG2SDR_SUCCESS on success, negative error code on failure
 */
int pg2sdr_exit(pg2sdr_context *ctx);

/**
 * \brief Set a callback to receive library logging.
 * \ingroup context
 *
 * By default, libpg2sdr will log INFO and ERROR messages to stderr.
 * If the PG2SDR_DEBUG environment variable is set, it will also log
 * DEBUG messages to stderr.
 *
 * If log messages should be handled differently, provide a logging
 * callback by calling this function. libpg2sdr will call the logging
 * callback to perform logging, replacing the default behaviour.
 *
 * Each library context has a separate log callback setting.
 *
 * Logging callbacks must be threadsafe.
 *
 * \param[in] ctx The library context to change
 * \param[in] callback The callback to call for each log message
 * \return ::PG2SDR_SUCCESS on success, negative error code on failure
 */
int pg2sdr_set_log_callback(pg2sdr_context *ctx, pg2sdr_log_callback callback);

/**
 * \defgroup errors Error handling
 *
 * Most libpg2sdr API functions return an integer, with 0 indicating
 * success and negative values representing errors. A few APIs can
 * also return positive values on success.
 *
 * Negative error codes are chosen from the ::pg2sdr_error
 * enumeration.  Users of the library API can directly match on these
 * values to handle specific errors differently, or use
 * pg2sdr_strerror() and pg2sdr_strerror_r() to format the errors for
 * human consumption.
 *
 * When libpg2sdr encounters an error from a system call or libusb,
 * that error is remapped into a value within the
 * ::PG2SDR_ERROR_SYSTEM_MIN .. ::PG2SDR_ERROR_SYSTEM_MAX or
 * ::PG2SDR_ERROR_LIBUSB_MIN .. ::PG2SDR_ERROR_LIBUSB_MAX ranges,
 * preserving the underlying error. pg2sdr_strerror() knows how to
 * interpret values in this range by calling strerror() or
 * libusb_strerror() as needed.
 */

/**
 * \brief Enumeration of possible negative error codes.
 * \ingroup errors
 *
 * libpg2sdr API functions return an integer error code, where values
 * >= 0 indicate success and values <0 indicate errors. This
 * enumeration describes those error codes.
 */
enum pg2sdr_error {
    PG2SDR_SUCCESS = 0,                    /**< no error */

    PG2SDR_ERROR_NOT_FOUND = -1,           /**< \ref pg2sdr_open_single_device found no matching devices */
    PG2SDR_ERROR_DISCONNECTED = -2,        /**< Device unexpectedly disconnected */
    PG2SDR_ERROR_BAD_ARGUMENT = -3,        /**< Bad argument to API call */
    PG2SDR_ERROR_NO_MEMORY = -4,           /**< Memory allocation failed */
    PG2SDR_ERROR_NOT_IMPLEMENTED = -5,     /**< Operation not implemented */
    PG2SDR_ERROR_FIRMWARE_MISMATCH = -6,   /**< Host/firmware version mismatch */
    PG2SDR_ERROR_MULTIPLE_DEVICES = -7,    /**< \ref pg2sdr_open_single_device found more than one matching device */
    PG2SDR_ERROR_BUSY = -8,                /**< Device already in use */
    PG2SDR_ERROR_BAD_STATE = -9,           /**< Operation not possible in this state */
    PG2SDR_ERROR_TIMEOUT = -10,            /**< Operation timed out */
    PG2SDR_ERROR_CORRUPTION = -11,         /**< Heap corruption, double-free, or use-after-free detected */
    PG2SDR_ERROR_ACCESS = -12,             /**< Insufficient permissions to access device */

    PG2SDR_ERROR_TRANSFER_OTHER = -200,    /**< Unexpected libusb transfer status */
    PG2SDR_ERROR_TRANSFER_STALL = -201,    /**< Bulk endpoint stalled (libusb transfer status LIBUSB_TRANSFER_STALL) */
    PG2SDR_ERROR_TRANSFER_OVERFLOW = -202, /**< Received unexpected data on bulk endpoint (libusb transfer status LIBUSB_TRANSFER_OVERFLOW) */
    PG2SDR_ERROR_TRANSFER_FORMAT = -203,   /**< Received malformed data on bulk endpoint */

    PG2SDR_ERROR_TUNER_DETECT = -300,      /**< Tuner not present on I2C bus */
    PG2SDR_ERROR_TUNER_PLL_LOCK = -301,    /**< Tuner LO PLL did not lock */
    PG2SDR_ERROR_TUNER_PLL_RANGE = -302,   /**< Required tuner LO PLL frequency out of range */
    PG2SDR_ERROR_TUNER_I2C = -303,         /**< Tuner I2C bus communication error */
    PG2SDR_ERROR_ADC_RATE_RANGE = -304,    /**< Required ADC sample rate out of range for ADC hardware */

    PG2SDR_ERROR_SYSTEM_MAX = -1000,       /**< Upper end of remapped errno range */
    PG2SDR_ERROR_SYSTEM_MIN = -1999,       /**< Lower end of remapped errno range */

    PG2SDR_ERROR_LIBUSB_MAX = -2000,      /**< Upper end of remapped libusb error range */
    PG2SDR_ERROR_LIBUSB_MIN = -2999,      /**< Lower end of remapped libusb error range */
};

/**
 * \brief Format a pg2sdr error code as a human-readable error message.
 * \ingroup errors
 *
 * This function is not threadsafe; the returned message may be a pointer
 * to a static buffer area that is reused on each call. For a threadsafe
 * version, use pg2sdr_strerror_r().
 *
 * \param[in] error an error code returned by the PG2SDR API
 * \return an ASCIIZ error message, that may point to shared buffer space
 */
const char *pg2sdr_strerror(int error);

/**
 * \brief Format a pg2sdr error code as a human-readable error message.
 * \ingroup errors
 *
 * This is the threadsafe variant of pg2sdr_strerror(), using a user-
 * provided buffer if needed.
 *
 * Note that in cases where the error message is fixed, \p buf is not
 * used and a pointer to the fixed error message is returned directly.
 *
 * \param[in] error an error code returned by the PG2SDR API
 *
 * \param[in] buf a buffer to use if a non-static error message needs
 *   to be generated
 *
 * \param[in] buflen the size of \p buf
 *
 * \return an ASCIIZ error message, that may point within \p buf
 */
const char *pg2sdr_strerror_r(int error, char *buf, size_t buflen);

/**
 * \defgroup device Device discovery and management
 *
 * description goes here
 */
 
/**
 * \brief Opaque type representing the state of an opened PG2SDR device.
 * \ingroup device
 *
 * Allocation of these instances is managed by the library, users
 * of the library deal only in terms of pointers to this type.
 *
 * \sa pg2sdr_open_device()
 * \sa pg2sdr_open_single_device()
 * \sa pg2sdr_close_device()
 */
typedef struct pg2sdr__device pg2sdr_device;

/**
 * \brief Enum controlling how ADC data is converted.
 * \ingroup config
 *
 * \sa pg2sdr_get_conversion_mode()
 * \sa pg2sdr_set_conversion_mode()
 */
typedef enum {
    PG2SDR_MODE_LOWIF_REAL, /**< Provide real-valued samples at low intermediate frequency (unconverted ADC output) */
    PG2SDR_MODE_BASEBAND    /**< Provide complex-valued samples at baseband */
} pg2sdr_conversion_mode_t;

/**
 * \brief Representation of an unopened PG2SDR device on the USB bus.
 * \ingroup device
 *
 * This is allocated during device discovery by
 * pg2sdr_discover_devices(), and freed by pg2sdr_free_device_list().
 *
 * The contents of pg2sdr_usb_device should not be directly modified.
 * pg2sdr_usb_device::lu_device may be used as needed to discover additional information
 * about the device if needed.
 */
typedef struct {
    const char *serial;        /**< Serial number of this device, ASCIIZ */
    const char *ports;         /**< Physical USB bus/port path for this device, ASCIIZ */
    libusb_device *lu_device;  /**< Underlying libusb_device for this device. */
} pg2sdr_usb_device;

/**
 * \brief Sample buffer passed to ::pg2sdr_stream_callback
 * \ingroup streaming
 *
 * Contains metadata and actual received samples to be processed by
 * the library user.
 *
 */
typedef struct {
    /**
     * \brief Handle of the device that produced this buffer.
     */
    pg2sdr_device *dev;

    /** \brief Sample data.
     *
     * In ::PG2SDR_MODE_LOWIF_REAL mode,
     * each sample is a single 16-bit value.
     *
     * In ::PG2SDR_MODE_BASEBAND mode, each
     * sample is two 16-bit values representing the I and Q channels
     * respectively.
     */
    int16_t *samples;

    /** \brief Number of samples available in #samples.
     *
     * Note that in BASEBAND mode, there will be count*2 individual
     * int16_t values in total , as each sample consists of two
     * int16_t values.
     *
     * pg2sdr_set_buffer_size() controls the maximum number of samples
     * provided per sample buffer, i.e. the maximum potential value of
     * "count".
     */
    unsigned count;

    /** \brief Sample timestamp at the start of this buffer.
     *
     * The sample timestamp is the cumulative number of received
     * samples, incrementing at the configured sample rate.
     *
     * This counter may not initially start at zero, and may be
     * discontinuous between buffers if an overrun causes data to be
     * dropped.
     */
    uint64_t timestamp;
} pg2sdr_sample_buffer;

/**
 * \brief Callback type that receives sample buffers
 * \ingroup streaming
 *
 * While streaming data via pg2sdr_stream_data(), this callback is
 * repeatedly called as samples are received.
 *
 * If the callback returns true (non-zero), the provided buffer is
 * immediately freed. If the callback returns false (zero), then it is
 * the responsibility of the user code to eventually call
 * pg2sdr_release_buffer() when the buffer is no longer needed. Sample
 * buffers are heap-allocated as needed, so failing to release buffers
 * will eventually exhaust available memory.
 *
 * \param[in] buffer Newly received samples to be processed. This
 *   buffer remains valid until either the callback function returns
 *   non-zero, or pg2sdr_release_buffer() is called.
 *
 * \param[in] user_data The opaque user_data value passed to
 *   pg2sdr_stream_data()
 *
 * \return non-zero to automatically release the provided buffer.
 */
typedef bool (*pg2sdr_stream_callback)(pg2sdr_sample_buffer *buffer, void *user_data);

/* Device discovery and open/close (device.c) */

/**
 * \brief Enumerate available pg2sdr devices.
 * \ingroup device
 *
 * Enumerates available devices and creates an array of pg2sdr_usb_device *,
 * one per discovered device.
 *
 * Optionally, a serial prefix and/or port path can be provided to
 * limit the search to only devices matching that prefix or path.
 *
 * Caller should eventually call pg2sdr_free_device_list(*usb_device_list)
 * to free storage associated with the device list.
 *
 * \param[in] ctx A library context.
 * \param[in] match_serial_prefix if not NULL, an ASCIIZ serial number
 *   prefix to match devices against
 * \param[in] match_ports if not NULL, an ASCIIZ port path to match
 *   devices against
 * \param[out] usb_device_list Storage for a pointer to an array of
 *   pg2sdr_usb_device
 *
 * \return the number of discovered devices in the returned list, or a
 *   negative error code on failure
 */
ssize_t pg2sdr_discover_devices(pg2sdr_context *ctx,
                                const char *match_serial_prefix,
                                const char *match_ports,
                                pg2sdr_usb_device ***usb_device_list);

/**
 * \brief Free a device list previously allocated by pg2sdr_discover_devices.
 * \ingroup device
 *
 * After a device list is freed, the individual ::pg2sdr_usb_device
 * instances in the list should not be used.
 *
 * \param[in] usb_device_list The device list to free
 * \return ::PG2SDR_SUCCESS on success, negative error code on failure
 */
void pg2sdr_free_device_list(pg2sdr_usb_device **usb_device_list);

/**
 * \brief Open a device previously discovered by pg2sdr_discover_devices.
 * \ingroup device
 *
 * \param[in] ctx The library context that allocated the device
 * \param[in] usb_device The USB device to open
 * \param[out] device Storage for the newly opened device instance
 * \return ::PG2SDR_SUCCESS on success, negative error code on failure
 */
int pg2sdr_open_device(pg2sdr_context *ctx, pg2sdr_usb_device *usb_device, pg2sdr_device **device);

/**
 * \brief Open a device directly from a libusb device.
 * \ingroup device
 *
 * No special checks are done to see if the device really is a PG2SDR,
 * it's assumed to be a PG2SDR and used directly.
 *
 * \param[in] ctx The library context that allocated the device
 * \param[in] lu_device The libusb device to open
 * \param[out] device Storage for the newly opened device instance
 * \return ::PG2SDR_SUCCESS on success, negative error code on failure
 */
int pg2sdr_open_libusb_device(pg2sdr_context *ctx, libusb_device *lu_device, pg2sdr_device **device);

/**
 * \brief Open a single device by serial number or port path.
 * \ingroup device
 *
 * Search for a single connected pg2sdr device and open it. This is
 * the preferred way for simple single-device library users to open a
 * device.
 *
 * Optionally, a serial prefix and/or port path can be provided to
 * limit the search to only devices matching that prefix or path.
 *
 * The search must be unambiguous -- exactly one device should match
 * the given criteria.  In most cases, there will only be a single
 * pg2sdr device connected, so no special criteria are needed. If more
 * than one device is connected, then match_serial_prefix or match_ports
 * must be provided to select a single device.
 *
 * If no devices match, ::PG2SDR_ERROR_NOT_FOUND is
 * returned.  If more than one device matches,
 * ::PG2SDR_ERROR_MULTIPLE_DEVICES is returned.
 *
 * \param[in] ctx The library context to use to open the device
 * \param[in] serial_prefix if not NULL, an ASCIIZ serial number
 *   prefix to match devices against
 * \param[in] ports if not NULL, an ASCIIZ port path to match devices against
 * \param[out] device Storage for the newly opened device instance
 * \return ::PG2SDR_SUCCESS on success, negative error code on failure
 */
int pg2sdr_open_single_device(pg2sdr_context *ctx,
                              const char *serial_prefix,
                              const char *ports,
                              pg2sdr_device **device);

/**
 * \brief Close a pg2sdr device.
 * \ingroup device
 *
 * If the device is currently streaming data,
 * ::PG2SDR_ERROR_BUSY will be returned. To avoid this,
 * call pg2sdr_stop_streaming() and wait for pg2sdr_stream_data() to
 * return before calling pg2sdr_close_device().
 *
 * The device handle should not be used after being closed.
 *
 * \param[in] dev the device to close
 * \return ::PG2SDR_SUCCESS on success, negative error code on failure
 */
int pg2sdr_close_device(pg2sdr_device *dev);

/**
 * \brief Get the serial number of an opened device.
 * \ingroup device
 *
 * The returned string is a shared value specific to the device instance and
 * should not be modified or freed, or used after the device is closed.
 *
 * \param[in] dev the device to query
 *
 * \return pointer to an ASCIIZ string, or NULL if the given device is
 * not valid.
 */
const char *pg2sdr_get_serial(pg2sdr_device *dev);

/**
 * \brief Get the USB port path of an opened device.
 * \ingroup device
 *
 * The USB port path represents the physical port a device is
 * connected to, in the same format used by ::pg2sdr_usb_device::ports
 * and the "port" parameter of pg2sdr_open_single_device().
 *
 * The returned string is a shared value specific to the device
 * instance and should not be modified or freed, or used after the
 * device is closed.
 *
 * \param[in] dev the device to query
 *
 * \return pointer to an ASCIIZ string, or NULL if the given device is
 * not valid.
 */
const char *pg2sdr_get_ports(pg2sdr_device *dev);

/* Device configuration (config.c) */

/*
 * Conversion mode, sample rate, center frequency, sideband, bandpass,
 * decimation mode: changes made to these parameters can interact, and
 * need a call to pg2sdr_apply_changes to take effect after you've
 * completed all the changes you want.
 */

/**
 * \brief Set the format of sample data.
 *
 * Sets the current conversion mode, controlling the format of data
 * returned (low-IF versus baseband).
 *
 * If \p mode is ::PG2SDR_MODE_BASEBAND,
 * then user samples are complex baseband, with two int16_t values
 * (I/Q, or real/imaginary) per sample. The resulting signal, centered
 * around 0Hz, corresponds to the RF signal centered around the
 * configured center frequency. This is the mode that most SDR clients
 * will want to use.
 *
 * If \p mode is ::PG2SDR_MODE_LOWIF_REAL,
 * then user samples are the real-valued output of the ADC, with one
 * int16_t value per sample. The resulting signal corresponds to one
 * sideband of the RF spectrum, either above or below the configured
 * frequency depending on the configured sideband mode. The configured
 * RF frequency maps to 0Hz (though there will not be anything useful
 * there, due to both the limits of the tuner bandpass filter, and LO
 * leakage). This mode is mostly for lower-level debugging of the
 * PG2SDR hardware or software itself, where direct inspection of the
 * ADC data is useful.
 *
 * May not be called while streaming; will return
 * ::PG2SDR_ERROR_BAD_STATE if this is attempted.
 *
 * Call pg2sdr_apply_changes() to complete the configuration change.
 *
 * \param[in] dev the device to configure
 * \param[in] mode the new conversion mode to set
 * \return ::PG2SDR_SUCCESS on success, negative error code on failure
 */
int pg2sdr_set_conversion_mode(pg2sdr_device *dev, pg2sdr_conversion_mode_t mode);

/* Get the current conversion mode and store it in *mode. */
int pg2sdr_get_conversion_mode(pg2sdr_device *dev, pg2sdr_conversion_mode_t *mode);

/* Set the user buffer size to "buffer_size" samples. This controls the maximum number of samples contained
 * in each pg2sdr_sample_buffer passed to the user callback while pg2sdr_stream_data is running.
 *
 * May not be called while streaming; will return PG2SDR_ERROR_BAD_STATE if this is attempted.
 */
int pg2sdr_set_buffer_size(pg2sdr_device *dev, size_t buffer_size);

/* Get the current user buffer size and store it in *buffer_size */
int pg2sdr_get_buffer_size(pg2sdr_device *dev, size_t *buffer_size);

/* Set the current requested sample rate to "rate".
 *
 * May be called at any time; does not affect the sample rate of any currently active stream.
 * Call pg2sdr_apply_changes to complete the configuration change.
 */
int pg2sdr_set_sample_rate(pg2sdr_device *dev, double rate);

/* Get current sample rate configuration.
 *
 * The currently requested sample rate is stored in *requested (if not NULL)
 *
 * The actual configured sample rate is stored in *actual (if not NULL), or 0 if sample
 * rate configuration is still waiting to be applied. The actual rate reflects the effective
 * rate that the hardware is configured for, and may differ slightly from the requested rate
 * due to limitations of the hardware.
 */
int pg2sdr_get_sample_rate(pg2sdr_device *dev, double *requested, double *actual);

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
 *   1..PG2SDR_DECIMATION_MAX: scale the ADC sample rate by 2**N. Decimate received data by 2**N.
 *
 *   PG2SDR_DECIMATION_AUTO: use power-of-two ADC scaling and decimation to move the
 *    intermediate frequency range used by the received signal away from 0Hz. This avoids
 *    problems with the effective bandwidth being limited by tuner filtering near 0Hz,
 *    an issue that mostly affects lower sample rates. Won't increase the ADC frequency past
 *    20MHz. This is the default setting.
 *
 *   PG2SDR_DECIMATION_AUTO_MAX: use the largest possible power-of-two ADC scaling
 *    and decimation. Won't increase the ADC frequency past 20MHz.
 *
 * May be called at any time; does not affect the configuration of any currently active stream.
 * Call pg2sdr_apply_changes to complete the configuration change.
 */
#define PG2SDR_DECIMATION_MAX (8)
#define PG2SDR_DECIMATION_AUTO (-1)
#define PG2SDR_DECIMATION_AUTO_MAX (-2)
int pg2sdr_set_decimation_mode(pg2sdr_device *dev, int decimation_mode);

/* Get the currently requested decimation mode and store it in *decimation_mode */
int pg2sdr_get_decimation_mode(pg2sdr_device *dev, int *decimation_mode);

/* Set the current undersampling mode.
 *
 * The default mode (1) corresponds to the normal case where the ADC samples at
 * some rate Fs, and we arrange for the IF signal to be contained between 0 .. Fs/2
 *
 * For values N>1, the ADC will sample at Fs but the IF signal will be contained
 * between (N-1)*Fs/2 and N*Fs/2. For example, N=2 places the IF signal between
 * Fs/2 and Fs.
 *
 * N>1 causes undersampling or bandpass sampling to happen - the IF signal is above
 * the Nyquist frequency for the ADC's sample rate. One of the spectral replicas
 * of the IF signal will lie between 0..Fs/2, and it is this replica/alias that is
 * captured by the ADC.
 *
 * Undersampling can help in cases where there's no suitable tuner bandpass filter
 * available for f < Fs/2. There are tradeoffs, notably increased noise.
 *
 * May be called at any time; does not affect the configuration of any currently active stream.
 * Call pg2sdr_apply_changes to complete the configuration change.
 */
int pg2sdr_set_undersampling_mode(pg2sdr_device *dev, int undersampling_mode);

/* Get the current undersampling mode, and place in *undersampling_mode */
int pg2sdr_get_undersampling_mode(pg2sdr_device *dev, int *undersampling_mode);

/* Set the ADC sampling rate limit, in Hz. libpg2sdr will not set the ADC to a rate higher
 * than this limit.
 *
 * Limiting the ADC rate implicitly limits the available sampling rates. In LOWIF_REAL mode,
 * the sampling rate is limited to the ADC rate. In BASEBAND mode, the complex sampling rate
 * is limited to one-half of the ADC rate.
 *
 * The ADC sampling rate defaults to 28MHz. Higher limits can be set, but they are not very
 * useful as the limiting factor becomes the speed of the USB bus.
 *
 * Setting a lower limit can be useful to limit the USB bandwidth used by a single device,
 * if it is sharing a USB bus with other devices. Setting a lower limit also reduces the
 * maximum CPU work required on the host.
 *
 * May be called at any time; does not affect the configuration of any currently active stream.
 * Call pg2sdr_apply_changes to complete the configuration change.
 */
int pg2sdr_set_adc_limit(pg2sdr_device *dev, double adc_limit);

/* Get the current ADC limit, and place in *adc_limit */
int pg2sdr_get_adc_limit(pg2sdr_device *dev, double *adc_limit);

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
 * Call pg2sdr_apply_changes to complete the configuration change.
 */
int pg2sdr_set_sideband(pg2sdr_device *dev, bool upper_sideband);

/* Get the currently requested sideband tuning mode and store it in *upper_sideband */
int pg2sdr_get_sideband(pg2sdr_device *dev, bool *upper_sideband);

/* Set the center frequency for received data. This is the RF frequency that will be
 * downconverted to 0Hz in samples provided to user callbacks, in both baseband and low-IF modes.
 *
 * May be called at any time; does not affect the configuration of any currently active stream.
 * Call pg2sdr_apply_changes to complete the configuration change.
 */
int pg2sdr_set_frequency(pg2sdr_device *dev, double frequency);

/* Get current center frequency configuration.
 *
 * The currently requested center frequency is stored in *requested (if not NULL)
 *
 * The actual configured center frequency is stored in *actual (if not NULL), or 0 if center
 * frequency configuration is still waiting to be applied. The actual frequency reflects the
 * effective frequency that the hardware is configured for, and may differ slightly from the
 * requested frequency due to limitations of the hardware.
 */
int pg2sdr_get_frequency(pg2sdr_device *dev, double *requested, double *actual);

/* Set bandpass filter limits.
 *
 * This controls the cutoffs of the analog tuner bandpass filter that shapes the incoming signal
 * before the ADC. The low and high limits are relative to the RF frequency set by
 * pg2sdr_set_frequency. Signals between the limits are retained, other signals are attenuated.
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
 * Call pg2sdr_apply_changes to complete the configuration change.
 */
int pg2sdr_set_bandpass(pg2sdr_device *dev, double low, double high);

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
int pg2sdr_get_bandpass(pg2sdr_device *dev, double *req_low, double *req_high, double *actual_low, double *actual_high);

/* Attempt to apply any outstanding configuration changes to conversion mode, sampling rate,
 * decimation mode, center frequency, sideband mode, and bandpass limits.
 *
 * This function should be called after all the corresponding pg2sdr_set_... functions for a
 * batch of changes have been completed. These settings interact, so it is possible that
 * the hardware can support the final configuration successfully but not every intermediate
 * configuration. Batching the changes together with a single "apply" call allows you to
 * jump to the final configuration in one step.
 *
 * Not all configuration changes can be applied while streaming is active. Either stop
 * streaming before calling pg2sdr_apply_changes, or interpret returned PG2SDR_ERROR_BAD_STATE
 * errors as "some changes could not be applied because streaming is active". If BAD_STATE is
 * returned, the device remains in a consistent state, and any unapplied changes remain
 * pending and can be applied later by a futher call to pg2sdr_apply_changes.
 *
 * It is safe to call pg2sdr_apply_changes if no changes are pending, doing so is a no-op.
 *
 * Calls to pg2sdr_stream_data implicit call pg2sdr_apply_changes before starting streaming.
 */
int pg2sdr_apply_changes(pg2sdr_device *dev);

/* Per-gain-stage gain configuration, in gain steps */
int pg2sdr_set_lna_gain(pg2sdr_device *dev, unsigned gain);
int pg2sdr_set_mix_gain(pg2sdr_device *dev, unsigned gain);
int pg2sdr_set_vga_gain(pg2sdr_device *dev, unsigned gain);
int pg2sdr_get_stage_gains(pg2sdr_device *dev, unsigned *lna, unsigned *mix, unsigned *vga);

/* Per-gain-stage gain configuration, in dB */
int pg2sdr_set_lna_gain_db(pg2sdr_device *dev, double gain_db);
int pg2sdr_set_mix_gain_db(pg2sdr_device *dev, double gain_db);
int pg2sdr_set_vga_gain_db(pg2sdr_device *dev, double gain_db);
int pg2sdr_get_stage_gains_db(pg2sdr_device *dev, double *lna_db, double *mix_db, double *vga_db);

/* Total gain configuration, in dB (uses all gain stages) */
int pg2sdr_set_total_gain_db(pg2sdr_device *dev, double gain_db);
int pg2sdr_get_total_gain_db(pg2sdr_device *dev, double *gain_db);

/* Gain table access */
typedef struct {
    double gain_db;         /* Total gain, dB */
    unsigned lna_gain : 4;  /* Register 5 bits 3:0 */
    unsigned mix_gain : 4;  /* Register 7 bits 3:0 */
    unsigned vga_gain : 4;  /* Register 12 bits 3:0 */
} pg2sdr_gain_table_t;
int pg2sdr_set_gain_tables(pg2sdr_device *dev,
                           const pg2sdr_gain_table_t *gain_table, size_t gain_table_size,
                           const double *lna_table,
                           const double *mix_table,
                           const double *vga_table);
int pg2sdr_get_gain_tables(pg2sdr_device *dev,
                           pg2sdr_gain_table_t **gain_table,
                           size_t *gain_table_size,
                           double *lna_table,
                           double *mix_table,
                           double *vga_table);

/* Tuner bandpass filter table access */
typedef struct {
    /* floats here to reduce the size of the (large) table */
    float lower_corner;      /* Passband lower corner frequency, Hz */
    float upper_corner;      /* Passband upper corner frequency, Hz */
    float ripple;            /* Passband ripple, dB */

    unsigned hpf_corner : 4; /* Register 11 bits 3:0 */
    unsigned lpf_narrow : 1; /* Register 11 bit 7 */
    unsigned lpf_coarse : 2; /* Register 11 bits 6:5 */
    unsigned lpf_fine : 4;   /* Register 10 bits 3:0 */
    unsigned lpf_q : 1;      /* Register 10 bit 4 */
} pg2sdr_bandpass_table_t;
int pg2sdr_set_bandpass_table(pg2sdr_device *dev,
                              const pg2sdr_bandpass_table_t *bandpass_table, size_t bandpass_table_size);
int pg2sdr_get_bandpass_table(pg2sdr_device *dev,
                              pg2sdr_bandpass_table_t **bandpass_table,
                              size_t *bandpass_table_size);

/* Streaming (stream.c) */
int pg2sdr_stream_data(pg2sdr_device *dev, pg2sdr_stream_callback callback, void *user_data, unsigned timeout_ms);
int pg2sdr_stop_streaming(pg2sdr_device *dev);
void pg2sdr_release_buffer(pg2sdr_sample_buffer *buffer);

#if defined(__cplusplus)
}
#endif


#endif /* PG2SDR_H */
