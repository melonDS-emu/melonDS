/*
    Copyright 2016-2020 Arisotura

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

#include <SDL2/SDL.h>

#include "types.h"
#include "Config.h"
#include "PlatformConfig.h"

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
    HK_FastForward,
    HK_FastForwardToggle,
    HK_FullscreenToggle,
    HK_Lid,
    HK_Mic,
};

const char* hk_general_labels[] =
{
    "Pause/resume",
    "Reset",
    "Fast forward",
    "Toggle FPS limit",
    "Toggle Fullscreen",
    "Close/open lid",
    "Microphone",
};


InputConfigDialog::InputConfigDialog(QWidget* parent) : QDialog(parent), ui(new Ui::InputConfigDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    for (int i = 0; i < 12; i++)
    {
        keypadKeyMap[i] = Config::KeyMapping[dskeyorder[i]];
        keypadJoyMap[i] = Config::JoyMapping[dskeyorder[i]];
    }

    for (int i = 0; i < 2; i++)
    {
        addonsKeyMap[i] = Config::HKKeyMapping[hk_addons[i]];
        addonsJoyMap[i] = Config::HKJoyMapping[hk_addons[i]];
    }

    for (int i = 0; i < 7; i++)
    {
        hkGeneralKeyMap[i] = Config::HKKeyMapping[hk_general[i]];
        hkGeneralJoyMap[i] = Config::HKJoyMapping[hk_general[i]];
    }

    populatePage(ui->tabInput, 12, dskeylabels, keypadKeyMap, keypadJoyMap);
    populatePage(ui->tabAddons, 2, hk_addons_labels, addonsKeyMap, addonsJoyMap);
    populatePage(ui->tabHotkeysGeneral, 7, hk_general_labels, hkGeneralKeyMap, hkGeneralJoyMap);

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
}

InputConfigDialog::~InputConfigDialog()
{
    delete ui;
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
    for (int i = 0; i < 12; i++)
    {
        Config::KeyMapping[dskeyorder[i]] = keypadKeyMap[i];
        Config::JoyMapping[dskeyorder[i]] = keypadJoyMap[i];
    }

    for (int i = 0; i < 2; i++)
    {
        Config::HKKeyMapping[hk_addons[i]] = addonsKeyMap[i];
        Config::HKJoyMapping[hk_addons[i]] = addonsJoyMap[i];
    }

    for (int i = 0; i < 7; i++)
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

void InputConfigDialog::on_cbxJoystick_currentIndexChanged(int id)
{
    // prevent a spurious change
    if (ui->cbxJoystick->count() < 2) return;

    Input::JoystickID = id;
    Input::OpenJoystick();
}


KeyMapButton::KeyMapButton(int* mapping, bool hotkey) : QPushButton()
{
    this->mapping = mapping;
    this->isHotkey = hotkey;

    setCheckable(true);
    setText(mappingText());

    connect(this, &KeyMapButton::clicked, this, &KeyMapButton::onClick);
}

KeyMapButton::~KeyMapButton()
{
}

void KeyMapButton::keyPressEvent(QKeyEvent* event)
{
    if (!isChecked()) return QPushButton::keyPressEvent(event);

    printf("KEY PRESSED = %08X %08X | %08X %08X %08X\n", event->key(), (int)event->modifiers(), event->nativeVirtualKey(), event->nativeModifiers(), event->nativeScanCode());

    int key = event->key();
    int mod = event->modifiers();
    bool ismod = (key == Qt::Key_Control ||
                  key == Qt::Key_Alt ||
                  key == Qt::Key_AltGr ||
                  key == Qt::Key_Shift ||
                  key == Qt::Key_Meta);

    if (!mod)
    {
        if (key == Qt::Key_Escape) { click(); return; }
        if (key == Qt::Key_Backspace) { *mapping = -1; click(); return; }
    }

    if (isHotkey)
    {
        if (ismod)
            return;
    }

    if (!ismod)
        key |= mod;
    else if (Input::IsRightModKey(event))
        key |= (1<<31);

    *mapping = key;
    click();
}

void KeyMapButton::focusOutEvent(QFocusEvent* event)
{
    if (isChecked())
    {
        // if we lost the focus while mapping, consider it 'done'
        click();
    }

    QPushButton::focusOutEvent(event);
}

void KeyMapButton::onClick()
{
    if (isChecked())
    {
        setText("[press key]");
    }
    else
    {
        setText(mappingText());
    }
}

QString KeyMapButton::mappingText()
{
    int key = *mapping;

    if (key == -1) return "None";

    QString isright = (key & (1<<31)) ? "Right " : "Left ";
    key &= ~(1<<31);

    switch (key)
    {
    case Qt::Key_Control: return isright + "Ctrl";
    case Qt::Key_Alt:     return "Alt";
    case Qt::Key_AltGr:   return "AltGr";
    case Qt::Key_Shift:   return isright + "Shift";
    case Qt::Key_Meta:    return "Meta";
    }

    QKeySequence seq(key);
    QString ret = seq.toString();

    // weak attempt at detecting garbage key names
    if (ret.length() == 2 && ret[0].unicode() > 0xFF)
        return QString("[%1]").arg(key, 8, 16);

    return ret.replace("&", "&&");
}


JoyMapButton::JoyMapButton(int* mapping, bool hotkey) : QPushButton()
{
    this->mapping = mapping;
    this->isHotkey = hotkey;

    setCheckable(true);
    setText(mappingText());

    connect(this, &JoyMapButton::clicked, this, &JoyMapButton::onClick);

    timerID = 0;
}

JoyMapButton::~JoyMapButton()
{
}

void JoyMapButton::keyPressEvent(QKeyEvent* event)
{
    if (!isChecked()) return QPushButton::keyPressEvent(event);

    int key = event->key();
    int mod = event->modifiers();

    if (!mod)
    {
        if (key == Qt::Key_Escape) { click(); return; }
        if (key == Qt::Key_Backspace) { *mapping = -1; click(); return; }
    }
}

void JoyMapButton::focusOutEvent(QFocusEvent* event)
{
    if (isChecked())
    {
        // if we lost the focus while mapping, consider it 'done'
        click();
    }

    QPushButton::focusOutEvent(event);
}

void JoyMapButton::timerEvent(QTimerEvent* event)
{
    SDL_Joystick* joy = Input::Joystick;
    if (!joy) { click(); return; }
    if (!SDL_JoystickGetAttached(joy)) { click(); return; }

    int oldmap;
    if (*mapping == -1) oldmap = 0xFFFF;
    else                oldmap = *mapping;

    int nbuttons = SDL_JoystickNumButtons(joy);
    for (int i = 0; i < nbuttons; i++)
    {
        if (SDL_JoystickGetButton(joy, i))
        {
            *mapping = (oldmap & 0xFFFF0000) | i;
            click();
            return;
        }
    }

    int nhats = SDL_JoystickNumHats(joy);
    if (nhats > 16) nhats = 16;
    for (int i = 0; i < nhats; i++)
    {
        Uint8 blackhat = SDL_JoystickGetHat(joy, i);
        if (blackhat)
        {
            if      (blackhat & 0x1) blackhat = 0x1;
            else if (blackhat & 0x2) blackhat = 0x2;
            else if (blackhat & 0x4) blackhat = 0x4;
            else                     blackhat = 0x8;

            *mapping = (oldmap & 0xFFFF0000) | 0x100 | blackhat | (i << 4);
            click();
            return;
        }
    }

    int naxes = SDL_JoystickNumAxes(joy);
    if (naxes > 16) naxes = 16;
    for (int i = 0; i < naxes; i++)
    {
        Sint16 axisval = SDL_JoystickGetAxis(joy, i);
        int diff = abs(axisval - axesRest[i]);

        if (axesRest[i] < -16384 && axisval >= 0)
        {
            *mapping = (oldmap & 0xFFFF) | 0x10000 | (2 << 20) | (i << 24);
            click();
            return;
        }
        else if (diff > 16384)
        {
            int axistype;
            if (axisval > 0) axistype = 0;
            else             axistype = 1;

            *mapping = (oldmap & 0xFFFF) | 0x10000 | (axistype << 20) | (i << 24);
            click();
            return;
        }
    }
}

void JoyMapButton::onClick()
{
    if (isChecked())
    {
        setText("[press button/axis]");
        timerID = startTimer(50);

        memset(axesRest, 0, sizeof(axesRest));
        if (Input::Joystick && SDL_JoystickGetAttached(Input::Joystick))
        {
            int naxes = SDL_JoystickNumAxes(Input::Joystick);
            if (naxes > 16) naxes = 16;
            for (int a = 0; a < naxes; a++)
            {
                axesRest[a] = SDL_JoystickGetAxis(Input::Joystick, a);
            }
        }
    }
    else
    {
        setText(mappingText());
        if (timerID) { killTimer(timerID); timerID = 0; }
    }
}

QString JoyMapButton::mappingText()
{
    int id = *mapping;

    if (id == -1) return "None";

    bool hasbtn = ((id & 0xFFFF) != 0xFFFF);
    QString str;

    if (hasbtn)
    {
        if (id & 0x100)
        {
            int hatnum = ((id >> 4) & 0xF) + 1;

            switch (id & 0xF)
            {
            case 0x1: str = "Hat %1 up"; break;
            case 0x2: str = "Hat %1 right"; break;
            case 0x4: str = "Hat %1 down"; break;
            case 0x8: str = "Hat %1 left"; break;
            }

            str = str.arg(hatnum);
        }
        else
        {
            str = QString("Button %1").arg((id & 0xFFFF) + 1);
        }
    }
    else
    {
        str = "";
    }

    if (id & 0x10000)
    {
        int axisnum = ((id >> 24) & 0xF) + 1;

        if (hasbtn) str += " / ";

        switch ((id >> 20) & 0xF)
        {
        case 0: str += QString("Axis %1 +").arg(axisnum); break;
        case 1: str += QString("Axis %1 -").arg(axisnum); break;
        case 2: str += QString("Trigger %1").arg(axisnum); break;
        }
    }

    return str;
}
