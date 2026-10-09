#include "audio/DemoAudioInput.h"
#include "audio/FileAudioInput.h"
#include "audio/LinuxSystemAudioInput.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <cmath>
#include <memory>
#include <stdexcept>
static void require(bool test, const char* message)
{
    if (!test)
        throw std::runtime_error(message);
}
static void wait(int ms, bool slow = false)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
    {
        QCoreApplication::processEvents();
        QThread::msleep(slow ? 500 : 5);
    }
    QCoreApplication::processEvents();
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try
    {
        if (argc > 1 && QString(argv[1]) == "--system")
        {
            LinuxSystemAudioInput input;
            input.start();
            wait(2500);
            if (input.devices().isEmpty())
            {
                QTextStream(stdout) << "SKIP: no PulseAudio monitor server\n";
                return 77;
            }
            require(input.isRunning(), "capture did not connect");
            QProcess play;
            play.start("paplay", {"--raw", "--rate=44100", "--channels=2", "--format=float32le"});
            require(play.waitForStarted(), "paplay missing");
            QByteArray tone(44100 * 2 * 4 * 3, 0);
            auto* data = reinterpret_cast<float*>(tone.data());
            for (int i = 0; i < 44100 * 3; ++i)
                data[i * 2] = data[i * 2 + 1] = 0.2f * std::sin(i * 440 * 6.2831853 / 44100);
            play.write(tone);
            play.closeWriteChannel();
            double peak = 0;
            QElapsedTimer clock;
            clock.start();
            while (clock.elapsed() < 3500)
            {
                wait(20);
                peak = std::max(peak, input.diagnostics()["peak"].toDouble());
            }
            require(peak > 0.1, "monitor did not receive non-silent PCM");
            require(input.diagnostics()["pcm_frames_decoded"].toDouble() > 44100,
                    "insufficient capture frames");
            const int beforeStopErrors = input.diagnostics()["errors"].toInt();
            input.stop();
            require(input.diagnostics()["errors"].toInt() == beforeStopErrors,
                    "intentional shutdown must not count as capture failure");
            require(!input.isRunning(), "capture did not stop");
            QTextStream(stdout) << "PASS system monitor capture\n";
            return 0;
        }
        if (!FileAudioInput::isSupported())
        {
            QTextStream(stdout) << "SKIP FFmpeg disabled\n";
            return 77;
        }
        QTemporaryDir dir;
        require(dir.isValid(), "temporary directory");
        for (const QString ext : QStringList{"wav", "mp3", "flac"})
        {
            const QString file = dir.filePath("tone." + ext);
            QProcess ffmpeg;
            ffmpeg.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i",
                                    "sine=frequency=440:sample_rate=48000:duration=0.3", "-ac",
                                    ext == "flac" ? "2" : "1", "-y", file});
            require(ffmpeg.waitForFinished(10000) && ffmpeg.exitCode() == 0,
                    "generate audio fixture");
            FileAudioInput input(file, true);
            qint64 frames = 0;
            double peak = 0;
            QObject::connect(&input, &AudioInput::audioReady, &app,
                             [&](const QVector<float>& pcm, int rate, int channels) {
                                 require(rate == 44100 && channels == 2, "output format");
                                 frames += pcm.size() / channels;
                                 for (float x : pcm)
                                     peak = std::max(peak, std::abs(double(x)));
                             });
            require(input.start(), "file start");
            wait(1600, true);
            auto d = input.diagnostics();
            require(d["pcm_frames_decoded"].toDouble() > 44100 * 1.4 &&
                        d["pcm_frames_decoded"].toDouble() < 44100 * 2.2,
                    "audio must advance in real time with 2 FPS GUI");
            require(d["loop_count"].toInt() >= 3, "loop count");
            require(frames > 0 && peak > 0.01, "genuine PCM");
            require(d["input_sample_rate"] == 48000 &&
                        d["input_channels"] == (ext == "flac" ? 2 : 1),
                    "input facts");
            QThread::msleep(1300);
            QCoreApplication::processEvents();
            require(input.diagnostics()["pcm_frames_dropped"].toDouble() > 0,
                    "blocked GUI must have bounded buffer with explicit drops");
            input.stop();
            require(!input.isRunning(), "clean shutdown");
            require(input.start(), "restart");
            wait(50);
            input.stop();
            FileAudioInput finite(file, false);
            require(finite.start(), "finite start");
            wait(700);
            require(finite.diagnostics()["audio_status"] == "ended", "EOF status");
            finite.stop();
            QTextStream(stdout) << "PASS " << ext
                                << " decode/resample/mono/loop/2FPS/restart/EOF\n";
        }
        qint64 oldFileFrames = 0, demoFrames = 0;
        std::unique_ptr<AudioInput> selected =
            std::make_unique<FileAudioInput>(dir.filePath("tone.wav"), true);
        QObject::connect(selected.get(), &AudioInput::audioReady, &app,
                         [&](const QVector<float>& pcm, int, int channels) {
                             oldFileFrames += pcm.size() / channels;
                         });
        require(selected->start(), "switch source file start");
        wait(100);
        require(oldFileFrames > 0, "file delivers before switch");
        selected->stop();
        selected.reset();
        const qint64 stoppedFrames = oldFileFrames;
        selected = std::make_unique<DemoAudioInput>();
        QObject::connect(selected.get(), &AudioInput::audioReady, &app,
                         [&](const QVector<float>& pcm, int, int channels) {
                             demoFrames += pcm.size() / channels;
                         });
        require(selected->start(), "switch source demo start");
        wait(100);
        require(demoFrames > 0 && oldFileFrames == stoppedFrames,
                "switch must stop old PCM and deliver new source");
        selected->stop();
        selected.reset();
        QTextStream(stdout) << "PASS file-to-demo source replacement without stale PCM\n";
        FileAudioInput missing(dir.filePath("missing.mp3"), false);
        require(!missing.start(), "missing rejected");
        QFile invalid(dir.filePath("invalid.mp3"));
        invalid.open(QIODevice::WriteOnly);
        invalid.write("not audio");
        invalid.close();
        FileAudioInput bad(invalid.fileName(), false);
        require(bad.start(), "async invalid accepted for validation");
        wait(500);
        require(bad.diagnostics()["audio_status"] == "error", "malformed audio error");
        bad.stop();
        {
            LinuxSystemAudioInput absent;
            absent.start();
            wait(100);
            absent.stop();
        }
        QTextStream(stdout) << "PASS invalid/missing/capture shutdown\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        QTextStream(stderr) << "FAIL " << e.what() << '\n';
        return 1;
    }
}
