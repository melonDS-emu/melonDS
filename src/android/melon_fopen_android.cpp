#include "../melon_fopen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "MelonDS.h"

FILE* melon_fopen(const char* filename, const char* perm)
{
    return fopen(filename, perm);
}

FILE* melon_fopen_local(const char* fileName, const char* permissions)
{
    char* configDir = MelonDSAndroid::configDir;

    size_t configDirLength = strlen(configDir);
    size_t configFileLength = configDirLength + strlen(fileName);
    char* configFile = new char[configFileLength + 1];
    strcpy(configFile, configDir);
    strcpy(&configFile[configDirLength], fileName);
    configFile[configFileLength] = '\0';

    FILE* file = fopen(configFile, permissions);
    delete[] configFile;

    if (file)
        return file;

    return NULL;
}
