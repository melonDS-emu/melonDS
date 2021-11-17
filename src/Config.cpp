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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "Config.h"
#include "Platform.h"


namespace Config
{

const char* kConfigFile = "melonDS.ini";

int ExternalBIOSEnable;
char BIOS9Path[1024];
char BIOS7Path[1024];
char FirmwarePath[1024];

char FirmwareUsername[64];
int FirmwareLanguage;
bool FirmwareOverrideSettings;
int FirmwareBirthdayMonth;
int FirmwareBirthdayDay;
int FirmwareFavouriteColour;
char FirmwareMessage[1024];

char DSiBIOS9Path[1024];
char DSiBIOS7Path[1024];
char DSiFirmwarePath[1024];
char DSiNANDPath[1024];

int RandomizeMAC;
int AudioBitrate;

ConfigEntry ConfigFile[] =
{
    {"ExternalBIOSEnable", 0, &ExternalBIOSEnable, 0, NULL, 0},
    {"BIOS9Path", 1, BIOS9Path, 0, "", 1023},
    {"BIOS7Path", 1, BIOS7Path, 0, "", 1023},
    {"FirmwarePath", 1, FirmwarePath, 0, "", 1023},

    {"FirmwareUsername", 1, FirmwareUsername, 0, "melonDS", 63},
    {"FirmwareLanguage", 0, &FirmwareLanguage, 1, NULL, 0},
    {"FirmwareOverrideSettings", 0, &FirmwareOverrideSettings, false, NULL, 0},
    {"FirmwareBirthdayMonth", 0, &FirmwareBirthdayMonth, 0, NULL, 0},
    {"FirmwareBirthdayDay", 0, &FirmwareBirthdayDay, 0, NULL, 0},
    {"FirmwareFavouriteColour", 0, &FirmwareFavouriteColour, 0, NULL, 0},
    {"FirmwareMessage", 1, FirmwareMessage, 0, "", 1023},

    {"DSiBIOS9Path", 1, DSiBIOS9Path, 0, "", 1023},
    {"DSiBIOS7Path", 1, DSiBIOS7Path, 0, "", 1023},
    {"DSiFirmwarePath", 1, DSiFirmwarePath, 0, "", 1023},
    {"DSiNANDPath", 1, DSiNANDPath, 0, "", 1023},

    {"RandomizeMAC", 0, &RandomizeMAC, 0, NULL, 0},
    {"AudioBitrate", 0, &AudioBitrate, 0, NULL, 0},

    {"", -1, NULL, 0, NULL, 0}
};

extern ConfigEntry PlatformConfigFile[];


void Load()
{
    ConfigEntry* entry = &ConfigFile[0];
    int c = 0;
    for (;;)
    {
        if (!entry->Value)
        {
            if (c > 0) break;
            entry = &PlatformConfigFile[0];
            if (!entry->Value) break;
            c++;
        }

        if (entry->Type == 0)
            *(int*)entry->Value = entry->DefaultInt;
        else
        {
            strncpy((char*)entry->Value, entry->DefaultStr, entry->StrLength);
            ((char*)entry->Value)[entry->StrLength] = '\0';
        }

        entry++;
    }

    FILE* f = Platform::OpenLocalFile(kConfigFile, "r");
    if (!f) return;

    char linebuf[1024];
    char entryname[32];
    char entryval[1024];
    while (!feof(f))
    {
        if (fgets(linebuf, 1024, f) == nullptr)
            break;

        int ret = sscanf(linebuf, "%31[A-Za-z_0-9]=%[^\t\r\n]", entryname, entryval);
        entryname[31] = '\0';
        if (ret < 2) continue;

        ConfigEntry* entry = &ConfigFile[0];
        c = 0;
        for (;;)
        {
            if (!entry->Value)
            {
                if (c > 0) break;
                entry = &PlatformConfigFile[0];
                if (!entry->Value) break;
                c++;
            }

            if (!strncmp(entry->Name, entryname, 32))
            {
                if (entry->Type == 0)
                    *(int*)entry->Value = strtol(entryval, NULL, 10);
                else
                    strncpy((char*)entry->Value, entryval, entry->StrLength);

                break;
            }

            entry++;
        }
    }

    fclose(f);
}

void Save()
{
    FILE* f = Platform::OpenLocalFile(kConfigFile, "w");
    if (!f) return;

    ConfigEntry* entry = &ConfigFile[0];
    int c = 0;
    for (;;)
    {
        if (!entry->Value)
        {
            if (c > 0) break;
            entry = &PlatformConfigFile[0];
            if (!entry->Value) break;
            c++;
        }

        if (entry->Type == 0)
            fprintf(f, "%s=%d\r\n", entry->Name, *(int*)entry->Value);
        else
            fprintf(f, "%s=%s\r\n", entry->Name, (char*)entry->Value);

        entry++;
    }

    fclose(f);
}


}
