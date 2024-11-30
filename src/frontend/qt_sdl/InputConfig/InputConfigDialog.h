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

#ifndef INPUTCONFIGDIALOG_H
#define INPUTCONFIGDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <initializer_list>

#include "Config.h"
#include "EmuInstance.h"

static constexpr int keypad_num = 12;

static constexpr std::initializer_list<int> hk_addons =
{
    HK_SolarSensorIncrease,
    HK_SolarSensorDecrease,
};

static constexpr std::initializer_list<const char*> hk_addons_labels =
{
    "[Boktai] Sunlight + ",
    "[Boktai] Sunlight - ",
};

static_assert(hk_addons.size() == hk_addons_labels.size());

static constexpr std::initializer_list<int> hk_general =
{
    HK_Pause,
    HK_Reset,
    HK_FrameStep,
    HK_FastForward,
    HK_FastForwardToggle,
    HK_SlowMo,
    HK_SlowMoToggle,
    HK_FrameLimitToggle,
    HK_FullscreenToggle,
    HK_Lid,
    HK_Mic,
    HK_SwapScreens,
    HK_SwapScreenEmphasis,
    HK_PowerButton,
    HK_VolumeUp,
    HK_VolumeDown
};

static constexpr std::initializer_list<const char*> hk_general_labels =
{
    "Pause/resume",
    "Reset",
    "Frame step",
    "Fast forward",
    "Toggle fast forward",
    "Slow mo",
    "Toggle slow mo",
    "Toggle FPS limit",
    "Toggle fullscreen",
    "Close/open lid",
    "Microphone",
    "Swap screens",
    "Swap screen emphasis",
    "DSi Power button",
    "DSi Volume up",
    "DSi Volume down"
};

static_assert(hk_general.size() == hk_general_labels.size());

// MelonPrimeDS {
static constexpr std::initializer_list<int> hk_tabAddonsMetroid =
{
    HK_MetroidMoveForward,
    HK_MetroidMoveBack,
    HK_MetroidMoveLeft,
    HK_MetroidMoveRight,
    HK_MetroidJump,
    HK_MetroidMorphBall,
    HK_MetroidZoom,
    HK_MetroidHoldMorphBallBoost,
    HK_MetroidScanVisor,
    HK_MetroidUILeft,
    HK_MetroidUIRight,
    HK_MetroidUIOk,
    HK_MetroidUIYes,
    HK_MetroidUINo,
    HK_MetroidShootScan,
    HK_MetroidScanShoot,
    HK_MetroidWeaponBeam,
    HK_MetroidWeaponMissile,
    HK_MetroidWeaponSpecial,
    HK_MetroidWeaponNext,
    HK_MetroidWeaponPrevious,
    HK_MetroidWeapon1,
    HK_MetroidWeapon2,
    HK_MetroidWeapon3,
    HK_MetroidWeapon4,
    HK_MetroidWeapon5,
    HK_MetroidWeapon6,
    HK_MetroidMenu,
    HK_MetroidIngameSensiUp,
    HK_MetroidIngameSensiDown

};

static constexpr std::initializer_list<const char*> hk_tabAddonsMetroid_labels =
{
    "[Metroid] (W) Move Forward",
    "[Metroid] (S) Move Back",
    "[Metroid] (A) Move Left",
    "[Metroid] (D) Move Right",
    "[Metroid] (Space) Jump",
    "[Metroid] (L. Ctrl) Transform",
    "[Metroid] (Mouse Right) Imperialist Zoom, Map Zoom Out, Morph Ball Boost",
    "[Metroid] (Shift) Hold to Fast Morph Ball Boost",
    "[Metroid] (C) Scan Visor",
    "[Metroid] (Z) UI Left",
    "[Metroid] (X) UI Right",
    "[Metroid] (F) UI Ok",
    "[Metroid] (G) UI Yes (Enter Starship)",
    "[Metroid] (H) UI No (Enter Starship)",
    "[Metroid] (Mouse Left) Shoot/Scan, Map Zoom In",
    "[Metroid] (V) Scan/Shoot, Map Zoom In",
    "[Metroid] (Mouse 5, Side Top) Weapon Beam",
    "[Metroid] (Mouse 4, Side Bottom) Weapon Missile",
    "[Metroid] (R) SpecialWeapon (Last used Weapon, Omega cannon)",
    "[Metroid] (J) Next Weapon in the sorted order",
    "[Metroid] (K) Previous Weapon in the sorted order",
    "[Metroid] (1) Weapon 1",
    "[Metroid] (2) Weapon 2",
    "[Metroid] (3) Weapon 3",
    "[Metroid] (4) Weapon 4",
    "[Metroid] (5) Weapon 5",
    "[Metroid] (6) Weapon 6",
    "[Metroid] (Tab) Menu/Map",
    "[Metroid] (PgUp) AimSensitivity Up",
    "[Metroid] (PgDown) AimSensitivity Down"
};

static_assert(hk_tabAddonsMetroid.size() == hk_tabAddonsMetroid_labels.size());
// } MelonPrimeDS


namespace Ui { class InputConfigDialog; }
class InputConfigDialog;

class InputConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InputConfigDialog(QWidget* parent);
    ~InputConfigDialog();

    SDL_Joystick* getJoystick();

    static InputConfigDialog* currentDlg;
    static InputConfigDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new InputConfigDialog(parent);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

    /* MelonPrimeDS { */
    void switchTabToAddons();
    void switchTabToMetroid();
    /* } MelonPrimeDS*/

private slots:
    void on_InputConfigDialog_accepted();
    void on_InputConfigDialog_rejected();

    void on_btnKeyMapSwitch_clicked();
    void on_btnJoyMapSwitch_clicked();
    void on_cbxJoystick_currentIndexChanged(int id);

    /* MelonPrimeDS { */
    void on_metroidResetSensitivityValues_clicked();
    void on_metroidSetVideoQualityToLow_clicked();
    void on_metroidSetVideoQualityToHigh_clicked();
    /* } MelonPrimeDS*/

private:
    void populatePage(QWidget* page,
        const std::initializer_list<const char*>& labels,
        int* keymap, int* joymap);
    void setupKeypadPage();

    Ui::InputConfigDialog* ui;

    EmuInstance* emuInstance;

    int keypadKeyMap[12], keypadJoyMap[12];
    int addonsKeyMap[hk_addons.size()], addonsJoyMap[hk_addons.size()];
    int addonsMetroidKeyMap[hk_tabAddonsMetroid.size()], addonsMetroidJoyMap[hk_tabAddonsMetroid.size()]; // MelonPrimeDS
    int hkGeneralKeyMap[hk_general.size()], hkGeneralJoyMap[hk_general.size()];
    int joystickID;
};


#endif // INPUTCONFIGDIALOG_H
