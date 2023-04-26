/*
    Copyright 2016-2022 melonDS team

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

#include <string>
#include <stdio.h>
#include "types.h"

#define SAVESTATE_MAJOR 10
#define SAVESTATE_MINOR 0

class Savestate
{
public:
    static constexpr u32 DEFAULT_SIZE = 16 * 1024 * 1024; // 16 MB
    [[deprecated]] Savestate(std::string filename, bool save);
    Savestate(void* buffer, u32 size, bool save);
    Savestate(u32 initial_size = DEFAULT_SIZE);

    ~Savestate();

    bool Error;

    bool Saving;
    u16 VersionMajor;
    u16 VersionMinor;

    u32 CurSection;

    void Section(const char* magic);

    void Var8(u8* var)
    {
        VarArray(var, sizeof(*var));
    }

    void Var16(u16* var)
    {
        VarArray(var, sizeof(*var));
    }

    void Var32(u32* var)
    {
        VarArray(var, sizeof(*var));
    }

    void Var64(u64* var)
    {
        VarArray(var, sizeof(*var));
    }

    void Bool32(bool* var);

    void VarArray(void* data, u32 len);

    void Finish();

    // TODO rewinds the stream
    void Reset(bool save);

    bool IsAtleastVersion(u32 major, u32 minor)
    {
        if (VersionMajor > major) return true;
        if (VersionMajor == major && VersionMinor >= minor) return true;
        return false;
    }

    void* Buffer() { return buffer; }
    [[nodiscard]] const void* Buffer() const { return buffer; }

    [[nodiscard]] u32 BufferLength() const { return buffer_length; }

    [[nodiscard]] u32 Length() const { return buffer_offset; }

private:
    static constexpr u32 NO_SECTION = 0xffffffff;
    void CloseCurrentSection();
    bool Resize(u32 new_length);
    void WriteSavestateHeader();
    u8* buffer;
    u32 buffer_offset;
    u32 buffer_length;
    bool buffer_owned;
    bool finished;
};

#endif // SAVESTATE_H
