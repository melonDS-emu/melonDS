#include "Cursor.h"
#include <cmath>
#include <algorithm>
#include "EmuInstance.h"

void Cursor::update(){
  if (emuInstance != nullptr){
    //Get inputs directional input
    rawCursorPos[0] += 0.05;
    rawCursorPos[1] += 0.05;
    //Clamp to region and ready position information for touchscreen
    clamp();
    updateCursorPos();

    //If touchButton pressed, send signal

    //if (wasTouching) and touchButton not pressed, send release

  }
}

void Cursor::clamp(){
  rawCursorPos[0] = std::clamp(rawCursorPos[0], 0.0f, 255.0f);
  rawCursorPos[1] = std::clamp(rawCursorPos[1], 0.0f, 191.0f);
}

void Cursor::touchScreen(){
  emuInstance->touchScreen(cursorPos[0], cursorPos[1]);
}

void Cursor::release(){
  emuInstance->releaseScreen();
}
void Cursor::setEmuInstance(EmuInstance* emuInstance){
  this->emuInstance = emuInstance;
}

void Cursor::updateCursorPos(){
  cursorPos[0] = std::floor(rawCursorPos[0]);
  cursorPos[1] = std::floor(rawCursorPos[1]);
}