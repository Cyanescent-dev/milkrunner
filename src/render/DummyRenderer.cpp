#include "DummyRenderer.h"

#include <algorithm>
#include <cmath>

bool DummyRenderer::initialize(const VisualizerRenderContext&)
{
    return true;
}

void DummyRenderer::resize(int width, int height)
{
    width_ = width;
    height_ = height;
}

void DummyRenderer::loadPreset(const std::filesystem::path& presetPath)
{
    currentPreset_ = presetPath;
}

void DummyRenderer::setPresetDuration(float seconds)
{
    presetDurationSeconds_ = seconds;
}

void DummyRenderer::setTransitionDuration(float seconds)
{
    transitionDurationSeconds_ = seconds;
}

void DummyRenderer::submitAudio(const float* pcm, std::size_t frameCount, int, int channels)
{
    if (!pcm || frameCount == 0 || channels <= 0) {
        audioLevel_ *= 0.92f;
        return;
    }

    double sum = 0.0;
    const std::size_t sampleCount = frameCount * static_cast<std::size_t>(channels);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        sum += static_cast<double>(pcm[i]) * static_cast<double>(pcm[i]);
    }
    audioLevel_ = std::clamp(static_cast<float>(std::sqrt(sum / static_cast<double>(sampleCount))), 0.0f, 1.0f);
}

void DummyRenderer::renderFrame(float)
{
}

const char* DummyRenderer::name() const
{
    return "DummyRenderer";
}

float DummyRenderer::audioLevel() const
{
    return audioLevel_;
}
