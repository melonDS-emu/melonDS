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

#ifndef MAPBUTTON_H
#define MAPBUTTON_H

#include <QPushButton>

#include <SDL2/SDL.h>

#include "Platform.h"
#include "EmuInstance.h"

class InputConfigDialog;

class KeyMapButton : public QPushButton
{
    Q_OBJECT

public:
    KeyMapButton(int* mapping, bool hotkey) : QPushButton()
    {
        this->mapping = mapping;
        this->isHotkey = hotkey;

        setCheckable(true);
        setText(mappingText());
        setFocusPolicy(Qt::StrongFocus); //Fixes binding keys in macOS

        connect(this, &KeyMapButton::clicked, this, &KeyMapButton::onClick);
    }

    ~KeyMapButton()
    {
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (!isChecked()) return QPushButton::keyPressEvent(event);

        Log(melonDS::Platform::Debug, "KEY PRESSED = %08X %08X | %08X %08X %08X\n", event->key(), (int)event->modifiers(), event->nativeVirtualKey(), event->nativeModifiers(), event->nativeScanCode());

        int key = event->key();

        int mod = event->modifiers();
        bool ismod = (key == Qt::Key_Control ||
                      key == Qt::Key_Alt ||
                      key == Qt::Key_AltGr ||
                      key == Qt::Key_Shift ||
                      key == Qt::Key_Meta);

        /* MelonPrimeDS CommentOuted
        if (!mod)
        {
        */

            if (key == Qt::Key_Escape) { click(); return; }
            if (key == Qt::Key_Backspace) { *mapping = -1; click(); return; }
        
        /* MelonPrimeDS CommentOuted
        }

        if (isHotkey)
        {
            if (ismod)
                return;
        }

        */
        if (!ismod)
            key |= mod;
        else if (isRightModKey(event))
            key |= (1<<31);

        *mapping = key;
        click();
    }

    // MelonPrimeDS
    void mousePressEvent(QMouseEvent* event) override {
        if (!isChecked()) return QPushButton::mousePressEvent(event);

        // Log(melonDS::Platform::Debug, "MOUSE BUTTON PRESSED = %08X\n", event->button());

        *mapping = (int)event->button() | 0xF0000000;
        click();
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        if (isChecked())
        {
            // if we lost the focus while mapping, consider it 'done'
            click();
        }

        QPushButton::focusOutEvent(event);
    }

    bool focusNextPrevChild(bool next) override { return false; }

private slots:
    void onClick()
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

private:
    QString mappingText()
    {
        int key = *mapping;

        if (key == -1) return "None";

        /* MelonPrimeDS { */
        auto getMouseButtonName = [](Qt::MouseButton button) -> std::optional<QString> {
            static const struct {
                Qt::MouseButton button;
                const char* name;
            } mouseButtons[] = {
                {Qt::LeftButton, "LeftButton"},
                {Qt::RightButton, "RightButton"},
                {Qt::MiddleButton, "MiddleButton"},
                {Qt::BackButton, "BackButton"},
                {Qt::ForwardButton, "ForwardButton"},
                {Qt::ExtraButton4, "ExtraButton4"},
                {Qt::ExtraButton5, "ExtraButton5"},
                {Qt::ExtraButton6, "ExtraButton6"},
                {Qt::ExtraButton7, "ExtraButton7"},
                {Qt::ExtraButton8, "ExtraButton8"},
                {Qt::ExtraButton9, "ExtraButton9"},
                {Qt::ExtraButton10, "ExtraButton10"},
                {Qt::ExtraButton11, "ExtraButton11"},
                {Qt::ExtraButton12, "ExtraButton12"},
                {Qt::ExtraButton13, "ExtraButton13"},
                {Qt::ExtraButton14, "ExtraButton14"},
                {Qt::ExtraButton15, "ExtraButton15"},
                {Qt::ExtraButton16, "ExtraButton16"},
                {Qt::ExtraButton17, "ExtraButton17"},
                {Qt::ExtraButton18, "ExtraButton18"},
                {Qt::ExtraButton19, "ExtraButton19"},
                {Qt::ExtraButton20, "ExtraButton20"},
                {Qt::ExtraButton21, "ExtraButton21"},
                {Qt::ExtraButton22, "ExtraButton22"},
                {Qt::ExtraButton23, "ExtraButton23"},
                {Qt::ExtraButton24, "ExtraButton24"}
            };

            for (const auto& mb : mouseButtons) {
                if (button == mb.button) {
                    return QString("Mouse ") + mb.name;
                }
            }
            return std::nullopt;
            };
        auto mouseButton = key & ~0xF0000000;
        if (auto name = getMouseButtonName(static_cast<Qt::MouseButton>(mouseButton))) {
            return *name;
        }
        /*
        auto mouseButton = key & ~0xF0000000;
        if (mouseButton == Qt::LeftButton) return "Mouse Left";
        if (mouseButton == Qt::MiddleButton) return "Mouse Middle";
        if (mouseButton == Qt::RightButton) return "Mouse Right";
        if (mouseButton == Qt::ExtraButton1) return "Mouse 4";
        if (mouseButton == Qt::ExtraButton2) return "Mouse 5";
        */
        /* } MelonPrimeDS */

        QString isright = (key & (1<<31)) ? "Right " : "Left ";
        key &= ~(1<<31);

    #ifndef __APPLE__
        switch (key)
        {
        case Qt::Key_Control: return isright + "Ctrl";
        case Qt::Key_Alt:     return "Alt";
        case Qt::Key_AltGr:   return "AltGr";
        case Qt::Key_Shift:   return isright + "Shift";
        case Qt::Key_Meta:    return "Meta";
        }
    #else
        switch (key)
        {
        case Qt::Key_Control: return isright + "⌘";
        case Qt::Key_Alt:     return isright + "⌥";
        case Qt::Key_Shift:   return isright + "⇧";
        case Qt::Key_Meta:    return isright + "⌃";
        }
    #endif

        QKeySequence seq(key);
        QString ret = seq.toString(QKeySequence::NativeText);

        // weak attempt at detecting garbage key names
        //if (ret.length() == 2 && ret[0].unicode() > 0xFF)
        //    return QString("[%1]").arg(key, 8, 16);

        return ret.replace("&", "&&");
    }

    int* mapping;
    bool isHotkey;
};

class JoyMapButton : public QPushButton
{
    Q_OBJECT

public:
    JoyMapButton(int* mapping, bool hotkey) : QPushButton()
    {
        this->mapping = mapping;
        this->isHotkey = hotkey;

        // the parent will be set later when this button is added to a layout
        parentDialog = nullptr;

        setCheckable(true);
        setText(mappingText());
        setFocusPolicy(Qt::StrongFocus); //Fixes binding keys in macOS

        connect(this, &JoyMapButton::clicked, this, &JoyMapButton::onClick);

        timerID = 0;
    }

    ~JoyMapButton()
    {
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        if (event->spontaneous()) return;

        QWidget* w = parentWidget();
        for (;;)
        {
            parentDialog = qobject_cast<InputConfigDialog*>(w);
            if (parentDialog) break;
            w = w->parentWidget();
            if (!w) break;
        }
    }

    void keyPressEvent(QKeyEvent* event) override
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

    void focusOutEvent(QFocusEvent* event) override
    {
        if (isChecked())
        {
            // if we lost the focus while mapping, consider it 'done'
            click();
        }

        QPushButton::focusOutEvent(event);
    }

    void timerEvent(QTimerEvent* event) override
    {
        SDL_Joystick* joy = parentDialog->getJoystick();
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

            if (diff >= 16384)
            {
                if (axesRest[i] < -16384) // Trigger
                {
                    *mapping = (oldmap & 0xFFFF) | 0x10000 | (2 << 20) | (i << 24);
                }
                else // Analog stick
                {
                    int axistype;
                    if (axisval > 0) axistype = 0;
                    else             axistype = 1;

                    *mapping = (oldmap & 0xFFFF) | 0x10000 | (axistype << 20) | (i << 24);
                }

                click();
                return;
            }
        }
    }

    bool focusNextPrevChild(bool next) override { return false; }

private slots:
    void onClick()
    {
        if (isChecked())
        {
            setText("[press button/axis]");
            timerID = startTimer(50);

            memset(axesRest, 0, sizeof(axesRest));

            SDL_Joystick* joy = parentDialog->getJoystick();
            if (joy && SDL_JoystickGetAttached(joy))
            {
                int naxes = SDL_JoystickNumAxes(joy);
                if (naxes > 16) naxes = 16;
                for (int a = 0; a < naxes; a++)
                {
                    axesRest[a] = SDL_JoystickGetAxis(joy, a);
                }
            }
        }
        else
        {
            setText(mappingText());
            if (timerID) { killTimer(timerID); timerID = 0; }
        }
    }

private:
    QString mappingText()
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

    InputConfigDialog* parentDialog;

    int* mapping;
    bool isHotkey;

    int timerID;
    int axesRest[16];
};

#endif // MAPBUTTON_H
