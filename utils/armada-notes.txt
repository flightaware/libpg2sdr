# Notes for Armada

# PG2SDR boot modes

The PG2SDR can start in one of two modes, depending on the position of the
recovery switch on the device and what's stored in the on-device flash.

## Normal mode

If the recovery switch is set to normal mode (towards the antenna connector)
then, when connected, the PG2SDR will attempt to load firmware from on-device
flash storage. If this fails (e.g. corrupted or missing firmware) then it
will continue to boot in recovery mode.

After successfully booting in normal mode, the device will identify itself
on the USB bus as a PG2SDR device, and full functionality is available --
the device serial number is available, flash storage can be read and updated,
and it can do the usual SDR things.

## Recovery mode

If the recovery switch is set to recovery mode (towards the USB connector),
or if booting in normal mode fails, then the PG2SDR will enter recovery mode.
In this mode, the LPC4370 ROM bootloader will provide a DFU device on the USB
bus that can be used to download new firmware to the device and start it. This
download does _not_ affect flash storage, it is a one-off download. Once the
new firmware is started, the device behaves like it booted in normal mode.

While in DFU/recovery mode, the device serial number is not available, the
device is not positively identified as a PG2SDR, and no access to flash
storage is available so the current firmware on flash cannot be checked. To
get access to any of those, new firmware needs to be downloaded over DFU first.

# Newly manufactured devices & QA/manufacturing steps

A newly manufactured PG2SDR will have empty flash storage, so it will always
initially start in recovery mode regardless of the switch position.

As part of the QA / final manufacturing steps we'll perform, we need to
load the latest version of the firmware onto flash, and set the recovery
switch to normal mode.

# Steps that Armada needs to mediate

 * Connect a new device
 * New device starts in recovery mode and appears on USB as a DFU device
 * Download the latest firmware, temporarily, over DFU:
      `pg2-firmware load [-p port] /path/to/firmware.bin`
 * The device will re-enumerate in normal mode. Now we have flash access and
   know the serial number.
 * Write the latest firmware to flash:
      `pg2-firmware write [-p port] /path/to/firmware.bin`
 * Disconnect the device
 * Ensure the recovery switch is set to normal mode
 * Proceed with whatever other QA is needed

# CLI <-> Armada interface

`pg2-firmware` provides "load" and "write" subcommands (as above) to,
respectively, load firmware temporarily and write firmware to flash.
Armada should invoke these when needed.

These commands can accept a `-p` option to operate on a specific USB port
only. They'll otherwise operate on a single device if there's only one device,
or complain if there are multiple devices. So armada should always pass a
`-p` option.

To work out what updates each device needs, armada can monitor the state of all
connected devices using `pg2-firmware device-info --json`. This will produce
machine-readable output in json format on stdout, describing all connected
PG2SDR devices and their current state, including:

 * the physical USB port identifier the device is connected to 
 * whether they're in normal mode or recovery mode
 * if in normal mode:
   * the device serial number
   * the version of firmware they're currently running
   * the version of firmware, if any, stored on flash

`pg2-firmware image-info --json` can be used to extract version information
from a firmware image file.

## Example device-info output

```
$ ./build/pg2-firmware device-info -j | jq .
[
  {
    "port": "1-8",
    "type": "pg2sdr",
    "serial": "38265463986061DC",
    "active": {
      "version": "0.9.0.0",
      "compat": "0.9.0.0",
      "max_control_transfer": 512,
      "control_timeout_ms": 1000,
      "build_type": "debug pg2sdr"
    },
    "flash": {
      "version": "0.9.0.0",
      "compat": "0.9.0.0",
      "max_control_transfer": 512,
      "control_timeout_ms": 1000,
      "build_type": "debug pg2sdr",
      "image_size": 26144,
      "dfu_release": "0000",
      "dfu_crc": "ddbc8567"
    }
  }
]
```


# approximate armada state machine

The armada internals might look something like this:

On startup:
 * find the firmware image we want to use somehow (configuration, etc)
 * read the firmware version number using `pg2-firmware image-info`

Periodically:
 * Poll for device state by running `pg2-firmware device-info`
 * Go through each device and decide what to do with it.

For each device:
 * If it is in DFU mode, use `pg2-firmware load -p port firmware.bin` to
   temporarily load firmware. On the next poll, it should show up as a
   PG2SDR in normal mode, and we can inspect the flash state.
 * If it is in normal mode:
   * If the active/running firmware version is older than our target firmware,
     then use `pg2-firmware load` as above to update the running version.
   * If the active/running firmware version is newer than our target firmware,
     something is janky and we should stop here.
   * Otherwise, the running firmware is okay and we should proceed to look
     at the flash firmware:
     * If the flash firmware is missing or older than our target firmware,
       then use `pg2-firmware -p port write firmware.bin` to update it. On
       the next poll, we'll confirm the updated version.
     * If the flash firmware is newer than our target firmware, something is
       janky and we should stop here.
     * Otherwise, the flash firmware matches what we expect, and this device
       is up to date and ready to be disconnected.

the "ready to be disconnected" state is also where we could do things like,
record serial number/firmware version/manufacturing date to a database,
print a serial number label, tell the device to blink its LEDs, etc.

The net effect should be that you can just do this:

 * plug in one or more devices (e.g. we could have a hub with a bunch of
   ports so you can do 10 at a time, or something like that)
 * wait until armada says they're up to date
 * unplug devices, move on to more devices

Port identifiers should match up consistently with physical ports so long as
the USB bus connections aren't changed, so we could e.g. make armada have
a diagram showing the state of the device connected to each physical port.
