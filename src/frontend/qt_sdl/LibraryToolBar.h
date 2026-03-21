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

#include <QWidget>
#include "LibrarySideBar.h"

class QButtonGroup;
class QComboBox;
class QLineEdit;
class QSlider;

class LibraryToolBar : public QWidget
{
Q_OBJECT

public:
    explicit LibraryToolBar(QWidget* parent = nullptr);

    // Update the "All Games (N)" label in the section combo.
    // Other slots are reserved for potential multi-platform use later.
    void setGameCounts(int total, int /*nds*/, int, int, int);

    // Restore button/slider state from config on startup
    void setViewMode(int mode);   // 0 = list, 2 = grid
    void setCoverSize(int size);  // restore slider position

    int coverSize() const;

signals:
    void filterChanged(const LibraryFilter& filter);
    void viewModeChanged(int mode);   // 0 = list, 2 = grid
    void coverSizeChanged(int size);

private:
    void emitFilter();

    QButtonGroup* m_viewGroup   = nullptr;
    QComboBox*    m_sectionBox  = nullptr;
    QLineEdit*    m_searchBar   = nullptr;
    QSlider*      m_coverSlider = nullptr;
    QWidget*      m_sliderWrap  = nullptr;

    LibraryFilter m_current;
};
