#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NTDDI_VERSION		0x06000000 // GROSS FUCKING HACK
#include <windows.h>
//#include <knownfolders.h> // FUCK THAT SHIT
extern "C" const GUID DECLSPEC_SELECTANY FOLDERID_RoamingAppData = {0x3eb685db, 0x65f9, 0x4cf6, {0xa0, 0x3a, 0xe3, 0xef, 0x65, 0x72, 0x9f, 0x3d}};
#include <shlobj.h>
#include "../Config.h"

namespace Config
{
    FILE* GetConfigFile(const char* fileName, const char* permissions)
    {
        // Locations are application directory, and XDG_CONFIG_HOME/melonds or AppData/MelonDS on windows

        FILE* f;

        // First check application directory
        f = fopen(fileName, permissions);
        if (f) return f;

        // Now check AppData
        PWSTR appDataPath = NULL;
        SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath);
        if (!appDataPath)
            return NULL;

        const WCHAR* appdir = L"\\melonDS\\";

        int fnlen = MultiByteToWideChar(CP_UTF8, 0, fileName, -1, NULL, 0);
        if (fnlen < 1) return NULL;
        WCHAR* wfileName = new WCHAR[fnlen];
        int res = MultiByteToWideChar(CP_UTF8, 0, fileName, -1, wfileName, fnlen);
        if (res != fnlen) { delete[] wfileName; return NULL; } // checkme?

        int pos = wcslen(appDataPath);
        void* ptr = CoTaskMemRealloc(appDataPath, (pos+wcslen(appdir)+fnlen+1)*sizeof(WCHAR));
        if (!ptr) { delete[] wfileName; return NULL; } // oh well
        appDataPath = (PWSTR)ptr;

        wcscpy(&appDataPath[pos], appdir); pos += wcslen(appdir);
        wcscpy(&appDataPath[pos], wfileName);

        // this will be more than enough
        WCHAR fatperm[4];
        fatperm[0] = permissions[0];
        fatperm[1] = permissions[1];
        fatperm[2] = permissions[2];
        fatperm[3] = 0;

        f = _wfopen(appDataPath, fatperm);
        CoTaskMemFree(appDataPath);
        delete[] wfileName;
        if (f) return f;

        return NULL;
    }

    char* GetConfigFilePath(const char *fileName)
    {
        size_t fileNameLength = strlen(fileName);
        char* newPath = new char[fileNameLength + 1];
        strcpy(newPath, fileName);
        return newPath;
    }
}
