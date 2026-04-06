#include "Cursor.h"
#include <cmath>
#include <algorithm>
#include "EmuInstance.h"
#include "Platform.h"

void Cursor::update(){
  if (emuInstance != nullptr){
    //Get inputs directional input

    //Compare Left vs Right
    if (emuInstance->stylusInput[2] > emuInstance->stylusInput[3]){
      normStylusDirection[0] = -(emuInstance->stylusInput[2]/32767.0f);
    } else if (emuInstance->stylusInput[3] > emuInstance->stylusInput[2]) {
      normStylusDirection[0] = (emuInstance->stylusInput[3]/32767.0f);
    } else {
      normStylusDirection[0] = 0;
    }
    //Compare Up vs Down
    if (emuInstance->stylusInput[0] > emuInstance->stylusInput[1]){
      normStylusDirection[1] = -(emuInstance->stylusInput[0]/32767.0f);
    } else if (emuInstance->stylusInput[1] > emuInstance->stylusInput[0]){
      normStylusDirection[1] = (emuInstance->stylusInput[1]/32767.0f);
    } else {
      normStylusDirection[1] = 0;
    }
    normStylusDirection[0] = std::min(normStylusDirection[0], 1.0f);
    normStylusDirection[1] = std::min(normStylusDirection[1], 1.0f);

    float speed = 192.0f/33;
    // melonDS::Platform::Log(melonDS::Platform::LogLevel::Debug, "Stylus: Up: %d, Down: %d, Left: %d, Right: %d\n", emuInstance->stylusInput[0], emuInstance->stylusInput[1], emuInstance->stylusInput[2], emuInstance->stylusInput[3]);
    melonDS::Platform::Log(melonDS::Platform::LogLevel::Debug, "Normalized Stylus: X: %f, Y: %f\n", normStylusDirection[0], normStylusDirection[1]);

    rawCursorPos[0] += normStylusDirection[0]*speed;
    rawCursorPos[1] += normStylusDirection[1]*speed;
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

    

    //if (wasTouching) and touchButton not pressed, send release
    //If touchButton pressed, and wasnt touching, send signal
    if (!wasTouching && emuInstance->stylusInput[5]){
      touchScreen();
      wasTouching = true;
    } else if (wasTouching && !emuInstance->stylusInput[5]){
      release();
      wasTouching = false;
    }
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