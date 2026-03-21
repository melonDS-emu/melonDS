/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <QPixmap>
#include <QString>

namespace melonds {

// One ROM entry in the game library.
struct LibraryEntry
{
    QString  fullpath;      // absolute path — unique key
    QString  base;          // directory portion
    QString  filename;      // filename with extension
    QString  title;         // NDS header GameTitle (12 bytes, trimmed)
    QString  gameCode;      // NDS header GameCode  (4 bytes, e.g. "ADME")
    QString  platform;      // always "NDS"
    qint64   fileSize  = 0; // bytes
    quint32  crc32     = 0; // CRC32 of whole file (0 = not computed)
    QPixmap  icon;          // decoded 32×32 NDS banner icon (may be null)

    bool isNull() const { return fullpath.isEmpty(); }
};

class LibraryModel : public QAbstractTableModel
{
Q_OBJECT

public:
    enum Column
    {
        COL_NAME         = 0,  // display name (filename stem, or custom rename)
        COL_HEADER_TITLE = 1,  // NDS ROM header title
        COL_GAME_CODE    = 2,  // NDS game code
        COL_SIZE         = 3,  // file size (human-readable)
        COL_CRC          = 4,  // CRC32 hex string
        COL_LOCATION     = 5,  // directory
        MAX_COLUMN       = COL_LOCATION,
    };

    enum Role
    {
        FullPathRole    = Qt::UserRole + 1,
        GameCodeRole    = Qt::UserRole + 2,
        FileSizeRole    = Qt::UserRole + 3, // qint64 for numeric sort
        CRCRole         = Qt::UserRole + 4, // quint32 for numeric sort
        DisplayNameRole = Qt::UserRole + 5, // resolved display name (custom > stem)
    };

    explicit LibraryModel(QObject* parent = nullptr);

    int           rowCount(const QModelIndex& parent = {}) const override;
    int           columnCount(const QModelIndex& parent = {}) const override;
    QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant      headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void addEntries(const QList<LibraryEntry>& entries);
    void removeEntries(const QStringList& fullpaths);
    void clear();

    LibraryEntry entry(const QString& fullpath) const;
    QModelIndex  index(const QString& fullpath) const;

    // Row-based index — avoids ambiguity with QAbstractItemModel::index(row,col,parent)
    QModelIndex  rowIndex(int row, int col = COL_NAME) const;

    // Custom display name override (user-facing rename, persisted externally).
    // Pass an empty string to clear the override and revert to filename stem.
    void setCustomName(const QString& fullpath, const QString& name);
    QString customName(const QString& fullpath) const;

    // Set the CRC for an entry after async computation. No-op if path not found.
    void setCRC(const QString& fullpath, quint32 crc);

    // Read NDS ROM header + icon. Does NOT compute CRC (that's done async).
    // Returns a null entry (isNull() == true) on any failure.
    static LibraryEntry readNDSHeader(const QString& path);

    // Compute CRC32 of a file. Called from background thread.
    static quint32 computeFileCRC32(const QString& path);

    // Decode the raw 32×32 4bpp tiled NDS icon + 16-colour palette to a QPixmap.
    static QPixmap decodeNDSIcon(const quint8 icon[512], const quint16 palette[16]);

private:
    QList<LibraryEntry>       m_entries;
    QHash<QString, QString>   m_customNames; // fullpath -> custom display name

    // Returns the display name for an entry: custom > filename stem
    QString resolvedName(const LibraryEntry& e) const;

    static QString formatSize(qint64 bytes);
};

} // namespace melonds
