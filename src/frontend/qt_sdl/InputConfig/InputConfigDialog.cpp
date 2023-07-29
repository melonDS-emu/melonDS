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

#include "MapButton.h"
#include "Input.h"
#include "InputConfigDialog.h"
#include "ui_InputConfigDialog.h"


InputConfigDialog* InputConfigDialog::currentDlg = nullptr;

const int dskeyorder[12] = {0, 1, 10, 11, 5, 4, 6, 7, 9, 8, 2, 3};
const char* dskeylabels[12] = {"A", "B", "X", "Y", "Left", "Right", "Up", "Down", "L", "R", "Select", "Start"};

InputConfigDialog::InputConfigDialog(QWidget* parent) : QDialog(parent), ui(new Ui::InputConfigDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    for (int i = 0; i < keypad_num; i++)
    {
        keypadKeyMap[i] = Config::KeyMapping[dskeyorder[i]];
        keypadJoyMap[i] = Config::JoyMapping[dskeyorder[i]];
    }

    int i = 0;
    for (int hotkey : hk_addons)
    {
        addonsKeyMap[i] = Config::HKKeyMapping[hotkey];
        addonsJoyMap[i] = Config::HKJoyMapping[hotkey];
        i++;
    }

    i = 0;
    for (int hotkey : hk_general)
    {
        hkGeneralKeyMap[i] = Config::HKKeyMapping[hotkey];
        hkGeneralJoyMap[i] = Config::HKJoyMapping[hotkey];
        i++;
    }

    populatePage(ui->tabAddons, hk_addons_labels, addonsKeyMap, addonsJoyMap);
    populatePage(ui->tabHotkeysGeneral, hk_general_labels, hkGeneralKeyMap, hkGeneralJoyMap);

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

    QLayout* layout = ui->btnJoyTouchPress->parentWidget()->layout();

    recenterBtn = new JoyMapButton(&recenterMapping, false);
    joyPressBtn = new JoyMapButton(&joyPressMapping, false);
    layout->replaceWidget(ui->btnJoyTouchRecenter, recenterBtn);
    layout->replaceWidget(ui->btnJoyTouchPress, joyPressBtn);

    joyTouchModeToUI(Input::JoystickTouch);
    updateJoyTouchOptions();
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

void InputConfigDialog::populatePage(QWidget* page,
    const std::initializer_list<const char*>& labels,
    int* keymap, int* joymap)
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
    int i = 0;
    for (const char* labelStr : labels)
    {
        QLabel* label = new QLabel(QString(labelStr)+":");
        KeyMapButton* btn = new KeyMapButton(&keymap[i], ishotkey);

        group_layout->addWidget(label, i, 0);
        group_layout->addWidget(btn, i, 1);
        i++;
    }
    group_layout->setRowStretch(labels.size(), 1);
    group->setLayout(group_layout);
    group->setMinimumWidth(275);

    group = new QGroupBox("Joystick mappings:");
    main_layout->addWidget(group);
    group_layout = new QGridLayout();
    group_layout->setSpacing(1);
    i = 0;
    for (const char* labelStr : labels)
    {
        QLabel* label = new QLabel(QString(labelStr)+":");
        JoyMapButton* btn = new JoyMapButton(&joymap[i], ishotkey);

        group_layout->addWidget(label, i, 0);
        group_layout->addWidget(btn, i, 1);
        i++;
    }
    group_layout->setRowStretch(labels.size(), 1);
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

    int i = 0;
    for (int hotkey : hk_addons)
    {
        Config::HKKeyMapping[hotkey] = addonsKeyMap[i];
        Config::HKJoyMapping[hotkey] = addonsJoyMap[i];
        i++;
    }

    i = 0;
    for (int hotkey : hk_general)
    {
        Config::HKKeyMapping[hotkey] = hkGeneralKeyMap[i];
        Config::HKJoyMapping[hotkey] = hkGeneralJoyMap[i];
        i++;
    }

    Config::JoystickID = Input::JoystickID;

    Input::SetJoystickTouchMode(joyTouchModeFromUI());
    Input::SaveJoystickTouchMode();

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

    updateJoyTouchOptions();
}

Input::JoystickTouchMode InputConfigDialog::joyTouchModeFromUI()
{
    using Mode = Input::JoystickTouchMode;

    Mode mode = {};

    QAbstractButton* uiMode = ui->grpJoyTouchInput->checkedButton();
    QAbstractButton* uiStyle = ui->grpJoyTouchStyle->checkedButton();
    QAbstractButton* uiStick = ui->grpJoyTouchStick->checkedButton();

    if (uiMode == ui->rbTouchNone)
        mode.mode = Mode::none;
    else if (uiMode == ui->rbTouchAnalog)
        mode.mode = Mode::analogStick;
    else if (uiMode == ui->rbTouchTouchpad)
        mode.mode = Mode::touchpad;
    else if (uiMode == ui->rbTouchGyro)
        mode.mode = Mode::gyroscope;

    mode.style = uiStyle == ui->rbTouchAbsolute ? Mode::absolute : Mode::relative;
    mode.stick = uiStick == ui->rbStickLeft ? Mode::leftStick : Mode::rightStick;

    mode.touchButton = joyPressMapping;
    mode.recenterButton = recenterMapping;

    return mode;
}

void InputConfigDialog::joyTouchModeToUI(Input::JoystickTouchMode mode)
{
    using Mode = Input::JoystickTouchMode;

    switch (mode.mode)
    {
        case Mode::none:
            ui->rbTouchNone->setChecked(true);
            break;
        case Mode::analogStick:
            ui->rbTouchAnalog->setChecked(true);
            break;
        case Mode::touchpad:
            ui->rbTouchTouchpad->setChecked(true);
            break;
        case Mode::gyroscope:
            ui->rbTouchGyro->setChecked(true);
            break;
    }

    if (mode.style == Mode::relative)
        ui->rbTouchRelative->setChecked(true);
    else
        ui->rbTouchAbsolute->setChecked(true);

    if (mode.stick == Mode::leftStick)
        ui->rbStickLeft->setChecked(true);
    else
        ui->rbStickRight->setChecked(true);

    recenterMapping = mode.recenterButton;
    joyPressMapping = mode.touchButton;
}

void InputConfigDialog::updateJoyTouchOptions()
{
    using namespace Input;
    using Mode = JoystickTouchMode;

    JoystickTouchMode tempMode = {};

    QAbstractButton* currentMode = ui->grpJoyTouchInput->checkedButton();
    bool relative = ui->grpJoyTouchStyle->checkedButton() == ui->rbTouchRelative;

    JoystickTouchMode analogMode = { .mode = JoystickTouchMode::gyroscope };

    bool currentAvailable = JoystickTouchModeAvailable(JoystickTouch);

    bool styleEnabled = (currentMode == ui->rbTouchAnalog || currentMode == ui->rbTouchTouchpad) && currentAvailable;
    bool stickEnabled = currentMode == ui->rbTouchAnalog && currentAvailable;
    bool recenterEnabled = (currentMode == ui->rbTouchGyro || relative) && currentAvailable;
    bool pressEnabled = currentMode != ui->rbTouchNone && currentAvailable;

    tempMode.mode = Mode::analogStick;
    tempMode.stick = Mode::leftStick;
    bool leftAvailable = JoystickTouchModeAvailable(tempMode);
    tempMode.stick = Mode::rightStick;
    bool rightAvailable = JoystickTouchModeAvailable(tempMode);
    tempMode.mode = Mode::touchpad;
    bool touchpadAvailable = JoystickTouchModeAvailable(tempMode);
    tempMode.mode = Mode::gyroscope;
    bool gyroAvailable = JoystickTouchModeAvailable(tempMode);

    ui->rbTouchAnalog->setEnabled(leftAvailable || rightAvailable);
    ui->rbTouchTouchpad->setEnabled(touchpadAvailable);
    ui->rbTouchGyro->setEnabled(gyroAvailable);

    ui->lblJoyTouchStyle->setEnabled(styleEnabled);
    ui->rbTouchRelative->setEnabled(styleEnabled);
    ui->rbTouchAbsolute->setEnabled(styleEnabled);

    ui->lblAnalogStick->setEnabled(stickEnabled && (leftAvailable || rightAvailable));
    ui->rbStickLeft->setEnabled(stickEnabled && leftAvailable);
    ui->rbStickRight->setEnabled(stickEnabled && rightAvailable);

    ui->lblRecenter->setEnabled(recenterEnabled);
    ui->lblJoyTouchPress->setEnabled(pressEnabled);

    recenterBtn->setEnabled(recenterEnabled);
    joyPressBtn->setEnabled(pressEnabled);
}

void InputConfigDialog::on_grpJoyTouchInput_buttonClicked(QAbstractButton* btn)
{
    updateJoyTouchOptions();
}

void InputConfigDialog::on_grpJoyTouchStyle_buttonClicked(QAbstractButton* btn)
{
    updateJoyTouchOptions();
}
