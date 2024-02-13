/*
    Copyright 2016-2023 melonDS team

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

#ifndef PLATFORMCONFIG_H
#define PLATFORMCONFIG_H

#include <variant>
#include <string>

enum
{
    HK_Lid = 0,
    HK_Mic,
    HK_Pause,
    HK_Reset,
    HK_FastForward,
    HK_FastForwardToggle,
    HK_FullscreenToggle,
    HK_SwapScreens,
    HK_SwapScreenEmphasis,
    HK_SolarSensorDecrease,
    HK_SolarSensorIncrease,
    HK_FrameStep,
    HK_PowerButton,
    HK_VolumeUp,
    HK_VolumeDown,
    HK_MAX
};

enum
{
    micInputType_Silence,
    micInputType_External,
    micInputType_Noise,
    micInputType_Wav,
    micInputType_MAX,
};

namespace Config
{

struct ConfigEntry
{
    char Name[32];
    int Type;           // 0=int 1=bool 2=string 3=64bit int
    void* Value;        // pointer to the value variable
    std::variant<int, bool, std::string, int64_t> Default;
    bool InstanceUnique; // whether the setting can exist individually for each instance in multiplayer
};

struct CameraConfig
{
    int InputType; // 0=blank 1=image 2=camera
    std::string ImagePath;
    std::string CamDeviceName;
    bool XFlip;
};


extern int KeyMapping[12];
extern int JoyMapping[12];

extern int HKKeyMapping[HK_MAX];
extern int HKJoyMapping[HK_MAX];

extern int JoystickID;

extern int WindowWidth;
extern int WindowHeight;
extern bool WindowMaximized;

extern int ScreenRotation;
extern int ScreenGap;
extern int ScreenLayout;
extern bool ScreenSwap;
extern int ScreenSizing;
extern int ScreenAspectTop;
extern int ScreenAspectBot;
extern bool IntegerScaling;
extern bool ScreenFilter;

extern bool ScreenUseGL;
extern bool ScreenVSync;
extern int ScreenVSyncInterval;

extern int _3DRenderer;
extern bool Threaded3D;

extern int GL_ScaleFactor;
extern bool GL_BetterPolygons;

extern bool LimitFPS;
extern int MaxFPS;
extern bool AudioSync;
extern bool ShowOSD;

extern int ConsoleType;
extern bool DirectBoot;

#ifdef JIT_ENABLED
extern bool JIT_Enable;
extern int JIT_MaxBlockSize;
extern bool JIT_BranchOptimisations;
extern bool JIT_LiteralOptimisations;
extern bool JIT_FastMemory;
#endif

extern bool ExternalBIOSEnable;

extern std::string BIOS9Path;
extern std::string BIOS7Path;
extern std::string FirmwarePath;

extern std::string DSiBIOS9Path;
extern std::string DSiBIOS7Path;
extern std::string DSiFirmwarePath;
extern std::string DSiNANDPath;

extern bool DLDIEnable;
extern std::string DLDISDPath;
extern int DLDISize;
extern bool DLDIReadOnly;
extern bool DLDIFolderSync;
extern std::string DLDIFolderPath;

extern bool DSiSDEnable;
extern std::string DSiSDPath;
extern int DSiSDSize;
extern bool DSiSDReadOnly;
extern bool DSiSDFolderSync;
extern std::string DSiSDFolderPath;

extern bool FirmwareOverrideSettings;
extern std::string FirmwareUsername;
extern int FirmwareLanguage;
extern int FirmwareBirthdayMonth;
extern int FirmwareBirthdayDay;
extern int FirmwareFavouriteColour;
extern std::string FirmwareMessage;
extern std::string FirmwareMAC;
extern std::string WifiSettingsPath;

extern int MPAudioMode;
extern int MPRecvTimeout;

extern std::string LANDevice;
extern bool DirectLAN;

extern bool SavestateRelocSRAM;

extern int AudioInterp;
extern int AudioBitDepth;
extern int AudioVolume;
extern bool DSiVolumeSync;
extern int MicInputType;
extern std::string MicDevice;
extern std::string MicWavPath;

extern std::string LastROMFolder;
extern std::string LastBIOSFolder;

extern std::string RecentROMList[10];

extern std::string SaveFilePath;
extern std::string SavestatePath;
extern std::string CheatFilePath;

extern bool EnableCheats;

extern bool MouseHide;
extern int MouseHideSeconds;
extern bool PauseLostFocus;
extern std::string UITheme;

extern int64_t RTCOffset;

extern bool DSBatteryLevelOkay;
extern int DSiBatteryLevel;
extern bool DSiBatteryCharging;

extern bool DSiFullBIOSBoot;

extern CameraConfig Camera[2];

extern bool GdbEnabled;
extern int GdbPortARM7;
extern int GdbPortARM9;
extern bool GdbARM7BreakOnStartup;
extern bool GdbARM9BreakOnStartup;


bool Load();
void Save();

}

#endif // PLATFORMCONFIG_H
