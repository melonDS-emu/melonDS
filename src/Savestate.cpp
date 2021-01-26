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
#include "Savestate.h"
#include "Platform.h"
#include "MemoryStream.h"

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
    mStream = NULL;

    if (save)
    {
        Saving = true;
        if (filename == NULL)
            mStream = new MemoryStream();
        else
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

        InitRead();
    }

    CurSection = -1;
}
Savestate::Savestate(u8* data, s32 len)
{
    Error = false;
    Saving = false;

    file = NULL;
    mStream = new MemoryStream(data, len);
    InitRead();
    if (Error)
    {
        delete mStream;
        return;
    }
   
    CurSection = -1;
}

void Savestate::InitRead()
{
    const char* magic = "MELN";

    u32 len;
    Seek(0, SEEK_END);
    len = Tell();
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
    if (buf != len)
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
    if (mStream) delete mStream;
}

void Savestate::Finalize()
{
    u32 pos = Tell();
    if (CurSection != -1)
    {
        Seek(CurSection+4, SEEK_SET);

        u32 len = pos - CurSection;
        Var32(&len);
    }

    Seek(0, SEEK_END);
    u32 len = Tell();
    Seek(8, SEEK_SET);
    Var32(&len);

    Seek(pos, SEEK_SET);
}

u8* Savestate::GetData()
{
    if (mStream)
    {
        if (Saving)
            Finalize();
        
        return mStream->GetData();
    }
    else if (Error)
        return NULL;
    else // file, but this should never happen
        throw "Not supported: GetData from file-based savestate.";
}
s32 Savestate::GetDataLength()
{
    if (mStream)
        return mStream->GetLength();
    else if (Error)
        return -1;
    else // file, but this should never happen
        throw "Not supported: GetData from file-based savestate.";
}

void Savestate::Section(const char* magic)
{
    if (Error) return;

    if (Saving)
    {
        if (CurSection != -1)
        {
            u32 pos = Tell();
            Seek(CurSection+4, SEEK_SET);

            u32 len = pos - CurSection;
            Var32(&len);

            Seek(pos, SEEK_SET);
        }

        CurSection = Tell();

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
        if (mStream)
            mStream->Write((u8*)data, len);
        else
            fwrite(data, len, 1, file);
    }
    else
    {
        if (mStream)
            mStream->Read((u8*)data, len);
        else
            fread(data, len, 1, file);
    }
}

void Savestate::Seek(s32 pos, s32 origin)
{
    if (mStream)
        mStream->Seek(pos, origin);
    else
        fseek(file, pos, origin);
}

s32 Savestate::Tell()
{
    if (mStream)
        return mStream->Tell();
    else
        return ftell(file);
}
