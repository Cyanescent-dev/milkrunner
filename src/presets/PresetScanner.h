#pragma once

#include "PresetMetadata.h"

#include <QList>
#include <QString>

class QFileInfo;

class PresetScanner
{
public:
    static QList<PresetMetadata> scanFolder(const QString& folderPath);
    static bool isPresetFile(const QFileInfo& fileInfo);
    static bool shouldIgnorePresetFile(const QFileInfo& fileInfo);

private:
    static PresetMetadata metadataForFile(const QFileInfo& fileInfo);
};
