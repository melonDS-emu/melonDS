#include "LuaMain.h"
#include <filesystem>
#include <QDir>
#include <QFileDialog>
#include <QPushButton>
#include <QGuiApplication>
#include <QScrollBar>
#include <QPainter>
#include "types.h"
#include "NDS.h"
#include <SDL_joystick.h>
#include <NDS_Header.h>
#include "main.h"

LuaBundle::LuaBundle(LuaConsoleDialog* dialog, EmuInstance* inst)
{
    emuInstance = inst;
    emuThread = emuInstance->getEmuThread();
    luaDialog = dialog;
    overlays = new std::vector<OverlayCanvas>;
    imageHash = new QHash<QString, QImage>;
}

LuaConsoleDialog::LuaConsoleDialog(QWidget* parent) : QDialog(parent)
{
    QWidget* w = parent;
    MainWindow* mainWindow;
    for (;;) //copied from ScreenPanel in Screen.cpp
    {
        mainWindow = qobject_cast<MainWindow*>(w);
        if (mainWindow) break;
        w = w->parentWidget();
        if (!w) break;
    }
    bundle = new LuaBundle(this,mainWindow->getEmuInstance());
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
    this->setWindowTitle("Lua Script");
}

void LuaConsoleDialog::closeEvent(QCloseEvent *event)
{
    onStop();
    bundle->overlays->clear();
    flagClosed = true;
    event->accept();
}

void LuaConsoleDialog::onOpenScript()
{
    QFileInfo file = QFileInfo(QFileDialog::getOpenFileName(this, "Load Lua Script",QDir::currentPath()));
    if (!file.exists()) return;
    currentScript = file;
    bundle->flagNewLua = true;
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

void LuaBundle::printText(QString string)
{
    this->luaDialog->console->onGetText(string);
}

void LuaConsoleDialog::onLuaSaveState(QString string)
{
    emit signalLuaSaveState(string);
}

void LuaConsoleDialog::onLuaLoadState(QString string)
{
    emit signalLuaLoadState(string);
}

void LuaConsole::onClear()
{
    this->clear();
}

LuaFunction::LuaFunction(luaFunctionPointer cf,const char* n,std::vector<LuaFunction*>* container)
{
    this->cfunction = cf;
    this->name = n;
    container->push_back(this);
}

static_assert(sizeof(LuaBundle*) <= LUA_EXTRASPACE,"LUA_EXTRASPACE too small");

LuaBundle* get_bundle(lua_State * L)
{
	LuaBundle* pBundle;
	std::memcpy(&pBundle, lua_getextraspace(L), sizeof(LuaBundle*));
	return pBundle;
}

#define MELON_LUA_HOOK_INSTRUCTION_COUNT 50 //number of vm instructions between hook calls
void luaHookFunction(lua_State* L, lua_Debug *arg)
{
    LuaBundle* bundle = get_bundle(L);
    if (bundle->flagStop and (arg->event == LUA_HOOKCOUNT)) 
        luaL_error(L, "Force Stopped");
}

std::vector<LuaFunction*> definedLuaFunctions;//List of all defined lua functions

void LuaBundle::createLuaState()
{
    if (!flagNewLua) return;
    overlays->clear();
    flagNewLua = false;
    luaState = nullptr;
    QByteArray fileName = luaDialog->currentScript.fileName().toLocal8Bit();
    QString filedir = luaDialog->currentScript.dir().path();
    lua_State* L = luaL_newstate();
    LuaBundle* pBundle = this;
    std::memcpy(lua_getextraspace(L), &pBundle, sizeof(LuaBundle*)); //Write a pointer to this LuaBundle into the extra space of the new lua_State
    luaL_openlibs(L);
    for (LuaFunction* function : definedLuaFunctions)
        lua_register(L,function->name,function->cfunction);
    QDir::setCurrent(filedir);
    lua_sethook(L,&luaHookFunction,LUA_MASKCOUNT,MELON_LUA_HOOK_INSTRUCTION_COUNT); 
    if (luaL_dofile(L,fileName.data())==LUA_OK)
    {
        luaState = L;
    }
    else //Error loading script
    {
        printText(lua_tostring(L,-1));
    }
}

void LuaConsoleDialog::onStop()
{
    if (bundle->getLuaState()) 
        bundle->flagStop = true;
}

void LuaConsoleDialog::onPausePlay()
{
    bundle->flagPause = !bundle->flagPause;
}

void LuaConsoleDialog::onLuaUpdate()
{
    bundle->createLuaState();
    bundle->luaUpdate();
}

//Gets Called once a frame
void LuaBundle::luaUpdate()
{
    if (!luaState || flagPause) return;
    if (lua_getglobal(luaState,"_Update")!=LUA_TFUNCTION)
    {
        printText("No \"_Update\" Function found, pausing script...");
        flagPause = true;
        return;
    }
    if (lua_pcall(luaState,0,0,0)!=0)
    {
        //Handel Errors
        printText(lua_tostring(luaState,-1));
        luaState = nullptr;
    }
}

OverlayCanvas::OverlayCanvas(int x,int y,int width,int height,LuaCanvasTarget t)
{
    target = t;
    buffer1 = new QImage(width,height,QImage::Format_ARGB32_Premultiplied);
    buffer2 = new QImage(width,height,QImage::Format_ARGB32_Premultiplied);
    buffer1->fill(0xffffff00); //initializes buffer with yellow pixels (probably should change this to transparent black pixels at some point...)
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

void LuaBundle::luaResetOSD()
{
    for (auto lo = overlays->begin(); lo != overlays->end();)
    {
        OverlayCanvas& overlay = *lo;
        overlay.GLTextureLoaded = false;
        lo++;
    }
}

//TODO: Organize lua functions into different named tables similar to bizhawk.
/*--------------------------------------------------------------------------------------------------
                            Start of lua function definitions
--------------------------------------------------------------------------------------------------*/

namespace luaDefinitions
{

#define AddLuaFunction(functPointer,name)LuaFunction name(functPointer,#name,&definedLuaFunctions)

int lua_MelonPrint(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    const char* string = luaL_checkstring(L,1);
    bundle->printText((QString)string);
    return 0;
}
AddLuaFunction(lua_MelonPrint,print);

int lua_MelonClear(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    bundle->getluaDialog()->console->clear();
    return 0;
}
AddLuaFunction(lua_MelonClear,MelonClear);

enum ramInfo_ByteType
{
    ramInfo_OneByte = 1, 
    ramInfo_TwoBytes = 2, 
    ramInfo_FourBytes = 4
};

melonDS::u32 GetMainRAMValueU(const melonDS::u32& addr, const ramInfo_ByteType& byteType,LuaBundle* bundle)
{

    melonDS::NDS* nds = bundle->getEmuInstance()->getNDS();
    switch (byteType)
    {
    case ramInfo_OneByte:
        return *(melonDS::u8*)(nds->MainRAM + (addr&nds->MainRAMMask));
    case ramInfo_TwoBytes:
        return *(melonDS::u16*)(nds->MainRAM + (addr&nds->MainRAMMask));
    case ramInfo_FourBytes:
        return *(melonDS::u32*)(nds->MainRAM + (addr&nds->MainRAMMask));
    default:
        return 0;
    }

}

melonDS::s32 GetMainRAMValueS(const melonDS::u32& addr, const ramInfo_ByteType& byteType,LuaBundle* bundle)
{
    melonDS::NDS* nds = bundle->getEmuInstance()->getNDS();
    switch (byteType)
    {
    case ramInfo_OneByte:
        return *(melonDS::s8*)(nds->MainRAM + (addr&nds->MainRAMMask));
    case ramInfo_TwoBytes:
        return *(melonDS::s16*)(nds->MainRAM + (addr&nds->MainRAMMask));
    case ramInfo_FourBytes:
        return *(melonDS::s32*)(nds->MainRAM + (addr&nds->MainRAMMask));
    default:
        return 0;
    }
}

int Lua_ReadDatau(lua_State* L,ramInfo_ByteType byteType) 
{   
    LuaBundle* bundle = get_bundle(L);
    melonDS::u32 address = luaL_checkinteger(L,1);
    melonDS::u32 value = GetMainRAMValueU(address,byteType,bundle);
    lua_pushinteger(L, value);
    return 1;
}

int Lua_ReadDatas(lua_State* L,ramInfo_ByteType byteType)
{
    LuaBundle* bundle = get_bundle(L);
    melonDS::u32 address = luaL_checkinteger(L,1);
    melonDS::s32 value = GetMainRAMValueS(address,byteType,bundle);
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
    LuaBundle* bundle = get_bundle(L);
    int x = luaL_checkinteger(L,1);
    int y = luaL_checkinteger(L,2);
    bundle->getEmuInstance()->touchScreen(x,y);
    return 0;
}
AddLuaFunction(Lua_NDSTapDown,NDSTapDown);

int Lua_NDSTapUp(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    bundle->getEmuInstance()->releaseScreen();
    return 0;
}
AddLuaFunction(Lua_NDSTapUp,NDSTapUp);

int Lua_StateSave(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    const char* filename = luaL_checkstring(L,1);
    bundle->getluaDialog()->onLuaSaveState((QString)filename);
    return 0;
}
AddLuaFunction(Lua_StateSave,StateSave);

int Lua_StateLoad(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    const char* filename = luaL_checkstring(L,1);

    bundle->getluaDialog()->onLuaLoadState((QString)filename);
    return 0;
}
AddLuaFunction(Lua_StateLoad,StateLoad);

int Lua_getMouse(lua_State* L)
{
    Qt::MouseButtons btns = QGuiApplication::mouseButtons();
    LuaBundle* bundle = get_bundle(L);
    QPoint pos = bundle->getEmuInstance()->getMainWindow()->panel->mapFromGlobal(QCursor::pos(QGuiApplication::primaryScreen()));
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
    for (int i=0;i<6;i++)
    {
        lua_pushboolean(L,vals[i]);
        lua_setfield(L,-2,keys[i]);
    }
    return 1;//returns table describing the current pos and state of the mouse
}
AddLuaFunction(Lua_getMouse,GetMouse);

int Lua_HeldKeys(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    lua_createtable(L,0,bundle->getEmuInstance()->heldKeys.size());
    for (int key : bundle->getEmuInstance()->heldKeys)
    {
        lua_pushboolean(L,true);
        lua_seti(L,-2,key);
    }
    return 1;//returns table of currently held keys.
}
AddLuaFunction(Lua_HeldKeys,HeldKeys);

/*--------------------------------------------------------------------------------------------------
                            Front-end lua function definitions
--------------------------------------------------------------------------------------------------*/
//TODO: Lua Colors 

//MakeCanvas(int x,int y,int width,int height,[int target,topScreen=0,bottomScreen=1,OSD(default)>=2)],[bool active = true])
int Lua_MakeCanvas(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int w = luaL_checknumber(L,3);
    int h = luaL_checknumber(L,4);
    int t = luaL_optinteger(L,5,2);
    bool a = 0 != luaL_optinteger(L,6,1);
    
    OverlayCanvas canvas(x,y,w,h,(LuaCanvasTarget)t);
    canvas.isActive = a;
    lua_pushinteger(L,bundle->overlays->size());
    bundle->overlays->push_back(canvas);
    return 1; //returns index of the new overlay
}
AddLuaFunction(Lua_MakeCanvas,MakeCanvas);

int Lua_SetCanvas(lua_State* L) //SetCanvas(int index)
{
    LuaBundle* bundle = get_bundle(L);
    int index = luaL_checknumber(L,1);

    bundle->luaCanvas = &bundle->overlays->at(index);
    return 0;
}
AddLuaFunction(Lua_SetCanvas,SetCanvas);
int Lua_ClearOverlay(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    bundle->luaCanvas->imageBuffer->fill(0x00000000);
    return 0;
}
AddLuaFunction(Lua_ClearOverlay,ClearOverlay);

int Lua_Flip(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    bundle->luaCanvas->flip();
    return 0;
}
AddLuaFunction(Lua_Flip,Flip);

//Text(int x, int y, string message, [u32 color = 'black'], [int fontsize = 9], [string fontfamily = Helvetica])
int Lua_text(lua_State* L) 
{
    LuaBundle* bundle = get_bundle(L);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    const char* message = luaL_checklstring(L,3,NULL);
    melonDS::u32 color = luaL_optnumber(L,4,0x00000000);
    const char* FontFamily = luaL_optlstring(L,6,"Helvetica",NULL);
    int size = luaL_optnumber(L,5,9);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    QFont font(FontFamily,size,0,false);
    //font.setStyleStrategy(QFont::NoAntialias);
    //font.setLetterSpacing(QFont::AbsoluteSpacing,-1);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(x,y,message);
    return 0;
}
AddLuaFunction(Lua_text,Text);

int Lua_line(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int x1 = luaL_checknumber(L,1);
    int y1 = luaL_checknumber(L,2);
    int x2 = luaL_checknumber(L,3);
    int y2 = luaL_checknumber(L,4);
    melonDS::u32 color = luaL_checknumber(L,5);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    painter.setPen(color);
    painter.drawLine(x1,y1,x2,y2);
    return 0;
}
AddLuaFunction(Lua_line,Line);

int Lua_rect(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    melonDS::u32 color = luaL_checknumber(L,5);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    painter.setPen(color);
    painter.drawRect(x,y,width,height);
    return 0;
}
AddLuaFunction(Lua_rect,Rect);

int Lua_fillrect(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    melonDS::u32 color = luaL_checknumber(L,5);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    painter.setPen(color);
    painter.fillRect(x,y,width,height,color);
    return 0;
}
AddLuaFunction(Lua_fillrect,FillRect);

int Lua_Ellipse(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    melonDS::u32 color = luaL_checknumber(L,5);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    painter.setPen(color);
    painter.drawEllipse(x,y,width,height);
    return 0;
}
AddLuaFunction(Lua_Ellipse,Ellipse);

int Lua_keystrokes(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    lua_createtable(L,0,bundle->getEmuInstance()->keyStrokes.size());
    for (int i = 0; i<bundle->getEmuInstance()->keyStrokes.size(); i++)
    {
        lua_pushinteger(L,bundle->getEmuInstance()->keyStrokes.at(i));
        lua_seti(L,-2,i);
    }
    bundle->getEmuInstance()->keyStrokes.clear();
    return 1;
}
AddLuaFunction(Lua_keystrokes,Keys);

//DrawImage(string path, int x, int y,[int source x=0], [int source y=0], [int source w=-1],[int source h=-1])
int Lua_drawImage(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    const char* path = luaL_checklstring(L,1,NULL);
    int x = luaL_checkinteger(L,2);
    int y = luaL_checkinteger(L,3);
    int sx = luaL_optinteger(L,4,0);
    int sy = luaL_optinteger(L,5,0);
    int sw = luaL_optinteger(L,6,-1);
    int sh = luaL_optinteger(L,7,-1);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    QImage image;
    if (bundle->imageHash->contains(path))
    {
        image=(*bundle->imageHash)[path];
    }
    else
    {
        image=QImage((QString)path);
        (*bundle->imageHash)[path] = image;
    }
    painter.drawImage(x,y,image,sx,sy,sw,sh);
    return 0;
}
AddLuaFunction(Lua_drawImage,DrawImage);

int Lua_clearImageHash(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    bundle->imageHash->clear();
    return 0;
}
AddLuaFunction(Lua_clearImageHash,ClearHash);

int Lua_getJoy(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    melonDS::u32 buttonMask=bundle->getEmuInstance()->getInputMask();//current button state.
    const char* keys[12] =
    {//Buttons in order of mask.
        "A","B","Select","Start",
        "Right","Left","Up","Down",
        "R","L","X","Y"
    };
    lua_createtable(L, 0, 12);
    for (melonDS::u32 i=0;i<12;i++)
    {
        lua_pushboolean(L,0 >= (buttonMask&(1<<i)));
        lua_setfield(L,-2,keys[i]);
    }
    return 1;
}
AddLuaFunction(Lua_getJoy,GetJoy);

int Lua_getJoyStick(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int axisNum = luaL_checknumber(L,1);
    int val = bundle->getEmuInstance()->getJoyStickAxis(axisNum);
    lua_pushinteger(L,val);
    return 1;
}
AddLuaFunction(Lua_getJoyStick,GetJoyStick);

}