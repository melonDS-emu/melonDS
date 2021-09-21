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

#ifndef FATSTORAGE_H
#define FATSTORAGE_H

#include <stdio.h>

#include "types.h"
#include "fatfs/ff.h"


class FATStorage
{
public:
    FATStorage();
    ~FATStorage();

private:
    FILE* file;
    u64 filesize;

    static FILE* FF_File;
    static u64 FF_FileSize;
    static UINT FF_ReadStorage(BYTE* buf, LBA_t sector, UINT num);
    static UINT FF_WriteStorage(BYTE* buf, LBA_t sector, UINT num);

    bool ImportFile(const char* path, const char* in);
    bool BuildSubdirectory(const char* sourcedir, const char* path, int level);
    bool Build(const char* sourcedir, u64 size, const char* filename);
};

#endif // FATSTORAGE_H
