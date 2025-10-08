# Getting dump1090 working with liblpcsdr

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

## Check out and build liblpcsdr

You might need to do the checkout on a host with VPN/GHE access then
copy over the checked-out repo

```
$ mkdir ~/git
$ cd ~/git
$ git clone --recurse-submodules git@github.flightaware.com:flightaware/liblpcsdr.git -b initial

$ cd ~/git/liblpcsdr
$ make
```

## Check out and build dump1090

You don't _have_ to build dump1090 yourself, it should be possible to use an
existing dump1090 package as they already have soapysdr support.

If you're not using an existing package, then build from source:

```
$ cd ~/git
$ git clone https://github.com/flightaware/dump1090.git

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

## Install lpcsdr udev rules

```
$ sudo cp ~/git/lpcsdr/lpcsdr_firmware/udev/99-lpcsdr.rules /etc/udev/rules.d/
$ sudo systemctl reload udev
```

Disconnect & reconnect the lpcsdr so the new rules are applied.

## Ensure lpcsdr firmware is loaded

```
$ ~/git/liblpcsdr/lpcsdr_firmware/python/status.py
```

## Set soapy env vars

You probably want to put these in your ~/.bashrc or something so you don't
have to remember to run them in every new shell:

```
$ export SOAPY_SDR_LOG_LEVEL=DEBUG
$ export SOAPY_SDR_PLUGIN_PATH=$HOME/git/liblpcsdr/build/soapy
```

## Check that SoapySDR sees the lpcsdr support library

```
$ SoapySDRUtil --info
[...]
Search path:  /home/pi/git/liblpcsdr/soapy
Module found: /home/pi/git/liblpcsdr/soapy/liblpcsdrSupport.so
Available factories... lpcsdr
[...]
```

## Check that SoapySDR sees the lpcsdr device

```
$ SoapySDRUtil --sparse --find=driver=lpcsdr
[DEBUG] LPCSDR: FindDevices("driver=lpcsdr")
[DEBUG] candidate: address=19, bus=3, driver=lpcsdr, index=0, label=LPCSDR@3:2 s/n 38265463986061DC, ports=2, serial=38265463986061DC
0: LPCSDR@3:2 s/n 38265463986061DC
```

## Run dump1090

You might want to run this under a `screen` instance if you're going to
leave it unattended.

```
$ /home/pi/git/dump1090/dump1090                                  \
   --device-type soapy --device driver=lpcsdr                     \
   --gain-element VGA:8 --gain-element MIX:8 --gain-element LNA:8 \
   --lat <your location> --lon <your location>                    \
   --fix
```

This will spit out a lot of debugging info on startup, then write decoded
ADS-B messages to stdout as it receives them.

The gain-element settings sets the gain of each individual gain stage directly
(each in the range 0-15)

## Extra dump1090 args

Useful extra flags to pass, depending on what you're doing:

`--quiet` - don't write the decoded ADS-B mesages to stdout. You will want this after you've confirmed that messages are arriving correctly.

`--net-bo-port 12345` -- make Beast-format data available on TCP port 12345. You could point a relay-mode piaware at this. Or, if you don't need skyaware, put dump1090 on port 30005 directly.

`--write-json /tmp/lpcsdr-json/` -- write json files to `/tmp/lpcsdr-json` (suitable for e.g. feeding to skyaware_logger)
