/*
    Copyright 2016-2024 melonDS team

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

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <optional>
#include <vector>
#include <string>
#include <algorithm>

#include <SDL2/SDL.h>

#include "main.h"

#include "types.h"
#include "version.h"

#include "ScreenLayout.h"

#include "Args.h"
#include "NDS.h"
#include "NDSCart.h"
#include "GBACart.h"
#include "GPU.h"
#include "SPU.h"
#include "Wifi.h"
#include "Platform.h"
#include "LocalMP.h"
#include "Config.h"
#include "RTC.h"
#include "DSi.h"
#include "DSi_I2C.h"
#include "GPU3D_Soft.h"
#include "GPU3D_OpenGL.h"
#include "GPU3D_Compute.h"

#include "Savestate.h"

#include "EmuInstance.h"

using namespace melonDS;


EmuThread::EmuThread(EmuInstance* inst, QObject* parent) : QThread(parent)
{
    emuInstance = inst;

    emuStatus = emuStatus_Paused;
    emuPauseStack = emuPauseStackRunning;
    emuActive = false;
}

void EmuThread::attachWindow(MainWindow* window)
{
    connect(this, SIGNAL(windowTitleChange(QString)), window, SLOT(onTitleUpdate(QString)));
    connect(this, SIGNAL(windowEmuStart()), window, SLOT(onEmuStart()));
    connect(this, SIGNAL(windowEmuStop()), window, SLOT(onEmuStop()));
    connect(this, SIGNAL(windowEmuPause(bool)), window, SLOT(onEmuPause(bool)));
    connect(this, SIGNAL(windowEmuReset()), window, SLOT(onEmuReset()));
    connect(this, SIGNAL(autoScreenSizingChange(int)), window->panel, SLOT(onAutoScreenSizingChanged(int)));
    connect(this, SIGNAL(windowFullscreenToggle()), window, SLOT(onFullscreenToggled()));
    connect(this, SIGNAL(screenEmphasisToggle()), window, SLOT(onScreenEmphasisToggled()));

    if (window->winHasMenu())
    {
        connect(this, SIGNAL(windowLimitFPSChange()), window->actLimitFramerate, SLOT(trigger()));
        connect(this, SIGNAL(swapScreensToggle()), window->actScreenSwap, SLOT(trigger()));
    }
}

void EmuThread::detachWindow(MainWindow* window)
{
    disconnect(this, SIGNAL(windowTitleChange(QString)), window, SLOT(onTitleUpdate(QString)));
    disconnect(this, SIGNAL(windowEmuStart()), window, SLOT(onEmuStart()));
    disconnect(this, SIGNAL(windowEmuStop()), window, SLOT(onEmuStop()));
    disconnect(this, SIGNAL(windowEmuPause(bool)), window, SLOT(onEmuPause(bool)));
    disconnect(this, SIGNAL(windowEmuReset()), window, SLOT(onEmuReset()));
    disconnect(this, SIGNAL(autoScreenSizingChange(int)), window->panel, SLOT(onAutoScreenSizingChanged(int)));
    disconnect(this, SIGNAL(windowFullscreenToggle()), window, SLOT(onFullscreenToggled()));
    disconnect(this, SIGNAL(screenEmphasisToggle()), window, SLOT(onScreenEmphasisToggled()));

    if (window->winHasMenu())
    {
        disconnect(this, SIGNAL(windowLimitFPSChange()), window->actLimitFramerate, SLOT(trigger()));
        disconnect(this, SIGNAL(swapScreensToggle()), window->actScreenSwap, SLOT(trigger()));
    }
}




























// melonPrime
static bool hasInitialized = false;
float mouseX;
float mouseY;
#include "melonPrime/def.h"

// CalculatePlayerAddress Function
uint32_t calculatePlayerAddress(uint32_t baseAddress, uint8_t playerPosition, int32_t increment) {
    // If player position is 0, return the base address without modification
    if (playerPosition == 0) {
        return baseAddress;
    }

    // Calculate using 64-bit integers to prevent overflow
    // Use playerPosition as is (no subtraction)
    int64_t result = static_cast<int64_t>(baseAddress) + (static_cast<int64_t>(playerPosition) * increment);

    // Ensure the result is within the 32-bit range
    if (result < 0 || result > UINT32_MAX) {
        return baseAddress;  // Return the original address if out of range
    }

    return static_cast<uint32_t>(result);
}

melonDS::u32 baseIsAltFormAddr;
melonDS::u32 baseLoadedSpecialWeaponAddr;
melonDS::u32 baseWeaponChangeAddr;
melonDS::u32 baseSelectedWeaponAddr;
melonDS::u32 baseChosenHunterAddr;
melonDS::u32 baseJumpFlagAddr;
melonDS::u32 inGameAddr;
melonDS::u32 PlayerPosAddr;
melonDS::u32 inVisorOrMapAddr;
melonDS::u32 baseAimXAddr;
melonDS::u32 baseAimYAddr;
melonDS::u32 aimXAddr;
melonDS::u32 aimYAddr;
melonDS::u32 isInAdventureAddr;
melonDS::u32 isMapOrUserActionPausedAddr; // for issue in AdventureMode, Aim Stopping when SwitchingWeapon. 

bool isAltForm;


void detectRomAndSetAddresses(EmuInstance* emuInstance) {


    switch (globalChecksum) {
    case RomVersions::US1_1:
        // USA1.1

        baseChosenHunterAddr = 0x020CBDA4; // BattleConfig:ChosenHunter 0 samus 1 kanden 2 trace 3 sylux 4 noxus 5 spire 6 weavel
        inGameAddr = 0x020eec40 + 0x8F0; // inGame:1
        inVisorOrMapAddr = 0x020D9A7D; // Estimated address
        PlayerPosAddr = 0x020DA538;
        baseIsAltFormAddr = 0x020DB098; // 1p(host)
        baseLoadedSpecialWeaponAddr = baseIsAltFormAddr + 0x56; // 1p(host). For special weapons only. Missile and powerBeam are not special weapon.
        baseWeaponChangeAddr = 0x020DB45B; // 1p(host)
        baseSelectedWeaponAddr = 0x020DB463; // 1p(host)
        baseJumpFlagAddr = baseSelectedWeaponAddr - 0xA;
        baseAimXAddr = 0x020DEDA6;
        baseAimYAddr = 0x020DEDAE;
        isInAdventureAddr = 0x020E83BC; // Read8 0x02: ADV, 0x03: Multi
        isMapOrUserActionPausedAddr = 0x020FBF18; // 0x00000001: true, 0x00000000 false. Read8 is enough though.
        isRomDetected = true;
        emuInstance->osdAddMessage(0, "MPH Rom version detected: US1.1");

        break;

    case RomVersions::US1_0:
        // USA1.0
        baseChosenHunterAddr = 0x020CB51C; // BattleConfig:ChosenHunter
        inGameAddr = 0x020ee180 + 0x8F0; // inGame:1
        PlayerPosAddr = 0x020D9CB8;
        inVisorOrMapAddr = PlayerPosAddr - 0xabb; // Estimated address
        baseIsAltFormAddr = 0x020DC6D8 - 0x1EC0; // 1p(host)
        baseLoadedSpecialWeaponAddr = baseIsAltFormAddr + 0x56; // 1p(host). For special weapons only. Missile and powerBeam are not special weapon.
        baseWeaponChangeAddr = 0x020DCA9B - 0x1EC0; // 1p(host)
        baseSelectedWeaponAddr = 0x020DCAA3 - 0x1EC0; // 1p(host)
        baseJumpFlagAddr = baseSelectedWeaponAddr - 0xA;
        baseAimXAddr = 0x020de526;
        baseAimYAddr = 0x020de52E;
        isInAdventureAddr = 0x020E78FC; // Read8 0x02: ADV, 0x03: Multi
        isMapOrUserActionPausedAddr = 0x020FB458; // 0x00000001: true, 0x00000000 false. Read8 is enough though.
        isRomDetected = true;
        emuInstance->osdAddMessage(0, "MPH Rom version detected: US1.0");

        break;

    case RomVersions::JP1_0:
        // Japan1.0
        baseChosenHunterAddr = 0x020CD358; // BattleConfig:ChosenHunter
        inGameAddr = 0x020F0BB0; // inGame:1
        PlayerPosAddr = 0x020DBB78;
        inVisorOrMapAddr = PlayerPosAddr - 0xabb; // Estimated address
        baseIsAltFormAddr = 0x020DC6D8; // 1p(host)
        baseLoadedSpecialWeaponAddr = baseIsAltFormAddr + 0x56; // 1p(host). For special weapons only. Missile and powerBeam are not special weapon.
        baseWeaponChangeAddr = 0x020DCA9B; // 1p(host)
        baseSelectedWeaponAddr = 0x020DCAA3; // 1p(host)
        baseJumpFlagAddr = baseSelectedWeaponAddr - 0xA;
        baseAimXAddr = 0x020E03E6;
        baseAimYAddr = 0x020E03EE;
        isInAdventureAddr = 0x020E9A3C; // Read8 0x02: ADV, 0x03: Multi
        isMapOrUserActionPausedAddr = 0x020FD598; // 0x00000001: true, 0x00000000 false. Read8 is enough though.
        isRomDetected = true;
        emuInstance->osdAddMessage(0, "MPH Rom version detected: JP1.0");

        break;

    case RomVersions::JP1_1:
        // Japan1.1
        baseChosenHunterAddr = 0x020CD318; // BattleConfig:ChosenHunter
        inGameAddr = 0x020F0280 + 0x8F0; // inGame:1
        PlayerPosAddr = 0x020DBB38;
        inVisorOrMapAddr = PlayerPosAddr - 0xabb; // Estimated address
        baseIsAltFormAddr = 0x020DC6D8 - 0x64; // 1p(host)
        baseLoadedSpecialWeaponAddr = baseIsAltFormAddr + 0x56; // 1p(host). For special weapons only. Missile and powerBeam are not special weapon.
        baseWeaponChangeAddr = 0x020DCA9B - 0x40; // 1p(host)
        baseSelectedWeaponAddr = 0x020DCAA3 - 0x40; // 1p(host)
        baseJumpFlagAddr = baseSelectedWeaponAddr - 0xA;
        baseAimXAddr = 0x020e03a6;
        baseAimYAddr = 0x020e03ae;
        isInAdventureAddr = 0x020E99FC; // Read8 0x02: ADV, 0x03: Multi
        isMapOrUserActionPausedAddr = 0x020FD558; // 0x00000001: true, 0x00000000 false. Read8 is enough though.
        isRomDetected = true;
        emuInstance->osdAddMessage(0, "MPH Rom version detected: JP1.1");

        break;

    case RomVersions::EU1_0:
        // EU1.0
        baseChosenHunterAddr = 0x020CBDC4; // BattleConfig:ChosenHunter
        inGameAddr = 0x020eec60 + 0x8F0; // inGame:1
        PlayerPosAddr = 0x020DA558;
        inVisorOrMapAddr = PlayerPosAddr - 0xabb; // Estimated address
        baseIsAltFormAddr = 0x020DC6D8 - 0x1620; // 1p(host)
        baseLoadedSpecialWeaponAddr = baseIsAltFormAddr + 0x56; // 1p(host). For special weapons only. Missile and powerBeam are not special weapon.
        baseWeaponChangeAddr = 0x020DCA9B - 0x1620; // 1p(host)
        baseSelectedWeaponAddr = 0x020DCAA3 - 0x1620; // 1p(host)
        baseJumpFlagAddr = baseSelectedWeaponAddr - 0xA;
        baseAimXAddr = 0x020dedc6;
        baseAimYAddr = 0x020dedcE;
        isInAdventureAddr = 0x020E83DC; // Read8 0x02: ADV, 0x03: Multi
        isMapOrUserActionPausedAddr = 0x020FBF38; // 0x00000001: true, 0x00000000 false. Read8 is enough though.
        isRomDetected = true;
        emuInstance->osdAddMessage(0, "MPH Rom version detected: EU1.0");

        break;

    case RomVersions::EU1_1:
        // EU1.1
        baseChosenHunterAddr = 0x020CBE44; // BattleConfig:ChosenHunter
        inGameAddr = 0x020eece0 + 0x8F0; // inGame:1
        PlayerPosAddr = 0x020DA5D8;
        inVisorOrMapAddr = PlayerPosAddr - 0xabb; // Estimated address
        baseIsAltFormAddr = 0x020DC6D8 - 0x15A0; // 1p(host)
        baseLoadedSpecialWeaponAddr = baseIsAltFormAddr + 0x56; // 1p(host). For special weapons only. Missile and powerBeam are not special weapon.
        baseWeaponChangeAddr = 0x020DCA9B - 0x15A0; // 1p(host)
        baseSelectedWeaponAddr = 0x020DCAA3 - 0x15A0; // 1p(host)
        baseJumpFlagAddr = baseSelectedWeaponAddr - 0xA;
        baseAimXAddr = 0x020dee46;
        baseAimYAddr = 0x020dee4e;
        isInAdventureAddr = 0x020E845C; // Read8 0x02: ADV, 0x03: Multi
        isMapOrUserActionPausedAddr = 0x020FBFB8; // 0x00000001: true, 0x00000000 false. Read8 is enough though.
        emuInstance->osdAddMessage(0, "MPH Rom version detected: EU1.1");

        isRomDetected = true;

        break;

    case RomVersions::KR1_0:
        // Korea1.0
        baseChosenHunterAddr = 0x020C4B88; // BattleConfig:ChosenHunter
        inGameAddr = 0x020E81B4; // inGame:1
        inVisorOrMapAddr = PlayerPosAddr - 0xabb; // Estimated address
        PlayerPosAddr = 0x020D33A9; // it's weird but "3A9" is correct.
        baseIsAltFormAddr = 0x020DC6D8 - 0x87F4; // 1p(host)
        baseLoadedSpecialWeaponAddr = baseIsAltFormAddr + 0x56; // 1p(host). For special weapons only. Missile and powerBeam are not special weapon.
        baseWeaponChangeAddr = 0x020DCA9B - 0x87F4; // 1p(host)
        baseSelectedWeaponAddr = 0x020DCAA3 - 0x87F4; // 1p(host)
        baseJumpFlagAddr = baseSelectedWeaponAddr - 0xA;
        baseAimXAddr = 0x020D7C0E;
        baseAimYAddr = 0x020D7C16;
        isInAdventureAddr = 0x020E11F8; // Read8 0x02: ADV, 0x03: Multi
        isMapOrUserActionPausedAddr = 0x020F4CF8; // 0x00000001: true, 0x00000000 false. Read8 is enough though.
        emuInstance->osdAddMessage(0, "MPH Rom version detected: KR1.0");

        isRomDetected = true;

        break;

    default:
        // Handle unsupported checksums.
        // Add default behavior or error handling.
        break;
    }
}






































/* MelonPrimeDS function goes here */
void EmuThread::run()
{
    Config::Table& globalCfg = emuInstance->getGlobalConfig();
    Config::Table& localCfg = emuInstance->getLocalConfig();
    u32 mainScreenPos[3];

    //emuInstance->updateConsole();
    // No carts are inserted when melonDS first boots

    mainScreenPos[0] = 0;
    mainScreenPos[1] = 0;
    mainScreenPos[2] = 0;
    autoScreenSizing = 0;

    //videoSettingsDirty = false;

    if (emuInstance->usesOpenGL())
    {
        emuInstance->initOpenGL(0);

        useOpenGL = true;
        videoRenderer = globalCfg.GetInt("3D.Renderer");
    }
    else
    {
        useOpenGL = false;
        videoRenderer = 0;
    }

    //updateRenderer();
    videoSettingsDirty = true;

    u32 nframes = 0;
    double perfCountsSec = 1.0 / SDL_GetPerformanceFrequency();
    double lastTime = SDL_GetPerformanceCounter() * perfCountsSec;
    double frameLimitError = 0.0;
    double lastMeasureTime = lastTime;

    u32 winUpdateCount = 0, winUpdateFreq = 1;
    u8 dsiVolumeLevel = 0x1F;

    char melontitle[100];

    bool fastforward = false;
    bool slowmo = false;
    emuInstance->fastForwardToggled = false;
    emuInstance->slowmoToggled = false;


    auto frameAdvanceOnce{
    [&]()
//    while (emuStatus != emuStatus_Exit)
    {
        MPInterface::Get().Process();
        emuInstance->inputProcess();

        if (emuInstance->hotkeyPressed(HK_FrameLimitToggle)) emit windowLimitFPSChange();

        if (emuInstance->hotkeyPressed(HK_Pause)) emuTogglePause();
        if (emuInstance->hotkeyPressed(HK_Reset)) emuReset();
        if (emuInstance->hotkeyPressed(HK_FrameStep)) emuFrameStep();

        if (emuInstance->hotkeyPressed(HK_FullscreenToggle)) emit windowFullscreenToggle();

        if (emuInstance->hotkeyPressed(HK_SwapScreens)) emit swapScreensToggle();
        if (emuInstance->hotkeyPressed(HK_SwapScreenEmphasis)) emit screenEmphasisToggle();

        // Lambda to update aim sensitivity and display a message
        auto updateAimSensitivity = [&](int change) {

            // Store the current sensitivity in a local variable
            int currentSensitivity = localCfg.GetInt("Metroid.Sensitivity.Aim");

            // Calculate the new sensitivity
            int newSensitivity = currentSensitivity + change;

            // Check if the new sensitivity is at least 1
            if (newSensitivity >= 1) {
                // Update the config only if the value has changed
                if (newSensitivity != currentSensitivity) {
                    localCfg.SetInt("Metroid.Sensitivity.Aim", newSensitivity);
                    // Save the changes to the configuration file (to persist settings for future sessions)
                    Config::Save();
                }
                // Create and display the OSD message
                emuInstance->osdAddMessage(0, ("AimSensi Updated: " + std::to_string(newSensitivity)).c_str());
            }
            else {
                // Display a message when trying to decrease below 1
                emuInstance->osdAddMessage(0, "AimSensi cannot be decreased below 1");
            }
            };

        // Sensitivity UP
        if (emuInstance->hotkeyReleased(HK_MetroidIngameSensiUp)) {
            updateAimSensitivity(1);  // Increase sensitivity by 1
        }

        // Sensitivity DOWN
        if (emuInstance->hotkeyReleased(HK_MetroidIngameSensiDown)) {
            updateAimSensitivity(-1);  // Decrease sensitivity by 1
        }

        if (emuStatus == emuStatus_Running || emuStatus == emuStatus_FrameStep)
        {
            if (emuStatus == emuStatus_FrameStep) emuStatus = emuStatus_Paused;

            /*
            if (emuInstance->hotkeyPressed(HK_SolarSensorDecrease))
            {
                int level = emuInstance->nds->GBACartSlot.SetInput(GBACart::Input_SolarSensorDown, true);
                if (level != -1)
                {
                    emuInstance->osdAddMessage(0, "Solar sensor level: %d", level);
                }
            }
            if (emuInstance->hotkeyPressed(HK_SolarSensorIncrease))
            {
                int level = emuInstance->nds->GBACartSlot.SetInput(GBACart::Input_SolarSensorUp, true);
                if (level != -1)
                {
                    emuInstance->osdAddMessage(0, "Solar sensor level: %d", level);
                }
            }
            */

            auto handleDSiInputs = [](EmuInstance* emuInstance, double perfCountsSec) {
                if (emuInstance->nds->ConsoleType == 1)
                {
                    DSi* dsi = static_cast<DSi*>(emuInstance->nds);
                    double currentTime = SDL_GetPerformanceCounter() * perfCountsSec;

                    // Handle power button
                    if (emuInstance->hotkeyDown(HK_PowerButton))
                    {
                        dsi->I2C.GetBPTWL()->SetPowerButtonHeld(currentTime);
                    }
                    else if (emuInstance->hotkeyReleased(HK_PowerButton))
                    {
                        dsi->I2C.GetBPTWL()->SetPowerButtonReleased(currentTime);
                    }

                    // Handle volume buttons
                    if (emuInstance->hotkeyDown(HK_VolumeUp))
                    {
                        dsi->I2C.GetBPTWL()->SetVolumeSwitchHeld(DSi_BPTWL::volumeKey_Up);
                    }
                    else if (emuInstance->hotkeyReleased(HK_VolumeUp))
                    {
                        dsi->I2C.GetBPTWL()->SetVolumeSwitchReleased(DSi_BPTWL::volumeKey_Up);
                    }

                    if (emuInstance->hotkeyDown(HK_VolumeDown))
                    {
                        dsi->I2C.GetBPTWL()->SetVolumeSwitchHeld(DSi_BPTWL::volumeKey_Down);
                    }
                    else if (emuInstance->hotkeyReleased(HK_VolumeDown))
                    {
                        dsi->I2C.GetBPTWL()->SetVolumeSwitchReleased(DSi_BPTWL::volumeKey_Down);
                    }

                    dsi->I2C.GetBPTWL()->ProcessVolumeSwitchInput(currentTime);
                }
                };

            handleDSiInputs(emuInstance, perfCountsSec);

            if (useOpenGL)
                emuInstance->makeCurrentGL();

            // update render settings if needed
            if (videoSettingsDirty)
            {
                emuInstance->renderLock.lock();
                if (useOpenGL)
                {
                    emuInstance->setVSyncGL(true);
                    videoRenderer = globalCfg.GetInt("3D.Renderer");
                }
#ifdef OGLRENDERER_ENABLED
                else
#endif
                {
                    videoRenderer = 0;
                }

                updateRenderer();

                videoSettingsDirty = false;
                emuInstance->renderLock.unlock();
            }

            /* MelonPrimeDS comment-outed
            // process input and hotkeys
            emuInstance->nds->SetKeyMask(emuInstance->inputMask);
            */


            /* MelonPrimeDS comment-outed
            if (emuInstance->hotkeyPressed(HK_Lid))
            {
                bool lid = !emuInstance->nds->IsLidClosed();
                emuInstance->nds->SetLidClosed(lid);
                emuInstance->osdAddMessage(0, lid ? "Lid closed" : "Lid opened");
            }
            */

            // microphone input
            emuInstance->micProcess();

            // auto screen layout
            {
                mainScreenPos[2] = mainScreenPos[1];
                mainScreenPos[1] = mainScreenPos[0];
                mainScreenPos[0] = emuInstance->nds->PowerControl9 >> 15;

                int guess;
                if (mainScreenPos[0] == mainScreenPos[2] &&
                    mainScreenPos[0] != mainScreenPos[1])
                {
                    // constant flickering, likely displaying 3D on both screens
                    // TODO: when both screens are used for 2D only...???
                    guess = screenSizing_Even;
                }
                else
                {
                    if (mainScreenPos[0] == 1)
                        guess = screenSizing_EmphTop;
                    else
                        guess = screenSizing_EmphBot;
                }

                if (guess != autoScreenSizing)
                {
                    autoScreenSizing = guess;
                    emit autoScreenSizingChange(autoScreenSizing);
                }
            }


            // emulate
            u32 nlines;
            if (emuInstance->nds->GPU.GetRenderer3D().NeedsShaderCompile())
            {
                compileShaders();
                nlines = 1;
            }
            else
            {
                nlines = emuInstance->nds->RunFrame();
            }

            if (emuInstance->ndsSave)
                emuInstance->ndsSave->CheckFlush();

            if (emuInstance->gbaSave)
                emuInstance->gbaSave->CheckFlush();

            if (emuInstance->firmwareSave)
                emuInstance->firmwareSave->CheckFlush();

            if (!useOpenGL)
            {
                frontBufferLock.lock();
                frontBuffer = emuInstance->nds->GPU.FrontBuffer;
                frontBufferLock.unlock();
            }
            else
            {
                frontBuffer = emuInstance->nds->GPU.FrontBuffer;
                emuInstance->drawScreenGL();
            }

#ifdef MELONCAP
            MelonCap::Update();
#endif // MELONCAP

            winUpdateCount++;
            if (winUpdateCount >= winUpdateFreq && !useOpenGL)
            {
                emit windowUpdate();
                winUpdateCount = 0;
            }
            
            if (emuInstance->hotkeyPressed(HK_FastForwardToggle)) emuInstance->fastForwardToggled = !emuInstance->fastForwardToggled;
            if (emuInstance->hotkeyPressed(HK_SlowMoToggle)) emuInstance->slowmoToggled = !emuInstance->slowmoToggled;

            bool enablefastforward = emuInstance->hotkeyDown(HK_FastForward) | emuInstance->fastForwardToggled;
            bool enableslowmo = emuInstance->hotkeyDown(HK_SlowMo) | emuInstance->slowmoToggled;

            if (useOpenGL)
            {
                // when using OpenGL: when toggling fast-forward or slowmo, change the vsync interval
                if ((enablefastforward || enableslowmo) && !(fastforward || slowmo))
                {
                    emuInstance->setVSyncGL(false);
                }
                else if (!(enablefastforward || enableslowmo) && (fastforward || slowmo))
                {
                    emuInstance->setVSyncGL(true);
                }
            }

            fastforward = enablefastforward;
            slowmo = enableslowmo;

            if (slowmo) emuInstance->curFPS = emuInstance->slowmoFPS;
            else if (fastforward) emuInstance->curFPS = emuInstance->fastForwardFPS;
            else if (!emuInstance->doLimitFPS) emuInstance->curFPS = 1000.0;
            else emuInstance->curFPS = emuInstance->targetFPS;

            if (emuInstance->audioDSiVolumeSync && emuInstance->nds->ConsoleType == 1)
            {
                DSi* dsi = static_cast<DSi*>(emuInstance->nds);
                u8 volumeLevel = dsi->I2C.GetBPTWL()->GetVolumeLevel();
                if (volumeLevel != dsiVolumeLevel)
                {
                    dsiVolumeLevel = volumeLevel;
                    emit syncVolumeLevel();
                }

                emuInstance->audioVolume = volumeLevel * (256.0 / 31.0);
            }

            if (emuInstance->doAudioSync && !(fastforward || slowmo))
                emuInstance->audioSync();

            double frametimeStep = nlines / (emuInstance->curFPS * 263.0);

            if (frametimeStep < 0.001) frametimeStep = 0.001;

            {
                double curtime = SDL_GetPerformanceCounter() * perfCountsSec;

                frameLimitError += frametimeStep - (curtime - lastTime);
                if (frameLimitError < -frametimeStep)
                    frameLimitError = -frametimeStep;
                if (frameLimitError > frametimeStep)
                    frameLimitError = frametimeStep;

                if (round(frameLimitError * 1000.0) > 0.0)
                {
                    SDL_Delay(round(frameLimitError * 1000.0));
                    double timeBeforeSleep = curtime;
                    curtime = SDL_GetPerformanceCounter() * perfCountsSec;
                    frameLimitError -= curtime - timeBeforeSleep;
                }

                lastTime = curtime;
            }

            nframes++;
            if (nframes >= 30)
            {
                double time = SDL_GetPerformanceCounter() * perfCountsSec;
                double dt = time - lastMeasureTime;
                lastMeasureTime = time;

                u32 fps = round(nframes / dt);
                nframes = 0;

                float fpstarget = 1.0/frametimeStep;

                winUpdateFreq = fps / (u32)round(fpstarget);
                if (winUpdateFreq < 1)
                    winUpdateFreq = 1;
                    
                double actualfps = (59.8261 * 263.0) / nlines;
                int inst = emuInstance->instanceID;
                if (inst == 0)
                    sprintf(melontitle, "[%d/%.0f] melonDS " MELONDS_VERSION, fps, actualfps);
                else
                    sprintf(melontitle, "[%d/%.0f] melonDS (%d)", fps, fpstarget, inst+1);
                changeWindowTitle(melontitle);
            }
        }
        else
        {
            // paused
            nframes = 0;
            lastTime = SDL_GetPerformanceCounter() * perfCountsSec;
            lastMeasureTime = lastTime;

            emit windowUpdate();

            int inst = emuInstance->instanceID;
            if (inst == 0)
                sprintf(melontitle, "melonDS " MELONDS_VERSION);
            else
                sprintf(melontitle, "melonDS (%d)", inst+1);
            changeWindowTitle(melontitle);

            SDL_Delay(75);

            if (useOpenGL)
            {
                emuInstance->drawScreenGL();
            }
        }

        handleMessages();
    }
    };


    auto frameAdvance{
    [&](int n)
    {
        for (int i = 0; i < n; i++) frameAdvanceOnce();
    }
    };



    // melonPrimeDS

    bool isCursorVisible = true;
    bool enableAim = true;
    bool wasLastFrameFocused = false;

    auto showCursorOnMelonPrimeDS = [&](bool show) {
        if (show == isCursorVisible) return;

        auto* panel = emuInstance->getMainWindow()->panel;
        if (!panel) return;

        QMetaObject::invokeMethod(panel,
            [panel, show]() {
                panel->setCursor(show ? Qt::ArrowCursor : Qt::BlankCursor);
            },
            Qt::ConnectionType::QueuedConnection
        );

        isCursorVisible = show;
        };


// #define STYLUS_MODE 1 // this is for stylus user

#define INPUT_A 0
#define INPUT_B 1
#define INPUT_SELECT 2
#define INPUT_START 3
#define INPUT_RIGHT 4
#define INPUT_LEFT 5
#define INPUT_UP 6
#define INPUT_DOWN 7
#define INPUT_R 8
#define INPUT_L 9
#define INPUT_X 10
#define INPUT_Y 11
#define FN_INPUT_PRESS(i) emuInstance->inputMask.setBit(i, false);;
#define FN_INPUT_RELEASE(i) emuInstance->inputMask.setBit(i, true);

    uint8_t playerPosition;
    const uint16_t playerAddressIncrement = 0xF30;
    const uint8_t aimAddrIncrement = 0x48;
    uint32_t isAltFormAddr;
    uint32_t loadedSpecialWeaponAddr;
    uint32_t chosenHunterAddr;
    uint32_t weaponChangeAddr;
    uint32_t selectedWeaponAddr;
    uint32_t jumpFlagAddr;

    uint32_t havingWeaponsAddr;
    uint32_t currentWeaponAddr;

    uint32_t boostGaugeAddr;
    uint32_t isBoostingAddr;

    uint32_t weaponAmmoAddr;

    // uint32_t isPrimeHunterAddr;


    bool isRoundJustStarted;
    bool isInAdventure;
    bool isSamus;

    bool isWeavel;


    // The QPoint class defines a point in the plane using integer precision. 
    // auto mouseRel = rawInputThread->fetchMouseDelta();
    QPoint mouseRel;

    // Initialize Adjusted Center 
    QPoint adjustedCenter;


    // test
    // Lambda function to get adjusted center position based on window geometry and screen layout
    auto getAdjustedCenter = [&]() {
        auto& windowCfg = emuInstance->getMainWindow()->getWindowConfig();

        // Get the actual game display area instead of full window
        const QRect displayRect = emuInstance->getMainWindow()->panel->geometry();
        QPoint adjustedCenter = emuInstance->getMainWindow()->panel->mapToGlobal(
            QPoint(displayRect.width() / 2, displayRect.height() / 2)
        );

        // Screen layout adjustment constants
        constexpr float DEFAULT_ADJUSTMENT = 0.25f;
        constexpr float HYBRID_RIGHT = 0.333203125f;  // (2133-1280)/2560
        constexpr float HYBRID_LEFT = 0.166796875f;   // (1280-853)/2560

        // Inner lambda for getting screen-specific adjustment factors
        auto getScreenAdjustment = [&](bool isFullscreen) {
            struct ScreenAdjustment {
                float x;
                float y;
            };

            // Base adjustment values
            static const std::map<int, ScreenAdjustment> layoutAdjustments = {
                {screenLayout_Natural,    {0.0f,  DEFAULT_ADJUSTMENT}},
                {screenLayout_Horizontal,   {DEFAULT_ADJUSTMENT, 0.0f}},
                {screenLayout_Vertical, {0.0f,  DEFAULT_ADJUSTMENT}},
                {screenLayout_Hybrid,     {HYBRID_LEFT, DEFAULT_ADJUSTMENT}}
            };

            // Get base adjustment for current layout
            ScreenAdjustment adj = layoutAdjustments.at(windowCfg.GetInt("ScreenLayout"));

            return adj;
            };

        // Get current state
        bool isFullscreen = emuInstance->getMainWindow()->isFullScreen();
        bool isSwapped = windowCfg.GetBool("ScreenSwap");
        int screenSizing = windowCfg.GetInt("ScreenSizing");

        // Get base adjustment values
        auto adj = getScreenAdjustment(isFullscreen);

        // Handle special cases first
        if (screenSizing == screenSizing_BotOnly) {
            if (isFullscreen) {
                adjustedCenter.rx() -= static_cast<int>(displayRect.width() * 0.4f);
                adjustedCenter.ry() -= static_cast<int>(displayRect.height() * 0.4f);
            }
            return adjustedCenter;
        }

        if (screenSizing == screenSizing_TopOnly) {
            return adjustedCenter;
        }

        // Apply layout-specific adjustments
        switch (windowCfg.GetInt("ScreenLayout")) {
        case screenLayout_Hybrid:
            if (isSwapped) {
                adjustedCenter.rx() += static_cast<int>(displayRect.width() * HYBRID_RIGHT);
                adjustedCenter.ry() -= static_cast<int>(displayRect.height() * adj.y);
            }
            else {
                adjustedCenter.rx() -= static_cast<int>(displayRect.width() * adj.x);
            }
            break;

        case screenLayout_Natural:
        case screenLayout_Vertical:
            adjustedCenter.ry() += static_cast<int>(displayRect.height() * adj.y * (isSwapped ? 1.0f : -1.0f));
            break;
        case screenLayout_Horizontal:
            adjustedCenter.rx() += static_cast<int>(displayRect.width() * adj.x * (isSwapped ? 1.0f : -1.0f));
            break;
        }

        return adjustedCenter;
        };

    // Get adjusted center position
    adjustedCenter = getAdjustedCenter();



    //const float dsAspectRatio = 4.0 / 3.0;
    const float dsAspectRatio = 1.333333333f;
    //const float aimAspectRatio = 6.0 / 4.0; // i have no idea
    const float aimAspectRatio = 1.5f; // i have no idea  6.0 / 4.0


    // processMoveInputFunction{

    // State variables for SnapTap mode
    static uint32_t lastInputBitmap = 0;
    static uint32_t priorityInput = 0;

    auto processMoveInput = [&]() {
        // Pack all input flags into a single 32-bit register for SIMD-like processing
        static constexpr uint32_t INPUT_PACKED_UP = (1u << 0) | (uint32_t(INPUT_UP) << 16);
        static constexpr uint32_t INPUT_PACKED_DOWN = (1u << 1) | (uint32_t(INPUT_DOWN) << 16);
        static constexpr uint32_t INPUT_PACKED_LEFT = (1u << 2) | (uint32_t(INPUT_LEFT) << 16);
        static constexpr uint32_t INPUT_PACKED_RIGHT = (1u << 3) | (uint32_t(INPUT_RIGHT) << 16);

        // Get current input state
        uint32_t currentInputBitmap =
            (uint32_t(emuInstance->hotkeyDown(HK_MetroidMoveForward)) << 0) |
            (uint32_t(emuInstance->hotkeyDown(HK_MetroidMoveBack)) << 1) |
            (uint32_t(emuInstance->hotkeyDown(HK_MetroidMoveLeft)) << 2) |
            (uint32_t(emuInstance->hotkeyDown(HK_MetroidMoveRight)) << 3);

        if (localCfg.GetBool("Metroid.Operation.SnapTap")) {
            // Detect newly pressed keys
            uint32_t newlyPressed = currentInputBitmap & ~lastInputBitmap;

            // Check for directional conflicts
            bool horizontalConflict = (currentInputBitmap & ((1u << 2) | (1u << 3))) == ((1u << 2) | (1u << 3));  // Left & Right
            bool verticalConflict = (currentInputBitmap & ((1u << 0) | (1u << 1))) == ((1u << 0) | (1u << 1));    // Up & Down

            // Update priority when new keys are pressed
            if (newlyPressed) {
                if (horizontalConflict) {
                    // For horizontal conflict, prioritize the newly pressed key
                    priorityInput &= ~((1u << 2) | (1u << 3));  // Clear horizontal flags
                    priorityInput |= newlyPressed & ((1u << 2) | (1u << 3));
                }
                if (verticalConflict) {
                    // For vertical conflict, prioritize the newly pressed key
                    priorityInput &= ~((1u << 0) | (1u << 1));  // Clear vertical flags
                    priorityInput |= newlyPressed & ((1u << 0) | (1u << 1));
                }
            }

            // Clear priority if the prioritized key is released
            if ((priorityInput & ~currentInputBitmap) != 0) {
                priorityInput &= currentInputBitmap;
            }

            // Determine final input based on priorities
            uint32_t finalInputBitmap = currentInputBitmap;
            if (horizontalConflict) {
                finalInputBitmap &= ~((1u << 2) | (1u << 3));  // Clear horizontal inputs
                finalInputBitmap |= priorityInput & ((1u << 2) | (1u << 3));
            }
            if (verticalConflict) {
                finalInputBitmap &= ~((1u << 0) | (1u << 1));  // Clear vertical inputs
                finalInputBitmap |= priorityInput & ((1u << 0) | (1u << 1));
            }

            // Use LUT to get final state
            static constexpr uint32_t PACKED_LUT[16] = {
                0x00000000u,
                INPUT_PACKED_UP,
                INPUT_PACKED_DOWN,
                0x00000000u,
                INPUT_PACKED_LEFT,
                INPUT_PACKED_UP | INPUT_PACKED_LEFT,
                INPUT_PACKED_DOWN | INPUT_PACKED_LEFT,
                INPUT_PACKED_LEFT,
                INPUT_PACKED_RIGHT,
                INPUT_PACKED_UP | INPUT_PACKED_RIGHT,
                INPUT_PACKED_DOWN | INPUT_PACKED_RIGHT,
                INPUT_PACKED_RIGHT,
                0x00000000u,
                INPUT_PACKED_UP,
                INPUT_PACKED_DOWN,
                0x00000000u
            };

            uint32_t finalState = PACKED_LUT[finalInputBitmap & 0xF];

            // Apply inputs
            static const auto applyInput = [&](uint32_t packedInput, uint32_t state) {
                if (state & packedInput & 0xF) {
                    FN_INPUT_PRESS(packedInput >> 16);
                }
                else {
                    FN_INPUT_RELEASE(packedInput >> 16);
                }
                };

            static constexpr uint32_t ALL_INPUTS[] = {
                INPUT_PACKED_UP, INPUT_PACKED_DOWN,
                INPUT_PACKED_LEFT, INPUT_PACKED_RIGHT
            };

#pragma unroll
            for (const auto& input : ALL_INPUTS) {
                applyInput(input, finalState);
            }

            // Store current input state for next frame
            lastInputBitmap = currentInputBitmap;
        }
        else {
            // Normal mode processing
            static constexpr uint32_t PACKED_LUT[16] = {
                0x00000000u,
                INPUT_PACKED_UP,
                INPUT_PACKED_DOWN,
                0x00000000u,
                INPUT_PACKED_LEFT,
                INPUT_PACKED_UP | INPUT_PACKED_LEFT,
                INPUT_PACKED_DOWN | INPUT_PACKED_LEFT,
                INPUT_PACKED_LEFT,
                INPUT_PACKED_RIGHT,
                INPUT_PACKED_UP | INPUT_PACKED_RIGHT,
                INPUT_PACKED_DOWN | INPUT_PACKED_RIGHT,
                INPUT_PACKED_RIGHT,
                0x00000000u,
                INPUT_PACKED_UP,
                INPUT_PACKED_DOWN,
                0x00000000u
            };

            uint32_t finalState = PACKED_LUT[currentInputBitmap & 0xF];

            static const auto applyInput = [&](uint32_t packedInput, uint32_t state) {
                if (state & packedInput & 0xF) {
                    FN_INPUT_PRESS(packedInput >> 16);
                }
                else {
                    FN_INPUT_RELEASE(packedInput >> 16);
                }
                };

            static constexpr uint32_t ALL_INPUTS[] = {
                INPUT_PACKED_UP, INPUT_PACKED_DOWN,
                INPUT_PACKED_LEFT, INPUT_PACKED_RIGHT
            };

#pragma unroll
            for (const auto& input : ALL_INPUTS) {
                applyInput(input, finalState);
            }
        }
        };
    // /processMoveInputFunction }



    while (emuStatus != emuStatus_Exit) {

        // MelonPrimeDS Functions START

#ifndef STYLUS_MODE
        // Mouse player
        bool isFocused = emuInstance->getMainWindow()->panel->getFocused();
#else
        // isStylus
        bool isFocused = true;
#endif


        if (!isRomDetected) {
            detectRomAndSetAddresses(emuInstance);
        }
        if (isRomDetected) {
            isInGame = emuInstance->nds->ARM9Read16(inGameAddr) == 0x0001;


            if (isInGame && !hasInitialized) {
                // Run once at game start

                // Set the initialization complete flag
                hasInitialized = true;

                // VSync Off
                emuInstance->setVSyncGL(false); // MelonPrimeDS

                // updateRenderer because of using softwareRenderer when not in Game.
                videoRenderer = emuInstance->getGlobalConfig().GetInt("3D.Renderer");
                updateRenderer();

                // VSync Off
                emuInstance->setVSyncGL(false); // MelonPrimeDS



                // Hide cursor
#ifndef STYLUS_MODE
                showCursorOnMelonPrimeDS(false);
#endif

                // Read the player position
                playerPosition = emuInstance->nds->ARM9Read8(PlayerPosAddr);

                // get addresses
                isAltFormAddr = calculatePlayerAddress(baseIsAltFormAddr, playerPosition, playerAddressIncrement);
                loadedSpecialWeaponAddr = calculatePlayerAddress(baseLoadedSpecialWeaponAddr, playerPosition, playerAddressIncrement);
                chosenHunterAddr = calculatePlayerAddress(baseChosenHunterAddr, playerPosition, 0x01);
                weaponChangeAddr = calculatePlayerAddress(baseWeaponChangeAddr, playerPosition, playerAddressIncrement);

                selectedWeaponAddr = calculatePlayerAddress(baseSelectedWeaponAddr, playerPosition, playerAddressIncrement); // 020DCAA3 in JP1.0
                currentWeaponAddr = selectedWeaponAddr - 0x1; // 020DCAA2 in JP1.0
                havingWeaponsAddr = selectedWeaponAddr + 0x3; // 020DCAA6 in JP1.0

                weaponAmmoAddr = selectedWeaponAddr - 0x383; // 020D720 in JP1.0 current weapon ammo. DC722 is for MissleAmmo. can read both with read32.
                jumpFlagAddr = calculatePlayerAddress(baseJumpFlagAddr, playerPosition, playerAddressIncrement);

                // getChosenHunterAddr
                chosenHunterAddr = calculatePlayerAddress(baseChosenHunterAddr, playerPosition, 0x01);
                uint8_t hunterID = emuInstance->nds->ARM9Read8(chosenHunterAddr); // Perform memory read only once
                isSamus = hunterID == 0x00;
                isWeavel = hunterID == 0x06;

                /*
                Hunter IDs:
                00 - Samus
                01 - Kanden
                02 - Trace
                03 - Sylux
                04 - Noxus
                05 - Spire
                06 - Weavel
                */

                boostGaugeAddr = isAltFormAddr + 0x44;
                isBoostingAddr = isAltFormAddr + 0x46;

                // aim addresses
                aimXAddr = calculatePlayerAddress(baseAimXAddr, playerPosition, aimAddrIncrement);
                aimYAddr = calculatePlayerAddress(baseAimYAddr, playerPosition, aimAddrIncrement);


                isInAdventure = emuInstance->nds->ARM9Read8(isInAdventureAddr) == 0x02;

                // isPrimeHunterAddr = isInAdventureAddr + 0xAD; // isPrimeHunter Addr NotPrimeHunter:0xFF, PrimeHunter:0x00 220E9AE9 in JP1.0

                // emuInstance->osdAddMessage(0, "Completed address calculation.");
            }

            if (isFocused) {


                // Calculate for aim 
                // updateMouseRelativeAndRecenterCursor
                // 
                // Handle the case when the window is focused
                // Update mouse relative position and recenter cursor for aim control

                if (isInGame) {
                    // inGame

                    /*
                    * doing this in Screen.cpp
                    if(!wasLastFrameFocused){
                        showCursorOnMelonPrimeDS(false);
                    }
                    */

#ifndef STYLUS_MODE
                    // processAimInput

                    // Check hotkey status
                    bool isLayoutChanging = emuInstance->hotkeyPressed(HK_SwapScreens) || emuInstance->hotkeyPressed(HK_FullscreenToggle);

                    // These conditional branches cannot be simplified to a simple else statement
                    // because they handle different independent cases:
                    // 1. Recalculating center position when focus is gained or layout is changing
                    // 2. Updating relative position only when focused and layout is not changing

                    // Recalculate center position when focus is gained or layout is changing
                    if (!wasLastFrameFocused || isLayoutChanging) {
                        // emuInstance->osdAddMessage(0, "adjust change needed"); // TODO DELETE THIS
                        adjustedCenter = getAdjustedCenter();// emuInstance->getMainWindow()
                    }

                    // Update relative position only when not changing layout
                    if (wasLastFrameFocused && !isLayoutChanging) {
                        mouseRel = QCursor::pos() - adjustedCenter;
                    }
                    else {
                        mouseRel = QPoint(0, 0);  // Initialize to origin
                    }

                    // Recenter cursor
                    QCursor::setPos(adjustedCenter);


                    // Aiming

                    // Lambda function to adjust scaled mouse input
                    auto adjustMouseInput = [](float value) {
                        // For positive values between 0.5 and 1, set to 1
                        if (value >= 0.5f && value < 1.0f) {
                            return 1.0f;
                        }
                        // For negative values between -0.5 and -1, set to -1
                        else if (value <= -0.5f && value > -1.0f) {
                            return -1.0f;
                        }
                        // For other values, return as is
                        return value;
                        };

                    // Define sensitivity factor as a constant
                    int currentSensitivity = localCfg.GetInt("Metroid.Sensitivity.Aim");
                    const float SENSITIVITY_FACTOR = currentSensitivity * 0.01f;

                    // Processing for the X-axis
                    float mouseX = mouseRel.x();
                    // We don't use abs() here to preserve the sign of the movement
                    // This allows us to detect and process even very small movements in either direction
                    if (mouseX != 0) {
                        // Scale the mouse X movement
                        float scaledMouseX = mouseX * SENSITIVITY_FACTOR;
                        // Adjust the scaled value to ensure minimal movement is registered
                        scaledMouseX = adjustMouseInput(scaledMouseX);
                        // Convert to 16-bit integer and write the adjusted X value to the NDS memory
                        emuInstance->nds->ARM9Write16(aimXAddr, static_cast<uint16_t>(scaledMouseX));
                        enableAim = true;
                    }

                    // Processing for the Y-axis
                    float mouseY = mouseRel.y();
                    // Again, we avoid using abs() to maintain directional information
                    // This ensures that even slight movements are captured and processed
                    if (mouseY != 0) {
                        // Scale the mouse Y movement and apply aspect ratio correction
                        float scaledMouseY = mouseY * aimAspectRatio * SENSITIVITY_FACTOR;
                        // Adjust the scaled value to ensure minimal movement is registered
                        scaledMouseY = adjustMouseInput(scaledMouseY);
                        // Convert to 16-bit integer and write the adjusted Y value to the NDS memory
                        emuInstance->nds->ARM9Write16(aimYAddr, static_cast<uint16_t>(scaledMouseY));
                        enableAim = true;
                    }
#else
                    if (emuInstance->isTouching) {
                        emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
                    }
                    else {
                        emuInstance->nds->ReleaseScreen();
                    }
#endif

                    // Move hunter
                    processMoveInput();

                    // Shoot
                    if (emuInstance->hotkeyDown(HK_MetroidShootScan) || emuInstance->hotkeyDown(HK_MetroidScanShoot)) {
                        FN_INPUT_PRESS(INPUT_L);
                    }
                    else {
                        FN_INPUT_RELEASE(INPUT_L);
                    }

                    // Zoom, map zoom out
                    if (emuInstance->hotkeyDown(HK_MetroidZoom)) {
                        FN_INPUT_PRESS(INPUT_R);
                    }
                    else {
                        FN_INPUT_RELEASE(INPUT_R);
                    }

                    // Jump
                    if (emuInstance->hotkeyDown(HK_MetroidJump)) {
                        FN_INPUT_PRESS(INPUT_B);
                    }
                    else {
                        FN_INPUT_RELEASE(INPUT_B);
                    }

                    // Alt-form
                    if (emuInstance->hotkeyPressed(HK_MetroidMorphBall)) {
                        emuInstance->nds->ReleaseScreen();
                        frameAdvance(2);
                        emuInstance->nds->TouchScreen(231, 167);
                        frameAdvance(2);

                        if (isSamus) {
                            enableAim = false; // in case isAltForm isnt immediately true

                            // boost ball doesnt work unless i release screen late enough
                            for (int i = 0; i < 4; i++) {
                                frameAdvance(2);
                                emuInstance->nds->ReleaseScreen();
                            }
                        }
                    }

                    // Define a lambda function to switch weapons
                    auto SwitchWeapon = [&](int weaponIndex) {

                        // Check for Already equipped
                        if (emuInstance->nds->ARM9Read8(selectedWeaponAddr) == weaponIndex) {
                            // emuInstance->osdAddMessage(0, "Weapon switch unnecessary: Already equipped");
                            return; // Early return if the weapon is already equipped
                        }

                        // Check isMapOrUserActionPaused, for the issue "If you switch weapons while the map is open, the aiming mechanism may become stuck."
                        if (isInAdventure && emuInstance->nds->ARM9Read8(isMapOrUserActionPausedAddr) == 0x1) {
                            return;
                        }

                        // Read the current jump flag value
                        uint8_t currentJumpFlags = emuInstance->nds->ARM9Read8(jumpFlagAddr);

                        // Check if the upper 4 bits are odd (1 or 3)
                        // this is for fixing issue: Shooting and transforming become impossible, when changing weapons at high speed while transitioning from transformed to normal form.
                        bool isTransforming = currentJumpFlags & 0x10;

                        uint8_t jumpFlag = currentJumpFlags & 0x0F;  // Get the lower 4 bits
                        //emuInstance->osdAddMessage(0, ("JumpFlag:" + std::string(1, "0123456789ABCDEF"[emuInstance->nds->ARM9Read8(jumpFlagAddr) & 0x0F])).c_str());

                        bool isRestoreNeeded = false;

                        // Check if in alternate form (transformed state)
                        isAltForm = emuInstance->nds->ARM9Read8(isAltFormAddr) == 0x02;

                        // If not jumping (jumpFlag == 0) and in normal form, temporarily set to jumped state (jumpFlag == 1)
                        if (!isTransforming && jumpFlag == 0 && !isAltForm) {
                            uint8_t newFlags = (currentJumpFlags & 0xF0) | 0x01;  // Set lower 4 bits to 1
                            emuInstance->nds->ARM9Write8(jumpFlagAddr, newFlags);
                            isRestoreNeeded = true;
                            //emuInstance->osdAddMessage(0, ("JumpFlag:" + std::string(1, "0123456789ABCDEF"[emuInstance->nds->ARM9Read8(jumpFlagAddr) & 0x0F])).c_str());
                            //emuInstance->osdAddMessage(0, "Done setting jumpFlag.");
                        }

                        // Lambda to set the weapon-changing state
                        auto setChangingWeapon = [](int value) -> int {
                            // Apply mask to set the lower 4 bits to 1011 (B in hexadecimal)
                            return (value & 0xF0) | 0x0B; // Keep the upper 4 bits, set lower 4 bits to 1011
                            };

                        // Modify the value using the lambda
                        int valueOfWeaponChange = setChangingWeapon(emuInstance->nds->ARM9Read8(weaponChangeAddr));

                        // Write the weapon change command to ARM9
                        emuInstance->nds->ARM9Write8(weaponChangeAddr, valueOfWeaponChange); // Only change the lower 4 bits to B

                        // Change the weapon
                        emuInstance->nds->ARM9Write8(selectedWeaponAddr, weaponIndex);  // Write the address of the corresponding weapon

                        // Release the screen (for weapon change)
                        emuInstance->nds->ReleaseScreen();

                        // Advance frames (for reflection of ReleaseScreen, WeaponChange)
                        frameAdvance(2);

                        // Need Touch after ReleaseScreen for aiming.
#ifndef STYLUS_MODE
                        emuInstance->nds->TouchScreen(128, 88);
#else
                        if (emuInstance->isTouching) {
                            emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
                        }
#endif

                        // Advance frames (for reflection of Touch. This is necessary for no jump)
                        frameAdvance(2);

                        // Restore the jump flag to its original value (if necessary)
                        if (isRestoreNeeded) {
                            currentJumpFlags = emuInstance->nds->ARM9Read8(jumpFlagAddr);
                            uint8_t restoredFlags = (currentJumpFlags & 0xF0) | jumpFlag;
                            emuInstance->nds->ARM9Write8(jumpFlagAddr, restoredFlags);
                            //emuInstance->osdAddMessage(0, ("JumpFlag:" + std::string(1, "0123456789ABCDEF"[emuInstance->nds->ARM9Read8(jumpFlagAddr) & 0x0F])).c_str());
                            //emuInstance->osdAddMessage(0, "Restored jumpFlag.");

                        }

                        };

                    // Switch to Power Beam
                    if (emuInstance->hotkeyPressed(HK_MetroidWeaponBeam)) {
                        SwitchWeapon(0);
                    }

                    // Switch to Missile
                    if (emuInstance->hotkeyPressed(HK_MetroidWeaponMissile)) {
                        SwitchWeapon(2);
                    }

                    // Array of sub-weapon hotkeys (Associating hotkey definitions with weapon indices)
                    static constexpr int weaponHotkeys[] = {
                        HK_MetroidWeapon1,  // ShockCoil    7
                        HK_MetroidWeapon2,  // Magmaul      6
                        HK_MetroidWeapon3,  // Judicator    5
                        HK_MetroidWeapon4,  // Imperialist  4
                        HK_MetroidWeapon5,  // Battlehammer 3
                        HK_MetroidWeapon6   // VoltDriver   1
                                            // Omega Cannon 8 we don't need to set this here, because we need {last used weapon / Omega cannon}
                    };

                    int weaponIndices[] = { 7, 6, 5, 4, 3, 1 };  // Address of the weapon corresponding to each hotkey

                    // Sub-weapons processing (handled in a loop)
                    for (int i = 0; i < 6; i++) {
                        if (emuInstance->hotkeyPressed(weaponHotkeys[i])) {
                            SwitchWeapon(weaponIndices[i]);  // Switch to the corresponding weapon

                            // Exit loop when hotkey is pressed (because weapon switching is completed)
                            break;
                        }
                    }

                    // Change to loaded SpecialWeapon, Last used weapon or Omega Canon
                    if (emuInstance->hotkeyPressed(HK_MetroidWeaponSpecial)) {
                        uint8_t loadedSpecialWeapon = emuInstance->nds->ARM9Read8(loadedSpecialWeaponAddr);
                        if (loadedSpecialWeapon != 0xFF) {
                            // switchWeapon if special weapon is loaded
                            SwitchWeapon(loadedSpecialWeapon);
                        }
                    }

                    // Morph ball boost
                    if (isSamus && emuInstance->hotkeyDown(HK_MetroidHoldMorphBallBoost))
                    {
                        isAltForm = emuInstance->nds->ARM9Read8(isAltFormAddr) == 0x02;
                        if (isAltForm) {
                            uint8_t boostGaugeValue = emuInstance->nds->ARM9Read8(boostGaugeAddr);
                            bool isBoosting = emuInstance->nds->ARM9Read8(isBoostingAddr) != 0x00;

                            // boostable when gauge value is 0x05-0x0F(max)
                            bool isBoostGaugeEnough = boostGaugeValue > 0x0A;

                            // just incase
                            enableAim = false;

                            // release for boost?
                            emuInstance->nds->ReleaseScreen();

                            if (!isBoosting && isBoostGaugeEnough) {
                                // do boost by releasing boost key
                                FN_INPUT_RELEASE(INPUT_R);
                            }
                            else {
                                // charge boost gauge by holding boost key
                                FN_INPUT_PRESS(INPUT_R);
                            }

                            if (isBoosting) {
                                // touch again for aiming
#ifndef STYLUS_MODE
                                emuInstance->nds->TouchScreen(128, 88);
#else
                                if (emuInstance->isTouching) {
                                    emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
                                }
#endif
                            }

                        }
                    }



                    // Weapon switching of Next/Previous
                    const int wheelDelta = emuInstance->getMainWindow()->panel->getDelta();
                    const bool hasDelta = wheelDelta != 0;
                    const bool hotkeyNext = hasDelta ? false : emuInstance->hotkeyPressed(HK_MetroidWeaponNext);

                    if (__builtin_expect(hasDelta || hotkeyNext || emuInstance->hotkeyPressed(HK_MetroidWeaponPrevious), true)) {
                        // Pre-fetch memory values to avoid multiple reads
                        const uint8_t currentWeapon = emuInstance->nds->ARM9Read8(currentWeaponAddr);
                        const uint16_t havingWeapons = emuInstance->nds->ARM9Read16(havingWeaponsAddr);

                        // Read both ammo values with a single 32-bit read
                        // Format: [16-bit missile ammo][16-bit weapon ammo]
                        const uint32_t ammoData = emuInstance->nds->ARM9Read32(weaponAmmoAddr);
                        const uint16_t missileAmmo = ammoData >> 16;     // Extract upper 16 bits for missile ammo
                        const uint16_t weaponAmmo = ammoData & 0xFFFF;   // Extract lower 16 bits for weapon ammo
                        const bool nextTrigger = (wheelDelta < 0) || hotkeyNext;

                        // Weapon constants as lookup tables
                        static constexpr uint8_t WEAPON_ORDER[] = { 0, 2, 7, 6, 5, 4, 3, 1, 8 };
                        static constexpr uint16_t WEAPON_MASKS[] = { 0x001, 0x004, 0x080, 0x040, 0x020, 0x010, 0x008, 0x002, 0x100 };
                        static constexpr uint8_t  MIN_AMMO[] = { 0, 0x5, 0xA, 0x4, 0x14, 0x5, 0xA, 0xA, 0 };

                        // static constexpr size_t WEAPON_COUNT = sizeof(WEAPON_ORDER) / sizeof(WEAPON_ORDER[0]);
                        static constexpr uint8_t WEAPON_COUNT = 9;

                        // Find current weapon index using lookup table
                        static constexpr uint8_t WEAPON_INDEX_MAP[WEAPON_COUNT] = { 0, 7, 1, 6, 5, 4, 3, 2, 8 };

                        uint8_t currentIndex = WEAPON_INDEX_MAP[currentWeapon];
                        const uint8_t startIndex = currentIndex;

                        // Inline weapon checking logic for better performance
                        auto hasWeapon = [havingWeapons](uint8_t weapon, uint16_t mask) {
                            // return weapon == 0 || weapon == 2 || (havingWeapons & mask);
                            return havingWeapons & mask;
                            };

                        auto hasEnoughAmmo = [weaponAmmo, missileAmmo, isWeavel](uint8_t weapon, uint8_t minAmmo) {
                            if (weapon == 0 || weapon == 8) return true;
                            if (weapon == 2) return missileAmmo >= 0xA;
                            if (weapon == 3 && isWeavel) return weaponAmmo >= 0x5; // Prime Hunter check is needless, if we have only 0x4 ammo, we can equipt battleHammer but can't shoot. it's a bug of MPH. so what we need to check is only it's weavel or not.
                            return weaponAmmo >= minAmmo;
                            };

                        // Main weapon selection loop
                        do {
                            currentIndex = (currentIndex + (nextTrigger ? 1 : WEAPON_COUNT - 1)) % WEAPON_COUNT;
                            uint8_t nextWeapon = WEAPON_ORDER[currentIndex];

                            if (hasWeapon(nextWeapon, WEAPON_MASKS[currentIndex]) &&
                                hasEnoughAmmo(nextWeapon, MIN_AMMO[nextWeapon])) {
                                SwitchWeapon(nextWeapon);
                                break;
                            }
                        } while (currentIndex != startIndex);
                    }

                    // Start / View Match progress, points
                    if (emuInstance->hotkeyDown(HK_MetroidMenu)) {
                        FN_INPUT_PRESS(INPUT_START);
                    }
                    else {
                        FN_INPUT_RELEASE(INPUT_START);
                    }


                    if (isInAdventure) {
                        // Adventure Mode Functions


                        // Scan Visor
                        if (emuInstance->hotkeyPressed(HK_MetroidScanVisor)) {
                            emuInstance->nds->ReleaseScreen();
                            frameAdvance(2);

                            bool inVisor = emuInstance->nds->ARM9Read8(inVisorOrMapAddr) == 0x1;
                            // emuInstance->osdAddMessage(0, "in visor %d", inVisor);

                            emuInstance->nds->TouchScreen(128, 173);

                            if (inVisor) {
                                frameAdvance(2);
                            }
                            else {
                                for (int i = 0; i < 30; i++) {
                                    // still allow movement whilst we're enabling scan visor
                                    processMoveInput();
                                    emuInstance->nds->SetKeyMask(emuInstance->getInputMask());
                                    frameAdvanceOnce();
                                }
                            }

                            emuInstance->nds->ReleaseScreen();
                            frameAdvance(2);
                        }

                        // OK (in scans and messages)
                        if (emuInstance->hotkeyPressed(HK_MetroidUIOk)) {
                            emuInstance->nds->ReleaseScreen();
                            frameAdvance(2);
                            emuInstance->nds->TouchScreen(128, 142);
                            frameAdvance(2);
                        }

                        // Left arrow (in scans and messages)
                        if (emuInstance->hotkeyPressed(HK_MetroidUILeft)) {
                            emuInstance->nds->ReleaseScreen();
                            frameAdvance(2);
                            emuInstance->nds->TouchScreen(71, 141);
                            frameAdvance(2);
                        }

                        // Right arrow (in scans and messages)
                        if (emuInstance->hotkeyPressed(HK_MetroidUIRight)) {
                            emuInstance->nds->ReleaseScreen();
                            frameAdvance(2);
                            emuInstance->nds->TouchScreen(185, 141); // optimization ?
                            frameAdvance(2);
                        }

                        // Enter to Starship
                        if (emuInstance->hotkeyPressed(HK_MetroidUIYes)) {
                            emuInstance->nds->ReleaseScreen();
                            frameAdvance(2);
                            emuInstance->nds->TouchScreen(96, 142);
                            frameAdvance(2);
                        }

                        // No Enter to Starship
                        if (emuInstance->hotkeyPressed(HK_MetroidUINo)) {
                            emuInstance->nds->ReleaseScreen();
                            frameAdvance(2);
                            emuInstance->nds->TouchScreen(160, 142);
                            frameAdvance(2);
                        }
                    } // End of Adventure Functions

#ifndef STYLUS_MODE
                    // Touch again for aiming
                    if (!wasLastFrameFocused || enableAim) {
                        // touch again for aiming
                        // When you return to melonPrimeDS or normal form

                        // emuInstance->osdAddMessage(0,"touching screen for aim");

                        // Changed Y point center(96) to 88, For fixing issue: Alt Tab switches hunter choice.
                        //emuInstance->nds->TouchScreen(128, 96); // required for aiming


                        emuInstance->nds->TouchScreen(128, 88); // required for aiming
                    }
#endif

                    // End of in-game
                }
                else {
                    // !isInGame

                    if(hasInitialized){
                        hasInitialized = false;
#ifndef STYLUS_MODE
                        showCursorOnMelonPrimeDS(true);
#endif
                    }

                    // Resolve Menu flickering
                    if(videoRenderer != renderer3D_Software){
                        videoRenderer = renderer3D_Software;
                        updateRenderer();
                    }

                    if (emuInstance->isTouching) {
                        emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
                    }
                    else {
                        emuInstance->nds->ReleaseScreen();
                    }

                    // L For Hunter License
                    if (emuInstance->hotkeyPressed(HK_MetroidUILeft)) {
                        FN_INPUT_PRESS(INPUT_L);
                    }
                    else {
                        FN_INPUT_RELEASE(INPUT_L);
                    }

                    // R For Hunter License
                    if (emuInstance->hotkeyPressed(HK_MetroidUIRight)) {
                        FN_INPUT_PRESS(INPUT_R);
                    }
                    else {
                        FN_INPUT_RELEASE(INPUT_R);
                    }

                }


            }// END of if(isFocused)

            emuInstance->nds->SetKeyMask(emuInstance->getInputMask());

            // record last frame was forcused or not
            wasLastFrameFocused = isFocused;
        } // End of isRomDetected
 

        // MelonPrimeDS Functions END


        frameAdvanceOnce();

    } // End of while (emuStatus != emuStatus_Exit)



}

void EmuThread::sendMessage(Message msg)
{
    msgMutex.lock();
    msgQueue.enqueue(msg);
    msgMutex.unlock();
}

void EmuThread::waitMessage(int num)
{
    if (QThread::currentThread() == this) return;
    msgSemaphore.acquire(num);
}

void EmuThread::waitAllMessages()
{
    if (QThread::currentThread() == this) return;
    while (!msgQueue.empty())
        msgSemaphore.acquire();
}

void EmuThread::handleMessages()
{
    msgMutex.lock();
    while (!msgQueue.empty())
    {
        Message msg = msgQueue.dequeue();
        switch (msg.type)
        {
        case msg_Exit:
            emuStatus = emuStatus_Exit;
            emuPauseStack = emuPauseStackRunning;

            emuInstance->audioDisable();
            MPInterface::Get().End(emuInstance->instanceID);
            break;

        case msg_EmuRun:
            emuStatus = emuStatus_Running;
            emuPauseStack = emuPauseStackRunning;
            emuActive = true;

            emuInstance->audioEnable();
            emit windowEmuStart();
            break;

        case msg_EmuPause:
            emuPauseStack++;
            if (emuPauseStack > emuPauseStackPauseThreshold) break;

            prevEmuStatus = emuStatus;
            emuStatus = emuStatus_Paused;

            if (prevEmuStatus != emuStatus_Paused)
            {
                emuInstance->audioDisable();
                emit windowEmuPause(true);
                emuInstance->osdAddMessage(0, "Paused");
            }
            break;

        case msg_EmuUnpause:
            if (emuPauseStack < emuPauseStackPauseThreshold) break;

            emuPauseStack--;
            if (emuPauseStack >= emuPauseStackPauseThreshold) break;

            emuStatus = prevEmuStatus;

            if (emuStatus != emuStatus_Paused)
            {
                emuInstance->audioEnable();
                emit windowEmuPause(false);
                emuInstance->osdAddMessage(0, "Resumed");

                // MelonPrimeDS {
                // applyVideoSettings Immediately when resumed
                if(isInGame){
                    emuInstance->setVSyncGL(false); // MelonPrimeDS
                    videoRenderer = emuInstance->getGlobalConfig().GetInt("3D.Renderer");
                    updateRenderer();
                    emuInstance->setVSyncGL(false); // MelonPrimeDS
                }
                // MelonPrimeDS }
            }
            break;

        case msg_EmuStop:
            if (msg.param.value<bool>())
                emuInstance->nds->Stop();
            emuStatus = emuStatus_Paused;
            emuActive = false;

            emuInstance->audioDisable();
            emit windowEmuStop();
            break;

        case msg_EmuFrameStep:
            emuStatus = emuStatus_FrameStep;
            break;

        case msg_EmuReset:
            emuInstance->reset();

            emuStatus = emuStatus_Running;
            emuPauseStack = emuPauseStackRunning;
            emuActive = true;

            emuInstance->audioEnable();
            emit windowEmuReset();
            emuInstance->osdAddMessage(0, "Reset");
            break;

        case msg_InitGL:
            emuInstance->initOpenGL(msg.param.value<int>());
            useOpenGL = true;
            break;

        case msg_DeInitGL:
            emuInstance->deinitOpenGL(msg.param.value<int>());
            if (msg.param.value<int>() == 0)
                useOpenGL = false;
            break;

        case msg_BootROM:
            msgResult = 0;
            if (!emuInstance->loadROM(msg.param.value<QStringList>(), true, msgError))
                break;

            assert(emuInstance->nds != nullptr);
            emuInstance->nds->Start();
            msgResult = 1;
            break;

        case msg_BootFirmware:
            msgResult = 0;
            if (!emuInstance->bootToMenu(msgError))
                break;

            assert(emuInstance->nds != nullptr);
            emuInstance->nds->Start();
            msgResult = 1;
            break;

        case msg_InsertCart:
            msgResult = 0;
            if (!emuInstance->loadROM(msg.param.value<QStringList>(), false, msgError))
                break;

            msgResult = 1;
            break;

        case msg_EjectCart:
            emuInstance->ejectCart();
            break;

        case msg_InsertGBACart:
            msgResult = 0;
            if (!emuInstance->loadGBAROM(msg.param.value<QStringList>(), msgError))
                break;

            msgResult = 1;
            break;

        case msg_InsertGBAAddon:
            msgResult = 0;
            emuInstance->loadGBAAddon(msg.param.value<int>(), msgError);
            msgResult = 1;
            break;

        case msg_EjectGBACart:
            emuInstance->ejectGBACart();
            break;

        case msg_SaveState:
            msgResult = emuInstance->saveState(msg.param.value<QString>().toStdString());
            break;

        case msg_LoadState:
            msgResult = emuInstance->loadState(msg.param.value<QString>().toStdString());
            break;

        case msg_UndoStateLoad:
            emuInstance->undoStateLoad();
            msgResult = 1;
            break;

        case msg_ImportSavefile:
            {
                msgResult = 0;
                auto f = Platform::OpenFile(msg.param.value<QString>().toStdString(), Platform::FileMode::Read);
                if (!f) break;

                u32 len = FileLength(f);

                std::unique_ptr<u8[]> data = std::make_unique<u8[]>(len);
                Platform::FileRewind(f);
                Platform::FileRead(data.get(), len, 1, f);

                assert(emuInstance->nds != nullptr);
                emuInstance->nds->SetNDSSave(data.get(), len);

                CloseFile(f);
                msgResult = 1;
            }
            break;

        case msg_EnableCheats:
            emuInstance->enableCheats(msg.param.value<bool>());
            break;
        }

        msgSemaphore.release();
    }
    msgMutex.unlock();
}

void EmuThread::changeWindowTitle(char* title)
{
    emit windowTitleChange(QString(title));
}

void EmuThread::initContext(int win)
{
    sendMessage({.type = msg_InitGL, .param = win});
    waitMessage();
}

void EmuThread::deinitContext(int win)
{
    sendMessage({.type = msg_DeInitGL, .param = win});
    waitMessage();
}

void EmuThread::emuRun()
{
    sendMessage(msg_EmuRun);
    waitMessage();
}

void EmuThread::emuPause(bool broadcast)
{
    sendMessage(msg_EmuPause);
    waitMessage();

    if (broadcast)
        emuInstance->broadcastCommand(InstCmd_Pause);
}

void EmuThread::emuUnpause(bool broadcast)
{
    sendMessage(msg_EmuUnpause);
    waitMessage();

    if (broadcast)
        emuInstance->broadcastCommand(InstCmd_Unpause);
}

void EmuThread::emuTogglePause(bool broadcast)
{
    if (emuStatus == emuStatus_Paused)
        emuUnpause(broadcast);
    else
        emuPause(broadcast);
}

void EmuThread::emuStop(bool external)
{
    sendMessage({.type = msg_EmuStop, .param = external});
    waitMessage();
}

void EmuThread::emuExit()
{
    sendMessage(msg_Exit);
    waitAllMessages();
}

void EmuThread::emuFrameStep()
{
    if (emuPauseStack < emuPauseStackPauseThreshold)
        sendMessage(msg_EmuPause);
    sendMessage(msg_EmuFrameStep);
    waitAllMessages();
}

void EmuThread::emuReset()
{
    sendMessage(msg_EmuReset);
    waitMessage();
}

bool EmuThread::emuIsRunning()
{
    return emuStatus == emuStatus_Running;
}

bool EmuThread::emuIsActive()
{
    return emuActive;
}

int EmuThread::bootROM(const QStringList& filename, QString& errorstr)
{
    sendMessage({.type = msg_BootROM, .param = filename});
    waitMessage();
    if (!msgResult)
    {
        errorstr = msgError;
        return msgResult;
    }

    sendMessage(msg_EmuRun);
    waitMessage();
    errorstr = "";
    return msgResult;
}

int EmuThread::bootFirmware(QString& errorstr)
{
    sendMessage(msg_BootFirmware);
    waitMessage();
    if (!msgResult)
    {
        errorstr = msgError;
        return msgResult;
    }

    sendMessage(msg_EmuRun);
    waitMessage();
    errorstr = "";
    return msgResult;
}

int EmuThread::insertCart(const QStringList& filename, bool gba, QString& errorstr)
{
    MessageType msgtype = gba ? msg_InsertGBACart : msg_InsertCart;

    sendMessage({.type = msgtype, .param = filename});
    waitMessage();
    errorstr = msgResult ? "" : msgError;
    return msgResult;
}

void EmuThread::ejectCart(bool gba)
{
    sendMessage(gba ? msg_EjectGBACart : msg_EjectCart);
    waitMessage();
}

int EmuThread::insertGBAAddon(int type, QString& errorstr)
{
    sendMessage({.type = msg_InsertGBAAddon, .param = type});
    waitMessage();
    errorstr = msgResult ? "" : msgError;
    return msgResult;
}

int EmuThread::saveState(const QString& filename)
{
    sendMessage({.type = msg_SaveState, .param = filename});
    waitMessage();
    return msgResult;
}

int EmuThread::loadState(const QString& filename)
{
    sendMessage({.type = msg_LoadState, .param = filename});
    waitMessage();
    return msgResult;
}

int EmuThread::undoStateLoad()
{
    sendMessage(msg_UndoStateLoad);
    waitMessage();
    return msgResult;
}

int EmuThread::importSavefile(const QString& filename)
{
    sendMessage(msg_EmuReset);
    sendMessage({.type = msg_ImportSavefile, .param = filename});
    waitMessage(2);
    return msgResult;
}

void EmuThread::enableCheats(bool enable)
{
    sendMessage({.type = msg_EnableCheats, .param = enable});
    waitMessage();
}

void EmuThread::updateRenderer()
{
    if (videoRenderer != lastVideoRenderer)
    {
        switch (videoRenderer)
        {
            case renderer3D_Software:
                emuInstance->nds->GPU.SetRenderer3D(std::make_unique<SoftRenderer>());
                break;
            case renderer3D_OpenGL:
                emuInstance->nds->GPU.SetRenderer3D(GLRenderer::New());
                break;
            case renderer3D_OpenGLCompute:
                emuInstance->nds->GPU.SetRenderer3D(ComputeRenderer::New());
                break;
            default: __builtin_unreachable();
        }
    }
    lastVideoRenderer = videoRenderer;

    auto& cfg = emuInstance->getGlobalConfig();
    switch (videoRenderer)
    {
        case renderer3D_Software:
            static_cast<SoftRenderer&>(emuInstance->nds->GPU.GetRenderer3D()).SetThreaded(
                    cfg.GetBool("3D.Soft.Threaded"),
                    emuInstance->nds->GPU);
            break;
        case renderer3D_OpenGL:
            static_cast<GLRenderer&>(emuInstance->nds->GPU.GetRenderer3D()).SetRenderSettings(
                    cfg.GetBool("3D.GL.BetterPolygons"),
                    cfg.GetInt("3D.GL.ScaleFactor"));
            break;
        case renderer3D_OpenGLCompute:
            static_cast<ComputeRenderer&>(emuInstance->nds->GPU.GetRenderer3D()).SetRenderSettings(
                    cfg.GetInt("3D.GL.ScaleFactor"),
                    cfg.GetBool("3D.GL.HiresCoordinates"));
            break;
        default: __builtin_unreachable();
    }
}

void EmuThread::compileShaders()
{
    int currentShader, shadersCount;
    u64 startTime = SDL_GetPerformanceCounter();
    // kind of hacky to look at the wallclock, though it is easier than
    // than disabling vsync
    do
    {
        emuInstance->nds->GPU.GetRenderer3D().ShaderCompileStep(currentShader, shadersCount);
    } while (emuInstance->nds->GPU.GetRenderer3D().NeedsShaderCompile() &&
             (SDL_GetPerformanceCounter() - startTime) * perfCountsSec < 1.0 / 6.0);
    emuInstance->osdAddMessage(0, "Compiling shader %d/%d", currentShader+1, shadersCount);
}
