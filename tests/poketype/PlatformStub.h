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

#ifndef POKETYPE_PLATFORMSTUB_H
#define POKETYPE_PLATFORMSTUB_H

#include <vector>

#include "types.h"

// one Platform::WriteNDSSave call, recorded so tests can check a save was flushed
struct SaveWrite
{
    melonDS::u32 Offset;
    melonDS::u32 Len;
};

std::vector<SaveWrite>& SaveWrites();
void ClearSaveWrites();

#endif // POKETYPE_PLATFORMSTUB_H
