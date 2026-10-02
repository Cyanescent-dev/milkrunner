#include "ProjectMRenderer.h"

#include <algorithm>
#include <string>

ProjectMRenderer::ProjectMRenderer() = default;

ProjectMRenderer::~ProjectMRenderer()
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    projectm_destroy(handle_);
    handle_ = nullptr;
#endif
}

bool ProjectMRenderer::initialize(const VisualizerRenderContext&)
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    // TODO(android/ios): initialize from the platform render thread with a current GL/GLES context.
    handle_ = projectm_create();
    if (!handle_) {
        return false;
    }
    projectm_set_preset_switch_failed_event_callback(handle_, &ProjectMRenderer::handlePresetSwitchFailed, this);
    projectm_set_preset_duration(handle_, 30.0);
    projectm_set_soft_cut_duration(handle_, 3.0);
    projectm_set_hard_cut_enabled(handle_, false);
    return true;
#else
    return false;
#endif
}

void ProjectMRenderer::resize(int width, int height)
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    if (handle_ && width > 0 && height > 0) {
        projectm_set_window_size(handle_, static_cast<std::size_t>(width), static_cast<std::size_t>(height));
    }
#else
    (void)width;
    (void)height;
#endif
}

void ProjectMRenderer::loadPreset(const std::filesystem::path& presetPath)
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    if (handle_) {
        projectm_load_preset_file(handle_, presetPath.string().c_str(), true);
    }
#else
    (void)presetPath;
#endif
}

void ProjectMRenderer::setPresetDuration(float seconds)
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    if (handle_) {
        projectm_set_preset_duration(handle_, std::max(1.0f, seconds));
    }
#else
    (void)seconds;
#endif
}

void ProjectMRenderer::setTransitionDuration(float seconds)
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    if (handle_) {
        projectm_set_soft_cut_duration(handle_, std::max(0.0f, seconds));
    }
#else
    (void)seconds;
#endif
}

void ProjectMRenderer::submitAudio(const float* pcm, std::size_t frameCount, int, int channels)
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    if (handle_ && pcm && frameCount > 0) {
        projectm_pcm_add_float(
            handle_,
            pcm,
            static_cast<unsigned int>(std::min<std::size_t>(frameCount, projectm_pcm_get_max_samples())),
            channels <= 1 ? PROJECTM_MONO : PROJECTM_STEREO);
    }
#else
    (void)pcm;
    (void)frameCount;
    (void)channels;
#endif
}

void ProjectMRenderer::renderFrame(float deltaTimeSeconds)
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    if (handle_) {
        elapsedSeconds_ += deltaTimeSeconds;
        projectm_opengl_render_frame(handle_);
    }
#else
    (void)deltaTimeSeconds;
#endif
}

const char* ProjectMRenderer::name() const
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    return "ProjectMRenderer";
#else
    return "ProjectMRenderer (not built)";
#endif
}

void ProjectMRenderer::setTextureSearchPaths(const std::vector<std::filesystem::path>& paths)
{
#ifdef MILK_RUNNER_HAS_PROJECTM
    if (!handle_) {
        return;
    }

    std::vector<std::string> pathStrings;
    pathStrings.reserve(paths.size());
    for (const std::filesystem::path& path : paths) {
        if (!path.empty()) {
            pathStrings.push_back(path.string());
        }
    }

    std::vector<const char*> pathPointers;
    pathPointers.reserve(pathStrings.size());
    for (const std::string& path : pathStrings) {
        pathPointers.push_back(path.c_str());
    }

    projectm_set_texture_search_paths(handle_, pathPointers.data(), pathPointers.size());
#else
    (void)paths;
#endif
}

std::optional<ProjectMRenderer::PresetFailure> ProjectMRenderer::takeLastPresetFailure()
{
    std::optional<PresetFailure> failure = std::move(lastPresetFailure_);
    lastPresetFailure_.reset();
    return failure;
}

#ifdef MILK_RUNNER_HAS_PROJECTM
void ProjectMRenderer::handlePresetSwitchFailed(const char* presetFilename, const char* message, void* userData)
{
    if (auto* renderer = static_cast<ProjectMRenderer*>(userData)) {
        renderer->recordPresetFailure(presetFilename, message);
    }
}
#endif

void ProjectMRenderer::recordPresetFailure(const char* presetFilename, const char* message)
{
    PresetFailure failure;
    if (presetFilename) {
        failure.presetPath = std::filesystem::path(presetFilename);
    }
    failure.message = message && *message ? message : "Preset failed to load.";
    lastPresetFailure_ = std::move(failure);
}
