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

#include <cstring>
#include <string>
#include <stdio.h>
#include "types.h"

#define SAVESTATE_MAJOR 10
#define SAVESTATE_MINOR 1

class Savestate
{
public:
    static constexpr u32 DEFAULT_SIZE = 32 * 1024 * 1024; // 32 MB
    Savestate(void* buffer, u32 size, bool save);
    explicit Savestate(u32 initial_size = DEFAULT_SIZE);

    ~Savestate();

    bool Error;

    bool Saving;

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
    void Rewind(bool save);

    bool IsAtLeastVersion(u32 major, u32 minor)
    {
        u16 major_version = MajorVersion();
        if (MajorVersion() > major) return true;
        if (major_version == major && MinorVersion() >= minor) return true;
        return false;
    }

    void* Buffer() { return buffer; }
    [[nodiscard]] const void* Buffer() const { return buffer; }

    [[nodiscard]] u32 BufferLength() const { return buffer_length; }

    [[nodiscard]] u32 Length() const { return buffer_offset; }

    [[nodiscard]] u16 MajorVersion() const
    {
        // major version is stored at offset 0x04
        u16 major = 0;
        memcpy(&major, buffer + 0x04, sizeof(major));
        return major;
    }

    [[nodiscard]] u16 MinorVersion() const
    {
        // minor version is stored at offset 0x06
        u16 minor = 0;
        memcpy(&minor, buffer + 0x06, sizeof(minor));
        return minor;
    }

private:
    static constexpr u32 NO_SECTION = 0xffffffff;
    void CloseCurrentSection();
    bool Resize(u32 new_length);
    void WriteSavestateHeader();
    void WriteStateLength();
    u32 FindSection(const char* magic) const;
    u8* buffer;
    u32 buffer_offset;
    u32 buffer_length;
    bool buffer_owned;
    bool finished;
};

#endif // SAVESTATE_H
