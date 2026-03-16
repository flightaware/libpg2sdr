#ifndef PG2_META_H
#define PG2_META_H

#include "firmware/pg2sdr_protocol.h"
#include "image.h"

typedef enum {
    DEVICE_OTHER,  /* non-PG2 device connected */
    DEVICE_DFU,    /* PG2 or other LPC device in DFU mode */
    DEVICE_PG2     /* PG2 with loaded firmware */
} device_type_t;

typedef struct {
    const char *port;   /* strdup'd bus-port[.port[.port ...]] */
    device_type_t device_type;
    const char *serial; /* strdup'd PG2 serial number, or NULL if unavailable */

    bool active_firmware_valid;
    firmware_metadata_t active_firmware;

    firmware_image_t *flash_image;
} port_metadata_t;

port_metadata_t *meta_query(libusb_device *dev);
void meta_free(port_metadata_t *meta);

#endif
