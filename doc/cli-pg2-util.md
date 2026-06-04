# Command line utilities - pg2-util

libpg2sdr includes `pg2-util`, a swiss-army-knife
CLI utility for low-level device maintenance tasks.

When installed from Debian packages, `pg2-util` is
installed as part of the `pg2sdr-tools` package.

`pg2-util` is broken up into subcommands, each
invoked as `pg2-util <subcommand> ...`:

 * [`pg2-util help`](#help-subcommand) --
   list all subcommands, or provide details on
   a particular subcommand

 * [`pg2-util load-firmware`](#load-firmware-subcommand) --
   temporarily loads and activates new firmware

 * [`pg2-util write-firmware`](#write-firmware-subcommand) --
   writes a new firmware image to flash storage

 * [`pg2-util verify-firmware`](#verify-firmware-subcommand) --
   compares a firmware image to what's stored in
   flash storage

 * [`pg2-util device-info`](#device-info-subcommand) --
   enumerates connected devices, and shows their
   state and loaded firmware versions

 * [`pg2-util image-info`](#image-info-subcommand) --
   displays version information about a firmware
   image file

 * [`pg2-util reset`](#reset-subcommand) --
   manually resets a ProStick Gen 2 device

 * [`pg2-util standby`](#standby-subcommand) --
   puts a ProStick Gen 2 device into standby mode,
   powering down the RF stages

 * [`pg2-util blink`](#blink-subcommand) --
   make the indicator LEDs on a particular ProStick
   blink in a pattern for identification

## Common options

All subcommands accept these common options:

 * `-h`, `--help` - show help on a subcommand
 * `-q`, `--quiet` - suppress informational messages, show errors only

## Device selection

Most subcommands expect to deal with a single ProStick Gen 2
device. If there is only one device connected, no special options
are needed. If there is more than one device connected, provide
command-line options to select exactly one device unambiguously:

 * `-s', `--serial` selects a device with a serial number starting
   with the given prefix. This only works for devices that have loaded
   firmware; it does not work for devices in recovery mode.

 * `-p, `--port` selects a device by the physical USB port it is
   connected to.

USB port identifiers consist of a USB bus number, and then the path
from the bus root port through any USB hubs to the particular
connected port. The port numbering should usually stay the same unless
the physical connection path from the host to the device changes
(e.g. device is moved to a different physical USB port, or a USB hub
is added or removed)

The simplest way to find the USB port for a device is to look at the
"Port" line in the output of `pg2-util device-info`.

For example, given this device-info output:

```
Port 1-11:
  Device type:          ProStick Gen 2
  Serial number:        386297DBD86461DC
  [.. etc ..]
```

That device could be selected by passing `-s 386297` or `-p 1-11`

## `help` subcommand

`pg2-util help` lists all available subcommands.

`pg2-util help <subcommand>` provides more detailed help on a subcommand.

## `load-firmware` subcommand

`pg2-util load-firmware FILE` loads a firmware image stored in `FILE`
from the host to the active memory of a single ProStick Gen 2 device,
and starts that new firmware. The new firmware will run until the next
time the device is disconnected or reset, at which point it will
revert to whatever is stored on flash.

It can load images to devices in either recovery or normal mode.

`load-firmware` can be used to:

 * Load working firmware to a device that is in recovery mode
   (e.g. if there is a problem with the firmware stored on flash)

 * Test a new firmware version before writing it to flash

## `write-firmware` subcommand

`pg2-util write-firmware FILE` copies a firmware image stored in `FILE`
from the host to the flash storage of a single ProStick Gen 2 device.

Updating the image stored on flash does not immediately start the
new firmware version. Changes will take effect when the device is
next reset or disconnected.

Additional options:

 * `-f`, `--force-erase` - erase all flash sectors before writing
   the new image, rather than only rewriting sectors that have
   changed.

 * `-n`, `--dry-run` - describe changes that would have been made,
   but do not actually change the flash contents.

## `verify-firmware` subcommand

`pg2-util verify-firmware FILE` compares the firmware image stored
in `FILE` to the firmware image in the flash storage of a single
ProStick Gen 2 device.

If the flash contents differ, `pg2-util verify-firmware` shows an
error and returns a non-zero exit status.

## `device-info` subcommand

`pg2-util device-info` enumerates connected ProStick Gen 2 devices
and shows their current status to stdout. By default it will
discover and show the status for all connected devices.
Use the `-p` or `-s` options to show the status for a subset of
devices only.

Status output includes, for each device, where available:

 * Connected USB port
 * Connected device type (PG2 running normal firmware vs. PG2 in recovery mode)
 * Serial number
 * Recovery switch position
 * RF and ADC status
 * Hardware variant that the firmware was built for
 * Active (running) firmware details
 * Flash-storage firmware image details

`device-info` supports one additional option:

 * `-j`, `--json` - output status information as json to stdout,
   rather than as human-readable text. Use this type of output for
   scripting purposes.

## `image-info` subcommand

`pg2-util image-info` shows firmware information for a firmware
file stored on the host. It does not need access to a ProStick Gen 2
device.

`image-info` supports one additional option:

 * `-j`, `--json` - output firmware information as json to stdout,
   rather than as human-readable text. Use this type of output for
   scripting purposes.

## `reset` subcommand

`pg2-util reset` forcibly resets a ProStick Gen 2 device, causing it
to disconnect from the USB bus and reload firmware as if it had been
disconnected and reconnected manually. If another process is
concurrently using the device, that process will get interrupted when
the device disconnects.

## `standby` subcommand

`pg2-util standby` turns off RF power and halts the ADC on a ProStick
Gen 2 device. It does not fully reset the device. If another process
is concurrently using the device, that process will see unexpected
data-streaming timeouts and a loss of tuner state.

This subcommand is useful if software that previously used the device
exited without properly closing the device through the libpg2sdr API.
This can leave the device in a state where RF power and ADC clocks are
still running, unnecessarily consuming power and generating heat.

## `blink` subcommand

`pg2-util blink` controls the LED indicator lights on a ProStick Gen 2
device. Changes made by `pg2-util blink` last until the next execution
of `pg2-util blink` or until the device is reset.

`pg2-util blink` will cause all LEDs to blink at 2Hz.

`pg2-util blink --off` will return the LEDs to the default behaviour,
where the LEDs reflect the state of RF power, ADC, and tuner status.

`pg2-util blink PATTERN` will cause the LEDs to blink according to
the user-defined pattern string PATTERN. Pattern strings define between
1 and 6 LED states which are cycled through repeatedly at a rate of 4Hz.
Each LED state is represented by 4 characters:

 * one character for the state of the yellow RF power LED
 * one character for the state of the bicolor ADC status LED
 * one character for the state of the bicolor tuner status LED
 * one character that's ignored, just for spacing

The state of each LED is a character representing a color:

 * r - red (bicolor LEDS only)
 * g - green (bicolor LEDS only)
 * y - yellow
 * any other character - LED off.

Some examples:

 * `yyy-000-`: blink all LEDs, yellow, at 2Hz (the default pattern)
 * `y00-y00-y00-000-`: blink just the RF power LED at 1Hz with a 75% duty cycle
 * `0rg-0gr-`: alternating red/green on two LEDs, out of phase, at 2Hz
