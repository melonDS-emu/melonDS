#ifndef LUAMAIN_H
#define LUAMAIN_H
#include <QDialog>
#include <QPlainTextEdit>
#include <QFileInfo>
#include <lua.hpp>
#include "EmuInstance.h"

class LuaConsole: public QPlainTextEdit
{
    Q_OBJECT
public:
    LuaConsole(QWidget* parent=nullptr);
public slots:
    void onGetText(const QString& string);
    void onClear();
};

class LuaBundle;

class LuaConsoleDialog: public QDialog
{
    Q_OBJECT
public:
    LuaConsoleDialog(QWidget* parent);
    LuaBundle* getLuaBundle(){return bundle;};
    LuaConsole* console;
    QFileInfo currentScript;
    QPushButton* buttonOpenScript;
    QPushButton* buttonStartStop;
    QPushButton* buttonPausePlay;
    QScrollBar* bar;
protected:
    void closeEvent(QCloseEvent *event) override;
    LuaBundle* bundle;
signals:
    void signalNewLua();
    void signalClosing();
public slots:
    //void onStartStop();
    void onOpenScript();
    void onStop();
    void onPausePlay();
};

//Based on ScreenLayout::GetScreenTransforms
enum LuaCanvasTarget 
{
    canvasTarget_TopScreen = 0,
    canvasTarget_BottomScreen = 1,
    canvasTarget_OSD = 2 //Used for drawing to OSD / non-screen target
};

struct OverlayCanvas
{
    QImage* imageBuffer; // buffer edited by luascript
    QImage* displayBuffer; //buffer displayed on screen
    QImage* buffer1;
    QImage* buffer2;
    QRect rectangle;
    LuaCanvasTarget target = canvasTarget_OSD;
    bool isActive = true; // only active overlays are drawn
    unsigned int GLTexture; // used by GL rendering
    bool GLTextureLoaded;
    OverlayCanvas(int x, int y,int w, int h, LuaCanvasTarget target = canvasTarget_OSD);
    void flip();//used to swap buffers / update canvas
    bool flipped; //used to signal update to graphics.
};

typedef int(*luaFunctionPointer)(lua_State*);
struct LuaFunction
{
    luaFunctionPointer cfunction;
    const char* name;
    LuaFunction(luaFunctionPointer,const char*,std::vector<LuaFunction*>*);
};

class LuaBundle
{
    EmuInstance* emuInstance;
    EmuThread* emuThread;
    LuaConsoleDialog* luaDialog;
    lua_State* luaState = nullptr;
    int basketID;

    void luaResetOSD();
    void luaPrint(QString string);
    void luaClearConsole();  
    
    OverlayCanvas* currentCanvas = nullptr;
    friend class LuaConsolDialog;
public:
    LuaBundle(LuaConsoleDialog* dialog,EmuInstance* inst);
    lua_State* getLuaState(){return luaState;};
    EmuThread* getEmuThread(){return emuThread;};
    EmuInstance* getEmuInstance(){return emuInstance;};
    void createLuaState();
    void luaUpdate();
    bool flagPause = false;
    bool flagStop = false;
    bool flagNewLua = false;
    OverlayCanvas* luaCanvas = nullptr;
    std::vector<OverlayCanvas>* overlays;
    QHash<QString, QImage>* imageHash;

};

#endif