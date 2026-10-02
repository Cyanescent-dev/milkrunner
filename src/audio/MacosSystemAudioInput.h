#pragma once

#include "AudioInput.h"
#include <memory>

class MacosSystemAudioInputPrivate;

class MacosSystemAudioInput : public AudioInput
{
    Q_OBJECT

public:
    explicit MacosSystemAudioInput(QObject* parent = nullptr);
    ~MacosSystemAudioInput() override;

    bool start() override;
    void stop() override;
    QString name() const override;

    static bool isSupported();

private:
    std::unique_ptr<MacosSystemAudioInputPrivate> d_;
};
