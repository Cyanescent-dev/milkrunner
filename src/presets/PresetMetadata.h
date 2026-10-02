#pragma once

#include <QString>

struct PresetMetadata
{
    QString path;
    QString title;
    QString author = QStringLiteral("Unknown");
    QString compatibilityStatus = QStringLiteral("untested");
    QString compatibilityNote = QStringLiteral("Ready to test");
    bool favourite = false;
};
