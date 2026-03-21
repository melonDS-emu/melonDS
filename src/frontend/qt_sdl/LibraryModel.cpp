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

#include "LibraryModel.h"

#include "NDS_Header.h" // melonDS::NDSHeader, melonDS::NDSBanner

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLocale>

using namespace melonds;

// ---------------------------------------------------------------------------
// CRC32 (standard polynomial, no external dependency)
// ---------------------------------------------------------------------------

static quint32 s_crc32Table[256];
static bool    s_crc32Ready = false;

static void buildCRC32Table()
{
    for (quint32 i = 0; i < 256; ++i)
    {
        quint32 c = i;
        for (int j = 0; j < 8; ++j)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        s_crc32Table[i] = c;
    }
    s_crc32Ready = true;
}

// ---------------------------------------------------------------------------

LibraryModel::LibraryModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

int LibraryModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_entries.size();
}

int LibraryModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return MAX_COLUMN + 1;
}

QString LibraryModel::resolvedName(const LibraryEntry& e) const
{
    const QString custom = m_customNames.value(e.fullpath);
    if (!custom.isEmpty()) return custom;
    return QFileInfo(e.filename).completeBaseName();
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};

    const LibraryEntry& e = m_entries[index.row()];

    // Identity / sort roles (respond on any column)
    if (role == FullPathRole)    return e.fullpath;
    if (role == GameCodeRole)    return e.gameCode;
    if (role == FileSizeRole)    return e.fileSize;
    if (role == CRCRole)         return e.crc32;
    if (role == DisplayNameRole) return resolvedName(e);

    // Tooltip: show display name only, not full path
    if (role == Qt::ToolTipRole)
        return resolvedName(e);

    // Icon on name column only
    if (role == Qt::DecorationRole && index.column() == COL_NAME)
    {
        if (!e.icon.isNull())
            return e.icon;
        return {};
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (index.column())
        {
        case COL_NAME:
            return resolvedName(e);

        case COL_HEADER_TITLE:
            return e.title;

        case COL_GAME_CODE:
            return e.gameCode;

        case COL_SIZE:
            if (role == Qt::EditRole) return e.fileSize;
            return formatSize(e.fileSize);

        case COL_CRC:
            if (role == Qt::EditRole) return e.crc32;
            return e.crc32 ? QString("%1").arg(e.crc32, 8, 16, QChar('0')).toUpper()
                           : QString();

        case COL_LOCATION:
            return e.base;
        }
    }

    if (role == Qt::TextAlignmentRole && index.column() == COL_SIZE)
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);

    return {};
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section)
    {
    case COL_NAME:         return tr("Name");
    case COL_HEADER_TITLE: return tr("Header Title");
    case COL_GAME_CODE:    return tr("Game Code");
    case COL_SIZE:         return tr("Size");
    case COL_CRC:          return tr("CRC32");
    case COL_LOCATION:     return tr("Location");
    }
    return {};
}

Qt::ItemFlags LibraryModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void LibraryModel::addEntries(const QList<LibraryEntry>& entries)
{
    if (entries.isEmpty()) return;
    beginInsertRows({}, m_entries.size(), m_entries.size() + entries.size() - 1);
    m_entries.append(entries);
    endInsertRows();
}

void LibraryModel::removeEntries(const QStringList& fullpaths)
{
    if (fullpaths.isEmpty()) return;
    const QSet<QString> toRemove(fullpaths.begin(), fullpaths.end());
    for (int i = m_entries.size() - 1; i >= 0; --i)
    {
        if (toRemove.contains(m_entries[i].fullpath))
        {
            beginRemoveRows({}, i, i);
            m_entries.removeAt(i);
            endRemoveRows();
        }
    }
}

void LibraryModel::clear()
{
    beginResetModel();
    m_entries.clear();
    m_customNames.clear();
    endResetModel();
}

LibraryEntry LibraryModel::entry(const QString& fullpath) const
{
    for (const LibraryEntry& e : m_entries)
        if (e.fullpath == fullpath) return e;
    return {};
}

QModelIndex LibraryModel::index(const QString& fullpath) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].fullpath == fullpath)
            return createIndex(i, COL_NAME);
    return {};
}

QModelIndex LibraryModel::rowIndex(int row, int col) const
{
    if (row < 0 || row >= m_entries.size()) return {};
    return createIndex(row, col);
}

void LibraryModel::setCustomName(const QString& fullpath, const QString& name)
{
    if (name.isEmpty())
        m_customNames.remove(fullpath);
    else
        m_customNames[fullpath] = name;

    // Find the row and notify the view
    for (int i = 0; i < m_entries.size(); ++i)
    {
        if (m_entries[i].fullpath == fullpath)
        {
            const QModelIndex idx = createIndex(i, COL_NAME);
            emit dataChanged(idx, idx, {Qt::DisplayRole, DisplayNameRole, Qt::ToolTipRole});
            break;
        }
    }
}

QString LibraryModel::customName(const QString& fullpath) const
{
    return m_customNames.value(fullpath);
}

// ---------------------------------------------------------------------------
// NDS icon decode — matches EmuInstance::romIcon exactly
// ---------------------------------------------------------------------------

QPixmap LibraryModel::decodeNDSIcon(const quint8 icon[512], const quint16 palette[16])
{
    quint32 rgba[16];
    for (int i = 0; i < 16; ++i)
    {
        const quint8 r = ((palette[i] >>  0) & 0x1F) * 255 / 31;
        const quint8 g = ((palette[i] >>  5) & 0x1F) * 255 / 31;
        const quint8 b = ((palette[i] >> 10) & 0x1F) * 255 / 31;
        const quint8 a = i ? 255 : 0;
        rgba[i] = (static_cast<quint32>(a) << 24)
                | (static_cast<quint32>(b) << 16)
                | (static_cast<quint32>(g) <<  8)
                |  static_cast<quint32>(r);
    }

    quint32 pixels[32 * 32];
    int count = 0;
    for (int ytile = 0; ytile < 4; ++ytile)
        for (int xtile = 0; xtile < 4; ++xtile)
            for (int ypx = 0; ypx < 8; ++ypx)
                for (int xpx = 0; xpx < 8; ++xpx)
                {
                    const quint8 byte = icon[count / 2];
                    const quint8 idx  = (count & 1) ? (byte >> 4) : (byte & 0x0F);
                    pixels[ytile * 256 + ypx * 32 + xtile * 8 + xpx] = rgba[idx];
                    ++count;
                }

    QImage img(reinterpret_cast<const uchar*>(pixels), 32, 32, QImage::Format_RGBA8888);
    return QPixmap::fromImage(img.copy());
}

// ---------------------------------------------------------------------------
// NDS header + icon reading — NO CRC (done async after scan)
// ---------------------------------------------------------------------------

LibraryEntry LibraryModel::readNDSHeader(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    const QFileInfo fi(path);

    melonDS::NDSHeader header;
    const bool hasHeader =
        (f.read(reinterpret_cast<char*>(&header), sizeof(header)) >= 16);
    f.close();

    LibraryEntry e;
    e.fullpath = path;
    e.base     = fi.absolutePath();
    e.filename = fi.fileName();
    e.platform = QStringLiteral("NDS");
    e.fileSize = fi.size();
    e.crc32    = 0; // filled in async by LibraryController

    if (hasHeader)
    {
        QByteArray rawTitle(header.GameTitle, 12);
        const int nullPos = rawTitle.indexOf('\0');
        if (nullPos >= 0) rawTitle.truncate(nullPos);
        e.title    = QString::fromLatin1(rawTitle).trimmed();
        e.gameCode = QString::fromLatin1(header.GameCode, 4);

        const quint32 bannerOffset = header.BannerOffset;
        if (bannerOffset > 0 && bannerOffset < static_cast<quint32>(fi.size()))
        {
            QFile f2(path);
            if (f2.open(QIODevice::ReadOnly) && f2.seek(bannerOffset))
            {
                melonDS::NDSBanner banner;
                const qint64 needed = offsetof(melonDS::NDSBanner, JapaneseTitle);
                if (f2.read(reinterpret_cast<char*>(&banner), needed) >= needed)
                    e.icon = decodeNDSIcon(
                        reinterpret_cast<const quint8*>(banner.Icon),
                        reinterpret_cast<const quint16*>(banner.Palette));
            }
        }
    }

    return e;
}

// ---------------------------------------------------------------------------
// CRC32 — public static, called from background thread
// ---------------------------------------------------------------------------

quint32 LibraryModel::computeFileCRC32(const QString& path)
{
    if (!s_crc32Ready) buildCRC32Table();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return 0;
    quint32 crc = 0xFFFFFFFFu;
    char buf[65536];
    qint64 n;
    while ((n = f.read(buf, sizeof(buf))) > 0)
        for (qint64 i = 0; i < n; ++i)
            crc = s_crc32Table[(crc ^ static_cast<quint8>(buf[i])) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// setCRC — called on main thread via queued connection from worker
// ---------------------------------------------------------------------------

void LibraryModel::setCRC(const QString& fullpath, quint32 crc)
{
    for (int i = 0; i < m_entries.size(); ++i)
    {
        if (m_entries[i].fullpath == fullpath)
        {
            m_entries[i].crc32 = crc;
            const QModelIndex idx = createIndex(i, COL_CRC);
            emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole});
            return;
        }
    }
}

// ---------------------------------------------------------------------------

QString LibraryModel::formatSize(qint64 bytes)
{
    if (bytes <= 0) return {};
    return QLocale::system().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}
