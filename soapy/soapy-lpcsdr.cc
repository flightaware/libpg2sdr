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
#include <sstream>
#include <cstdarg>
#include <cinttypes>

#include <sys/prctl.h>

namespace LPCSDR {

// Some settings constants

static std::string setting_buffer_size = "buffer_size";

static std::string setting_decimation = "decimation";
static std::string setting_decimation_auto = "auto";
static std::string setting_decimation_max = "max";

// This only exists for the __attribute__ annotation, so gcc will check the format strings against arguments
static inline void Logf(SoapySDR::LogLevel level, const char *format, ...) __attribute__ ((format (printf, 2, 3)));

static inline int ReportLPCSDRError(lpcsdr_context *ctx, const char *fname, int error, bool throw_on_error)
{
    if (error < 0) {
        std::string message = std::string(fname) + ": " + lpcsdr_strerror_string(error);
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
#define TRACECALLF(_format, ...) LPCSDR::Logf(SOAPY_SDR_DEBUG, "LPCSDR: %s" _format, __func__ __VA_OPT__(,) __VA_ARGS__)

#define LIBCALL_DIRECT(_ctx, fn, ...) LPCSDR::ReportLPCSDRError(_ctx, #fn, fn(__VA_ARGS__), true)
#define LIBCALL_DIRECT_NOTHROW(_ctx, fn, ...) LPCSDR::ReportLPCSDRError(_ctx, #fn, fn(__VA_ARGS__), false)

#define LIBCALL(fn, ...) LIBCALL_DIRECT(ctx_, fn, handle_ __VA_OPT__(,) __VA_ARGS__)
#define LIBCALL_NOTHROW(fn, ...) LIBCALL_DIRECT_NOTHROW(ctx_, fn, handle_ __VA_OPT__(,) __VA_ARGS__)

static std::string format_ports(uint8_t *ports)
{
    std::string s;
    for (unsigned i = 0; ports[i]; ++i) {
        if (i != 0)
            s += ":";
        s += std::to_string(ports[i]);
    }
    return s;
}

static SoapySDR::Kwargs DeviceToKwargs(lpc_device *device)
{
    SoapySDR::Kwargs entry;
    entry["driver"] = "lpcsdr";
    entry["index"] = std::to_string(device->index);
    if (device->serial[0])
        entry["serial"] = device->serial;
    entry["bus"] = std::to_string(device->usb_bus);
    entry["address"] = std::to_string(device->usb_address);
    entry["ports"] = format_ports(device->usb_ports);
    entry["label"] = "LPCSDR@" + entry["bus"] + ":" + entry["ports"] + " s/n " + device->serial;
    return entry;
}

static std::pair<SoapySDR::KwargsList, std::vector<lpc_device *>> FindDevicesMatching(DeviceList &devices, const SoapySDR::Kwargs &kwargs)
{
    // extract just the args we want to match on;
    // a device is a match if every entry in 'matchers' is present in its own per-device args;
    // extra per-device args, or extra args in the criteria we don't recognize, are ignored
    SoapySDR::Kwargs matchers;
    for (auto kwarg : kwargs) {
        if (!kwarg.second.empty() && (kwarg.first == "driver" || kwarg.first == "index" || kwarg.first == "serial" || kwarg.first == "bus" || kwarg.first == "address" || kwarg.first == "ports"))
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

    auto devices = DeviceList::Enumerate(ctx, false);
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

    auto devices = DeviceList::Enumerate(ctx, false); // this needs to live beyond the match loop
    auto matching = FindDevicesMatching(devices, kwargs);
    if (matching.second.empty())
        throw std::runtime_error("No LPCSDR device found that matches '" + SoapySDR::KwargsToString(kwargs) + "'");

    if (matching.second.size() > 1) {
        SoapySDR::log(SOAPY_SDR_WARNING, "LPCSDR: more than one LPCSDR device matched '" + SoapySDR::KwargsToString(kwargs) + "'; trying the first one");
    }

    lpcsdr_device_handle *handle;
    LIBCALL_DIRECT(ctx, lpcsdr_open_device, matching.second[0], &handle);

    auto dev = new LPCSDRDevice(std::move(ctx), handle);
    Logf(SOAPY_SDR_DEBUG, "LPCSDR: constructed %p with liblpcsdr handle %p", dev, handle);
    return dev;
}

static SoapySDR::Registry registerLPCSDRDevice("lpcsdr", &LPCSDRDevice::FindDevices, &LPCSDRDevice::MakeDevice, SOAPY_SDR_ABI_VERSION);

LPCSDRDevice::~LPCSDRDevice()
{
    Logf(SOAPY_SDR_DEBUG, "LPCSDR: dtor called for %p", this);
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

LPCSDRDevice::LPCSDRDevice(Context &&ctx, lpcsdr_device_handle *handle) : ctx_(std::move(ctx)), handle_(handle)
{
    LIBCALL(lpcsdr_set_buffer_size, 128*1024);
    LIBCALL(lpcsdr_set_conversion_mode, LPCSDR_MODE_BASEBAND);
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

bool LPCSDRDevice::tryApplyChanges() const
{
    // It's possible that changes will fail to apply for a couple of reasons:
    //
    //  1) we're actively streaming, and settings will take effect when streaming next restarts;
    //  2) the current combination of frequency/rate/sideband/etc puts some values out of range,
    //     but we might be halfway through a series of API calls reconfiguring the device to a
    //     state that _is_ acceptable
    //
    // In those cases, log about the errors, but continue without throwing an exception; if they
    // are real errors, we'll try to re-apply them when streaming is next started, and those will
    // throw an error.

    int error = LIBCALL_NOTHROW(lpcsdr_apply_changes);
    if (!error)
        return true;
    if (error == LPCSDR_ERROR_BAD_STATE  /* todo: also ignore rate/freq range errors once they are recognizable */) {
        Logf(SOAPY_SDR_DEBUG,
             "Ignoring %s error from lpcsdr_apply_changes (changes will be applied when streaming is next restarted)",
             lpcsdr_strerror_string(error).c_str());
        return false;
    }

    std::string message = "Unable to apply pending changes: " + lpcsdr_strerror_string(error);
    throw std::runtime_error(message);
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

    // extend the tuning range a little by using low-sideband for low frequencies, high-sideband
    // for high frequencies. (todo: should we have hysteresis here, rather than just a single boundary?)
    LIBCALL(lpcsdr_set_sideband, frequency > 1e9 ? true : false);
    LIBCALL(lpcsdr_set_frequency, frequency);
    tryApplyChanges();
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

    double req, actual;
    tryApplyChanges(); // ensure actual frequency is up to date before querying
    LIBCALL(lpcsdr_get_frequency, &req, &actual);

    double freq = actual ? actual : req;
    Logf(SOAPY_SDR_DEBUG, " = %.3fMHz", freq/1e6);
    return freq;
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

    LIBCALL(lpcsdr_set_sample_rate, rate);
    tryApplyChanges();
}

double LPCSDRDevice::getSampleRate(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);

    double req, actual;
    tryApplyChanges(); // ensure actual rate is up to date before querying
    LIBCALL(lpcsdr_get_sample_rate, &req, &actual);

    double rate = actual ? actual : req;
    Logf(SOAPY_SDR_DEBUG, " = %.3fMHz", rate/1e6);
    return rate;
}

std::vector<double> LPCSDRDevice::listSampleRates(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);

    std::vector<double> result;
    for (auto i = 4; i <= 30; ++i)
        result.push_back(i * 0.5e6);
    return result;
}

SoapySDR::RangeList LPCSDRDevice::getSampleRateRange(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);

    SoapySDR::RangeList ranges;
    ranges.push_back(SoapySDR::Range(2.0, 15.0));
    return ranges;
}

void LPCSDRDevice::setBandwidth(const int direction, const size_t channel, const double bw)
{
    TRACECALLF("(%d,%zu,%.3fMHz)", direction, channel, bw/1e6);
    CheckChannel(direction, channel);

    LIBCALL(lpcsdr_set_bandpass, -bw/2.0, bw/2.0);
    tryApplyChanges();
}

double LPCSDRDevice::getBandwidth(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);

    double low, high, actual_low, actual_high;
    tryApplyChanges();
    LIBCALL(lpcsdr_get_bandpass, &low, &high, &actual_low, &actual_high);
    if (actual_low)
        low = actual_low;
    if (actual_high)
        high = actual_high;

    double bw = high - low;
    Logf(SOAPY_SDR_DEBUG, " = %.3fMHz", bw/1e6);
    return bw;
}

std::vector<double> LPCSDRDevice::listBandwidths(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);

    std::vector<double> result;
    for (auto i = 1; i <= 10; i++)
        result.push_back(i * 1e6);
    return result;
}

SoapySDR::RangeList LPCSDRDevice::getBandwidthRange(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);

    SoapySDR::RangeList ranges;
    ranges.push_back(SoapySDR::Range(1e6, 10e6));
    return ranges;
}

SoapySDR::ArgInfoList LPCSDRDevice::getSettingInfo(void) const
{
    SoapySDR::ArgInfoList args;

    SoapySDR::ArgInfo buffer_size;
    buffer_size.key = setting_buffer_size;
    buffer_size.value = "262144";
    buffer_size.name = "Buffer size";
    buffer_size.description = "Buffer size used to pass data to application (controls stream MTU)";
    buffer_size.units = "samples";
    buffer_size.type = SoapySDR::ArgInfo::Type::INT;
    buffer_size.range = SoapySDR::Range(0, UINT32_MAX, 1);

    args.push_back(buffer_size);

    SoapySDR::ArgInfo decimation;
    decimation.key = setting_decimation;
    decimation.value = setting_decimation_auto;
    decimation.name = "Decimation mode";
    decimation.description =
        "Control internal decimation stages, which improve fidelity but require higher ADC rates.";
    decimation.type = SoapySDR::ArgInfo::Type::STRING;
    decimation.options = std::vector<std::string> {
        setting_decimation_auto,
        setting_decimation_max,
        "0",
        "1", "2", "3", "4", "5", "6", "7", "8"
    };
    decimation.optionNames = std::vector<std::string> {
        /* auto */ "decimate as needed to avoid bandwidth loss at low IF frequencies",
        /* max */  "add decimation stages until ADC limits are reached",
        /* 0 */    "no additional decimation",
        /* 1 */    "decimate by 2**1 (/2)",
        /* 2 */    "decimate by 2**2 (/4)",
        /* 3 */    "decimate by 2**3 (/8)",
        /* 4 */    "decimate by 2**4 (/16)",
        /* 5 */    "decimate by 2**5 (/32)",
        /* 6 */    "decimate by 2**6 (/64)",
        /* 7 */    "decimate by 2**7 (/128)",
        /* 8 */    "decimate by 2**8 (/256)",
    };

    args.push_back(decimation);
    return args;
}

void LPCSDRDevice::writeSetting(const std::string &key, const std::string &value)
{
    TRACECALLF("(\"%s\",\"%s\")", key.c_str(), value.c_str());
    if (key == setting_buffer_size) {
        size_t size = std::stoi(value);
        LIBCALL(lpcsdr_set_buffer_size, size);
        return;
    } else if (key == setting_decimation) {
        int mode;
        if (value == setting_decimation_auto)
            mode = LPCSDR_DECIMATION_AUTO;
        else if (value == setting_decimation_max)
            mode = LPCSDR_DECIMATION_AUTO_MAX;
        else
            mode = std::stoi(value);

        LIBCALL(lpcsdr_set_decimation_mode, mode);
        return;
    } else {
        throw std::invalid_argument("unrecognized setting " + key);
    }
}

std::string LPCSDRDevice::readSetting(const std::string &key) const
{
    TRACECALLF("(\"%s\")", key.c_str());
    if (key == setting_buffer_size) {
        size_t size;
        LIBCALL(lpcsdr_get_buffer_size, &size);
        return std::to_string(size);
    } else if (key == setting_decimation) {
        int mode;
        LIBCALL(lpcsdr_get_decimation_mode, &mode);
        if (mode >= 0)
            return std::to_string(mode);
        else if (mode == LPCSDR_DECIMATION_AUTO)
            return setting_decimation_auto;
        else if (mode == LPCSDR_DECIMATION_AUTO_MAX)
            return setting_decimation_max;
        else
            throw std::runtime_error("bad decimation mode value");
    } else {
        throw std::invalid_argument("unrecognized setting " + key);
    }
}

std::vector<std::string> LPCSDRDevice::listGains(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);
    return { "LNA", "MIX", "VGA" };
}

void LPCSDRDevice::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    TRACECALLF("(%d,%zu,\"%s\",%f)", direction, channel, name.c_str(), value);
    CheckChannel(direction, channel);

    unsigned gain;
    if (value < 0)
        gain = 0;
    else if (value > 15)
        gain = 15;
    else
        gain = (unsigned) value;
    
    if (name == "LNA") {
        LIBCALL(lpcsdr_set_lna_gain, gain);
    } else if (name == "MIX") {
        LIBCALL(lpcsdr_set_mix_gain, gain);
    } else if (name == "VGA") {
        LIBCALL(lpcsdr_set_vga_gain, gain);
    } else {
        throw std::invalid_argument("unrecognized amplification element: " + name);
    }
}

double LPCSDRDevice::getGain(const int direction, const size_t channel, const std::string &name) const
{
    TRACECALLF("(%d,%zu,\"%s\")", direction, channel, name.c_str());
    CheckChannel(direction, channel);

    unsigned gain;
    if (name == "LNA") {
        LIBCALL(lpcsdr_get_lna_gain, &gain);
    } else if (name == "MIX") {
        LIBCALL(lpcsdr_get_mix_gain, &gain);
    } else if (name == "VGA") {
        LIBCALL(lpcsdr_get_vga_gain, &gain);
    } else {
        throw std::invalid_argument("unrecognized amplification element: " + name);
    }
    return gain;
}
    
SoapySDR::Range LPCSDRDevice::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    TRACECALLF("(%d,%zu,\"%s\")", direction, channel, name.c_str());
    CheckChannel(direction, channel);

    if (name != "LNA" && name != "MIX" && name != "VGA")
        throw std::invalid_argument("unrecognized amplification element: " + name);

    return SoapySDR::Range(0, 15, 1);
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
    return {SOAPY_SDR_CS16, SOAPY_SDR_CF32};
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

static void copy_cs16_to_cs16(void *out, const void *in, std::size_t samples)
{
    memcpy(out, in, samples * sizeof(int16_t) * 2);
}

static void copy_cs16_to_cf32(void *out, const void *in, std::size_t samples)
{
    float *out_f32 = (float *)out;
    const std::int16_t *in_s16 = (const std::int16_t *)in;

    while (samples-- > 0) {
        *out_f32++ = *in_s16++ / 32768.0; // real
        *out_f32++ = *in_s16++ / 32768.0; // imag
    }
}


LPCSDRStream::LPCSDRStream(LPCSDRDevice &dev, const std::string &format, std::size_t queue_limit)
    : dev_(dev), sample_rate_(0), queue_limit_(queue_limit), queue_size_(0), expected_timestamp_(0)
{
    if (format == SOAPY_SDR_CS16) {
        convert_ = &copy_cs16_to_cs16;
    } else if (format == SOAPY_SDR_CF32) {
        convert_ = &copy_cs16_to_cf32;
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
    return size;
}

int LPCSDRStream::activate()
{
    if (thread_) {
        // nothing to do
        return 0;
    }

    // record sampling rate at start of streaming,
    // so we can compute timestamps appropriately
    if (LIBCALL_DIRECT_NOTHROW(dev_.context(), lpcsdr_get_sample_rate, dev_.handle(), &sample_rate_, NULL) < 0)
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
        const std::int16_t *samples = pending_->buffer->samples;

        convert_(out, samples + pending_->offset * 2, to_copy);

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
