#pragma once
#include "AudioInput.h"
#include <QJsonObject>
#include <QTimer>
#include <mutex>

// One second of PCM maximum; no per-block cross-thread Qt event backlog.
class BufferedAudioInput : public AudioInput
{
    Q_OBJECT
  public:
    explicit BufferedAudioInput(QObject* parent = nullptr);
    QJsonObject diagnostics() const override;

  protected:
    void publish(const float* samples, int frames);
    void report(const QString& status, const QString& error = {});
    void resetBuffer();
    mutable std::mutex mutex_;
    QJsonObject metrics_;
    QTimer drainTimer_;

  private:
    QVector<float> pending_;
    QString deliveredError_;
};
