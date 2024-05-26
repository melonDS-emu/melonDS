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

#include <QKeyEvent>
#include <SDL2/SDL.h>

#include "Input.h"
#include "Config.h"

using namespace melonDS;

namespace Input
{

int JoystickID;
SDL_Joystick* Joystick = nullptr;

u32 KeyInputMask, JoyInputMask;
u32 KeyHotkeyMask, JoyHotkeyMask;
u32 HotkeyMask, LastHotkeyMask;
u32 HotkeyPress, HotkeyRelease;

u32 InputMask;


void Init()
{
    KeyInputMask = 0xFFF;
    JoyInputMask = 0xFFF;
    InputMask = 0xFFF;

    KeyHotkeyMask = 0;
    JoyHotkeyMask = 0;
    HotkeyMask = 0;
    LastHotkeyMask = 0;
}


void OpenJoystick()
{
    if (Joystick) SDL_JoystickClose(Joystick);

    int num = SDL_NumJoysticks();
    if (num < 1)
    {
        Joystick = nullptr;
        return;
    }

    if (JoystickID >= num)
        JoystickID = 0;

    Joystick = SDL_JoystickOpen(JoystickID);
}

void CloseJoystick()
{
    if (Joystick)
    {
        SDL_JoystickClose(Joystick);
        Joystick = nullptr;
    }
}


int GetEventKeyVal(QKeyEvent* event)
{
    int key = event->key();
    int mod = event->modifiers();
    bool ismod = (key == Qt::Key_Control ||
                  key == Qt::Key_Alt ||
                  key == Qt::Key_AltGr ||
                  key == Qt::Key_Shift ||
                  key == Qt::Key_Meta);

    if (!ismod)
        key |= mod;
    else if (Input::IsRightModKey(event))
        key |= (1<<31);

    return key;
}

void KeyPress(QKeyEvent* event)
{
    int keyHK = GetEventKeyVal(event);
    int keyKP = keyHK;
    if (event->modifiers() != Qt::KeypadModifier)
        keyKP &= ~event->modifiers();

    for (int i = 0; i < 12; i++)
        if (keyKP == Config::KeyMapping[i])
            KeyInputMask &= ~(1<<i);

    for (int i = 0; i < HK_MAX; i++)
        if (keyHK == Config::HKKeyMapping[i])
            KeyHotkeyMask |= (1<<i);
}

void KeyRelease(QKeyEvent* event)
{
    int keyHK = GetEventKeyVal(event);
    int keyKP = keyHK;
    if (event->modifiers() != Qt::KeypadModifier)
        keyKP &= ~event->modifiers();

    for (int i = 0; i < 12; i++)
        if (keyKP == Config::KeyMapping[i])
            KeyInputMask |= (1<<i);

    for (int i = 0; i < HK_MAX; i++)
        if (keyHK == Config::HKKeyMapping[i])
            KeyHotkeyMask &= ~(1<<i);
}

void KeyReleaseAll()
{
    KeyInputMask = 0xFFF;
    KeyHotkeyMask = 0;
}

bool JoystickButtonDown(int val)
{
    if (val == -1) return false;

    bool hasbtn = ((val & 0xFFFF) != 0xFFFF);

    if (hasbtn)
    {
        if (val & 0x100)
        {
            int hatnum = (val >> 4) & 0xF;
            int hatdir = val & 0xF;
            Uint8 hatval = SDL_JoystickGetHat(Joystick, hatnum);

            bool pressed = false;
            if      (hatdir == 0x1) pressed = (hatval & SDL_HAT_UP);
            else if (hatdir == 0x4) pressed = (hatval & SDL_HAT_DOWN);
            else if (hatdir == 0x2) pressed = (hatval & SDL_HAT_RIGHT);
            else if (hatdir == 0x8) pressed = (hatval & SDL_HAT_LEFT);

            if (pressed) return true;
        }
        else
        {
            int btnnum = val & 0xFFFF;
            Uint8 btnval = SDL_JoystickGetButton(Joystick, btnnum);

            if (btnval) return true;
        }
    }

    if (val & 0x10000)
    {
        int axisnum = (val >> 24) & 0xF;
        int axisdir = (val >> 20) & 0xF;
        Sint16 axisval = SDL_JoystickGetAxis(Joystick, axisnum);

        switch (axisdir)
        {
        case 0: // positive
            if (axisval > 16384) return true;
            break;

        case 1: // negative
            if (axisval < -16384) return true;
            break;

        case 2: // trigger
            if (axisval > 0) return true;
            break;
        }
    }

    return false;
}

void Process()
{
    SDL_JoystickUpdate();

    if (Joystick)
    {
        if (!SDL_JoystickGetAttached(Joystick))
        {
            SDL_JoystickClose(Joystick);
            Joystick = NULL;
        }
    }
    if (!Joystick && (SDL_NumJoysticks() > 0))
    {
        JoystickID = Config::JoystickID;
        OpenJoystick();
    }

    JoyInputMask = 0xFFF;
    if (Joystick)
    {
        for (int i = 0; i < 12; i++)
            if (JoystickButtonDown(Config::JoyMapping[i]))
                JoyInputMask &= ~(1 << i);
    }

    InputMask = KeyInputMask & JoyInputMask;

    JoyHotkeyMask = 0;
    if (Joystick)
    {
        for (int i = 0; i < HK_MAX; i++)
            if (JoystickButtonDown(Config::HKJoyMapping[i]))
                JoyHotkeyMask |= (1 << i);
    }

    HotkeyMask = KeyHotkeyMask | JoyHotkeyMask;
    HotkeyPress = HotkeyMask & ~LastHotkeyMask;
    HotkeyRelease = LastHotkeyMask & ~HotkeyMask;
    LastHotkeyMask = HotkeyMask;
}


bool HotkeyDown(int id)     { return HotkeyMask    & (1<<id); }
bool HotkeyPressed(int id)  { return HotkeyPress   & (1<<id); }
bool HotkeyReleased(int id) { return HotkeyRelease & (1<<id); }


// distinguish between left and right modifier keys (Ctrl, Alt, Shift)
// Qt provides no real cross-platform way to do this, so here we go
// for Windows and Linux we can distinguish via scancodes (but both
// provide different scancodes)
#ifdef __WIN32__
bool IsRightModKey(QKeyEvent* event)
{
    quint32 scan = event->nativeScanCode();
    return (scan == 0x11D || scan == 0x138 || scan == 0x36);
}
#elif __APPLE__
bool IsRightModKey(QKeyEvent* event)
{
    quint32 scan = event->nativeVirtualKey();
    return (scan == 0x36 || scan == 0x3C || scan == 0x3D || scan == 0x3E);
}
#else
bool IsRightModKey(QKeyEvent* event)
{
    quint32 scan = event->nativeScanCode();
    return (scan == 0x69 || scan == 0x6C || scan == 0x3E);
}
#endif

}
