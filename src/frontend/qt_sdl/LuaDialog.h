#include "LuaFrontEnd.h"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QFileDialog>

class LuaLabel : public QLabel{
    Q_OBJECT
public:
    LuaLabel(QDialog* d,QString s);
    int ref;
    int index;
    lua_State* L;
public slots:
    void luaFunction();
};

class LuaButton : public QPushButton{
    Q_OBJECT
public:
    LuaButton(QDialog*,QString,int,lua_State*);
    int ref;
    int index;
    lua_State* L;
public slots:
    void luaFunction();
};

class LuaLineEdit : public QLineEdit{
    Q_OBJECT
public:
    LuaLineEdit(QDialog*);
    int ref;
    int index;
    lua_State* L;
public slots:
    void luaFunction();
};

class LuaComboBox : public QComboBox{
    Q_OBJECT
public:
    LuaComboBox(QDialog*,QStringList);
    int ref;
    int index;
    lua_State* L;
public slots:
    void luaFunction();
};

namespace LuaDialog{
extern std::vector<LuaScript::LuaFunction*> LuaDialogFunctions;
void DialogFunction();
void StartDialog();
enum LuaWidgetType{Dialog,Dropdown,Label,LineEdit,TextBox,CheckBox,Button,PictureBox};
struct WidgetContainer{
    LuaWidgetType type;
    void* widget;
    WidgetContainer(LuaWidgetType,void*);
};
}

