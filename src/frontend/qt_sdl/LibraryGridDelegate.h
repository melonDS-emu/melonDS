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

#include <QStyledItemDelegate>

class LibraryCoverManager;

// Paints DuckStation-style cover cards in a QListView (IconMode).
// Each card shows:
//   - Cover image if available, else a coloured NDS-red placeholder
//   - Game title below the art
//   - Selection / hover highlight
class LibraryGridDelegate : public QStyledItemDelegate
{
Q_OBJECT

public:
    static constexpr int CORNER = 6;

    explicit LibraryGridDelegate(LibraryCoverManager* covers, QObject* parent = nullptr);

    void  setCoverSize(int size) { m_coverSize = size; }
    int   coverSize() const { return m_coverSize; }
    QSize cardSize() const;  // full card size for QListView::setGridSize

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    LibraryCoverManager* m_covers;
    int m_coverSize = 160;
};
