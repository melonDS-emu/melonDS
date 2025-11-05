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

#include <cstring>
#include "ARDatabaseDAT.h"
#include "Platform.h"

/* FILE FORMAT for usrcheat.dat

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
 * flags (word): bit0-23 = data length, bit24 = flag, bit28 = type
 * item name (null-term string)
 * item description (null-term string)
 * padding (to align data below to 4-byte boundary)
 * item data

 meaning of item type/flag:
 * type=0: cheat, bit0-23 = total data length in words, bit24 = cheat active
 * type=1: category, bit0-23 = number of cheats inside, bit24 = only one cheat may be active in this category

 empty description still takes up one extra byte (for the null terminator)

 for a category: item data is a list of cheat items

 for a cheat: item data is as follows:
 * number of code words (word)
 * code
*/

namespace melonDS
{
using namespace Platform;

// TODO: more user-friendly error reporting

std::string ReadNTString(Platform::FileHandle* f);
void AlignFilePos(Platform::FileHandle* f);


ARDatabaseDAT::ARDatabaseDAT(const std::string& filename)
{
    Filename = filename;

    if (!LoadEntries())
        Error = true;
}

bool ARDatabaseDAT::FindGameCode(u32 gamecode)
{
    auto it = EntryList.find(gamecode);
    if (it == EntryList.end())
        return false;
    if ((*it).second.empty())
        return false;
    return true;
}

ARDatabaseEntryList ARDatabaseDAT::GetEntriesByGameCode(u32 gamecode)
{
    ARDatabaseEntryList ret;
    auto it = EntryList.find(gamecode);
    if (it == EntryList.end())
        return ret;

    for (auto& info : (*it).second)
    {
        ARDatabaseEntry entry;
        LoadCheatCodes(info, entry);
        ret.push_back(entry);
    }

    return ret;
}

bool ARDatabaseDAT::LoadEntries()
{
    FileHandle* f = OpenFile(Filename, FileMode::Read);
    if (!f) return false;

    u64 filelen = FileLength(f);
    if (filelen > 0xFFFFFFFFULL)
    {
        CloseFile(f);
        return false;
    }

    char header[16];
    FileRead(header, 16, 1, f);
    //if (strncmp(header, "R4 CheatCode", 12) != 0)
    if (memcmp(header, "R4 CheatCode\x00\x01\x00\x00", 16) != 0)
    {
        CloseFile(f);
        return false;
    }

    char name[0x3D] = {0};
    FileRead(name, 0x3C, 1, f);
    DBName = name;

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

        EntryInfo entry;
        entry.GameCode = entrydata[0];
        entry.Checksum = entrydata[1];
        entry.Offset = entrydata[2];

        EntryList[entry.GameCode].push_back(entry);
    }

    CloseFile(f);
    return true;
}

bool ARDatabaseDAT::LoadCheatCodes(EntryInfo& info, ARDatabaseEntry& entry)
{
    FileHandle* f = OpenFile(Filename, FileMode::Read);
    if (!f) return false;

    u64 filelen = FileLength(f);
    if (filelen > 0xFFFFFFFFULL)
    {
        CloseFile(f);
        return false;
    }

    if ((info.Offset < 0x100) || (info.Offset >= filelen))
    {
        CloseFile(f);
        return false;
    }

    entry.GameCode = info.GameCode;
    entry.Checksum = info.Checksum;

    FileSeek(f, info.Offset, FileSeekOrigin::Start);

    entry.Name = ReadNTString(f);
    AlignFilePos(f);

    u32 flags[9] = {0};
    FileRead(flags, 4*9, 1, f);

    entry.RootCat.Parent = nullptr;
    entry.RootCat.OnlyOneCodeEnabled = false;
    entry.RootCat.Children.clear();

    ARCodeCat* curcat = &entry.RootCat;
    int catlen = 0;

    u32 numentries = flags[0] & 0xFFFFFF;
    for (u32 i = 0; i < numentries; i++)
    {
        if (IsEndOfFile((f)))
            break;

        u32 itemflags = 0;
        FileRead(&itemflags, 4, 1, f);

        u32 totallen = itemflags & 0xFFFFFF;

        std::string itemname = ReadNTString(f);
        std::string itemdesc = ReadNTString(f);
        AlignFilePos(f);

        if (itemflags & (1<<28))
        {
            // this item is a category

            if ((totallen >= 0x10000) || (totallen == 0))
            {
                Log(LogLevel::Error, "AR: unreasonable category length %08X\n",
                    totallen);
                Log(LogLevel::Error, "game=%s, offset=%08X, cat=%s\n",
                    entry.Name.c_str(), info.Offset, itemname.c_str());

                CloseFile(f);
                return false;
            }

            ARCodeCat cat = {
                    .Parent = &entry.RootCat,
                    .Name = itemname,
                    .Description = itemdesc,
                    .OnlyOneCodeEnabled = !!(itemflags & (1<<24)),
                    .Children = {}
            };
            entry.RootCat.Children.emplace_back(cat);
            curcat = &std::get<ARCodeCat>(entry.RootCat.Children.back());

            catlen = totallen;
        }
        else
        {
            // this item is a code

            u32 codelen = 0;
            FileRead(&codelen, 4, 1, f);

            u32 chklen = itemname.length() + 1 + itemdesc.length() + 1;
            chklen = ((chklen + 3) >> 2) + 1 + codelen;
            if (chklen != totallen)
            {
                Log(LogLevel::Error, "AR: malformed code entry, codelen=%08X, totallen=%08X (expected %08X)\n",
                    codelen, totallen, chklen);
                Log(LogLevel::Error, "game=%s, offset=%08X, cheat=%s\n",
                    entry.Name.c_str(), info.Offset, itemname.c_str());

                CloseFile(f);
                return false;
            }

            if ((codelen >= 0x100000) || (codelen & 1))
            {
                Log(LogLevel::Error, "AR: unreasonable code length %08X\n",
                    codelen);
                Log(LogLevel::Error, "game=%s, offset=%08X, cheat=%s\n",
                    entry.Name.c_str(), info.Offset, itemname.c_str());

                CloseFile(f);
                return false;
            }

            if (catlen == 0)
            {
                curcat = &entry.RootCat;
            }

            ARCode code;
            code.Name = itemname;
            code.Description = itemdesc;
            code.Enabled = !!(itemflags & (1<<24));

            u32* rawcode = new u32[codelen];
            FileRead(rawcode, codelen*4, 1, f);

            for (u32 j = 0; j < codelen; j+=2)
            {
                code.Code.push_back(rawcode[j]);
                code.Code.push_back(rawcode[j+1]);
            }

            delete[] rawcode;

            curcat->Children.emplace_back(code);

            if (catlen >= 0)
                catlen--;
        }
    }

    for (auto& item : entry.RootCat.Children)
    {
        if (!std::holds_alternative<ARCodeCat>(item))
            continue;

        auto& cat = std::get<ARCodeCat>(item);
        if (!cat.OnlyOneCodeEnabled)
            continue;

        // for categories that only allow one code to be enabled:
        // make sure we don't have multiple ones enabled

        bool foundone = false;
        for (auto& childitem : cat.Children)
        {
            auto& code = std::get<ARCode>(childitem);
            if (!code.Enabled) continue;
            if (foundone)
                code.Enabled = false;
            else
                foundone = true;
        }
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

    // TODO might break with UTF8 and shit

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
