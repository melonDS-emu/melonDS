/*
    Copyright 2016-2021 Arisotura

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

#include <list>

#include "types.h"

struct ARCode
{
    char Name[128];
    bool Enabled;
    u32 CodeLen;
    u32 Code[2*64];
};

typedef std::list<ARCode> ARCodeList;

struct ARCodeCat
{
    char Name[128];
    ARCodeList Codes;
};

typedef std::list<ARCodeCat> ARCodeCatList;


class ARCodeFile
{
public:
    ARCodeFile(const char* filename);
    ~ARCodeFile();

    bool Error;

    bool Load();
    bool Save();

    ARCodeCatList Categories;

private:
    char Filename[1024];
};

#endif // ARCODEFILE_H
