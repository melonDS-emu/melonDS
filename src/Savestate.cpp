/*
    Copyright 2016-2019 Arisotura

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

/*
    Savestate format

    header:
    00 - magic MELN
    04 - version major
    06 - version minor
    08 - length
    0C - game serial
    10 - ARM9 binary checksum
    14 - ARM7 binary checksum
    18 - reserved
    1C - reserved

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

Savestate::Savestate(char* filename, bool save)
{
    char* magic = "MELN";

    Error = false;

    if (save)
    {
        Saving = true;
        file = Platform::OpenFile(filename, "wb");
        if (!file)
        {
            printf("savestate: file %s doesn't exist\n", filename);
            Error = true;
            return;
        }

        VersionMajor = SAVESTATE_MAJOR;
        VersionMinor = SAVESTATE_MINOR;

        fwrite(magic, 4, 1, file);
        fwrite(&VersionMajor, 2, 1, file);
        fwrite(&VersionMinor, 2, 1, file);
        fseek(file, 8, SEEK_CUR); // length to be fixed later
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

        u32 len;
        fseek(file, 0, SEEK_END);
        len = (u32)ftell(file);
        fseek(file, 0, SEEK_SET);

        u32 buf = 0;

        fread(&buf, 4, 1, file);
        if (buf != ((u32*)magic)[0])
        {
            printf("savestate: invalid magic %08X\n", buf);
            Error = true;
            return;
        }

        VersionMajor = 0;
        VersionMinor = 0;

        fread(&VersionMajor, 2, 1, file);
        if (VersionMajor != SAVESTATE_MAJOR)
        {
            printf("savestate: bad version major %d, expecting %d\n", VersionMajor, SAVESTATE_MAJOR);
            Error = true;
            return;
        }

        fread(&VersionMinor, 2, 1, file);
        // TODO: handle it???

        buf = 0;
        fread(&buf, 4, 1, file);
        if (buf != len)
        {
            printf("savestate: bad length %d\n", buf);
            Error = true;
            return;
        }

        fseek(file, 4, SEEK_CUR);
    }

    CurSection = -1;
}

Savestate::~Savestate()
{
    if (Error) return;

    if (Saving)
    {
        if (CurSection != -1)
        {
            u32 pos = (u32)ftell(file);
            fseek(file, CurSection+4, SEEK_SET);

            u32 len = pos - CurSection;
            fwrite(&len, 4, 1, file);

            fseek(file, pos, SEEK_SET);
        }

        fseek(file, 0, SEEK_END);
        u32 len = (u32)ftell(file);
        fseek(file, 8, SEEK_SET);
        fwrite(&len, 4, 1, file);
    }

    if (file) fclose(file);
}

void Savestate::Section(char* magic)
{
    if (Error) return;

    if (Saving)
    {
        if (CurSection != -1)
        {
            u32 pos = (u32)ftell(file);
            fseek(file, CurSection+4, SEEK_SET);

            u32 len = pos - CurSection;
            fwrite(&len, 4, 1, file);

            fseek(file, pos, SEEK_SET);
        }

        CurSection = (u32)ftell(file);

        fwrite(magic, 4, 1, file);
        fseek(file, 12, SEEK_CUR);
    }
    else
    {
        fseek(file, 0x10, SEEK_SET);

        for (;;)
        {
            u32 buf = 0;

            fread(&buf, 4, 1, file);
            if (buf != ((u32*)magic)[0])
            {
                if (buf == 0)
                {
                    printf("savestate: section %s not found. blarg\n", magic);
                    return;
                }

                buf = 0;
                fread(&buf, 4, 1, file);
                fseek(file, buf-8, SEEK_CUR);
                continue;
            }

            fseek(file, 12, SEEK_CUR);
            break;
        }
    }
}

void Savestate::Var8(u8* var)
{
    if (Error) return;

    if (Saving)
    {
        fwrite(var, 1, 1, file);
    }
    else
    {
        fread(var, 1, 1, file);
    }
}

void Savestate::Var16(u16* var)
{
    if (Error) return;

    if (Saving)
    {
        fwrite(var, 2, 1, file);
    }
    else
    {
        fread(var, 2, 1, file);
    }
}

void Savestate::Var32(u32* var)
{
    if (Error) return;

    if (Saving)
    {
        fwrite(var, 4, 1, file);
    }
    else
    {
        fread(var, 4, 1, file);
    }
}

void Savestate::Var64(u64* var)
{
    if (Error) return;

    if (Saving)
    {
        fwrite(var, 8, 1, file);
    }
    else
    {
        fread(var, 8, 1, file);
    }
}

void Savestate::VarArray(void* data, u32 len)
{
    if (Error) return;

    if (Saving)
    {
        fwrite(data, len, 1, file);
    }
    else
    {
        fread(data, len, 1, file);
    }
}
