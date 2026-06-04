# Limitations & to-do list

The ProStick Gen 2 has some hardware limitations, and the current
firmware and host library have some software limitations.  Some of
those can be fixed in the future.

# Hardware limitations

## ADC rate limited by USB bandwidth

The LPC4370 high-speed ADC can, in theory, run at up to 80MHz.  The M4
core can probably keep up with this data rate.  However, the bandwidth
available to a USB2.0 device limits the effective rate that samples
can be pushed to the host. This limit effectively restricts ADC
sampling rates to no more than about 28MHz (varying depending on the
host USB implementation) before sample data starts to be dropped.

It might be possible to implement something in the firmware to trade
ADC bits for ADC rate, e.g. sample at ~40MHz with 12 bit resolution,
but only send 8 bit samples over USB. Whether this is actually
_useful_ is a separate question!

## Hardware 12MHz spur

There's a weak 12MHz spur that appears in ADC output (either directly
at 12MHz, or at an alias of that frequency when the ADC sample rate is
less than 24MHz). This spur appears to be internal interference within
the LPC4370 itself.

Careful selection of sample rates can put that spur somewhere where it
doesn't interfere with the input signal. Generally, an ADC rate of
24MHz/N, for integer N, will put the spur at the very edge of the
captured bandwidth, where it shouldn't interfere with the input
signal.

It should be possible for the host library to automatically avoid the
spur, given enough headroom to increase the ADC rate:

 * select an ADC rate that's higher than the Nyquist rate for the
   requested sample rate, and which puts the spur alias near the edge
   of the captured IF signal;
 * tune so that the desired input signal does not overlap with the
   spur, using the extra bandwidth available due to the higher ADC
   rate;
 * after I/Q conversion, frequency-shift the converted signal to
   compensate for the tuning offset, and decimate to the requested
   sample rate.

However this automatic process is not currently implemented in the
host library.

## Low cutoff limits of the tuner bandpass filter

The low side of the tuner's bandpass filter (i.e. the high-pass filter
cutoff) has a minimum cutoff of around 500kHz. This means that
attempting a full-bandwidth capture of the IF signal (e.g.  trying to
capture 2MHz of bandwidth with an ADC sampling rate of 4MHz) will
unavoidably lose some of the low end of the signal and the effective
bandwidth available is lower than expected.

This particularly affects lower sampling rates, where the IF signal is
closer to DC and a larger proportion of the desired signal lies below
500kHz.

The host library tries to avoid this where possible by increasing the
ADC rate and adding decimation steps, if the decimation mode is set to
auto (the default). This moves the desired signal further from DC and
away from the low side of the bandpass filter.

# Software limitations

## Re-tuning can cause some loss of sample data

The current firmware runs only on the M4 core of the LPC4370, and is
essentially single-threaded. Transfer of sample data over USB, and
handling individual USB control transfer requests, operate serially.

If tuner parameters are changed, e.g. the tuned RF frequency is
changed, reprogramming the tuner over I2C happens synchronously in
response to a USB control transfer. This can monopolize the M4 core
for a comparatively long time - tens of milliseconds.

At higher sampling rates, this means that USB transfers of sample data
can be interrupted for long enough for the device buffers to fill, and
some sample data will be lost.

Ideally, tuner I2C communication should be delegated to one of the
auxiliary M0 cores of the LPC4370, so that the synchronous I2C work
can happen on that M0 core without blocking ongoing USB bulk transfers
handled by the M4 core. There is a framework for inter-core IPC
implemented in the current firmware, but the M0 cores are not
currently used.

## Tuner tracking filter is not used

The R860T tuner has a tracking filter early in the RF receive path
that's intended to be used for lower frequencies (below around
600MHz).

The current host library implementation does not attempt to set up the
tracking filter for lower frequencies, and always bypasses it.
Configuring the filter correctly might improve performance at lower
frequencies (though, the wideband LNA in the RF path before the tuner
may make additional filtering less useful)

## No tuner calibration

The current host library implementation does nothing special to
calibrate the R860T tuner, but appears to work without that.  Some
other software that uses the same tuner has additional calibration
steps that claim to calibrate the tuner's filters, and I/Q path
gain/phase corrections. It's unclear whether these steps are actually
doing anything; much of the shared knowledge of how to use these
tuners is heavily cargo-culted without explanation of the "why" of it,
and poking at the calibration registers on prototype devices produced
no apparent change in performance.

It's possible that there _is_ useful calibration to be done here, but
it needs more investigation and a wider range of individual tuner
chips to experiment against.
