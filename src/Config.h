/*
    Copyright 2016-2020 Arisotura

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

#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>

#include "types.h"

namespace Config
{

struct ConfigEntry
{
    char Name[32];
    int Type;
    void* Value;
    int DefaultInt;
    const char* DefaultStr;
    int StrLength; // should be set to actual array length minus one
};

FILE* GetConfigFile(const char* fileName, const char* permissions);
bool HasConfigFile(const char* fileName);
void Load();
void Save();

extern char BIOS9Path[1024];
extern char BIOS7Path[1024];
extern char FirmwarePath[1024];
extern int DLDIEnable;
extern char DLDISDPath[1024];

extern char FirmwareUsername[64];
extern int FirmwareLanguage;
extern bool FirmwareOverrideSettings;
extern int FirmwareBirthdayMonth;
extern int FirmwareBirthdayDay;
extern int FirmwareFavouriteColour;
extern char FirmwareMessage[1024];

extern char DSiBIOS9Path[1024];
extern char DSiBIOS7Path[1024];
extern char DSiFirmwarePath[1024];
extern char DSiNANDPath[1024];
extern int DSiSDEnable;
extern char DSiSDPath[1024];

extern int RandomizeMAC;

#ifdef JIT_ENABLED
extern int JIT_Enable;
extern int JIT_MaxBlockSize;
extern int JIT_BranchOptimisations;
extern int JIT_LiteralOptimisations;
extern int JIT_FastMemory;
#endif

}

#endif // CONFIG_H
