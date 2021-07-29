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

int SocketBindAnyAddr;
char LANDevice[128];
int DirectLAN;

int SavestateRelocSRAM;

int AudioVolume;
int MicInputType;
char MicWavPath[1024];

char LastROMFolder[1024];

char RecentROMList[10][1024];

int EnableCheats;

int MouseHide;
int MouseHideSeconds;

int PauseLostFocus;

bool EnableJIT;

ConfigEntry PlatformConfigFile[] =
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
    {"HKKey_CycleScreenLayout",   0, &HKKeyMapping[HK_CycleScreenLayout],   -1, NULL, 0},
    {"HKKey_CycleScreenSizing",   0, &HKKeyMapping[HK_CycleScreenSizing],   -1, NULL, 0},
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
    {"HKJoy_CycleScreenLayout",   0, &HKJoyMapping[HK_CycleScreenLayout],   -1, NULL, 0},
    {"HKJoy_CycleScreenSizing",   0, &HKJoyMapping[HK_CycleScreenSizing],   -1, NULL, 0},
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

    {"LimitFPS", 0, &LimitFPS, 0, NULL, 0},
    {"AudioSync", 0, &AudioSync, 1, NULL, 0},
    {"ShowOSD", 0, &ShowOSD, 1, NULL, 0},

    {"ConsoleType", 0, &ConsoleType, 0, NULL, 0},
    {"DirectBoot", 0, &DirectBoot, 1, NULL, 0},

    {"SockBindAnyAddr", 0, &SocketBindAnyAddr, 0, NULL, 0},
    {"LANDevice", 1, LANDevice, 0, "", 127},
    {"DirectLAN", 0, &DirectLAN, 0, NULL, 0},

    {"SavStaRelocSRAM", 0, &SavestateRelocSRAM, 0, NULL, 0},

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

}
