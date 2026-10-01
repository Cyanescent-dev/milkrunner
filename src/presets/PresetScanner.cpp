#include "PresetScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

QList<PresetMetadata> PresetScanner::scanFolder(const QString& folderPath)
{
    QList<PresetMetadata> presets;
    const QFileInfo folderInfo(folderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        return presets;
    }

    QDirIterator iterator(folderInfo.absoluteFilePath(),
        QStringList{QStringLiteral("*.milk"), QStringLiteral("*.prjm")},
        QDir::Files,
        QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo fileInfo = iterator.fileInfo();
        if (isPresetFile(fileInfo) && !shouldIgnorePresetFile(fileInfo)) {
            presets.append(metadataForFile(fileInfo));
        }
    }

    std::sort(presets.begin(), presets.end(), [](const PresetMetadata& left, const PresetMetadata& right) {
        return QString::localeAwareCompare(left.title.toCaseFolded(), right.title.toCaseFolded()) < 0;
    });

    return presets;
}

bool PresetScanner::isPresetFile(const QFileInfo& fileInfo)
{
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    const QString suffix = fileInfo.suffix().toCaseFolded();
    return suffix == QStringLiteral("milk") || suffix == QStringLiteral("prjm");
}

bool PresetScanner::shouldIgnorePresetFile(const QFileInfo& fileInfo)
{
    const QString path = QDir::cleanPath(fileInfo.absoluteFilePath()).toCaseFolded();
    const QStringList segments = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    if (segments.contains(QStringLiteral(".git"))
        || segments.contains(QStringLiteral("build"))
        || segments.contains(QStringLiteral("projectm-build"))
        || segments.contains(QStringLiteral("projectm-install"))) {
        return true;
    }

    const QString projectMTestPath = QStringLiteral("/external/projectm-4.1.6/presets/tests/");
    if (path.contains(projectMTestPath)) {
        return true;
    }

    const QString projectMUnitTestPath = QStringLiteral("/external/projectm-4.1.6/tests/");
    if (path.contains(projectMUnitTestPath)) {
        return true;
    }

    const QString fileName = fileInfo.fileName().toCaseFolded();
    const int testsIndex = segments.indexOf(QStringLiteral("tests"));
    if (testsIndex >= 0 && QRegularExpression(QStringLiteral("^\\d{3}-")).match(fileName).hasMatch()) {
        return true;
    }

    return false;
}

PresetMetadata PresetScanner::metadataForFile(const QFileInfo& fileInfo)
{
    PresetMetadata metadata;
    metadata.path = fileInfo.absoluteFilePath();

    QString title = fileInfo.completeBaseName().trimmed();
    QString author = QStringLiteral("Unknown");

    const QString separator = QStringLiteral(" - ");
    const int separatorIndex = title.indexOf(separator);
    if (separatorIndex > 0 && separatorIndex < title.size() - separator.size()) {
        author = title.left(separatorIndex).trimmed();
        title = title.mid(separatorIndex + separator.size()).trimmed();
    }

    metadata.title = title.isEmpty() ? fileInfo.fileName() : title;
    metadata.author = author.isEmpty() ? QStringLiteral("Unknown") : author;
    metadata.compatibilityStatus = QStringLiteral("untested");
    metadata.compatibilityNote = QStringLiteral("Ready to test");
    return metadata;
}
