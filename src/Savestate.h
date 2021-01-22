/*
    Copyright 2016-2020 Arisotura

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
#include "MemoryStream.h"

#define SAVESTATE_MAJOR 7
#define SAVESTATE_MINOR 0

class Savestate
{
public:
    Savestate(const char* filename, bool save);
    Savestate(u8* data, s32 len);
    ~Savestate();

    bool Error;

    bool Saving;
    u32 VersionMajor;
    u32 VersionMinor;

    u32 CurSection;

    void Section(const char* magic);

    void Var8(u8* var);
    void Var16(u16* var);
    void Var32(u32* var);
    void Var64(u64* var);

    void Bool32(bool* var);

    void VarArray(void* data, u32 len);

    bool IsAtleastVersion(u32 major, u32 minor)
    {
        if (VersionMajor > major) return true;
        if (VersionMajor == major && VersionMinor >= minor) return true;
        return false;
    }

    u8* GetData();
    s32 GetDataLength();

private:
    FILE* file;
    MemoryStream* mStream;

    void InitRead();
    void Finalize();

    void Seek(s32 pos, s32 origin);
    s32 Tell();
};

#endif // SAVESTATE_H
