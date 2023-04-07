#include "LuaDialog.h"
#include <QDialog>

std::vector<LuaScript::LuaFunction*> LuaDialog::LuaDialogFunctions;
//Initalize the lua function
#define AddDialogLuaFunct(fpointer,name)LuaScript::LuaFunction name(fpointer,#name,&LuaDialog::LuaDialogFunctions)

int (*CurrentDialogFunction)(lua_State*)=nullptr;
int DialogReturn;
int CurrentRunningDialog=0;
int DialogRetVal=0;
void LuaThread::luaDialogReturned(){
    flagDialogReturn=true;
    exit();
}

//SomeDialog Functions Must be executed by GUI Thread.
int LuaThread::luaDialogFunction(lua_State* L,int (*dialogFunction)(lua_State*)){
    flagDialogReturn=false;
    CurrentDialogFunction=dialogFunction;
    LuaScript::MainLuaState=L;
    emit signalDialogFunction();
    while (!flagDialogReturn){
        exec();
    }
    return DialogReturn;
}
//So dialog functions can be executed by GUI Thread
void LuaDialog::DialogFunction(){
    DialogReturn = CurrentDialogFunction(LuaScript::MainLuaState);
    luaThread->luaDialogReturned();
}

//Start Dialog From GUI Thread
int LuaThread::luaDialogStart(lua_State* L,int dialog){
    flagDialogClosed = false;
    CurrentRunningDialog = dialog;
    emit signalStartDialog();
    while (!flagDialogClosed){
        exec();
    }
    lua_pushinteger(L,DialogRetVal);
    return 1;
}

void LuaThread::luaDialogClosed(){
    flagDialogClosed=true;
    exit();
}


int lua_ExecDialog(lua_State* L){
    int dialog = luaL_checkinteger(L,1);
    return luaThread->luaDialogStart(L,dialog);
}
AddDialogLuaFunct(lua_ExecDialog,execDialog);





LuaDialog::WidgetContainer::WidgetContainer(LuaDialog::LuaWidgetType t,void* w){
    this->type=t;
    this->widget=w;
}

std::vector<QDialog*> Dialogs;
std::vector<LuaDialog::WidgetContainer> Widgets;

//Gui Thread Starts Dialog
void LuaDialog::StartDialog(){
    Dialogs.at(CurrentRunningDialog)->open();
    luaThread->luaDialogClosed();
}


//Call function in the Lua_Registry with index ref in lua_State L
void CallFunction(lua_State* L,int ref){
    //push callfunction to top of stack
    lua_geti(L,LUA_REGISTRYINDEX,ref);
    if(lua_pcall(L,0,0,0)!=0){
        //handel Errors
        printf("Error: %s\n",lua_tostring(L,-1));
    }
}

LuaLabel::LuaLabel(QDialog* parent,QString string){
    this->setParent(parent);
    this->setText(string);
    L=nullptr;
    ref=0;
    index = Widgets.size();
    Widgets.push_back(LuaDialog::WidgetContainer(LuaDialog::Label,this));
}
void LuaLabel::luaFunction(){
    CallFunction(L,ref);
}

LuaButton::LuaButton(QDialog* parent,QString string, int r,lua_State* l){
    this->setParent(parent);
    this->setText(string);
    L=l;
    ref=r;
    index=Widgets.size();
    Widgets.push_back(LuaDialog::WidgetContainer(LuaDialog::Button,this));
}
void LuaButton::luaFunction(){
    CallFunction(L,ref);
}

LuaLineEdit::LuaLineEdit(QDialog* parent){
    this->setParent(parent);
    L=nullptr;
    ref=0;
    index=Widgets.size();
    Widgets.push_back(LuaDialog::WidgetContainer(LuaDialog::LineEdit,this));
}
void LuaLineEdit::luaFunction(){
    CallFunction(L,ref);
}

LuaComboBox::LuaComboBox(QDialog* parent,QStringList list){
    this->setParent(parent);
    this->insertItems(0,list);
    L=nullptr;
    ref=0;
    index=Widgets.size();
    Widgets.push_back(LuaDialog::WidgetContainer(LuaDialog::Dropdown,this));
}
void LuaComboBox::luaFunction(){
    CallFunction(L,ref);
}



//used to define lua function that will be run by GUI thread while Lua thread waits.
#define LUADIALOG(c_pntr,name)\
int name(lua_State* L){return luaThread->luaDialogFunction(L,c_pntr);}

int Dialog_New(lua_State* L){
    int w=luaL_checkinteger(L,1);
    int h=luaL_checkinteger(L,2);
    QDialog* newDialog = new QDialog(LuaFront::panel->window());
    newDialog->setMinimumSize(w,h);
    Dialogs.push_back(newDialog);
    lua_pushinteger(L,Dialogs.size()-1);
    newDialog->open();
    return 1;
}
LUADIALOG(Dialog_New,lua_newDialog)
AddDialogLuaFunct(lua_newDialog,newDialog);


int Dialog_Label(lua_State* L){
    int id = lua_tointeger(L,-6);
    QString string=lua_tostring(L,-5);
    int x = lua_tointeger(L,-4);
    int y = lua_tointeger(L,-3);
    int w = lua_tointeger(L,-2);
    int h = lua_tointeger(L,-1);
    QDialog* dialog = Dialogs[id];
    LuaLabel* label = new LuaLabel(dialog,string);
    label->setGeometry(x,y,w,h);
    lua_pushinteger(L,label->index);
    label->show();
    return 1;
}
LUADIALOG(Dialog_Label,lua_newLabel)
AddDialogLuaFunct(lua_newLabel,newLabel);

int Dialog_Button(lua_State* L){
    int id = luaL_checkinteger(L,1);
    QString string=luaL_checkstring(L,2);
    luaL_checktype(L,3, LUA_TFUNCTION);
    int x = luaL_checkinteger(L,4);
    int y = luaL_checkinteger(L,5);
    int w = luaL_checkinteger(L,6);
    int h = luaL_checkinteger(L,7);
    lua_pushvalue(L,3);
    // create refrence to function using Lua registry.
    int ref = luaL_ref(L,LUA_REGISTRYINDEX);
    QDialog* dialog = Dialogs[id];
    LuaButton* button = new LuaButton(dialog,string,ref,L);
    button->setGeometry(x,y,w,h);
    dialog->connect(button,&QPushButton::clicked,button,LuaButton::luaFunction);
    lua_pushinteger(L,button->index);
    button->show();
    return 1;
}
LUADIALOG(Dialog_Button,lua_newButton)
AddDialogLuaFunct(lua_newButton,newButton);

int Dialog_LineEdit(lua_State* L){
    int id =luaL_checkinteger(L,1);
    QDialog* dialog = Dialogs[id];
    LuaLineEdit* lineEdit = new LuaLineEdit(dialog);
    lua_pushinteger(L,lineEdit->index);
    lineEdit->show();
    return 1;
}
LUADIALOG(Dialog_LineEdit,lua_newLineEdit)
AddDialogLuaFunct(lua_newLineEdit,newLineEdit);

int Dialog_setGeometry(lua_State*L){
    int id = luaL_checkinteger(L,1);
    int x = luaL_checkinteger(L,2);
    int y = luaL_checkinteger(L,3);
    int w = luaL_checkinteger(L,4);
    int h = luaL_checkinteger(L,5);
    static_cast<QWidget*>(Widgets[id].widget)->setGeometry(x,y,w,h);
    return 0;
}
LUADIALOG(Dialog_setGeometry,lua_setGeometry)//no need
AddDialogLuaFunct(lua_setGeometry,setGeometry);

int Dialog_getText(lua_State*L){
    int id = luaL_checkinteger(L,1);
    QString text="default";
    switch (Widgets[id].type){
    case LuaDialog::LineEdit:
        text = static_cast<LuaLineEdit*>(Widgets[id].widget)->text();
        break;
    case LuaDialog::Label:
        text = static_cast<LuaLabel*>(Widgets[id].widget)->text();
        break;
    case LuaDialog::Button:
        text = static_cast<LuaButton*>(Widgets[id].widget)->text();
        break;
    case LuaDialog::Dropdown:
        text = static_cast<LuaComboBox*>(Widgets[id].widget)->currentText();
        break;
    default:
        text = "";
        break;
    }
    lua_pushstring(L,&text.toStdString()[0]);
    return 1;
}
LUADIALOG(Dialog_getText,lua_getText)//no need
AddDialogLuaFunct(lua_getText,getText);

int Dialog_Dropdown(lua_State* L){
    int id = luaL_checkinteger(L,1);
    QStringList list;
    luaL_checktype(L,2,LUA_TTABLE);
    lua_pushnil(L);  // first key
    while (lua_next(L, -2) != 0) {
        list.append(lua_tostring(L,-1));//'value' (at index -1)
        lua_pop(L, 1);//remove 'value' leaving 'key' for lua_next
    }
    LuaComboBox* comboBox = new LuaComboBox(Dialogs[id],list);
    lua_pushinteger(L,comboBox->index);
    comboBox->show();
    return 1;
}
LUADIALOG(Dialog_Dropdown,lua_dropdown)
AddDialogLuaFunct(lua_dropdown,dropDown);

int Dialog_setDropDownItems(lua_State* L){
    int id = luaL_checkinteger(L,1);
    QStringList list;
    luaL_checktype(L,2,LUA_TTABLE);
    luaL_checktype(L,2,LUA_TTABLE);
    lua_pushnil(L);  // first key
    while (lua_next(L, -2) != 0) {
        list.append(lua_tostring(L,-1));//'value' (at index -1)
        lua_pop(L, 1);//remove 'value' leaving 'key' for lua_next
    }
    list.sort();
    LuaComboBox* comboBox = static_cast<LuaComboBox*>(Widgets[id].widget);
    comboBox->clear();
    comboBox->addItems(list);
    return 0;
}
LUADIALOG(Dialog_setDropDownItems,lua_setDropDownItems)//no need?
AddDialogLuaFunct(lua_setDropDownItems,setDropDownItems);


int Dialog_Destroy(lua_State* L){
    int id = luaL_checkinteger(L,1);
    Dialogs[id]->close();
    Dialogs.clear();
    Widgets.clear();
    return 0;
}
LUADIALOG(Dialog_Destroy,lua_destroy)
AddDialogLuaFunct(lua_destroy,destroy);

int Dialog_OpenFile(lua_State* L){
    QString filename = luaL_checkstring(L,1);
    QString initialDirectory = luaL_checkstring(L,2);
    QString filter = luaL_checkstring(L,3);
    QString ret = QFileDialog::getOpenFileName(LuaFront::panel->window(),"Open File", initialDirectory, filter);
    lua_pushstring(L,&ret.toStdString()[0]);
    return 1;
}
LUADIALOG(Dialog_OpenFile,lua_OpenFile)
AddDialogLuaFunct(lua_OpenFile,openFile);