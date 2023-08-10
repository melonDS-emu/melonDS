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

#ifndef ARCODEFILE_H
#define ARCODEFILE_H

#include <string>
#include <list>
#include <vector>
#include "types.h"

struct ARCode
{
    std::string Name;
    bool Enabled;
    std::vector<u32> Code;
};

typedef std::list<ARCode> ARCodeList;

struct ARCodeCat
{
    std::string Name;
    ARCodeList Codes;
};

typedef std::list<ARCodeCat> ARCodeCatList;


class ARCodeFile
{
public:
    ARCodeFile(const std::string& filename);
    ~ARCodeFile();

    bool Error;

    bool Load();
    bool Save();

    ARCodeCatList Categories;

private:
    std::string Filename;
};

#endif // ARCODEFILE_H
