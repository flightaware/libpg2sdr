#ifndef INTERNAL_H
#define INTERNAL_H

#include "lpcsdr.h"
#include "lpcsdr_protocol.h"
#include "dsp.h"
#include "tuner.h"
#include "adc.h"

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

    lpcsdr_conversion_mode_t conversion_mode;

    double adc_limit;                     /* maximum ADC sample rate allowed, Hz */

    double requested_sample_rate;         /* user requested sample rate */
    adc_pll_config_t adc_pll_config;      /* actually configured ADC clock settings */
    bool changing_rate;                   /* do we have unapplied sample rate changes? */

    bool upper_sideband;                  /* false: tune PLL above target frequency. true: tune below */
    double requested_frequency;           /* user requested frequency */
    tuner_pll_config_t tuner_pll_config;  /* actually configured tuner LO settings */
    bool changing_freq;                   /* do we have unapplied frequency/sideband changes? */

    double requested_bandpass_low;        /* bandpass low cutoff, relative to center frequency */
    double requested_bandpass_high;       /* bandpass high cutoff, relative to center frequency */
    bool changing_bandpass;               /* do we have unapplied bandpass changes? */

    size_t buffer_size;                   /* requested user buffer size, counted in user samples */

    int decimation_mode;                  /* decimation mode: LPCSDR_DECIMATION_AUTO, or >=0 for manual */
    int undersampling_mode;               /* undersampling mode: 1 for normal case, >1 for using a particular alias */

    /* current tuner gain steps (0-15) */
    unsigned lna_gain;
    unsigned mix_gain;
    unsigned vga_gain;

    lpcsdr_gain_table_t *gain_table;      /* current total-gain table */
    size_t gain_table_size;               /* # of entries in gain_table */
    double lna_table[16];                 /* LNA gain-step-to-dB lookup */
    double mix_table[16];                 /* MIX gain-step-to-dB lookup */
    double vga_table[16];                 /* VGA gain-step-to-dB lookup */

    const lpcsdr_gain_table_t *current_gain_entry; /* if gain was last set via lpcsdr_set_total_gain_db, pointer to the entry we used, otherwise NULL */

    lpcsdr_bandpass_table_t *bandpass_table;               /* available bandpass filters */
    size_t bandpass_table_size;                            /* # of entries in bandpass_table */
    const lpcsdr_bandpass_table_t *current_bandpass_entry; /* currently selected bandpass filter (pointer into bandpass_table) */

    unsigned usb_transfer_size;           /* transfer size we decided on */
    unsigned adc_samples_per_transfer;    /* ADC samples per USB transfer */
    unsigned adc_samples_per_user_sample; /* scale factor from user sample rate to ADC sample rate */
    unsigned post_decimation;             /* number of post-downconversion decimation stages */

    /* libusb transfers array */
    lpcsdr_transfer_state *transfers;
    unsigned int transfer_count;  /* size of dev->transfers array */

    /* linked list of active transfers */
    lpcsdr_transfer_state *active_transfers_head;
    lpcsdr_transfer_state *active_transfers_tail;

    /* completion flag, passed to libusb_handle_events_*, set to true to force wakeup */
    int completion_flag;

    /* Tuner reference crystal frequency, from firmware board status */
    uint32_t tuner_xtal;

    /* State for BASEBAND-mode streaming */
    unsigned partial_samples; /* ADC samples receieved but not yet dispatched to a user sample, used for timestamp correction */
    dsp_downconvert_state_t *downconverter;  /* Fs/4 downconvertor+decimator */
    dsp_halfband_decimate_state_t *post_decimators[LPCSDR_DECIMATION_MAX]; /* Chain of decimators for extra decimation following downconversion */
    int16_t *work_buffer[2];                 /* ping-pong work buffers */
};

/* context.c */
void lpcsdr__log(lpcsdr_context *ctx, lpcsdr_log_level level, const char *format, ...) __attribute__((format(printf, 3, 4)));

/* boot.c */
int lpcsdr__boot_firmware(lpcsdr_context *ctx, libusb_device *original_dev, libusb_device **reenumerated_dev);

/* errors.c */
int lpcsdr__translate_libusb_error(int error);
int lpcsdr__translate_libusb_transfer_status(enum libusb_transfer_status status);
int lpcsdr__translate_errno(int error);

//control transfers
int lpcsdr__ctrl_get_status(lpcsdr_device_handle *dev, ep0_in_board_status_t *status);
int lpcsdr__ctrl_set_rf_power(lpcsdr_device_handle *dev, rf_power_mode_t mode);
int lpcsdr__ctrl_comms_check(libusb_device_handle *usb_handle);
int lpcsdr__ctrl_start_transfer(lpcsdr_device_handle *dev, const adc_pll_config_t *config);
int lpcsdr__ctrl_stop_transfer(lpcsdr_device_handle *dev);
int lpcsdr__ctrl_tuner_update(lpcsdr_device_handle *dev, uint16_t first, uint8_t *payload, uint16_t payload_size);
int lpcsdr__ctrl_read_tuner_register(lpcsdr_device_handle *dev, uint16_t first_reg, tuner_cache_mode_t cache_mode, uint8_t *buffer, uint16_t buffer_size);
int lpcsdr__ctrl_update_tuner_lock(lpcsdr_device_handle *dev, uint16_t vco_current, uint16_t timeout);

/* bandpass.c */
const lpcsdr_bandpass_table_t *lpcsdr__select_bandpass_filter(lpcsdr_device_handle *dev,
                                                              double low_signal,
                                                              double high_signal,
                                                              double low_nyquist,
                                                              double high_nyquist);

/* gain-tables.gen.c (generated code) */
extern const double lpcsdr__default_lna_table[16];
extern const double lpcsdr__default_mix_table[16];
extern const double lpcsdr__default_vga_table[16];
extern const size_t lpcsdr__default_gain_table_size;
extern const lpcsdr_gain_table_t lpcsdr__default_gain_table[];

/* bandpass-table.gen.c (generated code) */
extern const size_t lpcsdr__default_bandpass_table_size;
extern const lpcsdr_bandpass_table_t lpcsdr__default_bandpass_table[];

#endif /* INTERNAL_H */
