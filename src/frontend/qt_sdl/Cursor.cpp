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


    /* Refactored Coded from Retroarch Fork
    int maxSpeed = config.JoystickCursorMaxSpeed();    
    //float multiplier = 0.5f * pow(4.0f, maxSpeed / 100.0f);
    //float widthSpeed = (NDS_SCREEN_WIDTH / 33.0) * multiplier; //Currently unused
    float heightSpeed = (NDS_SCREEN_HEIGHT / 33.0) * multiplier;
    float deadzone = config.JoystickCursorDeadzone() / 100.0f;
    bool speedup_enabled = config.JoystickSpeedupEnabled();
    float responsecurve = config.JoystickCursorResponse() / 100.0f;
    float speedupratio = config.JoystickCursorSpeedup() / 100.0f;
    vec2 joystickNorm = ((vec2)_joystickRawDirection/32767.0f);
    vec2 joystickScaled = vec2(0.0f);
    float radialLength = std::sqrt((joystickNorm.x * joystickNorm.x) + (joystickNorm.y * joystickNorm.y));
    if (radialLength > deadzone) {
        // Get X and Y as a relation to the radial length
        vec2 dir = joystickNorm/radialLength;
        // Apply deadzone and response curve
        float scaledLength = (radialLength - deadzone) / (1.0f - deadzone);
        float curvedLength = std::pow(std::min<float>(1.0f, scaledLength), responsecurve);
        // Final output
        float finalLength = speedup_enabled ? curvedLength * speedupratio : curvedLength;
        joystickScaled = dir * finalLength;  
    } else {
        joystickScaled = vec2(0.0f);
    }
    //The code below sets the cursor position to the position of the joystick (absolute)
    //_joystickCursorPosition = vec2((NDS_SCREEN_WIDTH/2.0f)+(std::min<float>(1.0,(joystickNorm.x/0.7071))*(NDS_SCREEN_WIDTH/2.0f)), (NDS_SCREEN_HEIGHT/2.0f)+(std::min<float>(1.0,(joystickNorm.y/0.7071))*(NDS_SCREEN_HEIGHT/2.0f)));

    _joystickCursorPosition +=  joystickScaled*heightSpeed;
    _joystickCursorPosition = clamp(_joystickCursorPosition, vec2(1.0), NDS_SCREEN_SIZE<float> - 1.0f); 
    */

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