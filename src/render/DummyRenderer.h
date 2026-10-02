#pragma once

#include "IVisualizerRenderer.h"

#include <filesystem>

class DummyRenderer final : public IVisualizerRenderer
{
public:
    bool initialize(const VisualizerRenderContext& context) override;
    void resize(int width, int height) override;
    void loadPreset(const std::filesystem::path& presetPath) override;
    void setPresetDuration(float seconds) override;
    void setTransitionDuration(float seconds) override;
    void submitAudio(const float* pcm, std::size_t frameCount, int sampleRate, int channels) override;
    void renderFrame(float deltaTimeSeconds) override;
    const char* name() const override;

    float audioLevel() const;

private:
    int width_ = 0;
    int height_ = 0;
    float presetDurationSeconds_ = 30.0f;
    float transitionDurationSeconds_ = 3.0f;
    float audioLevel_ = 0.0f;
    std::filesystem::path currentPreset_;
};
