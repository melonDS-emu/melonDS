#include <string>
#include "LuaScript.h"
#include <vector>
#include "NDS.h"
#include "LuaFrontEnd.h"
#include <direct.h>
#include <QDir>
#include "LuaDialog.h"

LuaThread* luaThread = nullptr;
bool frameFlag = false;

void LuaThread::luaUpdate()
{
    flagUpdate = true;
    frameFlag = true;
    exit();
}

void LuaThread::luaStop()
{
    flagRunning=false;
}

void LuaThread::luaYeild()
{
    LuaThread::exec();   
}

void LuaThread::luaLayoutChange()
{
    emit signalChangeScreenLayout();
}

void LuaThread::run()
{
    LuaScript::StartLuaScript(scriptFile);
    emit signalStarted();
    flagRunning=true;
    while (flagRunning){
        if(flagUpdate){
            flagUpdate=false;
            LuaScript::TriggerLuaScript();
        }
        LuaThread::exec();
    }
}

using namespace LuaScript;
std::vector<LuaFunction*> LuaFunctionList;
#define AddLuaFunction(functPointer,name)LuaScript::LuaFunction name(functPointer,#name,&LuaFunctionList)
LuaFunction::LuaFunction(lfunctpntr cfunctionPointer,const char* LuaFunctionName,std::vector<LuaFunction*>* holder){
    this->funct=cfunctionPointer;
    this->name=LuaFunctionName;
    holder->push_back(this);
}


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




int Lua_ReadData(lua_State* L,ramInfo_ByteType byteType) {   
    // address of memory
    lua_Number arg_1 = lua_tonumber(L, -1);
    u32 result =GetMainRAMValueU((u32)arg_1,byteType);
    lua_pushinteger(L, result);
    return 1;
}


int Lua_Readu8(lua_State* L){
    return Lua_ReadData(L,ramInfo_OneByte);
}
AddLuaFunction(Lua_Readu8,Readu8);

int Lua_Readu16(lua_State* L){
    return Lua_ReadData(L,ramInfo_TwoBytes);
}
AddLuaFunction(Lua_Readu16,Readu16);

int Lua_Readu32(lua_State* L){
    return Lua_ReadData(L,ramInfo_FourBytes);
}
AddLuaFunction(Lua_Readu32,Readu32);

int Lua_nextFrame(lua_State* L){
    frameFlag=false;
    while(!frameFlag){
        luaThread->luaYeild();
    }
    return 0;
}
AddLuaFunction(Lua_nextFrame,nextFrame);



bool MainLuaRunning=false;

lua_State* LuaScript::MainLuaState = nullptr;


void LuaScript::StartLuaScript(QFileInfo script){
    std::string fileName = script.fileName().toStdString();
    std::string filedir = script.dir().path().toStdString();
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    for(LuaFunction* function : LuaFunctionList){
        lua_pushcfunction(L, function->funct);
        lua_setglobal(L, function->name);
    }
    for(LuaFunction* function : LuaFront::FrontEndFunctions){
        lua_pushcfunction(L,function->funct);
        lua_setglobal(L, function->name);
    }
    for(LuaFunction* function: LuaDialog::LuaDialogFunctions){
        lua_pushcfunction(L,function->funct);
        lua_setglobal(L,function->name);
    }
    _chdir(&filedir[0]);
    if (luaL_dofile(L,&fileName[0])==LUA_OK){
        LuaScript::MainLuaState=L;
    }else{
        printf("Error: %s\n", lua_tostring(L, -1));
        LuaScript::MainLuaState=nullptr;
    }    
}



void LuaScript::TriggerLuaScript(){
    if (LuaScript::MainLuaState==nullptr){
        return;
    }
    //todo add proper checks for exceptions
    lua_getglobal(LuaScript::MainLuaState,"emuTrigger");//put function on stack and call
    if(lua_pcall(LuaScript::MainLuaState,0,0,0)==0){
    }
    else{
        printf( "Error: %s\n", lua_tostring(LuaScript::MainLuaState, -1));

    }

}


