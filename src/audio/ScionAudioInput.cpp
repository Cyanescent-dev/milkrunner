#include "audio/ScionAudioInput.h"

#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <cmath>

using namespace rt::midi;

namespace {
constexpr int kSampleRate = 44100;
constexpr int kChannels = 2;
constexpr int kDelayFrames = kSampleRate * 2;
constexpr float kDcBlockPole = 0.9979f;
constexpr float kSafetyCeiling = 0.89125f;

float finiteClamped(float value, float low, float high)
{
    return std::isfinite(value) ? std::clamp(value, low, high) : 0.0f;
}

float softCeiling(float value)
{
    constexpr float knee = 0.72f;
    const float magnitude = std::abs(value);
    if (!std::isfinite(value)) return 0.0f;
    if (magnitude <= knee) return value;
    const float compressed = knee + (kSafetyCeiling - knee)
        * std::tanh((magnitude - knee) / (kSafetyCeiling - knee));
    return std::copysign(std::min(compressed, kSafetyCeiling), value);
}
}

ScionAudioInput::ScionAudioInput(QObject* parent)
    : AudioInput(parent), timer_(this)
{
    try { midiIn_ = std::make_unique<RtMidiIn>(); }
    catch (RtMidiError& error) { qWarning() << "RtMidi init error:" << QString::fromStdString(error.getMessage()); }
    connect(&timer_, &QTimer::timeout, this, &ScionAudioInput::generateBlock);
}

ScionAudioInput::~ScionAudioInput() { stop(); }

bool ScionAudioInput::isValidSynthMode(int mode)
{
    switch (mode) {
    case EtherealDrone: case RhythmicPulses: case CrystalChimes: case DeepSeaSwells:
    case BreathingBrass: case SingingWind: case GhostStrings: case NeuralSync:
    case ChaoticFeedbackRing: return true;
    default: return false;
    }
}

float ScionAudioInput::modeGain(int mode)
{
    switch (mode) {
    case EtherealDrone: return 0.42f;
    case RhythmicPulses: return 0.34f;
    case CrystalChimes: return 0.29f;
    case DeepSeaSwells: return 0.34f;
    case BreathingBrass: return 0.32f;
    case SingingWind: return 0.18f;
    case GhostStrings: return 0.30f;
    case NeuralSync: return 0.35f;
    case ChaoticFeedbackRing: return 0.24f;
    default: return 0.0f;
    }
}

void ScionAudioInput::resetLayer(Layer& layer)
{
    layer.activeVoices.clear(); layer.heldNotes.clear();
    layer.arpStep = layer.arpFrames = layer.delayWritePtr = 0;
    layer.delayBufferL.assign(kDelayFrames, 0.0f);
    layer.delayBufferR.assign(kDelayFrames, 0.0f);
    layer.lfoPhase = 0.0;
    layer.lpfL = layer.lpfR = layer.hpfL = layer.hpfR = 0.0f;
}

void ScionAudioInput::setEnabledSynthModes(const QSet<int>& modes)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    std::set<int> requested;
    for (int mode : modes) {
        if (!isValidSynthMode(mode)) continue;
        requested.insert(mode);
        Layer& layer = layers_[mode];
        if (layer.delayBufferL.empty()) resetLayer(layer);
        layer.targetGain = 1.0f;
    }
    for (auto& [mode, layer] : layers_) {
        if (requested.contains(mode)) continue;
        layer.targetGain = 0.0f;
        layer.heldNotes.clear();
        for (Voice& voice : layer.activeVoices) voice.releasing = true;
    }
    enabledModes_ = std::move(requested);
}

bool ScionAudioInput::start()
{
    if (!midiIn_) return false;
    bool portFound = false;
    const unsigned int ports = midiIn_->getPortCount();
    for (unsigned int index = 0; index < ports; ++index) {
        const std::string portName = midiIn_->getPortName(index);
        if (portName.find("Pocket SC") == std::string::npos && portName.find("Scion") == std::string::npos && portName.find("SCION") == std::string::npos) continue;
        try { midiIn_->openPort(index); actualPortName_ = QString::fromStdString(portName); portFound = true; break; }
        catch (RtMidiError& error) { emit errorOccurred(QString::fromStdString(error.getMessage())); return false; }
    }
    if (!portFound && ports > 0) {
        try { midiIn_->openPort(0); actualPortName_ = QString::fromStdString(midiIn_->getPortName(0)); portFound = true; }
        catch (RtMidiError& error) { emit errorOccurred(QString::fromStdString(error.getMessage())); return false; }
    }
    if (!portFound) { emit errorOccurred(QStringLiteral("No MIDI inputs available for Scion.")); return false; }
    midiIn_->setCallback(&ScionAudioInput::midiCallback, this);
    midiIn_->ignoreTypes(false, false, false);
    startTime_ = QDateTime::currentMSecsSinceEpoch(); generatedFrames_ = 0;
    timer_.start(16); setRunning(true); return true;
}

void ScionAudioInput::stop()
{
    timer_.stop();
    if (midiIn_ && midiIn_->isPortOpen()) midiIn_->closePort();
    setRunning(false);
}

QString ScionAudioInput::name() const
{
    return actualPortName_.isEmpty() ? QStringLiteral("Pocket SCÍON") : QStringLiteral("Scion: %1").arg(actualPortName_);
}

void ScionAudioInput::midiCallback(double, std::vector<unsigned char>* message, void* userData)
{
    if (userData && message) static_cast<ScionAudioInput*>(userData)->handleMidiMessage(*message);
}

int ScionAudioInput::quantizeNote(int note, int mode) const
{
    const int safeNote = std::clamp(note, 0, 127);
    const int octave = safeNote / 12;
    const int pitchClass = safeNote % 12;
    std::vector<int> scale = {0, 2, 4, 5, 7, 9, 11};
    if (mode == RhythmicPulses) scale = {0, 3, 5, 7, 10};
    else if (mode == CrystalChimes) scale = {0, 2, 3, 5, 7, 9, 10};
    else if (mode == DeepSeaSwells || mode == GhostStrings) scale = {0, 2, 3, 5, 7, 8, 10};
    else if (mode == SingingWind) scale = {0, 2, 4, 7, 9};
    int closest = scale.front(); int difference = 12;
    for (int step : scale) if (std::abs(step - pitchClass) < difference) { difference = std::abs(step - pitchClass); closest = step; }
    return octave * 12 + closest;
}

void ScionAudioInput::handleNoteForLayer(Layer& layer, int mode, int note, int velocity, bool noteOn)
{
    const int quantized = quantizeNote(note, mode);
    if (!noteOn) {
        layer.heldNotes.erase(note);
        if (mode != RhythmicPulses) for (Voice& voice : layer.activeVoices) if (voice.note == quantized) voice.releasing = true;
        return;
    }
    layer.heldNotes.insert(note);
    if (mode == RhythmicPulses) return;
    if ((mode == BreathingBrass && layer.activeVoices.size() >= 4) || (mode == GhostStrings && layer.activeVoices.size() >= 6)) layer.activeVoices.pop_front();
    Voice voice;
    voice.note = quantized; voice.velocity = std::clamp(velocity / 127.0f, 0.0f, 1.0f);
    if (mode == DeepSeaSwells) {
        voice.targetFreq = 440.0 * std::pow(2.0, (quantized - 69) / 12.0);
        voice.currentFreq = layer.activeVoices.empty() ? voice.targetFreq : layer.activeVoices.back().currentFreq;
    } else if (mode == GhostStrings) {
        const double frequency = std::clamp(440.0 * std::pow(2.0, ((quantized - 12) - 69) / 12.0), 20.0, 8000.0);
        voice.ksDelay.resize(std::max(1, static_cast<int>(kSampleRate / frequency)));
        for (float& sample : voice.ksDelay) sample = (rand() / static_cast<float>(RAND_MAX) - 0.5f) * 0.7f;
    }
    layer.activeVoices.push_back(std::move(voice));
}

void ScionAudioInput::handleMidiMessage(const std::vector<unsigned char>& message)
{
    if (message.empty() || message[0] >= 0xF8) return;
    const unsigned char command = message[0] & 0xF0;
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (command == 0xB0 && message.size() >= 3) { globalCC_ = finiteClamped(message[2] / 127.0f, 0.0f, 1.0f); return; }
    if ((command != 0x90 && command != 0x80) || message.size() < 2) return;
    const int note = std::clamp<int>(message[1], 0, 127);
    const int velocity = message.size() >= 3 ? std::clamp<int>(message[2], 0, 127) : 0;
    const bool noteOn = command == 0x90 && velocity > 0;
    for (auto& [mode, layer] : layers_) if (enabledModes_.contains(mode) || layer.gain > 0.0001f) handleNoteForLayer(layer, mode, note, velocity, noteOn);
}

void ScionAudioInput::renderLayer(Layer& layer, int mode, float cc, float noise, float& left, float& right)
{
    float sampleL = mode == SingingWind ? noise * 0.04f : 0.0f;
    float sampleR = sampleL;
    if (mode == RhythmicPulses && ++layer.arpFrames >= kSampleRate / 8) {
        layer.arpFrames = 0;
        for (Voice& voice : layer.activeVoices) voice.releasing = true;
        if (!layer.heldNotes.empty()) {
            auto note = layer.heldNotes.begin(); std::advance(note, layer.arpStep++ % layer.heldNotes.size());
            Voice voice; voice.note = quantizeNote(*note, mode); voice.velocity = 0.8f; layer.activeVoices.push_back(std::move(voice));
        }
    }
    for (auto it = layer.activeVoices.begin(); it != layer.activeVoices.end();) {
        Voice& voice = *it;
        int note = voice.note;
        if (mode == RhythmicPulses || mode == CrystalChimes || mode == GhostStrings || mode == ChaoticFeedbackRing) note -= 12;
        if (mode == NeuralSync) note -= 24;
        double frequency = std::clamp(440.0 * std::pow(2.0, (note - 69) / 12.0), 20.0, 10000.0);
        float attack = 1.0f / (kSampleRate * 0.02f), release = 1.0f / (kSampleRate * 0.03f);
        switch (mode) {
        case EtherealDrone: attack = 1.0f / (kSampleRate * 2.0f); release = 1.0f / (kSampleRate * 3.0f); break;
        case RhythmicPulses: attack = 1.0f / (kSampleRate * 0.01f); release = 1.0f / (kSampleRate * 0.2f); break;
        case CrystalChimes: attack = 1.0f / (kSampleRate * 0.01f); release = 1.0f / (kSampleRate * 4.0f); break;
        case DeepSeaSwells: attack = 1.0f / (kSampleRate * 1.5f); release = 1.0f / (kSampleRate * 2.0f); voice.currentFreq += (voice.targetFreq - voice.currentFreq) * 0.0005; frequency = std::clamp(voice.currentFreq, 20.0, 10000.0); break;
        case BreathingBrass: attack = 1.0f / (kSampleRate * 0.5f); release = 1.0f / (kSampleRate * 1.0f); break;
        case SingingWind: attack = 1.0f / (kSampleRate * 3.0f); release = 1.0f / (kSampleRate * 5.0f); break;
        case GhostStrings: attack = 1.0f / (kSampleRate * 0.008f); release = 1.0f / (kSampleRate * 0.015f); break;
        case NeuralSync: attack = 1.0f / (kSampleRate * 1.0f); release = 1.0f / (kSampleRate * 2.0f); break;
        case ChaoticFeedbackRing: attack = 1.0f / (kSampleRate * 1.0f); release = 1.0f / (kSampleRate * 3.0f); break;
        }
        if (!voice.releasing) voice.envLevel = std::min(1.0, voice.envLevel + attack); else voice.envLevel -= release;
        if (voice.envLevel <= 0.0 || !std::isfinite(voice.envLevel)) { it = layer.activeVoices.erase(it); continue; }
        const double increment = frequency * 2.0 * M_PI / kSampleRate;
        voice.phase = std::fmod(voice.phase + increment, 2.0 * M_PI);
        float rawL = 0.0f, rawR = 0.0f;
        if (mode == EtherealDrone) {
            const float sine = std::sin(voice.phase), triangle = 2.0f * std::abs(2.0f * (voice.phase / (2.0 * M_PI)) - 1.0f) - 1.0f;
            rawL = rawR = sine * 0.7f + triangle * 0.3f;
        } else if (mode == RhythmicPulses) {
            const float raw = 2.0f * (voice.phase / (2.0 * M_PI)) - 1.0f;
            voice.filterPhase += (0.1f + cc * 0.8f) * (raw - voice.filterPhase); rawL = rawR = voice.filterPhase;
        } else if (mode == CrystalChimes) {
            voice.phase2 = std::fmod(voice.phase2 + increment * 2.0, 2.0 * M_PI); rawL = rawR = std::sin(voice.phase + std::sin(voice.phase2) * (1.0f + cc * 4.0f));
        } else if (mode == DeepSeaSwells) {
            voice.phase2 = std::fmod(voice.phase2 + increment * 1.005, 2.0 * M_PI);
            rawL = rawR = ((std::sin(voice.phase) > 0 ? 0.5f : -0.5f) + (std::sin(voice.phase2) > 0 ? 0.5f : -0.5f) + noise * 0.1f) * 0.5f;
        } else if (mode == BreathingBrass) {
            voice.phase2 = std::fmod(voice.phase2 + increment * 1.01, 2.0 * M_PI);
            const float raw = (2.0f * (voice.phase / (2.0 * M_PI)) - 1.0f + 2.0f * (voice.phase2 / (2.0 * M_PI)) - 1.0f) * 0.5f;
            voice.filterPhase += (0.2f + cc * 0.6f) * (raw - voice.filterPhase); rawL = rawR = voice.filterPhase;
        } else if (mode == SingingWind) {
            const float f = std::clamp(static_cast<float>(2.0 * std::sin(M_PI * frequency / kSampleRate)), 0.0f, 0.95f), q = 0.05f + (1.0f - cc) * 0.08f;
            voice.bpfL1 += f * voice.bpfL2; voice.bpfL2 += f * (noise - voice.bpfL1 - q * voice.bpfL2); rawL = rawR = finiteClamped(voice.bpfL1 * 8.0f, -2.0f, 2.0f);
        } else if (mode == GhostStrings && !voice.ksDelay.empty()) {
            const float output = voice.ksDelay[voice.ksPtr], next = voice.ksDelay[(voice.ksPtr + 1) % voice.ksDelay.size()];
            voice.ksDelay[voice.ksPtr] = finiteClamped((output + next) * 0.5f * (0.5f + cc * 0.48f), -1.0f, 1.0f); voice.ksPtr = (voice.ksPtr + 1) % voice.ksDelay.size(); rawL = rawR = output * 1.5f;
        } else if (mode == NeuralSync) {
            voice.phase2 = std::fmod(voice.phase2 + (frequency + 2.0 + cc * 18.0) * 2.0 * M_PI / kSampleRate, 2.0 * M_PI); rawL = std::sin(voice.phase); rawR = std::sin(voice.phase2);
        } else if (mode == ChaoticFeedbackRing) {
            const float index = 0.1f + cc * 3.0f;
            voice.phase3 = std::fmod(voice.phase3 + increment * 0.5, 2.0 * M_PI); voice.phase2 = std::fmod(voice.phase2 + increment * 1.5, 2.0 * M_PI);
            const float osc3 = std::sin(voice.phase3 + voice.filterPhase * index), osc2 = std::sin(voice.phase2 + osc3 * index);
            voice.filterPhase = std::sin(voice.phase + osc2 * index); rawL = rawR = (voice.filterPhase + osc2 + osc3) * 0.33f;
        }
        sampleL += finiteClamped(rawL * voice.envLevel * voice.velocity, -3.0f, 3.0f); sampleR += finiteClamped(rawR * voice.envLevel * voice.velocity, -3.0f, 3.0f); ++it;
    }
    const int delay = (mode == EtherealDrone || mode == CrystalChimes || mode == SingingWind || mode == GhostStrings) ? static_cast<int>(kSampleRate * 0.7) : static_cast<int>(kSampleRate * 0.375);
    const int readL = (layer.delayWritePtr - delay + kDelayFrames) % kDelayFrames, readR = (layer.delayWritePtr - delay - kSampleRate / 10 + kDelayFrames) % kDelayFrames;
    const float delayL = layer.delayBufferL[readL], delayR = layer.delayBufferR[readR];
    const float feedback = (mode == EtherealDrone || mode == CrystalChimes || mode == SingingWind) ? 0.72f : (mode == ChaoticFeedbackRing ? 0.25f : 0.42f);
    layer.delayBufferL[layer.delayWritePtr] = finiteClamped(sampleL + delayR * feedback, -2.0f, 2.0f); layer.delayBufferR[layer.delayWritePtr] = finiteClamped(sampleR + delayL * feedback, -2.0f, 2.0f); layer.delayWritePtr = (layer.delayWritePtr + 1) % kDelayFrames;
    const float wet = mode == CrystalChimes ? 0.55f : (mode == EtherealDrone || mode == SingingWind || mode == GhostStrings ? 0.45f : 0.25f);
    const float mixedL = sampleL * (1.0f - wet) + delayL * wet, mixedR = sampleR * (1.0f - wet) + delayR * wet;
    const float lpf = mode == SingingWind || mode == ChaoticFeedbackRing ? 4500.0f : 3000.0f, alpha = std::clamp(2.0f * static_cast<float>(M_PI) * lpf / kSampleRate, 0.0f, 0.95f);
    layer.lpfL += alpha * (mixedL - layer.lpfL); layer.lpfR += alpha * (mixedR - layer.lpfR); left = layer.lpfL; right = layer.lpfR;
}

void ScionAudioInput::generateBlock()
{
    const qint64 targetFrames = (QDateTime::currentMSecsSinceEpoch() - startTime_) * kSampleRate / 1000;
    const int frames = static_cast<int>(targetFrames - generatedFrames_);
    if (frames <= 0) return;
    generatedFrames_ = targetFrames;
    QVector<float> pcm(frames * kChannels);
    std::lock_guard<std::mutex> lock(stateMutex_);
    for (int frame = 0; frame < frames; ++frame) {
        smoothedCC_ += (finiteClamped(globalCC_, 0.0f, 1.0f) - smoothedCC_) * 0.004f;
        float sumL = 0.0f, sumR = 0.0f; int activeLayers = 0;
        const float noise = rand() / static_cast<float>(RAND_MAX) - 0.5f;
        for (auto& [mode, layer] : layers_) {
            layer.gain += (layer.targetGain - layer.gain) * 0.003f;
            if (layer.gain < 0.0001f && layer.targetGain == 0.0f) { layer.gain = 0.0f; if (!layer.activeVoices.empty()) resetLayer(layer); continue; }
            float layerL = 0.0f, layerR = 0.0f; renderLayer(layer, mode, smoothedCC_, noise, layerL, layerR);
            sumL += layerL * layer.gain * modeGain(mode); sumR += layerR * layer.gain * modeGain(mode); if (layer.gain > 0.05f) ++activeLayers;
        }
        const float compensation = activeLayers > 1 ? 1.0f / std::pow(static_cast<float>(activeLayers), 0.35f) : 1.0f;
        sumL *= compensation; sumR *= compensation;
        const float dcFreeL = sumL - dcInputL_ + kDcBlockPole * dcOutputL_, dcFreeR = sumR - dcInputR_ + kDcBlockPole * dcOutputR_;
        dcInputL_ = sumL; dcInputR_ = sumR; dcOutputL_ = finiteClamped(dcFreeL, -4.0f, 4.0f); dcOutputR_ = finiteClamped(dcFreeR, -4.0f, 4.0f);
        pcm[frame * 2] = softCeiling(dcOutputL_); pcm[frame * 2 + 1] = softCeiling(dcOutputR_);
    }
    emit audioReady(pcm, kSampleRate, kChannels);
}
