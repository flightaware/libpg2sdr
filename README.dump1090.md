# Getting dump1090 working with libpg2sdr

This assumes a Raspberry Pi OS bookworm arm64 install.
Other base installs (notably a piaware sdcard image) may need slight tweaks.

## Install prerequisites

```
$ sudo apt install \
 cmake             \
 libusb-1.0-0-dev  \
 python3-usb       \
 libncurses-dev    \
 rtl-sdr           \
 librtlsdr-dev     \
 git

$ sudo apt install --no-install-recommends \
 libsoapysdr-dev \
 soapysdr-tools
```

(nb: rtl-sdr / librtlsdr-dev only necessary if you want to build a dump1090
with rtl-sdr support, e.g. for direct comparison with a prostick)

## Check out and build libpg2sdr

You might need to do the checkout on a host with VPN/GHE access then
copy over the checked-out repo

```
$ mkdir ~/git
$ cd ~/git
$ git clone --recurse-submodules git@github.flightaware.com:flightaware/libpg2sdr.git

$ cd ~/git/libpg2sdr
$ make
```

## Check out and build dump1090

You don't _have_ to build dump1090 yourself, it should be possible to
use an existing dump1090 package as they already have soapysdr
support. But you probably want to build from the soapy-settings branch
for support for --device-setting

If you're not using an existing package, then build from source:

```
$ cd ~/git
$ git clone https://github.com/flightaware/dump1090.git -b soapy-settings

$ cd ~/git/dump1090

 # Check the build sees the SoapySDR libraries

$ make showconfig
Building with:
  Version string:   unknown
  Architecture:     aarch64
  DSP mix:          aarch64
  RTLSDR support:   yes
  BladeRF support:  no
  HackRF support:   no
  LimeSDR support:  no
  SoapySDR support: yes       <<< Check this says yes

  # Build it all

$ make all
```

## Install pg2sdr udev rules

```
$ sudo cp ~/git/libpg2sdr/firmware/udev/99-pg2sdr.rules /etc/udev/rules.d/
$ sudo systemctl reload udev
```

Disconnect & reconnect the pg2sdr so the new rules are applied.

## Ensure pg2sdr firmware is loaded

The python scripts are now in a separate repo https://github.flightaware.com/flightaware/pg2sdr-python
```
$ ~/git/pg2sdr-python/status.py
```

## Set soapy env vars

You probably want to put these in your ~/.bashrc or something so you don't
have to remember to run them in every new shell:

```
$ export SOAPY_SDR_LOG_LEVEL=DEBUG
$ export SOAPY_SDR_PLUGIN_PATH=$HOME/git/libpg2sdr/build/soapy
```

## Check that SoapySDR sees the pg2sdr support library

```
$ SoapySDRUtil --info
[...]
Search path:  /home/pi/git/libpg2sdr/soapy
Module found: /home/pi/git/libpg2sdr/soapy/libpg2sdrSupport.so
Available factories... pg2sdr
[...]
```

## Check that SoapySDR sees the pg2sdr device

```
$ SoapySDRUtil --sparse --find=driver=pg2sdr
[DEBUG] PG2SDR: FindDevices("driver=pg2sdr")
[DEBUG] candidate: address=2, bus=1, driver=pg2sdr, index=0, label=ProStick Gen 2 @ 1:11 s/n 386297DBD86461DC, ports=11, serial=386297DBD86461DC
0: ProStick Gen 2 @ 1:11 s/n 386297DBD86461DC
```

## Run dump1090

You might want to run this under a `screen` instance if you're going to
leave it unattended.

```
$ /home/pi/git/dump1090/dump1090                                  \
   --device-type soapy --device driver=pg2sdr                     \
   --gain 60 \
   --lat <your location> --lon <your location>                    \
   --fix
```

This will spit out a lot of debugging info on startup, then write decoded
ADS-B messages to stdout as it receives them.

Gain is total gain in dB (approx)

## Extra dump1090 args

Useful extra flags to pass, depending on what you're doing:

`--device-setting decimation=max` - makes the ADC will run at 19.2MHz
(default is 9.6)

`--quiet` - don't write the decoded ADS-B mesages to stdout. You will
want this after you've confirmed that messages are arriving correctly.

`--net-bo-port 12345` -- make Beast-format data available on TCP port
12345. You could point a relay-mode piaware at this. Or, if you don't
need skyaware, put dump1090 on port 30005 directly.

`--write-json /tmp/pg2sdr-json/` -- write json files to
`/tmp/pg2sdr-json` (suitable for e.g. feeding to skyaware_logger)
