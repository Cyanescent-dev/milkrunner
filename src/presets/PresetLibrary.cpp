#include "PresetLibrary.h"

#include "PresetScanner.h"

#include <QFileInfo>
#include <QUrl>

PresetLibraryModel::PresetLibraryModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int PresetLibraryModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return visibleRows_.size();
}

QVariant PresetLibraryModel::data(const QModelIndex& index, int role) const
{
    const int sourceRow = sourceRowForVisibleRow(index.row());
    if (!index.isValid() || sourceRow < 0) {
        return {};
    }

    const PresetMetadata& preset = presets_.at(sourceRow);
    switch (role) {
    case PathRole:
        return preset.path;
    case TitleRole:
    case Qt::DisplayRole:
        return preset.title;
    case AuthorRole:
        return preset.author;
    case FavouriteRole:
        return preset.favourite;
    case CompatibilityStatusRole:
        return preset.compatibilityStatus;
    case CompatibilityNoteRole:
        return preset.compatibilityNote;
    default:
        return {};
    }
}

QHash<int, QByteArray> PresetLibraryModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {TitleRole, "title"},
        {AuthorRole, "author"},
        {FavouriteRole, "favourite"},
        {CompatibilityStatusRole, "compatibilityStatus"},
        {CompatibilityNoteRole, "compatibilityNote"},
    };
}

QString PresetLibraryModel::folder() const
{
    return folder_;
}

int PresetLibraryModel::count() const
{
    return visibleRows_.size();
}

int PresetLibraryModel::totalCount() const
{
    return presets_.size();
}

QString PresetLibraryModel::filterText() const
{
    return filterText_;
}

bool PresetLibraryModel::favouritesOnly() const
{
    return favouritesOnly_;
}

QList<PresetMetadata> PresetLibraryModel::presets() const
{
    return presets_;
}

QList<PresetMetadata> PresetLibraryModel::visiblePresets() const
{
    QList<PresetMetadata> visible;
    visible.reserve(visibleRows_.size());
    for (const int sourceRow : visibleRows_) {
        if (sourceRow >= 0 && sourceRow < presets_.size()) {
            visible.append(presets_.at(sourceRow));
        }
    }
    return visible;
}

void PresetLibraryModel::setFavourites(const QSet<QString>& favouritePaths)
{
    favouritePaths_ = favouritePaths;
    for (PresetMetadata& preset : presets_) {
        preset.favourite = favouritePaths_.contains(preset.path);
    }

    if (favouritesOnly_) {
        rebuildVisibleRows();
    } else if (!visibleRows_.isEmpty()) {
        emit dataChanged(index(0), index(visibleRows_.size() - 1), {FavouriteRole});
    }
    emit favouritesChanged();
}

QSet<QString> PresetLibraryModel::favouritePaths() const
{
    return favouritePaths_;
}

bool PresetLibraryModel::scanFolder(const QString& folderOrUrl)
{
    const QString localPath = localPathFromInput(folderOrUrl);
    const QFileInfo folderInfo(localPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        return false;
    }

    beginResetModel();
    folder_ = folderInfo.absoluteFilePath();
    presets_ = PresetScanner::scanFolder(folder_);
    for (PresetMetadata& preset : presets_) {
        preset.favourite = favouritePaths_.contains(preset.path);
    }
    rebuildVisibleRowsWithoutReset();
    endResetModel();

    emit folderChanged();
    emit countChanged();
    emit totalCountChanged();
    emit scanCompleted(presets_.size());
    return true;
}

QVariantMap PresetLibraryModel::get(int row) const
{
    QVariantMap value;
    const int sourceRow = sourceRowForVisibleRow(row);
    if (sourceRow < 0) {
        return value;
    }

    const PresetMetadata& preset = presets_.at(sourceRow);
    value.insert(QStringLiteral("path"), preset.path);
    value.insert(QStringLiteral("title"), preset.title);
    value.insert(QStringLiteral("author"), preset.author);
    value.insert(QStringLiteral("favourite"), preset.favourite);
    value.insert(QStringLiteral("compatibilityStatus"), preset.compatibilityStatus);
    value.insert(QStringLiteral("compatibilityNote"), preset.compatibilityNote);
    return value;
}

void PresetLibraryModel::toggleFavourite(int row)
{
    const int sourceRow = sourceRowForVisibleRow(row);
    if (sourceRow < 0) {
        return;
    }

    PresetMetadata& preset = presets_[sourceRow];
    preset.favourite = !preset.favourite;
    if (preset.favourite) {
        favouritePaths_.insert(preset.path);
    } else {
        favouritePaths_.remove(preset.path);
    }

    if (favouritesOnly_) {
        rebuildVisibleRows();
    } else {
        emit dataChanged(index(row), index(row), {FavouriteRole});
    }
    emit favouritesChanged();
}

void PresetLibraryModel::setFavouriteByPath(const QString& path, bool favourite)
{
    const int row = indexForPath(path);
    if (favourite) {
        favouritePaths_.insert(path);
    } else {
        favouritePaths_.remove(path);
    }

    if (row >= 0) {
        presets_[row].favourite = favourite;
        if (favouritesOnly_) {
            rebuildVisibleRows();
        } else {
            const int visibleRow = visibleRowForSourceRow(row);
            if (visibleRow >= 0) {
                emit dataChanged(index(visibleRow), index(visibleRow), {FavouriteRole});
            }
        }
    }
    emit favouritesChanged();
}

void PresetLibraryModel::markPresetReady(const QString& path, const QString& message)
{
    const int sourceRow = indexForPath(path);
    if (sourceRow < 0 || presets_.at(sourceRow).compatibilityStatus == QStringLiteral("failed")) {
        return;
    }

    PresetMetadata& preset = presets_[sourceRow];
    preset.compatibilityStatus = QStringLiteral("ready");
    preset.compatibilityNote = message.trimmed().isEmpty() ? QStringLiteral("Rendered without errors") : message.trimmed();

    const int visibleRow = visibleRowForSourceRow(sourceRow);
    if (visibleRow >= 0) {
        emit dataChanged(index(visibleRow), index(visibleRow), {CompatibilityStatusRole, CompatibilityNoteRole});
    }
}

void PresetLibraryModel::markPresetFailed(const QString& path, const QString& message)
{
    const int sourceRow = indexForPath(path);
    if (sourceRow < 0) {
        return;
    }

    PresetMetadata& preset = presets_[sourceRow];
    preset.compatibilityStatus = QStringLiteral("failed");
    preset.compatibilityNote = message.trimmed().isEmpty() ? QStringLiteral("Load failed") : message.trimmed();

    const int visibleRow = visibleRowForSourceRow(sourceRow);
    if (visibleRow >= 0) {
        emit dataChanged(index(visibleRow), index(visibleRow), {CompatibilityStatusRole, CompatibilityNoteRole});
    }
}

void PresetLibraryModel::setFilterText(const QString& text)
{
    const QString normalized = text.trimmed();
    if (filterText_ == normalized) {
        return;
    }

    filterText_ = normalized;
    rebuildVisibleRows();
    emit filterChanged();
}

void PresetLibraryModel::setFavouritesOnly(bool enabled)
{
    if (favouritesOnly_ == enabled) {
        return;
    }

    favouritesOnly_ = enabled;
    rebuildVisibleRows();
    emit filterChanged();
}

QString PresetLibraryModel::localPathFromInput(const QString& folderOrUrl)
{
    const QUrl url(folderOrUrl);
    if (url.isValid() && url.isLocalFile()) {
        return url.toLocalFile();
    }
    return folderOrUrl;
}

int PresetLibraryModel::indexForPath(const QString& path) const
{
    for (int i = 0; i < presets_.size(); ++i) {
        if (presets_.at(i).path == path) {
            return i;
        }
    }
    return -1;
}

int PresetLibraryModel::sourceRowForVisibleRow(int row) const
{
    if (row < 0 || row >= visibleRows_.size()) {
        return -1;
    }

    const int sourceRow = visibleRows_.at(row);
    return sourceRow >= 0 && sourceRow < presets_.size() ? sourceRow : -1;
}

int PresetLibraryModel::visibleRowForSourceRow(int sourceRow) const
{
    for (int i = 0; i < visibleRows_.size(); ++i) {
        if (visibleRows_.at(i) == sourceRow) {
            return i;
        }
    }
    return -1;
}

void PresetLibraryModel::rebuildVisibleRows()
{
    const int previousCount = visibleRows_.size();

    beginResetModel();
    rebuildVisibleRowsWithoutReset();
    endResetModel();

    if (visibleRows_.size() != previousCount) {
        emit countChanged();
    }
}

void PresetLibraryModel::rebuildVisibleRowsWithoutReset()
{
    visibleRows_.clear();
    visibleRows_.reserve(presets_.size());
    for (int i = 0; i < presets_.size(); ++i) {
        if (acceptsPreset(presets_.at(i))) {
            visibleRows_.append(i);
        }
    }
}

bool PresetLibraryModel::acceptsPreset(const PresetMetadata& preset) const
{
    if (favouritesOnly_ && !preset.favourite) {
        return false;
    }

    if (filterText_.isEmpty()) {
        return true;
    }

    const QString needle = filterText_.toCaseFolded();
    return preset.title.toCaseFolded().contains(needle)
        || preset.author.toCaseFolded().contains(needle)
        || QFileInfo(preset.path).fileName().toCaseFolded().contains(needle);
}
