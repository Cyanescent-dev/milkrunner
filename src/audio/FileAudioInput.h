#pragma once
#include "BufferedAudioInput.h"
#include <atomic>
#include <condition_variable>
#include <thread>
class FileAudioInput : public BufferedAudioInput
{
    Q_OBJECT
  public:
    FileAudioInput(QString path, bool loop, QObject* parent = nullptr);
    ~FileAudioInput() override;
    bool start() override;
    void stop() override;
    QString name() const override
    {
        return QStringLiteral("File Audio");
    }
    static bool isSupported();

  private:
    void decode();
    QString path_;
    bool loop_;
    std::atomic<bool> stopping_{false};
    std::thread worker_;
    std::mutex waitMutex_;
    std::condition_variable wake_;
};
