#include "lua/LuaMain.h"
#include <QPushButton>
#include <QLabel>
#include <QComboBox>


void LuaConsoleDialog::onLuaCallback()
{
    //Get refrence to lua function from sender
    int ref = QObject::sender()->property("lua_func_ref").toInt();
    bundle->luaCallback(ref);
}

void LuaBundle::luaCallback(int ref){
    //Call refrenced lua function
    if (!luaState || flagPause) return;
    lua_rawgeti(luaState, LUA_REGISTRYINDEX, ref);
    if (lua_pcall(luaState,0,0,0)!=0)
    {
        //Handel Errors
        printText(lua_tostring(luaState,-1));
        luaState = nullptr;
    }
}

//#include <QMetaProperty>
std::vector<luaL_Reg> formFunctions;// list of registered lua_CFunctions for this library
LuaLibrary formLibrary("forms",&formFunctions);//adds "forms" to the list of luaLibraries

namespace luaFormDefinitions
{

//Macro to register lua_CFunction with 'name' to the "form" library
#define AddFormFunction(functPointer,name)LuaFunctionRegister name(functPointer,#name,&formFunctions)

int Lua_newForm(lua_State* L){
    LuaBundle* bundle = get_bundle(L);
    int w = 100;
    int h = 100;
    if (lua_isnil(L,1)==0){
        w = luaL_checkinteger(L,1);
        h = luaL_checkinteger(L,2);
    }
    const char* title = luaL_checkstring(L,3);

    luaL_checktype(L,4,LUA_TFUNCTION);//check arg4 is a function
    lua_pushvalue(L,4);//push function
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);//pop function ref
    
    int index = bundle->luaWidgets->size();
    bundle->luaWidgets->append(new QWidget());
    QWidget* newWidget = bundle->luaWidgets->at(index);

    newWidget->setProperty("lua_func_ref",ref);
    QObject::connect(newWidget,&QObject::destroyed,bundle->getluaDialog(),&LuaConsoleDialog::onLuaCallback);
    newWidget->setWindowTitle(title);
    newWidget->resize(w,h);

    lua_pushinteger(L,index);
    newWidget->show();
    return 1;
}
AddFormFunction(Lua_newForm,newform);

int Lua_newLabel(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int index = luaL_checkinteger(L,1);
    QString string=luaL_checkstring(L,2);
    int x = luaL_optinteger(L,3,0);
    int y = luaL_optinteger(L,4,0);
    int w = luaL_optinteger(L,5,100);
    int h = luaL_optinteger(L,6,100);
    QWidget* parent = bundle->luaWidgets->at(index);
    int labelIndex = bundle->luaWidgets->size();
    bundle->luaWidgets->append(new QLabel(string,parent));
    QWidget* label = bundle->luaWidgets->at(labelIndex);
    label->setGeometry(x,y,w,h);
    lua_pushinteger(L,labelIndex);
    label->show();
    return 1;
}
AddFormFunction(Lua_newLabel,label);

int Lua_newDropdown(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int index = luaL_checkinteger(L,1);
    QWidget* parent = bundle->luaWidgets->at(index);
    int DropdownIndex = bundle->luaWidgets->size();
    bundle->luaWidgets->append(new QComboBox(parent));
    QComboBox* Dropdown = static_cast<QComboBox*>(bundle->luaWidgets->at(DropdownIndex));
    
    int x = luaL_optinteger(L,3,0);
    int y = luaL_optinteger(L,4,0);
    int w = luaL_optinteger(L,5,100);
    int h = luaL_optinteger(L,6,100);
    Dropdown->setGeometry(x,y,w,h);

    QStringList list;
    luaL_checktype(L,2,LUA_TTABLE);//check arg2 is a table
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) //Copy lua string table to QStringList
    {
        list.append(lua_tostring(L,-1));
        lua_pop(L, 1);
    }

    Dropdown->addItems(list);
    lua_pushinteger(L,DropdownIndex);
    Dropdown->show();
    return 1;
}
AddFormFunction(Lua_newDropdown,dropdown);

int Lua_newButton(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int index = luaL_checkinteger(L,1);
    QWidget* parent = bundle->luaWidgets->at(index);
    int buttonIndex = bundle->luaWidgets->size();
    bundle->luaWidgets->append(new QPushButton(parent));
    QPushButton* button = static_cast<QPushButton*>(bundle->luaWidgets->at(buttonIndex));

    button->setText((QString)luaL_checkstring(L,2));
    luaL_checktype(L,3,LUA_TFUNCTION);//check arg3 is a function
    lua_pushvalue(L,3);//push function
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);//pop function ref
    button->setProperty("lua_func_ref",ref);
    QObject::connect(button,&QPushButton::clicked,bundle->getluaDialog(),&LuaConsoleDialog::onLuaCallback);

    int x = luaL_optinteger(L,4,0);
    int y = luaL_optinteger(L,5,0);
    int w = luaL_optinteger(L,6,100);
    int h = luaL_optinteger(L,7,100);
    button->setGeometry(x,y,w,h);

    lua_pushinteger(L,buttonIndex);
    button->show();
    return 1;
}
AddFormFunction(Lua_newButton,button);

int Lua_setLocation(lua_State* L){
    LuaBundle* bundle = get_bundle(L);
    int index = luaL_checkinteger(L,1);
    //TODO: check given index is valid
    QWidget* lWidget = bundle->luaWidgets->at(index);
    int x = luaL_checkinteger(L,2);
    int y = luaL_checkinteger(L,3);
    lWidget->move(x,y);
    return 0;
}
AddFormFunction(Lua_setLocation,setlocation);

int Lua_setDropDownItems(lua_State* L){
    LuaBundle* bundle = get_bundle(L);
    int index = luaL_checkinteger(L,1);
    //TODO: check given index is valid
    QComboBox* Dropdown = static_cast<QComboBox*>(bundle->luaWidgets->at(index));
    
    QStringList list;
    luaL_checktype(L,2,LUA_TTABLE);//check arg2 is a table
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) //Copy lua string table to QStringList
    {
        list.append(lua_tostring(L,-1));
        lua_pop(L, 1);
    }
    Dropdown->clear();
    Dropdown->addItems(list);
    return 0;
}
AddFormFunction(Lua_setDropDownItems,setdropdownitems);

int Lua_setProperty(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int index = luaL_checkinteger(L,1);
    QWidget* widget = bundle->luaWidgets->at(index);
    const char* propName = luaL_checkstring(L,2);
    if(lua_type(L,3)==LUA_TSTRING){
        const char* str = luaL_checkstring(L,3);
        lua_pushboolean(L,widget->setProperty(propName,(QString)str));
    }else{
        double num = luaL_checknumber(L,3);
        lua_pushboolean(L,widget->setProperty(propName,num));
    }
    return 1;
}
AddFormFunction(Lua_setProperty,setproperty);

int Lua_getText(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int index = luaL_checkinteger(L,1);
    QWidget* widget = bundle->luaWidgets->at(index);
    // const QMetaObject* metaObject = widget->metaObject();
    // for(int i = metaObject->propertyOffset(); i < metaObject->propertyCount(); ++i){
    //     bundle->printText(QString::fromLatin1(metaObject->property(i).name()));
    // }
    QString text = widget->property("currentText").toString();
    lua_pushstring(L,text.toStdString().c_str());
    return 1;
}
AddFormFunction(Lua_getText,gettext);

int Lua_Destroy(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int index = luaL_checkinteger(L,1);
    bool didClose = bundle->luaWidgets->at(index)->close();
    lua_pushboolean(L,didClose);
    return 1;
}
AddFormFunction(Lua_Destroy,destroy);

}
