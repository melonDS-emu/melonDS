/*
    Copyright 2016-2025 melonDS team

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
#include <string.h>
#include "ARDatabaseDAT.h"
#include "Platform.h"

namespace melonDS
{
using namespace Platform;

// TODO: import codes from other sources (usrcheat.dat, ...)
// TODO: more user-friendly error reporting


ARDatabaseDAT::ARDatabaseDAT(const std::string& filename)
{
    Filename = filename;

    if (!LoadEntries())
        Error = true;
}

/*std::vector<ARCode> ARDatabaseDAT::GetCodes() const noexcept
{
    if (Error)
        return {};

    std::vector<ARCode> codes;

    for (const ARCodeCat& cat : Categories)
    {
        for (const ARCode& code : cat.Codes)
        {
            codes.push_back(code);
        }
    }

    return codes;
}*/

bool ARDatabaseDAT::LoadEntries()
{
    FileHandle* f = OpenFile(Filename, FileMode::Read);
    if (!f) return false;

    // todo: load
    //
    /* FILE FORMAT

     header:
     00: ID string "R4 CheatCode" (12 bytes)
     0C: version? must be 0x100
     10: database description
     4C: ??? (gets set by r4cce.exe, but zero in usrcheat.dat)
     50: database active flag
     100: entry list

     entry:
     00: game code
     04: checksum of ROM header (logical NOT of CRC32 over first 512 bytes)
     08: offset to cheat data (absolute)
     0C: zero

     the entry list is terminated with an all-zero entry

     cheat data: header then items

     header:
     * game name (null-term string)
     * padding (to align entry below to 4-byte boundary)
     * flags (word): bit0-27 = total number of items, bit28-31 = game active (0=inactive, F=active)
     * master codes (8 words) - use unclear

     item:
     * flags (word): bit0-23 = data length in words, bit24 = flag, bit28 = type
     * item name (null-term string)
     * item description (null-term string)
     * padding (to align data below to 4-byte boundary)
     * item data

     meaning of item type/flag:
     * type=0: cheat, bit24 = cheat active
     * type=1: category, bit24 = only one cheat may be active in this category

     empty description still takes up one extra byte (for the null terminator)

     for a category: item data is a list of cheat items

     for a cheat: item data is as follows:
     * number of code words (word)
     * code
    */

    u64 filelen = FileLength(f);

    char header[16];
    FileRead(header, 16, 1, f);
    //if (strncmp(header, "R4 CheatCode", 12) != 0)
    if (memcmp(header, "R4 CheatCode\x00\x01\x00\x00", 16) != 0)
    {
        CloseFile(f);
        return false;
    }

    FileSeek(f, 0x100, FileSeekOrigin::Start);
    while (!IsEndOfFile(f))
    {
        u32 entrydata[4];
        FileRead(entrydata, 16, 1, f);

        // a zero entry marks the end of the entry list
        if (entrydata[0] == 0)
            break;

        if ((entrydata[2] < 0x100) || (entrydata[2] >= filelen))
        {
            Log(LogLevel::Error, "AR: malformed database file (invalid offset %08X)\n", entrydata[2]);
            CloseFile(f);
            return false;
        }

        printf("got entry %08X %08X %08X\n", entrydata[0], entrydata[1], entrydata[2]);

        Entry entry;
        entry.GameCode = entrydata[0];
        entry.Checksum = entrydata[1];
        entry.Offset = entrydata[2];

        EntryList[entry.GameCode].push_back(entry);
    }

    CloseFile(f);
    return true;
}


std::string ReadNTString(Platform::FileHandle* f)
{
    char tmp[256];
    std::string ret;
    int readlen = 0;
    u64 startpos = FilePosition(f);

    while (!IsEndOfFile(f))
    {
        // read 256 bytes of data, see where the string actually ends
        memset(tmp, 0, 256);
        u64 nread = FileRead(tmp, 1, 256, f);

        bool done = false;
        for (int i = 0; i < nread; i++)
        {
            readlen++;
            if (!tmp[i]) { done = true; break; }
            ret += tmp[i];
        }

        if (done)
            break;
    }

    // correct the file position to point right after the end of this string
    FileSeek(f, startpos + readlen, FileSeekOrigin::Start);

    return ret;
}

void AlignFilePos(Platform::FileHandle* f)
{
    u64 pos = FilePosition(f);
    if (pos & 3)
        FileSeek(f, (pos + 3) & (~3), FileSeekOrigin::Start);
}

}
