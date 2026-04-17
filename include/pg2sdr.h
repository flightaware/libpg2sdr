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
 * \brief Severity of log messages passed to pg2sdr_log_callback()
 * \ingroup context
 */
typedef enum {
    PG2SDR_LOG_DEBUG, /**< debug messages */
    PG2SDR_LOG_INFO,  /**< informational messages */
    PG2SDR_LOG_ERROR  /**< errors */
} pg2sdr_log_level;

/**
 * \brief Callback function passed to pg2sdr_set_log_callback()
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
 * \retval ::PG2SDR_SUCCESS success
 * \retval <0 negative error code on failure
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
 * \retval ::PG2SDR_SUCCESS success
 * \retval <0 negative error code on failure
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
 * \retval ::PG2SDR_SUCCESS success
 * \retval <0 negative error code on failure
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
 *
 * Some error codes can be returned from any function, and are
 * not individually documented in each function:
 *
 * * ::PG2SDR_ERROR_SYSTEM_MIN .. ::PG2SDR_ERROR_SYSTEM_MAX are returned
 *    if there is an error in a system/library call that sets errno which
 *    does not have a more specific PG2SDR_ERROR_* error code.
 *
 * * ::PG2SDR_ERROR_LIBUSB_MIN .. ::PG2SDR_ERROR_LIBUSB_MAX are returned
 *   if there is an error in a libusb call that does not have a more
 *   specific PG2SDR_ERROR_* error code.
 *
 * * ::PG2SDR_ERROR_NO_MEMORY is returned if memory allocation is needed,
 *   but allocation failed.
 *
 * * ::PG2SDR_ERROR_BAD_ARGUMENT is returned if a NULL device or context
 *   pointer is provided
 *
 * * ::PG2SDR_ERROR_CORRUPTION _might_ be returned if an invalid or
 *   freed device or context pointer is provided, but this is just a
 *   courtesy to try to detect bugs before heap corruption occurs -
 *   callers should not rely on this.
 *
 * * ::PG2SDR_ERROR_FIRMWARE_MISMATCH is returned if there is an
 *   unexpected protocol error in messages exchanged with the device;
 *   this generally indicates a bug or a mismatch in firmware versus
 *   library versions.
 *
 * * ::PG2SDR_ERROR_DISCONNECTED is returned if the device has disconnected
 *   from the USB bus (might be a power issue!)
 */

/**
 * \brief Enumeration of possible negative error codes.
 * \ingroup errors
 *
 * libpg2sdr API functions return an integer error code, where values
 * >= 0 indicate success and values <0 indicate errors. The
 * ::pg2sdr_error enumeration describes those error codes.
 */
enum pg2sdr_error {
    PG2SDR_SUCCESS = 0,                    /**< no error */

    PG2SDR_ERROR_NOT_FOUND = -1,           /**< pg2sdr_open_single_device() found no matching devices */
    PG2SDR_ERROR_DISCONNECTED = -2,        /**< Device unexpectedly disconnected */
    PG2SDR_ERROR_BAD_ARGUMENT = -3,        /**< Bad argument to API call */
    PG2SDR_ERROR_NO_MEMORY = -4,           /**< Memory allocation failed */
    PG2SDR_ERROR_NOT_IMPLEMENTED = -5,     /**< Operation not implemented */
    PG2SDR_ERROR_FIRMWARE_MISMATCH = -6,   /**< Host/firmware version mismatch */
    PG2SDR_ERROR_MULTIPLE_DEVICES = -7,    /**< pg2sdr_open_single_device() found more than one matching device */
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

    PG2SDR_ERROR_LIBUSB_MAX = -2000,       /**< Upper end of remapped libusb error range */
    PG2SDR_ERROR_LIBUSB_MIN = -2999,       /**< Lower end of remapped libusb error range */
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
 * \param[in] buf a buffer to use if a non-static error message needs
 *   to be generated
 * \param[in] buflen the size of \p buf
 * \return an ASCIIZ error message, that may point within \p buf
 */
const char *pg2sdr_strerror_r(int error, char *buf, size_t buflen);

/**
 * \defgroup device Device discovery and management
 *
 * This group of functions allow a libpg2sdr user to enumerate
 * available PG2SDR devices, and open them for streaming.
 *
 * Two main datatypes are used by the library:
 *
 * pg2sdr_usb_device describes a single device available on the USB
 * bus, without actively opening the device for streaming data. Arrays
 * of pg2sdr_usb_device are created by pg2sdr_discover_devices()
 * during device discovery, and should be freed by calling
 * pg2sdr_free_device_list() when no longer required.
 *
 * pg2sdr_device is an opaque type used after device discovery has
 * completed, to represent a device that has been actively opened for
 * use, together with the associated library state. Instances of
 * pg2sdr_device are allocated by pg2sdr_open_device(),
 * pg2sdr_open_libusb_device(), or pg2sdr_open_single_device(), and
 * should be closed/freed by calling pg2sdr_close_device() when no
 * longer required.
 */
 
/**
 * \brief Opaque type representing the state of an opened PG2SDR device.
 * \ingroup device
 *
 * Allocation of these instances is managed by the library, users
 * of the library deal only in terms of pointers to this type.
 *
 * \sa pg2sdr_open_device()
 * \sa pg2sdr_open_libusb_device()
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
 * \brief Enum controlling sideband tuning selection
 * \ingroup config
 *
 * \sa pg2sdr_get_sideband()
 * \sa pg2sdr_set_sideband()
 */
typedef enum {
    PG2SDR_SIDEBAND_LOWER, /**< Place tuner LO above target frequency, and capture lower sideband */
    PG2SDR_SIDEBAND_UPPER  /**< Place tuner LO below target frequency, and capture upper sideband */
} pg2sdr_sideband_mode_t;

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
 * libpg2sdr guarantees that (to the best of the ability of the
 * hardware to detect it), within a single ::pg2sdr_sample_buffer,
 * there are no gaps in the received data. That is, if sample data is
 * dropped and there is a detectable gap in the sample stream, then
 * the library will ensure that the gap appears at a boundary between
 * buffers, not within a buffer.
 */
typedef struct {
    /** \brief Conversion mode applied to these samples.
     *
     * This determines the number of int16_t values per user sample.
     *
     * In ::PG2SDR_MODE_LOWIF_REAL mode,
     * each sample is a single 16-bit value.
     *
     * In ::PG2SDR_MODE_BASEBAND mode, each
     * sample is two 16-bit values representing the I and Q channels
     * respectively.
     */
    pg2sdr_conversion_mode_t mode;

    /** \brief Sample data.
     *
     * Each user sample is one or two int16_t values, depending on the
     * conversion mode (see #mode)
     */
    int16_t *samples;

    /** \brief Number of user samples available in #samples.
     *
     * This counts *user* samples, not int16_t values.  The number of
     * int16_t values per sample depends on the value of #mode.
     *
     * pg2sdr_set_buffer_size() controls the maximum number of user
     * samples provided per sample buffer, i.e. the maximum potential
     * value of "count".
     */
    unsigned count;

    /** \brief Sample timestamp at the start of this buffer.
     *
     * The sample timestamp is the cumulative number of received
     * user samples, incrementing at the configured sampling rate.
     *
     * This counter may not initially start at zero, and may be
     * discontinuous between buffers if an overrun causes data to be
     * dropped. To detect discontinuities, look for callbacks where
     * `current_buffer->timestamp > last_buffer->timestamp + last_buffer->count`.
     */
    uint64_t timestamp;
} pg2sdr_sample_buffer;

/**
 * \brief Callback type that receives sample buffers
 * \ingroup streaming
 *
 * While streaming data with pg2sdr_stream_data(), this callback is
 * repeatedly called as samples are received.
 *
 * If the callback returns true, the provided buffer is
 * immediately freed. If the callback returns false, then it is
 * the responsibility of the user code to eventually call
 * pg2sdr_release_buffer() when the buffer is no longer needed. Sample
 * buffers are heap-allocated as needed, so failing to release buffers
 * will eventually exhaust available memory.
 *
 * \param[in] dev the device generating this callback
 * \param[in] buffer Newly received samples to be processed. This
 *   buffer remains valid until either the callback function returns
 *   non-zero, or pg2sdr_release_buffer() is called.
 * \param[in] user_data The opaque user_data value passed to
 *   pg2sdr_stream_data()
 * \retval false caller will not free \p buffer
 * \retval true caller will automatically free \p buffer
 */
typedef bool (*pg2sdr_stream_callback)(pg2sdr_device *dev, pg2sdr_sample_buffer *buffer, void *user_data);

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
 * The caller should not modify the returned device list. Once the
 * device list is no longer required, pg2sdr_free_device_list() should
 * be called to free storage associated with the list.
 *
 * The final value in the array is a sentinel NULL pointer. The return
 * value indicates the number of devices in the array (possibly zero),
 * not including the sentinel.
 *
 * \param[in] ctx A library context.
 * \param[in] match_serial_prefix if not NULL, an ASCIIZ serial number
 *   prefix to match devices against
 * \param[in] match_ports if not NULL, an ASCIIZ port path to match
 *   devices against
 * \param[out] usb_device_list Storage for a pointer to an array of
 *   pg2sdr_usb_device
 * \retval >=0 success, return value indicates size of returned array
 * \retval <0 failure, negative error code
 *
 * \sa pg2sdr_free_device_list
 * \sa pg2sdr_open_device
 */
ssize_t pg2sdr_discover_devices(pg2sdr_context *ctx,
                                const char *match_serial_prefix,
                                const char *match_ports,
                                pg2sdr_usb_device ***usb_device_list);

/**
 * \brief Free a device list previously allocated by pg2sdr_discover_devices()
 * \ingroup device
 *
 * After a device list is freed, the individual ::pg2sdr_usb_device
 * instances in the list should not be used.
 *
 * \param[in] usb_device_list The device list to free
 */
void pg2sdr_free_device_list(pg2sdr_usb_device **usb_device_list);

/**
 * \brief Open a device previously discovered by pg2sdr_discover_devices()
 * \ingroup device
 *
 * \param[in] ctx The library context that allocated the device
 * \param[in] usb_device The USB device to open
 * \param[out] device Storage for the newly opened device instance
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BUSY device is already in use by another process
 * \retval ::PG2SDR_ERROR_ACCESS insufficient permissions to open device
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_discover_devices()
 * \sa pg2sdr_close_device()
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
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BUSY device is already in use by another process
 * \retval ::PG2SDR_ERROR_ACCESS insufficient permissions to open device
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_close_device()
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
 * \param[in] ctx The library context to use to open the device
 * \param[in] serial_prefix if not NULL, an ASCIIZ serial number
 *   prefix to match devices against
 * \param[in] ports if not NULL, an ASCIIZ port path to match devices against
 * \param[out] device Storage for the newly opened device instance
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_NOT_FOUND no matching device found
 * \retval ::PG2SDR_ERROR_MULTIPLE_DEVICES more than one matching device found
 * \retval ::PG2SDR_ERROR_BUSY device is already in use by another process
 * \retval ::PG2SDR_ERROR_ACCESS insufficient permissions to open device
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_close_device()
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
 * ::PG2SDR_ERROR_BAD_STATE will be returned. To avoid this,
 * call pg2sdr_stop_streaming() and wait for pg2sdr_stream_data() to
 * return before calling pg2sdr_close_device().
 *
 * The device handle should not be used after being closed.
 *
 * \param[in] dev the device to close
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_STATE device is currently streaming data, cannot close
 * \retval <0 negative error code on failure
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
 * \return pointer to an ASCIIZ string, or NULL if the given device is not valid.
 */
const char *pg2sdr_get_ports(pg2sdr_device *dev);

/**
 * \defgroup config Device configuration
 *
 * This group of functions control the capture configuration of an
 * opened device: tuned frequency and bandwidth, sample rate and
 * format, gain settings, etc.
 *
 * Most changes to configuration settings require a subsequent call to
 * pg2sdr_apply_changes() before taking effect. pg2sdr_apply_changes()
 * is implicitly called by pg2sdr_stream_data(), so for simple cases
 * where the configuration does not change at runtime, it's sufficient
 * to just configure the device in advance of calling
 * pg2sdr_stream_data().
 *
 * In cases where configuration settings interact, it is possible that
 * out-of-range combinations of configuration settings are discovered
 * only when pg2sdr_apply_changes() is called. It is also possible
 * that, when several settings need to be changed, the intermediate
 * configurations after each individual setting change are
 * out-of-range, even if the final combination of all changes is
 * valid. If you have several settings to change, call
 * pg2sdr_apply_changes() once after all changes have been made, to
 * avoid trying to apply an intermediate configuration that may
 * be out-of-range.
 *
 * Some configuration settings cannot be changed while streaming is
 * active. In these cases, ::PG2SDR_ERROR_BAD_STATE is returned, and
 * changes are not applied.
 */

/**
 * \brief Set the conversion mode for sample data received by user callbacks
 * \ingroup config
 *
 * Sets the current conversion mode, controlling how raw ADC data is
 * converted into samples passed to the user callback (user samples).
 *
 * If \p mode is ::PG2SDR_MODE_BASEBAND, then ADC data is converted to
 * a complex baseband representation. A user sample is two int16_t
 * values (I/Q, or real/imaginary).  The complex baseband signal,
 * centered around 0Hz, corresponds to the RF signal centered around
 * the configured center frequency. This is the default mode, and the
 * mode that most SDR clients will want to use.
 *
 * If \p mode is ::PG2SDR_MODE_LOWIF_REAL, then ADC data from the
 * hardware is scaled to 16 bits, but is otherwise unmodified. Each
 * user sample is a single int16_t value, with the full signed 16-bit
 * range used. These samples are a digitization of the low-IF signal
 * produced by the tuner, corresponding to one sideband of the RF
 * input, either above or below the configured frequency depending on
 * the configured sideband mode. The configured RF frequency maps to
 * an IF frequency of 0Hz (though there will not be a useful signal at
 * 0Hz, due to both the limits of the tuner bandpass filter, and LO
 * leakage). This mode is mostly for lower-level debugging of the
 * PG2SDR hardware or software itself, where direct inspection of the
 * ADC data is useful.
 *
 * The conversion mode may not be changed while streaming is active.
 *
 * Call pg2sdr_apply_changes() to complete a change in conversion mode.
 *
 * \param[in] dev the device to configure
 * \param[in] mode the new conversion mode to set
 * \retval ::PG2SDR_SUCCESS Success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT \p mode is not a recognized conversion mode
 * \retval ::PG2SDR_ERROR_BAD_STATE cannot change conversion mode while streaming is active
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_conversion_mode()
 * \sa pg2sdr_apply_changes()
 */
int pg2sdr_set_conversion_mode(pg2sdr_device *dev, pg2sdr_conversion_mode_t mode);

/**
 * \brief Get the current sample conversion mode
 * \ingroup config
 *
 * This returns the currently requested mode, i.e. the mode
 * passed to the last successful call to
 * pg2sdr_set_conversion_mode(), even if pg2sdr_apply_changes() has
 * not yet been called.
 *
 * \param[in] dev the device to query
 * \param[out] mode a non-NULL pointer where the current format will be stored on success
 * \retval ::PG2SDR_SUCCESS success
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_conversion_mode()
 * \sa pg2sdr_apply_changes()
 */
int pg2sdr_get_conversion_mode(pg2sdr_device *dev, pg2sdr_conversion_mode_t *mode);

/**
 * \brief Set size of sample buffer passed to user callbacks.
 * \ingroup config
 *
 * Set the user buffer size to \p buffer_size user samples. The user
 * buffer size controls the maximum number of samples contained in
 * each pg2sdr_sample_buffer passed to the user callback while
 * pg2sdr_stream_data is running.
 *
 * This setting also influences the size of internally allocated USB
 * buffers. Setting large values may exceed system limits on USB
 * buffer size and return an error.
 *
 * The buffer size may not be changed while streaming is active.
 *
 * \param[in] dev the device to configure
 * \param[in] buffer_size the user buffer size, in samples
 * \retval ::PG2SDR_SUCCESS Success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT \p buffer_size is out of range
 * \retval ::PG2SDR_ERROR_BAD_STATE cannot change buffer size while streaming is active
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_buffer_size()
 */
int pg2sdr_set_buffer_size(pg2sdr_device *dev, size_t buffer_size);

/**
 * \brief Get size of sample buffer passed to user callbacks.
 * \ingroup config
 *
 * \param[in] dev the device to query
 * \param[out] buffer_size a non-NULL pointer where the current buffer size will be stored on success
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if \p buffer_size is NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_buffer_size()
 */
int pg2sdr_get_buffer_size(pg2sdr_device *dev, size_t *buffer_size);

/**
 * \brief Set the current requested user sampling rate
 * \ingroup config
 *
 * Change the requested user sampling rate. The user sampling rate is
 * the effective sampling rate seen by the user-provided callback: it
 * is the rate at which user samples are provided, \p rate user
 * samples per second. The actual ADC sampling rate may be higher than
 * the user sampling rate, with libpg2sdr providing decimation down to
 * the user sampling rate.
 *
 * As a broad sanity check, libpg2sdr will immediately reject sample rates
 * that are <1kHz or >100MHz. Sample rate support is still limited by
 * hardware support, and rates that are accepted by this function may
 * later be rejected by pg2sdr_apply_changes().
 *
 * May be called at any time; does not affect the sample rate of any
 * currently active stream.  Call pg2sdr_apply_changes() while not
 * streaming data to complete the configuration change.
 *
 * \param[in] dev the device to configure
 * \param[in] rate the new sampling rate, in Hz
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT \p rate is out of range
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_sample_rate()
 */
int pg2sdr_set_sample_rate(pg2sdr_device *dev, double rate);

/**
 * \brief Get current user sampling rate
 * \ingroup config
 *
 * Gets the currently requested user sampling rate, and/or the actual
 * effective sampling rate in use.
 *
 * The requested user sampling rate is the rate set by the last
 * call to pg2sdr_set_sample_rate().
 *
 * The actual effective sampling rate is the effective sampling rate
 * in use by the hardware, and may vary slightly from the requested
 * rate due to hardware limitations (not all sampling rates can be
 * exactly supported).
 *
 * If the requested sampling rate has been changed by calling
 * pg2sdr_set_sample_rate(), but pg2sdr_apply_changes() has not yet
 * been called, then the actual sampling rate has not yet been
 * determined, and will return 0.
 *
 * \param[in] dev the device to query
 * \param[out] requested if not NULL, location to store the requested user sampling rate
 * \param[out] actual if not NULL, location to store the actual user sampling rate
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if both \p requested and \p actual are NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_sample_rate()
 */
int pg2sdr_get_sample_rate(pg2sdr_device *dev, double *requested, double *actual);

/**
 * \brief Decimation mode that auto-selects decimation to avoid bandpass filter issues
 * \ingroup config
 *
 * This decimation mode adds decimation steps, if needed, to avoid the
 * received signal falling into the high-pass filter region of the
 * tuner bandpass filter. This avoids problems with the effective
 * bandwidth being limited by the available tuner bandpass settings
 * near 0Hz, an issue that mostly affects lower sample rates. This is
 * the default decimation mode.
 *
 * \sa pg2sdr_set_decimation_mode()
 */
#define PG2SDR_DECIMATION_AUTO (-1)

/**
 * \brief Set current decimation mode
 * \ingroup config
 *
 * Configures the decimation mode, which controls additional,
 * transparent, ADC sampling rate scaling and decimation performed in
 * the receive path. In all cases, the user callback still receives
 * data at the requested user sampling rate.
 *
 * Extra decimation can provide lower noise or better bandpass
 * filtering characteristics, at the cost of needing to run the ADC at
 * a higher sample rate and transfer more data over the USB bus.
 *
 * If \p decimation_mode is zero or positive, it is interpreted as the
 * maximum number of decimate-by-2 stages to use, i.e.  the total
 * decimation factor is 2^decimation_node. The ADC sampling rate is
 * scaled correspondingly. The actual number of stages used may be
 * limited by the maximum available ADC sampling rate.
 *
 * Otherwise, \p decimation_mode may be ::PG2SDR_DECIMATION_AUTO,
 * which adds decimation stages as needed to avoid losing signal to
 * the low end of the tuner bandpass filter. This is the default
 * setting.
 *
 * Decimation settings are ignored when the conversion mode is
 * ::PG2SDR_MODE_LOWIF_REAL.
 *
 * May be called at any time; does not affect the decimation of any
 * currently active stream.  Call pg2sdr_apply_changes() while not
 * streaming data to complete the configuration change.
 *
 * \param[in] dev the device to configure
 * \param[in] decimation_mode the new decimation mode to set
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT \p decimation_mode is not a valid decimation mode
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_decimation_mode()
 */
int pg2sdr_set_decimation_mode(pg2sdr_device *dev, int decimation_mode);

/**
 * \brief Get current decimation mode and/or actual number of decimation steps
 * \ingroup config
 *
 * The currently requested decimation mode is the mode given to the last call
 * to pg2sdr_set_decimation_mode().
 *
 * The actual decimation is the number of decimation stages configured
 * after a call to pg2sdr_apply_changes(). That is, for a returned
 * value of N, there are N decimate-by-2 stages configured, with a
 * total decimation factor of 2^N.
 *
 * If configuration settings have been changed but
 * pg2sdr_apply_changes() has not yet been called, then the actual
 * number of decimation steps has not yet been determined and will
 * return 0.
 *
 * \param[in] dev the device to query
 * \param[out] decimation_mode if non-NULL, pointer where the currently requested decimation mode will be stored on success
 * \param[out] actual_decimation if non-NULL, pointer where the current number of decimation steps will be stored on success
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if both \p decimation_mode and \p actual_decimation are NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_decimation_mode()
 */
int pg2sdr_get_decimation_mode(pg2sdr_device *dev, int *decimation_mode, unsigned *actual_decimation);

/**
 * \brief Set the current undersampling mode.
 * \ingroup config
 *
 * Experimental, use with caution.
 *
 * libpg2sdr can be configured to run the ADC at a sampling rate that
 * is less than the Nyquist rate of the IF signal, and deliberately
 * capture parts of the signal above the Nyquist rate as aliases.
 *
 * The default mode (N=1) corresponds to the normal case where the ADC
 * sampling rate is Fs, and we arrange for the IF signal to be
 * contained between 0 .. Fs/2
 *
 * For values N>1, we arrange for the IF signal to be placed between
 * (N-1)*Fs/2 and N*Fs/2. For example, N=2 places the IF signal
 * between Fs/2 and Fs. This causes undersampling (a.k.a. bandpass
 * sampling) to happen. The IF signal is above the Nyquist frequency
 * for the ADC's sampling rate. One of the spectral replicas of the IF
 * signal will lie between 0..Fs/2, and it is this replica/alias that
 * is captured by the ADC.
 *
 * Undersampling with N>1 can help in cases where there's no suitable
 * tuner bandpass filter available for f < Fs/2. There are tradeoffs,
 * notably increased noise.
 *
 * May be called at any time; does not affect the configuration of any
 * currently active stream.  Call pg2sdr_apply_changes() to complete
 * the configuration change.
 *
 * \param[in] dev the device to configure
 * \param[in] undersampling_mode the new undersampling mode (value of N) to set
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT \p undersampling_mode is out of range
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_undersampling_mode()
 */
int pg2sdr_set_undersampling_mode(pg2sdr_device *dev, unsigned undersampling_mode);

/**
 * \brief Get current undersampling mode
 * \ingroup config
 *
 * \param[in] dev the device to query
 * \param[out] undersampling_mode a non-NULL pointer where the current undersampling mode will be stored on success
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if \p undersampling_mode is NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_undersampling_mode()
 */
int pg2sdr_get_undersampling_mode(pg2sdr_device *dev, unsigned *undersampling_mode);

/**
 * \brief Set hardware ADC sampling rate limit
 * \ingroup config
 *
 * libpg2sdr will only select ADC sampling rates that are less than
 * the configured sampling rate limit.
 *
 * Limiting the ADC sampling rate implicitly limits the available user
 * sampling rates. In ::PG2SDR_MODE_LOWIF_REAL conversion mode, the
 * user sampling rate is limited to the ADC rate. In
 * ::PG2SDR_MODE_BASEBAND conversion mode, the user sampling rate is
 * limited to one-half of the ADC rate. If additional decimation is
 * requested (see pg2sdr_set_decimation_mode()) then this increases
 * the required ADC sampling rate for a given user sampling rate.
 *
 * The ADC limit defaults to 28MHz, which is close to the limit for
 * what most USB 2.0 host controllers can support on a single USB 2.0
 * bus. Higher limits can be set, up to the ADC hardware limit of
 * 80MHz, but anything above 28MHz is likely to start dropping data
 * due to USB bus bandwidth limits.
 *
 * Setting a lower limit can be useful to limit the USB bandwidth used
 * by a single device, if it is sharing a USB bus with other
 * devices. Setting a lower limit also reduces power consumption, and
 * reduces the amount of CPU work needed to process the received
 * samples on the host.
 *
 * May be called at any time; does not affect the configuration of any
 * currently active stream.  Call pg2sdr_apply_changes() to complete
 * the configuration change.
 *
 * \param[in] dev the device to configure
 * \param[in] adc_limit the maximum ADC sampling rate to allow, in Hz
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT \p adc_limit is out of range
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_adc_limit()
 */
int pg2sdr_set_adc_limit(pg2sdr_device *dev, double adc_limit);

/**
 * \brief Get current hardware ADC sampling rate limit
 * \ingroup config
 *
 * \param[in] dev the device to query
 * \param[out] adc_limit a non-NULL pointer where the current sampling rate limit will be stored on success
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if \p adc_limit is NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_adc_limit()
 */
int pg2sdr_get_adc_limit(pg2sdr_device *dev, double *adc_limit);

/**
 * \brief Get current ADC sampling rate
 * \ingroup config
 *
 * Gets the underlying ADC rate used for the current configuration.
 *
 * If there are configuration changes pending that affect the ADC rate,
 * and pg2sdr_apply_changes() has not yet been called, then the returned
 * rate is 0.
 *
 * \param[in] dev the device to query
 * \param[out] actual_adc_rate a non-NULL pointer where the ADC rate will be stored
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if \p actual_adc_rate is NULL
 * \retval <0 negative error code on failure
 */
int pg2sdr_get_adc_rate(pg2sdr_device *dev, double *actual_adc_rate);

/**
 * \brief Set the current sideband tuning mode.
 * \ingroup config
 *
 * The sideband tuning mode controls which of the two sidebands around
 * the tuner LO is retained by the tuner.
 *
 * When the ::PG2SDR_MODE_BASEBAND conversion mode is used, the choice
 * of sideband is largely transparent to the libpg2sdr user. Both
 * choices of sideband produce the same complex baseband signal, with
 * the requested frequency at 0Hz and increasing RF frequencies
 * appearing as increasing baseband frequencies. Using
 * ::PG2SDR_SIDEBAND_UPPER near the top of the available tuner range,
 * or using ::PG2SDR_SIDEBAND_LOWER near the bottom of the available
 * range, will slightly extend the range of tunable frequencies.
 *
 * When the ::PG2SDR_MODE_LOWIF_REAL conversion mode is used, the
 * tuned frequency directly sets the tuner LO, and the choice of
 * sideband affects the range of frequencies captured. When using
 * ::PG2SDR_SIDEBAND_LOWER, then frequencies above the tuned frequency
 * are captured and increasing RF frequencies appear as increasing
 * frequencies in captured data. When using ::PG2SDR_SIDEBAND_UPPER,
 * then frequencies below the tuned frequencies are captured, and
 * *decreasing* RF frequencies appear as increasing frequencies in
 * captured data.
 *
 * May be called at any time; does not affect the configuration of any
 * currently active stream.  Call pg2sdr_apply_changes() to complete
 * the configuration change.
 *
 * \param[in] dev the device to configure
 * \param[in] mode the new sideband mode to configure
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT \p mode is not a recognized sideband mode
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_sideband()
 */
int pg2sdr_set_sideband(pg2sdr_device *dev, pg2sdr_sideband_mode_t mode);

/**
 * \brief Get current sideband mode
 * \ingroup config
 *
 * \param[in] dev the device to query
 * \param[out] mode a non-NULL pointer where the current sideband mode will be stored on success
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if \p mode is NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_sideband()
 */
int pg2sdr_get_sideband(pg2sdr_device *dev, pg2sdr_sideband_mode_t *mode);

/**
 * \brief Set the center frequency of received data.
 * \ingroup config
 *
 * The center frequency is the RF frequency that will be downconverted
 * to 0Hz in samples provided to user callbacks, in both
 * ::PG2SDR_MODE_BASEBAND and ::PG2SDR_MODE_LOWIF_REAL conversion
 * modes.
 *
 * May be called at any time; does not affect the configuration of any
 * currently active stream. Call pg2sdr_apply_changes() to complete the
 * configuration change.
 *
 * \param[in] dev the device to configure
 * \param[in] frequency the new center frequency, in Hz
 * \retval ::PG2SDR_SUCCESS success
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_frequency()
 */
int pg2sdr_set_frequency(pg2sdr_device *dev, double frequency);

/**
 * \brief Get current center frequency
 * \ingroup config
 *
 * Gets the currently requested center frequency, and/or the actual
 * effective center frequency in use.
 *
 * The requested center frequency is the frequency set by the last call
 * to pg2sdr_set_frequency().
 *
 * The actual effective center frequency is the effective center
 * frequency in use by the hardware, and may vary slightly from the
 * requested frequency due to hardware limitations (not all
 * frequencies can be exactly tuned)
 *
 * If the requested frequency has been changed by calling
 * pg2sdr_set_frequency(), but pg2sdr_apply_changes() has not yet been
 * called, then the actual frequency has not yet been determined, and
 * will return 0.
 *
 * \param[in] dev the device to query
 * \param[out] requested if not NULL, location to store the requested center frequency
 * \param[out] actual if not NULL, location to store the actual center frequency
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if both \p requested and \p actual are NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_frequency()
 */
int pg2sdr_get_frequency(pg2sdr_device *dev, double *requested, double *actual);

/** 
 * \brief Set bandpass filter limits.
 * \ingroup config
 *
 * This controls the cutoffs of the analog tuner bandpass filter that
 * shapes the incoming signal before the ADC. The low and high limits
 * are relative to the RF frequency set by
 * pg2sdr_set_frequency(). Signals between the limits are retained,
 * other signals are attenuated.
 *
 * In ::PG2SDR_MODE_BASEBAND conversion mode, usually \p low will be
 * negative and \p high will be positive, as you want to receive a
 * signal that is on both sides of the center frequency. For a simple
 * bandwidth setting centered around the center frequency, use \p low
 * = -bandwidth/2 and \p high = +bandwidth/2.
 *
 * In ::PG2SDR_MODE_LOWIF_REAL conversion mode, either both values are
 * positive (with ::PG2SDR_SIDEBAND_UPPER) or both values are negative
 * (with ::PG2SDR_SIDEBAND_LOWER) as all of the RF signal captured is
 * on one side of the LO.
 *
 * The actual bandpass limits used may be significantly different to
 * what is requested, for two reasons:
 *
 *  * the hardware is quite limited in terms of the available filtering options;
 * 
 *  * the sampling rate chosen influences the maximum cutoff on one
 *    side of the bandpass region, as we must pick a cutoff below the
 *    ADC's Nyquist frequency to avoid aliasing in the ADC.
 *
 * May be called at any time; does not affect the configuration of any
 * currently active stream.  Call pg2sdr_apply_changes() to complete the
 * configuration change.
 *
 * \param[in] dev the device to configure
 * \param[in] low the new low-cutoff frequency, in Hz
 * \param[in] high the new low-cutoff frequency, in Hz
 * \retval ::PG2SDR_SUCCESS success
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_bandpassy()
 */
int pg2sdr_set_bandpass(pg2sdr_device *dev, double low, double high);

/**
 * \brief Get requested and actual bandpass filter settings.
 * \ingroup config
 *
 * The requested filter settings are the settings in the last call
 * to pg2sdr_set_bandpass()
 *
 * The actual filter settings are how the hardware has actually been
 * configured, and may differ (sometimes greatly) from the requested
 * filter settings, as the available bandpass filter choices are
 * limited by hardware.
 * 
 * If the requested filter has been changed by calling
 * pg2sdr_set_bandpass(), but pg2sdr_apply_changes() has not yet been
 * called, then the actual limits have not yet been determined, and
 * will return 0.
 *
 * \param[in] dev the device to query
 * \param[out] req_low if not NULL, location to store the requested low-cutoff frequency, in Hz
 * \param[out] req_high if not NULL, location to store the requested high-cutoff frequency, in Hz
 * \param[out] actual_low if not NULL, location to store the actual low-cutoff frequency, in Hz
 * \param[out] actual_high if not NULL, location to store the actual high-cutoff frequency, in Hz
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if all of \p req_low, \p req_high, \p actual_low, \p actual_high are NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_bandpass()
 */
int pg2sdr_get_bandpass(pg2sdr_device *dev, double *req_low, double *req_high, double *actual_low, double *actual_high);

/**
 * \brief Apply pending configuration changes
 * \ingroup config
 *
 * Attempts to apply any outstanding configuration changes to
 * conversion mode, sampling rate, center frequency, bandpass filter,
 * decimation mode, undersampling mode, sideband mode, ADC rate limit.
 *
 * This function should be called after all the corresponding
 * pg2sdr_set_xxx functions for a batch of changes have been
 * completed. These settings can interact, so it is possible that the
 * hardware can support the final configuration successfully but not
 * every intermediate configuration. Batching the changes together
 * with a single "apply" call allows you to jump to the final
 * configuration in one step.
 *
 * Not all configuration changes can be applied while streaming is
 * active. Either stop streaming before calling
 * pg2sdr_apply_changes(), or interpret a return value of
 * ::PG2SDR_ERROR_BAD_STATE as "some changes could not be applied
 * because streaming is active". If ::PG2SDR_ERROR_BAD_STATE is
 * returned, the device remains in a consistent state, and any
 * unapplied changes remain pending and can be applied later by a
 * further call to pg2sdr_apply_changes()
 *
 * It is safe to call pg2sdr_apply_changes() if no changes are
 * pending, doing so is a no-op.
 *
 * Calls to pg2sdr_stream_data() implicitly call
 * pg2sdr_apply_changes() before starting streaming.
 *
 * \param[in] dev the device to reconfigure
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_STATE streaming is active, and some pending changes cannot be applied while streaming
 * \retval ::PG2SDR_ERROR_ADC_RATE_RANGE requested configuration requires an out-of-range ADC sampling rate
 * \retval ::PG2SDR_ERROR_TUNER_PLL_RANGE requested configuration requires an out-of-range tuner LO frequency
 * \retval ::PG2SDR_ERROR_TUNER_PLL_LOCK tuner LO PLL failed to lock at requested frequency
 * \retval ::PG2SDR_ERROR_TUNER_I2C tuner I2C communication failed
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_conversion_mode()
 * \sa pg2sdr_set_sampling_rate()
 * \sa pg2sdr_set_frequency()
 * \sa pg2sdr_set_bandpass()
 * \sa pg2sdr_set_decimation_mode()
 * \sa pg2sdr_set_undersampling_mode()
 * \sa pg2sdr_set_sideband()
 * \sa pg2sdr_set_adc_limit()
 */
int pg2sdr_apply_changes(pg2sdr_device *dev);

/**
 * \defgroup gain Tuner gain control
 *
 * The tuner on PG2SDR hardware has three controllable gain-stages.
 * In order, starting from the RF input, these stages are LNA, MIX,
 * and VGA. Each stage has a configurable gain setting that can be set
 * to one of 16 possible gain steps.
 *
 * libpg2sdr provides three types of API for setting
 * the gain of these stages, and matching getters to retrieve gain:
 *
 * * Total gain setting across all stages, in dB:
 *   pg2sdr_set_total_gain_db(), pg2sdr_get_total_gain_db(). This uses
 *   a precomputed table to convert between total gain and combined
 *   stage gains. This is the API you probably want to use unless you
 *   have special requirements.
 *
 * * Gain setting per stage, in terms of dB: pg2sdr_set_lna_gain_db(),
 *   pg2sdr_set_mix_gain_db(), pg2sdr_set_vga_gain_db(),
 *   pg2sdr_get_stage_gains_db(). This API operates in terms of
 *   approximate gain in dB per stage, using a calibration table to
 *   convert between dB and hardware gain steps.
 *
 * * Direct control of hardware gain step settings:
 *   pg2sdr_set_lna_gain(), pg2sdr_set_mix_gain(),
 *   pg2sdr_set_vga_gain(), pg2sdr_get_stage_gains(). This is a
 *   low-level API that works in terms of hardware gain steps, which
 *   do not directly correspond to any particular amplification ratio.
 *
 * The gain APIs that deal in terms of dB rely on a set of gain
 * tables, which provide conversions between hardware gain steps
 * and values in dB. libpg2sdr provides a built-in set of tables
 * by default. The conversion tables can be inspected or
 * updated via pg2sdr_get_gain_tables() and pg2sdr_set_gain_tables().
 *
 * All of the gain APIs can be called while streaming data, and
 * take effect immediately when called.
 */

/**
 * \brief Set LNA stage gain step
 * \ingroup gain
 *
 * \param[in] dev the device to configure
 * \param[in] gain the LNA gain step to set, in the range [0..15]
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT \p gain is out of range
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_stage_gains()
 */
int pg2sdr_set_lna_gain(pg2sdr_device *dev, unsigned gain);

/**
 * \brief Set MIX stage gain step
 * \ingroup gain
 *
 * \param[in] dev the device to configure
 * \param[in] gain the MIX gain step to set, in the range [0..15]
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT \p gain is out of range
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_stage_gains()
 */
int pg2sdr_set_mix_gain(pg2sdr_device *dev, unsigned gain);

/**
 * \brief Set VGA stage gain step
 * \ingroup gain
 *
 * \param[in] dev the device to configure
 * \param[in] gain the VGA gain step to set, in the range [0..15]
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT \p gain is out of range
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_stage_gains()
 */
int pg2sdr_set_vga_gain(pg2sdr_device *dev, unsigned gain);

/**
 * \brief Get per-stage gain step settings
 * \ingroup gain
 *
 * This retrieves current hardware gain settings, in terms of the
 * low-level hardware gain step for each stage.
 *
 * \param[in] dev the device to query
 * \param[out] lna if not NULL, location to store the current LNA gain step
 * \param[out] mix if not NULL, location to store the current MIX gain step
 * \param[out] vga if not NULL, location to store the current VGA gain step
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if all of \p lna, \p mix, \p vga are NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_lna_gain()
 * \sa pg2sdr_set_mix_gain()
 * \sa pg2sdr_set_vga_gain()
 */
int pg2sdr_get_stage_gains(pg2sdr_device *dev, unsigned *lna, unsigned *mix, unsigned *vga);
  
/**
 * \brief Set LNA stage gain in dB
 * \ingroup gain
 *
 * The requested dB value will be converted to a hardware gain step
 * using the currently set gain table. The actual dB value may not
 * exactly match the requested value.
 *
 * \param[in] dev the device to configure
 * \param[in] gain_db the LNA gain to set, in dB
 * \retval ::PG2SDR_SUCCESS success
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_stage_gains_db()
 * \sa pg2sdr_set_gain_tables()
 */
int pg2sdr_set_lna_gain_db(pg2sdr_device *dev, double gain_db);

/**
 * \brief Set MIX stage gain in dB
 * \ingroup gain
 *
 * The requested dB value will be converted to a hardware gain step
 * using the currently set gain table. The actual dB value may not
 * exactly match the requested value.
 *
 * \param[in] dev the device to configure
 * \param[in] gain_db the MIX gain to set, in dB
 * \retval ::PG2SDR_SUCCESS success
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_stage_gains_db()
 * \sa pg2sdr_set_gain_tables()
 */
int pg2sdr_set_mix_gain_db(pg2sdr_device *dev, double gain_db);

/**
 * \brief Set VGA stage gain in dB
 * \ingroup gain
 *
 * The requested dB value will be converted to a hardware gain step
 * using the currently set gain table. The actual dB value may not
 * exactly match the requested value.
 *
 * \param[in] dev the device to configure
 * \param[in] gain_db the VGA gain to set, in dB
 * \retval ::PG2SDR_SUCCESS success
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_stage_gains_db()
 * \sa pg2sdr_set_gain_tables()
 */
int pg2sdr_set_vga_gain_db(pg2sdr_device *dev, double gain_db);

/**
 * \brief Get per-stage gain settings in dB
 * \ingroup gain
 *
 * This retrieves current hardware gain settings, converting
 * hardware gain step settings to a gain in dB using the
 * currently set gain table.
 *
 * \param[in] dev the device to query
 * \param[out] lna_db if not NULL, location to store the current LNA gain
 * \param[out] mix_db if not NULL, location to store the current MIX gain
 * \param[out] vga_db if not NULL, location to store the current VGA gain
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if all of \p lna_db, \p mix_db, \p vga_db are NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_lna_gain_db()
 * \sa pg2sdr_set_mix_gain_db()
 * \sa pg2sdr_set_vga_gain_db()
 * \sa pg2sdr_get_gain_tables()
 */
int pg2sdr_get_stage_gains_db(pg2sdr_device *dev, double *lna_db, double *mix_db, double *vga_db);

/**
 * \brief Set total gain, in dB
 * \ingroup gain
 *
 * The requested dB value is converted to a set of hardware gain steps
 * for each stage, using the currently set gain table.  The actual dB
 * value may not exactly match the requested value.
 *
 * \param[in] dev the device to configure
 * \param[in] gain_db the total gain to set, in dB
 * \retval ::PG2SDR_SUCCESS success
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_get_total_gain_db()
 * \sa pg2sdr_set_gain_tables()
 */
int pg2sdr_set_total_gain_db(pg2sdr_device *dev, double gain_db);

/**
 * \brief Get total gain setting, in dB
 * \ingroup gain
 *
 * This retrieves current hardware gain settings, and converts
 * them to a total gain value in dB using the
 * currently set gain table.
 *
 * If the current gain was set by calling pg2sdr_set_total_gain_db()
 * then the returned gain reflects the recorded gain setting for the
 * entry in the gain table that was used. If the current gain was set
 * by setting individual gain stages, then an estimated total gain is
 * returned by looking at the gain table for each stage individually;
 * this estimated total may not exactly match the total gain
 * measurements in the gain table.
 *
 * \param[in] dev the device to query
 * \param[out] gain_db location to store the current total gain, in dB
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if \p gain_db is NULL
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_set_total_gain_db()
 * \sa pg2sdr_get_gain_tables()
 */
int pg2sdr_get_total_gain_db(pg2sdr_device *dev, double *gain_db);

/**
 * \defgroup gaintable Gain calibration tables
 *
 * The gain control APIs rely on a set of four precalculated gain
 * tables which map gain values in dB to hardware settings.
 *
 * libpg2sdr provides built-in defaults for these tables which reflect
 * gain values measured during hardware development and should be okay
 * for most uses. Advanced users can provide their own tables by
 * calling pg2sdr_set_gain_tables().
 *
 * There are three stage-specific gain tables:
 *
 *  * LNA gain table: 16-entry array of doubles, mapping LNA gain steps to a gain in dB
 *  * MIX gain table: 16-entry array of doubles, mapping MIX gain steps to a gain in dB
 *  * VGA gain table: 16-entry array of doubles, mapping VGA gain steps to a gain in dB
 *
 * The stage-specific tables are always exactly 16 entries long, with entry [i]
 * containing the gain in dB for a hardware step of "i".
 *
 * There is also one total gain table, which maps a total gain value to an appropriate
 * combination of hardware settings for all three stages. This table is a
 * variable length array of \ref ::pg2sdr_gain_table_t structs.
 */
    
/**
 * \brief Gain table entry
 * \ingroup gaintable
 *
 * Element type for the total gain tables used in
 * pg2sdr_get_gain_tables() and pg2sdr_set_gain_tables().  Each entry
 * maps a total gain (in dB) to gain step settings for each of the
 * three gain stages.
 */
typedef struct {
    double gain_db;         /**< Total gain, dB */
    unsigned lna_gain : 4;  /**< LNA gain step, 0-15 (register 5 bits 3:0) */
    unsigned mix_gain : 4;  /**< MIX gain step, 0-15 (register 7 bits 3:0) */
    unsigned vga_gain : 4;  /**< VGA gain step, 0-15 (register 12 bits 3:0) */
} pg2sdr_gain_table_t;

/**
 * \brief Set the gain tables used for the dB-based gain API
 * \ingroup gaintable
 *
 * Update one or more of the internal gain tables. Gain tables are
 * device-specific.
 *
 * This can be called at any time, but changes to the gain table do
 * not retrospectively affect gain settings (the hardware settings are
 * not modified if the gain table is changed).
 *
 * Not all gain tables need to be updated at once, pass NULL for
 * tables to leave unchanged.
 *
 * A copy of the tables is made at the point when
 * pg2sdr_set_gain_tables() is called; ownership of provided tables
 * (and responsibility for freeing any allocated memory) stays with
 * the caller.
 *
 * \p lna_table, \p mix_table, and \p vga_table, if provided, must be
 * exactly 16 entries long.
 *
 * \p gain_table, if provided, must be exactly \p gain_table_size
 * entries long. It may be unsorted, and must have at least one entry.
 *
 * \param[in] dev the device to configure
 * \param[in] gain_table if not NULL, pointer to an array of length \p gain_table_size
 * \param[in] gain_table_size number of entries in \p gain_table
 * \param[in] lna_table if not NULL, pointer to an array of length 16
 * \param[in] mix_table if not NULL, pointer to an array of length 16
 * \param[in] vga_table if not NULL, pointer to an array of length 16
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if all of \p gain_table, \p lna_table, \p mix_table, \p vga_table are NULL, or if \p gain_table is not NULL and \p gain_table_size == 0
 * \retval <0 negative error code on failure
 */
int pg2sdr_set_gain_tables(pg2sdr_device *dev,
                           const pg2sdr_gain_table_t *gain_table, size_t gain_table_size,
                           const double *lna_table,
                           const double *mix_table,
                           const double *vga_table);

/**
 * \brief Get the current gain tables used for the dB-based gain API
 * \ingroup gaintable
 *
 * Creates and returns copies of the currently used gain tables.
 * These copies are a snapshot as at the time of the call, and will
 * not reflect future changes to the gain tables.
 *
 * If \p gain_table is not NULL, a copy of the current total-gain
 * table will be allocated and returned via \p gain_table. It is
 * the caller's responsibility to free this array via free()
 * when done. The returned gain table will be sorted by
 * increasing total_gain.
 *
 * If \p lna_table, \p mix_table, or \p vga_table are not NULL,
 * they should point to caller-managed memory with space for
 * at least 16 entries. The current per-stage gain table will
 * be copied there.
 *
 * \param[in] dev the device to query
 * \param[out] gain_table if not NULL, storage for a pointer to a copy of the total gain table
 * \param[out] gain_table_size if not NULL, stores the length of the array stored in \p gain_table
 * \param[out] lna_table if not NULL, points to a 16-entry array that will be filled with the LNA gain table
 * \param[out] mix_table if not NULL, points to a 16-entry array that will be filled with the MIX gain table
 * \param[out] vga_table if not NULL, points to a 16-entry array that will be filled with the VGA gain table
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if all of \p gain_table, \p lna_table, \p mix_table, \p vga_table are NULL, or \p gain_table is NULL but \p gain_table_size is not NULL (or vice versa)
 * \retval <0 negative error code on failure
 */
int pg2sdr_get_gain_tables(pg2sdr_device *dev,
                           pg2sdr_gain_table_t **gain_table,
                           size_t *gain_table_size,
                           double *lna_table,
                           double *mix_table,
                           double *vga_table);

/**
 * \defgroup filtertable Filter calibration tables
 *
 * The tuner hardware in a PG2SDR device has a configurable bandpass
 * filter that is applied to the mixed IF signal before reaching the
 * ADC.  libpg2sdr needs to configure this filter appropriaely for the
 * requested sampling rate and bandwidth requirements requested.
 *
 * The mapping from hardware settings to actual filter characteristics
 * is mostly undocumented and not particularly regular, so libpg2sdr
 * includes a calibration table that contains empirical measurements
 * of the filter characteristics for each combination of filter
 * settings. At runtime, this table is scanned to find filter hardware
 * settings that best match libpg2sdr's requirements.
 *
 * libpg2sdr includes a built-in calibration table that is used by
 * default, but an alternative table can be provided by used code
 * via the pg2sdr_set_bandpass_table() and pg2sdr_get_bandpass_table()
 * APIs.
 */

/**
 * \brief Tuner bandpass filter table entry
 * \ingroup filtertable
 *
 * Represents one possible combination of tuner hardware settings,
 * and a summary of the resulting filter characteristics.
 */
typedef struct {
    /* floats here to reduce the size of the (large) table */

    float lower_corner;      /**< Passband lower corner frequency, Hz */
    float upper_corner;      /**< Passband upper corner frequency, Hz */
    float ripple;            /**< Passband ripple, dB */

    unsigned hpf_corner : 4; /**< Register 11 bits 3:0 */
    unsigned lpf_narrow : 1; /**< Register 11 bit 7 */
    unsigned lpf_coarse : 2; /**< Register 11 bits 6:5 */
    unsigned lpf_fine : 4;   /**< Register 10 bits 3:0 */
    unsigned lpf_q : 1;      /**< Register 10 bit 4 */
} pg2sdr_bandpass_table_t;


/**
 * \brief Set the filter calibration tables used for bandpass filter selection
 * \ingroup filtertable
 *
 * Update the internal filter calibration table. Filter calibration
 * tables are device-specific.
 *
 * This can be called at any time, but changes to the calibration
 * table do not retrospectively affect the chosen filter settings (the
 * hardware settings are not modified when the calibration table is
 * updated).
 *
 * A copy of the table is made at the point when
 * pg2sdr_set_gain_tables() is called; ownership of provided table
 * (and responsibility for freeing any allocated memory) stays with
 * the caller.
 *
 * \p bandpass_table must be exactly \p bandpass_table_size entries
 * long, and must have at least one entry.
 *
 * \param[in] dev the device to configure
 * \param[in] bandpass_table pointer to an array of length \p bandpass_table_size entries
 * \param[in] bandpass_table_size number of entries in \p bandpass_table
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if \p bandpass_table is NULL or \p bandpass_table_size is 0
 * \retval <0 negative error code on failure
 */
int pg2sdr_set_bandpass_table(pg2sdr_device *dev,
                              const pg2sdr_bandpass_table_t *bandpass_table, size_t bandpass_table_size);

/**
 * \brief Get the current filter calibration tables used for bandpass filter selection
 * \ingroup filtertable
 *
 * Creates and returns a copy of the currently used filter calibration
 * table.  This copy is a snapshot as at the time of the call, and
 * will not reflect future changes to the calibration table.
 *
 * A copy of the current calibration table will be allocated and
 * returned via \p bandpass_table. It is the caller's responsibility
 * to free this array via free() when done.
 *
 * \param[in] dev the device to query
 * \param[out] bandpass_table storage for a pointer to a copy of the filter calibration table
 * \param[out] bandpass_table_size stores the length of the array stored in \p bandpass_table
 * \retval ::PG2SDR_SUCCESS success
 * \retval ::PG2SDR_ERROR_BAD_ARGUMENT if \p bandpass_table or \p bandpass_table_size are NULL
 * \retval <0 negative error code on failure
 */
int pg2sdr_get_bandpass_table(pg2sdr_device *dev,
                              pg2sdr_bandpass_table_t **bandpass_table,
                              size_t *bandpass_table_size);

/**
 * \defgroup streaming Receiving samples from the PG2SDR
 *
 * .. some overview description goes here ..
 */
    
/**
 * \brief Stream ADC data and pass converted samples to user callback
 * \ingroup streaming
 *
 * This function is the core of the sample capture process.  It
 * completes configuration of the PG2SDR device and then begins to
 * stream ADC samples from the device, calling the user-provided
 * callback as data becomes available.
 *
 * pg2sdr_stream_data() implicitly calls pg2sdr_apply_changes() as
 * part of configuring the device, and any possible errors returned by
 * pg2sdr_apply_changes() can also be returned by pg2sdr_stream_data()
 *
 * pg2sdr_stream_data() will block indefinitely, repeatedly receiving
 * data and calling the user callback (\p callback), until either
 * pg2sdr_stop_streaming() is called or an error occurs. Callers may
 * want to call this in a dedicated thread if they have other
 * processing that needs to happen in parallel with data capture.
 *
 * Calls to \p callback are made from the same thread that called
 * pg2sdr_stream_data(), and are made in order of increasing
 * sample timestamp. The provided callback should return promptly,
 * as data I/O is blocked while the callback is executing and
 * samples may be dropped if the callback takes too long to
 * return.
 *
 * Only one call to pg2sdr_stream_data() can be outstanding at
 * any particular time. Attempts to call it again while a call
 * is active will return ::PG2SDR_ERROR_BAD_STATE.
 *
 * Streaming of data can be halted by calling pg2sdr_stop_streaming(),
 * either from within execution of \p callback or from a separate
 * thread.  This will cause pg2sdr_stream_data() to stop streaming
 * data and return normally.
 *
 * \param[in] dev the device to stream data from
 * \param[in] callback a user callback function to call for each received block of sample data
 * \param[in] user_data an opaque value to pass through to the user callback function
 * \retval ::PG2SDR_SUCCESS normal termination of streaming
 * \retval ::PG2SDR_ERROR_BAD_STATE concurrent call to pg2sdr_stream_data() active
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_stop_streaming()
 * \sa pg2sdr_apply_changes()
 */
int pg2sdr_stream_data(pg2sdr_device *dev, pg2sdr_stream_callback callback, void *user_data);

/**
 * \brief Halt data streaming
 * \ingroup streaming
 *
 * Stops ongoing data streaming, and makes the current call to
 * pg2sdr_stream_data() return normally after it has finished
 * processing any outstanding data.
 *
 * This can be safely called from within a ::pg2sdr_stream_callback
 * callback if needed, or can be called concurrently from a different
 * thread.
 *
 * \param[in] dev the device that should stop streaming data
 * \retval ::PG2SDR_SUCCESS success, pg2sdr_stream_data() will return at some point soon
 * \retval ::PG2SDR_ERROR_BAD_STATE no concurrent call to pg2sdr_stream_data() is in progress
 * \retval <0 negative error code on failure
 *
 * \sa pg2sdr_stream_data()
 */
int pg2sdr_stop_streaming(pg2sdr_device *dev);

/**
 * \brief Free a sample buffer previously passed to a ::pg2sdr_stream_callback callback
 * \ingroup streaming
 *
 * If a ::pg2sdr_stream_callback returns 0, then the sample buffer that was passed to
 * that callback is not automatically freed and can continue to be used by user code.
 * When user code has finished using the buffer, it should be explicitly freed by
 * calling pg2sdr_release_buffer().
 *
 * \param[in] buffer the buffer to free
 */
void pg2sdr_release_buffer(pg2sdr_sample_buffer *buffer);

#if defined(__cplusplus)
}
#endif


#endif /* PG2SDR_H */
