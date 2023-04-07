#include "LuaFrontEnd.h"
#include "types.h"
#include "OSD.h"
#include <vector>
#include <map>
#include <QtGui>
#include <SDL_joystick.h>//should be in Input.h
#include "Input.h"
#include <QDialog>
using namespace LuaFront;

int LuaFront::RightPadding = 0;
int LuaFront::BottomPadding = 0;
QHash<QString , QImage> ImageHash;
std::vector<LuaScript::LuaFunction*> LuaFront::FrontEndFunctions;
#define AddFrontEndLuaFunct(fpointer,name)LuaScript::LuaFunction name(fpointer,#name,&FrontEndFunctions)

OverlayCanvas* SelectedCanvas;
QPoint SelectionDelta;
void LuaFront::onMousePress(QMouseEvent* event){
    if (event->button() != Qt::LeftButton) return;
    if (!(event->modifiers() & Qt::ControlModifier)) return;
    for (auto lo = LuaFront::LuaOverlays.begin(); lo != LuaFront::LuaOverlays.end();){
        LuaFront::OverlayCanvas& overlay = *lo;
        if (overlay.isActive && overlay.rectangle.contains(event->pos())){
            SelectedCanvas = &overlay;
            SelectionDelta = overlay.rectangle.topLeft() - event->pos();
        }
        lo++;
    }
}
void LuaFront::onMouseRelease(QMouseEvent* event){
    if (event->button() != Qt::LeftButton) return;
    SelectedCanvas= nullptr;
}
void LuaFront::onMouseMove(QMouseEvent* event){
    if (!(event->buttons() & Qt::LeftButton)) return;
    if (SelectedCanvas==nullptr) return;
    SelectedCanvas->rectangle.moveTo(event->pos()+SelectionDelta);
}



OverlayCanvas::OverlayCanvas(int x,int y,int width,int height,bool isActive){
    this->isActive=isActive;
    image = new QImage(width,height,QImage::Format_ARGB32_Premultiplied);
    image->fill(0xFFFFFF00);
    rectangle = QRect(x,y,width,height);    
}


std::vector<OverlayCanvas> LuaFront::LuaOverlays;
OverlayCanvas* CurrentCanvas;

//OverlayCanvas LuaFront::MainLuaOverlay(0,0,200,200);

int Lua_popup(lua_State* L){
    u32 color = lua_tonumber(L,-2);
    const char* msg = lua_tostring(L,-1);
    OSD::AddMessage(color, msg);
    return 0;
}
AddFrontEndLuaFunct(Lua_popup,popup);

int Lua_MakeCanvas(lua_State* L){
    int offset = 0;
    bool a = true;
    if (lua_isboolean(L,-1)){
        offset=-1;
        bool a = lua_toboolean(L,-1);
    }
    int x=lua_tonumber(L,-4+offset);
    int y=lua_tonumber(L,-3+offset);
    int w=lua_tonumber(L,-2+offset);
    int h=lua_tonumber(L,-1+offset);
    OverlayCanvas canvas(x,y,w,h,a);
    LuaOverlays.push_back(canvas);
    lua_pushinteger(L,LuaOverlays.size()-1);
    return 1;
}
AddFrontEndLuaFunct(Lua_MakeCanvas,makecanvas);
int Lua_SetCanvas(lua_State* L){
    int index=lua_tonumber(L,-1);
    CurrentCanvas = &LuaOverlays.at(index);
    return 0;
}
AddFrontEndLuaFunct(Lua_SetCanvas,setcanvas);

int Lua_ClearOverlay(lua_State* L){
    CurrentCanvas->image->fill(0x00000000);
    return 0;
}
AddFrontEndLuaFunct(Lua_ClearOverlay,clearOverlay);

int Lua_rect(lua_State* L){
    u32 color = lua_tonumber(L,-1);
    int x = lua_tonumber(L,-5);
    int y = lua_tonumber(L,-4);
    int width = lua_tonumber(L,-3);
    int height = lua_tonumber(L,-2);
    QPainter painter(CurrentCanvas->image);
    painter.setPen(color);
    painter.drawRect(x,y,width,height);
    return 0;
}
AddFrontEndLuaFunct(Lua_rect,Rect);

int Lua_fillrect(lua_State* L){
    u32 color = lua_tonumber(L,-1);
    int x = lua_tonumber(L,-5);
    int y = lua_tonumber(L,-4);
    int width = lua_tonumber(L,-3);
    int height = lua_tonumber(L,-2);
    QPainter painter(CurrentCanvas->image);
    painter.setPen(color);
    painter.fillRect(x,y,width,height,color);
    return 0;
}
AddFrontEndLuaFunct(Lua_fillrect,fillRect);

int Lua_text(lua_State* L){
    int x = lua_tonumber(L,-6);
    int y = lua_tonumber(L,-5);
    const char* text = lua_tostring(L,-4);
    u32 color = lua_tonumber(L,-3);
    QString FontFamily= lua_tostring(L,-2);
    int size = lua_tonumber(L,-1);
    QPainter painter(CurrentCanvas->image);
    QFont font(FontFamily,size,0,false);
    font.setStyleStrategy(QFont::NoAntialias);
    font.setLetterSpacing(QFont::AbsoluteSpacing,-1);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(x,y,text);
    return 0;
}
AddFrontEndLuaFunct(Lua_text,text);

int Lua_line(lua_State* L){
    int x1 = lua_tonumber(L,-5);
    int y1 = lua_tonumber(L,-4);
    int x2 = lua_tonumber(L,-3);
    int y2 = lua_tonumber(L,-2);
    u32 color = lua_tonumber(L,-1);
    QPainter painter(CurrentCanvas->image);
    painter.setPen(color);
    painter.drawLine(x1,y1,x2,y2);
    return 0;
}
AddFrontEndLuaFunct(Lua_line,line);
int Lua_getKey(lua_State* L){
    int key = lua_tonumber(L,-1);
    bool isdown = false;
    if( GetKeyState(key) & 0x8000 ){
        isdown = true;
    }
    lua_pushboolean(L,isdown);
    return 1;
}
AddFrontEndLuaFunct(Lua_getKey,getkey);

int Lua_drawImage(lua_State* L){
    QString path = lua_tostring(L,-5);
    int x = lua_tointeger(L,-4);
    int y = lua_tointeger(L,-3);
    int sw= lua_tointeger(L,-2);
    int sh=lua_tointeger(L,-1);
    QPainter painter(CurrentCanvas->image);
    QImage image;
    if(ImageHash.contains(path)){
        image=ImageHash[path];
    }else{
        image=QImage(path);
        ImageHash[path]=image;
    }
    int sx=0;
    int sy=0;
    painter.drawImage(x, y,image,sx,sy, sw, sh);
    return 0;
}
AddFrontEndLuaFunct(Lua_drawImage,drawImage);

int Lua_clearImageHash(lua_State* L){
    ImageHash.clear();
    return 0;
}
AddFrontEndLuaFunct(Lua_clearImageHash,clearHash);




QWidget* LuaFront::panel=nullptr;

int Lua_canvasPos(lua_State* L){
    QPoint pos = CurrentCanvas->rectangle.topLeft();
    lua_createtable(L,0,2);
    lua_pushinteger(L,pos.x());
    lua_setfield(L,-2,"X");
    lua_pushinteger(L,pos.y());
    lua_setfield(L,-2,"Y");
    return 1;
}
AddFrontEndLuaFunct(Lua_canvasPos,canvasPos);


int Lua_getMouse(lua_State* L){
    Qt::MouseButtons btns = QGuiApplication::mouseButtons();
    QPoint pos = LuaFront::panel->mapFromGlobal(QCursor::pos(QGuiApplication::primaryScreen()));
    const char* keys[6]={"Left","Middle","Right","XButton1","XButton2","Wheel"};
    bool vals[6]={
        btns.testFlag(Qt::LeftButton),
        btns.testFlag(Qt::MiddleButton),
        btns.testFlag(Qt::RightButton),
        btns.testFlag(Qt::XButton1),
        btns.testFlag(Qt::XButton2),
        false
    };
    lua_createtable(L, 0, 8);
    lua_pushinteger(L, pos.x());
    lua_setfield(L, -2, "X");
    lua_pushinteger(L, pos.y());
    lua_setfield(L, -2, "Y");
    for(int i=0;i<6;i++){
        lua_pushboolean(L,vals[i]);
        lua_setfield(L,-2,keys[i]);
    }
    return 1;
}
AddFrontEndLuaFunct(Lua_getMouse,getmouse);



int Lua_getJoy(lua_State* L){
    u32 buttonMask=Input::InputMask;//copy of current button state.
    const char* keys[12]{//Buttons in order of mask.
        "A","B","Select","Start",
        "Right","Left","Up","Down",
        "R","L","X","Y"
    };
    lua_createtable(L, 0, 12);
    for(u32 i=0;i<12;i++){
        lua_pushboolean(L,0>=(buttonMask&(1<<i)));
        lua_setfield(L,-2,keys[i]);
    }
    return 1;
}
AddFrontEndLuaFunct(Lua_getJoy,getJoy);

int Lua_setPadding(lua_State* L){
    LuaFront::RightPadding = lua_tointeger(L,-2);
    LuaFront::BottomPadding = lua_tointeger(L,-1);
    luaThread->luaLayoutChange();
    return 0;
}
AddFrontEndLuaFunct(Lua_setPadding,setPadding);

