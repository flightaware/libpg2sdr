#include "soapy-lpcsdr.h"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Registry.hpp>

#include <algorithm>
#include <exception>
#include <functional>

#include <climits>
#include <cstring>
#include <limits>
#include <iostream>
#include <cstdarg>
#include <cinttypes>

#include <sys/prctl.h>

namespace LPCSDR {

// This only exists for the __attribute__ annotation, so gcc will check the format strings against arguments
static inline void Logf(SoapySDR::LogLevel level, const char *format, ...) __attribute__ ((format (printf, 2, 3)));

static inline int ReportLPCSDRError(lpcsdr_context *ctx, const char *fname, int error, bool throw_on_error)
{
    if (error < 0) {
        std::string message = std::string(fname) + ": " + std::to_string(error) + "/" + lpcsdr_strerror_string(error);
        SoapySDR::log(SOAPY_SDR_ERROR, "LPCSDR: " + message);
        if (throw_on_error)
            throw std::runtime_error(message);
    }
    return error;
}

static inline void Logf(SoapySDR::LogLevel level, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    SoapySDR::vlogf(level, format, ap);
    va_end(ap);
}

#define TRACECALL LPCSDR::Logf(SOAPY_SDR_DEBUG, "LPCSDR: %s()", __func__)
#define TRACECALLF(_format, ...) LPCSDR::Logf(SOAPY_SDR_DEBUG, "LPCSDR: %s" _format, __func__, __VA_ARGS__)

#define LIBCALL_DIRECT(_ctx, fn, ...) LPCSDR::ReportLPCSDRError(_ctx, #fn, fn(__VA_ARGS__), true)
#define LIBCALL_DIRECT_NOTHROW(_ctx, fn, ...) LPCSDR::ReportLPCSDRError(_ctx, #fn, fn(__VA_ARGS__), false)

#define LIBCALL(fn, ...) LIBCALL_DIRECT(ctx_, fn, handle_, __VA_ARGS__)
#define LIBCALL_NOTHROW(fn, ...) LIBCALL_DIRECT_NOTHROW(ctx_, fn, handle_, __VA_ARGS__)

static SoapySDR::Kwargs DeviceToKwargs(lpc_device *device)
{
    SoapySDR::Kwargs entry;
    entry["driver"] = "lpcsdr";
    entry["index"] = std::to_string(device->index);
    if (device->serial[0])
        entry["serial"] = device->serial;
    entry["bus"] = std::to_string(device->usb_bus);
    entry["address"] = std::to_string(device->usb_address);

    std::string basename = std::string("LPCSDR@" + std::to_string(device->usb_bus) + ":" + std::to_string(device->usb_address));

    switch (device->mode) {
    case LPCSDR_DEVICE_MODE_NORMAL:
        entry["label"] = basename + " S/N " + device->serial;
        break;
    case LPCSDR_DEVICE_MODE_DFU_BOOTLOADER:
        entry["label"] = basename + " in bootloader mode";
        break;
    }
    return entry;
}

static std::pair<SoapySDR::KwargsList, std::vector<lpc_device *>> FindDevicesMatching(DeviceList &devices, const SoapySDR::Kwargs &kwargs)
{
    // extract just the args we want to match on;
    // a device is a match if every entry in 'matchers' is present in its own per-device args;
    // extra per-device args, or extra args in the criteria we don't recognize, are ignored
    SoapySDR::Kwargs matchers;
    for (auto kwarg : kwargs) {
        if (!kwarg.second.empty() && (kwarg.first == "driver" || kwarg.first == "index" || kwarg.first == "serial" || kwarg.first == "bus" || kwarg.first == "address"))
            matchers.insert(kwarg);
    }

    SoapySDR::KwargsList result_args;
    std::vector<lpc_device *> result_devices;
    for (unsigned i = 0; i < devices.size(); ++i) {
        auto dev_kwargs = DeviceToKwargs(devices[i]);
        if (std::includes(dev_kwargs.begin(), dev_kwargs.end(), matchers.begin(), matchers.end())) {
            result_args.emplace_back(dev_kwargs);
            result_devices.emplace_back(devices[i]);
        }
    }
    return std::make_pair(result_args, result_devices);
}

SoapySDR::KwargsList LPCSDRDevice::FindDevices(const SoapySDR::Kwargs &kwargs)
{

    // Bail out early on requests that aren't for us
    auto driver = kwargs.find("driver");
    if (driver != kwargs.end() && driver->second != "lpcsdr")
        return {};

    TRACECALLF("(\"%s\")", SoapySDR::KwargsToString(kwargs).c_str());

    Context ctx = Context::Make();

    if (!ctx) {
        return {};
    }

    auto devices = DeviceList::Enumerate(ctx);
    auto matching = FindDevicesMatching(devices, kwargs);
    for (auto &match : matching.first) {
        SoapySDR::log(SOAPY_SDR_DEBUG, "candidate: " + SoapySDR::KwargsToString(match));
    }

    return matching.first;
}

SoapySDR::Device *LPCSDRDevice::MakeDevice(const SoapySDR::Kwargs &kwargs)
{
    TRACECALLF("(\"%s\")", SoapySDR::KwargsToString(kwargs).c_str());

    Context ctx = Context::Make();
    if (!ctx)
        throw std::runtime_error("could not initialize liblpcsdr: " + ctx.Error());

    auto devices = DeviceList::Enumerate(ctx); // this needs to live beyond the match loop
    auto matching = FindDevicesMatching(devices, kwargs);
    if (matching.second.empty())
        throw std::runtime_error("No LPCSDR device found that matches '" + SoapySDR::KwargsToString(kwargs) + "'");

    if (matching.second.size() > 1) {
        SoapySDR::log(SOAPY_SDR_WARNING, "LPCSDR: more than one LPCSDR device matched '" + SoapySDR::KwargsToString(kwargs) + "'; trying the first one");
    }

    lpcsdr_device_handle *handle;
    LIBCALL_DIRECT(ctx, lpcsdr_open_device, matching.second[0], &handle);

    return new LPCSDRDevice(std::move(ctx), handle);
}

static SoapySDR::Registry registerLPCSDRDevice("lpcsdr", &LPCSDRDevice::FindDevices, &LPCSDRDevice::MakeDevice, SOAPY_SDR_ABI_VERSION);

// template <typename T> static T Clamp(double d)
// {
//     using L = std::numeric_limits<T>;
//     if (d < L::lowest())
//         return L::lowest();
//     if (d > L::max())
//         return L::max();
//     return (T)d;
// }

LPCSDRDevice::~LPCSDRDevice()
{
    if (handle_) {
        if (LIBCALL_DIRECT_NOTHROW(ctx_, lpcsdr_close_device, handle_) < 0) {
            // this can happen if e.g. the underlying device is still busy in another thread
            // this is bad, because
            //  (a) we will leak the handle and
            //  (b) we can't safely free the context and must leak it
            // so yell about it a bit
            Logf(SOAPY_SDR_CRITICAL, "LPCSDR: LPCSDRDevice destructor could not clean up properly - resources leaked");
            ctx_.Release(); // leak the context to avoid freeing it while still in use
        }
    }
}

LPCSDRDevice::LPCSDRDevice(Context &&ctx, lpcsdr_device_handle *handle) : ctx_(std::move(ctx)), handle_(handle), tuned_freq_(0)
{
    LIBCALL(lpcsdr_set_buffer_size, 128*1024);

    // todo: soapy gain API
    LIBCALL(lpcsdr_set_lna_gain, 7);
    LIBCALL(lpcsdr_set_mix_gain, 7);
    LIBCALL(lpcsdr_set_vga_gain, 7);

    LIBCALL(lpcsdr_set_sideband, true);

    // LIBCALL(pxsdr_set_sampling_mode, PXSDR_SAMPLING_MODE_COMPLEX_BASEBAND, PXSDR_SAMPLE_FORMAT_INT16);

    // unsigned quantum;
    // LIBCALL(pxsdr_get_buffer_quantum, &quantum);
    // LIBCALL(pxsdr_set_buffering, 8, quantum * 64); /* default to about 1MB per buffer */

    // /* set up device specific stuff */
    // switch (pxsdr_get_device_info(handle)->variant) {
    // case PXSDR_VARIANT_R820T2:
    // case PXSDR_VARIANT_R10:
    // case PXSDR_VARIANT_R11:
    //     frequencies_ = SoapySDR::Range(24e6, 1800e6);
    //     bandwidths_ = SoapySDR::Range(0, 10.5e6);
    //     gains_["VGA"] = SoapySDR::Range(0, 15, 1);
    //     gains_["MIXER"] = SoapySDR::Range(0, 15, 1);
    //     gains_["LNA"] = SoapySDR::Range(0, 15, 1);
    //     break;

    // case PXSDR_VARIANT_MAX2112:
    //     frequencies_ = SoapySDR::Range(530e6, 2250e6);
    //     bandwidths_ = SoapySDR::Range(4e6, 40e6);
    //     gains_["LNA"] = SoapySDR::Range(0, 64, 1);
    //     gains_["VGA"] = SoapySDR::Range(0, 15, 1);
    //     break;

    // default:
    //     break;
    // }
}

std::string LPCSDRDevice::getDriverKey(void) const { return "lpcsdr"; }

std::string LPCSDRDevice::getHardwareKey(void) const { return "lpcsdr"; }

static inline void CheckChannel(const int direction, const size_t channel)
{
    if (direction != SOAPY_SDR_RX)
        throw std::invalid_argument("direction must be SOAPY_SDR_RX");
    if (channel != 0)
        throw std::invalid_argument("channel out of range");
}

void LPCSDRDevice::setFrequency(const int direction, const size_t channel, const double frequency, const SoapySDR::Kwargs &args)
{
    setFrequency(direction, channel, "RF", frequency, args);
}

void LPCSDRDevice::setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args)
{
    TRACECALLF("(%d,%zu,\"%s\",%f,\"%s\")", direction, channel, name.c_str(), frequency, SoapySDR::KwargsToString(args).c_str());
    CheckChannel(direction, channel);
    if (name != "" && name != "RF")
        throw std::invalid_argument("unrecognized tunable element " + name);

    // don't enforce frequency ranges, some out-of-range values might actually work
    LIBCALL(lpcsdr_tune_pll, frequency);
    tuned_freq_ = frequency;
}

double LPCSDRDevice::getFrequency(const int direction, const size_t channel) const
{
    return getFrequency(direction, channel, "RF");
}

double LPCSDRDevice::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    TRACECALLF("(%d,%zu,\"%s\")", direction, channel, name.c_str());
    CheckChannel(direction, channel);
    if (name != "" && name != "RF")
        throw std::invalid_argument("unrecognized tuneable element " + name);

    return tuned_freq_; // todo: liblpcsdr needs a get-frequency API
}

std::vector<std::string> LPCSDRDevice::listFrequencies(const int direction, const size_t channel) const
{
    // This slightly-confusingly-named method wants us to return a list of tuneable elements, not a list of frequencies
    return { "RF" };
}

SoapySDR::RangeList LPCSDRDevice::getFrequencyRange(const int direction, const size_t channel) const
{
    return getFrequencyRange(direction, channel, "RF");
}

SoapySDR::RangeList LPCSDRDevice::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
    TRACECALLF("(%d,%zu,\"%s\")", direction, channel, name.c_str());
    CheckChannel(direction, channel);
    if (name != "" && name != "RF")
        throw std::invalid_argument("unrecognized tuneable element " + name);

    SoapySDR::RangeList result;
    result.push_back(SoapySDR::Range(28e6, 1850e6));
    return result;
}

void LPCSDRDevice::setSampleRate(const int direction, const size_t channel, const double rate)
{
    TRACECALLF("(%d,%zu,%f)", direction, channel, rate);
    CheckChannel(direction, channel);

    if (rate < 0 || rate > std::numeric_limits<uint32_t>::max())
        throw std::invalid_argument("sampling rate out of range");

    {
        /* liblpcsdr won't change sample rate while actively streaming (it's complicated)
         * but soapysdr clients want to do that
         * so deactivate/reactivate the stream if we have one active
         */

        std::unique_lock<std::mutex> lock(mutex_); /* protect access to active_stream_ */
        if (active_stream_)
            active_stream_->deactivate();

        LIBCALL(lpcsdr_set_sample_rate, (uint32_t)rate);

        // todo: use soapy bandwidth API
        int nyquist = (int)round(rate/2.0);
        LIBCALL(lpcsdr_set_center_frequency_bandwidth, /* low */ 0, /* high */ rate/2.0, /* max */ &nyquist);

        if (active_stream_)
            active_stream_->activate();
    }
}

double LPCSDRDevice::getSampleRate(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);

    uint32_t freq;
    LIBCALL(lpcsdr_get_sample_rate, &freq);
    return freq;
}

std::vector<double> LPCSDRDevice::listSampleRates(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);

    std::vector<double> result;
    for (auto i = 1; i <= 20; ++i)
        result.push_back(i * 1e6);
    return result;
}

SoapySDR::RangeList LPCSDRDevice::getSampleRateRange(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);

    SoapySDR::RangeList ranges;
    ranges.push_back(SoapySDR::Range(1.0, 20.0));
    return ranges;
}

void LPCSDRDevice::writeSetting(const std::string &key, const std::string &value)
{
    TRACECALLF("(\"%s\",\"%s\")", key.c_str(), value.c_str());
    if (key == "buffer_size") {
        size_t size = std::stoi(value);
        LIBCALL(lpcsdr_set_buffer_size, size);
        return;
    }
    throw std::invalid_argument("unrecognized setting " + key);
}

std::string LPCSDRDevice::readSetting(const std::string &key) const
{
    if (key == "buffer_size") {
        size_t size;
        LIBCALL(lpcsdr_get_buffer_size, &size);
        return std::to_string(size);
    }
    throw std::invalid_argument("unrecognized setting " + key);
}

//
// STREAMING
//

size_t LPCSDRDevice::getNumChannels(const int direction) const
{
    TRACECALL;

    if (direction != SOAPY_SDR_RX)
        return 0;

    return 1;
}

std::vector<std::string> LPCSDRDevice::getStreamFormats(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);
    return {SOAPY_SDR_CS16, SOAPY_SDR_CF32, SOAPY_SDR_S16, SOAPY_SDR_F32};
}

std::string LPCSDRDevice::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);
    fullScale = 32768.0;
    return SOAPY_SDR_CS16;
}

SoapySDR::Stream *LPCSDRDevice::setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args)
{
    TRACECALLF("(%d,%s,[%zu items],\"%s\")", direction, format.c_str(), channels.size(), SoapySDR::KwargsToString(args).c_str());
    if (channels.size() == 1)
        CheckChannel(direction, channels[0]);
    else if (channels.size() > 1)
        throw std::invalid_argument("unexpected number of channels");

    auto stream = reinterpret_cast<SoapySDR::Stream *>(new LPCSDRStream(*this, format, 20'000'000));
    Logf(SOAPY_SDR_DEBUG, " = %p", stream);
    return stream;
}

void LPCSDRDevice::closeStream(SoapySDR::Stream *stream)
{
    TRACECALLF("(%p)", stream);
    auto s = reinterpret_cast<LPCSDRStream *>(stream);

    deactivateStream(stream, 0, 0); /* if it's still active, stop it */

    delete s;
}

int LPCSDRDevice::activateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs, const size_t numElems)
{
    TRACECALLF("(%p,%d,%lld,%zu)", stream, flags, timeNs, numElems);

    auto s = reinterpret_cast<LPCSDRStream *>(stream);
    if (s == nullptr)
        return SOAPY_SDR_STREAM_ERROR; /* bad arg */
        
    if (flags != 0)
        return SOAPY_SDR_NOT_SUPPORTED;
    if (timeNs != 0)
        return SOAPY_SDR_NOT_SUPPORTED;
    if (numElems != 0)
        return SOAPY_SDR_NOT_SUPPORTED;

    {
        std::unique_lock<std::mutex> lock(mutex_); /* protect access to active_stream_ */

        if (active_stream_ && active_stream_ != s) {
            return SOAPY_SDR_STREAM_ERROR; /* different stream already active */
        }

        int error = s->activate();
        if (error < 0)
            return error; /* activation failed */

        /* activation succeeded, note this stream as the active one */
        active_stream_ = s;
        return 0;
    }
}

int LPCSDRDevice::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
    TRACECALLF("(%p,%d,%lld)", stream, flags, timeNs);

    auto s = reinterpret_cast<LPCSDRStream *>(stream);
    if (s == nullptr)
        return SOAPY_SDR_STREAM_ERROR; /* bad arg */
        
    if (flags != 0)
        return SOAPY_SDR_NOT_SUPPORTED;
    if (timeNs != 0)
        return SOAPY_SDR_NOT_SUPPORTED;

    {
        std::unique_lock<std::mutex> lock(mutex_); /* protect access to active_stream_ */

        if (active_stream_ != s) {
            return 0; /* stream not active, nothing to do */
        }

        int error = s->deactivate();
        active_stream_ = nullptr;     /* always clear the stream association, even if deactivate failed (??) */
        return error;
    }
}

std::size_t LPCSDRDevice::getStreamMTU(SoapySDR::Stream *stream) const
{
    TRACECALLF("(%p)", stream);
    auto s = reinterpret_cast<LPCSDRStream *>(stream);
    if (s == nullptr)
        return SOAPY_SDR_STREAM_ERROR; /* bad arg */

    return s->getMTU();
}

int LPCSDRDevice::readStream(SoapySDR::Stream *stream, void *const *buffs, const size_t numElems, int &flags, long long &timeNs, const long timeoutUs)
{
    // This is too busy to trace
    auto s = reinterpret_cast<LPCSDRStream *>(stream);
    if (s == nullptr) {
        return SOAPY_SDR_STREAM_ERROR; /* bad arg */
    }

    // nb: it's legal (if silly) to call readStream before activation, or after deactivation,
    // so don't validate against active_stream_ here. If the stream is not active,
    // we'll just time out.
    return s->read(buffs[0], numElems, flags, timeNs, timeoutUs);
}

//
// LPCSDRStream
//

static void copy_s16_to_s16(void *out, const void *in, std::size_t samples)
{
    memcpy(out, in, samples * sizeof(int16_t));
}

static void copy_s16_to_f32(void *out, const void *in, std::size_t samples)
{
    float *out_f32 = (float *)out;
    const std::int16_t *in_s16 = (const std::int16_t *)in;

    while (samples-- > 0) {
        *out_f32++ = *in_s16++ / 32768.0;
    }
}

static void copy_s16_to_cs16(void *out, const void *in, std::size_t samples)
{
    std::int16_t *out_s16 = (std::int16_t *)out;
    const std::int16_t *in_s16 = (const std::int16_t *)in;

    while (samples-- > 0) {
        *out_s16++ = *in_s16++;  // real
        *out_s16++ = 0.0;        // imag
    }
}        

static void copy_s16_to_cf32(void *out, const void *in, std::size_t samples)
{
    float *out_f32 = (float *)out;
    const std::int16_t *in_s16 = (const std::int16_t *)in;

    while (samples-- > 0) {
        *out_f32++ = *in_s16++ / 32768.0; // real
        *out_f32++ = 0.0;                 // imag
    }
}        

LPCSDRStream::LPCSDRStream(LPCSDRDevice &dev, const std::string &format, std::size_t queue_limit)
    : dev_(dev), sample_rate_(0), queue_limit_(queue_limit), queue_size_(0), expected_timestamp_(0)
{
    // todo: use soapysdr-provided converters?
    if (format == SOAPY_SDR_S16) {
        convert_ = &copy_s16_to_s16;
    } else if (format == SOAPY_SDR_F32) {
        convert_ = &copy_s16_to_f32;
    } else if (format == SOAPY_SDR_CS16) {
        convert_ = &copy_s16_to_cs16;
    } else if (format == SOAPY_SDR_CF32) {
        convert_ = &copy_s16_to_cf32;
    } else {
        throw new std::invalid_argument("unsupported format " + format);
    }

    bytes_per_sample_ = SoapySDR::formatToSize(format);
}

LPCSDRStream::~LPCSDRStream()
{
    if (thread_) {
        // Being destroyed while streaming is active
        // We must stop the streaming thread right now to avoid touching object state after
        // it has been destroyed
        (void)LIBCALL_DIRECT_NOTHROW(dev_.context(), lpcsdr_stop_streaming, dev_.handle());

        // wait for the streaming thread to stop
        thread_->join();
        thread_.reset();
    }
}

size_t LPCSDRStream::getMTU() const
{
    size_t size;    
    LIBCALL_DIRECT(dev_.context(), lpcsdr_get_buffer_size, dev_.handle(), &size);
    return size / sizeof(std::int16_t);
}

int LPCSDRStream::activate()
{
    if (thread_) {
        // nothing to do
        return 0;
    }

    // record sampling rate at start of streaming,
    // so we can compute timestamps appropriately
    if (LIBCALL_DIRECT_NOTHROW(dev_.context(), lpcsdr_get_sample_rate, dev_.handle(), &sample_rate_) < 0)
        return SOAPY_SDR_STREAM_ERROR;

    expected_timestamp_ = 0;

    // start the streaming thread
    thread_.reset(new std::thread(std::bind(&LPCSDRStream::StreamingWorker, this)));
    return 0;
}

int LPCSDRStream::deactivate()
{
    if (!thread_) {
        // nothing to do
        return 0;
    }

    // ask liblpcsdr to stop streaming; this will make lpcsdr_stream_data()
    // stop receiving new data and return control to StreamingWorker
    if (LIBCALL_DIRECT_NOTHROW(dev_.context(), lpcsdr_stop_streaming, dev_.handle()) < 0)
        return SOAPY_SDR_STREAM_ERROR;

    // wait for the streaming thread to stop (this should happen promptly)
    thread_->join();
    thread_.reset();

    // drain any remaining data left on the queue and any pending partial block
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_.clear();
        queue_size_ = 0;
        pending_.reset();
    }

    return 0;
}

int LPCSDRStream::read(void * const buf, const size_t numElems, int &flags, long long &timeNs, const long timeoutUs)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(timeoutUs);
    int received = 0;
    size_t remaining = numElems;
    char *out = (char *)buf;

    flags = 0;

    while (remaining > 0) {
        // Need to copy more data

        if (!pending_) {
            // We don't have a partial item left over, wait for more data
            std::unique_lock<std::mutex> lock(queue_mutex_);

            if (!queue_signal_.wait_until(lock, deadline, [this]() -> bool { return !queue_.empty(); })) {
                // Reached deadline with no more data, return what we have (or report timeout)
                break;
            }

            // Grab the next thing off the queue
            pending_.emplace(std::move(queue_.front()));
            queue_.pop_front();
            queue_size_ -= pending_->size();
        }

        if (pending_->error) {
            // Next item is a pending error.
            if (received > 0) {
                // Just return what we have, and report the error later
                break;
            }

            // Report the error, _don't_ clear pending_, all subsequent calls will continue to report an error
            return SOAPY_SDR_STREAM_ERROR;
        }

        std::uint64_t timestamp = pending_->buffer->timestamp + pending_->offset;
        if (received == 0) {
            // Start of output buffer, set the timestamp
            if (expected_timestamp_ != 0 && expected_timestamp_ < timestamp) {
                SoapySDR::logf(SOAPY_SDR_DEBUG, "LPCSDR: timestamp jumped by %" PRIu64 " (samples dropped)", timestamp - expected_timestamp_);
            }

            timeNs = (unsigned long long)(timestamp * 1e9 / sample_rate_);
            flags = SOAPY_SDR_HAS_TIME;
        } else if (timestamp != expected_timestamp_) {
            // Timestamp discontinuity, return whatever we have already,
            // so we can report the timestamp change in the next call
            break;
        }

        // copy some samples to user buffer, advance everything

        std::size_t to_copy = std::min<std::size_t>(pending_->buffer->count - pending_->offset, remaining);
        const int16_t *samples = pending_->buffer->samples;

        convert_(out, samples + pending_->offset, to_copy);

        out += to_copy * bytes_per_sample_;
        received += to_copy;
        remaining -= to_copy;
        expected_timestamp_ = timestamp + to_copy;

        pending_->offset += to_copy;
        if (pending_->offset >= pending_->buffer->count) {
            // Done with this buffer entirely, free it
            pending_.reset();
        }
    }

    // Filled the output buffer or reached our deadline
    if (received > 0) {
        return received;
    } else {
        return SOAPY_SDR_TIMEOUT;
    }
}

void LPCSDRStream::StreamingWorker()
{
    /* pthread_setname_np does not seem to play well with how libc++ wraps the underlying threading library,
     * so just use prctl directly here (ugh)
     */
    prctl(PR_SET_NAME, (unsigned long) "lpcsdr-stream", 0, 0, 0);

    Logf(SOAPY_SDR_DEBUG, "LPCSDR: streaming thread started");
    // The real work happens in LPCSDRStream::StreamCallback
    int error = LIBCALL_DIRECT_NOTHROW(dev_.context(), lpcsdr_stream_data, dev_.handle(), &LPCSDRStream::StreamCallback, (void *)this, 0);

    // Enqueue any errors as a final queue item
    if (error < 0)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        // emplace_back would be better, 
        queue_.emplace_back(error);
        queue_signal_.notify_all();
    }

    Logf(SOAPY_SDR_DEBUG, "LPCSDR: streaming thread terminated");
}

// Bridge C callback API to C++
bool LPCSDRStream::StreamCallback(lpcsdr_sample_buffer *buffer, void *user_data)
{
    return reinterpret_cast<LPCSDRStream *>(user_data)->StreamCallback(buffer);
}

bool LPCSDRStream::StreamCallback(lpcsdr_sample_buffer *buffer)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto new_size = queue_size_ + buffer->count;
    if (new_size > queue_limit_) {
        // Our queue is too large. Drop new data.
        SoapySDR::logf(SOAPY_SDR_DEBUG, "LPCSDR: queue full, buffer dropped");
        return true; // caller can free the buffer
    }

    queue_.emplace_back(buffer);
    queue_size_ = new_size;
    queue_signal_.notify_all();
    return false; // caller should not free the buffer
}


}; // namespace LPCSDR
