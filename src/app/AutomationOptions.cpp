#include "AutomationOptions.h"
#include "presets/PresetScanner.h"
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QSet>

bool AutomationOptions::parse(const QStringList& arguments, QString& error)
{
    error.clear();
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    // Preserve common Qt launch options; QGuiApplication still consumes them.
    for (const QString& name : QStringList{"platform", "platformpluginpath", "platformtheme", "plugin", "qmljsdebugger", "display", "geometry", "qwindowgeometry", "qwindowtitle", "qwindowicon", "session", "style"}) {
        parser.addOption({name, "Qt launch option", "value"});
    }
    parser.addOption({"reverse", "Qt right-to-left layout option"});
    parser.addOption({"preset", "Load one exact .milk file; repeat for a deterministic sequence.", "file"});
    parser.addOption({"list-presets", "List scanned preset identities as JSON without a desktop.", "folder"});
    parser.addOption({"preset-interval", "Seconds per preset in a sequence (5–120).", "seconds", "5"});
    parser.addOption({"readiness-timeout", "Seconds allowed for actual render confirmation, independent of preset duration (1–180).", "seconds", "120"});
    parser.addOption({"automation-status", "Write atomic JSON state/confirmed identities here.", "file"});
    parser.addOption({"run-seconds", "Graceful timed shutdown (1..3600 seconds).", "seconds"});
    parser.addOption({"audio-file", "Decode local music directly to projectM.", "file"});
    parser.addOption({"audio-loop", "Loop file playback."});
    parser.addOption({"audio-source", "Explicit source: file, system, demo, live or scion.", "source"});
    parser.addOption({"audio-device", "System monitor name or default.", "device", "default"});
    parser.addOption({"list-audio-devices", "List Linux monitor devices as JSON."});
    parser.addOption({"demo-audio", "Use deterministic demo audio for visual tests."});
    parser.addOption({{"h", "help"}, "Show automation options."});
    if (!parser.parse(arguments)) { error = parser.errorText(); return false; }
    if (!parser.positionalArguments().isEmpty()) { error = "Unexpected positional argument"; return false; }
    help = parser.isSet("help");
    presets = parser.values("preset");
    listFolder = parser.value("list-presets");
    statusFile = parser.value("automation-status");
    demoAudio = parser.isSet("demo-audio");
    audioFile=parser.value("audio-file");audioLoop=parser.isSet("audio-loop");
    audioSource=parser.value("audio-source");audioDevice=parser.value("audio-device");listAudioDevices=parser.isSet("list-audio-devices");
    if (!audioFile.isEmpty()) {
        QFile file(audioFile);
        if (!QFileInfo(audioFile).isFile() || !file.open(QIODevice::ReadOnly)) { error="Audio file is missing or unreadable: "+audioFile;return false; }
        if (!audioSource.isEmpty() && audioSource!="file") {error="--audio-file conflicts with --audio-source";return false;}
        audioSource="file";
    }
    if ((!audioSource.isEmpty() && !QStringList{"file","system","demo","live","scion"}.contains(audioSource)) ||
        (audioSource=="file" && audioFile.isEmpty()) || (audioLoop && audioFile.isEmpty()) ||
        (demoAudio && !audioSource.isEmpty() && audioSource!="demo")) {error="Invalid or conflicting audio source options";return false;}
    if (parser.isSet("run-seconds")) { bool ok=false;runSeconds=parser.value("run-seconds").toInt(&ok);if(!ok || runSeconds<1 || runSeconds>3600){error="Invalid --run-seconds";return false;} }
    bool valid = false;
    intervalSeconds = parser.value("preset-interval").toInt(&valid);
    if (!valid || intervalSeconds < 5 || intervalSeconds > 120 || presets.size() > 10) { error = "Invalid sequence timing/count"; return false; }
    if ((!listFolder.isEmpty() && !presets.isEmpty()) || (!statusFile.isEmpty() && presets.isEmpty())) { error = "Status requires presets; listing cannot launch presets"; return false; }
    readinessTimeoutSeconds = parser.value("readiness-timeout").toInt(&valid);
    if (!valid || readinessTimeoutSeconds < 1 || readinessTimeoutSeconds > 180) { error="Invalid --readiness-timeout (1–180 seconds)"; return false; }
    QSet<QString> hashes;
    for (QString& preset : presets) {
        const auto info = presetInfo(preset, error);
        if (!error.isEmpty()) return false;
        const auto hash = info.value("sha256").toString();
        if (hashes.contains(hash)) { error = "Sequence requires distinct preset contents"; return false; }
        hashes.insert(hash);
        preset = info.value("path").toString();
    }
    if (!listFolder.isEmpty() && !QFileInfo(listFolder).isDir()) { error = "Preset folder unavailable"; return false; }
    return true;
}

QJsonObject AutomationOptions::presetInfo(const QString& path, QString& error)
{
    error.clear();
    QFileInfo info(path);
    QFile file(path);
    if (!info.isFile() || !PresetScanner::isPresetFile(info) || PresetScanner::shouldIgnorePresetFile(info) || info.size() > 4 * 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
        error = "Preset must be a readable .milk file (max 4 MiB), not an ignored test fixture";
        return {};
    }
    const auto bytes = file.read(4 * 1024 * 1024 + 1);
    if (bytes.size() > 4 * 1024 * 1024) { error = "Preset exceeds 4 MiB"; return {}; }
    return {{"path", info.canonicalFilePath()}, {"filename", info.fileName()}, {"sha256", QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())}};
}
