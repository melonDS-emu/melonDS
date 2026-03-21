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

#include <QString>

// LibraryFilter is used by both LibraryToolBar and LibraryController.
// It lives here (mirroring the mGBA layout) so both can include it
// without a circular dependency.

struct LibraryFilter
{
    enum class Section { All = 0, RecentlyPlayed, Favorites } section = Section::All;
    QString platform;    // empty = all; "NDS" for future multi-platform use
    QString searchTerm;

    bool operator==(const LibraryFilter& o) const
    {
        return section == o.section && platform == o.platform && searchTerm == o.searchTerm;
    }
    bool operator!=(const LibraryFilter& o) const { return !(*this == o); }
};
