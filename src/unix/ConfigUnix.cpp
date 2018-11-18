#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
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

        // Now check XDG_CONFIG_HOME
        // TODO: check for memory leak there
        std::string path = std::string(g_get_user_config_dir()) + "/melonds/" + fileName;
        f = fopen(path.c_str(), permissions);
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
