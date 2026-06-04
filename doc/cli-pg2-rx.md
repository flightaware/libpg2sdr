# Command line utilities - pg2-rx

libpg2sdr includes `pg2-rx`, a command-line utility that configure a
ProStick Gen 2 to receive data, then streams sample data to stdout or
a file in one of a few different formats.

When installed from Debian packages, `pg2-rx` is installed as part of
the `pg2sdr-tools` package.

## Command line syntax

In general, `pg2-rx` is used like this:

```bash
pg2-rx -f FREQUENCY -r RATE [options] FILENAME
```

`FILENAME` is the file to write samples to, or '-' to write to stdout.

Options that accept frequencies or sample rates understand `k` and `M`
suffixes, e.g. `-f 978.0M` to use a center frequency of 978MHz.

## Device selection

`pg2-rx` operates on a single device at a time. If there is only one
ProStick Gen 2 connected, it will use that device automatically. If
there is more than one ProStick Gen 2 connected, you must provide
command-line options to select exactly one device:

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

## RF options

These options configure RF tuning, ADC capture, and DSP
postprocessing:

 * `-f`, `--frequency` - set center frequency (i.e. the RF frequency that will appears at 0Hz in output), in Hz
 * `-r`, `--rate` - set sampling rate, in Hz
 * `-g`, `--gain` - set total gain, in dB
 * `-b`, `--bandwidth` - set bandpass filter bandwidth, in Hz
 * `-d`, `--decimation` - set integer number of divide-by-2 decimation steps, or `auto` (see library API docs)
 * `-u`, `--undersampling` - set undersampling mode (see library API docs)
 * `-a`, `--adc-limit` - set ADC sampling rate limit in Hz
 * `-i lower`, `--sideband=lower - select lower-sideband tuning (captured sideband < tuner LO)
 * `-i upper`, `--sideband=upper` - select upper-sideband tuning (captured sideband > tuner LO)

## Output format options

 * `-m baseband`, `--mode=baseband` (default setting) - produce
   complex-valued baseband samples, two values (real and imaginary, I
   and Q) per sample.

 * `-m lowif`, `--mode=lowif` - produce real-valued samples without
   conversion to baseband (i.e. ADC output, scaled but otherwise
   untransformed), one value per sample

 * `-o FMT`, '--output=FMT` - set the output data type to FMT, one of:
  * `int16` - signed 16-bit integers (default setting),
    scaled to [-32768, +32767]
  * `uint8` - unsigned 8-bit integers,
    scaled to [0, 255] (like the librtlsdr native format)
  * `float32` - signed 32-bit floating-point (IEEE-754 binary32),
    scaled to (-1.0, +1.0)
  * `float64` - signed 64-bit floating-point (IEEE-754 binary64),
    scaled to (-1.0, +1.0)

## Capture length options

These options control how many samples are captured. If neither option
is given, capture continues until interrupted by a signal / control-C:

 * `-t`, `--time-limit` - stop capture after a given duration (in seconds)
 * `-n`, `--sample-limit` - stop capture after a given number of samples

## General options

 * `-q`, `--quiet` - suppress informational messages, show errors only
 * `-v`, `--verbose` - generate additional debugging messages
 * `-h`, `--help` - show option help
