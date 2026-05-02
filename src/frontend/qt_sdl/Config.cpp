/*
    Copyright 2016-2026 melonDS team

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
#include <inttypes.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include "toml/toml.hpp"

#include "Platform.h"
#include "Config.h"
#include "ScreenLayout.h"
#include "main.h"

using namespace std::string_literals;


namespace Config
{
    using namespace melonDS;


    const char* kConfigFile = "melonDS.toml";

    const char* kLegacyConfigFile = "melonDS.ini";
    const char* kLegacyUniqueConfigFile = "melonDS.%d.ini";

    toml::value RootTable;

    DefaultList<int> DefaultInts =
    {
        {"Instance*.Keyboard", -1},
        {"Instance*.Keyboard.HK_FullscreenToggle", Qt::Key_F11},
        {"Instance*.Joystick", -1},
        {"Instance*.Window*.Width", 256},
        {"Instance*.Window*.Height", 384},
        {"Screen.VSyncInterval", 1},
    #ifdef MELONPRIME_DS
        {"3D.Renderer", renderer3D_OpenGL}, // melonPrimeDS defaults
        {"3D.GL.ScaleFactor", 4},           // melonPrimeDS defaults
    #else
        {"3D.Renderer", renderer3D_Software},
        {"3D.GL.ScaleFactor", 1},
    #endif
    #ifdef JIT_ENABLED
        {"JIT.MaxBlockSize", 32},
    #endif
        {"Instance*.Firmware.Language", 1},
        {"Instance*.Firmware.BirthdayMonth", 1},
        {"Instance*.Firmware.BirthdayDay", 1},
        {"MP.AudioMode", 1},
        {"MP.RecvTimeout", 25},
        {"Instance*.Audio.Volume", 256},
        {"Mic.InputType", 1},
        {"Mouse.HideSeconds", 5},
        {"Instance*.DSi.Battery.Level", 0xF},
    #ifdef GDBSTUB_ENABLED
        {"Instance*.Gdb.ARM7.Port", 3334},
        {"Instance*.Gdb.ARM9.Port", 3333},
    #endif
        {"LAN.HostNumPlayers", 16},

    #ifdef MELONPRIME_DS
        /* MelonPrimeDS { */ // Sensitivity & Custom Hotkeys
        {"Instance*.Metroid.Sensitivity.Aim", 63},
        {"Instance*.Metroid.Volume.SFX", 9},
        {"Instance*.Metroid.Volume.Music", 9},
        {"Instance*.Metroid.Screen.SyncMode", 0},
        {"Instance*.Metroid.HunterLicense.Hunter.Selected", 0},
        {"Instance*.Metroid.HunterLicense.Color.Selected", 0},
        {"Instance*.Keyboard.HK_MetroidMoveForward",       Qt::Key_W},
        {"Instance*.Keyboard.HK_MetroidMoveBack",          Qt::Key_S},
        {"Instance*.Keyboard.HK_MetroidMoveLeft",          Qt::Key_A},
        {"Instance*.Keyboard.HK_MetroidMoveRight",         Qt::Key_D},
        {"Instance*.Keyboard.HK_MetroidJump",              Qt::Key_Space},
        {"Instance*.Keyboard.HK_MetroidMorphBall",         Qt::Key_Control},
        {"Instance*.Keyboard.HK_MetroidZoom",              (int)Qt::RightButton | (int)0xF0000000},
        {"Instance*.Keyboard.HK_MetroidHoldMorphBallBoost",Qt::Key_Shift},
        {"Instance*.Keyboard.HK_MetroidScanVisor",         Qt::Key_C},
        {"Instance*.Keyboard.HK_MetroidUILeft",            Qt::Key_Z},
        {"Instance*.Keyboard.HK_MetroidUIRight",           Qt::Key_X},
        {"Instance*.Keyboard.HK_MetroidUIOk",              Qt::Key_F},
        {"Instance*.Keyboard.HK_MetroidUIYes",             Qt::Key_G},
        {"Instance*.Keyboard.HK_MetroidUINo",              Qt::Key_H},
        {"Instance*.Keyboard.HK_MetroidShootScan",         (int)Qt::LeftButton | (int)0xF0000000},
        {"Instance*.Keyboard.HK_MetroidScanShoot",         Qt::Key_V},
        {"Instance*.Keyboard.HK_MetroidWeaponBeam",        (int)Qt::ExtraButton2 | (int)0xF0000000},
        {"Instance*.Keyboard.HK_MetroidWeaponMissile",     (int)Qt::ExtraButton1 | (int)0xF0000000},
        {"Instance*.Keyboard.HK_MetroidWeaponSpecial",     Qt::Key_R},
        {"Instance*.Keyboard.HK_MetroidWeaponNext",        Qt::Key_J},
        {"Instance*.Keyboard.HK_MetroidWeaponPrevious",    Qt::Key_K},
        {"Instance*.Keyboard.HK_MetroidWeapon1",           Qt::Key_1},
        {"Instance*.Keyboard.HK_MetroidWeapon2",           Qt::Key_2},
        {"Instance*.Keyboard.HK_MetroidWeapon3",           Qt::Key_3},
        {"Instance*.Keyboard.HK_MetroidWeapon4",           Qt::Key_4},
        {"Instance*.Keyboard.HK_MetroidWeapon5",           Qt::Key_5},
        {"Instance*.Keyboard.HK_MetroidWeapon6",           Qt::Key_6},
        {"Instance*.Keyboard.HK_MetroidWeaponCheck",       Qt::Key_Y},
        {"Instance*.Keyboard.HK_MetroidMenu",              Qt::Key_Tab},
        {"Instance*.Keyboard.HK_MetroidIngameSensiUp",     Qt::Key_PageUp},
        {"Instance*.Keyboard.HK_MetroidIngameSensiDown",   Qt::Key_PageDown},
        /* MelonPrimeDS } */

        /* MelonPrimeDS Custom HUD Crosshair defaults { */
        {"Instance*.Metroid.Visual.HudTextScale",           100},
        {"Instance*.Metroid.Visual.HudAutoScaleCap",       800},
        {"Instance*.Metroid.Visual.HudAutoScaleCapText",   500},
        {"Instance*.Metroid.Visual.HudAutoScaleCapIcons",  800},
        {"Instance*.Metroid.Visual.HudAutoScaleCapGauges", 800},
        {"Instance*.Metroid.Visual.HudAutoScaleCapCrosshair", 800},
        {"Instance*.Metroid.Visual.HudHpAnchor",           7},
        {"Instance*.Metroid.Visual.HudHpX",               -71},
        {"Instance*.Metroid.Visual.HudHpY",              -34},
        {"Instance*.Metroid.Visual.HudHpAlign",            2},
        {"Instance*.Metroid.Visual.HudHpTextColorR",      255},
        {"Instance*.Metroid.Visual.HudHpTextColorG",      255},
        {"Instance*.Metroid.Visual.HudHpTextColorB",      255},
        {"Instance*.Metroid.Visual.HudWeaponAnchor",       7},
        {"Instance*.Metroid.Visual.HudWeaponX",          8},
        {"Instance*.Metroid.Visual.HudWeaponY",          -17},
        {"Instance*.Metroid.Visual.HudAmmoAlign",          0},
        {"Instance*.Metroid.Visual.HudAmmoTextColorR",    255},
        {"Instance*.Metroid.Visual.HudAmmoTextColorG",    255},
        {"Instance*.Metroid.Visual.HudAmmoTextColorB",    255},
        {"Instance*.Metroid.Visual.HudWeaponLayout",       0},
        {"Instance*.Metroid.Visual.HudWeaponIconOffsetX",  0},
        {"Instance*.Metroid.Visual.HudWeaponIconOffsetY", 0},
        {"Instance*.Metroid.Visual.HudWeaponIconMode",     1},
        {"Instance*.Metroid.Visual.HudWeaponIconPosAnchor", 7},
        {"Instance*.Metroid.Visual.HudWeaponIconPosX",   0},
        {"Instance*.Metroid.Visual.HudWeaponIconPosY",   -21},
        {"Instance*.Metroid.Visual.HudWeaponIconAnchorX",  1},
        {"Instance*.Metroid.Visual.HudWeaponIconAnchorY",  1},
        // Per-weapon icon color overlays
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorRPowerBeam",    255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorGPowerBeam",    255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorBPowerBeam",    255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorRVoltDriver",   255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorGVoltDriver",   255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorBVoltDriver",   255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorRMissile",      255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorGMissile",      255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorBMissile",      255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorRBattleHammer", 255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorGBattleHammer", 255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorBBattleHammer", 255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorRImperialist",  255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorGImperialist",  255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorBImperialist",  255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorRJudicator",    255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorGJudicator",    255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorBJudicator",    255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorRMagmaul",      255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorGMagmaul",      255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorBMagmaul",      255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorRShockCoil",    255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorGShockCoil",    255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorBShockCoil",    255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorROmegaCannon",  255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorGOmegaCannon",  255},
        {"Instance*.Metroid.Visual.HudWeaponIconOverlayColorBOmegaCannon",  255},
        {"Instance*.Metroid.Visual.HudWeaponIconHeight",    10},
        {"Instance*.Metroid.Visual.HudWeaponInventoryAnchor",       8},      // BR
        {"Instance*.Metroid.Visual.HudWeaponInventoryX",            -29},
        {"Instance*.Metroid.Visual.HudWeaponInventoryY",            -101},
        {"Instance*.Metroid.Visual.HudWeaponInventoryAlign",        1},      // Left
        {"Instance*.Metroid.Visual.HudWeaponInventoryOrientation",  1},      // Vertical
        {"Instance*.Metroid.Visual.HudWeaponInventoryIconHeight",   6},
        {"Instance*.Metroid.Visual.HudWeaponInventorySpacing",      3},
        {"Instance*.Metroid.Visual.HudWeaponInventoryColorR",       255},
        {"Instance*.Metroid.Visual.HudWeaponInventoryColorG",       255},
        {"Instance*.Metroid.Visual.HudWeaponInventoryColorB",       255},
        {"Instance*.Metroid.Visual.HudWeaponInventoryOutlineColorR",        0},
        {"Instance*.Metroid.Visual.HudWeaponInventoryOutlineColorG",        0},
        {"Instance*.Metroid.Visual.HudWeaponInventoryOutlineColorB",        0},
        {"Instance*.Metroid.Visual.HudWeaponInventoryOutlineThickness",     1},
        {"Instance*.Metroid.Visual.HudWeaponInventoryIconOutlineColorR",    0},
        {"Instance*.Metroid.Visual.HudWeaponInventoryIconOutlineColorG",    0},
        {"Instance*.Metroid.Visual.HudWeaponInventoryIconOutlineColorB",    0},
        {"Instance*.Metroid.Visual.HudWeaponInventoryIconOutlineThickness", 1},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightColorR",   255},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightColorG",   255},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightColorB",   255},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightCornerRadius", 2},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightPadding",      2},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightSizeOffsetLeft",   0},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightSizeOffsetRight",  1},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightSizeOffsetTop",    -1},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightSizeOffsetBottom", -1},
        {"Instance*.Metroid.Visual.HudHpGaugeOrientation",   0},
        {"Instance*.Metroid.Visual.HudHpGaugeAlign",         1},
        {"Instance*.Metroid.Visual.HudHpGaugeLength",       97},
        {"Instance*.Metroid.Visual.HudHpGaugeWidth",         3},
        {"Instance*.Metroid.Visual.HudHpGaugeOffsetX",     -12},
        {"Instance*.Metroid.Visual.HudHpGaugeOffsetY",       -32},
        {"Instance*.Metroid.Visual.HudHpGaugeColorR",       56},
        {"Instance*.Metroid.Visual.HudHpGaugeColorG",      192},
        {"Instance*.Metroid.Visual.HudHpGaugeColorB",        8},
        {"Instance*.Metroid.Visual.HudHpGaugeAnchor",        2},
        {"Instance*.Metroid.Visual.HudHpGaugePosMode",       2},
        {"Instance*.Metroid.Visual.HudHpGaugePosAnchor",    7},
        {"Instance*.Metroid.Visual.HudHpGaugePosX",         0},
        {"Instance*.Metroid.Visual.HudHpGaugePosY",       -10},
        {"Instance*.Metroid.Visual.HudHpTextAnchor",         3},
        {"Instance*.Metroid.Visual.HudHpTextOffsetX",       -4},
        {"Instance*.Metroid.Visual.HudHpTextOffsetY",      0},
        {"Instance*.Metroid.Visual.HudAmmoGaugeOrientation", 0},
        {"Instance*.Metroid.Visual.HudAmmoGaugeAlign",       0},
        {"Instance*.Metroid.Visual.HudAmmoGaugeLength",     26},
        {"Instance*.Metroid.Visual.HudAmmoGaugeWidth",       3},
        {"Instance*.Metroid.Visual.HudAmmoGaugeOffsetX",     4},
        {"Instance*.Metroid.Visual.HudAmmoGaugeOffsetY",     0},
        {"Instance*.Metroid.Visual.HudAmmoGaugeColorR",     56},
        {"Instance*.Metroid.Visual.HudAmmoGaugeColorG",    192},
        {"Instance*.Metroid.Visual.HudAmmoGaugeColorB",      8},
        {"Instance*.Metroid.Visual.HudAmmoGaugeAnchor",      2},
        {"Instance*.Metroid.Visual.HudAmmoGaugePosMode",     0},
        {"Instance*.Metroid.Visual.HudAmmoGaugePosAnchor",  8},
        {"Instance*.Metroid.Visual.HudAmmoGaugePosX",      0},
        {"Instance*.Metroid.Visual.HudAmmoGaugePosY",     0},
        {"Instance*.Metroid.Visual.HudAmmoTextAnchor",      0},
        {"Instance*.Metroid.Visual.HudAmmoTextOffsetX",    0},
        {"Instance*.Metroid.Visual.HudAmmoTextOffsetY",   -8},
        /* Battle HUD */
        {"Instance*.Metroid.Visual.HudMatchStatusAnchor",   0},
        {"Instance*.Metroid.Visual.HudMatchStatusX",        12},
        {"Instance*.Metroid.Visual.HudMatchStatusY",        19},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelOfsX", 0},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelOfsY", 1},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelPos",  0},
        {"Instance*.Metroid.Visual.HudMatchStatusColorR",   255},
        {"Instance*.Metroid.Visual.HudMatchStatusColorG",   255},
        {"Instance*.Metroid.Visual.HudMatchStatusColorB",   255},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelColorR", 255},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelColorG", 255},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelColorB", 255},
        {"Instance*.Metroid.Visual.HudMatchStatusValueColorR", 255},
        {"Instance*.Metroid.Visual.HudMatchStatusValueColorG", 255},
        {"Instance*.Metroid.Visual.HudMatchStatusValueColorB", 255},
        {"Instance*.Metroid.Visual.HudMatchStatusSepColorR",   255},
        {"Instance*.Metroid.Visual.HudMatchStatusSepColorG",   255},
        {"Instance*.Metroid.Visual.HudMatchStatusSepColorB",   255},
        {"Instance*.Metroid.Visual.HudMatchStatusGoalColorR",  255},
        {"Instance*.Metroid.Visual.HudMatchStatusGoalColorG",  255},
        {"Instance*.Metroid.Visual.HudMatchStatusGoalColorB",  255},
        /* HUD outline color/thickness int defaults */
        {"Instance*.Metroid.Visual.HudGlobalOutlineColorR",       0},
        {"Instance*.Metroid.Visual.HudGlobalOutlineColorG",       0},
        {"Instance*.Metroid.Visual.HudGlobalOutlineColorB",       0},
        {"Instance*.Metroid.Visual.HudGlobalOutlineThickness",    10},
        {"Instance*.Metroid.Visual.HudHpOutlineColorR",            0},
        {"Instance*.Metroid.Visual.HudHpOutlineColorG",            0},
        {"Instance*.Metroid.Visual.HudHpOutlineColorB",            0},
        {"Instance*.Metroid.Visual.HudHpOutlineThickness",         1},
        {"Instance*.Metroid.Visual.HudHpGaugeOutlineColorR",       0},
        {"Instance*.Metroid.Visual.HudHpGaugeOutlineColorG",       0},
        {"Instance*.Metroid.Visual.HudHpGaugeOutlineColorB",       0},
        {"Instance*.Metroid.Visual.HudHpGaugeOutlineThickness",    1},
        {"Instance*.Metroid.Visual.HudWeaponOutlineColorR",        0},
        {"Instance*.Metroid.Visual.HudWeaponOutlineColorG",        0},
        {"Instance*.Metroid.Visual.HudWeaponOutlineColorB",        0},
        {"Instance*.Metroid.Visual.HudWeaponOutlineThickness",     1},
        {"Instance*.Metroid.Visual.HudWeaponIconOutlineColorR",    0},
        {"Instance*.Metroid.Visual.HudWeaponIconOutlineColorG",    0},
        {"Instance*.Metroid.Visual.HudWeaponIconOutlineColorB",    0},
        {"Instance*.Metroid.Visual.HudWeaponIconOutlineThickness", 1},
        {"Instance*.Metroid.Visual.HudAmmoGaugeOutlineColorR",     0},
        {"Instance*.Metroid.Visual.HudAmmoGaugeOutlineColorG",     0},
        {"Instance*.Metroid.Visual.HudAmmoGaugeOutlineColorB",     0},
        {"Instance*.Metroid.Visual.HudAmmoGaugeOutlineThickness",  1},
        {"Instance*.Metroid.Visual.HudMatchStatusOutlineColorR",   0},
        {"Instance*.Metroid.Visual.HudMatchStatusOutlineColorG",   0},
        {"Instance*.Metroid.Visual.HudMatchStatusOutlineColorB",   0},
        {"Instance*.Metroid.Visual.HudMatchStatusOutlineThickness",1},
        {"Instance*.Metroid.Visual.HudRankOutlineColorR",          0},
        {"Instance*.Metroid.Visual.HudRankOutlineColorG",          0},
        {"Instance*.Metroid.Visual.HudRankOutlineColorB",          0},
        {"Instance*.Metroid.Visual.HudRankOutlineThickness",       1},
        {"Instance*.Metroid.Visual.HudTimeLeftOutlineColorR",      0},
        {"Instance*.Metroid.Visual.HudTimeLeftOutlineColorG",      0},
        {"Instance*.Metroid.Visual.HudTimeLeftOutlineColorB",      0},
        {"Instance*.Metroid.Visual.HudTimeLeftOutlineThickness",   1},
        {"Instance*.Metroid.Visual.HudTimeLimitOutlineColorR",     0},
        {"Instance*.Metroid.Visual.HudTimeLimitOutlineColorG",     0},
        {"Instance*.Metroid.Visual.HudTimeLimitOutlineColorB",     0},
        {"Instance*.Metroid.Visual.HudTimeLimitOutlineThickness",  1},
        {"Instance*.Metroid.Visual.HudBombLeftOutlineColorR",      0},
        {"Instance*.Metroid.Visual.HudBombLeftOutlineColorG",      0},
        {"Instance*.Metroid.Visual.HudBombLeftOutlineColorB",      0},
        {"Instance*.Metroid.Visual.HudBombLeftOutlineThickness",   1},
        {"Instance*.Metroid.Visual.HudBombIconOutlineColorR",      0},
        {"Instance*.Metroid.Visual.HudBombIconOutlineColorG",      0},
        {"Instance*.Metroid.Visual.HudBombIconOutlineColorB",      0},
        {"Instance*.Metroid.Visual.HudBombIconOutlineThickness",   1},
        {"Instance*.Metroid.Visual.BtmOverlayOutlineColorR",       0},
        {"Instance*.Metroid.Visual.BtmOverlayOutlineColorG",       0},
        {"Instance*.Metroid.Visual.BtmOverlayOutlineColorB",       0},
        {"Instance*.Metroid.Visual.BtmOverlayOutlineThickness",    1},
        {"Instance*.Metroid.Visual.BtmOverlayFrameOutlineColorR",  0},
        {"Instance*.Metroid.Visual.BtmOverlayFrameOutlineColorG",  0},
        {"Instance*.Metroid.Visual.BtmOverlayFrameOutlineColorB",  0},
        {"Instance*.Metroid.Visual.BtmOverlayFrameOutlineThickness",3},
        /* Bomb Left HUD */
        {"Instance*.Metroid.Visual.HudBombLeftAnchor",  8},
        {"Instance*.Metroid.Visual.HudBombLeftX",      -42},
        {"Instance*.Metroid.Visual.HudBombLeftY",       -8},
        {"Instance*.Metroid.Visual.HudBombLeftAlign",  0},
        {"Instance*.Metroid.Visual.HudBombLeftColorR", 255},
        {"Instance*.Metroid.Visual.HudBombLeftColorG", 255},
        {"Instance*.Metroid.Visual.HudBombLeftColorB", 255},
        /* Bomb Left Icon */
        {"Instance*.Metroid.Visual.HudBombLeftIconColorR", 255},
        {"Instance*.Metroid.Visual.HudBombLeftIconColorG", 255},
        {"Instance*.Metroid.Visual.HudBombLeftIconColorB", 255},
        {"Instance*.Metroid.Visual.HudBombLeftIconMode",    1},
        {"Instance*.Metroid.Visual.HudBombLeftIconOfsX",   16},
        {"Instance*.Metroid.Visual.HudBombLeftIconOfsY",  -16},
        {"Instance*.Metroid.Visual.HudBombLeftIconPosAnchor", 8},
        {"Instance*.Metroid.Visual.HudBombLeftIconPosX",  -26},
        {"Instance*.Metroid.Visual.HudBombLeftIconPosY",  -24},
        {"Instance*.Metroid.Visual.HudBombLeftIconAnchorX", 1},
        {"Instance*.Metroid.Visual.HudBombLeftIconAnchorY", 1},
        {"Instance*.Metroid.Visual.HudBombIconHeight",      13},
        /* Rank & Time HUD */
        {"Instance*.Metroid.Visual.HudRankAnchor",       0},
        {"Instance*.Metroid.Visual.HudRankX",          12},
        {"Instance*.Metroid.Visual.HudRankY",          30},
        {"Instance*.Metroid.Visual.HudRankAlign",       0},
        {"Instance*.Metroid.Visual.HudRankColorR",    255},
        {"Instance*.Metroid.Visual.HudRankColorG",    255},
        {"Instance*.Metroid.Visual.HudRankColorB",    255},
        {"Instance*.Metroid.Visual.HudTimeLeftAnchor",   0},
        {"Instance*.Metroid.Visual.HudTimeLeftX",      12},
        {"Instance*.Metroid.Visual.HudTimeLeftY",      42},
        {"Instance*.Metroid.Visual.HudTimeLeftAlign",   0},
        {"Instance*.Metroid.Visual.HudTimeLeftColorR", 255},
        {"Instance*.Metroid.Visual.HudTimeLeftColorG", 255},
        {"Instance*.Metroid.Visual.HudTimeLeftColorB", 255},
        {"Instance*.Metroid.Visual.HudTimeLimitAnchor",  0},
        {"Instance*.Metroid.Visual.HudTimeLimitX",     12},
        {"Instance*.Metroid.Visual.HudTimeLimitY",     54},
        {"Instance*.Metroid.Visual.HudTimeLimitAlign",  0},
        {"Instance*.Metroid.Visual.HudTimeLimitColorR",255},
        {"Instance*.Metroid.Visual.HudTimeLimitColorG",255},
        {"Instance*.Metroid.Visual.HudTimeLimitColorB",255},
        {"Instance*.Metroid.Visual.CrosshairColorR",         255},
        {"Instance*.Metroid.Visual.CrosshairColorG",           0},
        {"Instance*.Metroid.Visual.CrosshairColorB",           0},
        {"Instance*.Metroid.Visual.CrosshairGap",              2},
        {"Instance*.Metroid.Visual.CrosshairSize",             1},
        {"Instance*.Metroid.Visual.CrosshairThickness",        1},
        {"Instance*.Metroid.Visual.CrosshairInnerLength",      2},
        {"Instance*.Metroid.Visual.CrosshairInnerLengthX",     2},
        {"Instance*.Metroid.Visual.CrosshairInnerLengthY",     2},
        {"Instance*.Metroid.Visual.CrosshairInnerOffset",      2},
        {"Instance*.Metroid.Visual.CrosshairOuterLength",      1},
        {"Instance*.Metroid.Visual.CrosshairOuterLengthX",     1},
        {"Instance*.Metroid.Visual.CrosshairOuterLengthY",     1},
        {"Instance*.Metroid.Visual.CrosshairOuterOffset",      4},
        {"Instance*.Metroid.Visual.CrosshairOutlineThickness", 1},
        {"Instance*.Metroid.Visual.CrosshairDotThickness",     1},
        {"Instance*.Metroid.Visual.CrosshairInnerThickness",   1},
        {"Instance*.Metroid.Visual.CrosshairOuterThickness",   1},
        {"Instance*.Metroid.Visual.CrosshairScale",           100},
        {"Instance*.Metroid.Visual.InGameAspectRatioMode",     0},
        {"Instance*.Metroid.Visual.InGameScalingMode",         0},
        /* MelonPrimeDS Bottom Screen Overlay defaults { */
        {"Instance*.Metroid.Visual.BtmOverlayAnchor",           2},
        {"Instance*.Metroid.Visual.BtmOverlayDstX",            -61},
        {"Instance*.Metroid.Visual.BtmOverlayDstY",            2},
        {"Instance*.Metroid.Visual.BtmOverlayDstSize",         64},
        {"Instance*.Metroid.Visual.BtmOverlaySrcRadius",       46},
        /* MelonPrimeDS Bottom Screen Overlay frame color (default #B90005) { */
        {"Instance*.Metroid.Visual.BtmOverlayRadarColorR",    80},
        {"Instance*.Metroid.Visual.BtmOverlayRadarColorG",      152},
        {"Instance*.Metroid.Visual.BtmOverlayRadarColorB",      208},
        /* MelonPrimeDS Bottom Screen Overlay frame color } */
        /* MelonPrimeDS Bottom Screen Overlay defaults } */
        /* MelonPrimeDS OSD Color Patch defaults { */
        /* Defaults are anchored to the game's original BGR555 literals so that
           enabling the patch with no further changes preserves vanilla appearance.
           See In-Game-OSD-COLOR-PATCH.md:
             - Default bright green = 0x3FEF -> R=123, G=255, B=123 (most messages)
             - Default no-ammo red  = 0x295F -> R=248, G=80,  B=80  (H009 only)
             - H211 "node stolen"   = 0x001F -> R=255, G=0,   B=0   (separate color path) */

        /* Global "set all" color (defaults to game's bright green 0x3FEF) */
        {"Instance*.Metroid.Visual.OsdColorR",              123},
        {"Instance*.Metroid.Visual.OsdColorG",              255},
        {"Instance*.Metroid.Visual.OsdColorB",              123},
        /* Per-category literal colors. All anchored to game default 0x3FEF except NoAmmo (0x295F). */
        {"Instance*.Metroid.Visual.OsdColorLostLivesR",     123},
        {"Instance*.Metroid.Visual.OsdColorLostLivesG",     255},
        {"Instance*.Metroid.Visual.OsdColorLostLivesB",     123},
        {"Instance*.Metroid.Visual.OsdColorKillDeathR",     123},
        {"Instance*.Metroid.Visual.OsdColorKillDeathG",     255},
        {"Instance*.Metroid.Visual.OsdColorKillDeathB",     123},
        {"Instance*.Metroid.Visual.OsdColorReturnBaseR",    123},
        {"Instance*.Metroid.Visual.OsdColorReturnBaseG",    255},
        {"Instance*.Metroid.Visual.OsdColorReturnBaseB",    123},
        /* H009 AMMO DEPLETED uses 0x295F (red-orange) in vanilla */
        {"Instance*.Metroid.Visual.OsdColorNoAmmoR",        248},
        {"Instance*.Metroid.Visual.OsdColorNoAmmoG",         80},
        {"Instance*.Metroid.Visual.OsdColorNoAmmoB",         80},
        {"Instance*.Metroid.Visual.OsdColorCowardDetectR",  123},
        {"Instance*.Metroid.Visual.OsdColorCowardDetectG",  255},
        {"Instance*.Metroid.Visual.OsdColorCowardDetectB",  123},
        {"Instance*.Metroid.Visual.OsdColorAcquiringNodeR", 123},
        {"Instance*.Metroid.Visual.OsdColorAcquiringNodeG", 255},
        {"Instance*.Metroid.Visual.OsdColorAcquiringNodeB", 123},
        {"Instance*.Metroid.Visual.OsdColorTurretR",        123},
        {"Instance*.Metroid.Visual.OsdColorTurretG",        255},
        {"Instance*.Metroid.Visual.OsdColorTurretB",        123},
        {"Instance*.Metroid.Visual.OsdColorOctoResetR",     123},
        {"Instance*.Metroid.Visual.OsdColorOctoResetG",     255},
        {"Instance*.Metroid.Visual.OsdColorOctoResetB",     123},
        {"Instance*.Metroid.Visual.OsdColorOctoDropR",      123},
        {"Instance*.Metroid.Visual.OsdColorOctoDropG",      255},
        {"Instance*.Metroid.Visual.OsdColorOctoDropB",      123},
        {"Instance*.Metroid.Visual.OsdColorOctoCondR",      123},
        {"Instance*.Metroid.Visual.OsdColorOctoCondG",      255},
        {"Instance*.Metroid.Visual.OsdColorOctoCondB",      123},
        {"Instance*.Metroid.Visual.OsdColorOctoMissingR",   123},
        {"Instance*.Metroid.Visual.OsdColorOctoMissingG",   255},
        {"Instance*.Metroid.Visual.OsdColorOctoMissingB",   123},
        /* H211 "node stolen" separate color (vanilla 0x001F = pure red) */
        {"Instance*.Metroid.Visual.OsdColorH211R",          255},
        {"Instance*.Metroid.Visual.OsdColorH211G",            0},
        {"Instance*.Metroid.Visual.OsdColorH211B",            0},
        /* Slot override per-flag category colors (used when applyGlobal=false).
           Defaults match game OSD defaults (0x3FEF = RGB 123,255,123 for all categories).
           flags=0x02: kill/death result messages (YOU KILLED, KILLED YOU, 5-kill, prime hunter)
           flags=0x11: node capture messages (acquiring node H204/H205, node stolen H211)
           flags=0x01: objective messages (AMMO DEPLETED, return to base, bounty, octolith events)
           flags=0x00: system/quick messages (HEADSHOT!, FACE OFF!, RETURN TO BATTLE!, COWARD, turret) */
        {"Instance*.Metroid.Visual.OsdColorSlotKillDeathR", 123},
        {"Instance*.Metroid.Visual.OsdColorSlotKillDeathG", 255},
        {"Instance*.Metroid.Visual.OsdColorSlotKillDeathB", 123},
        {"Instance*.Metroid.Visual.OsdColorSlotNodeR",      123},
        {"Instance*.Metroid.Visual.OsdColorSlotNodeG",      255},
        {"Instance*.Metroid.Visual.OsdColorSlotNodeB",      123},
        {"Instance*.Metroid.Visual.OsdColorSlotObjectiveR", 123},
        {"Instance*.Metroid.Visual.OsdColorSlotObjectiveG", 255},
        {"Instance*.Metroid.Visual.OsdColorSlotObjectiveB", 123},
        {"Instance*.Metroid.Visual.OsdColorSlotSystemR",    123},
        {"Instance*.Metroid.Visual.OsdColorSlotSystemG",    255},
        {"Instance*.Metroid.Visual.OsdColorSlotSystemB",    123},
        /* MelonPrimeDS OSD Color Patch defaults } */
        /* MelonPrimeDS Custom HUD Crosshair defaults } */
    #endif
    };

    RangeList IntRanges =
    {
        {"Emu.ConsoleType", {0, 1}},
        {"3D.Renderer", {0, renderer3D_Max - 1}},
        {"Screen.VSyncInterval", {1, 20}},
        {"3D.GL.ScaleFactor", {1, 16}},
        {"Audio.Interpolation", {0, 4}},
        {"Instance*.Audio.Volume", {0, 256}},
        {"Mic.InputType", {0, micInputType_MAX - 1}},
        {"Instance*.Window*.ScreenRotation", {0, screenRot_MAX - 1}},
        {"Instance*.Window*.ScreenGap", {0, 500}},
        {"Instance*.Window*.ScreenLayout", {0, screenLayout_MAX - 1}},
        {"Instance*.Window*.ScreenSizing", {0, screenSizing_MAX - 1}},
        {"Instance*.Window*.ScreenAspectTop", {0, AspectRatiosNum - 1}},
        {"Instance*.Window*.ScreenAspectBot", {0, AspectRatiosNum - 1}},
        {"MP.AudioMode", {0, 2}},
        {"LAN.HostNumPlayers", {2, 16}},

    #ifdef MELONPRIME_DS
        /* MelonPrimeDS. this is not for input. this is for loading. */
        {"Instance*.Metroid.Sensitivity.Aim", {0,99999}},
        {"Instance*.Metroid.Volume.Music", {0,9}},
        {"Instance*.Metroid.Volume.SFX", {0,9}},
        //{"Instance*.Metroid.Sensitivity.Mph", {-5,155}},
        //{"Instance*.Metroid.Sensitivity.AimYAxisScale", {0.000001,100.000000}},
    #endif
    };

    DefaultList<bool> DefaultBools =
    {
    #ifdef MELONPRIME_DS
        {"Screen.Filter", false}, // melonPrimeDS defaults
        {"Screen.UseGL", true},   // melonPrimeDS defaults
    #else
        {"Screen.Filter", true},
    #endif
        {"3D.Soft.Threaded", true},
    #ifdef MELONPRIME_DS
        {"3D.GL.BetterPolygons", true}, // melonPrimeDS Added
    #endif
        {"3D.GL.HiresCoordinates", true},
        {"LimitFPS", true},
        {"Instance*.Window*.ShowOSD", true},
        {"Emu.DirectBoot", true},
        {"Instance*.DS.Battery.LevelOkay", true},
        {"Instance*.DSi.Battery.Charging", true},
    #ifdef MELONPRIME_DS
        {"JIT.Enable", true}, // melonPrimeDS Added
    #endif
    #ifdef JIT_ENABLED
        {"JIT.BranchOptimisations", true},
        {"JIT.LiteralOptimisations", true},
    #ifndef __APPLE__
        {"JIT.FastMemory", true},
    #endif
    #endif
        {"DSi.DSP.HLE", true},

    #ifdef MELONPRIME_DS
        /* MelonPrimeDS { */
        {"Instance*.Metroid.Operation.SnapTap", false},
        {"Instance*.Metroid.Data.Unlock", false},
        {"Instance*.Metroid.Apply.Headphone", false},
        {"Instance*.Metroid.Use.Firmware.Name", false},
        {"Instance*.Metroid.HunterLicense.Hunter.Apply", false},
        {"Instance*.Metroid.HunterLicense.Color.Apply", false},
        {"Instance*.Metroid.Apply.SfxVolume", false},
        {"Instance*.Metroid.Apply.MusicVolume", false},
        {"Instance*.Metroid.Apply.joy2KeySupport", true},
        {"Instance*.Metroid.Enable.stylusMode", false},
        {"Instance*.Metroid.Aim.Disable.MphAimSmoothing", false},
        {"Instance*.Metroid.Aim.Enable.Accumulator", false},
        {"Instance*.Metroid.Aim.Enable.NativeDeltaHook", false},
        {"Instance*.Metroid.Aim.Enable.InstantAimFollow", false},
        /* MelonPrimeDS Crosshair bools { */
        {"Instance*.Metroid.Visual.CustomHUD", false},
        // Per-element default-HUD hide patches (DisableDefaultHud section).
        // Defaults match the legacy bulk-hide behavior so existing CustomHUD
        // users see no visual change on upgrade.
        {"Instance*.Metroid.Visual.DisableDefaultHud.Helmet",            true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.Ammo",              true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.WeaponIcon",        true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.HP",                true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.Crosshair",         true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.Bomb",              true},
        // Per-mode top-screen score row patches. Default true for all so
        // the custom MatchStatus HUD replaces the native row in every mode.
        {"Instance*.Metroid.Visual.DisableDefaultHud.ScoreBattle",       true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.ScoreSurvival",     true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.ScorePrimeHunter",  true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.ScoreBounty",       true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.ScoreCapture",      true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.ScoreDefender",     true},
        {"Instance*.Metroid.Visual.DisableDefaultHud.ScoreNode",         true},
        {"Instance*.Metroid.Visual.HudAutoScaleEnable", true},
        {"Instance*.Metroid.Visual.CrosshairOutline", true},
        {"Instance*.Metroid.Visual.CrosshairCenterDot", true},
        {"Instance*.Metroid.Visual.CrosshairTStyle", true},
        {"Instance*.Metroid.Visual.CrosshairInnerShow", true},
        {"Instance*.Metroid.Visual.CrosshairInnerLinkXY", true},
        {"Instance*.Metroid.Visual.CrosshairOuterShow", true},
        {"Instance*.Metroid.Visual.CrosshairOuterLinkXY", true},
        {"Instance*.Metroid.Visual.HudHpGauge", true},
        {"Instance*.Metroid.Visual.HudHpGaugeAutoColor", true},
        {"Instance*.Metroid.Visual.HudHpTextAutoColor", false},
        {"Instance*.Metroid.Visual.HudWeaponIconShow", true},
        {"Instance*.Metroid.Visual.HudWeaponIconColorOverlayPowerBeam", false},
        {"Instance*.Metroid.Visual.HudWeaponIconColorOverlayVoltDriver", false},
        {"Instance*.Metroid.Visual.HudWeaponIconColorOverlayMissile", false},
        {"Instance*.Metroid.Visual.HudWeaponIconColorOverlayBattleHammer", false},
        {"Instance*.Metroid.Visual.HudWeaponIconColorOverlayImperialist", false},
        {"Instance*.Metroid.Visual.HudWeaponIconColorOverlayJudicator", false},
        {"Instance*.Metroid.Visual.HudWeaponIconColorOverlayMagmaul", false},
        {"Instance*.Metroid.Visual.HudWeaponIconColorOverlayShockCoil", false},
        {"Instance*.Metroid.Visual.HudWeaponIconColorOverlayOmegaCannon", false},
        {"Instance*.Metroid.Visual.HudWeaponInventoryShow", true},
        {"Instance*.Metroid.Visual.HudAmmoGauge", true},
        {"Instance*.Metroid.Visual.HudMatchStatusShow", true},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelColorOverall", true},
        {"Instance*.Metroid.Visual.HudMatchStatusValueColorOverall", true},
        {"Instance*.Metroid.Visual.HudMatchStatusSepColorOverall",   true},
        {"Instance*.Metroid.Visual.HudMatchStatusGoalColorOverall",  true},
        {"Instance*.Metroid.Visual.HudRankShow",        true},
        {"Instance*.Metroid.Visual.HudRankShowOrdinal", true},
        {"Instance*.Metroid.Visual.HudTimeLeftShow",    true},
        {"Instance*.Metroid.Visual.HudTimeLimitShow",   false},
        {"Instance*.Metroid.Visual.HudGlobalOutline",         false},
        {"Instance*.Metroid.Visual.HudHpOutline",            true},
        {"Instance*.Metroid.Visual.HudHpGaugeOutline",       true},
        {"Instance*.Metroid.Visual.HudWeaponOutline",        true},
        {"Instance*.Metroid.Visual.HudWeaponIconOutline",    true},
        {"Instance*.Metroid.Visual.HudAmmoGaugeOutline",     true},
        {"Instance*.Metroid.Visual.HudMatchStatusOutline",   true},
        {"Instance*.Metroid.Visual.HudRankOutline",          true},
        {"Instance*.Metroid.Visual.HudTimeLeftOutline",      true},
        {"Instance*.Metroid.Visual.HudTimeLimitOutline",     true},
        {"Instance*.Metroid.Visual.HudBombLeftOutline",      true},
        {"Instance*.Metroid.Visual.HudBombIconOutline",      true},
        {"Instance*.Metroid.Visual.BtmOverlayOutline",       true},
        {"Instance*.Metroid.Visual.HudBombLeftShow",       true},
        {"Instance*.Metroid.Visual.HudBombLeftTextShow",   false},
        {"Instance*.Metroid.Visual.HudBombLeftIconShow",   true},
        {"Instance*.Metroid.Visual.HudBombLeftIconColorOverlay", true},
        {"Instance*.Metroid.BugFix.WifiBitset",        true},
        {"Instance*.Metroid.BugFix.FixShadowFreeze",   false},
        {"Instance*.Metroid.BugFix.FixNoxusBladePersistence", true},
        {"Instance*.Metroid.BugFix.UseFirmwareLanguage", false},
        {"Instance*.Metroid.Visual.InGameAspectRatio", true},
        {"Instance*.Metroid.Visual.OsdColor",            false},
        {"Instance*.Metroid.Visual.OsdColorApplyGlobal", false},
        {"Instance*.Metroid.Visual.OsdColorH211",        true},
        {"Instance*.Metroid.Visual.ClipCursorToBottomScreenWhenNotInGame", false},
        {"Instance*.Metroid.Visual.InGameTopScreenOnly", false},
        {"Instance*.Metroid.Visual.InGameScaling", true},
        {"Instance*.Metroid.Visual.InGameScalingAuto", true},
        {"Instance*.Metroid.UI.SectionCrosshair",      false},
        {"Instance*.Metroid.UI.SectionInner",          false},
        {"Instance*.Metroid.UI.SectionOuter",          false},
        {"Instance*.Metroid.UI.SectionHpPos",          false},
        {"Instance*.Metroid.UI.SectionWpnPos",         false},
        {"Instance*.Metroid.UI.SectionWpnIcon",        false},
        {"Instance*.Metroid.UI.SectionHpGauge",        false},
        {"Instance*.Metroid.UI.SectionAmmoGauge",      false},
        {"Instance*.Metroid.UI.SectionMatchStatusScore", false},
        {"Instance*.Metroid.UI.SectionRankTime",       false},
        {"Instance*.Metroid.UI.SectionBombLeft",       false},
        {"Instance*.Metroid.UI.SectionBombLeftIcon",   false},
        {"Instance*.Metroid.UI.SectionInputSettings",  false},
        {"Instance*.Metroid.UI.SectionScreenSync",     false},
        {"Instance*.Metroid.UI.SectionCursorClipSettings",  false},
        {"Instance*.Metroid.UI.SectionInGameApply",  false},
        {"Instance*.Metroid.UI.SectionInGameAspectRatio",  false},
        {"Instance*.Metroid.UI.SectionSensitivity",    true},
        {"Instance*.Metroid.UI.SectionBugFix",         true},
        {"Instance*.Metroid.UI.SectionGameFeature",    true},
        {"Instance*.Metroid.UI.SectionGameplay",       true},
        {"Instance*.Metroid.UI.SectionVideo",          true},
        {"Instance*.Metroid.UI.SectionVolume",         true},
        {"Instance*.Metroid.UI.SectionLicense",        true},
        /* MelonPrimeDS Bottom Screen Overlay bool { */
        {"Instance*.Metroid.Visual.BtmOverlayEnable", true},
        {"Instance*.Metroid.Visual.BtmOverlayRadarColorUseHunter", false},
        {"Instance*.Metroid.Visual.BtmOverlayFrameOutline", false},
        /* MelonPrimeDS Bottom Screen Overlay bool } */
        {"Instance*.Metroid.Visual.HudWeaponInventoryOutline",          true},
        {"Instance*.Metroid.Visual.HudWeaponInventoryIconOutline",      true},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightEnable", true},
        {"Instance*.Metroid.UI.SectionHudWeaponInventoryHighlight",    false},
        {"Instance*.Metroid.UI.SectionHudWeaponInventoryOutline",      false},
        {"Instance*.Metroid.UI.SectionHudWeaponInventoryIconOutline", false},
        {"Instance*.Metroid.UI.SectionDisableDefaultHud",              false},
        /* MelonPrimeDS Crosshair bools } */
        /* MelonPrimeDS OSD Color section toggles { */
        {"Instance*.Metroid.UI.SectionOsdColor",         false},
        {"Instance*.Metroid.UI.SectionOsdH211",          false},
        {"Instance*.Metroid.UI.SectionOsdLostLives",     false},
        {"Instance*.Metroid.UI.SectionOsdKillDeath",     false},
        {"Instance*.Metroid.UI.SectionOsdReturnBase",    false},
        {"Instance*.Metroid.UI.SectionOsdNoAmmo",        false},
        {"Instance*.Metroid.UI.SectionOsdCowardDetect",  false},
        {"Instance*.Metroid.UI.SectionOsdAcquiringNode", false},
        {"Instance*.Metroid.UI.SectionOsdTurret",        false},
        {"Instance*.Metroid.UI.SectionOsdOctoReset",     false},
        {"Instance*.Metroid.UI.SectionOsdOctoDrop",      false},
        {"Instance*.Metroid.UI.SectionOsdOctoCond",      false},
        {"Instance*.Metroid.UI.SectionOsdOctoMissing",   false},
        /* MelonPrimeDS OSD Color section toggles } */
        /* MelonPrimeDS } */
    #endif
    };

    DefaultList<std::string> DefaultStrings =
    {
        {"DLDI.ImagePath",                  "dldi.bin"},
        {"DSi.SD.ImagePath",                "dsisd.bin"},
        {"Instance*.Firmware.Username",     "melonDS"},
    #ifdef MELONPRIME_DS
        {"Instance*.Metroid.Visual.HudHpPrefix", ""},
        {"Instance*.Metroid.Visual.HudAmmoPrefix", ""},
        {"Instance*.Metroid.Visual.HudBombLeftPrefix", "bombs"},
        {"Instance*.Metroid.Visual.HudBombLeftSuffix", ""},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelPoints",    "points"},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelOctoliths", "octoliths"},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelLives",     "lives left"},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelRingTime",  "ring time"},
        {"Instance*.Metroid.Visual.HudMatchStatusLabelPrimeTime", "prime time"},
        {"Instance*.Metroid.Visual.HudRankPrefix", ""},
        {"Instance*.Metroid.Visual.HudRankSuffix", ""},
    #endif
    };
  
    DefaultList<double> DefaultDoubles =
    {
        {"TargetFPS", 60.0},
        {"FastForwardFPS", 1000.0},
        {"SlowmoFPS", 30.0},
    #ifdef MELONPRIME_DS
        {"Instance*.Metroid.Sensitivity.Mph", -3.000},
        {"Instance*.Metroid.Sensitivity.AimYAxisScale", 1.500000},
        {"Instance*.Metroid.Aim.Adjust", 0.010000},
        /* MelonPrimeDS Crosshair opacities { */
        {"Instance*.Metroid.Visual.CrosshairOutlineOpacity", 0.5},
        {"Instance*.Metroid.Visual.CrosshairDotOpacity",     1.0},
        {"Instance*.Metroid.Visual.CrosshairInnerOpacity",   0.8},
        {"Instance*.Metroid.Visual.CrosshairOuterOpacity",   0.4},
        /* MelonPrimeDS Crosshair opacities } */
        /* MelonPrimeDS HUD element opacities { */
        {"Instance*.Metroid.Visual.HudHpOpacity",            1.0},
        {"Instance*.Metroid.Visual.HudHpGaugeOpacity",       1.0},
        {"Instance*.Metroid.Visual.HudWeaponOpacity",        1.0},
        {"Instance*.Metroid.Visual.HudWpnIconOpacity",       1.0},
        {"Instance*.Metroid.Visual.HudAmmoGaugeOpacity",     1.0},
        {"Instance*.Metroid.Visual.HudMatchStatusOpacity",   1.0},
        {"Instance*.Metroid.Visual.HudRankOpacity",          1.0},
        {"Instance*.Metroid.Visual.HudTimeLeftOpacity",      1.0},
        {"Instance*.Metroid.Visual.HudTimeLimitOpacity",     1.0},
        {"Instance*.Metroid.Visual.HudBombLeftOpacity",      1.0},
        {"Instance*.Metroid.Visual.HudBombIconOpacity",      1.0},
        {"Instance*.Metroid.Visual.HudWeaponInventoryOutlineOpacity",      0.75},
        {"Instance*.Metroid.Visual.HudWeaponInventoryIconOutlineOpacity",   0.75},
        {"Instance*.Metroid.Visual.HudWeaponInventoryOpacity",         1.0},
        {"Instance*.Metroid.Visual.HudWeaponInventoryNotOwnedOpacity", 0.25},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightOpacity",   0.25},
        {"Instance*.Metroid.Visual.HudWeaponInventoryHighlightThickness", 0.75},
        /* MelonPrimeDS HUD element opacities } */
        /* MelonPrimeDS HUD per-element outlines { */
        {"Instance*.Metroid.Visual.HudGlobalOutlineOpacity",      0.75},
        {"Instance*.Metroid.Visual.HudHpOutlineOpacity",           0.75},
        {"Instance*.Metroid.Visual.HudHpGaugeOutlineOpacity",      0.75},
        {"Instance*.Metroid.Visual.HudWeaponOutlineOpacity",       0.75},
        {"Instance*.Metroid.Visual.HudWeaponIconOutlineOpacity",   0.75},
        {"Instance*.Metroid.Visual.HudAmmoGaugeOutlineOpacity",    0.75},
        {"Instance*.Metroid.Visual.HudMatchStatusOutlineOpacity",  0.75},
        {"Instance*.Metroid.Visual.HudRankOutlineOpacity",         0.75},
        {"Instance*.Metroid.Visual.HudTimeLeftOutlineOpacity",     0.75},
        {"Instance*.Metroid.Visual.HudTimeLimitOutlineOpacity",    0.75},
        {"Instance*.Metroid.Visual.HudBombLeftOutlineOpacity",     0.75},
        {"Instance*.Metroid.Visual.HudBombIconOutlineOpacity",     0.75},
        {"Instance*.Metroid.Visual.BtmOverlayOutlineOpacity",      0.75},
        {"Instance*.Metroid.Visual.BtmOverlayFrameOutlineOpacity", 0.75},
        /* MelonPrimeDS HUD per-element outlines } */
        /* MelonPrimeDS Bottom Screen Overlay opacity { */
        {"Instance*.Metroid.Visual.BtmOverlayOpacity", 0.85},
        /* MelonPrimeDS Bottom Screen Overlay opacity } */
    #endif
    };

    /*
    struct LegacyEntry
        char Name[32];
        int Type;           // 0=int 1=bool 2=string 3=64bit int
        char TOMLPath[64];
        bool InstanceUnique; // whether the setting can exist individually for each instance in multiplayer

    */
    LegacyEntry LegacyFile[] =
    {
        {"Key_A",      0, "Keyboard.A", true},
        {"Key_B",      0, "Keyboard.B", true},
        {"Key_Select", 0, "Keyboard.Select", true},
        {"Key_Start",  0, "Keyboard.Start", true},
        {"Key_Right",  0, "Keyboard.Right", true},
        {"Key_Left",   0, "Keyboard.Left", true},
        {"Key_Up",     0, "Keyboard.Up", true},
        {"Key_Down",   0, "Keyboard.Down", true},
        {"Key_R",      0, "Keyboard.R", true},
        {"Key_L",      0, "Keyboard.L", true},
        {"Key_X",      0, "Keyboard.X", true},
        {"Key_Y",      0, "Keyboard.Y", true},

        {"Joy_A",      0, "Joystick.A", true},
        {"Joy_B",      0, "Joystick.B", true},
        {"Joy_Select", 0, "Joystick.Select", true},
        {"Joy_Start",  0, "Joystick.Start", true},
        {"Joy_Right",  0, "Joystick.Right", true},
        {"Joy_Left",   0, "Joystick.Left", true},
        {"Joy_Up",     0, "Joystick.Up", true},
        {"Joy_Down",   0, "Joystick.Down", true},
        {"Joy_R",      0, "Joystick.R", true},
        {"Joy_L",      0, "Joystick.L", true},
        {"Joy_X",      0, "Joystick.X", true},
        {"Joy_Y",      0, "Joystick.Y", true},

        {"HKKey_Lid",                 0, "Keyboard.HK_Lid", true},
        {"HKKey_Mic",                 0, "Keyboard.HK_Mic", true},
        {"HKKey_Pause",               0, "Keyboard.HK_Pause", true},
        {"HKKey_Reset",               0, "Keyboard.HK_Reset", true},
        {"HKKey_FastForward",         0, "Keyboard.HK_FastForward", true},
        {"HKKey_FastForwardToggle",   0, "Keyboard.HK_FrameLimitToggle", true},
        {"HKKey_FullscreenToggle",    0, "Keyboard.HK_FullscreenToggle", true},
        {"HKKey_SwapScreens",         0, "Keyboard.HK_SwapScreens", true},
        {"HKKey_SwapScreenEmphasis",  0, "Keyboard.HK_SwapScreenEmphasis", true},
        {"HKJoy_SolarSensorDecrease", 0, "Joystick.HK_SolarSensorDecrease", true},
        {"HKJoy_SolarSensorIncrease", 0, "Joystick.HK_SolarSensorIncrease", true},


    #ifdef MELONPRIME_DS
        // melonPrimeDS
        {"HKKey_MetroidMoveForward",       0, "Keyboard.HK_MetroidMoveForward",        true},
        {"HKKey_MetroidMoveBack",          0, "Keyboard.HK_MetroidMoveBack",           true},
        {"HKKey_MetroidMoveLeft",          0, "Keyboard.HK_MetroidMoveLeft",           true},
        {"HKKey_MetroidMoveRight",         0, "Keyboard.HK_MetroidMoveRight",          true},

        {"HKKey_MetroidJump",              0, "Keyboard.HK_MetroidJump",               true},

        {"HKKey_MetroidMorphBall",         0, "Keyboard.HK_MetroidMorphBall",          true},
        {"HKKey_MetroidZoom",              0, "Keyboard.HK_MetroidZoom",               true},
        {"HKKey_MetroidHoldMorphBallBoost",0, "Keyboard.HK_MetroidHoldMorphBallBoost", true},

        {"HKKey_MetroidScanVisor",         0, "Keyboard.HK_MetroidScanVisor",          true},

        {"HKKey_MetroidUILeft",            0, "Keyboard.HK_MetroidUILeft",             true},
        {"HKKey_MetroidUIRight",           0, "Keyboard.HK_MetroidUIRight",            true},
        {"HKKey_MetroidUIOk",              0, "Keyboard.HK_MetroidUIOk",               true},
        {"HKKey_MetroidUIYes",             0, "Keyboard.HK_MetroidUIYes",              true},
        {"HKKey_MetroidUINo",              0, "Keyboard.HK_MetroidUINo",               true},

        {"HKKey_MetroidShootScan",         0, "Keyboard.HK_MetroidShootScan",          true},
        {"HKKey_MetroidScanShoot",         0, "Keyboard.HK_MetroidScanShoot",          true},

        {"HKKey_MetroidWeaponBeam",        0, "Keyboard.HK_MetroidWeaponBeam",         true},
        {"HKKey_MetroidWeaponMissile",     0, "Keyboard.HK_MetroidWeaponMissile",      true},
        {"HKKey_MetroidWeaponSpecial",     0, "Keyboard.HK_MetroidWeaponSpecial",      true},
        {"HKKey_MetroidWeaponNext",        0, "Keyboard.HK_MetroidWeaponNext",         true},
        {"HKKey_MetroidWeaponPrevious",    0, "Keyboard.HK_MetroidWeaponPrevious",     true},
        {"HKKey_MetroidWeapon1",           0, "Keyboard.HK_MetroidWeapon1",            true},
        {"HKKey_MetroidWeapon2",           0, "Keyboard.HK_MetroidWeapon2",            true},
        {"HKKey_MetroidWeapon3",           0, "Keyboard.HK_MetroidWeapon3",            true},
        {"HKKey_MetroidWeapon4",           0, "Keyboard.HK_MetroidWeapon4",            true},
        {"HKKey_MetroidWeapon5",           0, "Keyboard.HK_MetroidWeapon5",            true},
        {"HKKey_MetroidWeapon6",           0, "Keyboard.HK_MetroidWeapon6",            true},
        {"HKKey_MetroidWeaponCheck",       0, "Keyboard.HK_MetroidWeaponCheck",        true},

        {"HKKey_MetroidMenu",              0, "Keyboard.HK_MetroidMenu",               true},
        {"HKKey_MetroidIngameSensiUp",     0, "Keyboard.HK_MetroidIngameSensiUp",      true},
        {"HKKey_MetroidIngameSensiDown",   0, "Keyboard.HK_MetroidIngameSensiDown",    true},
    #endif

        // not metroid

        {"HKKey_FrameStep",           0, "Keyboard.HK_FrameStep", true},
        {"HKKey_PowerButton",         0, "Keyboard.HK_PowerButton", true},
        {"HKKey_VolumeUp",            0, "Keyboard.HK_VolumeUp", true},
        {"HKKey_VolumeDown",          0, "Keyboard.HK_VolumeDown", true},
        {"HKKey_GuitarGripGreen",     0, "Keyboard.HK_GuitarGripGreen", true},
        {"HKKey_GuitarGripRed",       0, "Keyboard.HK_GuitarGripRed", true},
        {"HKKey_GuitarGripYellow",    0, "Keyboard.HK_GuitarGripYellow", true},
        {"HKKey_GuitarGripBlue",      0, "Keyboard.HK_GuitarGripBlue", true},

        {"HKJoy_Lid",                 0, "Joystick.HK_Lid", true},
        {"HKJoy_Mic",                 0, "Joystick.HK_Mic", true},
        {"HKJoy_Pause",               0, "Joystick.HK_Pause", true},
        {"HKJoy_Reset",               0, "Joystick.HK_Reset", true},
        {"HKJoy_FastForward",         0, "Joystick.HK_FastForward", true},
        {"HKJoy_FastForwardToggle",   0, "Joystick.HK_FrameLimitToggle", true},
        {"HKJoy_FullscreenToggle",    0, "Joystick.HK_FullscreenToggle", true},
        {"HKJoy_SwapScreens",         0, "Joystick.HK_SwapScreens", true},
        {"HKJoy_SwapScreenEmphasis",  0, "Joystick.HK_SwapScreenEmphasis", true},
        {"HKJoy_SolarSensorDecrease", 0, "Joystick.HK_SolarSensorDecrease", true},
        {"HKJoy_SolarSensorIncrease", 0, "Joystick.HK_SolarSensorIncrease", true},

    #ifdef MELONPRIME_DS
        // melonPrimeDS
        { "HKJoy_MetroidMoveForward",       0, "Joystick.HK_MetroidMoveForward",        true},
        { "HKJoy_MetroidMoveBack",          0, "Joystick.HK_MetroidMoveBack",           true},
        { "HKJoy_MetroidMoveLeft",          0, "Joystick.HK_MetroidMoveLeft",           true},
        { "HKJoy_MetroidMoveRight",         0, "Joystick.HK_MetroidMoveRight",          true},

        { "HKJoy_MetroidJump",              0, "Joystick.HK_MetroidJump",               true},

        { "HKJoy_MetroidMorphBall",         0, "Joystick.HK_MetroidMorphBall",          true},
        { "HKJoy_MetroidZoom",              0, "Joystick.HK_MetroidZoom",               true},
        { "HKJoy_MetroidHoldMorphBallBoost",0, "Joystick.HK_MetroidHoldMorphBallBoost", true},

        { "HKJoy_MetroidScanVisor",         0, "Joystick.HK_MetroidScanVisor",          true},

        { "HKJoy_MetroidUILeft",            0, "Joystick.HK_MetroidUILeft",             true},
        { "HKJoy_MetroidUIRight",           0, "Joystick.HK_MetroidUIRight",            true},
        { "HKJoy_MetroidUIOk",              0, "Joystick.HK_MetroidUIOk",               true},
        { "HKJoy_MetroidUIYes",             0, "Joystick.HK_MetroidUIYes",              true},
        { "HKJoy_MetroidUINo",              0, "Joystick.HK_MetroidUINo",               true},

        { "HKJoy_MetroidShootScan",         0, "Joystick.HK_MetroidShootScan",          true},
        { "HKJoy_MetroidScanShoot",         0, "Joystick.HK_MetroidScanShoot",          true},

        { "HKJoy_MetroidWeaponBeam",        0, "Joystick.HK_MetroidWeaponBeam",         true},
        { "HKJoy_MetroidWeaponMissile",     0, "Joystick.HK_MetroidWeaponMissile",      true},
        { "HKJoy_MetroidWeaponSpecial",     0, "Joystick.HK_MetroidWeaponSpecial",      true},
        { "HKJoy_MetroidWeaponNext",        0, "Joystick.HK_MetroidWeaponNext",         true},
        { "HKJoy_MetroidWeaponPrevious",    0, "Joystick.HK_MetroidWeaponPrevious",     true},
        { "HKJoy_MetroidWeapon1",           0, "Joystick.HK_MetroidWeapon1",            true},
        { "HKJoy_MetroidWeapon2",           0, "Joystick.HK_MetroidWeapon2",            true},
        { "HKJoy_MetroidWeapon3",           0, "Joystick.HK_MetroidWeapon3",            true},
        { "HKJoy_MetroidWeapon4",           0, "Joystick.HK_MetroidWeapon4",            true},
        { "HKJoy_MetroidWeapon5",           0, "Joystick.HK_MetroidWeapon5",            true},
        { "HKJoy_MetroidWeapon6",           0, "Joystick.HK_MetroidWeapon6",            true },
        { "HKJoy_MetroidWeaponCheck",       0, "Joystick.HK_MetroidWeaponCheck",        true },

        { "HKJoy_MetroidMenu",              0, "Joystick.HK_MetroidMenu",               true},
        { "HKJoy_MetroidIngameSensiUp",     0, "Joystick.HK_MetroidIngameSensiUp",      true},
        { "HKJoy_MetroidIngameSensiDown",   0, "Joystick.HK_MetroidIngameSensiDown",    true},
    #endif

        // not metroid

        {"HKJoy_FrameStep",           0, "Joystick.HK_FrameStep", true},
        {"HKJoy_PowerButton",         0, "Joystick.HK_PowerButton", true},
        {"HKJoy_VolumeUp",            0, "Joystick.HK_VolumeUp", true},
        {"HKJoy_VolumeDown",          0, "Joystick.HK_VolumeDown", true},
        {"HKJoy_GuitarGripGreen",     0, "Joystick.HK_GuitarGripGreen", true},
        {"HKJoy_GuitarGripRed",       0, "Joystick.HK_GuitarGripRed", true},
        {"HKJoy_GuitarGripYellow",    0, "Joystick.HK_GuitarGripYellow", true},
        {"HKJoy_GuitarGripBlue",      0, "Joystick.HK_GuitarGripBlue", true},

        {"JoystickID", 0, "JoystickID", true},

        {"ScreenRotation", 0, "Window0.ScreenRotation", true},
        {"ScreenGap",      0, "Window0.ScreenGap", true},
        {"ScreenLayout",   0, "Window0.ScreenLayout", true},
        {"ScreenSwap",     1, "Window0.ScreenSwap", true},
        {"ScreenSizing",   0, "Window0.ScreenSizing", true},
        {"IntegerScaling", 1, "Window0.IntegerScaling", true},
        {"ScreenAspectTop",0, "Window0.ScreenAspectTop", true},
        {"ScreenAspectBot",0, "Window0.ScreenAspectBot", true},

    #ifdef MELONPRIME_DS
        {"ScreenFilter",        1, "Screen.Filter", false},
        {"ScreenUseGL",         1, "Screen.UseGL", false},
    #else
        {"ScreenFilter",        1, "Screen.Filter", false},
    #endif
        {"ScreenVSync",         1, "Screen.VSync", false},
        {"ScreenVSyncInterval", 0, "Screen.VSyncInterval", false},

        {"3DRenderer", 0, "3D.Renderer", false},
        {"Threaded3D", 1, "3D.Soft.Threaded", false},

        {"GL_ScaleFactor", 0, "3D.GL.ScaleFactor", false},
    #ifdef MELONPRIME_DS
        {"GL_BetterPolygons", 1, "3D.GL.BetterPolygons", false}, // Added for MelonPrimeDS
    #endif
        {"GL_HiresCoordinates", 1, "3D.GL.HiresCoordinates", false},

        {"LimitFPS", 1, "LimitFPS", false},
        {"MaxFPS", 0, "MaxFPS", false},
        {"AudioSync", 1, "AudioSync", false},
        {"ShowOSD", 1, "Window0.ShowOSD", true},

        {"ConsoleType", 0, "Emu.ConsoleType", false},
        {"DirectBoot", 1, "Emu.DirectBoot", false},

    #ifdef JIT_ENABLED
        {"JIT_Enable", 1, "JIT.Enable", false},
        {"JIT_MaxBlockSize", 0, "JIT.MaxBlockSize", false},
        {"JIT_BranchOptimisations", 1, "JIT.BranchOptimisations", false},
        {"JIT_LiteralOptimisations", 1, "JIT.LiteralOptimisations", false},
        {"JIT_FastMemory", 1, "JIT.FastMemory", false},
    #endif

        {"ExternalBIOSEnable", 1, "Emu.ExternalBIOSEnable", false},

        {"BIOS9Path", 2, "DS.BIOS9Path", false},
        {"BIOS7Path", 2, "DS.BIOS7Path", false},
        {"FirmwarePath", 2, "DS.FirmwarePath", false},
        {"DSiExternalBIOSEnable", 1, "DSi.ExternalBIOSEnable", false},
        {"DSiBIOS9Path", 2, "DSi.BIOS9Path", false},
        {"DSiBIOS7Path", 2, "DSi.BIOS7Path", false},
        {"DSiFirmwarePath", 2, "DSi.FirmwarePath", false},
        {"DSiNANDPath", 2, "DSi.NANDPath", false},

        {"DLDIEnable", 1, "DLDI.Enable", false},
        {"DLDISDPath", 2, "DLDI.ImagePath", false},
        {"DLDISize", 0, "DLDI.ImageSize", false},
        {"DLDIReadOnly", 1, "DLDI.ReadOnly", false},
        {"DLDIFolderSync", 1, "DLDI.FolderSync", false},
        {"DLDIFolderPath", 2, "DLDI.FolderPath", false},

        {"DSiSDEnable", 1, "DSi.SD.Enable", false},
        {"DSiSDPath", 2, "DSi.SD.ImagePath", false},
        {"DSiSDSize", 0, "DSi.SD.ImageSize", false},
        {"DSiSDReadOnly", 1, "DSi.SD.ReadOnly", false},
        {"DSiSDFolderSync", 1, "DSi.SD.FolderSync", false},
        {"DSiSDFolderPath", 2, "DSi.SD.FolderPath", false},

        {"FirmwareOverrideSettings", 1, "Firmware.OverrideSettings", true},
        {"FirmwareUsername", 2, "Firmware.Username", true},
        {"FirmwareLanguage", 0, "Firmware.Language", true},
        {"FirmwareBirthdayMonth", 0, "Firmware.BirthdayMonth", true},
        {"FirmwareBirthdayDay", 0, "Firmware.BirthdayDay", true},
        {"FirmwareFavouriteColour", 0, "Firmware.FavouriteColour", true},
        {"FirmwareMessage", 2, "Firmware.Message", true},
        {"FirmwareMAC", 2, "Firmware.MAC", true},

        {"MPAudioMode", 0, "MP.AudioMode", false},
        {"MPRecvTimeout", 0, "MP.RecvTimeout", false},

        {"LANDevice", 2, "LAN.Device", false},
        {"DirectLAN", 1, "LAN.DirectMode", false},

        {"SavStaRelocSRAM", 1, "Savestate.RelocSRAM", false},

        {"AudioInterp", 0, "Audio.Interpolation", false},
        {"AudioBitDepth", 0, "Audio.BitDepth", false},
        {"AudioVolume", 0, "Audio.Volume", true},
        {"DSiVolumeSync", 1, "Audio.DSiVolumeSync", true},
        {"MicInputType", 0, "Mic.InputType", false},
        {"MicDevice", 2, "Mic.Device", false},
        {"MicWavPath", 2, "Mic.WavPath", false},

        {"LastROMFolder", 2, "LastROMFolder", false},
        {"LastBIOSFolder", 2, "LastBIOSFolder", false},

        {"RecentROM_0", 4, "RecentROM[0]", false},
        {"RecentROM_1", 4, "RecentROM[1]", false},
        {"RecentROM_2", 4, "RecentROM[2]", false},
        {"RecentROM_3", 4, "RecentROM[3]", false},
        {"RecentROM_4", 4, "RecentROM[4]", false},
        {"RecentROM_5", 4, "RecentROM[5]", false},
        {"RecentROM_6", 4, "RecentROM[6]", false},
        {"RecentROM_7", 4, "RecentROM[7]", false},
        {"RecentROM_8", 4, "RecentROM[8]", false},
        {"RecentROM_9", 4, "RecentROM[9]", false},

        {"SaveFilePath", 2, "SaveFilePath", true},
        {"SavestatePath", 2, "SavestatePath", true},
        {"CheatFilePath", 2, "CheatFilePath", true},

        {"EnableCheats", 1, "EnableCheats", true},

        {"MouseHide",        1, "Mouse.Hide", false},
        {"MouseHideSeconds", 0, "Mouse.HideSeconds", false},
        {"PauseLostFocus",   1, "PauseLostFocus", false},
        {"MuteFastForward",   1, "MuteFastForward", false},
        {"UITheme",          2, "UITheme", false},

        {"RTCOffset",       3, "RTC.Offset", true},

        {"DSBatteryLevelOkay",   1, "DS.Battery.LevelOkay", true},
        {"DSiBatteryLevel",    0, "DSi.Battery.Level", true},
        {"DSiBatteryCharging", 1, "DSi.Battery.Charging", true},

    #ifdef GDBSTUB_ENABLED
        {"GdbEnabled", 1, "Gdb.Enabled", false},
        {"GdbPortARM7", 0, "Gdb.ARM7.Port", true},
        {"GdbPortARM9", 0, "Gdb.ARM9.Port", true},
        {"GdbARM7BreakOnStartup", 1, "Gdb.ARM7.BreakOnStartup", true},
        {"GdbARM9BreakOnStartup", 1, "Gdb.ARM9.BreakOnStartup", true},
    #endif
    #ifdef MELONPRIME_DS
        { "MetroidAimSensitivity", 0, "Metroid.Sensitivity.Aim",  true }, // MelonPrimeDS
    #endif
        {"Camera0_InputType", 0, "DSi.Camera0.InputType", false},
        {"Camera0_ImagePath", 2, "DSi.Camera0.ImagePath", false},
        {"Camera0_CamDeviceName", 2, "DSi.Camera0.DeviceName", false},
        {"Camera0_XFlip", 1, "DSi.Camera0.XFlip", false},
        {"Camera1_InputType", 0, "DSi.Camera1.InputType", false},
        {"Camera1_ImagePath", 2, "DSi.Camera1.ImagePath", false},
        {"Camera1_CamDeviceName", 2, "DSi.Camera1.DeviceName", false},
        {"Camera1_XFlip", 1, "DSi.Camera1.XFlip", false},

        {"", -1, "", false}
    };


    static std::string GetDefaultKey(std::string path)
    {
        std::string tables[] = { "Instance", "Window", "Camera" };

        std::string ret = "";
        int plen = path.length();
        for (int i = 0; i < plen;)
        {
            bool found = false;

            for (auto& tbl : tables)
            {
                int tlen = tbl.length();
                if ((plen - i) <= tlen) continue;
                if (path.substr(i, tlen) != tbl) continue;
                if (path[i + tlen] < '0' || path[i + tlen] > '9') continue;

                ret += tbl + "*";
                i = path.find('.', i + tlen);
                if (i == std::string::npos) return ret;

                found = true;
                break;
            }

            if (!found)
            {
                ret += path[i];
                i++;
            }
        }

        return ret;
    }


    Array::Array(toml::value& data) : Data(data)
    {
    }

    size_t Array::Size()
    {
        return Data.size();
    }

    void Array::Clear()
    {
        toml::array newarray;
        Data = newarray;
    }

    Array Array::GetArray(const int id)
    {
        while (Data.size() < id + 1)
            Data.push_back(toml::array());

        toml::value& arr = Data[id];
        if (!arr.is_array())
            arr = toml::array();

        return Array(arr);
    }

    int Array::GetInt(const int id)
    {
        while (Data.size() < id + 1)
            Data.push_back(0);

        toml::value& tval = Data[id];
        if (!tval.is_integer())
            tval = 0;

        return (int)tval.as_integer();
    }

    int64_t Array::GetInt64(const int id)
    {
        while (Data.size() < id + 1)
            Data.push_back(0);

        toml::value& tval = Data[id];
        if (!tval.is_integer())
            tval = 0;

        return tval.as_integer();
    }

    bool Array::GetBool(const int id)
    {
        while (Data.size() < id + 1)
            Data.push_back(false);

        toml::value& tval = Data[id];
        if (!tval.is_boolean())
            tval = false;

        return tval.as_boolean();
    }

    std::string Array::GetString(const int id)
    {
        while (Data.size() < id + 1)
            Data.push_back("");

        toml::value& tval = Data[id];
        if (!tval.is_string())
            tval = "";

        return tval.as_string();
    }

    double Array::GetDouble(const int id)
    {
        while (Data.size() < id + 1)
            Data.push_back(0.0);

        toml::value& tval = Data[id];
        if (!tval.is_floating())
            tval = 0.0;

        return tval.as_floating();
    }

    void Array::SetInt(const int id, int val)
    {
        while (Data.size() < id + 1)
            Data.push_back(0);

        toml::value& tval = Data[id];
        tval = val;
    }

    void Array::SetInt64(const int id, int64_t val)
    {
        while (Data.size() < id + 1)
            Data.push_back(0);

        toml::value& tval = Data[id];
        tval = val;
    }

    void Array::SetBool(const int id, bool val)
    {
        while (Data.size() < id + 1)
            Data.push_back(false);

        toml::value& tval = Data[id];
        tval = val;
    }

    void Array::SetString(const int id, const std::string& val)
    {
        while (Data.size() < id + 1)
            Data.push_back("");

        toml::value& tval = Data[id];
        tval = val;
    }

    void Array::SetDouble(const int id, double val)
    {
        while (Data.size() < id + 1)
            Data.push_back(0.0);

        toml::value& tval = Data[id];
        tval = val;
    }


    /*Table::Table()// : Data(toml::value())
    {
        Data = toml::value();
        PathPrefix = "";
    }*/

    Table::Table(toml::value& data, const std::string& path) : Data(data)
    {
        if (path.empty())
            PathPrefix = "";
        else
            PathPrefix = path + ".";
    }

    Table& Table::operator=(const Table& b)
    {
        Data = b.Data;
        PathPrefix = b.PathPrefix;

        return *this;
    }

    Array Table::GetArray(const std::string& path)
    {
        toml::value& arr = ResolvePath(path);
        if (!arr.is_array())
            arr = toml::array();

        return Array(arr);
    }

    Table Table::GetTable(const std::string& path, const std::string& defpath)
    {
        toml::value& tbl = ResolvePath(path);
        if (!tbl.is_table())
        {
            toml::value defval = toml::table();
            if (!defpath.empty())
                defval = ResolvePath(defpath);

            tbl = defval;
        }

        return Table(tbl, PathPrefix + path);
    }

    int Table::GetInt(const std::string& path)
    {
        toml::value& tval = ResolvePath(path);
        if (!tval.is_integer())
            tval = FindDefault(path, 0, DefaultInts);

        int ret = (int)tval.as_integer();

        std::string rngkey = GetDefaultKey(PathPrefix + path);
        if (IntRanges.count(rngkey) != 0)
        {
            auto& range = IntRanges[rngkey];
            ret = std::clamp(ret, std::get<0>(range), std::get<1>(range));
        }

        return ret;
    }

    int64_t Table::GetInt64(const std::string& path)
    {
        toml::value& tval = ResolvePath(path);
        if (!tval.is_integer())
            tval = 0;

        return tval.as_integer();
    }

    bool Table::GetBool(const std::string& path)
    {
        toml::value& tval = ResolvePath(path);
        if (!tval.is_boolean())
            tval = FindDefault(path, false, DefaultBools);

        return tval.as_boolean();
    }

    std::string Table::GetString(const std::string& path)
    {
        toml::value& tval = ResolvePath(path);
        if (!tval.is_string())
            tval = FindDefault(path, ""s, DefaultStrings);

        return tval.as_string();
    }

    double Table::GetDouble(const std::string& path)
    {
        toml::value& tval = ResolvePath(path);
        if (!tval.is_floating())
            tval = FindDefault(path, 0.0, DefaultDoubles);

        return tval.as_floating();
    }

    void Table::SetInt(const std::string& path, int val)
    {
        std::string rngkey = GetDefaultKey(PathPrefix + path);
        if (IntRanges.count(rngkey) != 0)
        {
            auto& range = IntRanges[rngkey];
            val = std::clamp(val, std::get<0>(range), std::get<1>(range));
        }

        toml::value& tval = ResolvePath(path);
        tval = val;
    }

    void Table::SetInt64(const std::string& path, int64_t val)
    {
        toml::value& tval = ResolvePath(path);
        tval = val;
    }

    void Table::SetBool(const std::string& path, bool val)
    {
        toml::value& tval = ResolvePath(path);
        tval = val;
    }

    void Table::SetString(const std::string& path, const std::string& val)
    {
        toml::value& tval = ResolvePath(path);
        tval = val;
    }

    void Table::SetDouble(const std::string& path, double val)
    {
        toml::value& tval = ResolvePath(path);
        toml::floating_format_info info = { .prec = 10 };
        tval = toml::value(val, info);
    }

    toml::value& Table::ResolvePath(const std::string& path)
    {
        toml::value* ret = &Data;
        std::string tmp = path;

        size_t sep;
        while ((sep = tmp.find('.')) != std::string::npos)
        {
            ret = &(*ret)[tmp.substr(0, sep)];
            tmp = tmp.substr(sep + 1);
        }

        return (*ret)[tmp];
    }

    bool Table::HasKey(const std::string& path)
    {
        const toml::value* node = &Data;
        std::string tmp = path;

        size_t sep;
        while ((sep = tmp.find('.')) != std::string::npos)
        {
            if (!node->is_table()) return false;
            const auto& key = tmp.substr(0, sep);
            if (!node->as_table(std::nothrow).count(key)) return false;
            node = &node->as_table(std::nothrow).at(key);
            tmp = tmp.substr(sep + 1);
        }

        if (!node->is_table()) return false;
        return node->as_table(std::nothrow).count(tmp) > 0;
    }

    template<typename T> T Table::FindDefault(const std::string& path, T def, DefaultList<T> list)
    {
        std::string defkey = GetDefaultKey(PathPrefix + path);

        T ret = def;
        while (list.count(defkey) == 0)
        {
            if (defkey.empty()) break;
            size_t sep = defkey.rfind('.');
            if (sep == std::string::npos) break;
            defkey = defkey.substr(0, sep);
        }
        if (list.count(defkey) != 0)
            ret = list[defkey];

        return ret;
    }


    bool LoadLegacyFile(int inst)
    {
        Platform::FileHandle* f;
        if (inst > 0)
        {
            char name[100] = { 0 };
            snprintf(name, 99, kLegacyUniqueConfigFile, inst + 1);
            f = Platform::OpenLocalFile(name, Platform::FileMode::ReadText);
        }
        else
        {
            f = Platform::OpenLocalFile(kLegacyConfigFile, Platform::FileMode::ReadText);
        }

        if (!f) return true;

        toml::value* root;// = GetLocalTable(inst);
        if (inst == -1)
            root = &RootTable;
        else
            root = &RootTable["Instance" + std::to_string(inst)];

        char linebuf[1024];
        char entryname[32];
        char entryval[1024];
        while (!Platform::IsEndOfFile(f))
        {
            if (!Platform::FileReadLine(linebuf, 1024, f))
                break;

            int ret = sscanf(linebuf, "%31[A-Za-z_0-9]=%[^\t\r\n]", entryname, entryval);
            entryname[31] = '\0';
            if (ret < 2) continue;

            for (LegacyEntry* entry = &LegacyFile[0]; entry->Type != -1; entry++)
            {
                if (!strncmp(entry->Name, entryname, 32))
                {
                    if (!(entry->InstanceUnique ^ (inst == -1)))
                        break;

                    std::string path = entry->TOMLPath;
                    toml::value* table = root;
                    size_t sep;
                    while ((sep = path.find('.')) != std::string::npos)
                    {
                        table = &(*table)[path.substr(0, sep)];
                        path = path.substr(sep + 1);
                    }

                    int arrayid = -1;
                    if (path[path.size() - 1] == ']')
                    {
                        size_t tmp = path.rfind('[');
                        arrayid = std::stoi(path.substr(tmp + 1, path.size() - tmp - 2));
                        path = path.substr(0, tmp);
                    }

                    toml::value& val = (*table)[path];

                    switch (entry->Type)
                    {
                    case 0:
                        val = strtol(entryval, nullptr, 10);
                        break;

                    case 1:
                        val = !!strtol(entryval, nullptr, 10);
                        break;

                    case 2:
                        val = entryval;
                        break;

                    case 3:
                        val = strtoll(entryval, nullptr, 10);
                        break;

                    case 4:
                        if (!val.is_array()) val = toml::array();
                        while (val.size() < arrayid + 1)
                            val.push_back("");
                        val[arrayid] = entryval;
                        break;
                    }

                    break;
                }
            }
        }

        CloseFile(f);
        return true;
    }

    bool LoadLegacy()
    {
        for (int i = -1; i < 16; i++)
            LoadLegacyFile(i);

        return true;
    }

    bool Load()
    {
        auto cfgpath = Platform::GetLocalFilePath(kConfigFile);

        if (!Platform::CheckFileWritable(cfgpath))
            return false;

        RootTable = toml::value();

        if (!Platform::FileExists(cfgpath))
            return LoadLegacy();

        try
        {
            RootTable = toml::parse(std::filesystem::u8path(cfgpath));
        }
        catch (toml::syntax_error& err)
        {
            //RootTable = toml::table();
        }

        return true;
    }

    void Save()
    {
        auto cfgpath = Platform::GetLocalFilePath(kConfigFile);
        if (!Platform::CheckFileWritable(cfgpath))
            return;

        std::ofstream file;
        file.open(std::filesystem::u8path(cfgpath), std::ofstream::out | std::ofstream::trunc);
        file << RootTable;
        file.close();
    }


    Table GetLocalTable(int instance)
    {
        if (instance == -1)
            return Table(RootTable, "");

        std::string key = "Instance" + std::to_string(instance);
        toml::value& tbl = RootTable[key];
        if (tbl.is_empty())
            RootTable[key] = RootTable["Instance0"];

        return Table(tbl, key);
    }

}


