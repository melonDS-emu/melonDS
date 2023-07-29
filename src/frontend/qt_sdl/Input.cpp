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

#include <QKeyEvent>
#include <SDL2/SDL.h>

#include "Input.h"
#include "Config.h"


namespace Input
{

int JoystickID;
SDL_Joystick* Joystick = nullptr;
SDL_GameController* GameController = nullptr;

u32 KeyInputMask, JoyInputMask;
u32 KeyHotkeyMask, JoyHotkeyMask;
u32 HotkeyMask, LastHotkeyMask;
u32 HotkeyPress, HotkeyRelease;

u32 InputMask;

u8 JoyTouchX, JoyTouchY;
bool JoyTouching;
bool JoyTouchReleased;
JoystickTouchMode JoystickTouch;

bool touchpadTouching;
float touchpadLastX;
float touchpadLastY;

void Init()
{
    KeyInputMask = 0xFFF;
    JoyInputMask = 0xFFF;
    InputMask = 0xFFF;

    KeyHotkeyMask = 0;
    JoyHotkeyMask = 0;
    HotkeyMask = 0;
    LastHotkeyMask = 0;

    JoyTouchX = 0;
    JoyTouchY = 0;
    JoyTouching = false;
    JoyTouchReleased = false;

    touchpadLastX = 0;
    touchpadLastY = 0;
    touchpadTouching = false;
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

    if (Joystick != nullptr)
    {
        if (!SDL_IsGameController(JoystickID))
            return;

        GameController = SDL_GameControllerOpen(JoystickID);
    }
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

bool JoystickTouchModeAvailable(JoystickTouchMode mode)
{
    if (GameController == nullptr)
        return false;

    switch (mode.mode) {
        case JoystickTouchMode::analogStick:
        {
            SDL_GameControllerAxis xAxis = mode.stick == JoystickTouchMode::leftStick
                    ? SDL_CONTROLLER_AXIS_LEFTX
                    : SDL_CONTROLLER_AXIS_RIGHTX;
            SDL_GameControllerAxis yAxis = mode.stick == JoystickTouchMode::leftStick
                    ? SDL_CONTROLLER_AXIS_LEFTY
                    : SDL_CONTROLLER_AXIS_RIGHTY;

            return SDL_GameControllerHasAxis(GameController, xAxis)
                   && SDL_GameControllerHasAxis(GameController, yAxis);
        }
        case JoystickTouchMode::touchpad:
            if (mode.style == JoystickTouchMode::relative && !SDL_GameControllerHasButton(GameController, SDL_CONTROLLER_BUTTON_TOUCHPAD))
                return false;

            return SDL_GameControllerGetNumTouchpads(GameController) != 0;
        case JoystickTouchMode::gyroscope:
            return SDL_GameControllerHasSensor(GameController, SDL_SENSOR_GYRO);
        case JoystickTouchMode::none:
            return true;
    }
}

bool SetJoystickTouchMode(JoystickTouchMode mode)
{
    JoystickTouch = mode;

    if (!JoystickTouchModeAvailable(mode))
        return false;

    SDL_GameControllerSetSensorEnabled(GameController, SDL_SENSOR_GYRO, mode.mode == JoystickTouchMode::gyroscope ? SDL_TRUE : SDL_FALSE);
    return true;
}

constexpr float psTouchpadAspectMul = ((52.f / 23.f) / (4.f / 3.f));

// The touchpad is about 52x23 mm on the PS4 controller, and the DualSense looks similar
// so correct it to be more like the DS's aspect ratio
float TouchpadCorrectAspect(float x)
{
    SDL_GameControllerType type = SDL_GameControllerGetType(GameController);

    if (type != SDL_CONTROLLER_TYPE_PS4 && type != SDL_CONTROLLER_TYPE_PS5)
        return x;

    float pos = (x - 0.5f) * psTouchpadAspectMul;
    pos = std::clamp(pos, -.5f, .5f);
    return pos + 0.5f;
}

float JoyTouchXFloat, JoyTouchYFloat;

void HandleRelativeInput(float dx, float dy, float sensitivity)
{
    // TODO: Screen rotation

    JoyTouchXFloat = std::clamp((float) JoyTouchXFloat + (dx * sensitivity), 0.f, 255.f);
    JoyTouchYFloat = std::clamp((float) JoyTouchYFloat + (dy * sensitivity), 0.f, 191.f);

    JoyTouchX = (u8) std::round(JoyTouchXFloat);
    JoyTouchY = (u8) std::round(JoyTouchYFloat);
}

void UpdateJoystickTouch()
{
    bool newTouching = false;

    JoyTouchReleased = false;

    auto mode = JoystickTouch.mode;
    auto style = JoystickTouch.style;
    auto stick = JoystickTouch.stick;

    if (!JoystickTouchModeAvailable(JoystickTouch))
    {
        if (JoyTouching)
        {
            JoyTouchReleased = true;
            JoyTouching = false;
        }

        return;
    }

    if (mode == JoystickTouchMode::touchpad)
    {
        u8 state;
        float x, y, pressure;

        SDL_GameControllerGetTouchpadFinger(GameController, 0, 0, &state, &x, &y, &pressure);

        if (style == JoystickTouchMode::relative)
        {
            if (state == 1)
            {
                float dx, dy;

                if (!touchpadTouching)
                {
                    touchpadTouching = true;
                    dx = 0;
                    dy = 0;
                }
                else
                {
                    dx = (x - touchpadLastX) * psTouchpadAspectMul;
                    dy = y - touchpadLastY;
                }

                HandleRelativeInput(dx, dy, JoystickTouch.sensitivity);
                newTouching = SDL_GameControllerGetButton(GameController, SDL_CONTROLLER_BUTTON_TOUCHPAD);

                touchpadLastX = x;
                touchpadLastY = y;
            }
            else
            {
                touchpadLastX = 0;
                touchpadLastY = 0;
                touchpadTouching = false;
            }
        }
        else
        {
            if (SDL_GameControllerHasButton(GameController, SDL_CONTROLLER_BUTTON_TOUCHPAD))
                newTouching = SDL_GameControllerGetButton(GameController, SDL_CONTROLLER_BUTTON_TOUCHPAD);
            else
                newTouching = state == 1;

            JoyTouchX = (u8) round(TouchpadCorrectAspect(x) * 256.f);
            JoyTouchY = (u8) round(y * 192.f);
        }
    }
    else if (mode == JoystickTouchMode::analogStick)
    {
        SDL_GameControllerAxis axisX = stick == JoystickTouchMode::leftStick
            ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX;
        SDL_GameControllerAxis axisY = stick == JoystickTouchMode::leftStick
            ? SDL_CONTROLLER_AXIS_LEFTY : SDL_CONTROLLER_AXIS_RIGHTY;

        s16 x = SDL_GameControllerGetAxis(GameController, axisX);
        s16 y = SDL_GameControllerGetAxis(GameController, axisY);
        float fx = ((float) x) / 32768.f;
        float fy = ((float) y) / 32768.f;

        if (style == JoystickTouchMode::relative)
        {
            HandleRelativeInput(fx, fy, JoystickTouch.sensitivity);
        }
        else
        {
            JoyTouchX = (u8) std::round(((fx + 1.0f) / 2) * 256.f);
            JoyTouchY = (u8) std::round(((fy + 1.0f) / 2) * 192.f);
        }

        newTouching = JoystickButtonDown(JoystickTouch.touchButton);
    }
    else if (mode == JoystickTouchMode::gyroscope)
    {
        float gyroPos[3] = {0};

        SDL_GameControllerSetSensorEnabled(GameController, SDL_SENSOR_GYRO, SDL_TRUE);

        SDL_GameControllerGetSensorData(GameController, SDL_SENSOR_GYRO, (float*) &gyroPos, 3);
        HandleRelativeInput(-gyroPos[1], -gyroPos[0], JoystickTouch.sensitivity);

        newTouching = JoystickButtonDown(JoystickTouch.touchButton);
    }

    if (!newTouching && JoyTouching)
        JoyTouchReleased = true;

    JoyTouching = newTouching;
}

void Process()
{
    SDL_JoystickUpdate();

    if (Joystick)
    {
        if (!SDL_JoystickGetAttached(Joystick))
        {
            if (GameController != nullptr)
            {
                SDL_GameControllerClose(GameController);
                GameController = nullptr;
            }

            SDL_JoystickClose(Joystick);
            Joystick = nullptr;
        }
    }
    if (!Joystick && (SDL_NumJoysticks() > 0))
    {
        JoystickID = Config::JoystickID;
        OpenJoystick();
    }

    JoyInputMask = 0xFFF;
    for (int i = 0; i < 12; i++)
        if (JoystickButtonDown(Config::JoyMapping[i]))
            JoyInputMask &= ~(1<<i);

    InputMask = KeyInputMask & JoyInputMask;

    JoyHotkeyMask = 0;
    for (int i = 0; i < HK_MAX; i++)
        if (JoystickButtonDown(Config::HKJoyMapping[i]))
            JoyHotkeyMask |= (1<<i);

    HotkeyMask = KeyHotkeyMask | JoyHotkeyMask;
    HotkeyPress = HotkeyMask & ~LastHotkeyMask;
    HotkeyRelease = LastHotkeyMask & ~HotkeyMask;
    LastHotkeyMask = HotkeyMask;

    if (JoystickTouch.mode != JoystickTouchMode::none)
        UpdateJoystickTouch();
}

void SaveJoystickTouchMode()
{
    using Mode = JoystickTouchMode;
    Mode mode = JoystickTouch;

    Config::JoyTouchMode = mode.mode;
    Config::JoyTouchMovementStyle = mode.style;
    Config::JoyTouchStick = mode.stick;
    Config::JoyTouchButton = mode.touchButton;
    Config::JoyTouchRecenterButton = mode.recenterButton;
    Config::JoyTouchSensitivity = (int) std::round(mode.sensitivity * 100.f);
}

void LoadJoystickTouchMode()
{
    using Mode = JoystickTouchMode;
    Mode mode = {
        .mode = (Mode::Mode) Config::JoyTouchMode,
        .style = (Mode::Style) Config::JoyTouchMovementStyle,
        .stick = (Mode::AnalogStick) Config::JoyTouchStick,
        .sensitivity = ((float) Config::JoyTouchSensitivity) / 100.f,
        .touchButton = Config::JoyTouchButton,
        .recenterButton = Config::JoyTouchRecenterButton
    };

    SetJoystickTouchMode(mode);
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
