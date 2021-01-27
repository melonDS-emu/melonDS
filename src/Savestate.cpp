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

#include <stdio.h>
#include <vector>
#include "Savestate.h"
#include "Platform.h"

/*
    Savestate format

    header:
    00 - magic MELN
    04 - version major
    06 - version minor
    08 - length
    0C - reserved (should be game serial later!)

    section header:
    00 - section magic
    04 - section length
    08 - reserved
    0C - reserved

    Implementation details

    version difference:
    * different major means savestate file is incompatible
    * different minor means adjustments may have to be made
*/

Savestate::Savestate(const char* filename, bool save)
{
    const char* magic = "MELN";

    Error = false;
    file = NULL;

    pos = 0;
    size = 0;

    if (save)
    {
        Saving = true;
        if (filename != NULL)
        {
            file = Platform::OpenFile(filename, "wb");
            if (!file)
            {
                printf("savestate: file %s doesn't exist\n", filename);
                Error = true;
                return;
            }
        }

        VersionMajor = SAVESTATE_MAJOR;
        VersionMinor = SAVESTATE_MINOR;

        buffer = std::vector<u8>(1 << 23); // 8 MB

        VarArray((void*)magic, 4);
        // major/minor versions are 2 bytes each in the file, though we keep them as u32
        VarArray(&VersionMajor, 2);
        VarArray(&VersionMinor, 2);
        Seek(8, SEEK_CUR); // length to be fixed later
    }
    else
    {
        Saving = false;
        file = Platform::OpenFile(filename, "rb");
        if (!file)
        {
            printf("savestate: file %s doesn't exist\n", filename);
            Error = true;
            return;
        }
        else
        {
            fseek(file, 0, SEEK_END);
            s32 len = ftell(file);
            fseek(file, 0, SEEK_SET);

            buffer = std::vector<u8>(len);
            fread(buffer.data(), 1, len, file);
            fclose(file);
            size = len;

            InitRead();
        }
    }

    CurSection = -1;
}
Savestate::Savestate(u8* data, s32 len)
{
    Error = false;
    Saving = false;

    file = NULL;

    buffer = std::vector<u8>(data, data + len);
    pos = 0;
    size = len;

    InitRead();
    if (Error)
        return;
   
    CurSection = -1;
}

void Savestate::InitRead()
{
    const char* magic = "MELN";

    Seek(0, SEEK_SET);

    u32 buf = 0;

    Var32(&buf);
    if (buf != ((u32*)magic)[0])
    {
        printf("savestate: invalid magic %08X\n", buf);
        Error = true;
        return;
    }

    VersionMajor = 0;
    VersionMinor = 0;

    // major/minor versions are 2 bytes each in the file, though we keep them as u32
    VarArray(&VersionMajor, 2);
    if (VersionMajor != SAVESTATE_MAJOR)
    {
        printf("savestate: bad version major %d, expecting %d\n", VersionMajor, SAVESTATE_MAJOR);
        Error = true;
        return;
    }

    VarArray(&VersionMinor, 2);
    if (VersionMinor > SAVESTATE_MINOR)
    {
        printf("savestate: state from the future, %d > %d\n", VersionMinor, SAVESTATE_MINOR);
        Error = true;
        return;
    }

    buf = 0;
    Var32(&buf);
    if (buf != size)
    {
        printf("savestate: bad length %d\n", buf);
        Error = true;
        return;
    }

    Seek(4, SEEK_CUR);
}

Savestate::~Savestate()
{
    if (Error) return;

    if (Saving && file)
        Finalize();

    if (file) fclose(file);
}

void Savestate::Finalize()
{
    u32 oldPos = pos;
    if (CurSection != -1)
    {
        Seek(CurSection+4, SEEK_SET);

        u32 len = oldPos - CurSection;
        Var32(&len);
    }

    Seek(8, SEEK_SET);
    Var32((u32*)&size);

    Seek(oldPos, SEEK_SET);

    // write actual file
    if (file)
    {
        fwrite(buffer.data(), 1, size, file);
        fclose(file);
        file == NULL;
    }
}

u8* Savestate::GetData()
{
    if (Error)
        return NULL;

    if (Saving)
        Finalize();
    
    return buffer.data();
}
s32 Savestate::GetDataLength()
{
    if (Error)
        return -1;

    return size;
}

void Savestate::ExpandCapacity()
{
    buffer.resize(buffer.size() << 1);
}


void Savestate::Section(const char* magic)
{
    if (Error) return;

    if (Saving)
    {
        if (CurSection != -1)
        {
            u32 oldPos = pos;
            Seek(CurSection+4, SEEK_SET);

            u32 len = oldPos - CurSection;
            Var32(&len);

            Seek(oldPos, SEEK_SET);
        }

        CurSection = pos;

        VarArray((void*)magic, 4);
        Seek(12, SEEK_CUR);
    }
    else
    {
        Seek(0x10, SEEK_SET);

        for (;;)
        {
            u32 buf = 0;

            Var32(&buf);
            if (buf != ((u32*)magic)[0])
            {
                if (buf == 0)
                {
                    printf("savestate: section %s not found. blarg\n", magic);
                    return;
                }

                buf = 0;
                Var32(&buf);
                Seek(buf-8, SEEK_CUR);
                continue;
            }

            Seek(12, SEEK_CUR);
            break;
        }
    }
}

void Savestate::Var8(u8* var)
{
    VarArray(var, 1);
}

void Savestate::Var16(u16* var)
{
    VarArray(var, 2);
}

void Savestate::Var32(u32* var)
{
    VarArray(var, 4);
}

void Savestate::Var64(u64* var)
{
    VarArray(var, 8);
}

void Savestate::Bool32(bool* var)
{
    // for compability
    if (Saving)
    {
        u32 val = *var;
        Var32(&val);
    }
    else
    {
        u32 val;
        Var32(&val);
        *var = val != 0;
    }
}

void Savestate::VarArray(void* data, u32 len)
{
    if (Error) return;

    if (Saving)
    {
        while (pos + len > buffer.size())
            ExpandCapacity();

        std::copy((u8*)data, (u8*)data + len, buffer.data() + pos);		
        if (pos > size)
            size = pos;
    }
    else
    {
        if (pos + len > size)
            return;

        std::copy(buffer.data() + pos, buffer.data() + pos + len, (u8*)data);
    }

    pos += len;
}

void Savestate::Seek(s32 pos, s32 origin)
{
    if (origin == SEEK_SET)
    {
        if (pos < 0)
            return;
        this->pos = pos;
    }
    else if (origin == SEEK_CUR)
        Seek(this->pos + pos, SEEK_SET);
    else if (origin == SEEK_END)
        Seek(size - pos, SEEK_SET);
}
