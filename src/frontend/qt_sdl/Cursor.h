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
  void setRawCursorPos(float x, float y);
  void setDeviceInUse(int device);
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
  int stylusInput[6] = {0};
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
  int rotation;
  int layout;
  int deviceInUse; //0 is Gamepad, 1 is Mouse/Tablet
};

#endif // CURSOR_H