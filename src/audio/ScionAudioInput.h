#pragma once

#include "AudioInput.h"
#include <QTimer>
#include <QSet>
#include <memory>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <mutex>
#include "rtmidi/RtMidi.h"

class ScionAudioInput : public AudioInput
{
    Q_OBJECT

public:
    explicit ScionAudioInput(QObject* parent = nullptr);
    ~ScionAudioInput() override;

    bool start() override;
    void stop() override;
    QString name() const override;
    
    // Mode identifiers intentionally retain their legacy values so persisted
    // selections never silently map to a different synth.
    enum SynthMode {
        EtherealDrone = 0,
        RhythmicPulses = 1,
        CrystalChimes = 2,
        DeepSeaSwells = 3,
        BreathingBrass = 4,
        SingingWind = 5,
        GhostStrings = 6,
        NeuralSync = 7,
        ChaoticFeedbackRing = 9
    };

    static bool isValidSynthMode(int mode);
    void setEnabledSynthModes(const QSet<int>& modes);

private slots:
    void generateBlock();

private:
    static void midiCallback(double timeStamp, std::vector<unsigned char> *message, void *userData);
    void handleMidiMessage(const std::vector<unsigned char>& message);
    int quantizeNote(int note, int mode) const;

    QTimer timer_;
    std::unique_ptr<rt::midi::RtMidiIn> midiIn_;
    
    struct Voice {
        int note = 0;
        float velocity = 0.0f;
        double phase = 0.0;
        double phase2 = 0.0;
        double phase3 = 0.0; // FM Ring osc 3
        double envLevel = 0.0;
        bool releasing = false;
        double filterPhase = 0.0;
        double currentFreq = 0.0;
        double targetFreq = 0.0;
        
        // Karplus-Strong
        std::vector<float> ksDelay;
        int ksPtr = 0;
        
        // Biquad / SVF states
        float bpfL1 = 0.0f;
        float bpfL2 = 0.0f;
    };
    
    std::mutex stateMutex_;
    struct Layer {
        std::list<Voice> activeVoices;
        std::set<int> heldNotes;
        int arpStep = 0;
        int arpFrames = 0;
        std::vector<float> delayBufferL;
        std::vector<float> delayBufferR;
        int delayWritePtr = 0;
        double lfoPhase = 0.0;
        float lpfL = 0.0f;
        float lpfR = 0.0f;
        float hpfL = 0.0f;
        float hpfR = 0.0f;
        float gain = 0.0f;
        float targetGain = 0.0f;
    };

    void resetLayer(Layer& layer);
    void handleNoteForLayer(Layer& layer, int mode, int note, int velocity, bool noteOn);
    void renderLayer(Layer& layer, int mode, float cc, float noise, float& left, float& right);
    static float modeGain(int mode);
    
    float globalCC_ = 0.0f;
    QString actualPortName_;
    qint64 startTime_ = 0;
    qint64 generatedFrames_ = 0;
    std::set<int> enabledModes_;
    std::map<int, Layer> layers_;
    float smoothedCC_ = 0.0f;
    float dcInputL_ = 0.0f;
    float dcInputR_ = 0.0f;
    float dcOutputL_ = 0.0f;
    float dcOutputR_ = 0.0f;
};
