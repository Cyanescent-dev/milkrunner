#pragma once

#include "AudioInput.h"

#include <QStringList>

#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>

#include <memory>
#endif

class QtAudioInput final : public AudioInput
{
    Q_OBJECT

public:
    explicit QtAudioInput(const QString& deviceName = QString(), QObject* parent = nullptr);

    bool start() override;
    void stop() override;
    QString name() const override;
    static QStringList availableInputDeviceNames();
    static QString preferredInputDeviceName(const QString& savedDeviceName = QString());

private:
#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
    void readAvailableAudio();
    bool configureFormat(const QAudioDevice& inputDevice);

    QString requestedDeviceName_;
    QString activeDeviceName_;
    QAudioFormat format_;
    std::unique_ptr<QAudioSource> source_;
    QIODevice* device_ = nullptr;
#else
    QString requestedDeviceName_;
#endif
};
