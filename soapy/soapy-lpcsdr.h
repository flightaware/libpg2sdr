// -*- c++ -*-
#pragma once

#ifndef SOAPY_PG2SDR_H
#define SOAPY_PG2SDR_H

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.hpp>
#include <pg2sdr.h>

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <iostream>
#include <optional>
#include <cstddef>

namespace PG2SDR {

static inline std::string pg2sdr_strerror_string(int error) {
    char buf[1024];
    return std::string(pg2sdr_strerror_r(error, buf, sizeof(buf)));
}

// RAII helper around pxsdr_context
class Context
{
  public:
    Context(Context &&from) : error_(std::move(from.error_))
    {
        ctx_ = from.ctx_;
        from.ctx_ = NULL;
    }

    ~Context()
    {
        if (ctx_)
            pg2sdr_exit(ctx_);
    }

    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    operator bool() { return (ctx_ != nullptr); }

    operator pg2sdr_context *() { return ctx_; }

    const std::string &Error() const { return error_; }

    static void Log(pg2sdr_context *ctx, pg2sdr_log_level level, const char *message)
    {
        (void) ctx; /* unused */

        SoapySDR::LogLevel ll;
        switch (level) {
        case PG2SDR_LOG_INFO:
            ll = SOAPY_SDR_INFO;
            break;
        case PG2SDR_LOG_ERROR:
            ll = SOAPY_SDR_ERROR;
            break;
        default:
            ll = SOAPY_SDR_DEBUG;
            break;
        }

        SoapySDR::logf(ll, "libpg2sdr: %s", message);
    }

    void Release() { ctx_ = nullptr; }

    static Context Make()
    {
        pg2sdr_context *newctx;
        int error;

        if ((error = pg2sdr_init(&newctx)) < 0) {
            
            return Context("pg2sdr_init: " + pg2sdr_strerror_string(error));
        }

        if ((error = pg2sdr_set_log_callback(newctx, &Context::Log)) < 0) {
            auto errormsg = "pg2sdr_set_log_callback: " + pg2sdr_strerror_string(error);
            pg2sdr_exit(newctx);
            return Context(errormsg);
        }

        return Context(newctx);
    }

  private:
    Context(std::string error) : ctx_(nullptr), error_(error) {}

    Context(pg2sdr_context *ctx) : ctx_(ctx), error_() {}

    pg2sdr_context *ctx_;
    std::string error_;
};

// RAII / container helper around pxsdr_usb_device arrays returned by
// pxsdr_discover_devices()
class DeviceList
{
  public:
    DeviceList(DeviceList &&from) : list_(from.list_), size_(from.size_)
    {
        from.list_ = NULL;
        from.size_ = 0;
    }

    ~DeviceList()
    {
        if (list_)
            pg2sdr_free_device_list(list_);
    }

    DeviceList(const Context &) = delete;
    DeviceList &operator=(const DeviceList &) = delete;

    static DeviceList Enumerate(Context &ctx, bool allow_bootloader)
    {
        int error;
        lpc_device **newlist;

        if ((error = pg2sdr_discover_devices(ctx, &newlist, allow_bootloader)) < 0) {
            SoapySDR::log(SOAPY_SDR_ERROR, "PG2SDR: could not enumerate PG2SDR devices: " + pg2sdr_strerror_string(error));
            return DeviceList(nullptr);
        }

        return DeviceList(newlist);
    }

    // InputIterator
    lpc_device **begin() { return list_; }

    lpc_device **end() { return list_ + size_; }

    lpc_device *operator[](size_t index)
    {
        if (index >= size_)
            throw std::out_of_range("device index out of range");
        return list_[index];
    }

    size_t size() const { return size_; }

    bool empty() const { return (size_ == 0); }

  private:
    DeviceList(lpc_device **list) : list_(list), size_(0)
    {
        if (list_) {
            while (list_[size_])
                ++size_;
        }
    }

    lpc_device **list_;
    size_t size_;
};

class PG2SDRStream;

class PG2SDRDevice : public SoapySDR::Device
{
  public:
    /* discovery */
    static SoapySDR::KwargsList FindDevices(const SoapySDR::Kwargs &kwargs);
    static SoapySDR::Device *MakeDevice(const SoapySDR::Kwargs &kwargs);

    PG2SDRDevice(PG2SDRDevice &&from);
    virtual ~PG2SDRDevice();

    PG2SDRDevice(const PG2SDRDevice &) = delete;
    PG2SDRDevice &operator=(const PG2SDRDevice &) = delete;

    /* identification */
    std::string getDriverKey(void) const override;
    std::string getHardwareKey(void) const override;

    /* settings */
    SoapySDR::ArgInfoList getSettingInfo(void) const override;
    void writeSetting(const std::string &key, const std::string &value) override;
    std::string readSetting(const std::string &key) const override;

    /* stream */
    size_t getNumChannels(const int direction) const override;
    std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const override;
    std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const override;
    SoapySDR::Stream *setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args) override;
    void closeStream(SoapySDR::Stream *stream) override;
    size_t getStreamMTU(SoapySDR::Stream *stream) const override;
    int activateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs, const size_t numElems) override;
    int deactivateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0) override;
    int readStream(SoapySDR::Stream *stream, void *const *buffs, const size_t numElems, int &flags, long long &timeNs, const long timeoutUs) override;

    /* gains */
    std::vector<std::string> listGains(const int direction, const size_t channel) const override;
    void setGain(const int direction, const size_t channel, const double value) override;
    void setGain(const int direction, const size_t channel, const std::string &name, const double value) override;
    double getGain(const int direction, const size_t channel) const override;
    double getGain(const int direction, const size_t channel, const std::string &name) const override;
    SoapySDR::Range getGainRange(const int direction, const size_t channel) const override;
    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const override;

    /* frequency */
    void setFrequency(const int direction, const size_t channel, const double frequency, const SoapySDR::Kwargs &args) override;
    void setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args) override;
    double getFrequency(const int direction, const size_t channel) const override;
    double getFrequency(const int direction, const size_t channel, const std::string &name) const override;
    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const override;
    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel) const override;
    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const override;

    /* sample rate */
    void setSampleRate(const int direction, const size_t channel, const double rate) override;
    double getSampleRate(const int direction, const size_t channel) const override;
    std::vector<double> listSampleRates(const int direction, const size_t channel) const override;
    SoapySDR::RangeList getSampleRateRange(const int direction, const size_t channel) const override;

    /* bandwidth */
    void setBandwidth(const int direction, const size_t channel, const double bw) override;
    double getBandwidth(const int direction, const size_t channel) const override;
    std::vector<double> listBandwidths(const int direction, const size_t channel) const override;
    SoapySDR::RangeList getBandwidthRange(const int direction, const size_t channel) const override;

    Context &context() { return ctx_; }
    pg2sdr_device_handle *handle() { return handle_; }

  private:
    PG2SDRDevice(Context &&ctx, pg2sdr_device_handle *handle);

    void tryApplyChanges() const;

    enum class GainElementMode { INDIVIDUAL, TOTAL, BOTH };
    enum class SidebandMode { LOWER, UPPER, AUTO };

    mutable std::mutex mutex_;                      // protects active_stream_
    mutable PG2SDRStream *active_stream_ = nullptr; // currently activated PG2SDRStream

    mutable Context ctx_;
    mutable pg2sdr_device_handle *handle_;

    // how are we advertising gain stages?
    GainElementMode gain_element_mode_;

    // computed gain ranges from gain table contents
    SoapySDR::Range lna_range_;
    SoapySDR::Range mix_range_;
    SoapySDR::Range vga_range_;
    SoapySDR::Range total_gain_range_;

    // requested bandwidth (0 = not set)
    double bandwidth_;

    // sideband mode
    SidebandMode sideband_mode_;

    friend class PauseStreamGuard;
};

/* The implementation hiding behind our opaque Stream* handle */
class PG2SDRStream
{
public:
    PG2SDRStream(PG2SDRDevice &dev, const std::string &format, std::size_t queue_limit);
    ~PG2SDRStream();

    // not copyable
    PG2SDRStream(const PG2SDRDevice &) = delete;
    PG2SDRStream &operator=(const PG2SDRDevice &) = delete;

    size_t getMTU() const;
    int activate();
    int deactivate();
    int read(void *out, const size_t numElems, int &flags, long long &timeNs, const long timeoutUs);

private:
    // A unique_ptr for pg2sdr_sample_buffer that frees buffers via the libpg2sdr API
    typedef std::unique_ptr<pg2sdr_sample_buffer, decltype(&pg2sdr_release_buffer)> BufferPtr;

    // Items contained in queue_ and pending_
    struct QueueItem {
        BufferPtr buffer;
        std::size_t offset;
        int error;

        unsigned size() const { return buffer ? buffer->count : 0; }

        QueueItem(pg2sdr_sample_buffer *buf) : buffer(buf, &pg2sdr_release_buffer), offset(0), error(0) {}
        QueueItem(int err) : buffer(nullptr, &pg2sdr_release_buffer), offset(0), error(err) {}
    };

    // Streaming thread entry point
    void StreamingWorker();

    // Callback implementation for pg2sdr_stream_data
    static bool StreamCallback(pg2sdr_sample_buffer *buffer, void *user_data);    // user_data points to the owning PG2SDRStream
    bool StreamCallback(pg2sdr_sample_buffer *buffer);

    PG2SDRDevice &dev_;

    void (*convert_)(void *, const void *, std::size_t);   // converter for provided int16_t samples -> requested format
    std::size_t bytes_per_sample_;         // sizeof(requested format)

    double sample_rate_;                   // configured sample rate for this stream (needed to derive timestamps)
    std::size_t queue_limit_;              // maximum # of samples we're willing to queue before dropping data

    std::unique_ptr<std::thread> thread_;  // currently running streaming thread, if any

    std::mutex queue_mutex_;               // protects queue_, queue_signal_, queue_size_
    std::condition_variable queue_signal_; // notified when an item is added to the queue
    std::list<QueueItem> queue_;           // queue of pending buffers/errors
    std::size_t queue_size_;               // sum of buffer->count_ for all items in queue_
    bool please_stop_;                     // flag that makes the worker thread exit (needed to avoid activate->deactivate races)

    std::optional<QueueItem> pending_;     // Next partially processed queue item waiting to be consumed

    std::uint64_t expected_timestamp_;
};

}; // namespace PG2SDR

#endif /* SOAPY_PG2SDR_H */
