#include "Cursor.h"
#include <cmath>
#include <algorithm>
#include "EmuInstance.h"
#include "Platform.h"

void Cursor::update(){
  if (emuInstance != nullptr){
    //Get inputs directional input
    if (inMacro){
      runMacro();
    } else {
      // //Swipe Gestures
      // if (emuInstance->modButtons[0]){
      //   if (!macroBtnPressed){
      //     swipe(1,0,4);
      //     return;
      //   }
      // } else if (emuInstance->modButtons[1]){
      //   if (!macroBtnPressed){
      //     swipe(0,1,4);
      //     return;      
      //   }
      // } else if (emuInstance->modButtons[10]){
      //   if (!macroBtnPressed){
      //     swipe(0,-1,4);
      //     return;
      //   }
      // } else if (emuInstance->modButtons[11]){
      //   if (!macroBtnPressed){
      //     swipe(-1,0,4);
      //     return;
      //   }
      // } else {
      //   if (macroBtnPressed){
      //     macroBtnPressed = false;
      //   }
      // }



      //Compare Left vs Right Values
      if (emuInstance->stylusInput[2] > emuInstance->stylusInput[3]){
        normStylusDirection[0] = -(emuInstance->stylusInput[2]/32767.0f);
      } else if (emuInstance->stylusInput[3] > emuInstance->stylusInput[2]) {
        normStylusDirection[0] = (emuInstance->stylusInput[3]/32767.0f);
      } else {
        normStylusDirection[0] = 0;
      }

      //Compare Up vs Down Values
      if (emuInstance->stylusInput[0] > emuInstance->stylusInput[1]){
        normStylusDirection[1] = -(emuInstance->stylusInput[0]/32767.0f);
      } else if (emuInstance->stylusInput[1] > emuInstance->stylusInput[0]){
        normStylusDirection[1] = (emuInstance->stylusInput[1]/32767.0f);
      } else {
        normStylusDirection[1] = 0;
      }
      normStylusDirection[0] = std::min(normStylusDirection[0], 1.0f);
      normStylusDirection[1] = std::min(normStylusDirection[1], 1.0f);

      // Logs
      // melonDS::Platform::Log(melonDS::Platform::LogLevel::Debug, "Stylus: Up: %d, Down: %d, Left: %d, Right: %d\n", emuInstance->stylusInput[0], emuInstance->stylusInput[1], emuInstance->stylusInput[2], emuInstance->stylusInput[3]);
      // melonDS::Platform::Log(melonDS::Platform::LogLevel::Debug, "Normalized Stylus: X: %f, Y: %f\n", normStylusDirection[0], normStylusDirection[1]);

      int maxSpeed = 50;  //Attach to Gui
      float multiplier = 0.5f * pow(4.0f, maxSpeed / 100.0f); //Attach to Gui. 0 is 0.5x, 100 is 2.0x speed
      float heightSpeed = (192.0f / 33.0f) * multiplier;

      float deadzone = 5.0f / 100.0f; //Attach to Gui
      bool stylusModPressed = emuInstance->stylusInput[4]; 
      float responsecurve = 200.0f / 100.0f; //Attach to Gui
      float speedupratio = 300.0f / 100.0f; //Attach to Gui
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
      //The code below sets the cursor position to the position of the joystick (absolute)
      //_joystickCursorPosition = vec2((NDS_SCREEN_WIDTH/2.0f)+(std::min<float>(1.0,(normStylusDirection[0]/0.7071))*(NDS_SCREEN_WIDTH/2.0f)), (NDS_SCREEN_HEIGHT/2.0f)+(std::min<float>(1.0,(normStylusDirection[1]/0.7071))*(NDS_SCREEN_HEIGHT/2.0f)));
      // qDebug("Current Rotation: %d", rotation);

      float tempX = joystickScaled[0];
      float tempY = joystickScaled[1];

      switch (rotation)
      {
          case 1: // 90° clockwise
              joystickScaled[0] =  tempY;
              joystickScaled[1] = -tempX;
              break;
          case 2: // 180°
              joystickScaled[0] = -tempX;
              joystickScaled[1] = -tempY;
              break;
          case 3: // 270° clockwise
              joystickScaled[0] = -tempY;
              joystickScaled[1] =  tempX;
              break;
          default: // 0° / no rotation
              break;
      }

      rawCursorPos[0] += joystickScaled[0]*heightSpeed;
      rawCursorPos[1] += joystickScaled[1]*heightSpeed;

      //Clamp to region and ready position information for touchscreen
      clamp();
      updateCursorPos();
      
      // If touchButton pressed, and wasnt touching, send signal
      // if (wasTouching) and touchButton not pressed, send release
      // qDebug("Stylus Mod Enabled: %d, Stylus Mod Pressed: %d, Stylus Mod Delay: %d, Hit Max Speed: %d", stylusModEnabled, stylusModPressed, stylusModDelay, hitMaxSpeed);
      if (stylusModEnabled){
        if (curvedLength < 0.9 && wasTouching  && hitMaxSpeed){ //Flicked but button still held
          release();
          hitMaxSpeed = false;
          stylusModDelay = 10;
          wasTouching = false;
          // alreadyFlicked = true; // Uncomment to have Stylus Mod not click the screen more than once
        } else {
          if (!alreadyFlicked){
            touchScreen();
            wasTouching = true;
            if (curvedLength == 1){
              hitMaxSpeed = true;
            }
          }
        }
      } else if (!stylusModEnabled && wasTouching && hitMaxSpeed){ //Let go of button before joystick
        release();
        hitMaxSpeed = false;
        stylusModDelay = 10;
        wasTouching = false;
      } else if (emuInstance->stylusInput[5]){
        touchScreen();
        wasTouching = true;
      } else if (wasTouching && !emuInstance->stylusInput[5]){ //if (wasTouching) and touchButton not pressed, send release
        release();
        wasTouching = false;
      }
      if (stylusModDelay > 0){
        stylusModDelay--;
      }
      if (!stylusModEnabled){
        alreadyFlicked = false;
      }
      
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

void Cursor::swipe(float x, float y, int frames){
  macroBtnPressed = true;
  inMacro = true;
  float swipeDistance = 192.0f/2; //Default swipe is 1/2 the height
  macroFrames = frames+1;
  float distancePerFrame = swipeDistance/frames;
  int total = std::abs(x) + std::abs(y);
  if (total == 2){
    x = x * 0.707;
    y = y * 0.707;
  }
for (int i = 0; i <= frames; i++){
    float offset = (i - frames / 2.0f) * distancePerFrame;
    macroPositions.push_back({rawCursorPos[0]+(x)*offset, rawCursorPos[1]+(y)*offset});
}
  macroPositions.push_back({rawCursorPos[0], rawCursorPos[1]});
  runMacro();
}

void Cursor::runMacro(){
  rawCursorPos[0] = macroPositions.front()[0];
  rawCursorPos[1] = macroPositions.front()[1];
  macroPositions.pop_front();
  clamp();
  updateCursorPos();
  if (macroFrames > 0){
    touchScreen();
    macroFrames--;
  } else {
    release();
    //Clear queue
    macroPositions.clear();
    inMacro = false;
  }
}

void Cursor::setRotation(int rot){
  rotation = rot;
}