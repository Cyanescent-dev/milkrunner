#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <optional>

struct PlaylistEntry
{
    QString path;
    QString title;
    QString author = QStringLiteral("Unknown");
    bool favourite = false;
};

class Playlist
{
public:
    int version = 1;
    QString name = QStringLiteral("Default");
    QString mode = QStringLiteral("shuffle");
    double presetDurationSeconds = 30.0;
    double transitionDurationSeconds = 3.0;
    QVector<PlaylistEntry> presets;

    QJsonObject toJson(const QString& basePresetFolder = QString()) const;

    static std::optional<Playlist> fromJson(
        const QJsonObject& object,
        const QString& basePresetFolder = QString(),
        QString* error = nullptr);

    static std::optional<Playlist> loadFromFile(
        const QString& filePath,
        const QString& basePresetFolder = QString(),
        QString* error = nullptr);

    bool saveToFile(
        const QString& filePath,
        const QString& basePresetFolder = QString(),
        QString* error = nullptr) const;

private:
    static QString pathForJson(const QString& path, const QString& basePresetFolder);
    static QString pathFromJson(const QString& path, const QString& basePresetFolder);
};
