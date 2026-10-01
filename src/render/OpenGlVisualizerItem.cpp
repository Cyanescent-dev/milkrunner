#include "OpenGlVisualizerItem.h"

#include "ProjectMRenderer.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QMutexLocker>
#include <QPointer>
#include <QMetaObject>
#include <QStringList>
#include <QVector2D>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace {
constexpr float Pi = 3.14159265358979323846f;

struct VisualizerVertex
{
    QVector2D position;
    float band = 0.0f;
};

class OpenGlVisualizerRenderer final : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions
{
public:
    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override
    {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setSamples(4);
        return new QOpenGLFramebufferObject(size, format);
    }

    void synchronize(QQuickFramebufferObject* item) override
    {
        const auto* visualizer = qobject_cast<OpenGlVisualizerItem*>(item);
        if (!visualizer) {
            return;
        }
        visualizerItem_ = const_cast<OpenGlVisualizerItem*>(visualizer);

        audioLevel_ = visualizer->audioLevel();
        renderScale_ = visualizer->renderScale();
        lowPowerMode_ = visualizer->lowPowerMode();

        const QString nextPresetPath = visualizer->presetPath();
        if (presetPath_ != nextPresetPath) {
            presetPath_ = nextPresetPath;
            presetDirty_ = true;
            texturePathsDirty_ = true;
        }

        const QString nextTextureRoot = visualizer->textureRoot();
        if (textureRoot_ != nextTextureRoot) {
            textureRoot_ = nextTextureRoot;
            texturePathsDirty_ = true;
        }

        const float nextPresetDuration = visualizer->presetDurationSeconds();
        if (!qFuzzyCompare(presetDurationSeconds_, nextPresetDuration)) {
            presetDurationSeconds_ = nextPresetDuration;
            timingDirty_ = true;
        }

        const float nextTransitionDuration = visualizer->transitionDurationSeconds();
        if (!qFuzzyCompare(transitionDurationSeconds_, nextTransitionDuration)) {
            transitionDurationSeconds_ = nextTransitionDuration;
            timingDirty_ = true;
        }

        QVector<float> pcm;
        int sampleRate = 0;
        int channels = 0;
        if (const_cast<OpenGlVisualizerItem*>(visualizer)->takePendingAudio(pcm, sampleRate, channels)) {
            queuedAudioPcm_.assign(pcm.cbegin(), pcm.cend());
            queuedAudioSampleRate_ = sampleRate;
            queuedAudioChannels_ = channels;
            lastQueuedAudioFrameCount_ = queuedAudioChannels_ > 0
                ? queuedAudioPcm_.size() / static_cast<std::size_t>(queuedAudioChannels_)
                : 0;
            audioDirty_ = true;
        }
    }

    void render() override
    {
        const QSize viewportSize = framebufferObject() ? framebufferObject()->size() : QSize();
        const float deltaSeconds = lowPowerMode_ ? 1.0f / 30.0f : 1.0f / 60.0f;
        timeSeconds_ += deltaSeconds;

        if (renderWithProjectM(viewportSize, deltaSeconds)) {
            maybePostDiagnostics(QStringLiteral("projectM"));
            return;
        }

        if (!fallbackInitialized_) {
            initializeOpenGLFunctions();
            fallbackInitialized_ = buildProgram();
        }

        glViewport(0, 0, viewportSize.width(), viewportSize.height());

        const float energy = std::clamp(audioLevel_, 0.02f, 1.0f);
        const float bass = 0.06f + energy * 0.22f;

        glDisable(GL_DEPTH_TEST);
        glClearColor(0.015f + bass * 0.20f, 0.018f + bass * 0.14f, 0.026f + bass * 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        if (!fallbackInitialized_) {
            return;
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        program_->bind();
        program_->setUniformValue("u_time", timeSeconds_);
        program_->setUniformValue("u_audio", energy);
        program_->setUniformValue("u_aspect", viewportSize.height() > 0
                ? static_cast<float>(viewportSize.width()) / static_cast<float>(viewportSize.height())
                : 1.0f);

        drawBackground();
        drawRings();
        drawWave();

        program_->release();
        glDisable(GL_BLEND);
        ++fallbackFrameCount_;
        maybePostDiagnostics(QStringLiteral("fallback"));
    }

private:
    bool renderWithProjectM(const QSize& viewportSize, float deltaSeconds)
    {
        if (!projectmAttempted_) {
            projectmAttempted_ = true;
            initializeOpenGLFunctions();
            glVersionText_ = glString(GL_VERSION);
            glslVersionText_ = glString(GL_SHADING_LANGUAGE_VERSION);
            projectmRenderer_ = std::make_unique<ProjectMRenderer>();
            if (projectmRenderer_->initialize({})) {
                projectmActive_ = true;
                projectmInitialized_ = true;
                timingDirty_ = true;
                presetDirty_ = true;
                postStatus(QStringLiteral("projectM initialized."));
            } else {
                projectmRenderer_.reset();
                projectmInitialized_ = false;
                projectmInitFailed_ = true;
                postStatus(QStringLiteral("projectM initialization failed; using fallback renderer."));
            }
        }

        if (!projectmActive_ || !projectmRenderer_) {
            return false;
        }

        if (viewportSize != lastProjectMSize_) {
            lastProjectMSize_ = viewportSize;
            projectmRenderer_->resize(viewportSize.width(), viewportSize.height());
        }

        if (timingDirty_) {
            projectmRenderer_->setPresetDuration(presetDurationSeconds_);
            projectmRenderer_->setTransitionDuration(transitionDurationSeconds_);
            timingDirty_ = false;
        }

        if (texturePathsDirty_) {
            projectmRenderer_->setTextureSearchPaths(textureSearchPaths());
            texturePathsDirty_ = false;
        }

        if (presetDirty_) {
            framesSincePresetLoad_ = 0;
            renderConfirmationPosted_ = false;
            glErrorFailurePosted_ = false;
            consecutiveGlErrorFrames_ = 0;
            if (presetPath_.isEmpty()) {
                postStatus(QString());
                ++presetLoadAttemptCount_;
                lastPresetLoadPath_ = QStringLiteral("idle://");
                projectmRenderer_->loadPreset(std::filesystem::path("idle://"));
            } else if (failedPresetPaths_.contains(presetPath_.toStdString())) {
                postStatus(QStringLiteral("Skipped failed preset: %1").arg(presetFileName(presetPath_)));
            } else {
                postStatus(QString());
                ++presetLoadAttemptCount_;
                lastPresetLoadPath_ = presetPath_;
                projectmRenderer_->loadPreset(std::filesystem::path(presetPath_.toStdString()));
            }
            presetDirty_ = false;
        }

        submitQueuedAudio();
        updateFramebufferDiagnostics();
        projectmRenderer_->renderFrame(deltaSeconds);
        lastGlErrorText_ = collectGlErrorText();
        updateGlitchDetection();
        ++projectmFrameCount_;
        ++framesSincePresetLoad_;
        drainPresetFailure();
        maybeConfirmPresetRender();
        return true;
    }

    bool buildProgram()
    {
        program_ = std::make_unique<QOpenGLShaderProgram>();
        const char* vertexShader = R"(
            attribute highp vec2 a_position;
            attribute highp float a_band;
            uniform highp float u_time;
            uniform highp float u_audio;
            uniform highp float u_aspect;
            varying highp float v_band;
            varying highp float v_radius;

            void main()
            {
                highp vec2 p = a_position;
                p.x /= max(u_aspect, 0.001);
                v_band = a_band;
                v_radius = length(a_position);
                gl_Position = vec4(p, 0.0, 1.0);
            }
        )";

        const char* fragmentShader = R"(
            uniform highp float u_time;
            uniform highp float u_audio;
            varying highp float v_band;
            varying highp float v_radius;

            void main()
            {
                highp float pulse = 0.5 + 0.5 * sin(u_time * 1.7 + v_band * 3.14159);
                highp vec3 teal = vec3(0.26, 0.95, 0.72);
                highp vec3 amber = vec3(1.0, 0.58, 0.22);
                highp vec3 blue = vec3(0.34, 0.52, 1.0);
                highp vec3 color = mix(mix(teal, blue, v_band), amber, pulse * 0.35);
                highp float alpha = mix(0.18, 0.72, clamp(u_audio + pulse * 0.28, 0.0, 1.0));
                gl_FragColor = vec4(color, alpha * (1.0 - clamp(v_radius * 0.22, 0.0, 0.45)));
            }
        )";

        if (!program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader)
            || !program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader)
            || !program_->link()) {
            program_.reset();
            return false;
        }

        positionAttribute_ = program_->attributeLocation("a_position");
        bandAttribute_ = program_->attributeLocation("a_band");
        return positionAttribute_ >= 0 && bandAttribute_ >= 0;
    }

    void drawBackground()
    {
        const std::array<VisualizerVertex, 6> vertices{{
            {{-1.0f, -1.0f}, 0.05f}, {{1.0f, -1.0f}, 0.20f}, {{-1.0f, 1.0f}, 0.70f},
            {{1.0f, -1.0f}, 0.20f}, {{1.0f, 1.0f}, 0.92f}, {{-1.0f, 1.0f}, 0.70f},
        }};

        drawVertices(vertices.data(), vertices.size(), GL_TRIANGLES);
    }

    void drawRings()
    {
        const int ringCount = lowPowerMode_ ? 5 : 9;
        const int segmentCount = lowPowerMode_ ? 96 : 160;
        std::vector<VisualizerVertex> vertices;
        vertices.reserve(segmentCount);

        for (int ring = 0; ring < ringCount; ++ring) {
            vertices.clear();
            const float baseRadius = 0.16f + static_cast<float>(ring) * 0.082f * renderScale_;
            const float ringBand = static_cast<float>(ring) / std::max(1, ringCount - 1);
            for (int i = 0; i < segmentCount; ++i) {
                const float angle = (static_cast<float>(i) / static_cast<float>(segmentCount)) * 2.0f * Pi;
                const float wobble = qSin(timeSeconds_ * (1.2f + ringBand) + angle * (3.0f + ringBand * 5.0f));
                const float radius = baseRadius + wobble * (0.012f + audioLevel_ * 0.052f);
                vertices.push_back({{qCos(angle) * radius, qSin(angle) * radius}, ringBand});
            }
            glLineWidth(1.5f + renderScale_ * 2.2f);
            drawVertices(vertices.data(), vertices.size(), GL_LINE_LOOP);
        }
    }

    void drawWave()
    {
        const int pointCount = lowPowerMode_ ? 96 : 192;
        std::vector<VisualizerVertex> vertices;
        vertices.reserve(pointCount);

        for (int i = 0; i < pointCount; ++i) {
            const float phase = static_cast<float>(i) / static_cast<float>(pointCount - 1);
            const float x = -0.94f + phase * 1.88f;
            const float wave = qSin(phase * 22.0f + timeSeconds_ * 4.8f)
                + qSin(phase * 41.0f - timeSeconds_ * 2.1f) * 0.35f;
            const float y = -0.58f + wave * (0.025f + audioLevel_ * 0.13f);
            vertices.push_back({{x, y}, 0.82f});
        }

        glLineWidth(2.0f + renderScale_ * 1.6f);
        drawVertices(vertices.data(), vertices.size(), GL_LINE_STRIP);
    }

    void drawVertices(const VisualizerVertex* vertices, std::size_t count, GLenum mode)
    {
        if (!vertices || count == 0) {
            return;
        }

        glVertexAttribPointer(positionAttribute_, 2, GL_FLOAT, GL_FALSE, sizeof(VisualizerVertex), &vertices[0].position);
        glVertexAttribPointer(bandAttribute_, 1, GL_FLOAT, GL_FALSE, sizeof(VisualizerVertex), &vertices[0].band);
        glEnableVertexAttribArray(positionAttribute_);
        glEnableVertexAttribArray(bandAttribute_);
        glDrawArrays(mode, 0, static_cast<GLsizei>(count));
        glDisableVertexAttribArray(bandAttribute_);
        glDisableVertexAttribArray(positionAttribute_);
    }

    void submitQueuedAudio()
    {
        if (!projectmRenderer_ || !audioDirty_ || queuedAudioPcm_.empty() || queuedAudioSampleRate_ <= 0 || queuedAudioChannels_ <= 0) {
            return;
        }

        const std::size_t frameCount = queuedAudioPcm_.size() / static_cast<std::size_t>(queuedAudioChannels_);
        projectmRenderer_->submitAudio(queuedAudioPcm_.data(), frameCount, queuedAudioSampleRate_, queuedAudioChannels_);
        lastSubmittedAudioFrameCount_ = frameCount;
        lastSubmittedAudioSampleRate_ = queuedAudioSampleRate_;
        lastSubmittedAudioChannels_ = queuedAudioChannels_;
        audioDirty_ = false;
    }

    void drainPresetFailure()
    {
        if (!projectmRenderer_) {
            return;
        }

        const std::optional<ProjectMRenderer::PresetFailure> failure = projectmRenderer_->takeLastPresetFailure();
        if (!failure) {
            return;
        }

        const QString failedPath = QString::fromStdString(failure->presetPath.string());
        if (!failedPath.isEmpty()) {
            failedPresetPaths_.insert(failedPath.toStdString());
        }

        const QString message = QString::fromStdString(failure->message);
        lastFailureText_ = QStringLiteral("%1: %2").arg(presetFileName(failedPath), message);
        renderConfirmationPosted_ = true;
        postPresetFailure(failedPath, message);
    }

    void maybeConfirmPresetRender()
    {
        if (renderConfirmationPosted_ || presetPath_.isEmpty() || failedPresetPaths_.contains(presetPath_.toStdString())) {
            return;
        }
        if (framesSincePresetLoad_ < 90 || lastGlErrorText_ != QStringLiteral("ok")) {
            return;
        }

        renderConfirmationPosted_ = true;
        const QString message = QStringLiteral("Rendered %1 frames, audio=%2, glerr=ok")
            .arg(framesSincePresetLoad_)
            .arg(audioLevel_, 0, 'f', 3);
        postPresetRenderConfirmed(presetPath_, message);
    }

    void updateGlitchDetection()
    {
        if (presetPath_.isEmpty() || failedPresetPaths_.contains(presetPath_.toStdString())) {
            consecutiveGlErrorFrames_ = 0;
            return;
        }

        if (lastGlErrorText_ == QStringLiteral("ok") || lastGlErrorText_.isEmpty()) {
            consecutiveGlErrorFrames_ = 0;
            return;
        }

        ++consecutiveGlErrorFrames_;
        if (glErrorFailurePosted_ || framesSincePresetLoad_ < 8 || consecutiveGlErrorFrames_ < 12) {
            return;
        }

        glErrorFailurePosted_ = true;
        renderConfirmationPosted_ = true;
        failedPresetPaths_.insert(presetPath_.toStdString());
        const QString message = QStringLiteral("Skipped render glitch: repeated GL error %1").arg(lastGlErrorText_);
        lastFailureText_ = QStringLiteral("%1: %2").arg(presetFileName(presetPath_), message);
        postPresetFailure(presetPath_, message);
    }

    void postStatus(const QString& message)
    {
        if (!visualizerItem_) {
            return;
        }
        QMetaObject::invokeMethod(
            visualizerItem_,
            [item = visualizerItem_, message]() {
                if (item) {
                    item->setStatusMessage(message);
                }
            },
            Qt::QueuedConnection);
    }

    void postPresetFailure(const QString& presetPath, const QString& message)
    {
        if (!visualizerItem_) {
            return;
        }

        const QString displayMessage = QStringLiteral("%1: %2").arg(presetFileName(presetPath), message);
        QMetaObject::invokeMethod(
            visualizerItem_,
            [item = visualizerItem_, presetPath, message, displayMessage]() {
                if (item) {
                    item->setStatusMessage(displayMessage);
                    emit item->presetFailed(presetPath, message);
                }
            },
            Qt::QueuedConnection);
    }

    void postPresetRenderConfirmed(const QString& presetPath, const QString& message)
    {
        if (!visualizerItem_) {
            return;
        }

        QMetaObject::invokeMethod(
            visualizerItem_,
            [item = visualizerItem_, presetPath, message]() {
                if (item) {
                    emit item->presetRenderConfirmed(presetPath, message);
                }
            },
            Qt::QueuedConnection);
    }

    static QString presetFileName(const QString& path)
    {
        if (path.isEmpty()) {
            return QStringLiteral("Preset");
        }
        const std::filesystem::path presetPath(path.toStdString());
        const std::string filename = presetPath.filename().string();
        return filename.empty() ? path : QString::fromStdString(filename);
    }

    void maybePostDiagnostics(const QString& backend)
    {
        const std::uint64_t frameCount = backend == QStringLiteral("projectM") ? projectmFrameCount_ : fallbackFrameCount_;
        if (frameCount == lastDiagnosticsFrameCount_ && backend == lastDiagnosticsBackend_) {
            return;
        }
        if (frameCount % 30 != 0 && backend == lastDiagnosticsBackend_) {
            return;
        }

        lastDiagnosticsFrameCount_ = frameCount;
        lastDiagnosticsBackend_ = backend;

        QStringList parts;
        parts << QStringLiteral("backend=%1").arg(backend);
        parts << QStringLiteral("projectM=%1").arg(projectmInitialized_ ? QStringLiteral("ready") : projectmInitFailed_ ? QStringLiteral("failed") : QStringLiteral("pending"));
        parts << QStringLiteral("frames=%1").arg(projectmFrameCount_);
        parts << QStringLiteral("presetFrames=%1").arg(framesSincePresetLoad_);
        parts << QStringLiteral("audio=%1").arg(audioLevel_, 0, 'f', 3);
        parts << QStringLiteral("pcm=%1/%2ch@%3")
            .arg(static_cast<qulonglong>(lastSubmittedAudioFrameCount_))
            .arg(lastSubmittedAudioChannels_)
            .arg(lastSubmittedAudioSampleRate_);
        parts << QStringLiteral("fb=%1 r%2 d%3")
            .arg(lastFramebufferHandle_)
            .arg(lastReadFramebuffer_)
            .arg(lastDrawFramebuffer_);
        parts << QStringLiteral("glerr=%1").arg(lastGlErrorText_.isEmpty() ? QStringLiteral("n/a") : lastGlErrorText_);
        if (!glVersionText_.isEmpty()) {
            parts << QStringLiteral("gl=%1").arg(glVersionText_);
        }
        if (!glslVersionText_.isEmpty()) {
            parts << QStringLiteral("glsl=%1").arg(glslVersionText_);
        }
        parts << QStringLiteral("loads=%1").arg(presetLoadAttemptCount_);
        if (!lastPresetLoadPath_.isEmpty()) {
            parts << QStringLiteral("preset=%1").arg(presetFileName(lastPresetLoadPath_));
        }
        if (!lastFailureText_.isEmpty()) {
            parts << QStringLiteral("lastFailure=%1").arg(lastFailureText_);
        }
        postDiagnostics(parts.join(QStringLiteral("  |  ")));
    }

    void updateFramebufferDiagnostics()
    {
        lastFramebufferHandle_ = framebufferObject() ? static_cast<int>(framebufferObject()->handle()) : 0;
        GLint readFramebuffer = 0;
        GLint drawFramebuffer = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        lastReadFramebuffer_ = readFramebuffer;
        lastDrawFramebuffer_ = drawFramebuffer;
    }

    QString collectGlErrorText()
    {
        QStringList errors;
        for (int i = 0; i < 8; ++i) {
            const GLenum error = glGetError();
            if (error == GL_NO_ERROR) {
                break;
            }
            errors << glErrorName(error);
        }
        return errors.isEmpty() ? QStringLiteral("ok") : errors.join(QLatin1Char(','));
    }

    static QString glErrorName(GLenum error)
    {
        switch (error) {
        case GL_INVALID_ENUM:
            return QStringLiteral("invalid_enum");
        case GL_INVALID_VALUE:
            return QStringLiteral("invalid_value");
        case GL_INVALID_OPERATION:
            return QStringLiteral("invalid_operation");
        case GL_OUT_OF_MEMORY:
            return QStringLiteral("out_of_memory");
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return QStringLiteral("invalid_framebuffer");
        default:
            return QStringLiteral("0x%1").arg(static_cast<unsigned int>(error), 0, 16);
        }
    }

    void postDiagnostics(const QString& text)
    {
        if (!visualizerItem_) {
            return;
        }
        QMetaObject::invokeMethod(
            visualizerItem_,
            [item = visualizerItem_, text]() {
                if (item) {
                    item->setDiagnosticsText(text);
                }
            },
            Qt::QueuedConnection);
    }

    QString glString(GLenum name)
    {
        const GLubyte* value = glGetString(name);
        return value ? QString::fromLatin1(reinterpret_cast<const char*>(value)) : QStringLiteral("unavailable");
    }

    std::vector<std::filesystem::path> textureSearchPaths() const
    {
        std::vector<std::filesystem::path> paths;
        const auto appendUnique = [&paths](const std::filesystem::path& path) {
            if (path.empty()) {
                return;
            }
            if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
                paths.push_back(path);
            }
        };

        if (!presetPath_.isEmpty()) {
            const std::filesystem::path presetPath(presetPath_.toStdString());
            appendUnique(presetPath.parent_path());
        }
        if (!textureRoot_.isEmpty()) {
            appendUnique(std::filesystem::path(textureRoot_.toStdString()));
        }
        return paths;
    }

    std::unique_ptr<ProjectMRenderer> projectmRenderer_;
    QPointer<OpenGlVisualizerItem> visualizerItem_;
    std::unique_ptr<QOpenGLShaderProgram> program_;
    QSize lastProjectMSize_;
    QString presetPath_;
    QString textureRoot_;
    QString lastPresetLoadPath_;
    QString lastFailureText_;
    QString lastGlErrorText_ = QStringLiteral("n/a");
    QString glVersionText_;
    QString glslVersionText_;
    std::set<std::string> failedPresetPaths_;
    std::vector<float> queuedAudioPcm_;
    int positionAttribute_ = -1;
    int bandAttribute_ = -1;
    float timeSeconds_ = 0.0f;
    float audioLevel_ = 0.0f;
    float renderScale_ = 1.0f;
    float presetDurationSeconds_ = 30.0f;
    float transitionDurationSeconds_ = 3.0f;
    int queuedAudioSampleRate_ = 0;
    int queuedAudioChannels_ = 0;
    int lastSubmittedAudioSampleRate_ = 0;
    int lastSubmittedAudioChannels_ = 0;
    int lastFramebufferHandle_ = 0;
    int lastReadFramebuffer_ = 0;
    int lastDrawFramebuffer_ = 0;
    int consecutiveGlErrorFrames_ = 0;
    std::size_t lastQueuedAudioFrameCount_ = 0;
    std::size_t lastSubmittedAudioFrameCount_ = 0;
    bool lowPowerMode_ = false;
    bool fallbackInitialized_ = false;
    bool projectmAttempted_ = false;
    bool projectmInitialized_ = false;
    bool projectmInitFailed_ = false;
    bool projectmActive_ = false;
    bool presetDirty_ = true;
    bool timingDirty_ = true;
    bool texturePathsDirty_ = true;
    bool audioDirty_ = false;
    bool renderConfirmationPosted_ = false;
    bool glErrorFailurePosted_ = false;
    std::uint64_t projectmFrameCount_ = 0;
    std::uint64_t fallbackFrameCount_ = 0;
    std::uint64_t presetLoadAttemptCount_ = 0;
    std::uint64_t framesSincePresetLoad_ = 0;
    std::uint64_t lastDiagnosticsFrameCount_ = 0;
    QString lastDiagnosticsBackend_;
};
}

OpenGlVisualizerItem::OpenGlVisualizerItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent)
{
    setMirrorVertically(true);
}

QQuickFramebufferObject::Renderer* OpenGlVisualizerItem::createRenderer() const
{
    return new OpenGlVisualizerRenderer();
}

float OpenGlVisualizerItem::audioLevel() const
{
    return audioLevel_;
}

float OpenGlVisualizerItem::renderScale() const
{
    return renderScale_;
}

bool OpenGlVisualizerItem::lowPowerMode() const
{
    return lowPowerMode_;
}

QString OpenGlVisualizerItem::presetPath() const
{
    return presetPath_;
}

QString OpenGlVisualizerItem::textureRoot() const
{
    return textureRoot_;
}

float OpenGlVisualizerItem::presetDurationSeconds() const
{
    return presetDurationSeconds_;
}

float OpenGlVisualizerItem::transitionDurationSeconds() const
{
    return transitionDurationSeconds_;
}

QString OpenGlVisualizerItem::statusMessage() const
{
    return statusMessage_;
}

QString OpenGlVisualizerItem::diagnosticsText() const
{
    return diagnosticsText_;
}

bool OpenGlVisualizerItem::takePendingAudio(QVector<float>& pcm, int& sampleRate, int& channels)
{
    QMutexLocker locker(&audioMutex_);
    if (!pendingAudioDirty_) {
        return false;
    }

    pcm = std::move(pendingAudioPcm_);
    sampleRate = pendingAudioSampleRate_;
    channels = pendingAudioChannels_;
    pendingAudioSampleRate_ = 0;
    pendingAudioChannels_ = 0;
    pendingAudioDirty_ = false;
    return !pcm.isEmpty() && sampleRate > 0 && channels > 0;
}

void OpenGlVisualizerItem::submitAudioBlock(const QVector<float>& pcm, int sampleRate, int channels)
{
    if (pcm.isEmpty() || sampleRate <= 0 || channels <= 0) {
        return;
    }

    {
        QMutexLocker locker(&audioMutex_);
        pendingAudioPcm_ = pcm;
        pendingAudioSampleRate_ = sampleRate;
        pendingAudioChannels_ = channels;
        pendingAudioDirty_ = true;
    }
    update();
}

void OpenGlVisualizerItem::setAudioLevel(float level)
{
    const float clamped = std::clamp(level, 0.0f, 1.0f);
    if (qFuzzyCompare(audioLevel_, clamped)) {
        return;
    }
    audioLevel_ = clamped;
    update();
    emit visualStateChanged();
}

void OpenGlVisualizerItem::setPresetPath(const QString& path)
{
    if (presetPath_ == path) {
        return;
    }
    presetPath_ = path;
    update();
    emit visualStateChanged();
}

void OpenGlVisualizerItem::setTextureRoot(const QString& path)
{
    if (textureRoot_ == path) {
        return;
    }
    textureRoot_ = path;
    update();
    emit visualStateChanged();
}

void OpenGlVisualizerItem::setPresetDurationSeconds(float seconds)
{
    const float clamped = std::clamp(seconds, 1.0f, 600.0f);
    if (qFuzzyCompare(presetDurationSeconds_, clamped)) {
        return;
    }
    presetDurationSeconds_ = clamped;
    update();
    emit visualStateChanged();
}

void OpenGlVisualizerItem::setTransitionDurationSeconds(float seconds)
{
    const float clamped = std::clamp(seconds, 0.0f, 60.0f);
    if (qFuzzyCompare(transitionDurationSeconds_, clamped)) {
        return;
    }
    transitionDurationSeconds_ = clamped;
    update();
    emit visualStateChanged();
}

void OpenGlVisualizerItem::setStatusMessage(const QString& message)
{
    if (statusMessage_ == message) {
        return;
    }
    statusMessage_ = message;
    emit statusMessageChanged();
}

void OpenGlVisualizerItem::setDiagnosticsText(const QString& text)
{
    if (diagnosticsText_ == text) {
        return;
    }
    diagnosticsText_ = text;
    emit diagnosticsTextChanged();
}

void OpenGlVisualizerItem::setRenderScale(float scale)
{
    const float clamped = std::clamp(scale, 0.5f, 1.0f);
    if (qFuzzyCompare(renderScale_, clamped)) {
        return;
    }
    renderScale_ = clamped;
    update();
    emit visualStateChanged();
}

void OpenGlVisualizerItem::setLowPowerMode(bool enabled)
{
    if (lowPowerMode_ == enabled) {
        return;
    }
    lowPowerMode_ = enabled;
    update();
    emit visualStateChanged();
}
