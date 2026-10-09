#include "app/AutomationOptions.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv); QTemporaryDir dir;
    const auto preset = dir.filePath("original.milk");
    QFile f(preset); if (!f.open(QIODevice::WriteOnly)) return 1; f.write("[preset00]\nfRating=3.0\n"); f.close();
    QString error; AutomationOptions options;
    if (!options.parse({"test", "--preset", preset, "--automation-status", dir.filePath("status.json")}, error)) return 2;
    if (options.presets.size() != 1 || options.intervalSeconds != 5) return 3;
    AutomationOptions qtOptions;
    if (!qtOptions.parse({"test", "-platform", "xcb", "--preset", preset}, error)) return 6;
    QStringList excessive{"test"}; for (int i = 0; i < 11; ++i) excessive << "--preset" << preset;
    AutomationOptions invalidCount;
    if (invalidCount.parse(excessive,error)) return 7;
    error.clear();
    const auto info = AutomationOptions::presetInfo(preset, error);
    if (info.value("sha256").toString().size() != 64) return 4;
    for (const auto& args : QList<QStringList>{{"test", "--preset", "/missing.milk"}, {"test", "--preset-interval", "0"}, {"test", "--preset", preset, "--preset", preset}, {"test", "--automation-status", "status"}, {"test", "--made-up"}, {"test","--audio-loop"}, {"test","--audio-source","file"}, {"test","--audio-source","unknown"}, {"test","--audio-file","/missing.mp3"}, {"test","--audio-source","system","--demo-audio"}}) {
        AutomationOptions invalid; error.clear(); if (invalid.parse(args, error) || error.isEmpty()) return 5;
    }
    QTextStream(stdout) << "Automation option/path/hash/bounds tests passed\n"; return 0;
}
