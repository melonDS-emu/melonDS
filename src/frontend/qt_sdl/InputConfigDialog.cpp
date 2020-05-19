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

#include "types.h"
#include "Config.h"
#include "PlatformConfig.h"

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
    HK_Lid,
    HK_Mic,
};

const char* hk_general_labels[] =
{
    "Pause/resume",
    "Reset",
    "Fast forward",
    "Toggle FPS limit",
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

    for (int i = 0; i < 6; i++)
    {
        hkGeneralKeyMap[i] = Config::HKKeyMapping[hk_general[i]];
        hkGeneralJoyMap[i] = Config::HKJoyMapping[hk_general[i]];
    }

    populatePage(ui->tabInput, 12, dskeylabels, keypadKeyMap, keypadJoyMap);
    populatePage(ui->tabAddons, 2, hk_addons_labels, addonsKeyMap, addonsJoyMap);
    populatePage(ui->tabHotkeysGeneral, 6, hk_general_labels, hkGeneralKeyMap, hkGeneralJoyMap);
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
        KeyMapButton* btn = new KeyMapButton(nullptr, &keymap[i], ishotkey);

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
        QPushButton* btn = new QPushButton();

        group_layout->addWidget(label, i, 0);
        group_layout->addWidget(btn, i, 1);

        btn->setText(joyMappingName(joymap[i]));

        //btn->setProperty("mapping", QVariant(&joymap[i]));
        //btn->setProperty("isHotkey", QVariant(ishotkey));
    }
    group_layout->setRowStretch(num, 1);
    group->setLayout(group_layout);
    group->setMinimumWidth(275);

    page->setLayout(main_layout);
}

QString InputConfigDialog::joyMappingName(int id)
{
    if (id < 0)
    {
        return "None";
    }

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

void InputConfigDialog::on_InputConfigDialog_accepted()
{
    closeDlg();
}

void InputConfigDialog::on_InputConfigDialog_rejected()
{
    closeDlg();
}


KeyMapButton::KeyMapButton(QWidget* parent, int* mapping, bool hotkey) : QPushButton(parent)
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
printf("KEY PRESSED = %08X %08X | %08X %08X %08X | %08X\n", event->key(), event->modifiers(), event->nativeVirtualKey(), event->nativeModifiers(), event->nativeScanCode(), Qt::SHIFT);
    int key = event->key();
    bool ismod = (key == Qt::Key_Control ||
                  key == Qt::Key_Alt ||
                  key == Qt::Key_AltGr ||
                  key == Qt::Key_Shift ||
                  key == Qt::Key_Meta);

    if (isHotkey)
    {
        if (ismod)
            return;
    }

    if (!ismod)
        key |= event->modifiers();

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

    switch (key)
    {
    case -1: return "None";

    case Qt::Key_Control: return "Ctrl";
    case Qt::Key_Alt:     return "Alt";
    case Qt::Key_AltGr:   return "AltGr";
    case Qt::Key_Shift:   return "Shift";
    case Qt::Key_Meta:    return "Meta";
    }

    QKeySequence seq(key);
    QString ret = seq.toString();
    
    // weak attempt at detecting garbage key names
    if (ret.length() == 2 && ret[0].unicode() > 0xFF)
        return QString("[%1]").arg(key, 8, 16);
    
    return ret.replace("&", "&&");
}
