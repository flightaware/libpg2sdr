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


namespace SoapySDR {
namespace LPCSDR {
static inline int ReportLPCSDRError(lpcsdr_context *ctx, const char *fname, int error, bool throw_on_error)
{
    if (error < 0) {
        SoapySDR::logf(SOAPY_SDR_ERROR, "SoapyLPCSDR: %s: %s", fname, lpcsdr_strerror(ctx, error));
        if (throw_on_error)
            throw std::runtime_error(std::string(fname) + ": " + lpcsdr_strerror(ctx, error));
    }
    return error;
}

#define TRACECALL SoapySDR::logf(SOAPY_SDR_DEBUG, "LPCSDR: %s()", __func__)
#define TRACECALLF(_format, ...) SoapySDR::logf(SOAPY_SDR_DEBUG, "LPCSDR: %s" _format, __func__, __VA_ARGS__)

#define PXCALL_DIRECT(_ctx, fn, ...) SoapySDR::LPCSDR::ReportLPCSDRError(_ctx, #fn, fn(__VA_ARGS__), true)
#define PXCALL_DIRECT_NOTHROW(_ctx, fn, ...) SoapySDR::LPCSDR::ReportLPCSDRError(_ctx, #fn, fn(__VA_ARGS__), false)

#define PXCALL(fn, ...) PXCALL_DIRECT(ctx_, fn, handle_, __VA_ARGS__)
#define PXCALL_NOTHROW(fn, ...) PXCALL_DIRECT_NOTHROW(ctx_, fn, handle_, __VA_ARGS__)

static SoapySDR::Kwargs DeviceToKwargs(lpc_device *device)
{
    SoapySDR::Kwargs entry;
    entry["driver"] = "lpcsdr";
    entry["index"] = std::to_string(device->index);
    if (device->serial[0])
        entry["serial"] = device->serial;
    entry["bus"] = std::to_string(device->usb_bus);
    entry["address"] = std::to_string(device->usb_address);
    return entry;
}

static std::pair<SoapySDR::KwargsList, std::vector<lpc_device *>> FindDevicesMatching(DeviceList &devices, const SoapySDR::Kwargs &kwargs)
{
    // extract just the args we want to match on;
    // a device is a match if every entry in 'matchers' is present in its own per-device args;
    // extra per-device args, or extra args in the criteria we don't recognize, are ignored
    SoapySDR::Kwargs matchers;
    std::string decimation = "auto";
    for (auto kwarg : kwargs) {
        if (!kwarg.second.empty() && (kwarg.first == "driver" || kwarg.first == "index" || kwarg.first == "serial" || kwarg.first == "bus" || kwarg.first == "address"))
            matchers.insert(kwarg);
        if (kwarg.first == "decimation")
            decimation = kwarg.second;
    }

    std::cout << "hello!";

    SoapySDR::KwargsList result_args;
    std::vector<lpc_device *> result_devices;
    for (unsigned i = 0; i < devices.size(); ++i) {

        auto dev_kwargs = DeviceToKwargs(devices[i]);
        if (std::includes(dev_kwargs.begin(), dev_kwargs.end(), matchers.begin(), matchers.end())) {
            dev_kwargs["decimation"] = decimation;
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

    TRACECALLF("(\"%s\")", KwargsToString(kwargs).c_str());

    Context ctx = Context::Make();

    if (!ctx) {
        return {};
    }

    auto devices = DeviceList::Enumerate(ctx);
    auto matching = FindDevicesMatching(devices, kwargs);
    return matching.first;
}

SoapySDR::Device *LPCSDRDevice::MakeDevice(const SoapySDR::Kwargs &kwargs)
{
    TRACECALLF("(\"%s\")", KwargsToString(kwargs).c_str());

    Context ctx = Context::Make();
    if (!ctx)
        throw std::runtime_error("could not initialize liblpcsdr: " + ctx.Error());

    auto devices = DeviceList::Enumerate(ctx); // this needs to live beyond the match loop
    auto matching = FindDevicesMatching(devices, kwargs);
    if (matching.second.empty())
        return nullptr;

    if (matching.second.size() > 1) {
        SoapySDR::logf(SOAPY_SDR_WARNING, "SoapyLPCSDR: more than one LCPSDR device matched the given criteria; trying the first one");
    }

    lpcsdr_device_handle *handle;
    PXCALL_DIRECT(ctx, lpcsdr_open_device, matching.second[0], &handle);

    // if (kwargs.count("decimation")) {
    //     auto &decimation = kwargs.at("decimation");
    //     unsigned factor;
    //     if (decimation == "auto")
    //         factor = 0;
    //     else
    //         factor = std::stoi(decimation);

    //     PXCALL_DIRECT(ctx, pxsdr_set_decimation, handle, factor);
    // }

    std::cout << "created handle\n";

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
        if (PXCALL_DIRECT_NOTHROW(ctx_, lpcsdr_close_device, handle_) < 0) {
            // this can happen if e.g. the underlying device is still busy in another thread
            // this is bad, because
            //  (a) we will leak the handle and
            //  (b) we can't safely free the context and must leak it
            // so yell about it a bit
            SoapySDR::logf(SOAPY_SDR_CRITICAL, "SoapyLPCSDR: LPCSDRDevice destructor could not clean up properly - resources leaked");
            ctx_.Release(); // leak the context to avoid freeing it while still in use
        }
    }
}

LPCSDRDevice::LPCSDRDevice(Context &&ctx, lpcsdr_device_handle *handle) : ctx_(std::move(ctx)), handle_(handle)
{
    // PXCALL(pxsdr_set_sampling_mode, PXSDR_SAMPLING_MODE_COMPLEX_BASEBAND, PXSDR_SAMPLE_FORMAT_INT16);

    // unsigned quantum;
    // PXCALL(pxsdr_get_buffer_quantum, &quantum);
    // PXCALL(pxsdr_set_buffering, 8, quantum * 64); /* default to about 1MB per buffer */

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

// std::string PXSDRDevice::getHardwareKey(void) const { return pxsdr_device_variant_string(pxsdr_get_device_info(handle_)->variant); }

static inline void CheckChannel(const int direction, const size_t channel)
{
    if (direction != SOAPY_SDR_RX)
        throw std::invalid_argument("direction must be SOAPY_SDR_RX");
    if (channel != 0)
        throw std::invalid_argument("channel out of range");
}



void LPCSDRDevice::setSampleRate(const int direction, const size_t channel, const double rate) {
    TRACECALLF("(Setting ADC clock target frequency %u)", rate);
    CheckChannel(direction, channel);
    std::cout << "starting tarnsfer\n";
    int resp = lpcsdr_start_transfer(handle_, (int) rate);
    // lpcsdr_comms_check(handle_->usb_handle);
    std::cout << resp;
    sample_frequency = rate;
}

double LPCSDRDevice::getSampleRate(const int direction, const size_t channel) const {
    TRACECALLF("(Getting ADC clock target frequency %u)", sample_frequency);
    CheckChannel(direction, channel);
    return sample_frequency;
}

void LPCSDRDevice::writeSetting(const std::string &key, const std::string &value)
{
    TRACECALLF("(\"%s\",\"%s\")", key.c_str(), value.c_str());
    if (key == "buffers") {
        lpcsdr_set_buffering(handle_, 2, 10212 + 20);
        // PXCALL(lpcsdr_set_buffering, handle_, buffers, buffersize);
        return;
    }
    throw std::invalid_argument("unrecognized setting " + key);
}

std::string LPCSDRDevice::readSetting(const std::string &key) const
{
    if (key == "buffers") {
        unsigned buffer;
        unsigned buffersize;
        lpcsdr_get_buffering(handle_, &buffer, &buffersize);
        std::stringstream ss;
        ss << std::to_string(buffer) + ", " + std::to_string(buffersize) ;
        return ss.str();
    }
    throw std::invalid_argument("unrecognized setting " + key);
}

}; // namespace LPCSDR
}; // namespace SoapySDR