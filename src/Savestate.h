/*
    Copyright 2016-2019 StapleButter

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

#ifndef SAVESTATE_H
#define SAVESTATE_H

#include <stdio.h>
#include "types.h"

#define SAVESTATE_MAJOR 2
#define SAVESTATE_MINOR 1

class Savestate
{
public:
    Savestate(char* filename, bool save);
    ~Savestate();

    bool Error;

    bool Saving;
    u32 VersionMajor;
    u32 VersionMinor;

    u32 CurSection;

    void Section(char* magic);

    void Var8(u8* var);
    void Var16(u16* var);
    void Var32(u32* var);
    void Var64(u64* var);

    void VarArray(void* data, u32 len);

    bool IsAtleastVersion(u32 major, u32 minor)
    {
        if (VersionMajor > major) return true;
        if (VersionMajor == major && VersionMinor >= minor) return true;
        return false;
    }

private:
    FILE* file;
};

#endif // SAVESTATE_H
