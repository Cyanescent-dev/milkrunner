#include "PlaylistManager.h"

#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QtGlobal>

#include <algorithm>

PlaylistManager::PlaylistManager(QObject* parent)
    : QAbstractListModel(parent)
{
}

int PlaylistManager::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return activePlaylist_.presets.size();
}

QVariant PlaylistManager::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= activePlaylist_.presets.size()) {
        return {};
    }

    const PlaylistEntry& entry = activePlaylist_.presets.at(index.row());
    switch (role) {
    case PathRole:
        return entry.path;
    case TitleRole:
    case Qt::DisplayRole:
        return entry.title;
    case AuthorRole:
        return entry.author;
    case FavouriteRole:
        return entry.favourite;
    case CurrentRole:
        return index.row() == currentIndex_;
    default:
        return {};
    }
}

QHash<int, QByteArray> PlaylistManager::roleNames() const
{
    return {
        {PathRole, "path"},
        {TitleRole, "title"},
        {AuthorRole, "author"},
        {FavouriteRole, "favourite"},
        {CurrentRole, "current"},
    };
}

QStringList PlaylistManager::playlistNames() const
{
    return playlistFiles_.keys();
}

QString PlaylistManager::activePlaylistName() const
{
    return activePlaylist_.name;
}

QString PlaylistManager::mode() const
{
    return activePlaylist_.mode;
}

double PlaylistManager::presetDurationSeconds() const
{
    return activePlaylist_.presetDurationSeconds;
}

double PlaylistManager::transitionDurationSeconds() const
{
    return activePlaylist_.transitionDurationSeconds;
}

QString PlaylistManager::storageFolder() const
{
    return storageFolder_;
}

QString PlaylistManager::errorMessage() const
{
    return errorMessage_;
}

int PlaylistManager::currentIndex() const
{
    return currentIndex_;
}

int PlaylistManager::count() const
{
    return activePlaylist_.presets.size();
}

QString PlaylistManager::currentPositionText() const
{
    if (currentIndex_ < 0 || activePlaylist_.presets.isEmpty()) {
        return QStringLiteral("0 / 0");
    }
    return QStringLiteral("%1 / %2").arg(currentIndex_ + 1).arg(activePlaylist_.presets.size());
}

void PlaylistManager::setStorageFolder(const QString& folder)
{
    storageFolder_ = folder;
    QDir().mkpath(storageFolder_);
}

void PlaylistManager::setPresetRoot(const QString& folder)
{
    presetRoot_ = folder;
}

void PlaylistManager::loadPlaylists()
{
    playlistFiles_.clear();
    QDir directory(storageFolder_);
    directory.mkpath(QStringLiteral("."));

    const QFileInfoList files = directory.entryInfoList(QStringList{QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo& file : files) {
        QString error;
        const std::optional<Playlist> playlist = Playlist::loadFromFile(file.absoluteFilePath(), presetRoot_, &error);
        if (playlist) {
            playlistFiles_.insert(playlist->name, file.absoluteFilePath());
        }
    }

    emit playlistNamesChanged();

    if (playlistFiles_.isEmpty()) {
        createPlaylist(QStringLiteral("Default"));
    } else {
        setActivePlaylist(playlistFiles_.firstKey());
    }
}

bool PlaylistManager::createPlaylist(const QString& requestedName)
{
    QString name = requestedName.trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("Untitled");
    }

    QString uniqueName = name;
    int suffix = 2;
    while (playlistFiles_.contains(uniqueName)) {
        uniqueName = QStringLiteral("%1 %2").arg(name).arg(suffix++);
    }

    Playlist playlist;
    playlist.name = uniqueName;
    const QString filePath = filePathForName(uniqueName);
    QString error;
    if (!playlist.saveToFile(filePath, presetRoot_, &error)) {
        setError(error);
        return false;
    }

    playlistFiles_.insert(uniqueName, filePath);
    emit playlistNamesChanged();
    return setActivePlaylist(uniqueName);
}

bool PlaylistManager::setActivePlaylist(const QString& name)
{
    if (!playlistFiles_.contains(name)) {
        setError(QStringLiteral("Playlist not found."));
        return false;
    }

    QString error;
    const std::optional<Playlist> playlist = Playlist::loadFromFile(playlistFiles_.value(name), presetRoot_, &error);
    if (!playlist) {
        setError(error);
        return false;
    }

    beginResetModel();
    activePlaylist_ = *playlist;
    currentIndex_ = activePlaylist_.presets.isEmpty() ? -1 : 0;
    resetShuffleHistory();
    endResetModel();

    clearError();
    emit activePlaylistChanged();
    if (currentIndex_ >= 0) {
        recordShuffleHistory(currentIndex_);
        const PlaylistEntry& entry = activePlaylist_.presets.at(currentIndex_);
        emit activePresetChanged(entry.path, entry.title);
    } else {
        emit activePresetChanged(QString(), QString());
    }
    return true;
}

bool PlaylistManager::saveActive()
{
    if (!hasActivePlaylist()) {
        return false;
    }

    const QString filePath = playlistFiles_.value(activePlaylist_.name, filePathForName(activePlaylist_.name));
    QString error;
    if (!activePlaylist_.saveToFile(filePath, presetRoot_, &error)) {
        setError(error);
        return false;
    }

    playlistFiles_.insert(activePlaylist_.name, filePath);
    clearError();
    return true;
}

void PlaylistManager::addPreset(const QString& path, const QString& title, const QString& author, bool favourite)
{
    if (path.isEmpty()) {
        return;
    }

    PlaylistEntry entry;
    entry.path = path;
    entry.title = title;
    entry.author = author;
    entry.favourite = favourite;

    addPresets(QVector<PlaylistEntry>{entry});
}

int PlaylistManager::addPresets(const QVector<PlaylistEntry>& entries)
{
    QVector<PlaylistEntry> normalizedEntries;
    normalizedEntries.reserve(entries.size());
    for (PlaylistEntry entry : entries) {
        if (entry.path.isEmpty()) {
            continue;
        }

        entry.title = entry.title.isEmpty() ? QFileInfo(entry.path).completeBaseName() : entry.title;
        entry.author = entry.author.isEmpty() ? QStringLiteral("Unknown") : entry.author;
        normalizedEntries.append(entry);
    }

    if (normalizedEntries.isEmpty()) {
        return 0;
    }

    if (!hasActivePlaylist()) {
        createPlaylist(QStringLiteral("Default"));
    }

    const int row = activePlaylist_.presets.size();
    const int previousIndex = currentIndex_;
    beginInsertRows(QModelIndex(), row, row + normalizedEntries.size() - 1);
    activePlaylist_.presets += normalizedEntries;
    if (currentIndex_ < 0) {
        currentIndex_ = row;
    }
    resetShuffleHistory();
    endInsertRows();

    saveActive();
    if (previousIndex < 0 && currentIndex_ == row) {
        recordShuffleHistory(currentIndex_);
        const PlaylistEntry& entry = activePlaylist_.presets.at(currentIndex_);
        emit activePresetChanged(entry.path, entry.title);
    }

    return normalizedEntries.size();
}

void PlaylistManager::removePreset(int row)
{
    if (row < 0 || row >= activePlaylist_.presets.size()) {
        return;
    }

    const int previousIndex = currentIndex_;
    beginRemoveRows(QModelIndex(), row, row);
    activePlaylist_.presets.removeAt(row);
    if (activePlaylist_.presets.isEmpty()) {
        currentIndex_ = -1;
    } else if (currentIndex_ >= activePlaylist_.presets.size()) {
        currentIndex_ = activePlaylist_.presets.size() - 1;
    } else if (row < currentIndex_) {
        --currentIndex_;
    }
    resetShuffleHistory();
    endRemoveRows();

    saveActive();
    emitCurrentRowChanged(previousIndex);
}

void PlaylistManager::movePresetUp(int row)
{
    if (row <= 0 || row >= activePlaylist_.presets.size()) {
        return;
    }

    const int previousIndex = currentIndex_;
    beginResetModel();
    activePlaylist_.presets.swapItemsAt(row, row - 1);
    if (currentIndex_ == row) {
        --currentIndex_;
    } else if (currentIndex_ == row - 1) {
        ++currentIndex_;
    }
    resetShuffleHistory();
    endResetModel();

    saveActive();
    emitCurrentRowChanged(previousIndex);
}

void PlaylistManager::movePresetDown(int row)
{
    if (row < 0 || row >= activePlaylist_.presets.size() - 1) {
        return;
    }

    const int previousIndex = currentIndex_;
    beginResetModel();
    activePlaylist_.presets.swapItemsAt(row, row + 1);
    if (currentIndex_ == row) {
        ++currentIndex_;
    } else if (currentIndex_ == row + 1) {
        --currentIndex_;
    }
    resetShuffleHistory();
    endResetModel();

    saveActive();
    emitCurrentRowChanged(previousIndex);
}

QVariantMap PlaylistManager::get(int row) const
{
    QVariantMap value;
    if (row < 0 || row >= activePlaylist_.presets.size()) {
        return value;
    }

    const PlaylistEntry& entry = activePlaylist_.presets.at(row);
    value.insert(QStringLiteral("path"), entry.path);
    value.insert(QStringLiteral("title"), entry.title);
    value.insert(QStringLiteral("author"), entry.author);
    value.insert(QStringLiteral("favourite"), entry.favourite);
    return value;
}

void PlaylistManager::toggleFavourite(int row)
{
    if (row < 0 || row >= activePlaylist_.presets.size()) {
        return;
    }

    activePlaylist_.presets[row].favourite = !activePlaylist_.presets[row].favourite;
    emit dataChanged(index(row), index(row), {FavouriteRole});
    saveActive();
}

bool PlaylistManager::playIndex(int index)
{
    return selectIndex(index);
}

bool PlaylistManager::nextPreset()
{
    if (activePlaylist_.presets.isEmpty()) {
        return false;
    }

    int nextIndex = 0;
    if (activePlaylist_.mode == QStringLiteral("shuffle")) {
        if (shuffleHistoryCursor_ >= 0 && shuffleHistoryCursor_ < shuffleHistory_.size() - 1) {
            ++shuffleHistoryCursor_;
            return selectIndex(shuffleHistory_.at(shuffleHistoryCursor_), false);
        }
        nextIndex = randomIndexAvoidingRecent();
    } else {
        nextIndex = (currentIndex_ + 1) % activePlaylist_.presets.size();
    }

    return selectIndex(nextIndex);
}

bool PlaylistManager::previousPreset()
{
    if (activePlaylist_.presets.isEmpty()) {
        return false;
    }

    if (activePlaylist_.mode == QStringLiteral("shuffle") && shuffleHistoryCursor_ > 0) {
        --shuffleHistoryCursor_;
        return selectIndex(shuffleHistory_.at(shuffleHistoryCursor_), false);
    }

    int previousIndex = currentIndex_ - 1;
    if (previousIndex < 0) {
        previousIndex = activePlaylist_.presets.size() - 1;
    }
    return selectIndex(previousIndex);
}

bool PlaylistManager::randomPreset()
{
    if (activePlaylist_.presets.isEmpty()) {
        return false;
    }
    return selectIndex(randomIndexAvoidingRecent());
}

void PlaylistManager::setMode(const QString& mode)
{
    const QString normalized = mode == QStringLiteral("sequential") ? QStringLiteral("sequential") : QStringLiteral("shuffle");
    if (activePlaylist_.mode == normalized) {
        return;
    }
    activePlaylist_.mode = normalized;
    resetShuffleHistory();
    if (currentIndex_ >= 0) {
        recordShuffleHistory(currentIndex_);
    }
    saveActive();
    emit activePlaylistChanged();
}

void PlaylistManager::setPresetDurationSeconds(double seconds)
{
    const double clamped = qBound(5.0, seconds, 600.0);
    if (qFuzzyCompare(activePlaylist_.presetDurationSeconds, clamped)) {
        return;
    }
    activePlaylist_.presetDurationSeconds = clamped;
    saveActive();
    emit activePlaylistChanged();
}

void PlaylistManager::setTransitionDurationSeconds(double seconds)
{
    const double clamped = qBound(0.0, seconds, 60.0);
    if (qFuzzyCompare(activePlaylist_.transitionDurationSeconds, clamped)) {
        return;
    }
    activePlaylist_.transitionDurationSeconds = clamped;
    saveActive();
    emit activePlaylistChanged();
}

QString PlaylistManager::filePathForName(const QString& name) const
{
    return QDir(storageFolder_).absoluteFilePath(sanitizedFileName(name) + QStringLiteral(".json"));
}

QString PlaylistManager::sanitizedFileName(const QString& name) const
{
    QString sanitized = name.toCaseFolded();
    sanitized.replace(QRegularExpression(QStringLiteral("[^a-z0-9._-]+")), QStringLiteral("-"));
    sanitized = sanitized.trimmed();
    while (sanitized.startsWith(QLatin1Char('-'))) {
        sanitized.remove(0, 1);
    }
    while (sanitized.endsWith(QLatin1Char('-'))) {
        sanitized.chop(1);
    }
    return sanitized.isEmpty() ? QStringLiteral("playlist") : sanitized;
}

void PlaylistManager::setError(const QString& message)
{
    if (errorMessage_ == message) {
        return;
    }
    errorMessage_ = message;
    emit errorMessageChanged();
}

void PlaylistManager::clearError()
{
    setError(QString());
}

void PlaylistManager::emitCurrentRowChanged(int previousIndex)
{
    if (previousIndex >= 0 && previousIndex < activePlaylist_.presets.size()) {
        emit dataChanged(index(previousIndex), index(previousIndex), {CurrentRole});
    }
    if (currentIndex_ >= 0 && currentIndex_ < activePlaylist_.presets.size()) {
        emit dataChanged(index(currentIndex_), index(currentIndex_), {CurrentRole});
        const PlaylistEntry& entry = activePlaylist_.presets.at(currentIndex_);
        emit activePresetChanged(entry.path, entry.title);
    } else {
        emit activePresetChanged(QString(), QString());
    }
}

bool PlaylistManager::selectIndex(int index, bool recordHistory)
{
    if (index < 0 || index >= activePlaylist_.presets.size()) {
        return false;
    }

    const int previousIndex = currentIndex_;
    currentIndex_ = index;
    if (recordHistory && activePlaylist_.mode == QStringLiteral("shuffle")) {
        recordShuffleHistory(index);
    }
    emitCurrentRowChanged(previousIndex);
    return true;
}

bool PlaylistManager::hasActivePlaylist() const
{
    return !activePlaylist_.name.isEmpty();
}

void PlaylistManager::resetShuffleHistory()
{
    shuffleHistory_.clear();
    shuffleHistoryCursor_ = -1;
}

void PlaylistManager::recordShuffleHistory(int index)
{
    if (index < 0 || index >= activePlaylist_.presets.size()) {
        return;
    }

    if (shuffleHistoryCursor_ >= 0 && shuffleHistoryCursor_ < shuffleHistory_.size() - 1) {
        shuffleHistory_.erase(shuffleHistory_.begin() + shuffleHistoryCursor_ + 1, shuffleHistory_.end());
    }
    if (!shuffleHistory_.isEmpty() && shuffleHistory_.last() == index) {
        shuffleHistoryCursor_ = shuffleHistory_.size() - 1;
        return;
    }

    shuffleHistory_.append(index);
    const int maxHistory = std::max(12, static_cast<int>(activePlaylist_.presets.size()) * 2);
    while (shuffleHistory_.size() > maxHistory) {
        shuffleHistory_.removeFirst();
    }
    shuffleHistoryCursor_ = shuffleHistory_.size() - 1;
}

int PlaylistManager::randomIndexAvoidingRecent() const
{
    const int presetCount = activePlaylist_.presets.size();
    if (presetCount <= 1) {
        return 0;
    }

    const int recentWindow = std::min(presetCount - 1, std::max(1, presetCount / 3));
    QSet<int> recent;
    for (int i = shuffleHistory_.size() - 1; i >= 0 && recent.size() < recentWindow; --i) {
        recent.insert(shuffleHistory_.at(i));
    }
    recent.insert(currentIndex_);

    QVector<int> candidates;
    candidates.reserve(presetCount);
    for (int i = 0; i < presetCount; ++i) {
        if (!recent.contains(i)) {
            candidates.append(i);
        }
    }
    if (candidates.isEmpty()) {
        for (int i = 0; i < presetCount; ++i) {
            if (i != currentIndex_) {
                candidates.append(i);
            }
        }
    }

    return candidates.at(QRandomGenerator::global()->bounded(candidates.size()));
}
