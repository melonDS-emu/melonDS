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

#ifndef INPUT_H
#define INPUT_H

#include <SDL2/SDL.h>

#include "types.h"

namespace Input
{

using namespace melonDS;
extern int JoystickID;
extern SDL_Joystick* Joystick;

extern u32 InputMask;

void Init();

// set joystickID before calling openJoystick()
void OpenJoystick();
void CloseJoystick();

void KeyPress(QKeyEvent* event);
void KeyRelease(QKeyEvent* event);
void KeyReleaseAll();

void Process();

bool HotkeyDown(int id);
bool HotkeyPressed(int id);
bool HotkeyReleased(int id);

bool IsRightModKey(QKeyEvent* event);

}

#endif // INPUT_H
