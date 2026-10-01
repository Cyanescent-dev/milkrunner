#include "QtAudioInput.h"

#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
#include <QAudioDevice>
#include <QByteArray>
#include <QMediaDevices>
#include <QStringList>

#include <algorithm>
#include <cstring>
#endif

QtAudioInput::QtAudioInput(const QString& deviceName, QObject* parent)
    : AudioInput(parent)
    , requestedDeviceName_(deviceName)
{
}

bool QtAudioInput::start()
{
#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
    if (isRunning()) {
        return true;
    }

    // TODO(android/ios): add explicit permission and lifecycle handling around capture start/stop.
    QAudioDevice inputDevice;
    const QList<QAudioDevice> inputDevices = QMediaDevices::audioInputs();
    for (const QAudioDevice& candidate : inputDevices) {
        if (candidate.description() == requestedDeviceName_) {
            inputDevice = candidate;
            break;
        }
    }

    if (inputDevice.isNull()) {
        inputDevice = QMediaDevices::defaultAudioInput();
    }
    if (inputDevice.isNull()) {
        emit errorOccurred(QStringLiteral("No audio input device is available."));
        return false;
    }
    activeDeviceName_ = inputDevice.description();

    if (!configureFormat(inputDevice)) {
        emit errorOccurred(QStringLiteral("No supported audio input format is available for %1.").arg(activeDeviceName_));
        return false;
    }

    source_ = std::make_unique<QAudioSource>(inputDevice, format_);
    device_ = source_->start();
    if (!device_) {
        source_.reset();
        emit errorOccurred(QStringLiteral("Could not start audio input: %1.").arg(activeDeviceName_));
        return false;
    }

    connect(device_, &QIODevice::readyRead, this, &QtAudioInput::readAvailableAudio);
    setRunning(true);
    return true;
#else
    emit errorOccurred(QStringLiteral("Qt Multimedia is not available in this build; using demo audio instead."));
    return false;
#endif
}

void QtAudioInput::stop()
{
#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
    if (source_) {
        source_->stop();
    }
    source_.reset();
    device_ = nullptr;
#endif
    setRunning(false);
}

QString QtAudioInput::name() const
{
#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
    return activeDeviceName_.isEmpty()
        ? QStringLiteral("Qt audio input")
        : QStringLiteral("Input: %1").arg(activeDeviceName_);
#else
    return QStringLiteral("Qt audio input unavailable");
#endif
}

QStringList QtAudioInput::availableInputDeviceNames()
{
#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
    QStringList names;
    const QList<QAudioDevice> inputDevices = QMediaDevices::audioInputs();
    for (const QAudioDevice& device : inputDevices) {
        if (!device.isNull() && !device.description().isEmpty() && !names.contains(device.description())) {
            names.append(device.description());
        }
    }
    names.sort(Qt::CaseInsensitive);
    return names;
#else
    return {};
#endif
}

QString QtAudioInput::preferredInputDeviceName(const QString& savedDeviceName)
{
    const QStringList names = availableInputDeviceNames();
    if (!savedDeviceName.isEmpty() && names.contains(savedDeviceName)) {
        return savedDeviceName;
    }
    for (const QString& name : names) {
        if (name.contains(QStringLiteral("blackhole"), Qt::CaseInsensitive)) {
            return name;
        }
    }
    return names.isEmpty() ? QString() : names.first();
}

#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
void QtAudioInput::readAvailableAudio()
{
    if (!device_) {
        return;
    }

    const QByteArray bytes = device_->readAll();
    const int channels = std::max(1, format_.channelCount());
    QVector<float> pcm;

    if (format_.sampleFormat() == QAudioFormat::Float) {
        const int sampleCount = bytes.size() / static_cast<int>(sizeof(float));
        pcm.resize(sampleCount);
        std::memcpy(pcm.data(), bytes.constData(), static_cast<std::size_t>(sampleCount) * sizeof(float));
    } else if (format_.sampleFormat() == QAudioFormat::Int16) {
        const int sampleCount = bytes.size() / static_cast<int>(sizeof(qint16));
        pcm.resize(sampleCount);
        const auto* input = reinterpret_cast<const qint16*>(bytes.constData());
        for (int i = 0; i < sampleCount; ++i) {
            pcm[i] = static_cast<float>(input[i]) / 32768.0f;
        }
    } else if (format_.sampleFormat() == QAudioFormat::UInt8) {
        const int sampleCount = bytes.size();
        pcm.resize(sampleCount);
        const auto* input = reinterpret_cast<const quint8*>(bytes.constData());
        for (int i = 0; i < sampleCount; ++i) {
            pcm[i] = (static_cast<float>(input[i]) - 128.0f) / 128.0f;
        }
    }

    if (!pcm.isEmpty()) {
        emit audioReady(pcm, format_.sampleRate(), channels);
    }
}

bool QtAudioInput::configureFormat(const QAudioDevice& inputDevice)
{
    format_.setSampleRate(44100);
    format_.setChannelCount(2);
    format_.setSampleFormat(QAudioFormat::Float);
    if (inputDevice.isFormatSupported(format_)) {
        return true;
    }

    format_.setSampleFormat(QAudioFormat::Int16);
    if (inputDevice.isFormatSupported(format_)) {
        return true;
    }

    format_ = inputDevice.preferredFormat();
    return format_.isValid()
        && format_.channelCount() > 0
        && format_.sampleRate() > 0
        && (format_.sampleFormat() == QAudioFormat::Float
            || format_.sampleFormat() == QAudioFormat::Int16
            || format_.sampleFormat() == QAudioFormat::UInt8);
}
#endif
