#ifndef PG2_META_H
#define PG2_META_H

#include "firmware/pg2sdr_protocol.h"

#include "image.h"

typedef struct {
    const char *port;   /* strdup'd bus-port[.port[.port ...]] */
    int device_type;    /* DEVTYPE_* */
    const char *serial; /* strdup'd PG2 serial number, or NULL if unavailable */

    bool active_firmware_valid;
    firmware_metadata_t active_firmware;

    bool status_valid;        /* are following fields valid? */
    bool recovery_switch_on;  /* boot switch state (true = recovery mode) */
    bool rf_power_on;         /* RF power state */
    bool adc_on;              /* ADC streaming state */
    const char *hw_type;      /* target hardware type */

    firmware_image_t *flash_image;
} port_metadata_t;

port_metadata_t *meta_query(libusb_device *dev);
void meta_free(port_metadata_t *meta);

#endif
