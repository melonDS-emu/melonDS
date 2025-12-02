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

#ifndef ARDATABASEDAT_H
#define ARDATABASEDAT_H

#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include "types.h"
#include "ARCodeFile.h"

namespace melonDS
{

struct ARDatabaseEntry
{
    u32 GameCode;
    u32 Checksum;
    std::string Name;
    ARCodeCat RootCat;
};

typedef std::vector<ARDatabaseEntry> ARDatabaseEntryList;


class ARDatabaseDAT
{
public:
    ARDatabaseDAT(const std::string& filename);
    ~ARDatabaseDAT() noexcept = default;

    bool Error = false;

    std::string GetDBName() const { return DBName; }
    bool FindGameCode(u32 gamecode);
    ARDatabaseEntryList GetEntriesByGameCode(u32 gamecode);

private:
    std::string Filename;
    std::string DBName;

    struct EntryInfo
    {
        u32 GameCode;
        u32 Checksum;
        u32 Offset;
    };

    // list of entries per gamecode
    std::unordered_map<u32, std::vector<EntryInfo>> EntryList;

    bool LoadEntries();
    bool LoadCheatCodes(EntryInfo& info, ARDatabaseEntry& entry);
};

}
#endif // ARDATABASEDAT_H
