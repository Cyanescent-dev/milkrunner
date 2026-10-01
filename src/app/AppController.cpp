#include "AppController.h"

#include "audio/AudioInput.h"
#include "audio/DemoAudioInput.h"
#include "audio/ScionAudioInput.h"
#include "audio/QtAudioInput.h"
#include "audio/MacosSystemAudioInput.h"
#include "presets/PresetScanner.h"

#include <QFileInfo>
#include <QUrl>
#include <QVector>
#include <QtMath>
#include <QtGlobal>

#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#endif

#include <algorithm>
#include <cmath>

AppController::AppController(QObject* parent)
    : QObject(parent)
    , presetLibrary_(new PresetLibraryModel(this))
    , playlistManager_(new PlaylistManager(this))
    , fpsCap_(settings_.fpsCap())
    , renderScale_(settings_.renderScale())
    , audioGain_(settings_.audioGain())
    , lowPowerMode_(settings_.lowPowerMode())
{
    refreshAudioInputDevices();
    chooseInitialAudioInputDevice();
    
    if (selectedAudioInputDevice_ == QStringLiteral("System Audio (Desktop Output)")) {
        useSystemAudio();
    } else if (selectedAudioInputDevice_ == QStringLiteral("Scion MIDI")) {
        useScionAudio();
    } else {
        useLiveAudio();
    }

    presetLibrary_->setFavourites(settings_.favourites());
    connect(presetLibrary_, &PresetLibraryModel::favouritesChanged, this, &AppController::persistFavourites);

    const QString savedPresetFolder = settings_.presetFolder();
    if (!savedPresetFolder.isEmpty()) {
        presetLibrary_->scanFolder(savedPresetFolder);
    }

    connect(playlistManager_, &PlaylistManager::activePresetChanged, this, &AppController::handleActivePresetChanged);
    connect(playlistManager_, &PlaylistManager::activePlaylistChanged, this, [this]() {
        settings_.setLastPlaylistName(playlistManager_->activePlaylistName());
        emit rendererSettingsChanged();
        restartPresetTimer();
    });

    playlistManager_->setStorageFolder(settings_.playlistFolder());
    playlistManager_->setPresetRoot(presetLibrary_->folder());
    
    playAudioOutput_ = settings_.playAudioOutput();
    playlistManager_->loadPlaylists();
    pruneIgnoredPlaylistEntries();

    const QString lastPlaylist = settings_.lastPlaylistName();
    if (!lastPlaylist.isEmpty() && playlistManager_->playlistNames().contains(lastPlaylist)) {
        playlistManager_->setActivePlaylist(lastPlaylist);
    }

    presetTimer_.setSingleShot(true);
    connect(&presetTimer_, &QTimer::timeout, this, &AppController::nextPreset);
    presetProgressTimer_.setInterval(250);
    connect(&presetProgressTimer_, &QTimer::timeout, this, &AppController::updatePresetCycleProgress);
    restartPresetTimer();
}

AppController::~AppController()
{
    if (audioInput_) {
        audioInput_->stop();
    }
}

PresetLibraryModel* AppController::presetLibrary() const
{
    return presetLibrary_;
}

PlaylistManager* AppController::playlistManager() const
{
    return playlistManager_;
}

QString AppController::presetFolder() const
{
    return presetLibrary_->folder();
}

int AppController::fpsCap() const
{
    return fpsCap_;
}

double AppController::renderScale() const
{
    return renderScale_;
}

bool AppController::lowPowerMode() const
{
    return lowPowerMode_;
}

QString AppController::activePresetTitle() const
{
    return activePresetTitle_;
}

QString AppController::activePresetPath() const
{
    return activePresetPath_;
}

double AppController::presetDurationSeconds() const
{
    return playlistManager_->presetDurationSeconds();
}

double AppController::transitionDurationSeconds() const
{
    return playlistManager_->transitionDurationSeconds();
}

bool AppController::playbackPaused() const
{
    return playbackPaused_;
}

bool AppController::presetLocked() const
{
    return presetLocked_;
}

double AppController::presetCycleProgress() const
{
    return presetCycleProgress_;
}

int AppController::presetRemainingSeconds() const
{
    return presetRemainingSeconds_;
}

QString AppController::presetCycleStatusText() const
{
    if (presetLocked_) {
        return QStringLiteral("Locked");
    }
    if (playbackPaused_) {
        return QStringLiteral("Paused");
    }
    if (playlistManager_->rowCount() <= 1) {
        return QStringLiteral("Manual");
    }
    return QStringLiteral("%1s").arg(presetRemainingSeconds_);
}

QString AppController::currentPresetPosition() const
{
    return playlistManager_->currentPositionText();
}

QString AppController::rendererName() const
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    return QStringLiteral("ProjectMRenderer");
#else
    return QStringLiteral("OpenGL fallback renderer");
#endif
}

float AppController::audioLevel() const
{
    return audioLevel_;
}

float AppController::rawAudioLevel() const
{
    return rawAudioLevel_;
}

float AppController::audioPeakLevel() const
{
    return audioPeakLevel_;
}

double AppController::audioGain() const
{
    return audioGain_;
}

QStringList AppController::audioInputDevices() const
{
    return audioInputDevices_;
}

QString AppController::selectedAudioInputDevice() const
{
    return selectedAudioInputDevice_;
}

QString AppController::audioSourceName() const
{
    return audioInput_ ? audioInput_->name() : QStringLiteral("No audio");
}

QString AppController::audioStatusMessage() const
{
    return audioStatusMessage_;
}

bool AppController::audioRunning() const
{
    return audioInput_ && audioInput_->isRunning();
}

bool AppController::playAudioOutput() const
{
    return playAudioOutput_;
}

QVariantList AppController::scionEnabledSynthModes() const
{
    QVariantList modes;
    for (int mode : settings_.scionEnabledSynthModes()) {
        modes.append(mode);
    }
    return modes;
}

QVariantList AppController::scionAudioModes() const
{
    // Keep legacy IDs explicit: Chaotic Feedback Ring is still ID 9, rather
    // than becoming a different synth after the broken modes were removed.
    return {
        QVariantMap{{QStringLiteral("id"), 0}, {QStringLiteral("name"), QStringLiteral("Ethereal Drone")}},
        QVariantMap{{QStringLiteral("id"), 1}, {QStringLiteral("name"), QStringLiteral("Rhythmic Pulses")}},
        QVariantMap{{QStringLiteral("id"), 2}, {QStringLiteral("name"), QStringLiteral("Crystal Chimes")}},
        QVariantMap{{QStringLiteral("id"), 3}, {QStringLiteral("name"), QStringLiteral("Deep Sea Swells")}},
        QVariantMap{{QStringLiteral("id"), 4}, {QStringLiteral("name"), QStringLiteral("Breathing Brass")}},
        QVariantMap{{QStringLiteral("id"), 5}, {QStringLiteral("name"), QStringLiteral("Singing Wind")}},
        QVariantMap{{QStringLiteral("id"), 6}, {QStringLiteral("name"), QStringLiteral("Ghost Strings")}},
        QVariantMap{{QStringLiteral("id"), 7}, {QStringLiteral("name"), QStringLiteral("Neural Sync")}},
        QVariantMap{{QStringLiteral("id"), 9}, {QStringLiteral("name"), QStringLiteral("Chaotic Feedback Ring")}}
    };
}

QString AppController::configFolder() const
{
    return settings_.appConfigFolder();
}

bool AppController::importPresetFolder(const QString& folderOrUrl)
{
    // TODO(android/ios): replace raw folder paths with platform document picker grants.
    if (!presetLibrary_->scanFolder(folderOrUrl)) {
        return false;
    }

    settings_.setPresetFolder(presetLibrary_->folder());
    playlistManager_->setPresetRoot(presetLibrary_->folder());
    playlistManager_->loadPlaylists();
    pruneIgnoredPlaylistEntries();
    emit presetFolderChanged();
    return true;
}

void AppController::addPresetToPlaylist(int presetIndex)
{
    const QVariantMap preset = presetLibrary_->get(presetIndex);
    if (preset.isEmpty()) {
        return;
    }

    playlistManager_->addPreset(
        preset.value(QStringLiteral("path")).toString(),
        preset.value(QStringLiteral("title")).toString(),
        preset.value(QStringLiteral("author")).toString(),
        preset.value(QStringLiteral("favourite")).toBool());
}

int AppController::addVisiblePresetsToPlaylist()
{
    QVector<PlaylistEntry> entries;
    const QList<PresetMetadata> visiblePresets = presetLibrary_->visiblePresets();
    entries.reserve(visiblePresets.size());

    for (const PresetMetadata& preset : visiblePresets) {
        entries.append({preset.path, preset.title, preset.author, preset.favourite});
    }

    return playlistManager_->addPresets(entries);
}

void AppController::toggleFavourite(int presetIndex)
{
    presetLibrary_->toggleFavourite(presetIndex);
}

void AppController::playPreset(int presetIndex)
{
    const QVariantMap preset = presetLibrary_->get(presetIndex);
    if (preset.isEmpty()) {
        return;
    }

    int newIndex = playlistManager_->rowCount();
    playlistManager_->addPreset(
        preset.value(QStringLiteral("path")).toString(),
        preset.value(QStringLiteral("title")).toString(),
        preset.value(QStringLiteral("author")).toString(),
        preset.value(QStringLiteral("favourite")).toBool());

    playlistManager_->playIndex(newIndex);
}

void AppController::nextPreset()
{
    const int presetCount = playlistManager_->rowCount();
    for (int attempt = 0; attempt < std::max(1, presetCount); ++attempt) {
        if (!playlistManager_->nextPreset()) {
            break;
        }
        if (!activePresetIsKnownFailed()) {
            break;
        }
    }
    restartPresetTimer();
}

void AppController::previousPreset()
{
    const int presetCount = playlistManager_->rowCount();
    for (int attempt = 0; attempt < std::max(1, presetCount); ++attempt) {
        if (!playlistManager_->previousPreset()) {
            break;
        }
        if (!activePresetIsKnownFailed()) {
            break;
        }
    }
    restartPresetTimer();
}

void AppController::randomPreset()
{
    const int presetCount = playlistManager_->rowCount();
    for (int attempt = 0; attempt < std::max(1, presetCount); ++attempt) {
        if (!playlistManager_->randomPreset()) {
            break;
        }
        if (!activePresetIsKnownFailed()) {
            break;
        }
    }
    restartPresetTimer();
}

void AppController::togglePlaybackPaused()
{
    setPlaybackPaused(!playbackPaused_);
}

void AppController::togglePresetLock()
{
    setPresetLocked(!presetLocked_);
}

void AppController::useDemoAudio()
{
    installAudioInput(std::make_unique<DemoAudioInput>(), false);
}

void AppController::useSystemAudio()
{
    installAudioInput(std::make_unique<MacosSystemAudioInput>(), true);
}

void AppController::useLiveAudio()
{
    refreshAudioInputDevices();
    if (selectedAudioInputDevice_.isEmpty()) {
        chooseInitialAudioInputDevice();
    }
    installAudioInput(std::make_unique<QtAudioInput>(selectedAudioInputDevice_), false);
}

void AppController::useScionAudio()
{
    auto input = std::make_unique<ScionAudioInput>();
    input->setEnabledSynthModes(settings_.scionEnabledSynthModes());
    installAudioInput(std::move(input), true);
}

void AppController::refreshAudioInputDevices()
{
    const QString previousSelection = selectedAudioInputDevice_;
    audioInputDevices_ = QtAudioInput::availableInputDeviceNames();
    audioInputDevices_.prepend(QStringLiteral("Scion MIDI"));
    if (MacosSystemAudioInput::isSupported()) {
        audioInputDevices_.prepend(QStringLiteral("System Audio (Desktop Output)"));
    }
    
    if (!previousSelection.isEmpty() && audioInputDevices_.contains(previousSelection)) {
        selectedAudioInputDevice_ = previousSelection;
    } else {
        chooseInitialAudioInputDevice();
    }
    emit audioDevicesChanged();
}

void AppController::setFpsCap(int fps)
{
    const int normalized = (fps == 30 || fps == 60) ? fps : 0;
    if (fpsCap_ == normalized) {
        return;
    }
    fpsCap_ = normalized;
    settings_.setFpsCap(fpsCap_);
    emit performanceSettingsChanged();
}

void AppController::setRenderScale(double scale)
{
    const double clamped = qBound(0.5, scale, 1.0);
    if (qFuzzyCompare(renderScale_, clamped)) {
        return;
    }
    renderScale_ = clamped;
    settings_.setRenderScale(renderScale_);
    emit performanceSettingsChanged();
}

void AppController::setLowPowerMode(bool enabled)
{
    if (lowPowerMode_ == enabled) {
        return;
    }
    lowPowerMode_ = enabled;
    settings_.setLowPowerMode(lowPowerMode_);
    if (lowPowerMode_ && fpsCap_ == 0) {
        setFpsCap(30);
    }
    emit performanceSettingsChanged();
}

void AppController::setPlaybackPaused(bool paused)
{
    if (playbackPaused_ == paused) {
        return;
    }
    playbackPaused_ = paused;
    restartPresetTimer();
    emit playbackStateChanged();
}

void AppController::setPresetLocked(bool locked)
{
    if (presetLocked_ == locked) {
        return;
    }
    presetLocked_ = locked;
    restartPresetTimer();
    emit playbackStateChanged();
}

void AppController::setAudioGain(double gain)
{
    const double clamped = qBound(0.25, gain, 8.0);
    if (qFuzzyCompare(audioGain_, clamped)) {
        return;
    }
    audioGain_ = clamped;
    settings_.setAudioGain(audioGain_);
    emit audioSettingsChanged();
}

void AppController::setPlayAudioOutput(bool play)
{
    if (playAudioOutput_ == play) {
        return;
    }
    playAudioOutput_ = play;
    settings_.setPlayAudioOutput(play);
    restartAudioOutput();
    emit audioSettingsChanged();
}

void AppController::setScionSynthEnabled(int mode, bool enabled)
{
    if (!ScionAudioInput::isValidSynthMode(mode)) {
        return;
    }
    QSet<int> modes = settings_.scionEnabledSynthModes();
    if (enabled == modes.contains(mode)) {
        return;
    }
    if (enabled) {
        modes.insert(mode);
    } else {
        modes.remove(mode);
    }
    settings_.setScionEnabledSynthModes(modes);
    
    if (auto* scionInput = dynamic_cast<ScionAudioInput*>(audioInput_.get())) {
        scionInput->setEnabledSynthModes(modes);
    }
    
    emit scionAudioModeChanged();
}

void AppController::setSelectedAudioInputDevice(const QString& deviceName)
{
    if (selectedAudioInputDevice_ == deviceName) {
        return;
    }
    selectedAudioInputDevice_ = deviceName;
    settings_.setAudioInputDeviceName(selectedAudioInputDevice_);
    
    if (selectedAudioInputDevice_ == QStringLiteral("System Audio (Desktop Output)")) {
        useSystemAudio();
    } else if (selectedAudioInputDevice_ == QStringLiteral("Scion MIDI")) {
        useScionAudio();
    } else {
        useLiveAudio();
    }
    
    emit audioDevicesChanged();
}

void AppController::handlePresetFailed(const QString& path, const QString& message)
{
    const QString failedPath = path.isEmpty() ? activePresetPath_ : path;
    if (!failedPath.isEmpty()) {
        failedPresetPaths_.insert(failedPath);
        presetLibrary_->markPresetFailed(failedPath, message);
    }

    if (failedPath.isEmpty() || failedPath == activePresetPath_) {
        advancePastFailedPreset();
    }
}

void AppController::handlePresetRenderConfirmed(const QString& path, const QString& message)
{
    if (!path.isEmpty() && !failedPresetPaths_.contains(path)) {
        presetLibrary_->markPresetReady(path, message);
    }
}

bool AppController::installAudioInput(std::unique_ptr<AudioInput> input, bool fallbackToDemo)
{
    if (!input) {
        return false;
    }

    connect(input.get(), &AudioInput::errorOccurred, this, [this](const QString& message) {
        audioStatusMessage_ = message;
        emit audioSourceChanged();
    });
    connect(input.get(), &AudioInput::audioReady, this, &AppController::handleAudio);
    connect(input.get(), &AudioInput::runningChanged, this, &AppController::audioSourceChanged);

    const bool started = input->start();
    if (!started) {
        if (audioStatusMessage_.isEmpty()) {
            audioStatusMessage_ = QStringLiteral("Could not start audio input.");
        }

        if (fallbackToDemo) {
            installAudioInput(std::make_unique<DemoAudioInput>(), false);
        } else {
            emit audioSourceChanged();
        }
        return false;
    }

    audioWriteBuffer_.clear();

    if (audioInput_) {
        audioInput_->stop();
    }

    audioInput_ = std::move(input);
    audioStatusMessage_.clear();
    emit audioSourceChanged();
    restartAudioOutput();
    return true;
}

void AppController::handleAudio(const QVector<float>& pcm, int sampleRate, int channels)
{
    if (pcm.isEmpty() || channels <= 0) {
        return;
    }

    float rawRms = 0.0f;
    float processedRms = 0.0f;
    float processedPeak = 0.0f;
    const QVector<float> processedPcm = prepareAudioBlock(pcm, channels, rawRms, processedRms, processedPeak);
    if (processedPcm.isEmpty()) {
        return;
    }

    rawAudioLevel_ = qBound(0.0f, rawAudioLevel_ * 0.86f + rawRms * 0.14f, 1.0f);
    audioLevel_ = qBound(0.0f, audioLevel_ * 0.78f + processedRms * 0.22f, 1.0f);
    audioPeakLevel_ = qBound(0.0f, audioPeakLevel_ * 0.70f + processedPeak * 0.30f, 1.0f);
    
#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
    if (audioOutputDevice_ && audioSink_ && audioSink_->state() != QAudio::StoppedState) {
        audioWriteBuffer_.append(reinterpret_cast<const char*>(processedPcm.constData()), processedPcm.size() * sizeof(float));
        
        qint64 toWrite = audioWriteBuffer_.size();
        qint64 freeBytes = audioSink_->bytesFree();
        qint64 frameSize = 2 * sizeof(float);
        
        qint64 writable = std::min(toWrite, freeBytes);
        writable -= writable % frameSize; // Ensure perfect float alignment
        
        if (writable > 0) {
            audioOutputDevice_->write(audioWriteBuffer_.constData(), writable);
            audioWriteBuffer_.remove(0, writable);
        }
        
        // Prevent buffer bloat if the timer generates data faster than playback
        qint64 maxBufferBytes = 44100 * 2 * sizeof(float); // 1 second cap
        if (audioWriteBuffer_.size() > maxBufferBytes) {
            qint64 excess = audioWriteBuffer_.size() - maxBufferBytes;
            excess += frameSize - (excess % frameSize);
            audioWriteBuffer_.remove(0, excess);
        }
    }
#endif

    emit audioBlockReady(processedPcm, sampleRate, 2);
    emit audioLevelChanged();
}

void AppController::handleActivePresetChanged(const QString& path, const QString& title)
{
    activePresetTitle_ = title;
    activePresetPath_ = path;
    emit activePresetTitleChanged();
    emit activePresetChanged();

    if (!path.isEmpty() && failedPresetPaths_.contains(path) && playlistManager_->rowCount() > 1 && !skippingFailedPreset_) {
        QTimer::singleShot(0, this, [this, path]() {
            if (activePresetPath_ == path) {
                advancePastFailedPreset();
            }
        });
    }

    settings_.setLastPlaylistName(playlistManager_->activePlaylistName());
    restartPresetTimer();
}

void AppController::advancePastFailedPreset()
{
    if (skippingFailedPreset_) {
        return;
    }

    const int presetCount = playlistManager_->rowCount();
    if (presetCount <= 1) {
        return;
    }

    skippingFailedPreset_ = true;
    for (int attempt = 0; attempt < presetCount; ++attempt) {
        nextPreset();
        if (activePresetPath_.isEmpty() || !failedPresetPaths_.contains(activePresetPath_)) {
            break;
        }
    }
    skippingFailedPreset_ = false;
}

bool AppController::activePresetIsKnownFailed() const
{
    return !activePresetPath_.isEmpty() && failedPresetPaths_.contains(activePresetPath_);
}

void AppController::pruneIgnoredPlaylistEntries()
{
    for (int row = playlistManager_->rowCount() - 1; row >= 0; --row) {
        const QVariantMap preset = playlistManager_->get(row);
        const QString path = preset.value(QStringLiteral("path")).toString();
        if (!path.isEmpty() && PresetScanner::shouldIgnorePresetFile(QFileInfo(path))) {
            playlistManager_->removePreset(row);
        }
    }
}

void AppController::chooseInitialAudioInputDevice()
{
    const QString saved = settings_.audioInputDeviceName();
    if (!saved.isEmpty() && audioInputDevices_.contains(saved)) {
        selectedAudioInputDevice_ = saved;
        return;
    }

    if (MacosSystemAudioInput::isSupported() && audioInputDevices_.contains(QStringLiteral("System Audio (Desktop Output)"))) {
        selectedAudioInputDevice_ = QStringLiteral("System Audio (Desktop Output)");
        settings_.setAudioInputDeviceName(selectedAudioInputDevice_);
        return;
    }

    selectedAudioInputDevice_ = QtAudioInput::preferredInputDeviceName(saved);
    if (selectedAudioInputDevice_.isEmpty() && audioInputDevices_.contains(QStringLiteral("Scion MIDI"))) {
        selectedAudioInputDevice_ = QStringLiteral("Scion MIDI");
    }
    if (!selectedAudioInputDevice_.isEmpty()) {
        settings_.setAudioInputDeviceName(selectedAudioInputDevice_);
    }
}

QVector<float> AppController::prepareAudioBlock(const QVector<float>& pcm, int channels, float& rawRms, float& processedRms, float& processedPeak) const
{
    rawRms = 0.0f;
    processedRms = 0.0f;
    processedPeak = 0.0f;

    if (pcm.isEmpty() || channels <= 0) {
        return {};
    }

    const int frameCount = pcm.size() / channels;
    if (frameCount <= 0) {
        return {};
    }

    QVector<float> processed;
    processed.resize(frameCount * 2);

    double rawSum = 0.0;
    double processedSum = 0.0;
    const float gain = static_cast<float>(audioGain_);

    for (int frame = 0; frame < frameCount; ++frame) {
        const int base = frame * channels;
        float left = pcm.at(base);
        float right = channels > 1 ? pcm.at(base + 1) : left;

        if (channels > 2) {
            double leftSum = 0.0;
            double rightSum = 0.0;
            int leftCount = 0;
            int rightCount = 0;
            for (int channel = 0; channel < channels; ++channel) {
                const float sample = pcm.at(base + channel);
                if ((channel % 2) == 0) {
                    leftSum += sample;
                    ++leftCount;
                } else {
                    rightSum += sample;
                    ++rightCount;
                }
            }
            left = leftCount > 0 ? static_cast<float>(leftSum / leftCount) : left;
            right = rightCount > 0 ? static_cast<float>(rightSum / rightCount) : left;
        }

        rawSum += static_cast<double>(left) * left;
        rawSum += static_cast<double>(right) * right;

        left = std::tanh(left * gain);
        right = std::tanh(right * gain);
        processed[frame * 2] = left;
        processed[frame * 2 + 1] = right;

        processedPeak = std::max(processedPeak, std::max(std::abs(left), std::abs(right)));
        processedSum += static_cast<double>(left) * left;
        processedSum += static_cast<double>(right) * right;
    }

    rawRms = static_cast<float>(std::sqrt(rawSum / static_cast<double>(frameCount * 2)));
    processedRms = static_cast<float>(std::sqrt(processedSum / static_cast<double>(frameCount * 2)));
    return processed;
}

void AppController::restartPresetTimer()
{
    const int seconds = qRound(playlistManager_->presetDurationSeconds());
    if (seconds <= 0 || playlistManager_->rowCount() <= 1 || playbackPaused_ || presetLocked_) {
        presetTimer_.stop();
        presetProgressTimer_.stop();
        if (seconds <= 0 || playlistManager_->rowCount() <= 1) {
            presetCycleProgress_ = 0.0;
            presetRemainingSeconds_ = 0;
        }
        emit playbackStateChanged();
        return;
    }
    presetCycleClock_.restart();
    presetCycleProgress_ = 0.0;
    presetRemainingSeconds_ = seconds;
    presetTimer_.start(seconds * 1000);
    presetProgressTimer_.start();
    emit playbackStateChanged();
}

void AppController::updatePresetCycleProgress()
{
    const double seconds = playlistManager_->presetDurationSeconds();
    if (seconds <= 0.0 || !presetTimer_.isActive() || !presetCycleClock_.isValid()) {
        return;
    }

    const double elapsedSeconds = static_cast<double>(presetCycleClock_.elapsed()) / 1000.0;
    presetCycleProgress_ = qBound(0.0, elapsedSeconds / seconds, 1.0);
    presetRemainingSeconds_ = std::max(0, qCeil(seconds - elapsedSeconds));
    emit playbackStateChanged();
}

void AppController::persistFavourites()
{
    settings_.setFavourites(presetLibrary_->favouritePaths());
}

void AppController::restartAudioOutput()
{
#ifdef MILK_RUNNER_HAS_QT_MULTIMEDIA
    if (audioSink_) {
        audioSink_->stop();
        audioSink_.reset();
        audioOutputDevice_ = nullptr;
    }

    if (playAudioOutput_ && !audioSourceName().startsWith(QStringLiteral("Input:"))) {
        QAudioFormat format;
        format.setSampleRate(44100);
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Float);

        QAudioDevice device = QMediaDevices::defaultAudioOutput();
        if (device.isFormatSupported(format)) {
            audioSink_ = std::make_unique<QAudioSink>(device, format, this);
            int bufferBytes = 44100 * 2 * sizeof(float) / 10; // 100ms
            audioSink_->setBufferSize(bufferBytes);
            audioOutputDevice_ = audioSink_->start();
            
            // Pre-fill buffer with silence to prevent instant underrun buzzing
            QByteArray silence(bufferBytes, 0);
            audioOutputDevice_->write(silence);
        } else {
            qWarning() << "Audio format not supported for playback.";
        }
    }
#endif
}
