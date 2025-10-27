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

#ifndef ARCODEFILE_H
#define ARCODEFILE_H

#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include "types.h"

namespace melonDS
{

struct ARCodeCat;

struct ARCode
{
    ARCodeCat* Parent;
    std::string Name;
    std::string Description;
    bool Enabled;
    std::vector<u32> Code;
};

typedef std::list<ARCode> ARCodeList;

struct ARCodeCat
{
    bool IsRoot;
    std::string Name;
    std::string Description;
    bool OnlyOneCodeEnabled;
    ARCodeList Codes;
};

typedef std::list<ARCodeCat> ARCodeCatList;

struct ARDatabaseEntry;
typedef std::unordered_map<ARCode*, bool> ARCodeEnableMap;


class ARCodeFile
{
public:
    ARCodeFile(const std::string& filename);
    ~ARCodeFile() noexcept = default;

    [[nodiscard]] std::vector<ARCode> GetCodes() const noexcept;

    bool Error = false;

    bool Load();
    bool Save();

    void Import(ARDatabaseEntry& dbentry, ARCodeEnableMap& enablemap, bool clear);

    ARCodeCatList Categories {};

private:
    std::string Filename;

    void FinalizeList();
};

}
#endif // ARCODEFILE_H
