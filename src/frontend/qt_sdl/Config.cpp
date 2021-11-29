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
#include "Platform.h"
#include "Config.h"


namespace Config
{

int KeyMapping[12];
int JoyMapping[12];

int HKKeyMapping[HK_MAX];
int HKJoyMapping[HK_MAX];

int JoystickID;

int WindowWidth;
int WindowHeight;
int WindowMaximized;

int ScreenRotation;
int ScreenGap;
int ScreenLayout;
int ScreenSwap;
int ScreenSizing;
int IntegerScaling;
int ScreenAspectTop;
int ScreenAspectBot;
int ScreenFilter;

int ScreenUseGL;
int ScreenVSync;
int ScreenVSyncInterval;

int _3DRenderer;
int Threaded3D;

int GL_ScaleFactor;
int GL_BetterPolygons;

int LimitFPS;
int AudioSync;
int ShowOSD;

int ConsoleType;
int DirectBoot;

#ifdef JIT_ENABLED
int JIT_Enable = false;
int JIT_MaxBlockSize = 32;
int JIT_BranchOptimisations = true;
int JIT_LiteralOptimisations = true;
int JIT_FastMemory = true;
#endif

int ExternalBIOSEnable;

char BIOS9Path[1024];
char BIOS7Path[1024];
char FirmwarePath[1024];

char DSiBIOS9Path[1024];
char DSiBIOS7Path[1024];
char DSiFirmwarePath[1024];
char DSiNANDPath[1024];

int DLDIEnable;
char DLDISDPath[1024];
int DLDISize;
int DLDIReadOnly;
int DLDIFolderSync;
char DLDIFolderPath[1024];

int DSiSDEnable;
char DSiSDPath[1024];
int DSiSDSize;
int DSiSDReadOnly;
int DSiSDFolderSync;
char DSiSDFolderPath[1024];

int FirmwareOverrideSettings;
char FirmwareUsername[64];
int FirmwareLanguage;
int FirmwareBirthdayMonth;
int FirmwareBirthdayDay;
int FirmwareFavouriteColour;
char FirmwareMessage[1024];
char FirmwareMAC[18];
int RandomizeMAC;

int SocketBindAnyAddr;
char LANDevice[128];
int DirectLAN;

int SavestateRelocSRAM;

int AudioInterp;
int AudioBitrate;
int AudioVolume;
int MicInputType;
char MicWavPath[1024];

char LastROMFolder[1024];

char RecentROMList[10][1024];

int EnableCheats;

int MouseHide;
int MouseHideSeconds;

int PauseLostFocus;


const char* kConfigFile = "melonDS.ini";

ConfigEntry ConfigFile[] =
{
    {"Key_A",      0, &KeyMapping[0],  -1, NULL, 0},
    {"Key_B",      0, &KeyMapping[1],  -1, NULL, 0},
    {"Key_Select", 0, &KeyMapping[2],  -1, NULL, 0},
    {"Key_Start",  0, &KeyMapping[3],  -1, NULL, 0},
    {"Key_Right",  0, &KeyMapping[4],  -1, NULL, 0},
    {"Key_Left",   0, &KeyMapping[5],  -1, NULL, 0},
    {"Key_Up",     0, &KeyMapping[6],  -1, NULL, 0},
    {"Key_Down",   0, &KeyMapping[7],  -1, NULL, 0},
    {"Key_R",      0, &KeyMapping[8],  -1, NULL, 0},
    {"Key_L",      0, &KeyMapping[9],  -1, NULL, 0},
    {"Key_X",      0, &KeyMapping[10], -1, NULL, 0},
    {"Key_Y",      0, &KeyMapping[11], -1, NULL, 0},

    {"Joy_A",      0, &JoyMapping[0],  -1, NULL, 0},
    {"Joy_B",      0, &JoyMapping[1],  -1, NULL, 0},
    {"Joy_Select", 0, &JoyMapping[2],  -1, NULL, 0},
    {"Joy_Start",  0, &JoyMapping[3],  -1, NULL, 0},
    {"Joy_Right",  0, &JoyMapping[4],  -1, NULL, 0},
    {"Joy_Left",   0, &JoyMapping[5],  -1, NULL, 0},
    {"Joy_Up",     0, &JoyMapping[6],  -1, NULL, 0},
    {"Joy_Down",   0, &JoyMapping[7],  -1, NULL, 0},
    {"Joy_R",      0, &JoyMapping[8],  -1, NULL, 0},
    {"Joy_L",      0, &JoyMapping[9],  -1, NULL, 0},
    {"Joy_X",      0, &JoyMapping[10], -1, NULL, 0},
    {"Joy_Y",      0, &JoyMapping[11], -1, NULL, 0},

    {"HKKey_Lid",                 0, &HKKeyMapping[HK_Lid],                 -1, NULL, 0},
    {"HKKey_Mic",                 0, &HKKeyMapping[HK_Mic],                 -1, NULL, 0},
    {"HKKey_Pause",               0, &HKKeyMapping[HK_Pause],               -1, NULL, 0},
    {"HKKey_Reset",               0, &HKKeyMapping[HK_Reset],               -1, NULL, 0},
    {"HKKey_FastForward",         0, &HKKeyMapping[HK_FastForward],         -1, NULL, 0},
    {"HKKey_FastForwardToggle",   0, &HKKeyMapping[HK_FastForwardToggle],   -1, NULL, 0},
    {"HKKey_FullscreenToggle",    0, &HKKeyMapping[HK_FullscreenToggle],    -1, NULL, 0},
    {"HKKey_SwapScreens",         0, &HKKeyMapping[HK_SwapScreens],         -1, NULL, 0},
    {"HKKey_SolarSensorDecrease", 0, &HKKeyMapping[HK_SolarSensorDecrease], -1, NULL, 0},
    {"HKKey_SolarSensorIncrease", 0, &HKKeyMapping[HK_SolarSensorIncrease], -1, NULL, 0},
    {"HKKey_FrameStep",           0, &HKKeyMapping[HK_FrameStep],           -1, NULL, 0},

    {"HKJoy_Lid",                 0, &HKJoyMapping[HK_Lid],                 -1, NULL, 0},
    {"HKJoy_Mic",                 0, &HKJoyMapping[HK_Mic],                 -1, NULL, 0},
    {"HKJoy_Pause",               0, &HKJoyMapping[HK_Pause],               -1, NULL, 0},
    {"HKJoy_Reset",               0, &HKJoyMapping[HK_Reset],               -1, NULL, 0},
    {"HKJoy_FastForward",         0, &HKJoyMapping[HK_FastForward],         -1, NULL, 0},
    {"HKJoy_FastForwardToggle",   0, &HKJoyMapping[HK_FastForwardToggle],   -1, NULL, 0},
    {"HKJoy_FullscreenToggle",    0, &HKJoyMapping[HK_FullscreenToggle],    -1, NULL, 0},
    {"HKJoy_SwapScreens",         0, &HKJoyMapping[HK_SwapScreens],         -1, NULL, 0},
    {"HKJoy_SolarSensorDecrease", 0, &HKJoyMapping[HK_SolarSensorDecrease], -1, NULL, 0},
    {"HKJoy_SolarSensorIncrease", 0, &HKJoyMapping[HK_SolarSensorIncrease], -1, NULL, 0},
    {"HKJoy_FrameStep",           0, &HKJoyMapping[HK_FrameStep],           -1, NULL, 0},

    {"JoystickID", 0, &JoystickID, 0, NULL, 0},

    {"WindowWidth",  0, &WindowWidth,  256, NULL, 0},
    {"WindowHeight", 0, &WindowHeight, 384, NULL, 0},
    {"WindowMax",    0, &WindowMaximized, 0, NULL, 0},

    {"ScreenRotation", 0, &ScreenRotation, 0, NULL, 0},
    {"ScreenGap",      0, &ScreenGap,      0, NULL, 0},
    {"ScreenLayout",   0, &ScreenLayout,   0, NULL, 0},
    {"ScreenSwap",     0, &ScreenSwap,     0, NULL, 0},
    {"ScreenSizing",   0, &ScreenSizing,   0, NULL, 0},
    {"IntegerScaling", 0, &IntegerScaling, 0, NULL, 0},
    {"ScreenAspectTop",0, &ScreenAspectTop,0, NULL, 0},
    {"ScreenAspectBot",0, &ScreenAspectBot,0, NULL, 0},
    {"ScreenFilter",   0, &ScreenFilter,   1, NULL, 0},

    {"ScreenUseGL",         0, &ScreenUseGL,         0, NULL, 0},
    {"ScreenVSync",         0, &ScreenVSync,         0, NULL, 0},
    {"ScreenVSyncInterval", 0, &ScreenVSyncInterval, 1, NULL, 0},

    {"3DRenderer", 0, &_3DRenderer, 0, NULL, 0},
    {"Threaded3D", 0, &Threaded3D, 1, NULL, 0},

    {"GL_ScaleFactor", 0, &GL_ScaleFactor, 1, NULL, 0},
    {"GL_BetterPolygons", 0, &GL_BetterPolygons, 0, NULL, 0},

    {"LimitFPS", 0, &LimitFPS, 1, NULL, 0},
    {"AudioSync", 0, &AudioSync, 0, NULL, 0},
    {"ShowOSD", 0, &ShowOSD, 1, NULL, 0},

    {"ConsoleType", 0, &ConsoleType, 0, NULL, 0},
    {"DirectBoot", 0, &DirectBoot, 1, NULL, 0},

#ifdef JIT_ENABLED
    {"JIT_Enable", 0, &JIT_Enable, 0, NULL, 0},
    {"JIT_MaxBlockSize", 0, &JIT_MaxBlockSize, 32, NULL, 0},
    {"JIT_BranchOptimisations", 0, &JIT_BranchOptimisations, 1, NULL, 0},
    {"JIT_LiteralOptimisations", 0, &JIT_LiteralOptimisations, 1, NULL, 0},
    #ifdef __APPLE__
        {"JIT_FastMemory", 0, &JIT_FastMemory, 0, NULL, 0},
    #else
        {"JIT_FastMemory", 0, &JIT_FastMemory, 1, NULL, 0},
    #endif
#endif

    {"ExternalBIOSEnable", 0, &ExternalBIOSEnable, 0, NULL, 0},

    {"BIOS9Path", 1, BIOS9Path, 0, "", 1023},
    {"BIOS7Path", 1, BIOS7Path, 0, "", 1023},
    {"FirmwarePath", 1, FirmwarePath, 0, "", 1023},

    {"DSiBIOS9Path", 1, DSiBIOS9Path, 0, "", 1023},
    {"DSiBIOS7Path", 1, DSiBIOS7Path, 0, "", 1023},
    {"DSiFirmwarePath", 1, DSiFirmwarePath, 0, "", 1023},
    {"DSiNANDPath", 1, DSiNANDPath, 0, "", 1023},

    {"DLDIEnable", 0, &DLDIEnable, 0, NULL, 0},
    {"DLDISDPath", 1, DLDISDPath, 0, "dldi.bin", 1023},
    {"DLDISize", 0, &DLDISize, 0, NULL, 0},
    {"DLDIReadOnly", 0, &DLDIReadOnly, 0, NULL, 0},
    {"DLDIFolderSync", 0, &DLDIFolderSync, 0, NULL, 0},
    {"DLDIFolderPath", 1, DLDIFolderPath, 0, "", 1023},

    {"DSiSDEnable", 0, &DSiSDEnable, 0, NULL, 0},
    {"DSiSDPath", 1, DSiSDPath, 0, "dsisd.bin", 1023},
    {"DSiSDSize", 0, &DSiSDSize, 0, NULL, 0},
    {"DSiSDReadOnly", 0, &DSiSDReadOnly, 0, NULL, 0},
    {"DSiSDFolderSync", 0, &DSiSDFolderSync, 0, NULL, 0},
    {"DSiSDFolderPath", 1, DSiSDFolderPath, 0, "", 1023},

    {"FirmwareOverrideSettings", 0, &FirmwareOverrideSettings, false, NULL, 0},
    {"FirmwareUsername", 1, FirmwareUsername, 0, "melonDS", 63},
    {"FirmwareLanguage", 0, &FirmwareLanguage, 1, NULL, 0},
    {"FirmwareBirthdayMonth", 0, &FirmwareBirthdayMonth, 0, NULL, 0},
    {"FirmwareBirthdayDay", 0, &FirmwareBirthdayDay, 0, NULL, 0},
    {"FirmwareFavouriteColour", 0, &FirmwareFavouriteColour, 0, NULL, 0},
    {"FirmwareMessage", 1, FirmwareMessage, 0, "", 1023},
    {"FirmwareMAC", 1, FirmwareMAC, 0, "", 17},
    {"RandomizeMAC", 0, &RandomizeMAC, 0, NULL, 0},

    {"SockBindAnyAddr", 0, &SocketBindAnyAddr, 0, NULL, 0},
    {"LANDevice", 1, LANDevice, 0, "", 127},
    {"DirectLAN", 0, &DirectLAN, 0, NULL, 0},

    {"SavStaRelocSRAM", 0, &SavestateRelocSRAM, 0, NULL, 0},

    {"AudioInterp", 0, &AudioInterp, 0, NULL, 0},
    {"AudioBitrate", 0, &AudioBitrate, 0, NULL, 0},
    {"AudioVolume", 0, &AudioVolume, 256, NULL, 0},
    {"MicInputType", 0, &MicInputType, 1, NULL, 0},
    {"MicWavPath", 1, MicWavPath, 0, "", 1023},

    {"LastROMFolder", 1, LastROMFolder, 0, "", 1023},

    {"RecentROM_0", 1, RecentROMList[0], 0, "", 1023},
    {"RecentROM_1", 1, RecentROMList[1], 0, "", 1023},
    {"RecentROM_2", 1, RecentROMList[2], 0, "", 1023},
    {"RecentROM_3", 1, RecentROMList[3], 0, "", 1023},
    {"RecentROM_4", 1, RecentROMList[4], 0, "", 1023},
    {"RecentROM_5", 1, RecentROMList[5], 0, "", 1023},
    {"RecentROM_6", 1, RecentROMList[6], 0, "", 1023},
    {"RecentROM_7", 1, RecentROMList[7], 0, "", 1023},
    {"RecentROM_8", 1, RecentROMList[8], 0, "", 1023},
    {"RecentROM_9", 1, RecentROMList[9], 0, "", 1023},

    {"EnableCheats", 0, &EnableCheats, 0, NULL, 0},

    {"MouseHide",        0, &MouseHide,        0, NULL, 0},
    {"MouseHideSeconds", 0, &MouseHideSeconds, 5, NULL, 0},
    {"PauseLostFocus",   0, &PauseLostFocus,   0, NULL, 0},

    {"", -1, NULL, 0, NULL, 0}
};


void Load()
{
    ConfigEntry* entry = &ConfigFile[0];
    for (;;)
    {
        if (!entry->Value) break;

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
        for (;;)
        {
            if (!entry->Value) break;

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
    for (;;)
    {
        if (!entry->Value) break;

        if (entry->Type == 0)
            fprintf(f, "%s=%d\r\n", entry->Name, *(int*)entry->Value);
        else
            fprintf(f, "%s=%s\r\n", entry->Name, (char*)entry->Value);

        entry++;
    }

    fclose(f);
}

}
