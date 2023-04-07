#ifndef LUAFRONTEND_H
#define LUAFRONTEND_H
#include "LuaScript.h"
#include <QPainter>
#include <QKeyEvent>
#include <QWidget>
namespace LuaFront{
typedef int(*lfunctpntr)(lua_State*);
extern QWidget* panel;
extern int RightPadding;
extern int BottomPadding;
extern std::vector<LuaScript::LuaFunction*> FrontEndFunctions;
struct OverlayCanvas{
    QImage* image;
    QRect rectangle;
    bool isActive = true;//Only Active overlays are drawn to the screen.
    OverlayCanvas(int,int,int,int,bool);
};
extern std::vector<OverlayCanvas> LuaOverlays;
extern void onMousePress(QMouseEvent* event);
extern void onMouseRelease(QMouseEvent* event);
extern void onMouseMove(QMouseEvent* event);

//extern OverlayCanvas MainLuaOverlay;
}
#endif