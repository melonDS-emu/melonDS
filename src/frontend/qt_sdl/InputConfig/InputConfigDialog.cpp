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

#include <QGroupBox>
#include <QLabel>
#include <QKeyEvent>
#include <QDebug>

#include <SDL2/SDL.h>

#include "types.h"
#include "Platform.h"
#include "Config.h"

#include "MapButton.h"
#include "Input.h"
#include "InputConfigDialog.h"
#include "ui_InputConfigDialog.h"


InputConfigDialog* InputConfigDialog::currentDlg = nullptr;

const int dskeyorder[12] = {0, 1, 10, 11, 5, 4, 6, 7, 9, 8, 2, 3};
const char* dskeylabels[12] = {"A", "B", "X", "Y", "Left", "Right", "Up", "Down", "L", "R", "Select", "Start"};

const int hk_addons[] =
{
    HK_SolarSensorIncrease,
    HK_SolarSensorDecrease,
};

const char* hk_addons_labels[] =
{
    "[Boktai] Sunlight + ",
    "[Boktai] Sunlight - ",
};

const int hk_general[] =
{
    HK_Pause,
    HK_Reset,
    HK_FrameStep,
    HK_FastForward,
    HK_FastForwardToggle,
    HK_FullscreenToggle,
    HK_Lid,
    HK_Mic,
    HK_SwapScreens
};

const char* hk_general_labels[] =
{
    "Pause/resume",
    "Reset",
    "Frame step",
    "Fast forward",
    "Toggle FPS limit",
    "Toggle fullscreen",
    "Close/open lid",
    "Microphone",
    "Swap screens"
};

const int keypad_num = 12;
const int hk_addons_num = 2;
const int hk_general_num = 9;


InputConfigDialog::InputConfigDialog(QWidget* parent) : QDialog(parent), ui(new Ui::InputConfigDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    for (int i = 0; i < keypad_num; i++)
    {
        keypadKeyMap[i] = Config::KeyMapping[dskeyorder[i]];
        keypadJoyMap[i] = Config::JoyMapping[dskeyorder[i]];
    }

    for (int i = 0; i < hk_addons_num; i++)
    {
        addonsKeyMap[i] = Config::HKKeyMapping[hk_addons[i]];
        addonsJoyMap[i] = Config::HKJoyMapping[hk_addons[i]];
    }

    for (int i = 0; i < hk_general_num; i++)
    {
        hkGeneralKeyMap[i] = Config::HKKeyMapping[hk_general[i]];
        hkGeneralJoyMap[i] = Config::HKJoyMapping[hk_general[i]];
    }

    populatePage(ui->tabAddons, hk_addons_num, hk_addons_labels, addonsKeyMap, addonsJoyMap);
    populatePage(ui->tabHotkeysGeneral, hk_general_num, hk_general_labels, hkGeneralKeyMap, hkGeneralJoyMap);

    int njoy = SDL_NumJoysticks();
    if (njoy > 0)
    {
        for (int i = 0; i < njoy; i++)
        {
            const char* name = SDL_JoystickNameForIndex(i);
            ui->cbxJoystick->addItem(QString(name));
        }
        ui->cbxJoystick->setCurrentIndex(Input::JoystickID);
    }
    else
    {
        ui->cbxJoystick->addItem("(no joysticks available)");
        ui->cbxJoystick->setEnabled(false);
    }

    setupKeypadPage();

    int inst = Platform::InstanceID();
    if (inst > 0)
        ui->lblInstanceNum->setText(QString("Configuring mappings for instance %1").arg(inst+1));
    else
        ui->lblInstanceNum->hide();
}

InputConfigDialog::~InputConfigDialog()
{
    delete ui;
}

void InputConfigDialog::setupKeypadPage()
{
    for (int i = 0; i < keypad_num; i++)
    {
        QPushButton* pushButtonKey = this->findChild<QPushButton*>(QStringLiteral("btnKey") + dskeylabels[i]);
        QPushButton* pushButtonJoy = this->findChild<QPushButton*>(QStringLiteral("btnJoy") + dskeylabels[i]);

        KeyMapButton* keyMapButtonKey = new KeyMapButton(&keypadKeyMap[i], false);
        JoyMapButton* keyMapButtonJoy = new JoyMapButton(&keypadJoyMap[i], false);

        pushButtonKey->parentWidget()->layout()->replaceWidget(pushButtonKey, keyMapButtonKey);
        pushButtonJoy->parentWidget()->layout()->replaceWidget(pushButtonJoy, keyMapButtonJoy);

        delete pushButtonKey;
        delete pushButtonJoy;

        if (ui->cbxJoystick->isEnabled())
        {
            ui->stackMapping->setCurrentIndex(1);
        }
    }
}

void InputConfigDialog::populatePage(QWidget* page, int num, const char** labels, int* keymap, int* joymap)
{
    // kind of a hack
    bool ishotkey = (page != ui->tabInput);

    QHBoxLayout* main_layout = new QHBoxLayout();

    QGroupBox* group;
    QGridLayout* group_layout;

    group = new QGroupBox("Keyboard mappings:");
    main_layout->addWidget(group);
    group_layout = new QGridLayout();
    group_layout->setSpacing(1);
    for (int i = 0; i < num; i++)
    {
        QLabel* label = new QLabel(QString(labels[i])+":");
        KeyMapButton* btn = new KeyMapButton(&keymap[i], ishotkey);

        group_layout->addWidget(label, i, 0);
        group_layout->addWidget(btn, i, 1);
    }
    group_layout->setRowStretch(num, 1);
    group->setLayout(group_layout);
    group->setMinimumWidth(275);

    group = new QGroupBox("Joystick mappings:");
    main_layout->addWidget(group);
    group_layout = new QGridLayout();
    group_layout->setSpacing(1);
    for (int i = 0; i < num; i++)
    {
        QLabel* label = new QLabel(QString(labels[i])+":");
        JoyMapButton* btn = new JoyMapButton(&joymap[i], ishotkey);

        group_layout->addWidget(label, i, 0);
        group_layout->addWidget(btn, i, 1);
    }
    group_layout->setRowStretch(num, 1);
    group->setLayout(group_layout);
    group->setMinimumWidth(275);

    page->setLayout(main_layout);
}

void InputConfigDialog::on_InputConfigDialog_accepted()
{
    for (int i = 0; i < keypad_num; i++)
    {
        Config::KeyMapping[dskeyorder[i]] = keypadKeyMap[i];
        Config::JoyMapping[dskeyorder[i]] = keypadJoyMap[i];
    }

    for (int i = 0; i < hk_addons_num; i++)
    {
        Config::HKKeyMapping[hk_addons[i]] = addonsKeyMap[i];
        Config::HKJoyMapping[hk_addons[i]] = addonsJoyMap[i];
    }

    for (int i = 0; i < hk_general_num; i++)
    {
        Config::HKKeyMapping[hk_general[i]] = hkGeneralKeyMap[i];
        Config::HKJoyMapping[hk_general[i]] = hkGeneralJoyMap[i];
    }

    Config::JoystickID = Input::JoystickID;
    Config::Save();

    closeDlg();
}

void InputConfigDialog::on_InputConfigDialog_rejected()
{
    Input::JoystickID = Config::JoystickID;
    Input::OpenJoystick();

    closeDlg();
}

void InputConfigDialog::on_btnKeyMapSwitch_clicked()
{
    ui->stackMapping->setCurrentIndex(0);
}

void InputConfigDialog::on_btnJoyMapSwitch_clicked()
{
    ui->stackMapping->setCurrentIndex(1);
}

void InputConfigDialog::on_cbxJoystick_currentIndexChanged(int id)
{
    // prevent a spurious change
    if (ui->cbxJoystick->count() < 2) return;

    Input::JoystickID = id;
    Input::OpenJoystick();
}
