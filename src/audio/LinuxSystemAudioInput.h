#pragma once
#include "BufferedAudioInput.h"
#include <memory>
class LinuxSystemAudioInput : public BufferedAudioInput
{
    Q_OBJECT
  public:
    explicit LinuxSystemAudioInput(QString device = "default", QObject* parent = nullptr);
    ~LinuxSystemAudioInput() override;
    bool start() override;
    void stop() override;
    QString name() const override
    {
        return QStringLiteral("Linux System Audio");
    }
    static bool isSupported();
    QStringList devices() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    QString device_;
    QTimer retry_;
};
