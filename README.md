# libpg2sdr

This is the host library for the Pro Stick Gen 2. It provides an API for
connecting to devices, configuring them for the desired frequency / sampling
rate / etc, streaming bulk sample data from the device, and converting those
samples to complex baseband.

It also provides a wrapper around the base API that implements a SoapySDR
driver, so any SoapySDR client can use the ProStick directly without further
code changes or recompilation.

## Building

cmake is used to build both the main library and the SoapySDR driver.

There is a top level Makefile that has some convenience targets to call
cmake to do the build.

The main ouputs (in `build/` by default) are:

 * Static libraries `libpg2sdr.a` and `libpg2sdr_dsp.a`; link to both.
 * A loadable SoapySDR driver module: `soapy/libpg2sdrSupport.so`

To use the SoapySDR driver, put it somewhere in SoapySDR's search path.
One way to do this is to set the environment variable `SOAPY_SDR_PLUGIN_PATH`
to include the `build/soapy` directory.

## Submodules

Two git submodules are present. You should do a `git submodule --init`
before building.

* `firmware` - points to the pg2sdr_firmware repository, required at build
time. The only part of this repository that gets used is
`firmware/include/pg2sdr_protocol.h`, which defines the USB protocol for
communicating over USB with the firmware.

* `dsp/starch` - code generator for parts of the DSP library. This is not
required at build time and can be omitted entirely. It is only needed
if the DSP code is modified and the starch-generated code needs to be
regenerated.

## Source layout

* `include` - both public and private includes. `include/pg2sdr.h` is the
public API.

* `src` - main host library implementation

* `dsp` - DSP library implementation. This library uses *starch* for code
generation, but this generation is done ahead of time and the resulting
generated code is committed to the repository, so you don't need a working
starch install unless you are modifying the DSP library.
  * `dsp/starch` - a git submodule pointing to the main starch repository
  * `dsp/include` - DSP library API
  * `dsp/src` - non-starch bits of the DSP library implementation
  * `dsp/impl` - implementations and benchmarks for DSP routines
  * `dsp/generated` - generated starch code that implements dispatchers for
    each DSP routine

* `bandpass` - data characterizing the tuner's bandpass filter, and a code
generation script that turns that data into code at build time.

* `gain` - data characterizing the tuner's gain stages, and a code
generation script that turns that data into code at build time.

* `screenshots` - images referenced by some readmes

* `tests` - handful of gtest-based unit tests; coverage isn't great.

* `soapy` - the SoapySDR driver

