#ifndef PG2SDR_DEVICE_H
#define PG2SDR_DEVICE_H

/* device.c */

/* ctx may be NULL, at the cost of no error logging */
char *pg2sdr__strdup_serial(pg2sdr_context *ctx, libusb_device *usb_dev);

/* ctx may be NULL, at the cost of no error logging */
char *pg2sdr__strdup_ports(pg2sdr_context *ctx, libusb_device *usb_dev);

/* this is also used as a bitmask */
enum {
    DEVTYPE_PG2SDR = 1,
    DEVTYPE_LEGACY = 2,
    DEVTYPE_RECOVERY = 4,
    DEVTYPE_OTHER = 8,
};

int pg2sdr__identify_device(libusb_device *lu_device);
ssize_t pg2sdr__discover_matching(pg2sdr_context *ctx,
                                  const char *match_serial_prefix,
                                  const char *match_ports,
                                  int match_types,
                                  pg2sdr_usb_device ***device_list);

#endif
