#include "playlists/Playlist.h"
#include "presets/PresetLibrary.h"
#include "presets/PresetScanner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

#include <iostream>

namespace {
bool writeTextFile(const QString& path, const QString& contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream << contents;
    return true;
}

int fail(const QString& message)
{
    std::cerr << message.toStdString() << '\n';
    return 1;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail(QStringLiteral("Could not create temporary directory."));
    }

    const QString presetA = tempDir.filePath(QStringLiteral("Runner - First Light.milk"));
    const QString presetB = tempDir.filePath(QStringLiteral("nested/Second.prjm"));
    const QString projectMTestPreset = tempDir.filePath(QStringLiteral("external/projectm-4.1.6/presets/tests/214-wave.milk"));
    const QString projectMFixturePreset = tempDir.filePath(QStringLiteral("external/projectm-4.1.6/tests/libprojectM/data/PresetFileParser/parser-simple.milk"));
    const QString ignored = tempDir.filePath(QStringLiteral("notes.txt"));

    QDir().mkpath(tempDir.filePath(QStringLiteral("nested")));
    QDir().mkpath(tempDir.filePath(QStringLiteral("external/projectm-4.1.6/presets/tests")));
    QDir().mkpath(tempDir.filePath(QStringLiteral("external/projectm-4.1.6/tests/libprojectM/data/PresetFileParser")));
    if (!writeTextFile(presetA, QStringLiteral("[preset00]\n")) ||
        !writeTextFile(presetB, QStringLiteral("{}\n")) ||
        !writeTextFile(projectMTestPreset, QStringLiteral("[preset00]\n")) ||
        !writeTextFile(projectMFixturePreset, QStringLiteral("[preset00]\n")) ||
        !writeTextFile(ignored, QStringLiteral("not a preset\n"))) {
        return fail(QStringLiteral("Could not write test files."));
    }

    const QList<PresetMetadata> scanned = PresetScanner::scanFolder(tempDir.path());
    if (scanned.size() != 2) {
        return fail(QStringLiteral("Preset scanner did not find exactly two preset files."));
    }

    bool foundMilk = false;
    for (const PresetMetadata& preset : scanned) {
        if (preset.path == QFileInfo(presetA).absoluteFilePath()) {
            foundMilk = preset.author == QStringLiteral("Runner")
                && preset.title == QStringLiteral("First Light");
        }
    }
    if (!foundMilk) {
        return fail(QStringLiteral("Preset scanner did not extract expected filename metadata."));
    }
    for (const PresetMetadata& preset : scanned) {
        if (preset.path == QFileInfo(projectMTestPreset).absoluteFilePath()) {
            return fail(QStringLiteral("Preset scanner exposed projectM test presets."));
        }
        if (preset.path == QFileInfo(projectMFixturePreset).absoluteFilePath()) {
            return fail(QStringLiteral("Preset scanner exposed projectM fixture presets."));
        }
        if (preset.compatibilityStatus != QStringLiteral("untested")) {
            return fail(QStringLiteral("Preset scanner did not mark presets as untested by default."));
        }
    }

    PresetLibraryModel library;
    if (!library.scanFolder(tempDir.path())) {
        return fail(QStringLiteral("Preset library could not scan the temporary folder."));
    }
    if (library.totalCount() != 2 || library.count() != 2) {
        return fail(QStringLiteral("Preset library did not expose the expected unfiltered counts."));
    }
    if (library.get(0).value(QStringLiteral("compatibilityStatus")).toString().isEmpty()) {
        return fail(QStringLiteral("Preset library did not expose compatibility status."));
    }

    library.markPresetReady(QFileInfo(presetA).absoluteFilePath(), QStringLiteral("Rendered cleanly"));
    library.setFilterText(QStringLiteral("runner"));
    if (library.get(0).value(QStringLiteral("compatibilityStatus")).toString() != QStringLiteral("ready")) {
        return fail(QStringLiteral("Preset library did not mark a preset ready."));
    }
    library.markPresetFailed(QFileInfo(presetA).absoluteFilePath(), QStringLiteral("Load failed"));
    if (library.get(0).value(QStringLiteral("compatibilityStatus")).toString() != QStringLiteral("failed")) {
        return fail(QStringLiteral("Preset library did not mark a preset failed."));
    }
    library.markPresetReady(QFileInfo(presetA).absoluteFilePath(), QStringLiteral("Recovered"));
    if (library.get(0).value(QStringLiteral("compatibilityStatus")).toString() != QStringLiteral("failed")) {
        return fail(QStringLiteral("Preset library should not overwrite failed status with ready."));
    }

    library.setFilterText(QStringLiteral("runner"));
    if (library.count() != 1 || library.get(0).value(QStringLiteral("path")).toString() != QFileInfo(presetA).absoluteFilePath()) {
        return fail(QStringLiteral("Preset library search did not match by author."));
    }

    library.setFavouriteByPath(QFileInfo(presetA).absoluteFilePath(), true);
    library.setFavouritesOnly(true);
    if (library.count() != 1 || !library.get(0).value(QStringLiteral("favourite")).toBool()) {
        return fail(QStringLiteral("Preset library saved-only filter did not show the saved preset."));
    }

    library.toggleFavourite(0);
    if (library.count() != 0 || library.favouritePaths().contains(QFileInfo(presetA).absoluteFilePath())) {
        return fail(QStringLiteral("Preset library saved-only filter did not remove an unsaved preset."));
    }

    library.setFavouritesOnly(false);
    library.setFilterText(QStringLiteral("second"));
    if (library.count() != 1 || library.get(0).value(QStringLiteral("title")).toString() != QStringLiteral("Second")) {
        return fail(QStringLiteral("Preset library search did not match by title."));
    }

    Playlist playlist;
    playlist.name = QStringLiteral("Classic Psytrance");
    playlist.mode = QStringLiteral("shuffle");
    playlist.presetDurationSeconds = 30;
    playlist.transitionDurationSeconds = 3;
    playlist.presets.append({QFileInfo(presetA).absoluteFilePath(), QStringLiteral("First Light"), QStringLiteral("Runner"), true});

    const QString playlistPath = tempDir.filePath(QStringLiteral("playlist.json"));
    QString error;
    if (!playlist.saveToFile(playlistPath, tempDir.path(), &error)) {
        return fail(QStringLiteral("Could not save playlist: ") + error);
    }

    QFile playlistFile(playlistPath);
    if (!playlistFile.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Could not reopen saved playlist."));
    }
    const QJsonDocument savedDocument = QJsonDocument::fromJson(playlistFile.readAll());
    const QString savedPath = savedDocument.object()
        .value(QStringLiteral("presets"))
        .toArray()
        .first()
        .toObject()
        .value(QStringLiteral("path"))
        .toString();
    if (QFileInfo(savedPath).isAbsolute()) {
        return fail(QStringLiteral("Playlist path was not saved relative to the preset root."));
    }

    const std::optional<Playlist> loaded = Playlist::loadFromFile(playlistPath, tempDir.path(), &error);
    if (!loaded || loaded->presets.size() != 1) {
        return fail(QStringLiteral("Could not load playlist: ") + error);
    }
    if (loaded->presets.first().path != QFileInfo(presetA).absoluteFilePath()) {
        return fail(QStringLiteral("Playlist relative path did not resolve to the original absolute path."));
    }

    return 0;
}
