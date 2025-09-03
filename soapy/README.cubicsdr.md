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
[...]
[DEBUG] candidate: address=10, bus=1, driver=lpcsdr, index=0, label=LPCSDR@1:10 S/N 38265463986061DC, serial=38265463986061DC
[...]
Found device 2
  address = 10
  bus = 1
  driver = lpcsdr
  index = 0
  label = LPCSDR@1:10 S/N 38265463986061DC
  serial = 38265463986061DC
```

```
$ SoapySDRUtil --args="driver=lpcsdr" --rate=20000000 --direction=RX --channels=0
######################################################
##     Soapy SDR -- the SDR abstraction library     ##
######################################################

[DEBUG] LPCSDR: FindDevices("driver=lpcsdr")
[DEBUG] candidate: address=10, bus=1, driver=lpcsdr, index=0, label=LPCSDR@1:10 S/N 38265463986061DC, serial=38265463986061DC
[DEBUG] LPCSDR: MakeDevice("address=10, bus=1, driver=lpcsdr, index=0, label=LPCSDR@1:10 S/N 38265463986061DC, serial=38265463986061DC")
[DEBUG] LPCSDR: setSampleRate(1,0,20000000.000000)
[DEBUG] LPCSDR: getNativeStreamFormat(1,0)
[DEBUG] LPCSDR: setupStream(1,CS16,[1 items],"")
[DEBUG]  = 0xaaaabdb11170
Stream format: CS16
Num channels: 1
Element size: 4 bytes
Begin RX rate test at 20 Msps
[DEBUG] LPCSDR: getStreamMTU(0xaaaabdb11170)
Starting stream loop, press Ctrl+C to exit...
[DEBUG] LPCSDR: activateStream(0xaaaabdb11170,0,0,0)
[DEBUG] LPCSDR: streaming thread started
[DEBUG] liblpcsdr: allocate_transfers: 
  buffer_size    131072
  samples/buffer 65536
  samples/block  6808
  blocks/buffer  9
  bytes/block    10240
  transfer_size  92160
  transfer_count 4
  transfer_timeout_ms 512

19.834 Msps	79.3361 MBps
19.9135 Msps	79.6539 MBps
19.9428 Msps	79.7713 MBps
\^C[DEBUG] LPCSDR: deactivateStream(0xaaaabdb11170,0,0)
[DEBUG] liblpcsdr: lpcsdr_stream_data: something set the draining flag, stopping
[DEBUG] LPCSDR: streaming thread terminated
[DEBUG] LPCSDR: closeStream(0xaaaabdb11170)
[DEBUG] LPCSDR: deactivateStream(0xaaaabdb11170,0,0)
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

The "center frequency" (top right corner) in CubicSDR controls the tuned PLL frequency.
Frequencies immediately above the PLL will be captured (e.g., to see the broadcast FM
spectrum, try tuning to 90MHz and you will capture 90-95MHz)

Unlike earlier iterations of this code, we are capturing the upper sideband, so the PLL
should be tuned below the frequency you want to look at, and the resulting ADC spectrum
is _not_ inverted.

## Gain

Currently the soapy driver just sets a fixed gain.

You can use the python tuner.py script in a separate console while CubicSDR is running, to
change the gains manually:

```
$ cd ~/git/liblpcsdr/lpcsdr_firmware
$ python/tuner.py --lna-gain 7 --mix-gain 7 --vga-gain 7
```

## Interpreting the waterfall

The current implementation is a bit of a hack, in that it does not actually shift the signal
to baseband. What you're seeing is directly the spectrum of the real-valued signal coming from
the ADC.

You will see a mirrored signal, mirrored around a center frequency which is the PLL / center
frequency you set. The right-hand side of the spectrum is exactly what the ADC is seeing, with
the center frequency corresponding to a 0Hz (DC) signal from the ADC. Frequency marks on the
right-hand side will correctly reflect the RF frequency. For example, if you tune the PLL to
90MHz, then a signal showing at 92MHz corresponds to a RF signal at 92MHz, and an ADC signal
at 2MHz.

The left-hand side is a mirror-image of the right-hand side (because we are doing a discrete
Fourier transform on a real-valued signal, and that produces negative frequency components that
mirror the positive frequency components). There are no new signals in there.

It should look something like this:

![CubicSDR FM waterfall](screenshots/cubicsdr-fm-waterfall.png)

## Listen to some FM radio!

Hook up an antenna and poke around the FM spectrum. left-clicking on the waterfall will center
a FM demodulator on that frequency and you should get some audio output, all going well ...

