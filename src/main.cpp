#include "app/AppController.h"
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
                              "[--preset-interval 5..120] [--automation-status FILE] [--demo-audio]\n";
        return 0;
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

    AppController controller;

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
        struct State { int index = -1; bool confirmed = false; QString phase; QString error; QJsonArray events; };
        auto state = std::make_shared<State>();
        auto writeStatus = [&, state, visual]() {
            if (automation.statusFile.isEmpty()) return true;
            QSaveFile file(automation.statusFile);
            QJsonObject status{{"state", state->phase}, {"error", state->error},
                {"updated_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                {"active_preset", controller.activePresetPath()}, {"renderer_diagnostics", visual->diagnosticsText()},
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
        auto load = [&, state, writeStatus, fail]() {
            ++state->index;
            if (state->index >= automation.presets.size()) return;
            state->confirmed = false; state->phase = "requested";
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
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [state, writeStatus]() {
            if (state->phase != "failed") state->phase = "stopped";
            writeStatus();
        });
        controller.setPresetLocked(true);
        if (automation.demoAudio) controller.useDemoAudio();
        QObject::connect(visual, &OpenGlVisualizerItem::presetFailed, &app, [&, state, fail](const QString& path, const QString& message) {
            if (state->index >= 0 && QFileInfo(path).canonicalFilePath() == automation.presets.at(state->index)) fail("Preset render failed: " + path + ": " + message);
        });
        QObject::connect(visual, &OpenGlVisualizerItem::presetRenderConfirmed, &app, [&, state, writeStatus, fail](const QString& path, const QString& message) {
            if (state->index < 0 || state->confirmed || QFileInfo(path).canonicalFilePath() != automation.presets.at(state->index)) return;
            state->confirmed = true; state->phase = "render_confirmed";
            auto event = state->events.last().toObject();
            event.insert("confirmed_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)); event.insert("message", message);
            state->events.replace(state->events.size() - 1, event);
            if (!writeStatus()) fail("Cannot write automation status");
        });
        auto* sequence = new QTimer(&app);
        QObject::connect(sequence, &QTimer::timeout, &app, [&, state, load, fail, sequence]() {
            if (!state->confirmed) { sequence->stop(); fail("Preset was not render-confirmed within its interval"); return; }
            if (state->index + 1 < automation.presets.size()) load();
            else sequence->stop(); // Hold final preset until managed stop.
        });
        sequence->start(automation.intervalSeconds * 1000);
        QTimer::singleShot(0, &app, load);
    }
    return app.exec();
}
