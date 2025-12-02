#include "lua/LuaMain.h"
#include "version.h"
#include "Platform.h"
#include <QCryptographicHash>

std::vector<luaL_Reg> clientFunctions;// list of registered lua_CFunctions for this library
LuaLibrary clientLibrary("client",&clientFunctions);//adds "client" to the list of luaLibraries

namespace luaClientDefinitions
{
//Macro to register lua_CFunction with 'name' to the "client" library
#define AddClientFunction(functPointer,name)LuaFunctionRegister name(functPointer,#name,&clientFunctions)

//TODO: add way for lua scripts to distingish between running on Bizhawk and MelonDS
int Lua_getVersion(lua_State* L)
{
    lua_pushstring(L,MELONDS_VERSION);
    return 1;
}
AddClientFunction(Lua_getVersion,getversion);
}
std::vector<luaL_Reg> gameinfoFunctions;// list of registered lua_CFunctions for this library
LuaLibrary gameinfoLibrary("gameinfo",&gameinfoFunctions);//adds "gameinfo" to the list of luaLibraries

namespace luaGameinfoDefinitions
{

//Macro to register lua_CFunction with 'name' to the "gameinfo" library
#define AddGameinfoFunction(functPointer,name)LuaFunctionRegister name(functPointer,#name,&gameinfoFunctions)

int Lua_getromhash(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    auto emuInstance = bundle->getEmuInstance();
    auto nds = emuInstance->getNDS();
    if (emuInstance->getCartType() == -1)
    {
        lua_pushstring(L,"NULL");
        return 1;
    }

    //TODO: Better way to get file path to the current NDS rom...
    std::string romPath = emuInstance->getbaseROMDir()+"/"+emuInstance->getbaseROMName();
    melonDS::Platform::FileHandle* f = melonDS::Platform::OpenFile(romPath, melonDS::Platform::FileMode::Read);
    if (!f)
    {
        romPath = emuInstance->getbaseROMDir()+"\\"+emuInstance->getbaseROMName();
        f = melonDS::Platform::OpenFile(romPath, melonDS::Platform::FileMode::Read);
    }

    if (!f)
    {
        lua_pushstring(L,"NULL");
        return 1;
    }

    long len = melonDS::Platform::FileLength(f);

    melonDS::Platform::FileRewind(f);
    QByteArray filedata = QByteArray(len,0);

    size_t nread = melonDS::Platform::FileRead(filedata.data(), (size_t)len, 1, f);
    melonDS::Platform::CloseFile(f);

    QByteArray hash = QCryptographicHash::hash(filedata, QCryptographicHash::Md5);
    lua_pushstring(L,hash.toHex(0));
    return 1;
}
AddGameinfoFunction(Lua_getromhash,getromhash);
}

std::vector<luaL_Reg> emuFunctions;// list of registered lua_CFunctions for this library
LuaLibrary emuLibrary("emu",&emuFunctions);//adds "emu" to the list of luaLibraries

namespace luaEmuDefinitions
{

//Macro to register lua_CFunction with 'name' to the "emu" library
#define AddEmuFunction(functPointer,name)LuaFunctionRegister name(functPointer,#name,&emuFunctions)

//For compatability with Bizhawk scripts, also an easy way to test if an NDS rom has been loaded yet.
int Lua_Getsystemid(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    auto emuInstance = bundle->getEmuInstance();
    auto nds = emuInstance->getNDS();
    if (emuInstance->getCartType() == -1)
    {
        lua_pushstring(L,"NULL");
        return 1;
    }
    lua_pushstring(L,"NDS");
    return 1;
}
AddEmuFunction(Lua_Getsystemid,getsystemid);

int Lua_Framecount(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    auto emuInstance = bundle->getEmuInstance();
    auto nds = emuInstance->getNDS();
    lua_pushinteger(L,nds->NumFrames);
    return 1;
}
AddEmuFunction(Lua_Framecount,framecount);
}
