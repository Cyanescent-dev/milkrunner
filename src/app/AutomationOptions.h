#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

struct AutomationOptions {
    QStringList presets;
    QString listFolder;
    QString statusFile;
    int intervalSeconds = 5;
    int readinessTimeoutSeconds = 120;
    bool demoAudio = false;
    QString audioFile;
    QString audioSource;
    QString audioDevice = "default";
    bool audioLoop = false;
    bool listAudioDevices = false;
    int runSeconds = 0;
    bool help = false;
    bool parse(const QStringList& arguments, QString& error);
    static QJsonObject presetInfo(const QString& path, QString& error);
};
