# Library internals

Random notes mostly useful for doing development of the library itself.

## Structure of this repository

* `include` - public and private header files
    * `include/pg2sdr.h` - the public API.
    * `include/internal/*` - internal library headers
    * `include/firmware/` - symlink to the firmware submodule include dir
      (see below)

* `corelib` - code shared by `pg2-util` and the main library. This is
  a collection of object files used at build time (a CMake "object
  library") rather than a separate shared library.

* `src` - main host library implementation

* `dsp` - DSP library implementation. This library uses
  [starch](https://github.com/flightaware/starch/) for code
  generation, but this generation is done ahead of time and the
  resulting generated code is committed to the repository, so you
  don't need a working starch install unless you are modifying the DSP
  library.

    * `dsp/starchgen.py` - script that drives code generation
    * `dsp/starch` - a git submodule pointing to the main starch repository
    * `dsp/include` - DSP library API
    * `dsp/src` - non-starch bits of the DSP library implementation
    * `dsp/impl` - implementations and benchmarks for DSP routines
    * `dsp/generated` - generated starch code that implements dispatchers for
      each DSP routine

* `firmware` - a git submodule pointing to the `pg2sdr-firmware`
  repository.  The only thing used from this submodule is the header
  file that defines the USB protocol used for controlling the
  firmware.

    * `firmware/include/pg2sdr_protocol.h` - USB protocol definition header

* `tests` - handful of gtest-based unit tests; coverage isn't great.

* `bandpass` - data characterizing the tuner's bandpass filter, and a
  code generation script that turns that data into code at build time.

* `gain` - data characterizing the tuner's gain stages, and a code
  generation script that turns that data into code at build time.

* `soapy` - the SoapySDR driver implementation

* `cli/pg2-util` - a swiss-army-knife CLI utility for device
  maintenance e.g. loading firmware to RAM or flash, enumerating
  connected devices, inspecting device state, etc. This is closely
  tied to the hardware details and uses the `corelib` object library
  for low-level device access.

* `cli/pg2-rx` - a simple "receive samples and write them somewhere"
  utility that exposes most of the configuration options that the
  public library API. This is similar to the `rtl_sdr` utility
  from [librtlsdr](https://github.com/librtlsdr/librtlsdr), or
  `rx_sdr` from [rx_tools](https://github.com/rxseger/rx_tools).
  This uses the public library API only.

* `doc` - ad-hoc documentation

## Documentation

The public API is documented inline in `include/pg2sdr.h` using
doxygen-style formatting. Formatted API documentation is generated
automatically on commit to the master branch and is [published to
GitHub Pages](https://flightaware.github.io/libpg2sdr/modules.html)

To generate this documentation locally, run doxygen from the top level
of the repository and the generated documentation will be written to
`doc/html`:

```
sudo apt install doxygen graphviz
doxygen
```

There's not much formal documentation within the library itself.

## Symbol naming

This library is going to be used by third-party code, so there are a few
things we need to do to make sure it doesn't conflict with whatever that
code does.

### Contents of the public pg2sdr.h header

`pg2sdr.h` is the public header that third-party code is going to include.
We need to make sure that nothing in that header is going to conflict with
things in the third-party code.

All names -- functions, type names, macro names, enum names, etc -- visible
in pg2sdr.h should start with `pg2sdr_` or `PG2SDR_`.

The only things that should be present in `pg2sdr.h` are things that are
necessary for external use of the library.

Internal typedefs, functions, etc should be declared in a separate header.
In the internal headers we can do whatever we want (except for the non-static-
function naming rule below) as the internal headers won't be included in
third-party code.

### Naming non-static functions and global variables

Every non-static function, and every non-static global variable, should start
with `pg2sdr_`. This includes both internal and external functions/globals.

These symbols will be visible as public symbols in the compiled library, so
we need to make sure that they cannot clash with whatever names third-party
code uses.

For internal functions that are non-static, maybe we should follow a convention
like starting them with `pg2sdr__` (note two underscores) to distinguish them
from functions that are intended to be used externally.

For static functions, call them what you want, they will not turn into public
symbols. Generally, use static functions for anything internal that does not
need to be used from more than one `.c` source file (and isn't needed by
tests)

### Checking for stray symbols

Here's one way to look for stray symbols in the library output:

```
$ nm -C build/src/libpg2sdr.a  | grep -v pg2sdr_ | grep -v ' [Ua-z] '
```
