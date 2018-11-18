#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../Config.h"
#include "MelonDS.h"

namespace Config
{
    FILE* GetConfigFile(const char* fileName, const char* permissions)
    {
        FILE* f;

        char* configFile = GetConfigFilePath(fileName);

        // First check application directory
        f = fopen(configFile, permissions);
        delete configFile;
        if (f) return f;

        return NULL;
    }

    char* GetConfigFilePath(const char *fileName)
    {
        char* configDir = MelonDSAndroid::configDir;

        size_t configDirLength = strlen(configDir);
        size_t configFileLength = configDirLength + strlen(fileName);
        char* configFile = new char[configFileLength + 1];
        strcpy(configFile, configDir);
        strcpy(&configFile[configDirLength], fileName);
        configFile[configFileLength] = '\0';
        return configFile;
    }
}
