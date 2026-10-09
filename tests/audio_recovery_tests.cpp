#include "audio/LinuxSystemAudioInput.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
static void wait(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
    {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (!LinuxSystemAudioInput::isSupported() ||
        QProcess::execute("sh", {"-c", "command -v pulseaudio >/dev/null"}) != 0)
        return 77;
    QTemporaryDir dir;
    QFile config(dir.filePath("test.pa"));
    config.open(QIODevice::WriteOnly);
    config.write(".fail\nload-module module-native-protocol-unix auth-anonymous=0\nload-module "
                 "module-null-sink sink_name=first\nload-module module-null-sink "
                 "sink_name=second\nset-default-sink first\n");
    config.close();
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/mycelium-audio-test-bus");
    qputenv("XDG_RUNTIME_DIR", dir.path().toUtf8());
    qputenv("PULSE_SERVER", ("unix:" + dir.path() + "/pulse/native").toUtf8());
    LinuxSystemAudioInput input;
    input.start();
    wait(400);
    if (input.isRunning())
        return 1;
    QProcess server;
    auto start = [&] {
        server.start("pulseaudio", {"-n", "--daemonize=no", "--exit-idle-time=-1",
                                    "--use-pid-file=no", "--file=" + config.fileName()});
        return server.waitForStarted();
    };
    if (!start())
        return 77;
    wait(3000);
    if (!input.isRunning() || input.diagnostics()["monitor_device"] != "first.monitor")
    {
        QTextStream(stderr) << QJsonDocument(input.diagnostics()).toJson()
                            << server.readAllStandardError();
        return 2;
    }
    if (QProcess::execute("pactl", {"set-default-sink", "second"}) != 0)
        return 3;
    wait(1000);
    if (input.diagnostics()["monitor_device"] != "second.monitor")
        return 4;
    if (QProcess::execute("pactl", {"unload-module", "module-null-sink"}) != 0)
        return 8;
    wait(1000);
    if (input.isRunning())
        return 9;
    if (QProcess::execute("pactl", {"load-module", "module-null-sink", "sink_name=recovered"}) != 0)
        return 10;
    wait(2500);
    if (!input.isRunning() || input.diagnostics()["monitor_device"] != "recovered.monitor")
        return 11;
    server.terminate();
    server.waitForFinished(5000);
    wait(500);
    if (input.isRunning())
        return 5;
    if (!start())
        return 6;
    wait(3000);
    if (!input.isRunning())
        return 7;
    input.stop();
    server.terminate();
    server.waitForFinished(5000);
    QTextStream(stdout) << "PASS absent server / late server / default change / device removal / "
                           "new device / disconnect / reconnect / shutdown\n";
    return 0;
}
