# using this with CubicSDR

## Install soapysdr, cubicsdr & friends

```
$ sudo apt install cubicsdr libsoapysdr-dev soapysdr-tools
```

## Build liblpcsdr

```
$ cd ~/git/liblpcsdr
$ make
```

## Build the soapysdr driver

```
$ cd ~/git/liblpcsdr/soapy
$ make

```

## Set env vars

```
$ export LPCSDR_FIRMWARE=$HOME/git/liblpcsdr/lpcsdr_firmware/images/lpcsdr.bin
$ export SOAPY_SDR_PLUGIN_PATH=$HOME/git/liblpcsdr/soapy
$ export SOAPY_SDR_LOG_LEVEL=DEBUG
```

## Ensure that the LPCSDR has firmware loaded

The SoapySDR driver won't automatically load firmware. If needed, run any of
the python scripts once to load firmware:

```
$ python/status.py
```

## Check that the lpcsdr driver works OK standalone:

```
$ SoapySDRUtil --find
######################################################
##     Soapy SDR -- the SDR abstraction library     ##
######################################################

[DEBUG] LPCSDR: FindDevices("")
[DEBUG] candidate: address=3, bus=1, driver=lpcsdr, index=0, label=LPCSDR@1:1 s/n 38265463986061DC, ports=1, serial=38265463986061DC
Found device 0
  address = 3
  bus = 1
  driver = lpcsdr
  index = 0
  label = LPCSDR@1:1 s/n 38265463986061DC
  ports = 1
  serial = 38265463986061DC

```

```
$ SoapySDRUtil --args="driver=lpcsdr" --rate=10000000 --direction=RX --channels=0
######################################################
##     Soapy SDR -- the SDR abstraction library     ##
######################################################

[DEBUG] LPCSDR: FindDevices("driver=lpcsdr")
[DEBUG] candidate: address=3, bus=1, driver=lpcsdr, index=0, label=LPCSDR@1:1 s/n 38265463986061DC, ports=1, serial=38265463986061DC
[DEBUG] LPCSDR: MakeDevice("address=3, bus=1, driver=lpcsdr, index=0, label=LPCSDR@1:1 s/n 38265463986061DC, ports=1, serial=38265463986061DC")
[DEBUG] LPCSDR: setSampleRate(1,0,10000000.000000)
[DEBUG] liblpcsdr: set HPF cutoff = 0.527MHz
[DEBUG] liblpcsdr: set LPF cutoff = 9.955MHz
[DEBUG] liblpcsdr: ADC sample rate changes to 20000000.000000
[DEBUG] LPCSDR: getNativeStreamFormat(1,0)
[DEBUG] LPCSDR: setupStream(1,CS16,[1 items],"")
[DEBUG]  = 0xaaaad34d4ea0
Stream format: CS16
Num channels: 1
Element size: 4 bytes
Begin RX rate test at 10 Msps
[DEBUG] LPCSDR: getStreamMTU(0xaaaad34d4ea0)
Starting stream loop, press Ctrl+C to exit...
[DEBUG] LPCSDR: activateStream(0xaaaad34d4ea0,0,0,0)
[DEBUG] LPCSDR: streaming thread started
[DEBUG] liblpcsdr: allocate_transfers: 
  buffer_size              131072
  usb_transfer_size        92160
  adc_samples_per_transfer 61272
  transfer_count           32
  transfer_timeout_ms      598

[DEBUG] liblpcsdr: starting ADC transfers with:
  N: 0
  M: 15.00000
  P: 9
  I: 0
  fCCO: 360.00 MHz
  fADC: 20.00 MHz

[DEBUG] liblpcsdr: ADC overrun
[DEBUG] liblpcsdr: USB overrun
9.91835 Msps	39.6734 MBps
9.96034 Msps	39.8414 MBps
9.97378 Msps	39.8951 MBps
9.97941 Msps	39.9176 MBps
\^C[DEBUG] LPCSDR: deactivateStream(0xaaaad34d4ea0,0,0)
[DEBUG] liblpcsdr: lpcsdr_stream_data: something set the draining flag, stopping
[DEBUG] LPCSDR: streaming thread terminated
[DEBUG] LPCSDR: closeStream(0xaaaad34d4ea0)
[DEBUG] LPCSDR: deactivateStream(0xaaaad34d4ea0,0,0)
```

## Start cubicsdr

```
$ CubicSDR
```

## Configure cubicsdr

You should see the LPCSDR in the device list. Select it, set a sample rate of 10MHz, click Start:

![CubicSDR setup screen](screenshots/cubicsdr-startup.png)

You should now have a waterfall display running.

## Tuning

The complex baseband stuff is working now, so the tuned center frequency (top right
corner of CubicSDR) controls the center of the tuned band, i.e. the "zero" frequency
in the baseband data. The LPCSDR will capture a chunk of the spectrum surrounding that
center frequency, with a total width = the configured sample rate. Internally, the
tuner PLL will actually be tuned to one edge of that band. e.g. if you tune to a
center frequency of 95MHz, with a (complex) sampling rate of 10MHz, then the tuner PLL
will be tuned to 90MHz and the ADC will run at 20MHz, capturing frequencies between
90MHz - 100MHz.

## Gain

Currently the soapy driver just sets a fixed gain.

You can use the python tuner.py script in a separate console while CubicSDR is running, to
change the gains manually:

```
$ cd ~/git/liblpcsdr/lpcsdr_firmware
$ python/tuner.py --lna-gain 7 --mix-gain 7 --vga-gain 7
```

## Interpreting the waterfall

Now that we're using complex baseband, there's nothing special here, it's a direct interpretation
of a chunk of the spectrum with no mirroring.

It should look something like this:

![CubicSDR FM waterfall](screenshots/cubicsdr-fm-waterfall.png)

## Listen to some FM radio!

Hook up an antenna and poke around the FM spectrum. You can change the center frequency by
clicking the top/bottom parts of each frequency digit, or using the mousewheel there.
Clicking on the waterfall will center a FM demodulator on that frequency and you should
get some audio output, all going well ...
