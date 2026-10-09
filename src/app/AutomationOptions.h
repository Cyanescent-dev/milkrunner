#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

struct AutomationOptions {
    QStringList presets;
    QString listFolder;
    QString statusFile;
    int intervalSeconds = 5;
    bool demoAudio = false;
    bool help = false;
    bool parse(const QStringList& arguments, QString& error);
    static QJsonObject presetInfo(const QString& path, QString& error);
};
