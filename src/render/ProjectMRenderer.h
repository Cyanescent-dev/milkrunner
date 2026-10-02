#pragma once

#include "IVisualizerRenderer.h"

#include <optional>
#include <string>
#include <vector>

#ifdef MILK_RUNNER_HAS_PROJECTM
#include <projectM-4/projectM.h>
#endif

class ProjectMRenderer final : public IVisualizerRenderer
{
public:
    struct PresetFailure
    {
        std::filesystem::path presetPath;
        std::string message;
    };

    ProjectMRenderer();
    ~ProjectMRenderer() override;

    bool initialize(const VisualizerRenderContext& context) override;
    void resize(int width, int height) override;
    void loadPreset(const std::filesystem::path& presetPath) override;
    void setPresetDuration(float seconds) override;
    void setTransitionDuration(float seconds) override;
    void submitAudio(const float* pcm, std::size_t frameCount, int sampleRate, int channels) override;
    void renderFrame(float deltaTimeSeconds) override;
    const char* name() const override;
    void setTextureSearchPaths(const std::vector<std::filesystem::path>& paths);
    std::optional<PresetFailure> takeLastPresetFailure();

private:
#ifdef MILK_RUNNER_HAS_PROJECTM
    static void handlePresetSwitchFailed(const char* presetFilename, const char* message, void* userData);
#endif
    void recordPresetFailure(const char* presetFilename, const char* message);

#ifdef MILK_RUNNER_HAS_PROJECTM
    projectm_handle handle_ = nullptr;
#endif
    std::optional<PresetFailure> lastPresetFailure_;
    float elapsedSeconds_ = 0.0f;
};
