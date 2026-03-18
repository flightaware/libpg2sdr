#ifndef PG2_DEVICE_H
#define PG2_DEVICE_H

#include <stdbool.h>
#include <libusb-1.0/libusb.h>

#include "pg2sdr.h"

libusb_device_handle *device_open(libusb_device *dev, bool claim_interface);
void device_close(libusb_device_handle *handle);

const char *device_serial(libusb_device *dev);
const char *device_string(libusb_device *dev);
const char *device_ports(libusb_device *dev);

#define SEARCH_RECOVERY 1
#define SEARCH_PG2SDR 2
libusb_device *device_search(const char *serial_prefix, const char *port_path, unsigned flags);

extern pg2sdr_context *shared_pg2sdr_ctx;

#endif
