#pragma once

#include <QObject>
#include <QSet>
#include <QString>

class SettingsStore : public QObject
{
    Q_OBJECT

public:
    explicit SettingsStore(QObject* parent = nullptr);

    QString presetFolder() const;
    void setPresetFolder(const QString& folder);

    QString lastPlaylistName() const;
    void setLastPlaylistName(const QString& name);

    int fpsCap() const;
    void setFpsCap(int fps);

    double renderScale() const;
    void setRenderScale(double scale);

    bool lowPowerMode() const;
    void setLowPowerMode(bool enabled);

    double audioGain() const;
    void setAudioGain(double gain);

    QString audioInputDeviceName() const;
    void setAudioInputDeviceName(const QString& name);

    bool playAudioOutput() const;
    void setPlayAudioOutput(bool play);

    QSet<int> scionEnabledSynthModes() const;
    void setScionEnabledSynthModes(const QSet<int>& modes) const;

    QSet<QString> favourites() const;
    void setFavourites(const QSet<QString>& paths);

    QString appConfigFolder() const;
    QString playlistFolder() const;

private:
    QStringList favouriteList() const;
};
