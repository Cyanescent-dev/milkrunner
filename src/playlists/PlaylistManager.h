#pragma once

#include "Playlist.h"

#include <QAbstractListModel>
#include <QMap>
#include <QStringList>

class PlaylistManager : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList playlistNames READ playlistNames NOTIFY playlistNamesChanged)
    Q_PROPERTY(QString activePlaylistName READ activePlaylistName NOTIFY activePlaylistChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY activePlaylistChanged)
    Q_PROPERTY(double presetDurationSeconds READ presetDurationSeconds WRITE setPresetDurationSeconds NOTIFY activePlaylistChanged)
    Q_PROPERTY(double transitionDurationSeconds READ transitionDurationSeconds WRITE setTransitionDurationSeconds NOTIFY activePlaylistChanged)
    Q_PROPERTY(QString storageFolder READ storageFolder CONSTANT)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY activePresetChanged)
    Q_PROPERTY(int count READ count NOTIFY activePlaylistChanged)
    Q_PROPERTY(QString currentPositionText READ currentPositionText NOTIFY activePresetChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        TitleRole,
        AuthorRole,
        FavouriteRole,
        CurrentRole
    };
    Q_ENUM(Roles)

    explicit PlaylistManager(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QStringList playlistNames() const;
    QString activePlaylistName() const;
    QString mode() const;
    double presetDurationSeconds() const;
    double transitionDurationSeconds() const;
    QString storageFolder() const;
    QString errorMessage() const;
    int currentIndex() const;
    int count() const;
    QString currentPositionText() const;

    void setStorageFolder(const QString& folder);
    void setPresetRoot(const QString& folder);
    void loadPlaylists();

    Q_INVOKABLE bool createPlaylist(const QString& requestedName);
    Q_INVOKABLE bool setActivePlaylist(const QString& name);
    Q_INVOKABLE bool saveActive();
    Q_INVOKABLE void addPreset(const QString& path, const QString& title, const QString& author, bool favourite);
    int addPresets(const QVector<PlaylistEntry>& entries);
    Q_INVOKABLE void removePreset(int row);
    Q_INVOKABLE void movePresetUp(int row);
    Q_INVOKABLE void movePresetDown(int row);
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE void toggleFavourite(int row);
    Q_INVOKABLE bool playIndex(int index);
    Q_INVOKABLE bool nextPreset();
    Q_INVOKABLE bool previousPreset();
    Q_INVOKABLE bool randomPreset();

public slots:
    void setMode(const QString& mode);
    void setPresetDurationSeconds(double seconds);
    void setTransitionDurationSeconds(double seconds);

signals:
    void playlistNamesChanged();
    void activePlaylistChanged();
    void activePresetChanged(const QString& path, const QString& title);
    void errorMessageChanged();

private:
    QString filePathForName(const QString& name) const;
    QString sanitizedFileName(const QString& name) const;
    void setError(const QString& message);
    void clearError();
    void emitCurrentRowChanged(int previousIndex);
    bool selectIndex(int index, bool recordHistory = true);
    bool hasActivePlaylist() const;
    void resetShuffleHistory();
    void recordShuffleHistory(int index);
    int randomIndexAvoidingRecent() const;

    QString storageFolder_;
    QString presetRoot_;
    QString errorMessage_;
    QMap<QString, QString> playlistFiles_;
    Playlist activePlaylist_;
    int currentIndex_ = -1;
    QVector<int> shuffleHistory_;
    int shuffleHistoryCursor_ = -1;
};
