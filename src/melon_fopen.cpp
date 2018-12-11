/*
    Copyright 2016-2019 StapleButter

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
#include <stdlib.h>
#include <string>
#ifdef _WIN32
#define NTDDI_VERSION		0x06000000 // GROSS FUCKING HACK
#include <windows.h>
//#include <knownfolders.h> // FUCK THAT SHIT
extern "C" const GUID DECLSPEC_SELECTANY FOLDERID_RoamingAppData = {0x3eb685db, 0x65f9, 0x4cf6, {0xa0, 0x3a, 0xe3, 0xef, 0x65, 0x72, 0x9f, 0x3d}};
#include <shlobj.h>
#else
#include <glib.h>
#endif

extern char* EmuDirectory;


#ifdef __WIN32__



FILE* melon_fopen(const char* path, const char* mode)
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

FILE* melon_fopen_local(const char* fileName, const char* permissions)
{
    // Locations are application directory, and AppData/melonDS on windows

    FILE* f;

    // First check current working directory
    f = fopen(fileName, permissions);
    if (f) return f;

    // then emu directory
    {
        int dirlen = strlen(EmuDirectory);
        if (dirlen)
        {
            int filelen = strlen(fileName);
            int len = dirlen + 1 + filelen + 1;
            char* tmp = new char[len];
            strncpy(&tmp[0], EmuDirectory, dirlen);
            tmp[dirlen] = '\\';
            strncpy(&tmp[dirlen+1], fileName, filelen);
            tmp[dirlen+1+filelen] = '\0';

            f = melon_fopen(tmp, permissions);
            delete[] tmp;
            if (f) return f;
        }
    }

    // Now check AppData
    PWSTR appDataPath = NULL;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath);
    if (!appDataPath)
        return NULL;

    // this will be more than enough
    WCHAR fatperm[4];
    fatperm[0] = permissions[0];
    fatperm[1] = permissions[1];
    fatperm[2] = permissions[2];
    fatperm[3] = 0;

    int fnlen = MultiByteToWideChar(CP_UTF8, 0, fileName, -1, NULL, 0);
    if (fnlen < 1) return NULL;
    WCHAR* wfileName = new WCHAR[fnlen];
    int res = MultiByteToWideChar(CP_UTF8, 0, fileName, -1, wfileName, fnlen);
    if (res != fnlen) { delete[] wfileName; return NULL; } // checkme?

    const WCHAR* appdir = L"\\melonDS\\";

    int pos = wcslen(appDataPath);
    void* ptr = CoTaskMemRealloc(appDataPath, (pos+wcslen(appdir)+fnlen+1)*sizeof(WCHAR));
    if (!ptr) { delete[] wfileName; return NULL; } // oh well
    appDataPath = (PWSTR)ptr;

    wcscpy(&appDataPath[pos], appdir); pos += wcslen(appdir);
    wcscpy(&appDataPath[pos], wfileName);

    f = _wfopen(appDataPath, fatperm);
    CoTaskMemFree(appDataPath);
    delete[] wfileName;
    if (f) return f;

    return NULL;

}



#else



FILE* melon_fopen(const char* filename, const char* perm) { return fopen(filename, perm); }

FILE* melon_fopen_local(const char* fileName, const char* permissions)
{
    // Locations are application directory, and XDG_CONFIG_HOME/melonds

    FILE* f;

    // First check current working directory
    f = fopen(fileName, permissions);
    if (f) return f;

    // then emu directory
    {
        int dirlen = strlen(EmuDirectory);
        if (dirlen)
        {
            int filelen = strlen(fileName);
            int len = dirlen + 1 + filelen + 1;
            char* tmp = new char[len];
            strncpy(&tmp[0], EmuDirectory, dirlen);
            tmp[dirlen] = '/';
            strncpy(&tmp[dirlen+1], fileName, filelen);
            tmp[dirlen+1+filelen] = '\0';

            f = fopen(tmp, permissions);
            delete[] tmp;
            if (f) return f;
        }
    }

    // Now check XDG_CONFIG_HOME
    // TODO: check for memory leak there
    std::string path = std::string(g_get_user_config_dir()) + "/melonds/" + fileName;
    f = fopen(path.c_str(), permissions);
    if (f) return f;

    return NULL;

}



#endif
