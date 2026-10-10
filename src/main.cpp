#include "app/AppController.h"
#include "app/ProcessMetrics.h"
#include "audio/LinuxSystemAudioInput.h"
#include "app/AutomationOptions.h"
#include "presets/PresetScanner.h"
#include "render/OpenGlVisualizerItem.h"

#include <QGuiApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>
#include <memory>
#include <csignal>
namespace { volatile std::sig_atomic_t terminateRequested = 0; void requestTermination(int) { terminateRequested = 1; } }
#include <QPalette>
#include <QColor>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QQuickStyle>
#include <QSurfaceFormat>

int main(int argc, char* argv[])
{
    QStringList arguments;
    for (int i = 0; i < argc; ++i) arguments.append(QString::fromLocal8Bit(argv[i]));
    AutomationOptions automation;
    QString automationError;
    if (!automation.parse(arguments, automationError)) {
        QTextStream(stderr) << automationError << '\n';
        return 2;
    }
    if (automation.help) {
        QTextStream(stdout) << "--list-presets FOLDER | --preset FILE [--preset FILE ...] "
                              "[--preset-interval 5..120] [--readiness-timeout 1..180] [--automation-status FILE] [--demo-audio] [--audio-file FILE --audio-loop] [--audio-source system --audio-device default] [--list-audio-devices] [--run-seconds 1..3600]\n";
        return 0;
    }
    if (automation.listAudioDevices) {
        QCoreApplication core(argc,argv);LinuxSystemAudioInput input(automation.audioDevice);
        if(!input.start()){QTextStream(stderr)<<"Linux system audio unavailable\n";return 3;}
        QTimer::singleShot(1500,&core,[&]{QJsonArray devices;for(const auto& d:input.devices())devices.append(d);
          auto status=input.diagnostics();status["devices"]=devices;QTextStream(stdout)<<QJsonDocument(status).toJson(QJsonDocument::Compact)<<'\n';core.exit(devices.isEmpty()?3:0);});
        return core.exec();
    }
    if (!automation.listFolder.isEmpty()) {
        QCoreApplication core(argc, argv);
        QJsonArray presets;
        for (const auto& preset : PresetScanner::scanFolder(automation.listFolder)) {
            QString error;
            const auto identity = AutomationOptions::presetInfo(preset.path, error);
            if (error.isEmpty()) presets.append(identity);
        }
        QTextStream(stdout) << QJsonDocument(QJsonObject{{"presets", presets}}).toJson(QJsonDocument::Compact) << '\n';
        return 0;
    }
#ifndef MILK_RUNNER_HAS_PROJECTM
    if (!automation.presets.isEmpty()) { QTextStream(stderr) << "Preset rendering automation requires a projectM-enabled build\n"; return 3; }
#endif
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("MilkRunner"));
    QGuiApplication::setApplicationName(QStringLiteral("Milk Runner Visualizer"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(26, 29, 36));
    darkPalette.setColor(QPalette::WindowText, QColor(242, 244, 248));
    darkPalette.setColor(QPalette::Base, QColor(23, 26, 32));
    darkPalette.setColor(QPalette::AlternateBase, QColor(35, 40, 51));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(32, 36, 46));
    darkPalette.setColor(QPalette::ToolTipText, QColor(242, 244, 248));
    darkPalette.setColor(QPalette::Text, QColor(242, 244, 248));
    darkPalette.setColor(QPalette::Button, QColor(37, 43, 56));
    darkPalette.setColor(QPalette::ButtonText, QColor(242, 244, 248));
    darkPalette.setColor(QPalette::BrightText, Qt::white);
    darkPalette.setColor(QPalette::Link, QColor(110, 168, 255));
    darkPalette.setColor(QPalette::Highlight, QColor(64, 115, 214));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::PlaceholderText, QColor(140, 148, 162));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 128, 142));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 128, 142));
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 128, 142));
    app.setPalette(darkPalette);

    if (!automation.presets.isEmpty()) {
        std::signal(SIGTERM, requestTermination); std::signal(SIGINT, requestTermination);
        auto* termination = new QTimer(&app);
        QObject::connect(termination, &QTimer::timeout, &app, [&app]() { if (terminateRequested) app.quit(); });
        termination->start(100);
    }
    if (automation.runSeconds) QTimer::singleShot(automation.runSeconds*1000,&app,&QCoreApplication::quit);
    AppController controller;
    if (!automation.audioFile.isEmpty()) { if(!controller.useFileAudio(automation.audioFile,automation.audioLoop)) { QTextStream(stderr)<<controller.audioStatusMessage()<<'\n';return 3; } }
    else if (automation.audioSource=="system") {
#ifdef Q_OS_LINUX
        if(!LinuxSystemAudioInput::isSupported()) { QTextStream(stderr)<<"Build has no Linux system capture support\n";return 3; }
        controller.useLinuxSystemAudio(automation.audioDevice);
#else
        controller.useSystemAudio();
        if(controller.audioSourceName()=="Demo audio") { QTextStream(stderr)<<"System audio unavailable\n";return 3; }
#endif
    }
    else if (automation.audioSource=="live") controller.useLiveAudio();
    else if (automation.audioSource=="scion") controller.useScionAudio();
    else if (automation.demoAudio || automation.audioSource=="demo") controller.useDemoAudio();
    if (!automation.audioSource.isEmpty() || !automation.audioFile.isEmpty())
        QObject::connect(&controller,&AppController::audioSourceChanged,&app,[&]{
            const auto d=controller.audioDiagnostics();
            if(d["audio_status"]=="unavailable") QTextStream(stderr)<<d["error"].toString()<<" (will retry)\n";
            if(d["audio_status"]=="error") { QTextStream(stderr)<<d["error"].toString()<<'\n';app.exit(3); }
        });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("presetLibrary"), controller.presetLibrary());
    engine.rootContext()->setContextProperty(QStringLiteral("playlistManager"), controller.playlistManager());

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    engine.loadFromModule(QStringLiteral("MilkRunnerVisualizer"), QStringLiteral("Main"));
#else
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/MilkRunnerVisualizer/src/ui/qml/Main.qml")));
#endif
    if (!engine.rootObjects().isEmpty()) {
        if (auto* visualSurface = engine.rootObjects().first()->findChild<OpenGlVisualizerItem*>(QStringLiteral("visualSurface"))) {
            QObject::connect(
                &controller,
                &AppController::audioBlockReady,
                visualSurface,
                &OpenGlVisualizerItem::submitAudioBlock,
                Qt::QueuedConnection);
            QObject::connect(
                visualSurface,
                &OpenGlVisualizerItem::presetFailed,
                &controller,
                &AppController::handlePresetFailed,
                Qt::QueuedConnection);
            QObject::connect(
                visualSurface,
                &OpenGlVisualizerItem::presetRenderConfirmed,
                &controller,
                &AppController::handlePresetRenderConfirmed,
                Qt::QueuedConnection);
        }
    }
    // Opt-in automation uses the same controller/render path as the UI. No
    // network listener, synthetic display, or keyboard/mouse automation.
    if (!automation.presets.isEmpty()) {
        auto* visual = engine.rootObjects().isEmpty() ? nullptr : engine.rootObjects().first()->findChild<OpenGlVisualizerItem*>("visualSurface");
        if (!visual) { QTextStream(stderr) << "Visual surface unavailable\n"; return 3; }
        visual->setDiagnosticsIntervalFrames(5);
        struct State { int index = -1; bool confirmed = false; QString phase; QString error; QJsonArray events; };
        auto state = std::make_shared<State>();
        auto writeStatus = [&, state, visual]() {
            if (automation.statusFile.isEmpty()) return true;
            QSaveFile file(automation.statusFile);
            QJsonObject status{{"state", state->phase}, {"error", state->error},
                {"updated_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                {"active_preset", controller.activePresetPath()}, {"renderer_diagnostics", visual->diagnosticsText()},
                {"process", processMetrics()}, {"audio", controller.audioDiagnostics()}, {"audio_delivery", visual->audioDiagnostics()},
                {"audio_source", controller.audioSourceName()}, {"audio_running", controller.audioRunning()}, {"events", state->events}};
            const QByteArray bytes = QJsonDocument(status).toJson(QJsonDocument::Compact);
            // Set permissions on the temporary file before atomic commit;
            // the shared workspace group can read evidence, never the world.
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size()
                && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup) && file.commit();
        };
        auto fail = [&, state, writeStatus](const QString& error) {
            state->phase = "failed"; state->error = error;
            writeStatus(); QTextStream(stderr) << error << '\n'; app.exit(3);
        };
        auto* readiness = new QTimer(&app); readiness->setSingleShot(true);
        QObject::connect(readiness, &QTimer::timeout, &app, [fail]() { fail("Preset render readiness timed out"); });
        auto load = [&, state, writeStatus, fail, readiness]() {
            ++state->index;
            if (state->index >= automation.presets.size()) return;
            state->confirmed = false; state->phase = "requested";
            readiness->start(automation.readinessTimeoutSeconds * 1000);
            const QString preset = automation.presets.at(state->index);
            controller.presetLibrary()->setFilterText({});
            controller.presetLibrary()->setFavouritesOnly(false);
            if (!controller.importPresetFolder(QFileInfo(preset).absolutePath())) { fail("Preset import failed"); return; }
            int row = -1;
            for (int i = 0; i < controller.presetLibrary()->count(); ++i) {
                if (QFileInfo(controller.presetLibrary()->get(i).value("path").toString()).canonicalFilePath() == preset) { row = i; break; }
            }
            if (row < 0) { fail("Requested preset not found in scanned library"); return; }
            controller.playPreset(row);
            QString error;
            QJsonObject event = AutomationOptions::presetInfo(preset, error);
            if (!error.isEmpty()) { fail(error); return; }
            event.insert("requested_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
            state->events.append(event);
            if (!writeStatus()) fail("Cannot write automation status");
        };
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&, state, writeStatus]() {
            controller.stopAudio();
            if (state->phase != "failed") state->phase = "stopped";
            writeStatus();
        });
        controller.setPresetLocked(true);

        QObject::connect(visual, &OpenGlVisualizerItem::presetFailed, &app, [&, state, fail](const QString& path, const QString& message) {
            if (state->index >= 0 && QFileInfo(path).canonicalFilePath() == automation.presets.at(state->index)) fail("Preset render failed: " + path + ": " + message);
        });
        QObject::connect(visual, &OpenGlVisualizerItem::presetRenderConfirmed, &app, [&, state, writeStatus, fail, readiness](const QString& path, const QString& message) {
            if (state->index < 0 || state->confirmed || QFileInfo(path).canonicalFilePath() != automation.presets.at(state->index)) return;
            state->confirmed = true; state->phase = "render_confirmed"; readiness->stop();
            auto event = state->events.last().toObject();
            event.insert("confirmed_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)); event.insert("message", message);
            state->events.replace(state->events.size() - 1, event);
            if (!writeStatus()) fail("Cannot write automation status");
        });
        // Keep current diagnostics observable during a short visual sweep even
        // before the renderer's stronger 90-frame confirmation is reached.
        auto* evidence = new QTimer(&app);
        QObject::connect(evidence, &QTimer::timeout, &app, [writeStatus, fail]() {
            if (!writeStatus()) fail("Cannot update automation evidence");
        });
        evidence->start(1000);
        auto* sequence = new QTimer(&app);
        QObject::connect(sequence, &QTimer::timeout, &app, [&, state, load, fail, sequence]() {
            if (!state->confirmed) return; // Readiness has its own timeout; slow software rendering is valid.
            if (state->index + 1 < automation.presets.size()) load();
            else sequence->stop(); // Hold final preset until managed stop.
        });
        sequence->start(automation.intervalSeconds * 1000);
        QTimer::singleShot(0, &app, load);
    }
    return app.exec();
}
