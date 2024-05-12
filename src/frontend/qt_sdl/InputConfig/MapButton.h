/*
    Copyright 2016-2023 melonDS team

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

#include "Input.h"
#include "Platform.h"

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

    int* mapping;
    bool isHotkey;

    int timerID;
    int axesRest[16];
};

#endif // MAPBUTTON_H
