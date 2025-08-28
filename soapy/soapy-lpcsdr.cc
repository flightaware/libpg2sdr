#include "soapy.h"

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


namespace SoapySDR {
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

LPCSDRDevice::LPCSDRDevice(Context &&ctx, lpcsdr_device_handle *handle) : ctx_(std::move(ctx)), handle_(handle)
{
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



void LPCSDRDevice::setSampleRate(const int direction, const size_t channel, const double rate)
{
    TRACECALLF("(%d,%zu,%f)", direction, channel, rate);
    CheckChannel(direction, channel);

    if (rate < 0 || rate > std::numeric_limits<uint32_t>::max())
        throw std::invalid_argument("sampling rate out of range");

    LIBCALL(lpcsdr_set_sample_rate, (uint32_t)rate);
}

double LPCSDRDevice::getSampleRate(const int direction, const size_t channel) const
{
    TRACECALLF("(%d,%zu)", direction, channel);
    CheckChannel(direction, channel);

    uint32_t freq;
    LIBCALL(lpcsdr_get_sample_rate, &freq);
    return freq;
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

}; // namespace LPCSDR
}; // namespace SoapySDR
