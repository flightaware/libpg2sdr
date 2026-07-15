# libpg2sdr

&#x26A0; Work in progress, not finalized yet! &#x26A0;

This is the host library for the FlightAware ProStick Gen 2 ("pg2"), a
USB-connected, receive-only, software-defined radio designed to
receive aircraft navigation signals at frequencies near 1GHz (but also
capable of general-purpose SDR tasks).

The host library provides an API for connecting to devices,
configuring them for the desired frequency / sampling rate / etc,
streaming bulk sample data from the device, and converting those
samples to complex baseband.

It also provides a wrapper around the base API that implements a
SoapySDR driver, so any SoapySDR client can use the ProStick directly
without further code changes or recompilation.

Also included in this repository are CLI tools for inspecting and
updating device state and firmware (`pg2-util`) and a general-purpose
signal streaming tool (`pg2-rx`).

## User documentation

 * [Building and installing libpg2sdr](doc/install.md)
 * CLI tools
   * [`pg2-util`](doc/cli-pg2-util.md)
   * [`pg2-rx`](doc/cli-pg2-rx.md)
 * [Using the SoapySDR driver](doc/soapysdr.md)
 * [Updating the ProStick Gen 2 firmware](doc/firmware-update.md)
 * Hardware features (TBA)
 * [Current hardware/software limitations & TODO](doc/todo.md)

## Developer documentation

 * [Library overview](doc/library.md)
 * [Library API (doxygen)](https://flightaware.github.io/libpg2sdr/modules.html)
 * [Library internals](doc/internals.md)

## Related repositories

 * [ProStick Gen 2 firmware](https://github.com/flightaware/pg2sdr-firmware)
 * Python scripts for firmware development (TBA)
 * Python scripts for characterizing the hardware (TBA)

## License

libpg2sdr is licensed under a [2-clause BSD license](LICENSE)
