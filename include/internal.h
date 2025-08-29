#ifndef INTERNAL_H
#define INTERNAL_H

#include "lpcsdr.h"
#include "lpcsdr_protocol.h"
#include "dsp.h"
#include "tuner.h"

#define MAGIC_CTX 0x18273645
#define MAGIC_DEV 0xABCD
#define MAGIC_FREE 0xFEEE
#define VID_ROM 0x1fc9
#define PID_ROM 0x000c

#define VID_LPCSDR 0xDEAD
#define PID_LPCSDR 0xBEEF

#define CHECK_CTX(ctx)                       \
    do {                                     \
        if (!ctx)                            \
            return LPCSDR_ERROR_BAD_ARGUMENT;\
        if (ctx->magic != MAGIC_CTX)         \
            return LPCSDR_ERROR_CORRUPTION;  \
    } while (0)

#define CHECK_DEV(dev)                                                            \
    do {                                                                          \
        if (!dev)                                                                 \
            return LPCSDR_ERROR_BAD_ARGUMENT;                                     \
        if (!dev->ctx || dev->magic != MAGIC_DEV || dev->ctx->magic != MAGIC_CTX) \
            return LPCSDR_ERROR_CORRUPTION;                                       \
    } while (0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define memset_elements(_dst, _val, _count) memset((_dst), (_val), (_count) * sizeof((_dst)[0]))
#define memmove_elements(_dst, _src, _count) memmove((_dst), (_src), (_count) * sizeof((_dst)[0]))
#define memcpy_elements(_dst, _src, _count) memcpy((_dst), (_src), (_count) * sizeof((_dst)[0]))

#define LOGDEBUG(dev,fmt,...) lpcsdr__log((dev)->ctx,LPCSDR_LOG_DEBUG,(fmt) __VA_OPT__(,) __VA_ARGS__)
#define LOGINFO(dev,fmt,...) lpcsdr__log((dev)->ctx,LPCSDR_LOG_INFO,(fmt) __VA_OPT__(,) __VA_ARGS__)
#define LOGERROR(dev,fmt,...) lpcsdr__log((dev)->ctx,LPCSDR_LOG_ERROR,(fmt) __VA_OPT__(,) __VA_ARGS__)

struct lpcsdr_context {
    int magic;
    libusb_context *libusb_ctx;
    char *firmware_path;
    lpcsdr_log_callback log_cb;
};

typedef struct lpcsdr_transfer_state {
    lpcsdr_device_handle *dev; /* owning device */
    enum {
        XFER_IDLE,      /* not submitted */
        XFER_BUSY,      /* submitted and waiting for a result */
        XFER_COMPLETED, /* submitted, marked as completed by the libusb callback */
    } state;

    struct libusb_transfer *transfer;  /* the associated libusb transfer */
    void *buffer;                      /* the buffer used by the libusb transfer */
    struct lpcsdr_transfer_state *next; /* next transfer in the list */
} lpcsdr_transfer_state;


struct lpcsdr_device_handle {
    unsigned magic;
    pthread_mutex_t mutex;
    lpcsdr_context *ctx;

    uint32_t usb_samples_per_block;
    uint32_t usb_bytes_per_block;

    libusb_device_handle *usb_handle;

    bool streaming; /* true when lpcsdr_stream_data is active */
    bool draining;  /* true when we are waiting to drain all active transfers */

    lpcsdr_conversion_mode conversion_mode;
    uint32_t sample_rate;         /* user requested sample rate */
    uint32_t adc_sample_rate;     /* target ADC sample rate (only valid while streaming) */

    size_t buffer_size;           /* requested user buffer size, in bytes (todo: is this more obvious if it's in samples, not bytes?) */

    /* decimation filters */
    struct lpcsdr_decimate *decimation_filter;

    /* libusb transfers array */
    lpcsdr_transfer_state *transfers;
    unsigned int transfer_count;  /* size of dev->transfers array */
    unsigned int transfer_size;   /* size of each transfer buffer, in bytes */

    /* linked list of active transfers */
    lpcsdr_transfer_state *active_transfers_head;
    lpcsdr_transfer_state *active_transfers_tail;

    /* completion flag, passed to libusb_handle_events_*, set to true to force wakeup */
    int completion_flag;

    /* invert spectrum i.e. tune PLL above center frequency? */
    bool invert_spectrum;

    // Tuner
    uint32_t tuner_xtal;
};

/* context.c */
void lpcsdr__log(lpcsdr_context *ctx, lpcsdr_log_level level, const char *format, ...) __attribute__((format(printf, 3, 4)));

/* boot.c */
int lpcsdr__boot_firmware(lpcsdr_context *ctx, libusb_device *original_dev, libusb_device **reenumerated_dev);

/* errors.c */
int lpcsdr__translate_libusb_error(int error);
int lpcsdr__translate_libusb_transfer_status(enum libusb_transfer_status status);
int lpcsdr__translate_errno(int error);

// ADC
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

int init_global_adc_divisor_tables();
int calculate_adc_divisor_tables(uint32_t **n_out, uint32_t **p_out, uint32_t **i_out, uint32_t ***p_i_divisors_out, uint32_t *p_i_divisors_out_length);
int calculate_adc_clock_divisors(uint32_t target_frequency, pll_divisors **divisors, bool minimize_error, bool enable_fractional, double *optional_epsilon);
int candidate_is_better(pll_divisors *current_best, pll_divisors *candidate, uint32_t min_fcco, uint32_t max_fcco, bool minimize_error, float error_threshold);
int populate_new_current_best(pll_divisors **current_best, pll_divisors *candidate);
int effective_n_divisor(uint32_t n);
int effective_p_divisor(uint32_t p);
int effective_i_divisor(uint32_t i);
int fixed_point_m(pll_divisors *divisors);

//control transfers
int lpcsdr__ctrl_get_status(lpcsdr_device_handle *dev, ep0_in_board_status_t *status);
int lpcsdr__ctrl_set_rf_power(lpcsdr_device_handle *dev, rf_power_mode_t mode);
int lpcsdr__ctrl_comms_check(libusb_device_handle *usb_handle);
int lpcsdr__ctrl_start_transfer(lpcsdr_device_handle *dev, uint32_t target_frequency);
int lpcsdr__ctrl_stop_transfer(lpcsdr_device_handle *dev);
int lpcsdr__ctrl_tuner_update(lpcsdr_device_handle *dev, uint16_t first, uint8_t *payload, uint16_t payload_size);
int lpcsdr__ctrl_read_tuner_register(lpcsdr_device_handle *dev, tuner_reg_num first_reg, uint16_t cache, uint8_t *buffer, uint16_t buffer_size);
int lpcsdr__ctrl_update_tuner_lock(lpcsdr_device_handle *dev, uint16_t vco_current, uint16_t timeout);

#endif /* INTERNAL_H */
