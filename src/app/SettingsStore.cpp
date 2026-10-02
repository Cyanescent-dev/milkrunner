#include "SettingsStore.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace {
QSettings settings()
{
    return QSettings(QStringLiteral("MilkRunner"), QStringLiteral("Milk Runner Visualizer"));
}
}

SettingsStore::SettingsStore(QObject* parent)
    : QObject(parent)
{
    QDir().mkpath(appConfigFolder());
    QDir().mkpath(playlistFolder());
}

QString SettingsStore::presetFolder() const
{
    QSettings store = settings();
    return store.value(QStringLiteral("presetFolder")).toString();
}

void SettingsStore::setPresetFolder(const QString& folder)
{
    QSettings store = settings();
    store.setValue(QStringLiteral("presetFolder"), folder);
}

QString SettingsStore::lastPlaylistName() const
{
    QSettings store = settings();
    return store.value(QStringLiteral("lastPlaylistName")).toString();
}

void SettingsStore::setLastPlaylistName(const QString& name)
{
    QSettings store = settings();
    store.setValue(QStringLiteral("lastPlaylistName"), name);
}

int SettingsStore::fpsCap() const
{
    QSettings store = settings();
    return store.value(QStringLiteral("fpsCap"), 60).toInt();
}

void SettingsStore::setFpsCap(int fps)
{
    QSettings store = settings();
    store.setValue(QStringLiteral("fpsCap"), fps);
}

double SettingsStore::renderScale() const
{
    QSettings store = settings();
    return store.value(QStringLiteral("renderScale"), 1.0).toDouble();
}

void SettingsStore::setRenderScale(double scale)
{
    QSettings store = settings();
    store.setValue(QStringLiteral("renderScale"), scale);
}

bool SettingsStore::lowPowerMode() const
{
    QSettings store = settings();
    return store.value(QStringLiteral("lowPowerMode"), false).toBool();
}

void SettingsStore::setLowPowerMode(bool enabled)
{
    QSettings store = settings();
    store.setValue(QStringLiteral("lowPowerMode"), enabled);
}

double SettingsStore::audioGain() const
{
    QSettings store = settings();
    return store.value(QStringLiteral("audioGain"), 2.0).toDouble();
}

void SettingsStore::setAudioGain(double gain)
{
    QSettings store = settings();
    store.setValue(QStringLiteral("audioGain"), gain);
}

QString SettingsStore::audioInputDeviceName() const
{
    QSettings store = settings();
    return store.value(QStringLiteral("audioInputDeviceName")).toString();
}

void SettingsStore::setAudioInputDeviceName(const QString& name)
{
    QSettings store = settings();
    store.setValue(QStringLiteral("audioInputDeviceName"), name);
}

bool SettingsStore::playAudioOutput() const
{
    QSettings store = settings();
    return store.value(QStringLiteral("playAudioOutput"), false).toBool();
}

void SettingsStore::setPlayAudioOutput(bool play)
{
    QSettings store = settings();
    store.setValue(QStringLiteral("playAudioOutput"), play);
}

QSet<int> SettingsStore::scionEnabledSynthModes() const
{
    QSettings store = settings();
    constexpr int legacyFallbackMode = 0;
    const QString enabledKey = QStringLiteral("scionEnabledSynthModes");
    QSet<int> modes;

    if (store.contains(enabledKey)) {
        for (const QString& value : store.value(enabledKey).toStringList()) {
            bool ok = false;
            const int mode = value.toInt(&ok);
            if (ok && (mode >= 0 && mode <= 7 || mode == 9)) {
                modes.insert(mode);
            }
        }
        return modes;
    }

    // Migrate the former single, legacy-ID selection without renumbering mode 9.
    const int oldMode = store.value(QStringLiteral("scionAudioMode"), legacyFallbackMode).toInt();
    modes.insert((oldMode >= 0 && oldMode <= 7 || oldMode == 9) ? oldMode : legacyFallbackMode);
    setScionEnabledSynthModes(modes);
    store.remove(QStringLiteral("scionAudioMode"));
    return modes;
}

void SettingsStore::setScionEnabledSynthModes(const QSet<int>& modes) const
{
    QStringList values;
    for (int mode : modes) {
        if ((mode >= 0 && mode <= 7) || mode == 9) {
            values.append(QString::number(mode));
        }
    }
    values.sort();
    QSettings store = settings();
    store.setValue(QStringLiteral("scionEnabledSynthModes"), values);
}

QSet<QString> SettingsStore::favourites() const
{
    QSet<QString> values;
    const QStringList paths = favouriteList();
    for (const QString& path : paths) {
        values.insert(path);
    }
    return values;
}

void SettingsStore::setFavourites(const QSet<QString>& paths)
{
    QStringList values;
    for (const QString& path : paths) {
        values.append(path);
    }
    values.sort(Qt::CaseInsensitive);

    QSettings store = settings();
    store.setValue(QStringLiteral("favourites"), values);
}

QString SettingsStore::appConfigFolder() const
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (folder.isEmpty()) {
        folder = QDir::home().absoluteFilePath(QStringLiteral(".config/MilkRunner"));
    }
    return folder;
}

QString SettingsStore::playlistFolder() const
{
    return QDir(appConfigFolder()).absoluteFilePath(QStringLiteral("playlists"));
}

QStringList SettingsStore::favouriteList() const
{
    QSettings store = settings();
    return store.value(QStringLiteral("favourites")).toStringList();
}
