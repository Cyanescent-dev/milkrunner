#pragma once

#include "PresetMetadata.h"

#include <QAbstractListModel>
#include <QSet>
#include <QVector>

class PresetLibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString folder READ folder NOTIFY folderChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterChanged)
    Q_PROPERTY(bool favouritesOnly READ favouritesOnly WRITE setFavouritesOnly NOTIFY filterChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        TitleRole,
        AuthorRole,
        FavouriteRole,
        CompatibilityStatusRole,
        CompatibilityNoteRole
    };
    Q_ENUM(Roles)

    explicit PresetLibraryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString folder() const;
    int count() const;
    int totalCount() const;
    QString filterText() const;
    bool favouritesOnly() const;
    QList<PresetMetadata> presets() const;
    QList<PresetMetadata> visiblePresets() const;

    void setFavourites(const QSet<QString>& favouritePaths);
    QSet<QString> favouritePaths() const;

    Q_INVOKABLE bool scanFolder(const QString& folderOrUrl);
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE void toggleFavourite(int row);
    Q_INVOKABLE void setFavouriteByPath(const QString& path, bool favourite);
    void markPresetReady(const QString& path, const QString& message);
    void markPresetFailed(const QString& path, const QString& message);

signals:
    void folderChanged();
    void countChanged();
    void totalCountChanged();
    void filterChanged();
    void favouritesChanged();
    void scanCompleted(int count);

public slots:
    void setFilterText(const QString& text);
    void setFavouritesOnly(bool enabled);

private:
    static QString localPathFromInput(const QString& folderOrUrl);
    int indexForPath(const QString& path) const;
    int sourceRowForVisibleRow(int row) const;
    int visibleRowForSourceRow(int sourceRow) const;
    void rebuildVisibleRows();
    void rebuildVisibleRowsWithoutReset();
    bool acceptsPreset(const PresetMetadata& preset) const;

    QString folder_;
    QList<PresetMetadata> presets_;
    QVector<int> visibleRows_;
    QSet<QString> favouritePaths_;
    QString filterText_;
    bool favouritesOnly_ = false;
};
