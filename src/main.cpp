#include "app/AppController.h"
#include "render/OpenGlVisualizerItem.h"

#include <QGuiApplication>
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

    engine.loadFromModule(QStringLiteral("MilkRunnerVisualizer"), QStringLiteral("Main"));
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
    return app.exec();
}
