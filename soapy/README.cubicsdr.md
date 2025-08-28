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

The tuner logic is not yet hooked up to liblpcsdr / the soapysdr driver. You will need to
manually configure the tuner using the python scripts. Changing the "center frequency" in
CubicSDR (top right corner) does _nothing_ -- but you probably want to set it to the same
frequency as you actually tuned the tuner to, for simplicity. Same with gain, changing the gain
in CubicSDR does nothing.

Keep CubicSDR running. In a separate console window, run the python tuner.py script to
configure the tuner. You'll want to tune just above the frequency region you're interested in:

```
$ cd ~/git/liblpcsdr/lpcsdr_firmware
$ python/tuner.py --reset
$ python/tuner.py --pll 99.5M
$ python/tuner.py --lna-gain 7 --mix-gain 7 --vga-gain 7
$ python/tuner.py --hpf 500 --lpf-below 5M
```

## Interpreting the waterfall

The current implementation is a bit of a hack, in that it does not actually shift the signal
to baseband. What you're seeing is directly the spectrum of the real-valued signal coming from
the ADC. The left-hand side of the spectrum is what you want to be looking at; this is the
uninverted part of spectrum immediately below the PLL frequency you set. So in the above case
you should set the CubicSDR "center frequency" to 99.5MHz, to match the PLL, and then all the
frequencies in the waterfall that are <99.5MHz reflect what's being received. Frequencies
>99.5MHz are a mirror-image of the other side.

It should look something like this:

![CubicSDR FM waterfall](screenshot/cubicsdr-fm-waterfall.png)

## Listen to some FM radio!

Hook up an antenna and poke around the FM spectrum. left-clicking on the waterfall will center
a FM demodulator on that frequency and you should get some audio output, all going well ...

