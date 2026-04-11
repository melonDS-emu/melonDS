/*
    Copyright 2016-2026 melonDS team

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

#ifndef CURSOR_H
#define CURSOR_H
#include <queue>
#include <array>
#include <vector>
class EmuInstance;

class Cursor
{
public:
  void update();
  void setEmuInstance(EmuInstance* emuInstance);
  void setRotation(int rot);
  void setLayout(int layout);
  int cursorPos[2];
  float normStylusDirection[2];
  bool cursorEnabled = true;
private:
  EmuInstance* emuInstance = nullptr;
  float rawCursorPos[2] = {127, 95};
  void touchScreen();
  void release();
  void clamp();
  void updateCursorPos();
  bool wasTouching;

  //Swipe in direction of x and y over the course of specified frames. Expects integer values between -1 and 1
  // void swipe(float x, float y, int frames);
  void circle(int direction); //0 is clockwise, 1 is counter clockwise
  void rub();
  void runMacro();
  std::vector<std::array<float, 2>> rotateVector(std::vector<std::array<float, 2>> input);
  bool inMacro;
  std::deque<std::array<float, 2>> macroPositions;
  int macroFrames;
  bool macroBtnPressed;
  int macroType;
  int justFinishedMacro;
  std::array<float, 2> macroInitPos;
  int stylusModDelay;
  bool hitMaxSpeed;
  bool alreadyFlicked;
  bool joystickNegativeEdge; //Auto flick on joystick release
  int rotation;
  int layout;
};

#endif // CURSOR_H
