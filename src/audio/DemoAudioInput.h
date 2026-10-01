#pragma once

#include "AudioInput.h"

#include <QTimer>

class DemoAudioInput final : public AudioInput
{
    Q_OBJECT

public:
    explicit DemoAudioInput(QObject* parent = nullptr);

    bool start() override;
    void stop() override;
    QString name() const override;

private:
    void generateBlock();

private:
    QTimer timer_;
    bool running_ = false;
    double phase_ = 0.0;
    double wobblePhase_ = 0.0;
    double highPhase_ = 0.0;
    double midPhase_ = 0.0;

    qint64 startTime_ = 0;
    qint64 generatedFrames_ = 0;
};
