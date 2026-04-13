#include "Cursor.h"
#include <cmath>
#include <algorithm>
#include "EmuInstance.h"
#include "Platform.h"



void Cursor::update(){
  if (emuInstance != nullptr){
    if (deviceInUse == 0){
      if (inMacro){
        runMacro();
      } else {
        // Reset the cursor position if macro was just played
        if (justFinishedMacro > 0){
          justFinishedMacro = 0;
          rawCursorPos[0] = macroInitPos[0];
          rawCursorPos[1] = macroInitPos[1];
        }

        // Macros
        if (emuInstance->modButtons[0]){
          circle(0);
          return;
        } else if (emuInstance->modButtons[1]){
          rub();
          return;
        } else if (emuInstance->modButtons[10]){
          if (!macroBtnPressed){
            // Add macro
            return;
          }
        } else if (emuInstance->modButtons[11]){
          circle(1);
          return;
        } else if (emuInstance->hotkeyPressed(HK_GuitarGripStrum1)){
          if (!macroBtnPressed){
            ggstrum(0);
            return;
          }
        } else if (emuInstance->hotkeyPressed(HK_GuitarGripStrum2)){
          if (!macroBtnPressed){
            ggstrum(1);
            return;
          }
        } else if (emuInstance->hotkeyDown(HK_GuitarGripWhammy)){
          ggwhammy();
          return;
        } else if (emuInstance->hotkeyPressed(HK_GuitarGripStarPower)){
          if (!macroBtnPressed){
            ggstarpower();
            return;
          }
        } else {
          if (macroBtnPressed){
            macroBtnPressed = false;
          }
        }

        // Compare Left vs Right Values
        if (emuInstance->stylusInput[2] > emuInstance->stylusInput[3]){
          normStylusDirection[0] = -(emuInstance->stylusInput[2]/32767.0f);
        } else if (emuInstance->stylusInput[3] > emuInstance->stylusInput[2]) {
          normStylusDirection[0] = (emuInstance->stylusInput[3]/32767.0f);
        } else {
          normStylusDirection[0] = 0;
        }

        // Compare Up vs Down Values
        if (emuInstance->stylusInput[0] > emuInstance->stylusInput[1]){
          normStylusDirection[1] = -(emuInstance->stylusInput[0]/32767.0f);
        } else if (emuInstance->stylusInput[1] > emuInstance->stylusInput[0]){
          normStylusDirection[1] = (emuInstance->stylusInput[1]/32767.0f);
        } else {
          normStylusDirection[1] = 0;
        }

        normStylusDirection[0] = std::min(normStylusDirection[0], 1.0f);
        normStylusDirection[1] = std::min(normStylusDirection[1], 1.0f);

        int maxSpeed = 50;
        float multiplier = 0.5f * pow(4.0f, maxSpeed / 100.0f); // 0 is 0.5x speed, 100 is 2.0x speed.
        float heightSpeed = (192.0f / 33.0f) * multiplier;

        float deadzone = 5.0f / 100.0f;
        bool stylusModPressed = emuInstance->stylusInput[4]; 
        float responsecurve = 200.0f / 100.0f;
        float speedupratio = 400.0f / 100.0f;
        float joystickScaled[2] = {0.0f};
        bool stylusModEnabled = stylusModPressed && !stylusModDelay;
        float radialLength = std::sqrt((normStylusDirection[0] * normStylusDirection[0]) + (normStylusDirection[1] * normStylusDirection[1]));
        float finalLength;
        float curvedLength;
        if (radialLength > deadzone) {
            // Get X and Y as a relation to the radial length
            float rComponents[2];
            rComponents[0] = normStylusDirection[0]/radialLength;
            rComponents[1] = normStylusDirection[1]/radialLength;
            // Apply deadzone and response curve
            float scaledLength = (radialLength - deadzone) / (1.0f - deadzone);
            curvedLength = std::pow(std::min<float>(1.0f, scaledLength), responsecurve);
            // Final output
            finalLength = stylusModPressed ? curvedLength * speedupratio : curvedLength;
            joystickScaled[0] = rComponents[0] * finalLength;
            joystickScaled[1] = rComponents[1] * finalLength;
        } else {
            joystickScaled[2] = {0.0f};
        }
        // The code below sets the cursor position to the position of the joystick (absolute). Needs to be readjusted for standalone melonDS
        // _joystickCursorPosition = vec2((NDS_SCREEN_WIDTH/2.0f)+(std::min<float>(1.0,(normStylusDirection[0]/0.7071))*(NDS_SCREEN_WIDTH/2.0f)), (NDS_SCREEN_HEIGHT/2.0f)+(std::min<float>(1.0,(normStylusDirection[1]/0.7071))*(NDS_SCREEN_HEIGHT/2.0f)));

        float tempX = joystickScaled[0];
        float tempY = joystickScaled[1];

        switch (rotation)
        {
            case 1: // 90°
                joystickScaled[0] =  tempY;
                joystickScaled[1] = -tempX;
                break;
            case 2: // 180°
                joystickScaled[0] = -tempX;
                joystickScaled[1] = -tempY;
                break;
            case 3: // 270°
                joystickScaled[0] = -tempY;
                joystickScaled[1] =  tempX;
                break;
            default: // 0°
                break;
        }

        rawCursorPos[0] += joystickScaled[0]*heightSpeed;
        rawCursorPos[1] += joystickScaled[1]*heightSpeed;

        // Clamp to region and ready position information for touchscreen
        clamp();
        updateCursorPos();
        
        if (stylusModEnabled){
          // When joystickNegativeEdge is enabled, this allows flicking by releasing joystick after hitting max stick deflection
          if (curvedLength < 0.9 && wasTouching  && hitMaxSpeed && joystickNegativeEdge){ 
            release();
            hitMaxSpeed = false;
            stylusModDelay = 10;
            wasTouching = false;
            // alreadyFlicked = true; // Uncomment to have Stylus Mod not click the screen more than once when joystickNegativeEdge is enabled
          } else {
            if (!alreadyFlicked && joystickNegativeEdge){
              touchScreen();
              wasTouching = true;
              if (curvedLength == 1){
                hitMaxSpeed = true;
              }
            }
          }
        } else if (!stylusModEnabled && wasTouching && hitMaxSpeed && joystickNegativeEdge){ // When joystickNegativeEdge is enabled, this makes it so letting go of Stylus Mod also flicks after hitting max stick deflection
          release();
          hitMaxSpeed = false;
          stylusModDelay = 10;
          wasTouching = false;
        }

        // Handle stylus touch button presses
        if (emuInstance->stylusInput[5]){
          touchScreen();
          wasTouching = true;
        } else if (wasTouching && !emuInstance->stylusInput[5]){
          release();
          wasTouching = false;
        }

        // Other handling of variables. These are only in use when joystickNegativeEdge is enabled
        if (stylusModDelay > 0){
          stylusModDelay--;
        }
        if (!stylusModEnabled){
          alreadyFlicked = false;
        }
      }
    } else {
      //Update cursor based on mouse position
      clamp();
      updateCursorPos();
      setDeviceInUse(0);
    }
  }
}

void Cursor::setDeviceInUse(int device){
  deviceInUse = device;
}

void Cursor::setRawCursorPos(float x, float y){
  rawCursorPos[0] = x;
  rawCursorPos[1] = y;
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

void Cursor::circle(int direction){
  macroBtnPressed = true;
  inMacro = true;
  wasTouching = true;
  macroType = 1;
  float radius = 192.0f/4.0f;
  if (justFinishedMacro != 1){ // Set the original position if just starting
    macroInitPos = {rawCursorPos[0],  rawCursorPos[1]};
  }


  std::vector<std::array<float, 2>> offsetArray;
  if (direction == 0){
    offsetArray.push_back({(0.0f*radius),      (-1.0f*radius)});
    offsetArray.push_back({(0.7071f*radius),   (-0.7071f*radius)});
    offsetArray.push_back({(1.0f*radius),      (0.0f*radius)});
    offsetArray.push_back({(0.7071f*radius),   (0.7071f*radius)});
    offsetArray.push_back({(0.0f*radius),      (1.0f*radius)});
    offsetArray.push_back({(-0.7071f*radius),  (0.7071f*radius)});
    offsetArray.push_back({(-1.0f*radius),     (0.0f*radius)});
    offsetArray.push_back({(-0.7071f*radius),  (-0.7071f*radius)});
  } else {
    offsetArray.push_back({(0.0f*radius),      (-1.0f*radius)});
    offsetArray.push_back({(-0.7071f*radius),  (-0.7071f*radius)});
    offsetArray.push_back({(-1.0f*radius),     (0.0f*radius)});
    offsetArray.push_back({(-0.7071f*radius),  (0.7071f*radius)});
    offsetArray.push_back({(0.0f*radius),      (1.0f*radius)});
    offsetArray.push_back({(0.7071f*radius),   (0.7071f*radius)});
    offsetArray.push_back({(1.0f*radius),      (0.0f*radius)});
    offsetArray.push_back({(0.7071f*radius),   (-0.7071f*radius)});
  }
  offsetArray = rotateVector(offsetArray);

  macroPositions.push_back({rawCursorPos[0]+offsetArray[0][0], rawCursorPos[1]+offsetArray[0][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[1][0], rawCursorPos[1]+offsetArray[1][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[2][0], rawCursorPos[1]+offsetArray[2][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[3][0], rawCursorPos[1]+offsetArray[3][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[4][0], rawCursorPos[1]+offsetArray[4][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[5][0], rawCursorPos[1]+offsetArray[5][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[6][0], rawCursorPos[1]+offsetArray[6][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[7][0], rawCursorPos[1]+offsetArray[7][1]});
  macroFrames = macroPositions.size();
  runMacro();
}

void Cursor::rub(){
  macroBtnPressed = true;
  inMacro = true;
  wasTouching = true;
  macroType = 2;
  float radius = 192.0f/6.0f;
  if (justFinishedMacro != 2){ // Set the original position if just starting
    macroInitPos = {rawCursorPos[0],  rawCursorPos[1]};
  }
  std::vector<std::array<float, 2>> offsetArray;
  offsetArray.push_back({(0.0f*radius),   0});
  offsetArray.push_back({(0.5f*radius),   0});
  offsetArray.push_back({(1.0f*radius),   0});
  offsetArray.push_back({(0.5f*radius),   0});
  offsetArray.push_back({(0.0f*radius),   0});
  offsetArray.push_back({(-0.5f*radius),  0});
  offsetArray.push_back({(-1.0f*radius),  0});
  offsetArray.push_back({(-0.5f*radius),  0});
  offsetArray = rotateVector(offsetArray);
  

  macroPositions.push_back({rawCursorPos[0]+offsetArray[0][0], rawCursorPos[1]+offsetArray[0][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[1][0], rawCursorPos[1]+offsetArray[1][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[2][0], rawCursorPos[1]+offsetArray[2][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[3][0], rawCursorPos[1]+offsetArray[3][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[4][0], rawCursorPos[1]+offsetArray[4][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[5][0], rawCursorPos[1]+offsetArray[5][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[6][0], rawCursorPos[1]+offsetArray[6][1]});
  macroPositions.push_back({rawCursorPos[0]+offsetArray[7][0], rawCursorPos[1]+offsetArray[7][1]});
  macroFrames = macroPositions.size();
  runMacro();
}


void Cursor::runMacro(){
    rawCursorPos[0] = macroPositions.front()[0];
    rawCursorPos[1] = macroPositions.front()[1];
    macroPositions.pop_front();
    clamp();
    updateCursorPos();
    touchScreen();
    macroFrames--;
    if (macroFrames == 0){
      macroPositions.clear();
      inMacro = false;
      justFinishedMacro = macroType;
    }
}

void Cursor::setRotation(int rot){
  rotation = rot;
  if (layout == 2){
    rotation = 0;
  } else if (layout == 5){
    rotation = 3;
  } else if (layout == 6){
    rotation = 1;
  }
}
void Cursor::setLayout(int lay){
  layout = lay;
}

std::vector<std::array<float, 2>> Cursor::rotateVector(std::vector<std::array<float, 2>> input){
  for (auto& currArray : input){
      float tempX = currArray[0];
      float tempY = currArray[1];
      switch (rotation)
      {
          case 1: // 90°
              currArray[0] =  tempY;
              currArray[1] = -tempX;
              break;
          case 2: // 180°
              currArray[0] = -tempX;
              currArray[1] = -tempY;
              break;
          case 3: // 270°
              currArray[0] = -tempY;
              currArray[1] =  tempX;
              break;
          default: // 0°
              break;
    }
  }
  return input;
}

//Game Specific Macros

void Cursor::ggstrum(int direction){
  macroBtnPressed = true;
  inMacro = true;
  wasTouching = true;
  macroType = 4; // Unused by any looping macros
  float distance = 50;
  int startingY;
  int sign;
  if (direction == 0){
    sign = 1;
    startingY = 76;
  } else {
    sign = -1;
    startingY = 126;
  }
  macroInitPos = {82,  101};
  macroPositions.push_back({82, startingY+(sign*0.00f*distance)});
  macroPositions.push_back({82, startingY+(sign*0.50f*distance)});
  macroPositions.push_back({82, startingY+(sign*1.00f*distance)});
  macroFrames = macroPositions.size();
  runMacro();
}

void Cursor::ggwhammy(){
  macroBtnPressed = true;
  inMacro = true;
  wasTouching = true;
  macroType = 3;
  float distance = 50;
  if (justFinishedMacro != 3){ // Set the original position if just starting
    macroInitPos = {82,  101};
  }

  macroPositions.push_back({82, 76+(0.00f*distance)});
  macroPositions.push_back({82, 76+(0.25f*distance)});
  macroPositions.push_back({82, 76+(0.50f*distance)});
  macroPositions.push_back({82, 76+(0.75f*distance)});
  macroPositions.push_back({82, 76+(1.00f*distance)});
  macroPositions.push_back({82, 76+(0.75f*distance)});
  macroPositions.push_back({82, 76+(0.50f*distance)});
  macroPositions.push_back({82, 76+(0.25f*distance)});
  macroFrames = macroPositions.size();
  runMacro();
}

void Cursor::ggstarpower(){
  macroBtnPressed = true;
  inMacro = true;
  wasTouching = true;
  macroType = 4; // Unused by any looping macros
  macroInitPos = {82,  101};
  macroPositions.push_back({134, 16});
  macroFrames = macroPositions.size();
  runMacro();
}