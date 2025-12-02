#include "lua/LuaMain.h"
#include <QFileDialog>
#include <QPushButton>
#include <QScrollBar>

LuaBundle::LuaBundle(LuaConsoleDialog* dialog, EmuInstance* inst)
{
    emuInstance = inst;
    emuThread = emuInstance->getEmuThread();
    luaDialog = dialog;
    overlays = new std::vector<OverlayCanvas>;
    imageHash = new QHash<QString, QImage>;
    luaWidgets = new QList<QWidget*>;
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
QList<LuaLibrary*> luaLibraries;

LuaLibrary::LuaLibrary(const char* libName,std::vector<luaL_Reg>* luaFuncs){
    this->libName=libName;
    this->luaFuncs=luaFuncs;
    luaLibraries.push_back(this);
}

void LuaLibrary::load(lua_State* L){
    lua_createtable(L, 0, this->luaFuncs->size());
    this->luaFuncs->push_back((luaL_Reg){NULL,NULL});//append sentinel value
    luaL_setfuncs(L,this->luaFuncs->data(),0);
    this->luaFuncs->pop_back();
    lua_setglobal(L,this->libName);
}

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
    for (LuaLibrary* lLib: luaLibraries)
        lLib->load(L);
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

//"Global" lua functions

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
AddLuaFunction(lua_MelonPrint,print); //re-defines gloabal "print" function

int lua_MelonClear(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    bundle->getluaDialog()->console->clear();
    return 0;
}
AddLuaFunction(lua_MelonClear,console_clear); //defines a global "console_clear" function

}