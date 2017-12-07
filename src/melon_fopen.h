/*
    Copyright 2016-2017 StapleButter

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

#ifndef MELON_FOPEN_H
#define MELON_FOPEN_H

#ifdef __WIN32__

#include <windows.h>

static FILE* melon_fopen(const char* path, const char* mode)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (len < 1) return NULL;
    WCHAR* fatass = new WCHAR[len];
    int res = MultiByteToWideChar(CP_UTF8, 0, path, -1, fatass, len);
    if (res != len) { delete[] fatass; return NULL; } // checkme?

    // this will be more than enough
    WCHAR fatmode[4];
    fatmode[0] = mode[0];
    fatmode[1] = mode[1];
    fatmode[2] = mode[2];
    fatmode[3] = 0;

    FILE* ret = _wfopen(fatass, fatmode);
    delete[] fatass;
    return ret;
}

#else

#define melon_fopen fopen

#endif

#endif // MELON_FOPEN_H
