#pragma once

#include <QMutex>
#include <QQuickFramebufferObject>
#include <QString>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class OpenGlVisualizerItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(float audioLevel READ audioLevel WRITE setAudioLevel NOTIFY visualStateChanged)
    Q_PROPERTY(float renderScale READ renderScale WRITE setRenderScale NOTIFY visualStateChanged)
    Q_PROPERTY(bool lowPowerMode READ lowPowerMode WRITE setLowPowerMode NOTIFY visualStateChanged)
    Q_PROPERTY(QString presetPath READ presetPath WRITE setPresetPath NOTIFY visualStateChanged)
    Q_PROPERTY(QString textureRoot READ textureRoot WRITE setTextureRoot NOTIFY visualStateChanged)
    Q_PROPERTY(float presetDurationSeconds READ presetDurationSeconds WRITE setPresetDurationSeconds NOTIFY visualStateChanged)
    Q_PROPERTY(float transitionDurationSeconds READ transitionDurationSeconds WRITE setTransitionDurationSeconds NOTIFY visualStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString diagnosticsText READ diagnosticsText NOTIFY diagnosticsTextChanged)

public:
    explicit OpenGlVisualizerItem(QQuickItem* parent = nullptr);

    Renderer* createRenderer() const override;

    float audioLevel() const;
    float renderScale() const;
    bool lowPowerMode() const;
    QString presetPath() const;
    QString textureRoot() const;
    float presetDurationSeconds() const;
    float transitionDurationSeconds() const;
    QString statusMessage() const;
    QString diagnosticsText() const;
    bool takePendingAudio(QVector<float>& pcm, int& sampleRate, int& channels);

public slots:
    void submitAudioBlock(const QVector<float>& pcm, int sampleRate, int channels);
    void setAudioLevel(float level);
    void setRenderScale(float scale);
    void setLowPowerMode(bool enabled);
    void setPresetPath(const QString& path);
    void setTextureRoot(const QString& path);
    void setPresetDurationSeconds(float seconds);
    void setTransitionDurationSeconds(float seconds);
    void setStatusMessage(const QString& message);
    void setDiagnosticsText(const QString& text);

signals:
    void visualStateChanged();
    void statusMessageChanged();
    void diagnosticsTextChanged();
    void presetFailed(const QString& presetPath, const QString& message);
    void presetRenderConfirmed(const QString& presetPath, const QString& message);

private:
    float audioLevel_ = 0.0f;
    float renderScale_ = 1.0f;
    bool lowPowerMode_ = false;
    QString presetPath_;
    QString textureRoot_;
    QString statusMessage_;
    QString diagnosticsText_;
    float presetDurationSeconds_ = 30.0f;
    float transitionDurationSeconds_ = 3.0f;
    mutable QMutex audioMutex_;
    QVector<float> pendingAudioPcm_;
    int pendingAudioSampleRate_ = 0;
    int pendingAudioChannels_ = 0;
    bool pendingAudioDirty_ = false;
};
