#include "LuaMain.h"
#include <filesystem>
#include <QDir>
#include <QFileDialog>
#include <QPushButton>
#include <QGuiApplication>
#include <QScrollBar>
#include <QPainter>
#include  "main.h"
#include "types.h"
#include "NDS.h"
#include <SDL_joystick.h>
#include "Input.h"

extern EmuThread* emuThread;

void EmuThread::onLuaPrint(const QString& string)
{
    emit signalLuaPrint(string);
}

void EmuThread::onLuaClearConsole()
{
    emit signalLuaClearConsole();
}

void EmuThread::onLuaLoadState(const QString& string)
{
    emit signalLuaLoadState(string);
}

void EmuThread::onLuaLayoutChange()
{
    emit screenLayoutChange();
}

void EmuThread::onLuaSaveState(const QString& string)
{
    emit signalLuaSaveState(string);
}

namespace LuaScript
{
LuaConsoleDialog* LuaDialog=nullptr;

LuaConsoleDialog::LuaConsoleDialog(QWidget* parent) : QDialog(parent)
{
    console = new LuaConsole(this);
    console->setGeometry(0,20,302,80);
    bar = console->verticalScrollBar();
    buttonPausePlay = new QPushButton("Pause/UnPause",this);
    buttonPausePlay->setGeometry(0,0,100,20);
    buttonStartStop = new QPushButton("Stop",this);
    buttonStartStop->setGeometry(101,0,100,20);
    buttonOpenScript = new QPushButton("OpenLuaFile",this);
    buttonOpenScript->setGeometry(202,0,100,20);
    connect(buttonOpenScript,&QPushButton::clicked,this,&LuaConsoleDialog::onOpenScript);
    connect(buttonStartStop,&QPushButton::clicked,this,&LuaConsoleDialog::onStop);
    connect(buttonPausePlay,&QPushButton::clicked,this,&LuaConsoleDialog::onPausePlay);
    connect(emuThread,&EmuThread::signalLuaPrint,console,&LuaConsole::onGetText);
    connect(emuThread,&EmuThread::signalLuaClearConsole,console,&LuaConsole::onClear);
    this->setWindowTitle("Lua Script");
}

void LuaConsoleDialog::closeEvent(QCloseEvent *event)
{
    onStop();
    LuaDialog = nullptr;
    LuaOverlays.clear();
    event->accept();
}

void LuaConsoleDialog::onOpenScript()
{
    QFileInfo file = QFileInfo(QFileDialog::getOpenFileName(this, "Load Lua Script",QDir::currentPath()));
    if (!file.exists())
        return;
    currentScript = file;
    FlagNewLua = true;
}

LuaConsole::LuaConsole(QWidget* parent)
{
    this->setParent(parent);
}

void LuaConsole::onGetText(const QString& string)
{
    this->appendPlainText(string);
    QScrollBar* bar = verticalScrollBar();
    bar->setValue(bar->maximum());
}

void LuaConsole::onClear()
{
    this->clear();
}

void luaClearConsole()
{
    LuaDialog->console->clear();
}

std::vector<LuaFunction*> LuaFunctionList; // List of all lua functions.
QWidget* panel = nullptr;
lua_State* MainLuaState = nullptr;
bool FlagPause = false;
bool FlagStop = false;
bool FlagNewLua = false;

LuaFunction::LuaFunction(luaFunctionPointer cf,const char* n,std::vector<LuaFunction*>* container)
{
    this->cfunction = cf;
    this->name = n;
    container->push_back(this);
}

#define MELON_LUA_HOOK_INSTRUCTION_COUNT 50 //number of vm instructions between hook calls
void luaHookFunction(lua_State* L, lua_Debug *arg)
{
    if(FlagStop and (arg->event == LUA_HOOKCOUNT))
        luaL_error(L, "Force Stopped");
}

void createLuaState()
{
    if(!FlagNewLua)
        return;
    LuaOverlays.clear();
    FlagNewLua = false;
    MainLuaState = nullptr;
    std::string fileName = LuaDialog->currentScript.fileName().toStdString();
    std::string filedir = LuaDialog->currentScript.dir().path().toStdString();
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    for(LuaFunction* function : LuaFunctionList)
        lua_register(L,function->name,function->cfunction);
    std::filesystem::current_path(filedir.c_str());
    lua_sethook(L,&luaHookFunction,LUA_MASKCOUNT,MELON_LUA_HOOK_INSTRUCTION_COUNT);
    FlagStop = false;
    if (luaL_dofile(L,&fileName[0])==LUA_OK)
    {
        MainLuaState = L;
        FlagPause = false;
    }
    else //Error loading script
    {
        emuThread->onLuaPrint(lua_tostring(L,-1));
        MainLuaState = nullptr;
    }
}

void LuaConsoleDialog::onStop()
{
    FlagStop = true;
}

void LuaConsoleDialog::onPausePlay()
{
    FlagPause = !FlagPause;
}

//Gets Called once a frame
void luaUpdate()
{
    if(!MainLuaState || FlagPause)
        return;
    if (lua_getglobal(MainLuaState,"_Update")!=LUA_TFUNCTION)
        return;
    if(lua_pcall(MainLuaState,0,0,0)!=0)
    {
        //Handel Errors
        emuThread->onLuaPrint(lua_tostring(MainLuaState,-1));
        MainLuaState = nullptr;
    }
}

/*
*   Front End Stuff
*/

OverlayCanvas::OverlayCanvas(int x,int y,int width,int height,bool isActive)
{
    this->isActive=isActive;
    buffer1 = new QImage(width,height,QImage::Format_ARGB32_Premultiplied);
    buffer2 = new QImage(width,height,QImage::Format_ARGB32_Premultiplied);
    buffer1->fill(0xffffff00); //initializes buffer with yellow pixels
    buffer2->fill(0xffffff00);
    imageBuffer = buffer1;
    displayBuffer = buffer2;
    rectangle = QRect(x,y,width,height);
    flipped = false;
    GLTextureLoaded = false;
}
void OverlayCanvas::flip()
{
    if (imageBuffer == buffer1)
    {
        imageBuffer = buffer2;
        displayBuffer = buffer1;
    }
    else
    {
        imageBuffer = buffer1;
        displayBuffer = buffer2;
    }
    flipped = true;
}

std::vector<OverlayCanvas> LuaOverlays;
OverlayCanvas* CurrentCanvas;
QHash<QString , QImage> ImageHash;

void luaResetOSD()
{
    for (auto lo = LuaOverlays.begin(); lo != LuaOverlays.end();)
    {
        OverlayCanvas& overlay = *lo;
        overlay.GLTextureLoaded = false;
        lo++;
    }
}

int RightPadding = 0;
int BottomPadding = 0;
int LeftPadding = 0;
int TopPadding = 0;

#define AddLuaFunction(functPointer,name)LuaFunction name(functPointer,#name,&LuaFunctionList)

/*
*   Start Defining Lua Functions:
*/

namespace LuaFunctionDefinition
{
int lua_MelonPrint(lua_State* L)
{
    QString string = luaL_checkstring(L,1);
    emuThread->onLuaPrint(string);
    return 0;
}
AddLuaFunction(lua_MelonPrint,MelonPrint);

int lua_MelonClear(lua_State* L)
{
    emuThread->onLuaClearConsole();
    return 0;
}
AddLuaFunction(lua_MelonClear,MelonClear);

enum ramInfo_ByteType
{
    ramInfo_OneByte = 1, 
    ramInfo_TwoBytes = 2, 
    ramInfo_FourBytes = 4
};

u32 GetMainRAMValueU(const u32& addr, const ramInfo_ByteType& byteType)
{
    switch (byteType)
    {
    case ramInfo_OneByte:
        return *(u8*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    case ramInfo_TwoBytes:
        return *(u16*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    case ramInfo_FourBytes:
        return *(u32*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    default:
        return 0;
    }
}

s32 GetMainRAMValueS(const u32& addr, const ramInfo_ByteType& byteType)
{
    switch (byteType)
    {
    case ramInfo_OneByte:
        return *(s8*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    case ramInfo_TwoBytes:
        return *(s16*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    case ramInfo_FourBytes:
        return *(s32*)(NDS::MainRAM + (addr&NDS::MainRAMMask));
    default:
        return 0;
    }
}

int Lua_ReadDatau(lua_State* L,ramInfo_ByteType byteType) 
{   
    u32 address = luaL_checkinteger(L,1);
    u32 value = GetMainRAMValueU(address,byteType);
    lua_pushinteger(L, value);
    return 1;
}

int Lua_ReadDatas(lua_State* L,ramInfo_ByteType byteType)
{
    u32 address = luaL_checkinteger(L,1);
    s32 value = GetMainRAMValueS(address,byteType);
    lua_pushinteger(L, value);
    return 1;
}

int Lua_Readu8(lua_State* L)
{
    return Lua_ReadDatau(L,ramInfo_OneByte);
}
AddLuaFunction(Lua_Readu8,Readu8);

int Lua_Readu16(lua_State* L)
{
    return Lua_ReadDatau(L,ramInfo_TwoBytes);
}
AddLuaFunction(Lua_Readu16,Readu16);

int Lua_Readu32(lua_State* L)
{
    return Lua_ReadDatau(L,ramInfo_FourBytes);
}
AddLuaFunction(Lua_Readu32,Readu32);

int Lua_Reads8(lua_State* L)
{
    return Lua_ReadDatas(L,ramInfo_OneByte);
}
AddLuaFunction(Lua_Reads8,Reads8);

int Lua_Reads16(lua_State* L)
{
    return Lua_ReadDatas(L,ramInfo_TwoBytes);
}
AddLuaFunction(Lua_Reads16,Reads16);

int Lua_Reads32(lua_State* L)
{
    return Lua_ReadDatas(L,ramInfo_FourBytes);
}
AddLuaFunction(Lua_Reads32,Reads32);

int Lua_NDSTapDown(lua_State* L)
{
    int x = luaL_checkinteger(L,1);
    int y = luaL_checkinteger(L,2);
    NDS::TouchScreen(x,y);
    return 0;
}
AddLuaFunction(Lua_NDSTapDown,NDSTapDown);

int Lua_NDSTapUp(lua_State* L)
{
    NDS::ReleaseScreen();
    return 0;
}
AddLuaFunction(Lua_NDSTapUp,NDSTapUp);

int Lua_StateSave(lua_State* L)
{
    QString filename = luaL_checkstring(L,1);
    emuThread->onLuaSaveState(filename);
    return 0;
}
AddLuaFunction(Lua_StateSave,StateSave);

int Lua_StateLoad(lua_State* L)
{
    QString filename = luaL_checkstring(L,1);
    emuThread->onLuaLoadState(filename);
    return 0;
}
AddLuaFunction(Lua_StateLoad,StateLoad);

int Lua_getMouse(lua_State* L)
{
    Qt::MouseButtons btns = QGuiApplication::mouseButtons();
    QPoint pos = panel->mapFromGlobal(QCursor::pos(QGuiApplication::primaryScreen()));
    const char* keys[6] = {"Left","Middle","Right","XButton1","XButton2","Wheel"};
    bool vals[6] =
    {
        btns.testFlag(Qt::LeftButton),
        btns.testFlag(Qt::MiddleButton),
        btns.testFlag(Qt::RightButton),
        btns.testFlag(Qt::XButton1),
        btns.testFlag(Qt::XButton2),
        false //TODO: add mouse wheel support
    };
    lua_createtable(L, 0, 8);
    lua_pushinteger(L, pos.x());
    lua_setfield(L, -2, "X");
    lua_pushinteger(L, pos.y());
    lua_setfield(L, -2, "Y");
    for(int i=0;i<6;i++)
    {
        lua_pushboolean(L,vals[i]);
        lua_setfield(L,-2,keys[i]);
    }
    return 1;//returns table describing the current pos and state of the mouse
}
AddLuaFunction(Lua_getMouse,GetMouse);

/*
 *  Front end lua functions
*/
//TODO: Lua Colors 

int Lua_MakeCanvas(lua_State* L) //MakeCanvas(int x,int y,int width,int height,[bool active = true])
{
    int offset = 0;
    bool a = true;
    if (lua_isboolean(L,-1))
    {
        offset = -1;
        bool a = lua_toboolean(L,-1);
    }
    int x = lua_tonumber(L,-4+offset);
    int y = lua_tonumber(L,-3+offset);
    int w = lua_tonumber(L,-2+offset);
    int h = lua_tonumber(L,-1+offset);
    OverlayCanvas canvas(x,y,w,h,a);
    lua_pushinteger(L,LuaOverlays.size());
    LuaOverlays.push_back(canvas);
    return 1; //returns index of the new overlay
}
AddLuaFunction(Lua_MakeCanvas,MakeCanvas);

int Lua_SetCanvas(lua_State* L)
{
    int index = lua_tonumber(L,-1);
    CurrentCanvas = &LuaOverlays.at(index);
    return 0;
}
AddLuaFunction(Lua_SetCanvas,SetCanvas);
int Lua_ClearOverlay(lua_State* L)
{
    CurrentCanvas->imageBuffer->fill(0x00000000);
    return 0;
}
AddLuaFunction(Lua_ClearOverlay,ClearOverlay);

int Lua_Flip(lua_State* L)
{
    CurrentCanvas->flip();
    return 0;
}
AddLuaFunction(Lua_Flip,Flip);

//text(int x, int y, string message, [u32 color = 'black'], [int fontsize = 9], [string fontfamily = Franklin Gothic Medium])
int Lua_text(lua_State* L) 
{
    int x = lua_tonumber(L,-6);
    int y = lua_tonumber(L,-5);
    const char* message = lua_tostring(L,-4);
    u32 color = lua_tonumber(L,-3);
    QString FontFamily = lua_tostring(L,-2);
    int size = lua_tonumber(L,-1);
    QPainter painter(CurrentCanvas->imageBuffer);
    QFont font(FontFamily,size,0,false);
    font.setStyleStrategy(QFont::NoAntialias);
    font.setLetterSpacing(QFont::AbsoluteSpacing,-1);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(x,y,message);
    return 0;
}
AddLuaFunction(Lua_text,Text);

int Lua_line(lua_State* L)
{
    int x1 = luaL_checknumber(L,1);
    int y1 = luaL_checknumber(L,2);
    int x2 = luaL_checknumber(L,3);
    int y2 = luaL_checknumber(L,4);
    u32 color = luaL_checknumber(L,5);
    QPainter painter(CurrentCanvas->imageBuffer);
    painter.setPen(color);
    painter.drawLine(x1,y1,x2,y2);
    return 0;
}
AddLuaFunction(Lua_line,Line);

int Lua_rect(lua_State* L)
{
    u32 color = luaL_checknumber(L,5);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);
    QPainter painter(CurrentCanvas->imageBuffer);
    painter.setPen(color);
    painter.drawRect(x,y,width,height);
    return 0;
}
AddLuaFunction(Lua_rect,Rect);

int Lua_fillrect(lua_State* L)
{
    u32 color = luaL_checknumber(L,5);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);
    QPainter painter(CurrentCanvas->imageBuffer);
    painter.setPen(color);
    painter.fillRect(x,y,width,height,color);
    return 0;
}
AddLuaFunction(Lua_fillrect,FillRect);

int Lua_keystrokes(lua_State* L)
{
    lua_createtable(L,0,Input::Keystrokes.size());
    for (int i = 0; i<Input::Keystrokes.size(); i++)
    {
        lua_pushinteger(L,Input::Keystrokes.at(i));
        lua_seti(L,-2,i);
    }
    Input::Keystrokes.clear();
    return 1;
}
AddLuaFunction(Lua_keystrokes,Keys);

int Lua_drawImage(lua_State* L)
{
    QString path = luaL_checkstring(L,1);
    int x = luaL_checkinteger(L,2);
    int y = luaL_checkinteger(L,3);
    int sourceWidth = luaL_checkinteger(L,4);
    int sourceHeight = luaL_checkinteger(L,5);
    QPainter painter(CurrentCanvas->imageBuffer);
    QImage image;
    if(ImageHash.contains(path))
    {
        image=ImageHash[path];
    }
    else
    {
        image=QImage(path);
        ImageHash[path] = image;
    }
    painter.drawImage(x,y,image,0,0,sourceWidth,sourceHeight);
    return 0;
}
AddLuaFunction(Lua_drawImage,DrawImage);

int Lua_clearImageHash(lua_State* L)
{
    ImageHash.clear();
    return 0;
}
AddLuaFunction(Lua_clearImageHash,ClearHash);

int Lua_getJoy(lua_State* L)
{
    u32 buttonMask=Input::InputMask;//current button state.
    const char* keys[12] =
    {//Buttons in order of mask.
        "A","B","Select","Start",
        "Right","Left","Up","Down",
        "R","L","X","Y"
    };
    lua_createtable(L, 0, 12);
    for(u32 i=0;i<12;i++)
    {
        lua_pushboolean(L,0 >= (buttonMask&(1<<i)));
        lua_setfield(L,-2,keys[i]);
    }
    return 1;
}
AddLuaFunction(Lua_getJoy,GetJoy);

int Lua_setPadding(lua_State* L) //TODO: Currently only works well with force integer scaling
{
    LeftPadding = abs(luaL_checkinteger(L,1));
    TopPadding = abs(luaL_checkinteger(L,2));
    RightPadding = abs(luaL_checkinteger(L,3));
    BottomPadding = abs(luaL_checkinteger(L,4));
    emuThread->onLuaLayoutChange();
    return 0;
}
AddLuaFunction(Lua_setPadding,SetPadding);

}//LuaFunctionDefinition
}//LuaScript
