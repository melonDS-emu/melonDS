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
#include "MelonPrimeDef.h"
#include "MelonPrimeRomAddrTable.h"

// CalculatePlayerAddress Function
__attribute__((always_inline, flatten)) inline uint32_t calculatePlayerAddress(uint32_t baseAddress, uint8_t playerPosition, int32_t increment) {
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

bool isAltForm;
bool isInGame = false; // MelonPrimeDS
bool isLayoutChangePending = true;       // MelonPrimeDS layout change flag - set true to trigger on first run
bool isSensitivityChangePending = true;  // MelonPrimeDS sensitivity change flag - set true to trigger on first run
bool isSnapTapMode = false;
bool isUnlockHuntersMaps = false;

melonDS::u32 baseIsAltFormAddr;
melonDS::u32 baseLoadedSpecialWeaponAddr;
melonDS::u32 baseWeaponChangeAddr;
melonDS::u32 baseSelectedWeaponAddr;
melonDS::u32 baseChosenHunterAddr;
melonDS::u32 baseJumpFlagAddr;
melonDS::u32 inGameAddr;
melonDS::u32 PlayerPosAddr;
melonDS::u32 isInVisorOrMapAddr;
melonDS::u32 baseAimXAddr;
melonDS::u32 baseAimYAddr;
melonDS::u32 aimXAddr;
melonDS::u32 aimYAddr;
melonDS::u32 isInAdventureAddr;
melonDS::u32 isMapOrUserActionPausedAddr; // for issue in AdventureMode, Aim Stopping when SwitchingWeapon. 
melonDS::u32 unlockMapsHuntersAddr;
melonDS::u32 unlockMapsHuntersAddr2;
melonDS::u32 unlockMapsHuntersAddr3;
melonDS::u32 unlockMapsHuntersAddr4;
melonDS::u32 unlockMapsHuntersAddr5;
static bool isUnlockMapsHuntersApplied = false;

// ROM detection and address setup (self-contained within this function)
__attribute__((always_inline, flatten)) inline void detectRomAndSetAddresses(EmuInstance* emuInstance) {
    // Define ROM groups
    /*
    enum RomGroup {
        GROUP_US1_1,     // US1.1, US1.1_ENCRYPTED
        GROUP_US1_0,     // US1.0, US1.0_ENCRYPTED
        GROUP_EU1_1,     // EU1.1, EU1.1_ENCRYPTED, EU1_1_BALANCED
        GROUP_EU1_0,     // EU1.0, EU1.0_ENCRYPTED
        GROUP_JP1_0,     // JP1.0, JP1.0_ENCRYPTED
        GROUP_JP1_1,     // JP1.1, JP1.1_ENCRYPTED
        GROUP_KR1_0,     // KR1.0, KR1.0_ENCRYPTED
    };
    */

    // ROM information structure
    struct RomInfo {
        uint32_t checksum;
        const char* name;
        RomGroup group;
    };

    // Mapping of checksums to ROM info (stack-allocated)
    const RomInfo ROM_INFO_TABLE[] = {
        {RomVersions::US1_1,           "US1.1",           GROUP_US1_1},
        {RomVersions::US1_1_ENCRYPTED, "US1.1 ENCRYPTED", GROUP_US1_1},
        {RomVersions::US1_0,           "US1.0",           GROUP_US1_0},
        {RomVersions::US1_0_ENCRYPTED, "US1.0 ENCRYPTED", GROUP_US1_0},
        {RomVersions::EU1_1,           "EU1.1",           GROUP_EU1_1},
        {RomVersions::EU1_1_ENCRYPTED, "EU1.1 ENCRYPTED", GROUP_EU1_1},
        {RomVersions::EU1_1_BALANCED,  "EU1.1 BALANCED",  GROUP_EU1_1},
        {RomVersions::EU1_0,           "EU1.0",           GROUP_EU1_0},
        {RomVersions::EU1_0_ENCRYPTED, "EU1.0 ENCRYPTED", GROUP_EU1_0},
        {RomVersions::JP1_0,           "JP1.0",           GROUP_JP1_0},
        {RomVersions::JP1_0_ENCRYPTED, "JP1.0 ENCRYPTED", GROUP_JP1_0},
        {RomVersions::JP1_1,           "JP1.1",           GROUP_JP1_1},
        {RomVersions::JP1_1_ENCRYPTED, "JP1.1 ENCRYPTED", GROUP_JP1_1},
        {RomVersions::KR1_0,           "KR1.0",           GROUP_KR1_0},
        {RomVersions::KR1_0_ENCRYPTED, "KR1.0 ENCRYPTED", GROUP_KR1_0},
    };

    // Search ROM info from checksum
    const RomInfo* romInfo = nullptr;
    for (const auto& info : ROM_INFO_TABLE) {
        if (globalChecksum == info.checksum) {
            romInfo = &info;
            break;
        }
    }

    // If ROM is unsupported
    if (!romInfo) {
        return;
    }

    // ---- ここで呼ぶ！ ----
    // グループを一度だけローカル変数に保存
    RomGroup detectedGroup = romInfo->group;

    // SetRomGroupFlags(detectedGroup);   // ★ ROMグループのフラグ設定 ★

    // 既存変数への一括設定実施(分岐削減のため)
    // グローバル列挙体の完全修飾指定とキャスト実施(同名列挙体のシャドーイング回避のため)
    detectRomAndSetAddresses_fast(
        // 列挙体の完全修飾(::RomGroup)とint変換後のstatic_cast実施(型不一致解消のため)
        detectedGroup,
        // ChosenHunterアドレス引数受け渡し実施(既存変数の直接利用のため)
        baseChosenHunterAddr,
        // inGameアドレス引数受け渡し実施(既存変数の直接利用のため)
        inGameAddr,
        // プレイヤ位置アドレス引数受け渡し実施(既存変数の直接利用のため)
        PlayerPosAddr,
        // AltFormアドレス引数受け渡し実施(既存変数の直接利用のため)
        baseIsAltFormAddr,
        // 武器変更アドレス引数受け渡し実施(既存変数の直接利用のため)
        baseWeaponChangeAddr,
        // 選択武器アドレス引数受け渡し実施(既存変数の直接利用のため)
        baseSelectedWeaponAddr,
        // AimXアドレス引数受け渡し実施(既存変数の直接利用のため)
        baseAimXAddr,
        // AimYアドレス引数受け渡し実施(既存変数の直接利用のため)
        baseAimYAddr,
        // ADV/Multi判定アドレス引数受け渡し実施(既存変数の直接利用のため)
        isInAdventureAddr,
        // ポーズ判定アドレス引数受け渡し実施(既存変数の直接利用のため)
        isMapOrUserActionPausedAddr,
        unlockMapsHuntersAddr
    );

    // Addresses calculated from base values
    isInVisorOrMapAddr = PlayerPosAddr - 0xABB;
    baseLoadedSpecialWeaponAddr = baseIsAltFormAddr + 0x56;
    baseJumpFlagAddr = baseSelectedWeaponAddr - 0xA;
    unlockMapsHuntersAddr2 = unlockMapsHuntersAddr + 0x3;
    unlockMapsHuntersAddr3 = unlockMapsHuntersAddr + 0x7;
    unlockMapsHuntersAddr4 = unlockMapsHuntersAddr + 0xB;
    unlockMapsHuntersAddr5 = unlockMapsHuntersAddr + 0xF;

    isRomDetected = true;

    // ROM detection message
    char message[256];
    sprintf(message, "MPH Rom version detected: %s", romInfo->name);
    emuInstance->osdAddMessage(0, message);

    // 判定後の処理

    // フラグリセット
    isUnlockMapsHuntersApplied = false;
}




































/* MelonPrimeDS function goes here */
void EmuThread::run()
{
    Config::Table& globalCfg = emuInstance->getGlobalConfig();
    Config::Table& localCfg = emuInstance->getLocalConfig();
    isSnapTapMode = localCfg.GetBool("Metroid.Operation.SnapTap"); // MelonPrimeDS
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

    



    auto frameAdvanceOnce = [&]()  __attribute__((hot, always_inline, flatten)) {
        MPInterface::Get().Process();
        emuInstance->inputProcess();

        if (emuInstance->hotkeyPressed(HK_FrameLimitToggle)) emit windowLimitFPSChange();

        if (emuInstance->hotkeyPressed(HK_Pause)) emuTogglePause();
        if (emuInstance->hotkeyPressed(HK_Reset)) emuReset();
        if (emuInstance->hotkeyPressed(HK_FrameStep)) emuFrameStep();

        // MelonPrimeDS ホットキー処理部分を修正
        if (emuInstance->hotkeyPressed(HK_FullscreenToggle)) {
            emit windowFullscreenToggle();
            isLayoutChangePending = true;
        }

        if (emuInstance->hotkeyPressed(HK_SwapScreens)) {
            emit swapScreensToggle();
            isLayoutChangePending = true;
        }

        if (emuInstance->hotkeyPressed(HK_SwapScreenEmphasis)) {
            emit screenEmphasisToggle();
            isLayoutChangePending = true;
        }
        // Define minimum sensitivity as a constant to improve readability and optimization
        static constexpr int MIN_SENSITIVITY = 1;

        // Lambda function to update aim sensitivity with low latency
        static const auto updateAimSensitivity = [&](const int change) {
            // Get current sensitivity from config
            int currentSensitivity = localCfg.GetInt("Metroid.Sensitivity.Aim");

            // Calculate new sensitivity value
            int newSensitivity = currentSensitivity + change;

            // Check for minimum value threshold with early return for optimization
            if (newSensitivity < MIN_SENSITIVITY) {
                emuInstance->osdAddMessage(0, "AimSensi cannot be decreased below %d", MIN_SENSITIVITY);
                return;
            }

            // Only process if the value has actually changed
            if (newSensitivity != currentSensitivity) {
                // Update the configuration with new value
                localCfg.SetInt("Metroid.Sensitivity.Aim", newSensitivity);
                Config::Save();

                // Display message using format string instead of concatenation
                emuInstance->osdAddMessage(0, "AimSensi Updated: %d->%d", currentSensitivity, newSensitivity);
				isSensitivityChangePending = true;  // フラグを立てる
            }
            };

        // Optimize hotkey handling with a single expression
        {
            const int sensitivityChange =
                emuInstance->hotkeyReleased(HK_MetroidIngameSensiUp) ? 1 :
                emuInstance->hotkeyReleased(HK_MetroidIngameSensiDown) ? -1 : 0;

            if (sensitivityChange != 0) {
                updateAimSensitivity(sensitivityChange);
            }
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

            /*
              MelonPrimeDS commentOut
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

            */

            if (useOpenGL)
                emuInstance->makeCurrentGL();

            // update render settings if needed
            if (videoSettingsDirty)
            {
                emuInstance->renderLock.lock();
                if (useOpenGL)
                {
                    emuInstance->setVSyncGL(true); // is this really needed??
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
    };


    /*
    auto frameAdvance = [&](int n)  __attribute__((hot, always_inline, flatten)) {
        for (int i = 0; i < n; i++) {
            frameAdvanceOnce();
        }
        };

// Define a frequently used macro to advance 2 frames
#define FRAME_ADVANCE_2 \
    do { \
        frameAdvanceOnce(); \
        frameAdvanceOnce(); \
    } while (0) \
    // Note: Why use do { ... } while (0)?
    // This is the standard safe macro form, allowing it to be treated as a single block in statements such as if-conditions.

    */

    static const auto frameAdvanceTwice = [&]() __attribute__((hot, always_inline, flatten)) {
        frameAdvanceOnce();
        frameAdvanceOnce();
    };

    // melonPrimeDS

    bool isCursorVisible = true;
    bool enableAim = true;
    bool wasLastFrameFocused = false;

    /**
     * @brief Function to show or hide the cursor on MelonPrimeDS
     *
     * Controls the mouse cursor visibility state on MelonPrimeDS. Does nothing if
     * the requested state is the same as the current state. When changing cursor
     * visibility, uses Qt::QueuedConnection to safely execute on the UI thread.
     *
     * @param show true to show the cursor, false to hide it
     */
    auto showCursorOnMelonPrimeDS = [&](bool show) __attribute__((always_inline, flatten)) {
        // Do nothing if the requested state is the same as current state (optimization)
        if (show == isCursorVisible) return;

        // Get and verify panel exists
        auto* panel = emuInstance->getMainWindow()->panel;
        if (!panel) return;

        // Use Qt::QueuedConnection to safely execute on the UI thread
        QMetaObject::invokeMethod(panel,
            [panel, show]() {
                // Set cursor visibility (normal ArrowCursor or invisible BlankCursor)
                panel->setCursor(show ? Qt::ArrowCursor : Qt::BlankCursor);
            },
            Qt::ConnectionType::QueuedConnection
        );

        // Record the state change
        isCursorVisible = show;
        };

// #define STYLUS_MODE 1 // this is for stylus user. MelonEK

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

/*
#define FN_INPUT_PRESS(i)   emuInstance->inputMask.setBit(i, false) // No semicolon at the end here
#define FN_INPUT_RELEASE(i) emuInstance->inputMask.setBit(i, true)  // No semicolon at the end here

// Optimized macro definitions - perform direct bit operations instead of using setBit()
// #define FN_INPUT_PRESS(i)   emuInstance->inputMask[i] = false   // Direct assignment for press
// #define FN_INPUT_RELEASE(i) emuInstance->inputMask[i] = true    // Direct assignment for release

    /*
#define PERFORM_TOUCH(x, y) do { \
    emuInstance->nds->ReleaseScreen(); \
    frameAdvanceTwice(); \
    emuInstance->nds->TouchScreen(x, y); \
    frameAdvanceTwice(); \
} while(0)
*/
/*
// 
    static const auto PERFORM_TOUCH = [&](int x, int y) __attribute__((always_inline)) {
        emuInstance->nds->ReleaseScreen();
        frameAdvanceTwice();
        emuInstance->nds->TouchScreen(x, y);
        frameAdvanceTwice();
    };
    */

    /**
     * Macro to obtain a 12-bit input state from a QBitArray as a bitmask.
     * Originally defined in EmuInstanceInput.cpp.
     *
     * @param input QBitArray (must have at least 12 bits).
     * @return uint32_t Bitmask containing input state (bits 0–11).
     */
#define GET_INPUT_MASK(inputMask) (                                          \
    (static_cast<uint32_t>((inputMask).testBit(0))  << 0)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(1))  << 1)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(2))  << 2)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(3))  << 3)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(4))  << 4)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(5))  << 5)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(6))  << 6)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(7))  << 7)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(8))  << 8)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(9))  << 9)  |                 \
    (static_cast<uint32_t>((inputMask).testBit(10)) << 10) |                 \
    (static_cast<uint32_t>((inputMask).testBit(11)) << 11)                   \
)

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
    bool isPaused = false; // MelonPrimeDS

    // The QPoint class defines a point in the plane using integer precision. 
    // auto mouseRel = rawInputThread->fetchMouseDelta();
    // QPoint mouseRel;

    // Initialize Adjusted Center 


    // test
    // Lambda function to get adjusted center position based on window geometry and screen layout
#ifndef STYLUS_MODE
    static const auto getAdjustedCenter = [&]()__attribute__((hot, always_inline, flatten)) -> QPoint {
        // Cache static constants outside the function to avoid recomputation
        static constexpr float DEFAULT_ADJUSTMENT = 0.25f;
        static constexpr float HYBRID_RIGHT = 0.333203125f;  // (2133-1280)/2560 = 85/256  = (n * 85) >> 8
        static constexpr float HYBRID_LEFT = 0.166796875f;   // (1280-853)/2560 = 43/256  =  (n * 43) >> 8
        static QPoint adjustedCenter;
        static bool lastFullscreen = false;
        static bool lastSwapped = false;
        static int lastScreenSizing = -1;
        static int lastLayout = -1;

        auto& windowCfg = emuInstance->getMainWindow()->getWindowConfig();

        // Fast access to current settings - get all at once
        int currentLayout = windowCfg.GetInt("ScreenLayout");
        int currentScreenSizing = windowCfg.GetInt("ScreenSizing");
        bool currentSwapped = windowCfg.GetBool("ScreenSwap");
        bool currentFullscreen = emuInstance->getMainWindow()->isFullScreen();

        // Return cached value if settings haven't changed
        if (currentLayout == lastLayout &&
            currentScreenSizing == lastScreenSizing &&
            currentSwapped == lastSwapped &&
            currentFullscreen == lastFullscreen) {
            return adjustedCenter;
        }

        // Update cached settings
        lastLayout = currentLayout;
        lastScreenSizing = currentScreenSizing;
        lastSwapped = currentSwapped;
        lastFullscreen = currentFullscreen;

        // Get display dimensions once
        const QRect& displayRect = emuInstance->getMainWindow()->panel->geometry();
        const int displayWidth = displayRect.width();
        const int displayHeight = displayRect.height();

        // Calculate base center position
        adjustedCenter = emuInstance->getMainWindow()->panel->mapToGlobal(
            QPoint(displayWidth >> 1, displayHeight >> 1)
        );

        // Fast path for special cases
        if (currentScreenSizing == screenSizing_TopOnly) {
            return adjustedCenter;
        }

        if (currentScreenSizing == screenSizing_BotOnly) {
            if (currentFullscreen) {
                // Precompute constants to avoid repeated multiplications
                const float widthAdjust = displayWidth * 0.4f;
                const float heightAdjust = displayHeight * 0.4f;

                adjustedCenter.rx() -= static_cast<int>(widthAdjust);
                adjustedCenter.ry() -= static_cast<int>(heightAdjust);
            }
            return adjustedCenter;
        }

        // Fast path for Hybrid layout with swap
        if (currentLayout == screenLayout_Hybrid && currentSwapped) {
            // Directly compute result for this specific case
            adjustedCenter.rx() += static_cast<int>(displayWidth * HYBRID_RIGHT);
            adjustedCenter.ry() -= static_cast<int>(displayHeight * DEFAULT_ADJUSTMENT);
            return adjustedCenter;
        }

        // For other cases, determine adjustment values
        float xAdjust = 0.0f;
        float yAdjust = 0.0f;

        // Simplified switch with fewer branches
        switch (currentLayout) {
        case screenLayout_Natural:
        case screenLayout_Vertical:
            yAdjust = DEFAULT_ADJUSTMENT;
            break;
        case screenLayout_Horizontal:
            xAdjust = DEFAULT_ADJUSTMENT;
            break;
        case screenLayout_Hybrid:
            // We already handled the swapped case above
            xAdjust = HYBRID_LEFT;
            break;
        }

        // Apply non-zero adjustments only
        if (xAdjust != 0.0f) {
            // Using direct ternary operator instead of swapFactor variable
            adjustedCenter.rx() += static_cast<int>(displayWidth * xAdjust * (currentSwapped ? 1.0f : -1.0f));
        }

        if (yAdjust != 0.0f) {
            // Using direct ternary operator instead of swapFactor variable
            adjustedCenter.ry() += static_cast<int>(displayHeight * yAdjust * (currentSwapped ? 1.0f : -1.0f));
        }

        return adjustedCenter;
        };
#endif




    // processMoveInputFunction{

#ifdef COMMENTOUTTTTTTTT
        /**
         * 移動入力を処理 v1.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
        static const auto processMoveInput = [&]() __attribute__((hot, always_inline, flatten)) {
            // SnapTap状態構造体定義(キャッシュライン最適化)
            alignas(64) static struct {
                uint32_t lastInputBitmap;    // 前回入力ビットマップ保持
                uint32_t priorityInput;      // 優先入力ビットマップ保持
                uint32_t _padding[14];       // 64バイト境界確保
            } snapTapState = { 0, 0, {} };

            // 水平・垂直競合用マスク定数定義
            static constexpr uint32_t HORIZ_MASK = 0xC;   // (1<<2)|(1<<3) - LEFT|RIGHT
            static constexpr uint32_t VERT_MASK = 0x3;   // (1<<0)|(1<<1) - UP|DOWN

            // 超コンパクトLUT - L1キャッシュ効率最大化
            alignas(16) static constexpr uint8_t LUT[16] = {
                0,    // 0000: なし
                1,    // 0001: ↑ 
                2,    // 0010: ↓
                0,    // 0011: ↑↓(キャンセル)
                4,    // 0100: ←
                5,    // 0101: ↑←
                6,    // 0110: ↓←
                4,    // 0111: ←(↑↓キャンセル)
                8,    // 1000: →
                9,    // 1001: ↑→
                10,   // 1010: ↓→
                8,    // 1011: →(↑↓キャンセル)
                0,    // 1100: ←→(キャンセル)
                1,    // 1101: ↑(←→キャンセル)
                2,    // 1110: ↓(←→キャンセル)
                0     // 1111: 全キャンセル
            };

            // 超高速入力取得 - 現代コンパイラが自動最適化
            const uint32_t f = emuInstance->hotkeyDown(HK_MetroidMoveForward);
            const uint32_t b = emuInstance->hotkeyDown(HK_MetroidMoveBack);
            const uint32_t l = emuInstance->hotkeyDown(HK_MetroidMoveLeft);
            const uint32_t r = emuInstance->hotkeyDown(HK_MetroidMoveRight);

            // 入力ビットマップ生成 - 並列実行最適化
            const uint32_t curr = f | (b << 1) | (l << 2) | (r << 3);

            uint8_t finalState;

            // 分岐予測最適化 - 通常モード優先
            if (__builtin_expect(!isSnapTapMode, 1)) {
                // 通常モード - 直接配列アクセス（最速）
                finalState = LUT[curr];
            }
            else {
                // SnapTap超高速モード
                const uint32_t newPressed = curr & ~snapTapState.lastInputBitmap;

                // 並列競合判定 - XOR最適化
                const uint32_t hConflict = (curr & HORIZ_MASK) ^ HORIZ_MASK;
                const uint32_t vConflict = (curr & VERT_MASK) ^ VERT_MASK;

                // 条件最適化 - 新入力は稀
                if (__builtin_expect(newPressed != 0, 0)) {
                    // 超高速branchless更新 - 単一式統合
                    const uint32_t hMask = -(hConflict == 0);
                    const uint32_t vMask = -(vConflict == 0);

                    snapTapState.priorityInput =
                        (snapTapState.priorityInput & (~HORIZ_MASK | ~hMask) & (~VERT_MASK | ~vMask)) |
                        ((newPressed & HORIZ_MASK) & hMask) |
                        ((newPressed & VERT_MASK) & vMask);
                }

                snapTapState.priorityInput &= curr;

                // 超高速競合解決 - 単一パス処理
                uint32_t finalInput = curr;
                const uint32_t conflictMask =
                    ((hConflict == 0) ? HORIZ_MASK : 0) |
                    ((vConflict == 0) ? VERT_MASK : 0);

                if (__builtin_expect(conflictMask != 0, 0)) {
                    finalInput = (finalInput & ~conflictMask) | (snapTapState.priorityInput & conflictMask);
                }

                snapTapState.lastInputBitmap = curr;

                // 直接配列アクセス - ポインタより高速
                finalState = LUT[finalInput];
            }

            // 究極の入力適用 - コンパイラ自動最適化
            const uint32_t states = finalState;

            // QBitArray超高速更新 - ループ展開 + 最適化
            auto& mask = emuInstance->inputMask;

            // 並列実行可能な独立した操作
            mask.setBit(INPUT_UP, !(states & 1));
            mask.setBit(INPUT_DOWN, !(states & 2));
            mask.setBit(INPUT_LEFT, !(states & 4));
            mask.setBit(INPUT_RIGHT, !(states & 8));
        };

        /**
         * 移動入力を処理 v1 remake.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // SnapTap状態構造体定義(キャッシュライン最適化)
            alignas(64) static struct {
                uint32_t lastInputBitmap;    // 前回入力ビットマップ保持
                uint32_t priorityInput;      // 優先入力ビットマップ保持
                uint32_t _padding[14];       // 64バイト境界確保
            } snapTapState = { 0, 0, {} };

            // 水平・垂直競合用マスク定数定義
            static constexpr uint32_t HORIZ_MASK = 0xC;   // (1<<2)|(1<<3) - LEFT|RIGHT
            static constexpr uint32_t VERT_MASK = 0x3;   // (1<<0)|(1<<1) - UP|DOWN

            // 超コンパクトLUT - L1キャッシュ効率最大化
            alignas(16) static constexpr uint8_t LUT[16] = {
                0,    // 0000: なし
                1,    // 0001: ↑ 
                2,    // 0010: ↓
                0,    // 0011: ↑↓(キャンセル)
                4,    // 0100: ←
                5,    // 0101: ↑←
                6,    // 0110: ↓←
                4,    // 0111: ←(↑↓キャンセル)
                8,    // 1000: →
                9,    // 1001: ↑→
                10,   // 1010: ↓→
                8,    // 1011: →(↑↓キャンセル)
                0,    // 1100: ←→(キャンセル)
                1,    // 1101: ↑(←→キャンセル)
                2,    // 1110: ↓(←→キャンセル)
                0     // 1111: 全キャンセル
            };

            // 超高速入力取得 - 現代コンパイラが自動最適化
            const uint32_t f = hk[HK_MetroidMoveForward];
            const uint32_t b = hk[HK_MetroidMoveBack];
            const uint32_t l = hk[HK_MetroidMoveLeft];
            const uint32_t r = hk[HK_MetroidMoveRight];

            // 入力ビットマップ生成 - 並列実行最適化
            const uint32_t curr = f | (b << 1) | (l << 2) | (r << 3);

            uint8_t finalState;

            // 分岐予測最適化 - 通常モード優先
            if (__builtin_expect(!isSnapTapMode, 1)) {
                // 通常モード - 直接配列アクセス（最速）
                finalState = LUT[curr];
            }
            else {
                // SnapTap超高速モード
                const uint32_t newPressed = curr & ~snapTapState.lastInputBitmap;

                // 並列競合判定 - XOR最適化
                const uint32_t hConflict = (curr & HORIZ_MASK) ^ HORIZ_MASK;
                const uint32_t vConflict = (curr & VERT_MASK) ^ VERT_MASK;

                // 条件最適化 - 新入力は稀
                if (__builtin_expect(newPressed != 0, 0)) {
                    // 超高速branchless更新 - 単一式統合
                    const uint32_t hMask = -(hConflict == 0);
                    const uint32_t vMask = -(vConflict == 0);

                    snapTapState.priorityInput =
                        (snapTapState.priorityInput & (~HORIZ_MASK | ~hMask) & (~VERT_MASK | ~vMask)) |
                        ((newPressed & HORIZ_MASK) & hMask) |
                        ((newPressed & VERT_MASK) & vMask);
                }

                snapTapState.priorityInput &= curr;

                // 超高速競合解決 - 単一パス処理
                uint32_t finalInput = curr;
                const uint32_t conflictMask =
                    ((hConflict == 0) ? HORIZ_MASK : 0) |
                    ((vConflict == 0) ? VERT_MASK : 0);

                if (__builtin_expect(conflictMask != 0, 0)) {
                    finalInput = (finalInput & ~conflictMask) | (snapTapState.priorityInput & conflictMask);
                }

                snapTapState.lastInputBitmap = curr;

                // 直接配列アクセス - ポインタより高速
                finalState = LUT[finalInput];
            }

            // 究極の入力適用 - コンパイラ自動最適化
            const uint32_t states = finalState;

            // QBitArray超高速更新 - ループ展開 + 最適化

            // 並列実行可能な独立した操作
            mask.setBit(INPUT_UP, !(states & 1));
            mask.setBit(INPUT_DOWN, !(states & 2));
            mask.setBit(INPUT_LEFT, !(states & 4));
            mask.setBit(INPUT_RIGHT, !(states & 8));
        };



        /**
         * 移動入力を処理 v1 remake v2.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // SnapTap状態構造体定義(キャッシュライン最適化)
            alignas(64) static struct {
                uint32_t lastInputBitmap;    // 前回入力ビットマップ保持
                uint32_t priorityInput;      // 優先入力ビットマップ保持
                uint32_t _padding[14];       // 64バイト境界確保
            } snapTapState = { 0, 0, {} };

            // 水平・垂直競合用マスク定数定義
            static constexpr uint32_t HORIZ_MASK = 0xC;   // (1<<2)|(1<<3) - LEFT|RIGHT
            static constexpr uint32_t VERT_MASK = 0x3;   // (1<<0)|(1<<1) - UP|DOWN

            // 超コンパクトLUT - L1キャッシュ効率最大化
            alignas(16) static constexpr uint8_t LUT[16] = {
                0,    // 0000: なし
                1,    // 0001: ↑ 
                2,    // 0010: ↓
                0,    // 0011: ↑↓(キャンセル)
                4,    // 0100: ←
                5,    // 0101: ↑←
                6,    // 0110: ↓←
                4,    // 0111: ←(↑↓キャンセル)
                8,    // 1000: →
                9,    // 1001: ↑→
                10,   // 1010: ↓→
                8,    // 1011: →(↑↓キャンセル)
                0,    // 1100: ←→(キャンセル)
                1,    // 1101: ↑(←→キャンセル)
                2,    // 1110: ↓(←→キャンセル)
                0     // 1111: 全キャンセル
            };

            // 超高速入力取得 - 現代コンパイラが自動最適化
            const uint32_t f = hk[HK_MetroidMoveForward];
            const uint32_t b = hk[HK_MetroidMoveBack];
            const uint32_t l = hk[HK_MetroidMoveLeft];
            const uint32_t r = hk[HK_MetroidMoveRight];

            // 入力ビットマップ生成 - 並列実行最適化
            const uint32_t curr = f | (b << 1) | (l << 2) | (r << 3);

            uint8_t finalState;

            // 分岐予測最適化 - 通常モード優先
            if (__builtin_expect(!isSnapTapMode, 1)) {
                // 通常モード - 直接配列アクセス（最速）
                finalState = LUT[curr];

                // 並列実行可能な独立した操作
                mask.setBit(INPUT_UP, !(finalState & 1));
                mask.setBit(INPUT_DOWN, !(finalState & 2));
                mask.setBit(INPUT_LEFT, !(finalState & 4));
                mask.setBit(INPUT_RIGHT, !(finalState & 8));

                return;
            }

            // SnapTap超高速モード
            const uint32_t newPressed = curr & ~snapTapState.lastInputBitmap;

            // 並列競合判定 - XOR最適化
            const uint32_t hConflict = (curr & HORIZ_MASK) ^ HORIZ_MASK;
            const uint32_t vConflict = (curr & VERT_MASK) ^ VERT_MASK;

            // 条件最適化 - 新入力は稀
            if (__builtin_expect(newPressed != 0, 0)) {
                // 超高速branchless更新 - 単一式統合
                const uint32_t hMask = -(hConflict == 0);
                const uint32_t vMask = -(vConflict == 0);

                snapTapState.priorityInput =
                    (snapTapState.priorityInput & (~HORIZ_MASK | ~hMask) & (~VERT_MASK | ~vMask)) |
                    ((newPressed & HORIZ_MASK) & hMask) |
                    ((newPressed & VERT_MASK) & vMask);
            }

            snapTapState.priorityInput &= curr;

            // 超高速競合解決 - 単一パス処理
            uint32_t finalInput = curr;
            const uint32_t conflictMask =
                ((hConflict == 0) ? HORIZ_MASK : 0) |
                ((vConflict == 0) ? VERT_MASK : 0);

            if (__builtin_expect(conflictMask != 0, 0)) {
                finalInput = (finalInput & ~conflictMask) | (snapTapState.priorityInput & conflictMask);
            }

            snapTapState.lastInputBitmap = curr;

            // 直接配列アクセス - ポインタより高速
            finalState = LUT[finalInput];


            // QBitArray超高速更新 - ループ展開 + 最適化

            // 並列実行可能な独立した操作
            mask.setBit(INPUT_UP, !(finalState & 1));
            mask.setBit(INPUT_DOWN, !(finalState & 2));
            mask.setBit(INPUT_LEFT, !(finalState & 4));
            mask.setBit(INPUT_RIGHT, !(finalState & 8));
        };


        /**
         * 移動入力を処理 v2.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {

            // MaskLUT[16][4]：UP, DOWN, LEFT, RIGHT の有効ビットマスク
            // 1 = 無効（キャンセル） / 0 = 有効（反転型）
            // MaskLUT[curr][UP, DOWN, LEFT, RIGHT]
            alignas(16) static constexpr uint8_t MaskLUT[16][4] = {
                {1,1,1,1}, // 0000: なし
                {0,1,1,1}, // 0001: ↑
                {1,0,1,1}, // 0010: ↓
                {1,1,1,1}, // 0011: ↑↓キャンセル
                {1,1,0,1}, // 0100: ←
                {0,1,0,1}, // 0101: ↑←
                {1,0,0,1}, // 0110: ↓←
                {1,1,0,1}, // 0111: ←（↑↓キャンセル）
                {1,1,1,0}, // 1000: →
                {0,1,1,0}, // 1001: ↑→
                {1,0,1,0}, // 1010: ↓→
                {1,1,1,0}, // 1011: →（↑↓キャンセル）
                {1,1,1,1}, // 1100: ←→キャンセル
                {0,1,1,1}, // 1101: ↑（←→キャンセル）
                {1,0,1,1}, // 1110: ↓（←→キャンセル）
                {1,1,1,1}, // 1111: 全キャンセル
            };

            // SnapTap状態（下位8bit: last、上位8bit: priority）を保持（1レジスタ内）
            static uint16_t snapState = 0;

            // 入力・出力マスク参照の取得（無駄な再アクセス回避）
            // const QBitArray& hk = emuInstance->hotkeyMask;
            // QBitArray& mask = emuInstance->inputMask;

            // 現在の入力状態を4bitでエンコード（1bitずつ独立反映）
            const uint32_t curr =
                (hk[HK_MetroidMoveForward] ? 1 : 0) |
                (hk[HK_MetroidMoveBack] ? 2 : 0) |
                (hk[HK_MetroidMoveLeft] ? 4 : 0) |
                (hk[HK_MetroidMoveRight] ? 8 : 0);

            // SnapTapが無効な場合、即座にLUT参照して方向bit反映（最短4命令）
            if (__builtin_expect(!isSnapTapMode, 1)) {
                mask[INPUT_UP] = MaskLUT[curr][0];
                mask[INPUT_DOWN] = MaskLUT[curr][1];
                mask[INPUT_LEFT] = MaskLUT[curr][2];
                mask[INPUT_RIGHT] = MaskLUT[curr][3];
                return;
            }

            // 前回入力状態の復元（lastInput, priorityInput）
            const uint32_t last = snapState & 0xFF;
            const uint32_t priority = (snapState >> 8) & 0xFF;

            // 新しく押されたキー（priorityに反映候補）
            const uint32_t newPress = curr & ~last;

            // 水平・垂直方向の競合検出（キャンセル対象bitを生成）
            const uint32_t vConflict = ((curr & 0x3) == 0x3) ? 0x3 : 0;
            const uint32_t hConflict = ((curr & 0xC) == 0xC) ? 0xC : 0;
            const uint32_t cMask = vConflict | hConflict;

            // 優先度更新（新たに押された中で競合しているものだけを反映）
            const uint32_t newPriority = ((newPress & cMask) ? ((priority & ~cMask) | (newPress & cMask)) : priority) & curr;

            // 次回用に状態を保存（下位8bit: curr、上位8bit: priority）
            snapState = static_cast<uint16_t>(curr | (newPriority << 8));

            // 競合解決済み最終入力を合成（競合bitだけpriority、それ以外はcurr）
            const uint32_t finalInput = (curr & ~cMask) | (newPriority & cMask);

            // LUTを用いて最終方向マスクを直接反映（即値4命令で終端）
            mask[INPUT_UP] = MaskLUT[finalInput][0];
            mask[INPUT_DOWN] = MaskLUT[finalInput][1];
            mask[INPUT_LEFT] = MaskLUT[finalInput][2];
            mask[INPUT_RIGHT] = MaskLUT[finalInput][3];
        };


        /**
         * 移動入力を処理 v4.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
         /**
          * 移動入力処理 - 最低遅延版
          * @note x86_64向け完全最適化。分岐最小化・メモリアクセス削減
          */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // 反転型マスクLUT（0=有効, 1=無効）
            // インデックス: [Right|Left|Back|Forward]
            // 各バイト: UP, DOWN, LEFT, RIGHT
            alignas(64) static constexpr uint32_t MaskLUT[16] = {
                0x0F0F0F0F, // 0000: なし
                0x0F0F0F0E, // 0001: →
                0x0F0F0E0F, // 0010: ←
                0x0F0F0F0F, // 0011: ←→キャンセル
                0x0F0E0F0F, // 0100: ↓
                0x0F0E0F0E, // 0101: ↓→
                0x0F0E0E0F, // 0110: ↓←
                0x0F0E0F0F, // 0111: ↓
                0x0E0F0F0F, // 1000: ↑
                0x0E0F0F0E, // 1001: ↑→
                0x0E0F0E0F, // 1010: ↑←
                0x0E0F0F0F, // 1011: ↑
                0x0F0F0F0F, // 1100: ↑↓キャンセル
                0x0F0F0F0E, // 1101: →
                0x0F0F0E0F, // 1110: ←
                0x0F0F0F0F  // 1111: 全キャンセル
            };

            // SnapTap状態（16bit: last|priority）
            static uint16_t snapState = 0;

            // 入力状態を4bitエンコード（正しいマッピング）
            const uint32_t curr =
                (-hk[HK_MetroidMoveForward] & 1) |  // Forward = bit0
                (-hk[HK_MetroidMoveBack] & 2) |     // Back = bit1
                (-hk[HK_MetroidMoveLeft] & 4) |     // Left = bit2
                (-hk[HK_MetroidMoveRight] & 8);     // Right = bit3

            // 通常モード：即座にLUT適用
            if (__builtin_expect(!isSnapTapMode, 1)) {
                const uint32_t maskBits = MaskLUT[curr];
                mask[INPUT_UP] = maskBits & 1;
                mask[INPUT_DOWN] = (maskBits >> 8) & 1;
                mask[INPUT_LEFT] = (maskBits >> 16) & 1;
                mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
                return;
            }

            // SnapTapモード：競合解決
            const uint32_t last = snapState & 0xFF;
            const uint32_t priority = snapState >> 8;
            const uint32_t newPress = curr & ~last;

            // 競合検出（Forward/BackとLeft/Rightの組み合わせ）
            const uint32_t hConflict = ((curr & 3) == 3) ? 3 : 0;    // Forward/Back競合
            const uint32_t vConflict = ((curr & 12) == 12) ? 12 : 0; // Left/Right競合
            const uint32_t conflict = vConflict | hConflict;

            // 優先度更新
            const uint32_t newPriority = (newPress & conflict) ?
                ((priority & ~conflict) | (newPress & conflict)) : priority;
            const uint32_t activePriority = newPriority & curr;

            // 状態保存
            snapState = curr | (activePriority << 8);

            // 最終入力計算とLUT適用
            const uint32_t final = (curr & ~conflict) | (activePriority & conflict);
            const uint32_t maskBits = MaskLUT[final];
            mask[INPUT_UP] = maskBits & 1;
            mask[INPUT_DOWN] = (maskBits >> 8) & 1;
            mask[INPUT_LEFT] = (maskBits >> 16) & 1;
            mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
        };


        /**
         * 移動入力を処理 v4.
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
         /**
          * 移動入力処理 - 最低遅延版
          * @note x86_64向け完全最適化。分岐最小化・メモリアクセス削減
          */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // 反転型マスクLUT（0=有効, 1=無効）
            // インデックス: [Right|Left|Back|Forward]
            // 各バイト: UP, DOWN, LEFT, RIGHT
            alignas(64) static constexpr uint32_t MaskLUT[16] = {
                0x0F0F0F0F, // 0000: なし
                0x0F0F0F0E, // 0001: →
                0x0F0F0E0F, // 0010: ←
                0x0F0F0F0F, // 0011: ←→キャンセル
                0x0F0E0F0F, // 0100: ↓
                0x0F0E0F0E, // 0101: ↓→
                0x0F0E0E0F, // 0110: ↓←
                0x0F0E0F0F, // 0111: ↓
                0x0E0F0F0F, // 1000: ↑
                0x0E0F0F0E, // 1001: ↑→
                0x0E0F0E0F, // 1010: ↑←
                0x0E0F0F0F, // 1011: ↑
                0x0F0F0F0F, // 1100: ↑↓キャンセル
                0x0F0F0F0E, // 1101: →
                0x0F0F0E0F, // 1110: ←
                0x0F0F0F0F  // 1111: 全キャンセル
            };

            // SnapTap状態（16bit: last|priority）
            static uint16_t snapState = 0;

            /*
            // 入力状態を4bitエンコード（正しいマッピング）
            const uint32_t curr =
                (-hk[HK_MetroidMoveForward] & 1) |  // Forward = bit0
                (-hk[HK_MetroidMoveBack] & 2) |     // Back = bit1
                (-hk[HK_MetroidMoveLeft] & 4) |     // Left = bit2
                (-hk[HK_MetroidMoveRight] & 8);     // Right = bit3
                */

                // 超高速入力取得 - 4並列ロード
            const uint32_t f = hk[HK_MetroidMoveForward];
            const uint32_t b = hk[HK_MetroidMoveBack];
            const uint32_t l = hk[HK_MetroidMoveLeft];
            const uint32_t r = hk[HK_MetroidMoveRight];

            // ビットマップ生成 - 単純OR演算
            const uint32_t curr = f | (b << 1) | (l << 2) | (r << 3);

            // 通常モード：即座にLUT適用
            if (__builtin_expect(!isSnapTapMode, 1)) {
                const uint32_t maskBits = MaskLUT[curr];
                mask[INPUT_UP] = maskBits & 1;
                mask[INPUT_DOWN] = (maskBits >> 8) & 1;
                mask[INPUT_LEFT] = (maskBits >> 16) & 1;
                mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
                return;
            }

            // SnapTapモード：競合解決
            const uint32_t last = snapState & 0xFF;
            const uint32_t priority = snapState >> 8;
            const uint32_t newPress = curr & ~last;

            // 競合検出（Forward/BackとLeft/Rightの組み合わせ）
            const uint32_t hConflict = ((curr & 3) == 3) ? 3 : 0;    // Forward/Back競合
            const uint32_t vConflict = ((curr & 12) == 12) ? 12 : 0; // Left/Right競合
            const uint32_t conflict = vConflict | hConflict;

            // 優先度更新
            const uint32_t newPriority = (newPress & conflict) ?
                ((priority & ~conflict) | (newPress & conflict)) : priority;
            const uint32_t activePriority = newPriority & curr;

            // 状態保存
            snapState = curr | (activePriority << 8);

            // 最終入力計算とLUT適用
            const uint32_t final = (curr & ~conflict) | (activePriority & conflict);
            const uint32_t maskBits = MaskLUT[final];
            mask[INPUT_UP] = maskBits & 1;
            mask[INPUT_DOWN] = (maskBits >> 8) & 1;
            mask[INPUT_LEFT] = (maskBits >> 16) & 1;
            mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
        };


        /**
         * 移動入力を処理 v5
         *
         * @note x86_64向けに完全最短化。分岐・演算最小化により低サイクル処理を実現.
         * 押しっぱなしでも移動できるようにすること。
         * snapTapモードじゃないときは、左右キーを同時押しで左右移動をストップしないといけない。上下キーも同様。
         * 通常モードの同時押しキャンセルは LUT によってすでに表現されている」
         * snapTapの時は左を押しているときに右を押しても右移動できる。上下も同様。
         */
         /**
          * 移動入力処理 - 最低遅延版
          * @note x86_64向け完全最適化。分岐最小化・メモリアクセス削減
          */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // 反転型マスクLUT（0=有効, 1=無効）
            // 各バイト: UP, DOWN, LEFT, RIGHT
            alignas(64) static constexpr uint32_t MaskLUT[16] = {
                0x0F0F0F0F, // 0000: なし
                0x0F0F0F0E, // 0001: →
                0x0F0F0E0F, // 0010: ←
                0x0F0F0F0F, // 0011: ←→キャンセル
                0x0F0E0F0F, // 0100: ↓
                0x0F0E0F0E, // 0101: ↓→
                0x0F0E0E0F, // 0110: ↓←
                0x0F0E0F0F, // 0111: ↓
                0x0E0F0F0F, // 1000: ↑
                0x0E0F0F0E, // 1001: ↑→
                0x0E0F0E0F, // 1010: ↑←
                0x0E0F0F0F, // 1011: ↑
                0x0F0F0F0F, // 1100: ↑↓キャンセル
                0x0F0F0F0E, // 1101: →
                0x0F0F0E0F, // 1110: ←
                0x0F0F0F0F  // 1111: 全キャンセル
            };

            // SnapTap状態（16bit: last|priority）
            static uint16_t snapState = 0;

            // 超高速入力取得 - 4並列ロード
            const uint32_t f = hk[HK_MetroidMoveForward];
            const uint32_t b = hk[HK_MetroidMoveBack];
            const uint32_t l = hk[HK_MetroidMoveLeft];
            const uint32_t r = hk[HK_MetroidMoveRight];

            // ビットマップ生成
            const uint32_t curr = f | (b << 1) | (l << 2) | (r << 3);

            // 通常モード：即座にLUT適用
            if (__builtin_expect(!isSnapTapMode, 1)) {
                const uint32_t maskBits = MaskLUT[curr];
                mask[INPUT_UP] = maskBits & 1;
                mask[INPUT_DOWN] = (maskBits >> 8) & 1;
                mask[INPUT_LEFT] = (maskBits >> 16) & 1;
                mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
                return;
            }

            // SnapTapモード
            const uint32_t last = snapState & 0xFF;
            const uint32_t priority = snapState >> 8;
            const uint32_t newPress = curr & ~last;

            // XOR最適化による競合検出 XOR最適化でブランチレス
            const uint32_t hConflict = ((curr & 3) ^ 3) ? 0 : 3;
            const uint32_t vConflict = ((curr & 12) ^ 12) ? 0 : 12;
            const uint32_t conflict = vConflict | hConflict;

            // branchless優先度更新
            const uint32_t updateMask = -(newPress & conflict != 0);
            const uint32_t newPriority = (priority & ~(conflict & updateMask)) |
                (newPress & conflict & updateMask);
            const uint32_t activePriority = newPriority & curr;

            // 状態保存
            snapState = curr | (activePriority << 8);

            // 最終入力計算とLUT適用
            const uint32_t final = (curr & ~conflict) | (activePriority & conflict);
            const uint32_t maskBits = MaskLUT[final];
            mask[INPUT_UP] = maskBits & 1;
            mask[INPUT_DOWN] = (maskBits >> 8) & 1;
            mask[INPUT_LEFT] = (maskBits >> 16) & 1;
            mask[INPUT_RIGHT] = (maskBits >> 24) & 1;
        };


#endif

        /**
 * 移動入力処理 v6 最低遅延版.
 *
 *
 * @note x86_64向けに分岐最小化とアクセス回数削減を徹底.
 *       読み取りはtestBitで確定値取得. 書き込みはsetBit/clearBitで確定反映.
 *       snapTapの優先ロジックはビット演算で維持し、水平/垂直の競合は同値判定で分岐レスに処理.
 *       通常モードの同時押しキャンセルは既存LUT表現を厳守.
 *       snapTapでは新規押下が競合時に優先側を上書き保持.
 * .
 */
        static const auto processMoveInput = [&](const QBitArray& hk, QBitArray& mask) __attribute__((hot, always_inline, flatten)) {
            // 反転型マスクLUT定義(0=有効,1=無効 として各出力ビットのLSBを使用するための値で構成)
            // (後続のLUTロード時のキャッシュ効率確保と即時抽出のため)
            alignas(64) static constexpr uint32_t MaskLUT[16] = {
                // 入力ビット配列: [bit0=UP, bit1=DOWN, bit2=LEFT, bit3=RIGHT]
                // 各バイト順序: [UP(byte0), DOWN(byte1), LEFT(byte2), RIGHT(byte3)]
                // 各バイトのLSB(1bit)のみ評価する構成
                // 0000
                0x0F0F0F0F,
                // 0001
                0x0F0F0F0E,
                // 0010
                0x0F0F0E0F,
                // 0011 ←→キャンセル
                0x0F0F0F0F,
                // 0100
                0x0F0E0F0F,
                // 0101
                0x0F0E0F0E,
                // 0110
                0x0F0E0E0F,
                // 0111
                0x0F0E0F0F,
                // 1000
                0x0E0F0F0F,
                // 1001
                0x0E0F0F0E,
                // 1010
                0x0E0F0E0F,
                // 1011
                0x0E0F0F0F,
                // 1100 ↑↓キャンセル
                0x0F0F0F0F,
                // 1101
                0x0F0F0F0E,
                // 1110
                0x0F0F0E0F,
                // 1111 全キャンセル
                0x0F0F0F0F
            };

            // SnapTap状態格納(下位8bit=直近入力, 上位8bit=優先保持)
            // (フレーム間で切り替え優先度を維持するため)
            static uint16_t snapState = 0;

            // 入力読み取り(演算前に確定値取得のため)
            const uint32_t f = hk.testBit(HK_MetroidMoveForward);
            // 入力読み取り(演算前に確定値取得のため)
            const uint32_t b = hk.testBit(HK_MetroidMoveBack);
            // 入力読み取り(演算前に確定値取得のため)
            const uint32_t l = hk.testBit(HK_MetroidMoveLeft);
            // 入力読み取り(演算前に確定値取得のため)
            const uint32_t r = hk.testBit(HK_MetroidMoveRight);

            // 4方向の現在ビット生成(後段LUT索引用ビット圧縮のため)
            const uint32_t curr = (f) | (b << 1) | (l << 2) | (r << 3);

            // 通常モード判定(分岐予測命中率向上のため)
            if (__builtin_expect(!isSnapTapMode, 1)) {
                // LUTロード(即時マスク決定のため)
                const uint32_t mb = MaskLUT[curr];

                // 出力UP確定(LSB評価で分岐レス化のため)
                (mb & 0x00000001) ? mask.setBit(INPUT_UP) : mask.clearBit(INPUT_UP);
                // 出力DOWN確定(LSB評価で分岐レス化のため)
                ((mb >> 8) & 0x01) ? mask.setBit(INPUT_DOWN) : mask.clearBit(INPUT_DOWN);
                // 出力LEFT確定(LSB評価で分岐レス化のため)
                ((mb >> 16) & 0x01) ? mask.setBit(INPUT_LEFT) : mask.clearBit(INPUT_LEFT);
                // 出力RIGHT確定(LSB評価で分岐レス化のため)
                ((mb >> 24) & 0x01) ? mask.setBit(INPUT_RIGHT) : mask.clearBit(INPUT_RIGHT);

                // 早期return(無駄な計算回避のため)
                return;
            }

            // 直近状態抽出(優先制御に使用するため)
            const uint32_t last = snapState & 0xFFu;
            // 優先保持抽出(競合時の優先を保持するため)
            const uint32_t priority = snapState >> 8;

            // 新規押下検出(前フレームとの差分抽出のため)
            const uint32_t newPress = curr & ~last;

            // 水平競合検出(== 3 を分岐レスで表現するため)
            const uint32_t h3 = (curr & 0x3u);
            // 水平競合マスク生成(完全一致時のみ3を返すため)
            const uint32_t hConflict = (h3 ^ 0x3u) ? 0u : 0x3u;

            // 垂直競合検出(== 12 を分岐レスで表現するため)
            const uint32_t v12 = (curr & 0xCu);
            // 垂直競合マスク生成(完全一致時のみ12を返すため)
            const uint32_t vConflict = (v12 ^ 0xCu) ? 0u : 0xCu;

            // 総合競合マスク作成(水平垂直の論理和で一括処理のため)
            const uint32_t conflict = vConflict | hConflict;

            // 競合発生時の更新フラグ生成(新規押下が競合線上に存在する場合のみ1化するため)
            const uint32_t updateMask = -((newPress & conflict) != 0u);

            // 新優先の計算(PR=既存優先を該当軸でクリアし、新規押下を該当軸のみで立てるため)
            const uint32_t newPriority =
                (priority & ~(conflict & updateMask)) | (newPress & conflict & updateMask);

            // 現在入力に限定した優先(押下継続時のみ優先を有効化するため)
            const uint32_t activePriority = newPriority & curr;

            // 状態更新(次フレームの差分検出と優先保持のため)
            snapState = static_cast<uint16_t>((curr & 0xFFu) | ((activePriority & 0xFFu) << 8));

            // 競合軸の最終入力決定(非競合はそのまま、競合は優先側のみ残すため)
            const uint32_t final = (curr & ~conflict) | (activePriority & conflict);

            // LUTロード(確定入力をマスク出力に変換するため)
            const uint32_t mb = MaskLUT[final];

            // 出力UP確定(LSB評価で分岐レス化のため)
            (mb & 0x00000001) ? mask.setBit(INPUT_UP) : mask.clearBit(INPUT_UP);
            // 出力DOWN確定(LSB評価で分岐レス化のため)
            ((mb >> 8) & 0x01) ? mask.setBit(INPUT_DOWN) : mask.clearBit(INPUT_DOWN);
            // 出力LEFT確定(LSB評価で分岐レス化のため)
            ((mb >> 16) & 0x01) ? mask.setBit(INPUT_LEFT) : mask.clearBit(INPUT_LEFT);
            // 出力RIGHT確定(LSB評価で分岐レス化のため)
            ((mb >> 24) & 0x01) ? mask.setBit(INPUT_RIGHT) : mask.clearBit(INPUT_RIGHT);
        };

    // /processMoveInputFunction }




















    /**
     * エイム入力処理(QCursor使用・構造保持・低遅延・ドリフト防止版).
     *
     * 最適化のポイント:
     * 1. キャッシュライン整列構造体によるデータアクセス高速化.
     * 2. adjust処理のマクロ化によるインライン性能向上.
     * 3. 自然な処理順序とホットパス/コールドパスの構造維持.
     */
    static const auto processAimInputOldVer = [&]() __attribute__((hot, always_inline, flatten)) {
#ifndef STYLUS_MODE
        // エイム処理用の構造体(キャッシュライン境界に配置)
        struct alignas(64) {
            int centerX;
            int centerY;
            float sensitivityFactor;
            float dsAspectRatio;
            float combinedSensitivityY;  // 感度とアスペクト比の積
        } static aimData = { 0, 0, 0.01f, 1.3333333f, 0.013333333f };

        // ドリフト防止のための丸め処理マクロ（関数呼び出しのオーバーヘッドを削減） 推定サイクル数: 4-15サイクル

#ifdef COMMENTOUTTTTTTTT



// 最小命令数に逆算された超軽量版 AIM_ADJUST マクロ

        // 調整関数（マクロ化前までの） 10-20サイクル
        static const auto AIM_ADJUST = [](float value) __attribute__((hot, always_inline)) -> int16_t {
            if (value >= 0.5f && value < 1.0f) return static_cast<int16_t>(1.0f);
            if (value <= -0.5f && value > -1.0f) return static_cast<int16_t>(-1.0f);
            return static_cast<int16_t>(value);  // 切り捨て(0方向への丸め)
        };

        // v1.0 検証の結果、以下のような平均処理時間が得られました（各方式とも1万回の変換を100回繰り返した平均値）：条件分岐版（branch）：約 0.0536 秒
#define AIM_ADJUST(v) ((v) >= 0.5f && (v) < 1.0f ? 1 : ((v) <= -0.5f && (v) > -1.0f ? -1 : static_cast<int16_t>(v)))


        // bitwise ver v2 bug fixed. なんか遅延ある。
#define AIM_ADJUST(value) ({ \
    uint32_t __bits; \
    memcpy(&__bits, &(value), sizeof(__bits)); /* float → uint32_t */ \
    uint32_t __abs = __bits & 0x7FFFFFFF; /* 絶対値を取得 */ \
    int16_t __fallback = (int16_t)(value); /* 通常の切り捨て */ \
    int16_t __alt = 1 - ((__bits >> 30) & 2); /* ±1生成 */ \
    (__abs >= 0x3F000000u && __abs < 0x3F800000u) ? __alt : __fallback; \
})



// 最速版3: ルックアップテーブル + ビット操作ハイブリッド 
// 両方の AIM_ADJUST 実装（条件分岐版とビット操作版）は、全てのテスト値（ - 2.0～2.0の範囲で1万件）において完全に一致する結果を返しました。
// つまり、同じ結果になります。ビット操作版は、条件分岐を避けつつも期待される数値変換を正確に再現できています。
// 検証の結果、以下のような平均処理時間が得られました（各方式とも1万回の変換を100回繰り返した平均値）：ビット操作版（bit操作 + union）：約 0.0691 秒
#define AIM_ADJUST(v) ({ \
    union { float f; uint32_t u; } x = {v}; \
    uint32_t exp = (x.u >> 23) & 0xFF; \
    uint32_t sign = x.u >> 31; \
    /* 指数が126（0.5-1.0の範囲）の場合のみ特別処理 */ \
    int16_t result = (int16_t)v; \
    result = (exp == 126) ? (1 - (sign << 1)) : result; \
    result; \
})

// NG! LUTベース。最速だが -0.499付近で誤差あり(この誤差は許容範囲か？) 平均時間（秒）：0.0326
        static const int8_t aim_adjust_table[20] = {
            -1, -1, -1, -1, -1,  // -1.0 ~ -0.6
            0, 0, 0, 0, 0,       // -0.5 ~ -0.1
            0, 0, 0, 0, 0,       // 0.0 ~ 0.4
            1, 1, 1, 1, 1        // 0.5 ~ 0.9
        };
#define AIM_ADJUST(v) ({ \
    float _v = (v); \
    int idx = (int)(_v * 10 + 10); \
    (idx >= 0 && idx < 20) ? aim_adjust_table[idx] : (int16_t)_v; \
})

// テーブル版　	平均時間（秒）：0.0288
namespace AimAdjustTable {
    static constexpr int8_t table[20] = {
        -1, -1, -1, -1, -1,  // -1.0 ~ -0.6
            0,  0,  0,  0,  0,  // -0.5 ~ -0.1
            0,  0,  0,  0,  0,  //  0.0 ~  0.4
            1,  1,  1,  1,  1   //  0.5 ~  0.9
    };

    inline int16_t adjust(float v) {
        int idx = static_cast<int>(v * 10 + 10);
        if (idx >= 0 && idx < 20) {
            return table[idx];
        }
        return static_cast<int16_t>(v);
    }
}


// 最速版4: 条件分岐最小化（2段階判定） 平均時間（秒）：0.0465
#define AIM_ADJUST(v) ({ \
    float _v = (v); \
    float _av = __builtin_fabsf(_v); \
    (_av >= 0.5f && _av < 1.0f) ? ((_v > 0) ? 1 : -1) : (int16_t)_v; \
})

// 正確。	平均時間（秒）：0.0295
#define AIM_ADJUST(v) \
    (({ \
        static constexpr int8_t _table[20] = { \
            -1, -1, -1, -1, -1, \
             0,  0,  0,  0,  0, \
             0,  0,  0,  0,  0, \
             1,  1,  1,  1,  1 \
        }; \
        float _v = (v); \
        int _idx = static_cast<int>(_v * 10 + 10); \
        (_idx >= 0 && _idx < 20) ? _table[_idx] : static_cast<int16_t>(_v); \
    }))
// 正確。	平均時間（秒）：0.0289
#define AIM_ADJUST(v)                          \
    ({                                                  \
        float _v = (v);                                 \
        int _vi = static_cast<int>(_v * 10);            \
        (_vi >= 5 && _vi <= 9) ? 1 :                    \
        (_vi >= -9 && _vi <= -5) ? -1 :                 \
        static_cast<int16_t>(_v);                       \
    })

// v1.0 検証の結果、以下のような平均処理時間が得られました（各方式とも1万回の変換を100回繰り返した平均値）：条件分岐版（branch）：約 0.0536 秒
#define AIM_ADJUST(v) ((v) >= 0.5f && (v) < 1.0f ? 1 : ((v) <= -0.5f && (v) > -1.0f ? -1 : static_cast<int16_t>(v)))


#endif



// 正確。	平均時間（秒）：0.0234
#define AIM_ADJUST(v)                        \
    ({                                                  \
        float _v = (v);                                 \
        int _vi = (int)(_v * 10.001f);                  \
        (_vi >= 5 && _vi <= 9) ? 1 :                    \
        (_vi >= -9 && _vi <= -5) ? -1 :                 \
        static_cast<int16_t>(_v);                       \
    })


// Hot path: when there is focus and no layout change
        if (__builtin_expect(!isLayoutChangePending && wasLastFrameFocused, 1)) {
            // Get current mouse coordinates
            const QPoint currentPos = QCursor::pos();
            const int posX = currentPos.x();
            const int posY = currentPos.y();

            // Calculate mouse movement delta
            const int deltaX = posX - aimData.centerX;
            const int deltaY = posY - aimData.centerY;

            // If there’s no movement, do nothing and exit
            if ((deltaX | deltaY) == 0) return;

            // Reconfigure if sensitivity has changed
            if (__builtin_expect(isSensitivityChangePending, 0)) {
                const int sens = localCfg.GetInt("Metroid.Sensitivity.Aim");
                aimData.sensitivityFactor = sens * 0.01f;
                aimData.combinedSensitivityY = aimData.sensitivityFactor * aimData.dsAspectRatio;
                isSensitivityChangePending = false;
            }

            // Apply sensitivity scaling to movement delta
            const float scaledX = deltaX * aimData.sensitivityFactor;
            const float scaledY = deltaY * aimData.combinedSensitivityY;

            // スケーリング後の座標を調整(ドリフト防止のため)
            const int16_t outputX = AIM_ADJUST(scaledX);
            const int16_t outputY = AIM_ADJUST(scaledY);

            // メモリ書き込み(NDSエミュレータへ送信)
            emuInstance->nds->ARM9Write16(aimXAddr, outputX);
            emuInstance->nds->ARM9Write16(aimYAddr, outputY);

            // AIM動作フラグを有効化
            enableAim = true;

            // カーソルを中央に戻す(後処理として実行)
            QCursor::setPos(aimData.centerX, aimData.centerY);
            return;
        }

        // コールドパス：レイアウト変更または初期化時
        const QPoint center = getAdjustedCenter();
        aimData.centerX = center.x();
        aimData.centerY = center.y();

        // カーソル初期位置を設定
        QCursor::setPos(center);
        isLayoutChangePending = false;

        // 感度も初期化(必要に応じて)
        if (isSensitivityChangePending) {
            const int sens = localCfg.GetInt("Metroid.Sensitivity.Aim");
            aimData.sensitivityFactor = sens * 0.01f;
            aimData.combinedSensitivityY = aimData.sensitivityFactor * aimData.dsAspectRatio;
            isSensitivityChangePending = false;
        }

#else
        // スタイラス入力モード(タッチ処理)
        if (__builtin_expect(emuInstance->isTouching, 1)) {
            emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
        }
        else {
            emuInstance->nds->ReleaseScreen();
        }
#endif
    };


    /**
     * Aim input processing (QCursor-based, structure-preserving, low-latency, drift-prevention version).
     *
     * @note Minimizes hot-path branching and reduces QPoint copying.
     *       Sensitivity recalculation is performed only once via flag monitoring.
     *       AIM_ADJUST is fixed as a safe single-evaluation macro with lightweight ±1 range snapping.
     */
    static const auto processAimInput = [&]() __attribute__((hot, always_inline, flatten)) {
#ifndef STYLUS_MODE
        // Structure definition for aim processing (to improve cache locality)
        struct alignas(64) {
            // Store center X coordinate (maintain origin for delta calculation)
            int centerX;
            // Store center Y coordinate (maintain origin for delta calculation)
            int centerY;
            // Store X-axis sensitivity factor (speed up scaling)
            float sensitivityFactor;
            // Store screen aspect ratio (for Y-axis sensitivity adjustment)
            float dsAspectRatio;
            // Store combined Y-axis sensitivity (reduce multiplication operations)
            float combinedSensitivityY;
        } static aimData = { 0, 0, 0.01f, 1.3333333f, 0.013333333f };

        // Define macro to prevent drift by rounding (single evaluation and snap within tolerance range)
#define AIM_ADJUST(v)                                        \
        ({                                                       \
            /* Store input value (to prevent multiple evaluations) */ \
            float _v = (v);                                      \
            /* Convert to int scaled by 10 (make threshold comparison constant-domain) */ \
            int _vi = static_cast<int>(_v * 10.001f);            \
            /* Snap positive range (fix ±1 for values in 0.5–0.9 range) */ \
            (_vi >= 5 && _vi <= 9) ? 1 :                         \
            /* Snap negative range (fix ±1 for values in -0.9–-0.5 range) */ \
            (_vi >= -9 && _vi <= -5) ? -1 :                      \
            /* Otherwise truncate (to suppress drift) */         \
            static_cast<int16_t>(_v);                            \
        })

// Hot path branch (fast processing when focus is maintained and layout is unchanged)

        if (__builtin_expect(!isLayoutChangePending && wasLastFrameFocused, 1)) {
            // Get current mouse coordinates (for delta calculation input)
            const QPoint currentPos = QCursor::pos();
            // Extract X coordinate (early retrieval from QPoint)
            const int posX = currentPos.x();
            // Extract Y coordinate (early retrieval from QPoint)
            const int posY = currentPos.y();

            // Calculate X delta (preserve raw value before scaling)
            const int deltaX = posX - aimData.centerX;
            // Calculate Y delta (preserve raw value before scaling)
            const int deltaY = posY - aimData.centerY;

            // Early exit if no movement (prevent unnecessary processing)
            if ((deltaX | deltaY) == 0) return;

            // Check if sensitivity needs update (ensure single recalculation)
            if (__builtin_expect(isSensitivityChangePending, 0)) {
                // Retrieve sensitivity value (apply updated settings)
                const int sens = localCfg.GetInt("Metroid.Sensitivity.Aim");
                // Update X sensitivity (refresh scaling factor)
                aimData.sensitivityFactor = sens * 0.01f;
                // Update combined Y sensitivity (reduce multiplications)
                aimData.combinedSensitivityY = aimData.sensitivityFactor * aimData.dsAspectRatio;
                // Clear flag (prevent duplicate recalculation)
                isSensitivityChangePending = false;
            }

            // Calculate X scaling (apply sensitivity)
            const float scaledX = deltaX * aimData.sensitivityFactor;
            // Calculate Y scaling (apply sensitivity)
            const float scaledY = deltaY * aimData.combinedSensitivityY;

            // Calculate X output adjustment (prevent drift and snap to ±1)
            const int16_t outputX = AIM_ADJUST(scaledX);
            // Calculate Y output adjustment (prevent drift and snap to ±1)
            const int16_t outputY = AIM_ADJUST(scaledY);

            // Write X register (update aim on NDS side)
            emuInstance->nds->ARM9Write16(aimXAddr, outputX);
            // Write Y register (update aim on NDS side)
            emuInstance->nds->ARM9Write16(aimYAddr, outputY);

            // Set aim enable flag (for conditional processing downstream)
            enableAim = true;

            // Return cursor to center (keep next delta calculation zero-based)
            QCursor::setPos(aimData.centerX, aimData.centerY);
            // End processing (avoid unnecessary branching)
            return;
        }

        // Recalculate center coordinates (for layout changes and initialization)
        const QPoint center = getAdjustedCenter();
        // Update center X (set origin for next delta calculation)
        aimData.centerX = center.x();
        // Update center Y (set origin for next delta calculation)
        aimData.centerY = center.y();

        // Set initial cursor position (for visual consistency and zeroing delta)
        QCursor::setPos(center);
        // Clear layout change flag (to return to hot path)
        isLayoutChangePending = false;

        // Sensitivity initialization branch (to immediately apply config changes)
        if (isSensitivityChangePending) {
            // Retrieve sensitivity value (to apply settings)
            const int sens = localCfg.GetInt("Metroid.Sensitivity.Aim");
            // Update X sensitivity (set scaling factor)
            aimData.sensitivityFactor = sens * 0.01f;
            // Update combined Y sensitivity (reduce multiplication operations)
            aimData.combinedSensitivityY = aimData.sensitivityFactor * aimData.dsAspectRatio;
            // Clear flag (prevent redundant recalculation)
            isSensitivityChangePending = false;
        }

#else
        // スタイラス押下分岐(タッチ入力直通処理のため)
        if (__builtin_expect(emuInstance->isTouching, 1)) {
            // タッチ送出(座標反映のため)
            emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
        }
        // 非押下分岐(タッチ解放反映のため)
        else {
            // 画面解放(入力状態リセットのため)
            emuInstance->nds->ReleaseScreen();
        }
#endif
    };



    // Define a lambda function to switch weapons
    static const auto SwitchWeapon = [&](int weaponIndex) __attribute__((hot, always_inline)) {

        // Check for Already equipped
        if (emuInstance->nds->ARM9Read8(selectedWeaponAddr) == weaponIndex) {
            // emuInstance->osdAddMessage(0, "Weapon switch unnecessary: Already equipped");
            return; // Early return if the weapon is already equipped
        }

        if (isInAdventure) {

            // Check isMapOrUserActionPaused, for the issue "If you switch weapons while the map is open, the aiming mechanism may become stuck."
            if (isPaused) {
                return;
            }
            else if (emuInstance->nds->ARM9Read8(isInVisorOrMapAddr) == 0x1) {
                // isInVisor

                // Prevent visual glitches during weapon switching in visor mode
                return;
            }
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
            // Leave the upper 4 bits of currentJumpFlags as they are and set the lower 4 bits to 0x01
            emuInstance->nds->ARM9Write8(jumpFlagAddr, (currentJumpFlags & 0xF0) | 0x01);
            isRestoreNeeded = true;
            //emuInstance->osdAddMessage(0, ("JumpFlag:" + std::string(1, "0123456789ABCDEF"[emuInstance->nds->ARM9Read8(jumpFlagAddr) & 0x0F])).c_str());
            //emuInstance->osdAddMessage(0, "Done setting jumpFlag.");
        }

        // Leave the upper 4 bits of WeaponChangeAddr as they are, and set only the lower 4 bits to 1011 (B in hexadecimal)
        emuInstance->nds->ARM9Write8(weaponChangeAddr, (emuInstance->nds->ARM9Read8(weaponChangeAddr) & 0xF0) | 0x0B); // Only change the lower 4 bits to B

        // Change the weapon
        emuInstance->nds->ARM9Write8(selectedWeaponAddr, weaponIndex);  // Write the address of the corresponding weapon

        // Release the screen (for weapon change)
        emuInstance->nds->ReleaseScreen();

        // Advance frames (for reflection of ReleaseScreen, WeaponChange)
        frameAdvanceTwice();

        // Need Touch after ReleaseScreen for aiming.
#ifndef STYLUS_MODE
        emuInstance->nds->TouchScreen(128, 88);
#else
        if (emuInstance->isTouching) {
            emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
        }
#endif

        // Advance frames (for reflection of Touch. This is necessary for no jump)
        frameAdvanceTwice();

        // Restore the jump flag to its original value (if necessary)
        if (isRestoreNeeded) {
            currentJumpFlags = emuInstance->nds->ARM9Read8(jumpFlagAddr);
            // Create and set a new value by combining the upper 4 bits of currentJumpFlags and the lower 4 bits of jumpFlag
            emuInstance->nds->ARM9Write8(jumpFlagAddr, (currentJumpFlags & 0xF0) | jumpFlag);
            //emuInstance->osdAddMessage(0, ("JumpFlag:" + std::string(1, "0123456789ABCDEF"[emuInstance->nds->ARM9Read8(jumpFlagAddr) & 0x0F])).c_str());
            //emuInstance->osdAddMessage(0, "Restored jumpFlag.");

        }

        };

    static const QBitArray& hotkeyMask = emuInstance->hotkeyMask;
    static QBitArray& inputMask = emuInstance->inputMask;
    static const QBitArray& hotkeyPress = emuInstance->hotkeyPress;

#define TOUCH_IF(PRESS, X, Y) if (hotkeyPress.testBit(PRESS)) { emuInstance->nds->ReleaseScreen(); frameAdvanceTwice(); emuInstance->nds->TouchScreen(X, Y); frameAdvanceTwice(); }


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
        // No "else" here, cuz flag will be changed after detecting.

        if (__builtin_expect(isRomDetected, 1)) {
            isInGame = emuInstance->nds->ARM9Read16(inGameAddr) == 0x0001;

            // Determine whether it is cursor mode in one place
            bool shouldBeCursorMode = !isInGame || (isInAdventure && isPaused);

            if (isInGame && !hasInitialized) {
                // Run once at game start

                // Set the initialization complete flag
                hasInitialized = true;

                // updateRenderer because of using softwareRenderer when not in Game.
                videoRenderer = emuInstance->getGlobalConfig().GetInt("3D.Renderer");
                updateRenderer();

                /* MelonPrimeDS test
                if (vsyncFlag) {
                    emuInstance->osdAddMessage(0, "Vsync is enabled.");
                }
                else {
                    emuInstance->osdAddMessage(0, "Vsync is disabled.");
                }
                */

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

                if (__builtin_expect(isInGame, 1)) {
                    // inGame

                    /*
                    * doing this in Screen.cpp
                    if(!wasLastFrameFocused){
                        showCursorOnMelonPrimeDS(false);
                    }
                    */

                    if (!isCursorMode) {
                        processAimInput();
                    }

                    // Move hunter
                    processMoveInput(hotkeyMask, inputMask);

                    // Shoot
                    inputMask.setBit(INPUT_L, !(hotkeyMask.testBit(HK_MetroidShootScan) | hotkeyMask.testBit(HK_MetroidScanShoot)));

                    // Zoom, map zoom out
                    inputMask.setBit(INPUT_R, !hotkeyMask.testBit(HK_MetroidZoom));

                    // Jump
                    inputMask.setBit(INPUT_B, !hotkeyMask.testBit(HK_MetroidJump));

                    // Alt-form
                    TOUCH_IF(HK_MetroidMorphBall, 231, 167) 

                    // Compile-time constants
                    static constexpr uint8_t WEAPON_ORDER[] = { 0, 2, 7, 6, 5, 4, 3, 1, 8 };
                    static constexpr uint16_t WEAPON_MASKS[] = { 0x001, 0x004, 0x080, 0x040, 0x020, 0x010, 0x008, 0x002, 0x100 };
                    static constexpr uint8_t MIN_AMMO[] = { 0, 0x5, 0xA, 0x4, 0x14, 0x5, 0xA, 0xA, 0 };
                    static constexpr uint8_t WEAPON_INDEX_MAP[9] = { 0, 7, 1, 6, 5, 4, 3, 2, 8 };
                    static constexpr uint8_t WEAPON_COUNT = 9;

                    // Hotkey mapping
                    static constexpr struct {
                        int hotkey;
                        uint8_t weapon;
                    } HOTKEY_MAP[] = {
                        { HK_MetroidWeaponBeam, 0 },
                        { HK_MetroidWeaponMissile, 2 },
                        { HK_MetroidWeapon1, 7 },
                        { HK_MetroidWeapon2, 6 },
                        { HK_MetroidWeapon3, 5 },
                        { HK_MetroidWeapon4, 4 },
                        { HK_MetroidWeapon5, 3 },
                        { HK_MetroidWeapon6, 1 },
                        { HK_MetroidWeaponSpecial, 0xFF }
                    };

                    // Branch prediction hints
                    #define likely(x)   __builtin_expect(!!(x), 1)
                    #define unlikely(x) __builtin_expect(!!(x), 0)

                    // Main lambda for weapon switching
                    static const auto processWeaponSwitch = [&]() -> bool {
                        bool weaponSwitched = false;

                        // Lambda: Gather hotkey states efficiently
                        static const auto gatherHotkeyStates = [&]() -> uint32_t {
                            uint32_t states = 0;
                            for (size_t i = 0; i < 9; ++i) {
                                // if (hotkeyPressed(HOTKEY_MAP[i].hotkey)) {
                                //if (hotkeyPress.testBit(HOTKEY_MAP[i].hotkey)) {
                                //    states |= (1u << i);
                                //}
                                states |= static_cast<uint32_t>(hotkeyPress.testBit(HOTKEY_MAP[i].hotkey)) << i;
                            }
                            return states;
                            };

                        // Lambda: Process hotkeys
                        static const auto processHotkeys = [&](uint32_t hotkeyStates) -> bool {
                            if (!hotkeyStates) return false;

                            const int firstSet = __builtin_ctz(hotkeyStates);

                            if (unlikely(firstSet == 8)) {
                                const uint8_t special = emuInstance->nds->ARM9Read8(loadedSpecialWeaponAddr);
                                if (special != 0xFF) {
                                    SwitchWeapon(special);
                                    return true;
                                }
                            }
                            else {
                                SwitchWeapon(HOTKEY_MAP[firstSet].weapon);
                                return true;
                            }
                            return false;
                            };

                        // Lambda: Calculate available weapons
                        static const auto getAvailableWeapons = [&]() -> uint16_t {
                            // Batch read weapon data
                            const uint16_t having = emuInstance->nds->ARM9Read16(havingWeaponsAddr);
                            const uint32_t ammoData = emuInstance->nds->ARM9Read32(weaponAmmoAddr);
                            const uint16_t missileAmmo = ammoData >> 16;
                            const uint16_t weaponAmmo = ammoData & 0xFFFF;

                            uint16_t available = 0;

                            // Check each weapon
                            for (int i = 0; i < WEAPON_COUNT; ++i) {
                                const uint8_t weapon = WEAPON_ORDER[i];
                                const uint16_t mask = WEAPON_MASKS[i];

                                // Check if owned
                                if (!(having & mask)) continue;

                                // Check ammo requirements
                                bool hasAmmo = true;
                                if (weapon == 2) {
                                    hasAmmo = missileAmmo >= 0xA;
                                }
                                else if (weapon > 2 && weapon < 8) {
                                    uint8_t required = MIN_AMMO[weapon];
                                    if (weapon == 3 && isWeavel) {
                                        required = 0x5;
                                    }
                                    hasAmmo = weaponAmmo >= required;
                                }

                                if (hasAmmo) {
                                    available |= (1u << i);
                                }
                            }

                            return available;
                            };

                        // Lambda: Find next available weapon
                        static const auto findNextWeapon = [&](uint8_t current, bool forward, uint16_t available) -> int {
                            if (!available) return -1;  // No weapons available

                            uint8_t startIndex = WEAPON_INDEX_MAP[current];
                            uint8_t index = startIndex;

                            // Search for next available weapon
                            for (int attempts = 0; attempts < WEAPON_COUNT; ++attempts) {
                                if (forward) {
                                    index = (index + 1) % WEAPON_COUNT;
                                }
                                else {
                                    index = (index + WEAPON_COUNT - 1) % WEAPON_COUNT;
                                }

                                if (available & (1u << index)) {
                                    return WEAPON_ORDER[index];
                                }
                            }

                            return -1;  // Should not reach here if available != 0
                            };

                        // Lambda: Process wheel and navigation keys
                        static const auto processWheelInput = [&]() -> bool {
                            const int wheelDelta = emuInstance->getMainWindow()->panel->getDelta();
                            const bool nextKey = hotkeyPress.testBit(HK_MetroidWeaponNext);
                            const bool prevKey = hotkeyPress.testBit(HK_MetroidWeaponPrevious);

                            if (!wheelDelta && !nextKey && !prevKey) return false;

                            const bool forward = (wheelDelta < 0) || nextKey;
                            const uint8_t current = emuInstance->nds->ARM9Read8(currentWeaponAddr);
                            const uint16_t available = getAvailableWeapons();

                            int nextWeapon = findNextWeapon(current, forward, available);
                            if (nextWeapon >= 0) {
                                SwitchWeapon(static_cast<uint8_t>(nextWeapon));
                                return true;
                            }

                            return false;
                            };

                        // Main execution flow
                        const uint32_t hotkeyStates = gatherHotkeyStates();

                        // Try hotkeys first
                        if (processHotkeys(hotkeyStates)) {
                            return true;
                        }

                        // Try wheel/navigation if no hotkey was pressed
                        if (processWheelInput()) {
                            return true;
                        }

                        return false;
                        };

                    // Execute the weapon switch logic
                    bool weaponSwitched = processWeaponSwitch();


                    // Weapon check {
                    /*
                    // TODO 終了時の武器選択を防ぐ。　マグモールになってしまう
                    static bool isWeaponCheckActive = false;

                    if (emuInstance->hotkeyDown(HK_MetroidWeaponCheck)) {
                        if (!isWeaponCheckActive) {
                            // 初回のみ実行
                            isWeaponCheckActive = true;
                            emuInstance->nds->ReleaseScreen();
                            frameAdvanceTwice();
                        }

                        // キーが押されている間は継続
                        emuInstance->nds->TouchScreen(236, 30);
                        frameAdvanceTwice();

                        // still allow movement
                        processMoveInput(hotkeyMask, inputMask);
                    }
                    else {
                        if (isWeaponCheckActive) {
                            // キーが離されたときの終了処理
                            emuInstance->nds->ReleaseScreen();
                            frameAdvanceTwice();
                            frameAdvanceTwice();
                            isWeaponCheckActive = false;
                        }
                    }
                    */
                    // } Weapon check


                    // Morph ball boost
                    // INFO If this function is not used, mouse boosting can only be done once.
                    // This is because it doesn't release from the touch state, which is necessary for aiming. 
                    // There's no way around it.
                    if (isSamus && hotkeyMask.testBit(HK_MetroidHoldMorphBallBoost))
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

                            // Boost input determination (true during boost, false during charge)
                            inputMask.setBit(INPUT_R, (!isBoosting && isBoostGaugeEnough));

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

                    if (__builtin_expect(isInAdventure, 0)) {
                        // Adventure Mode Functions

                        // To determine the state of pause or user operation stop (to detect the state of map or action pause)
                        isPaused = emuInstance->nds->ARM9Read8(isMapOrUserActionPausedAddr) == 0x1;

                        // Scan Visor
                        if (hotkeyPress.testBit(HK_MetroidScanVisor)) {
                            emuInstance->nds->ReleaseScreen();
                            frameAdvanceTwice();

                            // emuInstance->osdAddMessage(0, "in visor %d", inVisor);

                            emuInstance->nds->TouchScreen(128, 173);

                            if (emuInstance->nds->ARM9Read8(isInVisorOrMapAddr) == 0x1) {
                                // isInVisor
                                frameAdvanceTwice();
                            }
                            else {
                                for (int i = 0; i < 30; i++) {
                                    // still allow movement whilst we're enabling scan visor
                                    processAimInput();
                                    processMoveInput(hotkeyMask, inputMask); 

                                    //emuInstance->nds->SetKeyMask(emuInstance->getInputMask());
                                    emuInstance->nds->SetKeyMask(GET_INPUT_MASK(inputMask));
                                    
                                    frameAdvanceOnce();
                                }
                            }

                            emuInstance->nds->ReleaseScreen();
                            frameAdvanceTwice();
                        }

                        TOUCH_IF(HK_MetroidUIOk, 128, 142) // OK (in scans and messages)
                        TOUCH_IF(HK_MetroidUILeft, 71, 141) // Left arrow (in scans and messages)
                        TOUCH_IF(HK_MetroidUIRight, 185, 141) // Right arrow (in scans and messages)
                        TOUCH_IF(HK_MetroidUIYes, 96, 142)  // Enter to Starship
                        TOUCH_IF(HK_MetroidUINo, 160, 142) // No Enter to Starship

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

                    isInAdventure = false;

                    if (hasInitialized) {
                        hasInitialized = false;
                    }

                    // Resolve Menu flickering
                    if (videoRenderer != renderer3D_Software) {
                        videoRenderer = renderer3D_Software;
                        updateRenderer();
                    }

                    // L For Hunter License
                    inputMask.setBit(INPUT_L, !hotkeyPress.testBit(HK_MetroidUILeft));
                    // R For Hunter License
                    inputMask.setBit(INPUT_R, !hotkeyPress.testBit(HK_MetroidUIRight));

                    if (__builtin_expect(isRomDetected, 1)) {

                        if (__builtin_expect(!isUnlockMapsHuntersApplied, 1)) {
                            if (emuInstance->getLocalConfig().GetBool("Metroid.Data.Unlock")) {
                                // 1回だけ適用
                                emuInstance->nds->ARM9Write8(unlockMapsHuntersAddr, 0x27);
                                emuInstance->nds->ARM9Write32(unlockMapsHuntersAddr2, 0x07FFFFFF);
                                emuInstance->nds->ARM9Write8(unlockMapsHuntersAddr3, 0x7F);
                                emuInstance->nds->ARM9Write32(unlockMapsHuntersAddr4, 0xFFFFFFFF);
                                emuInstance->nds->ARM9Write8(unlockMapsHuntersAddr5, 0xFF);

                                // フラグ更新（以降チェック不要にする）
                                isUnlockMapsHuntersApplied = true;
                                /*
                                auto& localCfg = emuInstance->getLocalConfig();
                                localCfg.SetBool("Metroid.Data.Unlock", false);
                                Config::Save();
                                */
                                // emuInstance->osdAddMessage(0, "Unlock All Hunters/Maps applied.");
                            }
                        }
                    }

                }

                if (shouldBeCursorMode != isCursorMode) {
                    isCursorMode = shouldBeCursorMode;
#ifndef STYLUS_MODE
                    showCursorOnMelonPrimeDS(isCursorMode);
#endif
                }

                if (isCursorMode) {

                    if (emuInstance->isTouching) {
                        emuInstance->nds->TouchScreen(emuInstance->touchX, emuInstance->touchY);
                    }
                    else {
                        emuInstance->nds->ReleaseScreen();
                    }

                }

                // Start / View Match progress, points / Map(Adventure)
                inputMask.setBit(INPUT_START, !hotkeyMask.testBit(HK_MetroidMenu));


            }// END of if(isFocused)
            
            // Apply input
            // emuInstance->nds->SetKeyMask(emuInstance->getInputMask());
            emuInstance->nds->SetKeyMask(GET_INPUT_MASK(inputMask));

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
                if (isInGame) {
                    // updateRenderer because of using softwareRenderer when not in Game.
                    videoRenderer = emuInstance->getGlobalConfig().GetInt("3D.Renderer");
                    updateRenderer();
                }

                // reset Settings when unPaused
                isSnapTapMode = emuInstance->getLocalConfig().GetBool("Metroid.Operation.SnapTap");
                isUnlockMapsHuntersApplied = false;

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
    // MelonPrimeDS {

    // getVsyncFlag
    bool vsyncFlag = emuInstance->getGlobalConfig().GetBool("Screen.VSync");  // MelonPrimeDS
    // VSync Override
    emuInstance->setVSyncGL(vsyncFlag); // MelonPrimeDS

	// } MelonPrimeDS

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
    // MelonPrimeDS {
    // VSync Override
    emuInstance->setVSyncGL(vsyncFlag); // MelonPrimeDS

    /* MelonPrimeDS test
    if (vsyncFlag) {
        emuInstance->osdAddMessage(0, "Vsync is enabled.");
    }
    else {
        emuInstance->osdAddMessage(0, "Vsync is disabled.");
    }
    */

    // } MelonPrimeDS
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
