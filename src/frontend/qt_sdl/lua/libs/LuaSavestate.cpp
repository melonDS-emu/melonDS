#include "lua/LuaMain.h"

void LuaConsoleDialog::onLuaSaveState(QString string)
{
    emit signalLuaSaveState(string);
}

void LuaConsoleDialog::onLuaLoadState(QString string)
{
    emit signalLuaLoadState(string);
}

std::vector<luaL_Reg> savestateFunctions;// list of registered lua_CFunctions for this library
LuaLibrary savestateLibrary("savestate",&savestateFunctions);//adds "savestate" to the list of luaLibraries

namespace luaSavestateDefinitions
{

//Macro to register lua_CFunction with 'name' to the "savestate" library
#define AddSavestateFunction(functPointer,name)LuaFunctionRegister name(functPointer,#name,&savestateFunctions)

int Lua_StateSave(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    const char* filename = luaL_checkstring(L,1);
    bundle->getluaDialog()->onLuaSaveState((QString)filename);
    return 0;
}
AddSavestateFunction(Lua_StateSave,StateSave);

int Lua_StateLoad(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    const char* filename = luaL_checkstring(L,1);

    bundle->getluaDialog()->onLuaLoadState((QString)filename);
    return 0;
}
AddSavestateFunction(Lua_StateLoad,StateLoad);
}