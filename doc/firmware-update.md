# Updating ProStick Gen 2 firmware

The ProStick Gen 2 contains a firmware image, which is the software
that runs on the device itself and manages the hardware. Each ProStick
is shipped loaded with the current firmware image at the time of
manufacture. There may be newer firmware available that fixes bugs,
adds new features, etc.

There are two copies of the firmware involved:

1. A version stored permanently on a flash chip on the ProStick Gen 2;

2. A version currently loaded and running on the microcontroller.
   This is the "active" firmware and might be temporarily different to
   the version stored on the flash chip. It is a temporary copy that
   is reset whenever the device restarts.

Normally, when the device is reset, the active firmware is loaded from
the copy stored on the flash chip.

You can update the firmware stored on your device in a few different ways:

# Updating using the `pg2-update-firmware` script

The `pg2sdr-tools` package includes a script, `pg2-update-firmware`, which
automates the manual process described below. It will fetch the current
latest firmware release from Github, then apply it to any connected
ProStick Gen 2 devices that require updates.

To use this script, just run `pg2-update-firmware` from the command line
on the host with the ProStick connected, and follow the prompts:

```bash
pg2-update-firmware
```

```
Updating devices to firmware version 1.0.0.0, CRC 3e285548
Devices that will be upgraded:
  Port 1-11: Pro Stick Gen 2 (serial 386297DBD86461DC) (firmware v0.9.7.0, CRC f7842fc6)
Update these devices? yes
Updating device on port 1-11 to firmware version 1.0.0.0, CRC 3e285548
```

## Updating when recovery mode is enabled

To update devices using `pg2-update-firmware` using recovery mode:

1. Disconnect the device
2. Find the small recovery switch on the edge of the device. Set it so that it
   is switched towards the USB connector.
3. Reconnect the device. The LED indicators should show one solid orange
   LED, and two faint red LEDs. The device is now in recovery mode, waiting
   to load new firmware.
4. Run `pg2-update-firmware --recovery`:

```
Updating devices to firmware version 1.0.0.0, CRC 3e285548
Devices in recovery mode:
  Port 1-15: Pro Stick Gen 2 in recovery mode
Loading firmware for device on port 1-15
Rescanning USB bus for updated devices
Devices that will be upgraded:
  Port 1-4: Pro Stick Gen 2 (serial 386297DBD86461DC) (firmware v0.9.7.0, CRC f7842fc6)
Update these devices? yes
Updating device on port 1-4 to firmware version 1.0.0.0, CRC 3e285548
Not resetting device on port 1-4, recovery switch is set to recovery mode
```

5. Disconnect the device and move the recovery switch to the other position
   (switched towards the RF connector)
6. Reconnect the device.

# Manual update using `pg2-util`

To manually update the firmware without using `pg2-update-firmware`, you will need
to download an appropriate firmware image and use `pg2-util` to apply it:

## Prerequisites

You will need the ProStick Gen 2 device to update, connected to a
system with `pg2-util` installed. On Debian-like systems, `pg2-util`
is part of the `pg2sdr-tools` package. See [Building and installing
libpg2sdr](install.md) for details on how to install this.

## Find a new firmware image

New firmware images are released in the
[firmware repository on GitHub](https://github.com/flightaware/pg2sdr-firmware/releases/latest).

On the release page under `Assets`, you will find a firmware image
file named `pg2sdr-firmware-VERSION.bin`. Download this file to the
system you will be using to update the firmware. You can use wget to do this:

```bash
wget https://github.com/flightaware/pg2sdr-firmware/releases/download/v0.9.7.0/pg2sdr-firmware-0.9.7.0.bin    # replace with the appropriate link for your release
```

## Checking the current firmware version

You can check the versions of the currently running firmware, and the
firmware stored on flash, using the `pg2-util device-info` command:

```bash
pg2-util device-info
```

```
Port 1-11:
  Device type:          ProStick Gen 2
  Serial number:        386297DBD86461DC
  Recovery switch:      normal
  RF power:             off
  ADC data stream:      not active
  Hardware type:        pg2sdr
  Active firmware:
    Version:            0.9.7.0             <---- running firmware version
    Compat:             0.9.0.0
    Max control xfer:   512 bytes
    Control timeout:    1000 ms
    Build type:         release pg2sdr
    Boot mode:          flash
  Flash firmware:
    Version:            0.9.7.0             <---- version stored on flash
    Compat:             0.9.0.0
    Max control xfer:   512 bytes
    Control timeout:    1000 ms
    Build type:         release pg2sdr
    Boot mode:          flash
    Total image size:   12832 bytes
    DFU release number: 0970
    DFU CRC:            4fbd29ad
1 matching device found
```

## Stop any processes 

First, stop any tools that are using the ProStick Gen 2, e.g. if you are
using dump1090-fa:

```bash
sudo systemctl stop dump1090-fa
```

## Testing a firmware update

You can temporarily test a new firmware version without making permanent
changes using the `pg2-util load-firmware` command:

```bash
pg2-util load-firmware pg2sdr-firmware-0.9.7.0.bin
```

The new firmware version will stay running until the device is next
reset or disconnected. After a reset or disconnection, the device will
go back to the old firmware version stored on flash.

## Writing a firmware update to flash (normal mode)

To permanently update your device to a new firmware version, use the
`pg2-util write-firmware` command, then reset the device to start
using the new firmware:

```bash
pg2-util write-firmware pg2sdr-firmware-0.9.7.0.bin
pg2-util reset
```

## Writing a firmware update to flash (recovery mode)

If your device has faulty firmware on flash and the process above does
not work, then you can try using recovery mode to update the firmware:

1. Disconnect the device
2. Find the small recovery switch on the edge of the device. Set it so that it
   is switched towards the USB connector.
3. Reconnect the device. The LED indicators should show one solid orange
   LED, and two faint red LEDs. The device is now in recovery mode, waiting
   to load new firmware.
4. Load and write the new firmware to flash:

```bash
pg2-util write-firmware pg2sdr-firmware-0.9.7.0.bin
```

5. Disconnect the device and move the recovery switch to the other position
   (switched towards the RF connector)
6. Reconnect the device.

## Verifying that the new firmware is running

To check that everything is correctly updated, run `pg2-util device-info`
again to check the firmware versions currently running and stored on flash.

## Restart any processes you stopped

Now that the device is on the new firmware version, restart any services
you stopped:

```bash
sudo systemctl start dump1090-fa
```
