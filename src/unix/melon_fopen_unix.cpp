#include "../melon_fopen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <glib.h>

extern char* EmuDirectory;

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
