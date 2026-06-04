# Building and installing libpg2sdr

There are a few different ways to install libpg2sdr, depending
on your OS and preferences.

On Debian and Debian-like systems (Raspberry Pi OS, Ubuntu, ...),
you probably want to install via Debian packages:

* [Install using prebuilt Debian packages](#install-using-prebuilt-debian-packages)

* [Build and install Debian packages from source](#build-and-install-debian-packages-from-source)

On other systems, you can build and install from source using cmake:

* [Build and install from source with cmake](#build-and-install-from-source-with-cmake)

## Install using prebuilt Debian packages

FlightAware provides prebuilt Debian packages for some
version/architecture combinations of Debian and Raspberry Pi OS via an
apt repository. Currently supported are:

* Raspberry Pi OS trixie, armhf (32-bit)
* Raspberry Pi OS trixie, arm64 (64-bit)
* Debian trixie, armhf (32-bit)
* Debian trixie, arm64 (64-bit)

To install from this repository:

1. Manually download and install the `flightaware-apt-repository`
   package. Installing this package adds configuration files to tell
   `apt` how to use the FlightAware apt repository:

```bash
wget https://apt.svc.flightaware.com/piaware/release/pool/piaware/f/flightaware-apt-repository/flightaware-apt-repository_1.3_all.deb
sudo dpkg -i flightaware-apt-repository_1.3_all.deb
```

2. Install the relevant libpg2sdr packages from the FlightAware apt
   repository:

```bash
sudo apt update
sudo apt install libpg2sdr
sudo apt install libpg2sdr-dev              # if you need development headers/libraries
sudo apt install pg2sdr-tools               # if you need the pg2-util and pg2-rx CLI utilities
sudo apt install soapysdr-module-pg2sdr     # if you need SoapySDR driver support
```

3. If you have the ProStick Gen 2 already connected, disconnect and
   reconnect it to ensure the system picks up the new device-specific
   udev rules installed by libpg2sdr.

## Build and install Debian packages from source

For Debian versions or architectures not directly supported by
FlightAware's apt repository, you can build your own packages from
source code:

1. Install build prerequisites:

```bash
sudo apt update
sudo apt install build-essential git debhelper pkg-config libusb-1.0-0-dev libsoapysdr-dev python3-minimal
```

2. Fetch the libpg2sdr source code:

```bash
git clone --recurse-submodules https://github.com/flightaware/libpg2sdr.git
```

3. Build the libpg2sdr packages:

```bash
cd libpg2sdr && dpkg-buildpackage -b --no-sign
```

4. Install the built packages:

```bash
sudo dpkg -i libpg2sdr_*.deb libpg2sdr-dev_*.deb pg2sdr-tools_*.deb soapysdr-module-pg2sdr_*.deb
```

5. If you have the ProStick Gen 2 already connected, disconnect and
   reconnect it to ensure the system picks up the new device-specific
   udev rules installed by libpg2sdr.


## Build and install from source with cmake

The exact details of how to do this will vary from system to system. This section gives a
general outline.

### Prerequisites

To build libpg2sdr and the CLI tools, you will need:

* A recent Linux-based OS. Other POSIX-ish (BSD, OSX, ...) systems
  may work with some tweaks but aren't tested.  Building on Windows
  is currently not supported.
* git
* CMake 3.15 or newer
* A C compiler that understands C11. gcc or clang both work fine.
* pkg-config
* libusb-1.0 development headers, library, and pkg-config file
* A Python 3 interpreter (used at build time only)

To build the SoapySDR driver, you will also need:

* A C++ compiler that understands C++17. g++ or clang both work fine.
* SoapySDR development headers, library, and CMake module

### Building from source

Fetch the libpg2sdr source code:

```bash
git clone --recurse-submodules https://github.com/flightaware/libpg2sdr.git
```

To build, you can either use the helper Makefile targets that invoke cmake:

```bash
# Building and installing with default configuration
cd libpg2sdr
make build
sudo make install PREFIX=/usr/local    # install to given prefix (defaults to /usr/local)
```

Alternatively, directly invoke cmake:

```bash
# Building and installing with custom cmake options
cd libpg2sdr
cmake -B ./build -S ./ -DENABLE_CLI=ON -DENABLE_SOAPYSDR=ON -DENABLE_TESTING=OFF -DCMAKE_VERBOSE_MAKEFILE=ON
cmake --build ./build
sudo cmake --install ./build --prefix /usr/local          # install to given prefix
```

The cmake system understands a few project-specific build-time options:

 * `ENABLE_CLI=(ON|OFF)` controls whether the CLI tools (pg2-util and
   pg2-rx) are built

 * `ENABLE_SOAPYSDR=(ON|OFF)` controls whether the SoapySDR driver
   module is built

 * `ENABLE_TESTING=(ON|OFF)` controls whether the unit test binaries
   are built

plus standard CMake options such as `CMAKE_VERBOSE_MAKEFILE` etc.

If you are installing to an unusual prefix, you may need to adjust
the dynamic linker search path (`LD_LIBRARY_PATH` etc) and/or set
the `SOAPY_SDR_PLUGIN_PATH` environment variable so that the library
and driver module can be found.

## Verify that your libpg2sdr installation is working

To verify that you have a working libpg2sdr, check that `pg2-rx`
runs correctly. You should see output similar to this:

```bash
pg2-rx -f 1090M -r 2.4M -t 5 /dev/null
```

```
Opened device on port 1-11 with serial 386297DBD86461DC
Configured with:
  center frequency: 1090.000104 MHz
  sampling rate:    2.400000 MSPS complex
  bandpass:         1088.437 MHz .. 1091.829 MHz (3392 kHz bandwidth)
  gain:             97.5 dB
  ADC rate:         9.600000 MSPS
  decimation:       1 stages, divide-by-2
Capturing for about 5.0 seconds
Writing sample data to /dev/null, ^C to stop early
Finished capturing data.
Received 13735680 samples (+ 0 dropped) in 5.762s
Effective sampling rate: 2.384 MSPS
```

## Verify that your SoapySDR driver installation is working

To verify that the SoapySDR driver is working correctly, use
`SoapySDRUtil` which is part of the standard SoapySDR tools. On Debian
systems it can be installed from the `soapysdr-tools` package. You
should get output similar to this:

```bash
SoapySDRUtil --info
```

```
######################################################
##     Soapy SDR -- the SDR abstraction library     ##
######################################################
[...]
Module found: /usr/lib/aarch64-linux-gnu/SoapySDR/modules0.8/libpg2sdrSupport.so (6e9b033)
[...]
Available factories... [...] pg2sdr [...]
```

```bash
SoapySDRUtil --find=driver=pg2sdr
```

```
######################################################
##     Soapy SDR -- the SDR abstraction library     ##
######################################################

Found device 0
  driver = pg2sdr
  label = ProStick Gen 2 @ 1-11 s/n 386297DBD86461DC
  ports = 1-11
  serial = 386297DBD86461DC
```
