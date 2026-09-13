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

#ifndef INPUTCONFIGDIALOG_H
#define INPUTCONFIGDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <initializer_list>

#include "Config.h"
#include "EmuInstance.h"
#include "PokeTypeBindings.h"

static constexpr int keypad_num = 12;

static constexpr std::initializer_list<int> hk_addons =
{
    HK_SolarSensorIncrease,
    HK_SolarSensorDecrease,
    HK_GuitarGripGreen,
    HK_GuitarGripRed,
    HK_GuitarGripYellow,
    HK_GuitarGripBlue,
};

static constexpr std::initializer_list<const char*> hk_addons_labels =
{
    "[Boktai] Sunlight + ",
    "[Boktai] Sunlight - ",
    "[Guitar Grip] Green",
    "[Guitar Grip] Red",
    "[Guitar Grip] Yellow",
    "[Guitar Grip] Blue",
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
    HK_VolumeDown,
    HK_AudioMuteToggle
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
    "DSi Volume down",
    "Toggle audio mute"
};

static_assert(hk_general.size() == hk_general_labels.size());


namespace Ui { class InputConfigDialog; }
class InputConfigDialog;
class KeyMapButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;

class InputConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InputConfigDialog(QWidget* parent);
    ~InputConfigDialog();

    SDL_Joystick* getJoystick();
    std::shared_ptr<SDL_mutex> getJoyMutex();

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

private slots:
    void on_InputConfigDialog_accepted();
    void on_InputConfigDialog_rejected();

    void on_btnKeyMapSwitch_clicked();
    void on_btnJoyMapSwitch_clicked();
    void on_cbxJoystick_currentIndexChanged(int id);

    // not on_<object>_<signal>: these widgets are built in code, so
    // connectSlotsByName would warn about them
    void pokeTypeRegionChanged(int id);
    void pokeTypeResetClicked();
    void pokeTypeEdited();
    void pokeTypeKeyCaptured();

private:
    void populatePage(QWidget* page,
        const std::initializer_list<const char*>& labels,
        int* keymap, int* joymap);
    void setupKeypadPage();

    void setupPokeTypePage();
    void rebuildPokeTypeRows();
    void loadPokeTypeRegion(melonDS::PokeTypeKeyboard::Region r);
    void flushPokeTypeRegion();
    void refreshPokeTypeWarnings();
    bool rejectPokeTypeConflict(KeyMapButton* btn);

    Ui::InputConfigDialog* ui;

    EmuInstance* emuInstance;

    int keypadKeyMap[12], keypadJoyMap[12];
    int addonsKeyMap[hk_addons.size()], addonsJoyMap[hk_addons.size()];
    int hkGeneralKeyMap[hk_general.size()], hkGeneralJoyMap[hk_general.size()];
    int joystickID;

    PokeTypeBindings pokeTypeBindings;
    int pokeTypeKeyMap[PokeTypeBindings::MaxKeys] = {};
    melonDS::PokeTypeKeyboard::Region pokeTypeRegion = melonDS::PokeTypeKeyboard::Region::Europe;
    int pokeTypeReleaseKey;
    // set by any edit; lets accept save bindings with no supported cart inserted
    bool pokeTypeDirty = false;
    // mapping value when the current capture was armed
    int pokeTypeCaptureBefore = 0;
    QComboBox* cbxPokeTypeRegion = nullptr;
    QWidget* pokeTypeRowHost = nullptr;
    QLabel* lblPokeTypeDuplicate = nullptr;

    QCheckBox* chkPokeTypeEnable = nullptr;
    QCheckBox* chkPokeTypeAutoSendFn = nullptr;
    QCheckBox* chkPokeTypeArrowsDpad = nullptr;
    QLabel* lblPokeTypeNoRelease = nullptr;
    QButtonGroup* grpPokeTypeMode = nullptr;
    QRadioButton* radPokeTypeMode[3] = {};
    QPushButton* btnPokeTypeReset = nullptr;
};


#endif // INPUTCONFIGDIALOG_H
