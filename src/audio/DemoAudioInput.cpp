#include "DemoAudioInput.h"

#include <QDateTime>
#include <QtMath>
#include <algorithm>
#include <cmath>

DemoAudioInput::DemoAudioInput(QObject* parent)
    : AudioInput(parent)
{
    timer_.setInterval(16);
    connect(&timer_, &QTimer::timeout, this, &DemoAudioInput::generateBlock);
}

bool DemoAudioInput::start()
{
    startTime_ = QDateTime::currentMSecsSinceEpoch();
    generatedFrames_ = 0;
    
    timer_.start();
    setRunning(true);
    return true;
}

void DemoAudioInput::stop()
{
    timer_.stop();
    setRunning(false);
}

QString DemoAudioInput::name() const
{
    return QStringLiteral("Demo audio");
}

void DemoAudioInput::generateBlock()
{
    constexpr int sampleRate = 44100;
    constexpr int channels = 2;
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = now - startTime_;
    qint64 targetFrames = (elapsed * sampleRate) / 1000;
    int frames = targetFrames - generatedFrames_;
    
    if (frames <= 0) return;
    generatedFrames_ = targetFrames;

    QVector<float> pcm;
    pcm.resize(frames * channels);

    const double baseFrequency = 55.0 + 18.0 * qSin(wobblePhase_);
    for (int frame = 0; frame < frames; ++frame) {
        const double beatPhase = std::fmod(wobblePhase_ * 0.62, 2.0 * M_PI);
        const double beatPulse = qExp(-beatPhase * 3.6);
        const double sweep = 0.5 + 0.5 * qSin(wobblePhase_ * 0.23);
        const float kick = static_cast<float>(0.72 * beatPulse * qSin(phase_));
        const float bass = static_cast<float>(0.34 * qSin(phase_ * (1.0 + sweep * 0.12)));
        const float mid = static_cast<float>(0.18 * qSin(midPhase_) * (0.35 + 0.65 * sweep));
        const float high = static_cast<float>(0.08 * qSin(highPhase_) * (0.4 + 0.6 * beatPulse));
        const float left = std::clamp(kick + bass + mid + high, -0.95f, 0.95f);
        const float right = std::clamp(kick * 0.92f + bass * 0.76f - mid * 0.6f + high, -0.95f, 0.95f);
        pcm[frame * 2] = left;
        pcm[frame * 2 + 1] = right;

        phase_ += (2.0 * M_PI * baseFrequency) / sampleRate;
        midPhase_ += (2.0 * M_PI * (220.0 + 90.0 * sweep)) / sampleRate;
        highPhase_ += (2.0 * M_PI * (1800.0 + 700.0 * beatPulse)) / sampleRate;
        wobblePhase_ += (2.0 * M_PI * 1.3) / sampleRate;
    }

    emit audioReady(pcm, sampleRate, channels);
}
