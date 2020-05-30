/*
    Copyright 2016-2019 Arisotura

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
#include "PlatformConfig.h"

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
int ScreenSizing;
int ScreenFilter;

int ScreenUseGL;
int ScreenVSync;
int ScreenRatio;

int LimitFPS;
int AudioSync;
int ShowOSD;

int DirectBoot;

int SocketBindAnyAddr;
char LANDevice[128];
int DirectLAN;

int SavestateRelocSRAM;

int AudioVolume;
int MicInputType;
char MicWavPath[512];

char LastROMFolder[512];


ConfigEntry PlatformConfigFile[] =
{
    {"Key_A",      0, &KeyMapping[0],   32, NULL, 0},
    {"Key_B",      0, &KeyMapping[1],   31, NULL, 0},
    {"Key_Select", 0, &KeyMapping[2],   57, NULL, 0},
    {"Key_Start",  0, &KeyMapping[3],   28, NULL, 0},
    {"Key_Right",  0, &KeyMapping[4],  333, NULL, 0},
    {"Key_Left",   0, &KeyMapping[5],  331, NULL, 0},
    {"Key_Up",     0, &KeyMapping[6],  328, NULL, 0},
    {"Key_Down",   0, &KeyMapping[7],  336, NULL, 0},
    {"Key_R",      0, &KeyMapping[8],   54, NULL, 0},
    {"Key_L",      0, &KeyMapping[9],   86, NULL, 0},
    {"Key_X",      0, &KeyMapping[10],  17, NULL, 0},
    {"Key_Y",      0, &KeyMapping[11],  30, NULL, 0},

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

    {"HKKey_Lid",               0, &HKKeyMapping[HK_Lid],               0x0D, NULL, 0},
    {"HKKey_Mic",               0, &HKKeyMapping[HK_Mic],               0x35, NULL, 0},
    {"HKKey_Pause",             0, &HKKeyMapping[HK_Pause],               -1, NULL, 0},
    {"HKKey_Reset",             0, &HKKeyMapping[HK_Reset],               -1, NULL, 0},
    {"HKKey_FastForward",       0, &HKKeyMapping[HK_FastForward],       0x0F, NULL, 0},
    {"HKKey_FastForwardToggle", 0, &HKKeyMapping[HK_FastForwardToggle],   -1, NULL, 0},

    {"HKJoy_Lid",               0, &HKJoyMapping[HK_Lid],               -1, NULL, 0},
    {"HKJoy_Mic",               0, &HKJoyMapping[HK_Mic],               -1, NULL, 0},
    {"HKJoy_Pause",             0, &HKJoyMapping[HK_Pause],             -1, NULL, 0},
    {"HKJoy_Reset",             0, &HKJoyMapping[HK_Reset],             -1, NULL, 0},
    {"HKJoy_FastForward",       0, &HKJoyMapping[HK_FastForward],       -1, NULL, 0},
    {"HKJoy_FastForwardToggle", 0, &HKJoyMapping[HK_FastForwardToggle], -1, NULL, 0},

    {"JoystickID", 0, &JoystickID, 0, NULL, 0},

    {"WindowWidth",  0, &WindowWidth,  256, NULL, 0},
    {"WindowHeight", 0, &WindowHeight, 384, NULL, 0},
    {"WindowMax", 0, &WindowMaximized, 0, NULL, 0},

    {"ScreenRotation", 0, &ScreenRotation, 0, NULL, 0},
    {"ScreenGap",      0, &ScreenGap,      0, NULL, 0},
    {"ScreenLayout",   0, &ScreenLayout,   0, NULL, 0},
    {"ScreenSizing",   0, &ScreenSizing,   0, NULL, 0},
    {"ScreenFilter",   0, &ScreenFilter,   1, NULL, 0},

    {"ScreenUseGL",     0, &ScreenUseGL,     1, NULL, 0},
    {"ScreenVSync",     0, &ScreenVSync,     0, NULL, 0},
    {"ScreenRatio",     0, &ScreenRatio,     0, NULL, 0},

    {"LimitFPS", 0, &LimitFPS, 0, NULL, 0},
    {"AudioSync", 0, &AudioSync, 1, NULL, 0},
    {"ShowOSD", 0, &ShowOSD, 1, NULL, 0},

    {"DirectBoot", 0, &DirectBoot, 1, NULL, 0},

    {"SockBindAnyAddr", 0, &SocketBindAnyAddr, 0, NULL, 0},
    {"LANDevice", 1, LANDevice, 0, "", 127},
    {"DirectLAN", 0, &DirectLAN, 0, NULL, 0},

    {"SavStaRelocSRAM", 0, &SavestateRelocSRAM, 0, NULL, 0},

    {"AudioVolume", 0, &AudioVolume, 256, NULL, 0},
    {"MicInputType", 0, &MicInputType, 1, NULL, 0},
    {"MicWavPath", 1, MicWavPath, 0, "", 511},

    {"LastROMFolder", 1, LastROMFolder, 0, "", 511},

    {"", -1, NULL, 0, NULL, 0}
};

}
