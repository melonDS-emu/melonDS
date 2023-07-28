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

#ifndef INPUTCONFIGDIALOG_H
#define INPUTCONFIGDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <initializer_list>

#include "Config.h"

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

static constexpr std::initializer_list<int> hk_save_load =
{
    HK_Save_Slot_1,
    HK_Save_Slot_2,
    HK_Save_Slot_3,
    HK_Save_Slot_4,
    HK_Save_Slot_5,
    HK_Save_Slot_6,
    HK_Save_Slot_7,
    HK_Save_Slot_8,
    HK_Save_Slot_File,
    HK_Load_Slot_1,
    HK_Load_Slot_2,
    HK_Load_Slot_3,
    HK_Load_Slot_4,
    HK_Load_Slot_5,
    HK_Load_Slot_6,
    HK_Load_Slot_7,
    HK_Load_Slot_8,
    HK_Load_Slot_File,
};

static constexpr std::initializer_list<const char*> hk_save_load_labels =
{
    "Save slot 1",
    "Save slot 2",
    "Save slot 3",
    "Save slot 4",
    "Save slot 5",
    "Save slot 6",
    "Save slot 7",
    "Save slot 8",
    "Save slot File",
    "Load slot 1",
    "Load slot 2",
    "Load slot 3",
    "Load slot 4",
    "Load slot 5",
    "Load slot 6",
    "Load slot 7",
    "Load slot 8",
    "Load slot File"
};

static_assert(hk_save_load.size() == hk_save_load_labels.size());


namespace Ui { class InputConfigDialog; }
class InputConfigDialog;

class InputConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InputConfigDialog(QWidget* parent);
    ~InputConfigDialog();

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

private:
    void populatePage(QWidget* page,
        const std::initializer_list<const char*>& labels,
        int* keymap, int* joymap);
    void setupKeypadPage();

    Ui::InputConfigDialog* ui;

    int keypadKeyMap[12], keypadJoyMap[12];
    int addonsKeyMap[hk_addons.size()], addonsJoyMap[hk_addons.size()];
    int hkGeneralKeyMap[hk_general.size()], hkGeneralJoyMap[hk_general.size()];
    int hkSaveLoadKeyMap[hk_save_load.size()], hkSaveLoadJoyMap[hk_save_load.size()];
};


#endif // INPUTCONFIGDIALOG_H
