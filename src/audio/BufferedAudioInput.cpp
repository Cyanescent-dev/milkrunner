#include "BufferedAudioInput.h"
#include <algorithm>
#include <cmath>
BufferedAudioInput::BufferedAudioInput(QObject* parent) : AudioInput(parent)
{
    drainTimer_.setInterval(20);
    connect(&drainTimer_, &QTimer::timeout, this, [this] {
        QVector<float> pcm;
        QString status, error;
        {
            std::lock_guard lock(mutex_);
            pcm.swap(pending_);
            status = metrics_["audio_status"].toString();
            error = metrics_["error"].toString();
        }
        setRunning(status == "active");
        if (!error.isEmpty() && error != deliveredError_)
        {
            deliveredError_ = error;
            emit errorOccurred(error);
        }
        if (!pcm.isEmpty())
            emit audioReady(pcm, 44100, 2);
    });
}
void BufferedAudioInput::resetBuffer()
{
    std::lock_guard lock(mutex_);
    pending_.clear();
    deliveredError_.clear();
    metrics_ = {{"audio_status", "starting"},
                {"sample_rate", 44100},
                {"channels", 2},
                {"pcm_frames_decoded", 0},
                {"pcm_frames_dropped", 0},
                {"underruns", 0},
                {"errors", 0},
                {"rms", 0},
                {"peak", 0}};
}
void BufferedAudioInput::publish(const float* samples, int frames)
{
    if (frames <= 0)
        return;
    double squares = 0, peak = 0;
    for (int i = 0; i < frames * 2; ++i)
    {
        squares += double(samples[i]) * samples[i];
        peak = std::max(peak, std::abs(double(samples[i])));
    }
    std::lock_guard lock(mutex_);
    metrics_["pcm_frames_decoded"] = metrics_["pcm_frames_decoded"].toDouble() + frames;
    const double rms = std::sqrt(squares / (frames * 2));
    metrics_["rms"] = rms;
    metrics_["peak"] = peak;
    metrics_["peak_max"] = std::max(peak, metrics_["peak_max"].toDouble());
    if (rms > 0.00001)
        metrics_["non_silent_frames"] = metrics_["non_silent_frames"].toDouble() + frames;
    metrics_["audio_status"] = "active";
    metrics_["error"] = "";
    const auto before = pending_.size();
    pending_.resize(before + frames * 2);
    std::copy_n(samples, frames * 2, pending_.data() + before);
    const int excess = pending_.size() - 88200;
    if (excess > 0)
    {
        pending_.remove(0, excess);
        metrics_["pcm_frames_dropped"] = metrics_["pcm_frames_dropped"].toDouble() + excess / 2;
    }
}
void BufferedAudioInput::report(const QString& status, const QString& error)
{
    std::lock_guard lock(mutex_);
    metrics_["audio_status"] = status;
    metrics_["error"] = error;
    if (!error.isEmpty())
        metrics_["errors"] = metrics_["errors"].toInt() + 1;
}
QJsonObject BufferedAudioInput::diagnostics() const
{
    std::lock_guard lock(mutex_);
    auto d = metrics_;
    d["source_pending_frames"] = pending_.size() / 2;
    return d;
}
