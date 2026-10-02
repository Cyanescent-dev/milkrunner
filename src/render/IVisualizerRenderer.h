#pragma once

#include <cstddef>
#include <filesystem>

struct VisualizerRenderContext
{
    void* nativeHandle = nullptr;
};

class IVisualizerRenderer
{
public:
    virtual ~IVisualizerRenderer() = default;

    virtual bool initialize(const VisualizerRenderContext& context) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void loadPreset(const std::filesystem::path& presetPath) = 0;
    virtual void setPresetDuration(float seconds) = 0;
    virtual void setTransitionDuration(float seconds) = 0;
    virtual void submitAudio(const float* pcm, std::size_t frameCount, int sampleRate, int channels) = 0;
    virtual void renderFrame(float deltaTimeSeconds) = 0;
    virtual const char* name() const = 0;
};
