#ifndef PG2SDR_INTERNAL_H
#define PG2SDR_INTERNAL_H

#include "pg2sdr.h"
#include "core.h"

#include "firmware/pg2sdr_protocol.h"
#include "dsp/dsp.h"

#include "tuner.h"
#include "adc.h"

#define MAGIC_DEV 0xABCD

#define CHECK_DEV(dev)                                                            \
    do {                                                                          \
        if (!dev)                                                                 \
            return PG2SDR_ERROR_BAD_ARGUMENT;                                     \
        if (!dev->ctx || dev->magic != MAGIC_DEV || dev->ctx->magic != MAGIC_CTX) \
            return PG2SDR_ERROR_CORRUPTION;                                       \
    } while (0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define LOGDEBUG(dev,fmt,...) pg2sdr__log((dev)->ctx,PG2SDR_LOG_DEBUG,(fmt) __VA_OPT__(,) __VA_ARGS__)
#define LOGINFO(dev,fmt,...) pg2sdr__log((dev)->ctx,PG2SDR_LOG_INFO,(fmt) __VA_OPT__(,) __VA_ARGS__)
#define LOGERROR(dev,fmt,...) pg2sdr__log((dev)->ctx,PG2SDR_LOG_ERROR,(fmt) __VA_OPT__(,) __VA_ARGS__)

typedef struct pg2sdr__transfer_state {
    pg2sdr_device *dev; /* owning device */
    enum {
        XFER_IDLE,      /* not submitted */
        XFER_BUSY,      /* submitted and waiting for a result */
        XFER_COMPLETED, /* submitted, marked as completed by the libusb callback */
    } state;

    struct libusb_transfer *transfer;  /* the associated libusb transfer */
    void *buffer;                      /* the buffer used by the libusb transfer */
    struct pg2sdr__transfer_state *next; /* next transfer in the list */
} pg2sdr__transfer_state;

#ifdef ENABLE_INSTRUMENTATION
#include <time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>

typedef struct {
    struct timespec last;
    uint64_t elapsed;
    uint64_t samples;
} pg2sdr__profile;

static inline void pg2sdr__profile_start(pg2sdr__profile *p, unsigned count);
static inline void pg2sdr__profile_end(pg2sdr__profile *p, unsigned count);
static inline void pg2sdr__profile_reset(pg2sdr__profile *p);
static inline void pg2sdr__profile_log(pg2sdr_device *dev, pg2sdr__profile *p, const char *fmt, ...)  __attribute__((format(printf,3,4)));
#else
# define pg2sdr__profile_start(p,count) do {} while(0)
# define pg2sdr__profile_end(p,count) do {} while(0)
# define pg2sdr__profile_reset(p) do {} while(0)
# define pg2sdr__profile_log(dev,p,fmt,...) do {} while(0)
#endif

/* maximum number of decimation stages we support */
#define PG2SDR_DECIMATION_MAX 8

struct pg2sdr__device {
    unsigned magic;
    pthread_mutex_t mutex;
    pg2sdr_context *ctx;

    char *serial;
    char *ports;

    uint32_t fw_version;
    unsigned control_timeout_ms;
    uint32_t usb_samples_per_block;
    uint32_t usb_bytes_per_block;

    libusb_device_handle *usb_handle;

    bool streaming; /* true when pg2sdr_stream_data is active */
    bool draining;  /* true when we are waiting to drain all active transfers */

    pg2sdr_conversion_mode_t conversion_mode;

    double adc_limit;                     /* maximum ADC sample rate allowed, Hz */

    double requested_sample_rate;         /* user requested sample rate */
    adc_pll_config_t adc_pll_config;      /* actually configured ADC clock settings */
    bool changing_rate;                   /* do we have unapplied sample rate changes? */

    pg2sdr_sideband_mode_t sideband_mode; /* controls placement of tuner LO relative to target frequency */
    double requested_frequency;           /* user requested frequency */
    tuner_pll_config_t tuner_pll_config;  /* actually configured tuner LO settings */
    bool changing_freq;                   /* do we have unapplied frequency/sideband changes? */

    double requested_bandpass_low;        /* bandpass low cutoff, relative to center frequency */
    double requested_bandpass_high;       /* bandpass high cutoff, relative to center frequency */
    bool changing_bandpass;               /* do we have unapplied bandpass changes? */

    size_t buffer_size;                   /* requested user buffer size, counted in user samples */

    int decimation_mode;                  /* decimation mode: PG2SDR_DECIMATION_AUTO, or >=0 for manual */
    unsigned undersampling_mode;          /* undersampling mode: 1 for normal case, >1 for using a particular alias */

    /* current tuner gain steps (0-15) */
    unsigned lna_gain;
    unsigned mix_gain;
    unsigned vga_gain;

    pg2sdr_gain_table_t *gain_table;      /* current total-gain table */
    size_t gain_table_size;               /* # of entries in gain_table */
    double lna_table[16];                 /* LNA gain-step-to-dB lookup */
    double mix_table[16];                 /* MIX gain-step-to-dB lookup */
    double vga_table[16];                 /* VGA gain-step-to-dB lookup */

    const pg2sdr_gain_table_t *current_gain_entry; /* if gain was last set via pg2sdr_set_total_gain_db, pointer to the entry we used, otherwise NULL */

    pg2sdr_bandpass_table_t *bandpass_table;               /* available bandpass filters */
    size_t bandpass_table_size;                            /* # of entries in bandpass_table */
    const pg2sdr_bandpass_table_t *current_bandpass_entry; /* currently selected bandpass filter (pointer into bandpass_table) */

    unsigned usb_transfer_size;           /* transfer size we decided on */
    unsigned adc_samples_per_transfer;    /* ADC samples per USB transfer */
    unsigned adc_samples_per_user_sample; /* scale factor from user sample rate to ADC sample rate */
    unsigned post_decimation;             /* number of post-downconversion decimation stages */

    /* libusb transfers array */
    pg2sdr__transfer_state *transfers;
    unsigned int transfer_count;  /* size of dev->transfers array */

    /* linked list of active transfers */
    pg2sdr__transfer_state *active_transfers_head;
    pg2sdr__transfer_state *active_transfers_tail;

    /* completion flag, passed to libusb_handle_events_*, set to true to force wakeup */
    int completion_flag;

    /* Tuner reference crystal frequency, from firmware board status */
    uint32_t tuner_xtal;

    /* State for BASEBAND-mode streaming */
    unsigned partial_samples; /* ADC samples receieved but not yet dispatched to a user sample, used for timestamp correction */
    dsp_downconvert_state_t *downconverter;  /* Fs/4 downconvertor+decimator */
    dsp_halfband_decimate_state_t *post_decimators[PG2SDR_DECIMATION_MAX]; /* Chain of decimators for extra decimation following downconversion */
    int16_t *work_buffer[2];                 /* ping-pong work buffers */

#ifdef ENABLE_INSTRUMENTATION
    pg2sdr__profile profile_unpack;
    pg2sdr__profile profile_downconverter;
    pg2sdr__profile profile_decimator[PG2SDR_DECIMATION_MAX];
#endif
};

/* boot.c */
int pg2sdr__boot_firmware(pg2sdr_context *ctx, libusb_device *original_dev, libusb_device **reenumerated_dev);

/* bandpass.c */
const pg2sdr_bandpass_table_t *pg2sdr__select_bandpass_filter(pg2sdr_device *dev,
                                                              double low_signal,
                                                              double high_signal,
                                                              double low_nyquist,
                                                              double high_nyquist);

/* gain-tables.gen.c (generated code) */
extern const double pg2sdr__default_lna_table[16];
extern const double pg2sdr__default_mix_table[16];
extern const double pg2sdr__default_vga_table[16];
extern const size_t pg2sdr__default_gain_table_size;
extern const pg2sdr_gain_table_t pg2sdr__default_gain_table[];

/* bandpass-table.gen.c (generated code) */
extern const size_t pg2sdr__default_bandpass_table_size;
extern const pg2sdr_bandpass_table_t pg2sdr__default_bandpass_table[];

#ifdef ENABLE_INSTRUMENTATION
/* instrumentation */
static inline void pg2sdr__profile_start(pg2sdr__profile *p, unsigned count)
{
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &p->last);
    p->samples += count;
}

static inline void pg2sdr__profile_end(pg2sdr__profile *p, unsigned count)
{
    struct timespec now;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    p->elapsed += ((uint64_t)(now.tv_sec - p->last.tv_sec) * 1000000000U + (uint64_t)now.tv_nsec - (uint64_t)p->last.tv_nsec);
    p->samples += count;
}

static inline void pg2sdr__profile_reset(pg2sdr__profile *p)
{
    p->elapsed = 0;
    p->samples = 0;
    p->last.tv_sec = 0;
    p->last.tv_nsec = 0;
}

static inline void pg2sdr__profile_log(pg2sdr_device *dev, pg2sdr__profile *p, const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    LOGDEBUG(dev, "%s: %" PRIu64 " samples in %" PRIu64 ".%09" PRIu64 " s", buf, p->samples, p->elapsed/1000000000U, p->elapsed%1000000000U);
}
#endif

#endif /* PG2SDR_INTERNAL_H */
