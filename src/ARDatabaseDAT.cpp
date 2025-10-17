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

    if (!Load())
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

bool ARDatabaseDAT::Load()
{
    FileHandle* f = OpenFile(Filename, FileMode::ReadText);
    if (!f) return true;

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

    CloseFile(f);
    return true;
}

}
