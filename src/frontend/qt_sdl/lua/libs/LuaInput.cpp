#include "lua/LuaMain.h"

std::vector<luaL_Reg> inputFunctions;// list of registered lua_CFunctions for this library
LuaLibrary inputLibrary("input",&inputFunctions);//adds "input" to the list of luaLibraries

namespace luaInputDefinitions
{
//Macro to register lua_CFunction with 'name' to the "input" library
#define AddInputFunction(functPointer,name)LuaFunctionRegister name(functPointer,#name,&inputFunctions)

int Lua_NDSTapDown(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int x = luaL_checkinteger(L,1);
    int y = luaL_checkinteger(L,2);
    bundle->getEmuInstance()->touchScreen(x,y);
    return 0;
}
AddInputFunction(Lua_NDSTapDown,NDSTapDown);

int Lua_NDSTapUp(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    bundle->getEmuInstance()->releaseScreen();
    return 0;
}
AddInputFunction(Lua_NDSTapUp,NDSTapUp);

int Lua_getMouse(lua_State* L)
{
    Qt::MouseButtons btns = QGuiApplication::mouseButtons();
    LuaBundle* bundle = get_bundle(L);
    int wheel = bundle->getEmuInstance()->getMainWindow()->panel->getMouseWheel();
    QPoint pos = bundle->getEmuInstance()->getMainWindow()->panel->mapFromGlobal(QCursor::pos(QGuiApplication::primaryScreen()));
    const char* keys[5] = {"Left","Middle","Right","XButton1","XButton2"};
    bool vals[5] =
    {
        btns.testFlag(Qt::LeftButton),
        btns.testFlag(Qt::MiddleButton),
        btns.testFlag(Qt::RightButton),
        btns.testFlag(Qt::XButton1),
        btns.testFlag(Qt::XButton2)
    };
    lua_createtable(L, 0, 8);
    lua_pushinteger(L, pos.x());
    lua_setfield(L, -2, "X");
    lua_pushinteger(L, pos.y());
    lua_setfield(L, -2, "Y");
    lua_pushinteger(L,wheel);
    lua_setfield(L,-2,"Wheel");
    for (int i=0;i<5;i++)
    {
        lua_pushboolean(L,vals[i]);
        lua_setfield(L,-2,keys[i]);
    }
    return 1;//returns table describing the current pos and state of the mouse
}
AddInputFunction(Lua_getMouse,getmouse);

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
AddInputFunction(Lua_HeldKeys,HeldKeys);

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
AddInputFunction(Lua_keystrokes,Keys);

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
AddInputFunction(Lua_getJoy,getjoy);

int Lua_getJoyStick(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int axisNum = luaL_checknumber(L,1);
    int val = bundle->getEmuInstance()->getJoyStickAxis(axisNum);
    lua_pushinteger(L,val);
    return 1;
}
AddInputFunction(Lua_getJoyStick,GetJoyStick);

}