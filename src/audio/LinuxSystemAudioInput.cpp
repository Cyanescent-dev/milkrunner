#include "LinuxSystemAudioInput.h"
#ifdef MILK_RUNNER_HAS_PULSE
#include <pulse/pulseaudio.h>
struct LinuxSystemAudioInput::Impl
{
    LinuxSystemAudioInput* owner;
    pa_threaded_mainloop* loop = nullptr;
    pa_context* context = nullptr;
    pa_stream* stream = nullptr;
    QString monitor;
    QStringList names;
    bool started = false;
    explicit Impl(LinuxSystemAudioInput* o) : owner(o)
    {
    }
    static void release(pa_operation* op)
    {
        if (op)
            pa_operation_unref(op);
    }
    void discover()
    {
        if (pa_context_get_state(context) != PA_CONTEXT_READY)
            return;
        {
            std::lock_guard lock(owner->mutex_);
            names.clear();
        }
        release(pa_context_get_source_info_list(
            context,
            [](pa_context*, const pa_source_info* i, int, void* p) {
                auto* s = static_cast<Impl*>(p);
                if (i && i->monitor_of_sink != PA_INVALID_INDEX)
                {
                    std::lock_guard lock(s->owner->mutex_);
                    s->names.append(QString::fromUtf8(i->name));
                }
            },
            this));
        if (owner->device_ != "default")
        {
            attach(owner->device_);
            return;
        }
        release(pa_context_get_server_info(
            context,
            [](pa_context* c, const pa_server_info* info, void* p) {
                if (!info || !info->default_sink_name)
                {
                    static_cast<Impl*>(p)->owner->report(
                        "unavailable", "Audio server has no default output monitor");
                    return;
                }
                release(pa_context_get_sink_info_by_name(
                    c, info->default_sink_name,
                    [](pa_context*, const pa_sink_info* sink, int, void* q) {
                        if (sink && sink->monitor_source_name)
                            static_cast<Impl*>(q)->attach(
                                QString::fromUtf8(sink->monitor_source_name));
                    },
                    p));
            },
            this));
    }
    void attach(const QString& name)
    {
        if (stream && monitor == name && pa_stream_get_state(stream) == PA_STREAM_READY)
            return;
        if (stream)
        {
            pa_stream_set_state_callback(stream, nullptr, nullptr);
            pa_stream_set_read_callback(stream, nullptr, nullptr);
            pa_stream_disconnect(stream);
            pa_stream_unref(stream);
            stream = nullptr;
        }
        monitor = name;
        pa_sample_spec spec{PA_SAMPLE_FLOAT32NE, 44100, 2};
        stream = pa_stream_new(context, "MilkRunner output monitor", &spec, nullptr);
        if (!stream)
        {
            owner->report("error", "Cannot allocate monitor stream");
            return;
        }
        pa_stream_set_state_callback(
            stream,
            [](pa_stream* st, void* p) {
                auto* s = static_cast<Impl*>(p);
                auto state = pa_stream_get_state(st);
                if (state == PA_STREAM_READY)
                {
                    s->owner->report("active");
                    std::lock_guard lock(s->owner->mutex_);
                    s->owner->metrics_["monitor_device"] = s->monitor;
                }
                if (state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED)
                    s->owner->report("unavailable",
                                     "Monitor unavailable: " + QString::fromUtf8(pa_strerror(
                                                                   pa_context_errno(s->context))));
            },
            this);
        pa_stream_set_read_callback(
            stream,
            [](pa_stream* st, size_t, void* p) {
                auto* s = static_cast<Impl*>(p);
                const void* data = nullptr;
                size_t size = 0;
                if (pa_stream_peek(st, &data, &size) < 0)
                {
                    s->owner->report("error", "Monitor read failed");
                    return;
                }
                if (size)
                {
                    if (data)
                        s->owner->publish(static_cast<const float*>(data),
                                          size / (sizeof(float) * 2));
                    else
                    {
                        std::lock_guard lock(s->owner->mutex_);
                        s->owner->metrics_["underruns"] =
                            s->owner->metrics_["underruns"].toInt() + 1;
                    }
                    pa_stream_drop(st);
                }
            },
            this);
        pa_buffer_attr attr{uint32_t(-1), uint32_t(-1), uint32_t(-1), uint32_t(-1),
                            441 * 2 * sizeof(float)};
        if (pa_stream_connect_record(stream, name.toUtf8().constData(), &attr,
                                     PA_STREAM_ADJUST_LATENCY) < 0)
            owner->report("unavailable", "Cannot connect output monitor");
    }
    bool connect()
    {
        loop = pa_threaded_mainloop_new();
        if (!loop)
            return false;
        context = pa_context_new(pa_threaded_mainloop_get_api(loop), "MilkRunner");
        if (!context)
            return false;
        pa_context_set_state_callback(
            context,
            [](pa_context* c, void* p) {
                auto* s = static_cast<Impl*>(p);
                auto state = pa_context_get_state(c);
                if (state == PA_CONTEXT_READY)
                {
                    pa_context_set_subscribe_callback(
                        c,
                        [](pa_context*, pa_subscription_event_type_t, uint32_t, void* q) {
                            static_cast<Impl*>(q)->discover();
                        },
                        s);
                    release(
                        pa_context_subscribe(c,
                                             pa_subscription_mask_t(PA_SUBSCRIPTION_MASK_SERVER |
                                                                    PA_SUBSCRIPTION_MASK_SINK |
                                                                    PA_SUBSCRIPTION_MASK_SOURCE),
                                             nullptr, nullptr));
                    s->discover();
                }
                if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED)
                    s->owner->report("unavailable",
                                     "PulseAudio/PipeWire server unavailable: " +
                                         QString::fromUtf8(pa_strerror(pa_context_errno(c))));
            },
            this);
        if (pa_context_connect(context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) < 0)
            return false;
        started = pa_threaded_mainloop_start(loop) >= 0;
        return started;
    }
    bool failed()
    {
        if (!started || !context)
            return true;
        pa_threaded_mainloop_lock(loop);
        const bool bad = pa_context_get_state(context) == PA_CONTEXT_FAILED ||
                         pa_context_get_state(context) == PA_CONTEXT_TERMINATED;
        pa_threaded_mainloop_unlock(loop);
        return bad;
    }
    ~Impl()
    {
        if (started)
            pa_threaded_mainloop_stop(loop);
        if (stream)
        {
            pa_stream_set_state_callback(stream, nullptr, nullptr);
            pa_stream_set_read_callback(stream, nullptr, nullptr);
            pa_stream_disconnect(stream);
            pa_stream_unref(stream);
        }
        if (context)
        {
            pa_context_set_state_callback(context, nullptr, nullptr);
            pa_context_set_subscribe_callback(context, nullptr, nullptr);
            pa_context_disconnect(context);
            pa_context_unref(context);
        }
        if (loop)
            pa_threaded_mainloop_free(loop);
    }
};
#else
struct LinuxSystemAudioInput::Impl
{
    explicit Impl(LinuxSystemAudioInput*)
    {
    }
    bool connect()
    {
        return false;
    }
};
#endif
LinuxSystemAudioInput::LinuxSystemAudioInput(QString device, QObject* parent)
    : BufferedAudioInput(parent), device_(std::move(device))
{
    retry_.setInterval(2000);
    connect(&retry_, &QTimer::timeout, this, [this] {
#ifdef MILK_RUNNER_HAS_PULSE
        if (!impl_ || impl_->failed())
        {
            impl_.reset();
            impl_ = std::make_unique<Impl>(this);
            if (!impl_->connect())
                report("unavailable", "Cannot connect PulseAudio/PipeWire server");
        }
        else
        {
            pa_threaded_mainloop_lock(impl_->loop);
            impl_->discover();
            pa_threaded_mainloop_unlock(impl_->loop);
        }
#endif
    });
}
LinuxSystemAudioInput::~LinuxSystemAudioInput()
{
    stop();
}
bool LinuxSystemAudioInput::isSupported()
{
#ifdef MILK_RUNNER_HAS_PULSE
    return true;
#else
    return false;
#endif
}
bool LinuxSystemAudioInput::start()
{
    stop();
    resetBuffer();
    {
        std::lock_guard lock(mutex_);
        metrics_["audio_source"] = "system";
        metrics_["audio_backend"] = "PulseAudio-compatible";
    }
    if (!isSupported())
    {
        report("unavailable", "Linux monitor capture not enabled in this build");
        emit errorOccurred(diagnostics()["error"].toString());
        return false;
    }
    impl_ = std::make_unique<Impl>(this);
    if (!impl_->connect())
        report("unavailable", "Cannot connect PulseAudio/PipeWire server");
    drainTimer_.start();
    retry_.start();
    return true;
}
void LinuxSystemAudioInput::stop()
{
    retry_.stop();
    drainTimer_.stop();
    impl_.reset();
    setRunning(false);
    report("stopped");
}
QStringList LinuxSystemAudioInput::devices() const
{
#ifdef MILK_RUNNER_HAS_PULSE
    std::lock_guard lock(mutex_);
    return impl_ ? impl_->names : QStringList{};
#else
    return {};
#endif
}
