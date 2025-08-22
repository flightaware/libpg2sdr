#ifndef INTERNAL_H
#define INTERNAL_H

#include "lpcsdr.h"
#include "dsp.h"
#include "tuner.h"

#define MAGIC_CTX 0x18273645
#define MAGIC_DEV 0xABCD
#define MAGIC_FREE 0xFEEE
#define VID_ROM 0x1fc9
#define PID_ROM 0x000c

#define VID_LPCSDR 0xDEAD
#define PID_LPCSDR 0xBEEF
#define EXPECTED_BLOCK_HEADER_MAGIC 0xDEADBEEF

#define DFU_DOWNLOAD_REQUEST 0x1
#define DFU_GET_STATUS_REQUEST 0x3
#define USB_BLOCK_SIZE 10240

#define CHECK_CTX(ctx)                       \
    do {                                     \
        if (!ctx)                            \
            return LPCSDR_ERROR_BAD_ARGUMENT;\
        if (ctx->magic != MAGIC_CTX)         \
            return LPCSDR_ERROR_CORRUPTION;   \
    } while (0)

#define CHECK_DEV(dev)                                                            \
    do {                                                                          \
        if (!dev)                                                                 \
            return LPCSDR_ERROR_BAD_ARGUMENT;                                      \
        if (!dev->ctx || dev->magic != MAGIC_DEV || dev->ctx->magic != MAGIC_CTX) \
            return LPCSDR_ERROR_CORRUPTION;                                        \
    } while (0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define memset_elements(_dst, _val, _count) memset((_dst), (_val), (_count) * sizeof((_dst)[0]))
#define memmove_elements(_dst, _src, _count) memmove((_dst), (_src), (_count) * sizeof((_dst)[0]))
#define memcpy_elements(_dst, _src, _count) memcpy((_dst), (_src), (_count) * sizeof((_dst)[0]))

extern const uint16_t LPCSDR_FIXED_POINT_SCALE_FACTOR;
extern const uint16_t ADC_OUTPUT_VALUE_BIT_LENGTH;

struct lpcsdr_context {
    int magic;
    libusb_context *libusb_ctx;
    char *firmware_path;
    lpcsdr_log_callback log_cb;
};

struct match_tuple {
    const char *serial;
    int bus;
    int address;
    int index;
};

typedef struct lpcsdr_internal_sample_buffer {
    lpcsdr_sample_buffer public_buffer; /* sample block visible to the public API */
    bool busy;                 /* true if currently owned by application code */
    bool orphan;               /* true if this is an orphaned buffer, false if it's a reusable buffer */
    struct lpcsdr_internal_sample_buffer *next_available;
} lpcsdr_internal_sample_buffer;

typedef struct lpcsdr_transfer_state{
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

typedef struct lpcsdr_buffer_manager {
    lpcsdr_internal_sample_buffer **buffers;
    int buffer_count;
    int buffer_size;
    lpcsdr_internal_sample_buffer *available_head;
    lpcsdr_internal_sample_buffer *available_tail;
} lpcsdr_buffer_manager;

typedef struct {
    //libusb_bulk_transfer func pointer
    int (*bulk_transfer)(libusb_device_handle *dev_handle, unsigned char endpoint, 
    unsigned char *data, int length, int *actual_length, unsigned int timeout);

    //libusb_control_transfer func pointer
    int (*control_transfer)(libusb_device_handle *dev_handle,
	uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
	unsigned char *data, uint16_t wLength, unsigned int timeout);

} libusb_vtable;

struct lpcsdr_device_handle {
    unsigned magic;
    pthread_mutex_t mutex;
    lpcsdr_context *ctx;

    ep0_in_board_status_t *last_status;

    libusb_device_handle *usb_handle;
    libusb_vtable *libusb_vtable;

    bool streaming;
    bool draining;
    lpcsdr_conversion_mode conversion_mode;

    /* decimation filters */
    struct lpcsdr_decimate *decimation_filter;

    struct lpcsdr_buffer_manager *buffer_manager;

    lpcsdr_transfer_state *transfers;
    unsigned int transfer_count;
    unsigned int transfer_size;
    int completion_flag;
    lpcsdr_transfer_state *active_transfers_head; /* linked list of active transfers */
    lpcsdr_transfer_state *active_transfers_tail;

    // Tuner
    // Stores tuner changes
    change_set *tuner_change_set;
};

int lpcsdr_upload_firmware(lpcsdr_context *ctx, libusb_device_handle *handle);
int lpcsdr_handle_rom_bootloader(lpcsdr_context *ctx, libusb_device *original_dev, libusb_device **reenumerated_dev);

int lpcsdr_translate_libusb_error(int error);
int lpcsdr_translate_libusb_transfer_status(enum libusb_transfer_status status);
int lpcsdr_translate_errno(int error);

// ADC
int init_global_adc_divisor_tables();
int calculate_adc_divisor_tables(uint32_t **n_out, uint32_t **p_out, uint32_t **i_out, uint32_t ***p_i_divisors_out, uint32_t *p_i_divisors_out_length);
int calculate_adc_clock_divisors(uint32_t target_frequency, pll_divisors **divisors, bool minimize_error, bool enable_fractional, double *optional_epsilon);
int candidate_is_better(pll_divisors *current_best, pll_divisors *candidate, uint32_t min_fcco, uint32_t max_fcco, bool minimize_error, float error_threshold);
int populate_new_current_best(pll_divisors **current_best, pll_divisors *candidate);
int effective_n_divisor(uint32_t n);
int effective_p_divisor(uint32_t p);
int effective_i_divisor(uint32_t i);
int fixed_point_m(pll_divisors *divisors);
void unpack_header(uint32_t offset, uint8_t *in, ep1_header_t* out);
int unpack_raw_adc_data(lpcsdr_device_handle *handle, uint8_t *in, uint32_t in_length, int16_t *out, uint32_t skip, const char *output_file);

int build_lpc_device(lpcsdr_context *ctx, libusb_device_handle *usb_handle, lpcsdr_device_handle **out);
int get_initial_device_from_list(lpcsdr_context *ctx, libusb_device **usb_list, int device_count, libusb_device **device);
int populate_libusb_vtable(libusb_vtable **out);
void free_libusb_vtable(libusb_vtable *vtable);

int lpcsdr_realloc_buffers(lpcsdr_device_handle *dev, unsigned block_count, unsigned block_size_bytes);
void lpcsdr_free_buffers(lpcsdr_buffer_manager *bm);

//control transfers
int lpcsdr_comms_check(libusb_device_handle *usb_handle);
int lpcsdr_start_transfer(lpcsdr_device_handle *dev, uint32_t target_frequency);
int lpcsdr_stop_transfer(lpcsdr_device_handle *dev);
int lpcsdr_tuner_update(lpcsdr_device_handle *dev, uint16_t first, uint8_t *payload, uint16_t payload_size);
int lpcsdr_read_tuner_register(lpcsdr_device_handle *dev, tuner_reg_num first_reg, uint16_t cache, uint8_t *buffer, uint16_t buffer_size);

#endif /* INTERNAL_H */
