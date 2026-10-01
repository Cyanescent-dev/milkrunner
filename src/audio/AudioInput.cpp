#include "AudioInput.h"

AudioInput::AudioInput(QObject* parent)
    : QObject(parent)
{
}

bool AudioInput::isRunning() const
{
    return running_;
}

void AudioInput::setRunning(bool running)
{
    if (running_ == running) {
        return;
    }
    running_ = running;
    emit runningChanged();
}
