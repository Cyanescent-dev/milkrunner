#include "Playlist.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

QJsonObject Playlist::toJson(const QString& basePresetFolder) const
{
    QJsonArray presetArray;
    for (const PlaylistEntry& preset : presets) {
        QJsonObject presetObject;
        presetObject.insert(QStringLiteral("path"), pathForJson(preset.path, basePresetFolder));
        presetObject.insert(QStringLiteral("title"), preset.title);
        presetObject.insert(QStringLiteral("author"), preset.author.isEmpty() ? QStringLiteral("Unknown") : preset.author);
        presetObject.insert(QStringLiteral("favourite"), preset.favourite);
        presetArray.append(presetObject);
    }

    QJsonObject object;
    object.insert(QStringLiteral("version"), version);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("mode"), mode == QStringLiteral("sequential") ? QStringLiteral("sequential") : QStringLiteral("shuffle"));
    object.insert(QStringLiteral("presetDurationSeconds"), presetDurationSeconds);
    object.insert(QStringLiteral("transitionDurationSeconds"), transitionDurationSeconds);
    object.insert(QStringLiteral("presets"), presetArray);
    return object;
}

std::optional<Playlist> Playlist::fromJson(const QJsonObject& object, const QString& basePresetFolder, QString* error)
{
    if (!object.contains(QStringLiteral("name")) || !object.value(QStringLiteral("name")).isString()) {
        if (error) {
            *error = QStringLiteral("Playlist is missing a name.");
        }
        return std::nullopt;
    }

    Playlist playlist;
    playlist.version = object.value(QStringLiteral("version")).toInt(1);
    playlist.name = object.value(QStringLiteral("name")).toString().trimmed();
    playlist.mode = object.value(QStringLiteral("mode")).toString(QStringLiteral("shuffle"));
    if (playlist.mode != QStringLiteral("sequential")) {
        playlist.mode = QStringLiteral("shuffle");
    }
    playlist.presetDurationSeconds = object.value(QStringLiteral("presetDurationSeconds")).toDouble(30.0);
    playlist.transitionDurationSeconds = object.value(QStringLiteral("transitionDurationSeconds")).toDouble(3.0);

    const QJsonArray presetArray = object.value(QStringLiteral("presets")).toArray();
    playlist.presets.reserve(presetArray.size());
    for (const QJsonValue& value : presetArray) {
        const QJsonObject presetObject = value.toObject();
        PlaylistEntry entry;
        entry.path = pathFromJson(presetObject.value(QStringLiteral("path")).toString(), basePresetFolder);
        entry.title = presetObject.value(QStringLiteral("title")).toString(QFileInfo(entry.path).completeBaseName());
        entry.author = presetObject.value(QStringLiteral("author")).toString(QStringLiteral("Unknown"));
        entry.favourite = presetObject.value(QStringLiteral("favourite")).toBool(false);
        if (!entry.path.isEmpty()) {
            playlist.presets.append(entry);
        }
    }

    if (playlist.name.isEmpty()) {
        playlist.name = QStringLiteral("Untitled");
    }

    return playlist;
}

std::optional<Playlist> Playlist::loadFromFile(const QString& filePath, const QString& basePresetFolder, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = parseError.errorString();
        }
        return std::nullopt;
    }

    return fromJson(document.object(), basePresetFolder, error);
}

bool Playlist::saveToFile(const QString& filePath, const QString& basePresetFolder, QString* error) const
{
    const QFileInfo fileInfo(filePath);
    QDir().mkpath(fileInfo.absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    const QJsonDocument document(toJson(basePresetFolder));
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    return true;
}

QString Playlist::pathForJson(const QString& path, const QString& basePresetFolder)
{
    if (path.isEmpty() || basePresetFolder.isEmpty()) {
        return path;
    }

    const QFileInfo pathInfo(path);
    const QFileInfo baseInfo(basePresetFolder);
    if (!pathInfo.isAbsolute() || !baseInfo.exists()) {
        return path;
    }

    const QString relative = QDir(baseInfo.absoluteFilePath()).relativeFilePath(pathInfo.absoluteFilePath());
    if (!relative.startsWith(QStringLiteral("../")) && relative != QStringLiteral("..")) {
        return relative;
    }
    return path;
}

QString Playlist::pathFromJson(const QString& path, const QString& basePresetFolder)
{
    if (path.isEmpty()) {
        return {};
    }

    const QFileInfo pathInfo(path);
    if (pathInfo.isAbsolute() || basePresetFolder.isEmpty()) {
        return path;
    }

    return QDir(basePresetFolder).absoluteFilePath(path);
}
