#pragma once

#include "SettingsStore.h"
#include "playlists/PlaylistManager.h"
#include "presets/PresetLibrary.h"

#include <QObject>
#include <QElapsedTimer>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include <memory>

#include <memory>

class AudioInput;
#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
class QAudioSink;
class QIODevice;
#endif

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PresetLibraryModel* presetLibrary READ presetLibrary CONSTANT)
    Q_PROPERTY(PlaylistManager* playlistManager READ playlistManager CONSTANT)
    Q_PROPERTY(QString presetFolder READ presetFolder NOTIFY presetFolderChanged)
    Q_PROPERTY(int fpsCap READ fpsCap WRITE setFpsCap NOTIFY performanceSettingsChanged)
    Q_PROPERTY(double renderScale READ renderScale WRITE setRenderScale NOTIFY performanceSettingsChanged)
    Q_PROPERTY(bool lowPowerMode READ lowPowerMode WRITE setLowPowerMode NOTIFY performanceSettingsChanged)
    Q_PROPERTY(QString activePresetTitle READ activePresetTitle NOTIFY activePresetTitleChanged)
    Q_PROPERTY(QString activePresetPath READ activePresetPath NOTIFY activePresetChanged)
    Q_PROPERTY(double presetDurationSeconds READ presetDurationSeconds NOTIFY rendererSettingsChanged)
    Q_PROPERTY(double transitionDurationSeconds READ transitionDurationSeconds NOTIFY rendererSettingsChanged)
    Q_PROPERTY(bool playbackPaused READ playbackPaused WRITE setPlaybackPaused NOTIFY playbackStateChanged)
    Q_PROPERTY(bool presetLocked READ presetLocked WRITE setPresetLocked NOTIFY playbackStateChanged)
    Q_PROPERTY(double presetCycleProgress READ presetCycleProgress NOTIFY playbackStateChanged)
    Q_PROPERTY(int presetRemainingSeconds READ presetRemainingSeconds NOTIFY playbackStateChanged)
    Q_PROPERTY(QString presetCycleStatusText READ presetCycleStatusText NOTIFY playbackStateChanged)
    Q_PROPERTY(QString currentPresetPosition READ currentPresetPosition NOTIFY activePresetChanged)
    Q_PROPERTY(QString rendererName READ rendererName CONSTANT)
    Q_PROPERTY(float audioLevel READ audioLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(float rawAudioLevel READ rawAudioLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(float audioPeakLevel READ audioPeakLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(double audioGain READ audioGain WRITE setAudioGain NOTIFY audioSettingsChanged)
    Q_PROPERTY(QStringList audioInputDevices READ audioInputDevices NOTIFY audioDevicesChanged)
    Q_PROPERTY(QString selectedAudioInputDevice READ selectedAudioInputDevice WRITE setSelectedAudioInputDevice NOTIFY audioDevicesChanged)
    Q_PROPERTY(QString audioSourceName READ audioSourceName NOTIFY audioSourceChanged)
    Q_PROPERTY(QString audioStatusMessage READ audioStatusMessage NOTIFY audioSourceChanged)
    Q_PROPERTY(bool audioRunning READ audioRunning NOTIFY audioSourceChanged)
    Q_PROPERTY(bool playAudioOutput READ playAudioOutput WRITE setPlayAudioOutput NOTIFY audioSettingsChanged)
    Q_PROPERTY(QVariantList scionEnabledSynthModes READ scionEnabledSynthModes NOTIFY scionAudioModeChanged)
    Q_PROPERTY(QVariantList scionAudioModes READ scionAudioModes CONSTANT)
    Q_PROPERTY(QString configFolder READ configFolder CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    PresetLibraryModel* presetLibrary() const;
    PlaylistManager* playlistManager() const;
    QString presetFolder() const;
    int fpsCap() const;
    double renderScale() const;
    bool lowPowerMode() const;
    QString activePresetTitle() const;
    QString activePresetPath() const;
    double presetDurationSeconds() const;
    double transitionDurationSeconds() const;
    bool playbackPaused() const;
    bool presetLocked() const;
    double presetCycleProgress() const;
    int presetRemainingSeconds() const;
    QString presetCycleStatusText() const;
    QString currentPresetPosition() const;
    QString rendererName() const;
    float audioLevel() const;
    float rawAudioLevel() const;
    float audioPeakLevel() const;
    double audioGain() const;
    QStringList audioInputDevices() const;
    QString selectedAudioInputDevice() const;
    QString audioSourceName() const;
    QString audioStatusMessage() const;
    bool audioRunning() const;
    bool playAudioOutput() const;
    QVariantList scionEnabledSynthModes() const;
    QVariantList scionAudioModes() const;
    QString configFolder() const;

    Q_INVOKABLE bool importPresetFolder(const QString& folderOrUrl);
    Q_INVOKABLE void addPresetToPlaylist(int presetIndex);
    Q_INVOKABLE int addVisiblePresetsToPlaylist();
    Q_INVOKABLE void toggleFavourite(int presetIndex);
    Q_INVOKABLE void playPreset(int presetIndex);
    Q_INVOKABLE void nextPreset();
    Q_INVOKABLE void previousPreset();
    Q_INVOKABLE void randomPreset();
    Q_INVOKABLE void togglePlaybackPaused();
    Q_INVOKABLE void togglePresetLock();
    Q_INVOKABLE void useDemoAudio();
    Q_INVOKABLE void useLiveAudio();
    Q_INVOKABLE void useScionAudio();
    Q_INVOKABLE void useSystemAudio();
    Q_INVOKABLE void refreshAudioInputDevices();
    Q_INVOKABLE void setScionSynthEnabled(int mode, bool enabled);

public slots:
    void handlePresetFailed(const QString& path, const QString& message);
    void handlePresetRenderConfirmed(const QString& path, const QString& message);
    void setFpsCap(int fps);
    void setRenderScale(double scale);
    void setLowPowerMode(bool enabled);
    void setPlaybackPaused(bool paused);
    void setPresetLocked(bool locked);
    void setAudioGain(double gain);
    void setPlayAudioOutput(bool play);
    void setSelectedAudioInputDevice(const QString& deviceName);

signals:
    void presetFolderChanged();
    void performanceSettingsChanged();
    void activePresetTitleChanged();
    void activePresetChanged();
    void rendererSettingsChanged();
    void playbackStateChanged();
    void audioBlockReady(const QVector<float>& pcm, int sampleRate, int channels);
    void audioLevelChanged();
    void audioSettingsChanged();
    void scionAudioModeChanged();
    void audioDevicesChanged();
    void audioSourceChanged();

private:
    bool installAudioInput(std::unique_ptr<AudioInput> input, bool fallbackToDemo);
    void handleAudio(const QVector<float>& pcm, int sampleRate, int channels);
    void handleActivePresetChanged(const QString& path, const QString& title);
    void advancePastFailedPreset();
    bool activePresetIsKnownFailed() const;
    void pruneIgnoredPlaylistEntries();
    void chooseInitialAudioInputDevice();
    QVector<float> prepareAudioBlock(const QVector<float>& pcm, int channels, float& rawRms, float& processedRms, float& processedPeak) const;
    void restartPresetTimer();
    void updatePresetCycleProgress();
    void persistFavourites();
    void restartAudioOutput();

    SettingsStore settings_;
    PresetLibraryModel* presetLibrary_ = nullptr;
    PlaylistManager* playlistManager_ = nullptr;
    std::unique_ptr<AudioInput> audioInput_;
    QTimer presetTimer_;
    QTimer presetProgressTimer_;
    QElapsedTimer presetCycleClock_;
    int fpsCap_ = 60;
    double renderScale_ = 1.0;
    double audioGain_ = 2.0;
    bool lowPowerMode_ = false;
    bool playbackPaused_ = false;
    bool presetLocked_ = false;
    double presetCycleProgress_ = 0.0;
    int presetRemainingSeconds_ = 0;
    QString activePresetTitle_;
    QString activePresetPath_;
    QString audioStatusMessage_;
    QStringList audioInputDevices_;
    QString selectedAudioInputDevice_;
    QSet<QString> failedPresetPaths_;
    float audioLevel_ = 0.0f;
    float rawAudioLevel_ = 0.0f;
    float audioPeakLevel_ = 0.0f;
    bool skippingFailedPreset_ = false;
    bool playAudioOutput_ = false;
#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
    std::unique_ptr<QAudioSink> audioSink_;
    QIODevice* audioOutputDevice_ = nullptr;
#endif
    QByteArray audioWriteBuffer_;
};
