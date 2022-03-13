/*
    Copyright 2016-2022 melonDS team

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
bool WindowMaximized;

int ScreenRotation;
int ScreenGap;
int ScreenLayout;
bool ScreenSwap;
int ScreenSizing;
bool IntegerScaling;
int ScreenAspectTop;
int ScreenAspectBot;
bool ScreenFilter;

bool ScreenUseGL;
bool ScreenVSync;
int ScreenVSyncInterval;

int _3DRenderer;
bool Threaded3D;

int GL_ScaleFactor;
bool GL_BetterPolygons;

bool LimitFPS;
bool AudioSync;
bool ShowOSD;

int ConsoleType;
bool DirectBoot;

#ifdef JIT_ENABLED
bool JIT_Enable = false;
int JIT_MaxBlockSize = 32;
bool JIT_BranchOptimisations = true;
bool JIT_LiteralOptimisations = true;
bool JIT_FastMemory = true;
#endif

bool ExternalBIOSEnable;

std::string BIOS9Path;
std::string BIOS7Path;
std::string FirmwarePath;

std::string DSiBIOS9Path;
std::string DSiBIOS7Path;
std::string DSiFirmwarePath;
std::string DSiNANDPath;

bool DLDIEnable;
std::string DLDISDPath;
int DLDISize;
bool DLDIReadOnly;
bool DLDIFolderSync;
std::string DLDIFolderPath;

bool DSiSDEnable;
std::string DSiSDPath;
int DSiSDSize;
bool DSiSDReadOnly;
bool DSiSDFolderSync;
std::string DSiSDFolderPath;

bool FirmwareOverrideSettings;
std::string FirmwareUsername;
int FirmwareLanguage;
int FirmwareBirthdayMonth;
int FirmwareBirthdayDay;
int FirmwareFavouriteColour;
std::string FirmwareMessage;
std::string FirmwareMAC;
bool RandomizeMAC;

bool SocketBindAnyAddr;
std::string LANDevice;
bool DirectLAN;

bool SavestateRelocSRAM;

int AudioInterp;
int AudioBitrate;
int AudioVolume;
int MicInputType;
std::string MicWavPath;

std::string LastROMFolder;

std::string RecentROMList[10];

std::string SaveFilePath;
std::string SavestatePath;
std::string CheatFilePath;

bool EnableCheats;

bool MouseHide;
int MouseHideSeconds;

bool PauseLostFocus;

bool DSBatteryLevelOkay;
int DSiBatteryLevel;
bool DSiBatteryCharging;


const char* kConfigFile = "melonDS.ini";

ConfigEntry ConfigFile[] =
{
    {"Key_A",      0, &KeyMapping[0],  -1},
    {"Key_B",      0, &KeyMapping[1],  -1},
    {"Key_Select", 0, &KeyMapping[2],  -1},
    {"Key_Start",  0, &KeyMapping[3],  -1},
    {"Key_Right",  0, &KeyMapping[4],  -1},
    {"Key_Left",   0, &KeyMapping[5],  -1},
    {"Key_Up",     0, &KeyMapping[6],  -1},
    {"Key_Down",   0, &KeyMapping[7],  -1},
    {"Key_R",      0, &KeyMapping[8],  -1},
    {"Key_L",      0, &KeyMapping[9],  -1},
    {"Key_X",      0, &KeyMapping[10], -1},
    {"Key_Y",      0, &KeyMapping[11], -1},

    {"Joy_A",      0, &JoyMapping[0],  -1},
    {"Joy_B",      0, &JoyMapping[1],  -1},
    {"Joy_Select", 0, &JoyMapping[2],  -1},
    {"Joy_Start",  0, &JoyMapping[3],  -1},
    {"Joy_Right",  0, &JoyMapping[4],  -1},
    {"Joy_Left",   0, &JoyMapping[5],  -1},
    {"Joy_Up",     0, &JoyMapping[6],  -1},
    {"Joy_Down",   0, &JoyMapping[7],  -1},
    {"Joy_R",      0, &JoyMapping[8],  -1},
    {"Joy_L",      0, &JoyMapping[9],  -1},
    {"Joy_X",      0, &JoyMapping[10], -1},
    {"Joy_Y",      0, &JoyMapping[11], -1},

    {"HKKey_Lid",                 0, &HKKeyMapping[HK_Lid],                 -1},
    {"HKKey_Mic",                 0, &HKKeyMapping[HK_Mic],                 -1},
    {"HKKey_Pause",               0, &HKKeyMapping[HK_Pause],               -1},
    {"HKKey_Reset",               0, &HKKeyMapping[HK_Reset],               -1},
    {"HKKey_FastForward",         0, &HKKeyMapping[HK_FastForward],         -1},
    {"HKKey_FastForwardToggle",   0, &HKKeyMapping[HK_FastForwardToggle],   -1},
    {"HKKey_FullscreenToggle",    0, &HKKeyMapping[HK_FullscreenToggle],    -1},
    {"HKKey_SwapScreens",         0, &HKKeyMapping[HK_SwapScreens],         -1},
    {"HKKey_SolarSensorDecrease", 0, &HKKeyMapping[HK_SolarSensorDecrease], -1},
    {"HKKey_SolarSensorIncrease", 0, &HKKeyMapping[HK_SolarSensorIncrease], -1},
    {"HKKey_FrameStep",           0, &HKKeyMapping[HK_FrameStep],           -1},

    {"HKJoy_Lid",                 0, &HKJoyMapping[HK_Lid],                 -1},
    {"HKJoy_Mic",                 0, &HKJoyMapping[HK_Mic],                 -1},
    {"HKJoy_Pause",               0, &HKJoyMapping[HK_Pause],               -1},
    {"HKJoy_Reset",               0, &HKJoyMapping[HK_Reset],               -1},
    {"HKJoy_FastForward",         0, &HKJoyMapping[HK_FastForward],         -1},
    {"HKJoy_FastForwardToggle",   0, &HKJoyMapping[HK_FastForwardToggle],   -1},
    {"HKJoy_FullscreenToggle",    0, &HKJoyMapping[HK_FullscreenToggle],    -1},
    {"HKJoy_SwapScreens",         0, &HKJoyMapping[HK_SwapScreens],         -1},
    {"HKJoy_SolarSensorDecrease", 0, &HKJoyMapping[HK_SolarSensorDecrease], -1},
    {"HKJoy_SolarSensorIncrease", 0, &HKJoyMapping[HK_SolarSensorIncrease], -1},
    {"HKJoy_FrameStep",           0, &HKJoyMapping[HK_FrameStep],           -1},

    {"JoystickID", 0, &JoystickID, 0},

    {"WindowWidth",  0, &WindowWidth,  256},
    {"WindowHeight", 0, &WindowHeight, 384},
    {"WindowMax",    1, &WindowMaximized, false},

    {"ScreenRotation", 0, &ScreenRotation, 0},
    {"ScreenGap",      0, &ScreenGap,      0},
    {"ScreenLayout",   0, &ScreenLayout,   0},
    {"ScreenSwap",     1, &ScreenSwap,     false},
    {"ScreenSizing",   0, &ScreenSizing,   0},
    {"IntegerScaling", 1, &IntegerScaling, false},
    {"ScreenAspectTop",0, &ScreenAspectTop,0},
    {"ScreenAspectBot",0, &ScreenAspectBot,0},
    {"ScreenFilter",   1, &ScreenFilter,   true},

    {"ScreenUseGL",         1, &ScreenUseGL,         false},
    {"ScreenVSync",         1, &ScreenVSync,         false},
    {"ScreenVSyncInterval", 0, &ScreenVSyncInterval, 1},

    {"3DRenderer", 0, &_3DRenderer, 0},
    {"Threaded3D", 1, &Threaded3D, true},

    {"GL_ScaleFactor", 0, &GL_ScaleFactor, 1},
    {"GL_BetterPolygons", 1, &GL_BetterPolygons, false},

    {"LimitFPS", 1, &LimitFPS, true},
    {"AudioSync", 1, &AudioSync, false},
    {"ShowOSD", 1, &ShowOSD, true},

    {"ConsoleType", 0, &ConsoleType, 0},
    {"DirectBoot", 1, &DirectBoot, true},

#ifdef JIT_ENABLED
    {"JIT_Enable", 1, &JIT_Enable, false},
    {"JIT_MaxBlockSize", 0, &JIT_MaxBlockSize, 32},
    {"JIT_BranchOptimisations", 1, &JIT_BranchOptimisations, true},
    {"JIT_LiteralOptimisations", 1, &JIT_LiteralOptimisations, true},
    #ifdef __APPLE__
        {"JIT_FastMemory", 1, &JIT_FastMemory, false},
    #else
        {"JIT_FastMemory", 1, &JIT_FastMemory, true},
    #endif
#endif

    {"ExternalBIOSEnable", 1, &ExternalBIOSEnable, false},

    {"BIOS9Path", 2, &BIOS9Path, (std::string)""},
    {"BIOS7Path", 2, &BIOS7Path, (std::string)""},
    {"FirmwarePath", 2, &FirmwarePath, (std::string)""},

    {"DSiBIOS9Path", 2, &DSiBIOS9Path, (std::string)""},
    {"DSiBIOS7Path", 2, &DSiBIOS7Path, (std::string)""},
    {"DSiFirmwarePath", 2, &DSiFirmwarePath, (std::string)""},
    {"DSiNANDPath", 2, &DSiNANDPath, (std::string)""},

    {"DLDIEnable", 1, &DLDIEnable, false},
    {"DLDISDPath", 2, &DLDISDPath, (std::string)"dldi.bin"},
    {"DLDISize", 0, &DLDISize, 0},
    {"DLDIReadOnly", 1, &DLDIReadOnly, false},
    {"DLDIFolderSync", 1, &DLDIFolderSync, false},
    {"DLDIFolderPath", 2, &DLDIFolderPath, (std::string)""},

    {"DSiSDEnable", 1, &DSiSDEnable, false},
    {"DSiSDPath", 2, &DSiSDPath, (std::string)"dsisd.bin"},
    {"DSiSDSize", 0, &DSiSDSize, 0},
    {"DSiSDReadOnly", 1, &DSiSDReadOnly, false},
    {"DSiSDFolderSync", 1, &DSiSDFolderSync, false},
    {"DSiSDFolderPath", 2, &DSiSDFolderPath, (std::string)""},

    {"FirmwareOverrideSettings", 1, &FirmwareOverrideSettings, false},
    {"FirmwareUsername", 2, &FirmwareUsername, (std::string)"melonDS"},
    {"FirmwareLanguage", 0, &FirmwareLanguage, 1},
    {"FirmwareBirthdayMonth", 0, &FirmwareBirthdayMonth, 1},
    {"FirmwareBirthdayDay", 0, &FirmwareBirthdayDay, 1},
    {"FirmwareFavouriteColour", 0, &FirmwareFavouriteColour, 0},
    {"FirmwareMessage", 2, &FirmwareMessage, (std::string)""},
    {"FirmwareMAC", 2, &FirmwareMAC, (std::string)""},
    {"RandomizeMAC", 1, &RandomizeMAC, false},

    {"SockBindAnyAddr", 1, &SocketBindAnyAddr, false},
    {"LANDevice", 2, &LANDevice, (std::string)""},
    {"DirectLAN", 1, &DirectLAN, false},

    {"SavStaRelocSRAM", 1, &SavestateRelocSRAM, false},

    {"AudioInterp", 0, &AudioInterp, 0},
    {"AudioBitrate", 0, &AudioBitrate, 0},
    {"AudioVolume", 0, &AudioVolume, 256},
    {"MicInputType", 0, &MicInputType, 1},
    {"MicWavPath", 2, &MicWavPath, (std::string)""},

    {"LastROMFolder", 2, &LastROMFolder, (std::string)""},

    {"RecentROM_0", 2, &RecentROMList[0], (std::string)""},
    {"RecentROM_1", 2, &RecentROMList[1], (std::string)""},
    {"RecentROM_2", 2, &RecentROMList[2], (std::string)""},
    {"RecentROM_3", 2, &RecentROMList[3], (std::string)""},
    {"RecentROM_4", 2, &RecentROMList[4], (std::string)""},
    {"RecentROM_5", 2, &RecentROMList[5], (std::string)""},
    {"RecentROM_6", 2, &RecentROMList[6], (std::string)""},
    {"RecentROM_7", 2, &RecentROMList[7], (std::string)""},
    {"RecentROM_8", 2, &RecentROMList[8], (std::string)""},
    {"RecentROM_9", 2, &RecentROMList[9], (std::string)""},

    {"SaveFilePath", 2, &SaveFilePath, (std::string)""},
    {"SavestatePath", 2, &SavestatePath, (std::string)""},
    {"CheatFilePath", 2, &CheatFilePath, (std::string)""},

    {"EnableCheats", 1, &EnableCheats, false},

    {"MouseHide",        1, &MouseHide,        false},
    {"MouseHideSeconds", 0, &MouseHideSeconds, 5},
    {"PauseLostFocus",   1, &PauseLostFocus,   false},

    {"DSBatteryLevelOkay",   1, &DSBatteryLevelOkay, true},
    {"DSiBatteryLevel",    0, &DSiBatteryLevel, 0xF},
    {"DSiBatteryCharging", 1, &DSiBatteryCharging, true},

    {"", -1, nullptr, 0}
};


void Load()
{
    ConfigEntry* entry = &ConfigFile[0];
    for (;;)
    {
        if (!entry->Value) break;

        switch (entry->Type)
        {
        case 0: *(int*)entry->Value = std::get<int>(entry->Default); break;
        case 1: *(bool*)entry->Value = std::get<bool>(entry->Default); break;
        case 2: *(std::string*)entry->Value = std::get<std::string>(entry->Default); break;
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
                switch (entry->Type)
                {
                case 0: *(int*)entry->Value = strtol(entryval, NULL, 10); break;
                case 1: *(bool*)entry->Value = strtol(entryval, NULL, 10) ? true:false; break;
                case 2: *(std::string*)entry->Value = entryval; break;
                }

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

        switch (entry->Type)
        {
        case 0: fprintf(f, "%s=%d\r\n", entry->Name, *(int*)entry->Value); break;
        case 1: fprintf(f, "%s=%d\r\n", entry->Name, *(bool*)entry->Value ? 1:0); break;
        case 2: fprintf(f, "%s=%s\r\n", entry->Name, (*(std::string*)entry->Value).c_str()); break;
        }

        entry++;
    }

    fclose(f);
}

}
