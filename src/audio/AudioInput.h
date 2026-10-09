#pragma once

#include <QObject>
#include <QVector>
#include <QJsonObject>

class AudioInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    explicit AudioInput(QObject* parent = nullptr);
    ~AudioInput() override = default;

    bool isRunning() const;

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual QString name() const = 0;
    virtual QJsonObject diagnostics() const { return {{"audio_source",name()},{"audio_status",isRunning()?"active":"stopped"}}; }

signals:
    void audioReady(const QVector<float>& pcm, int sampleRate, int channels);
    void runningChanged();
    void errorOccurred(const QString& message);

protected:
    void setRunning(bool running);

private:
    bool running_ = false;
};
