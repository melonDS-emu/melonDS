#include "lua/LuaMain.h"
#include <QPainter>


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
    updated = false;
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

std::vector<luaL_Reg> guiFunctions;// list of registered lua_CFunctions for this library
LuaLibrary guiLibrary("gui",&guiFunctions);//adds "gui" to the list of luaLibraries

namespace luaGuiDefinitions
{

//Macro to register lua_CFunction with 'name' to the "gui" library
#define AddGuiFunction(functPointer,name)LuaFunctionRegister name(functPointer,#name,&guiFunctions)

int Lua_MakeCanvas(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int w = luaL_checknumber(L,3);
    int h = luaL_checknumber(L,4);
    int t = luaL_optinteger(L,5,2);
    bool a = (0 != luaL_optinteger(L,6,1));//non zero is true 
    
    OverlayCanvas canvas(x,y,w,h,(LuaCanvasTarget)t);
    canvas.isActive = a;
    lua_pushinteger(L,bundle->overlays->size());
    bundle->overlays->push_back(canvas);
    return 1; //returns index of the new overlay
}
AddGuiFunction(Lua_MakeCanvas,MakeCanvas);

int Lua_SetCanvas(lua_State* L) //SetCanvas(int index)
{
    LuaBundle* bundle = get_bundle(L);
    int index = luaL_checknumber(L,1);

    bundle->luaCanvas = &bundle->overlays->at(index);
    return 0;
}
AddGuiFunction(Lua_SetCanvas,SetCanvas);

int Lua_ClearOverlay(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    bundle->luaCanvas->imageBuffer->fill(0x00000000);
    bundle->luaCanvas->updated=true;
    return 0;
}
AddGuiFunction(Lua_ClearOverlay,ClearOverlay);

int Lua_Flip(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    bundle->luaCanvas->flip();
    return 0;
}
AddGuiFunction(Lua_Flip,Flip);

int Lua_CheckUpdate(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    if (bundle->luaCanvas->updated)
    {
        bundle->luaCanvas->flip();
        bundle->luaCanvas->imageBuffer->fill(0x00000000);
    }
    return 0;
}
AddGuiFunction(Lua_CheckUpdate,checkupdate);
//drawString(int x, int y, string message, [u32 color = 'black'], [int fontsize = 9], [string fontfamily = Helvetica], [int spacing = 0])
int Lua_drawString(lua_State* L) 
{
    if (lua_isnil(L,3)) return 0;
    LuaBundle* bundle = get_bundle(L);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    const char* message = luaL_checkstring(L,3);
    melonDS::u32 color = luaL_optnumber(L,4,0x00000000);
    int size = luaL_optnumber(L,5,9);
    const char* FontFamily = luaL_optlstring(L,6,"Helvetica",NULL);
    int spacing = luaL_optnumber(L,7,0);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    QFont font(FontFamily,size,0,false);
    font.setLetterSpacing(QFont::AbsoluteSpacing,spacing);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(x,y,message);
    bundle->luaCanvas->updated=true;
    return 0;
}
AddGuiFunction(Lua_drawString,drawString);
AddGuiFunction(Lua_drawString,drawText);

int Lua_Line(lua_State* L)
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
    bundle->luaCanvas->updated=true;
    return 0;
}
AddGuiFunction(Lua_Line,drawLine);

int Lua_Pixel(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    melonDS::u32 color = luaL_checknumber(L,3);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    painter.setPen(color);
    painter.drawPoint(x,y);
    bundle->luaCanvas->updated=true;
    return 0;
}
AddGuiFunction(Lua_Pixel,drawPixel);

int Lua_rect(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);

    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);
    melonDS::u32 color = luaL_checknumber(L,5);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    painter.setPen(color);
    painter.drawRect(x,y,width,height);
    bundle->luaCanvas->updated=true;
    return 0;
}
AddGuiFunction(Lua_rect,Rect);

int Lua_fillrect(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);
    melonDS::u32 color = luaL_checknumber(L,5);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    painter.setPen(color);
    painter.fillRect(x,y,width,height,color);
    bundle->luaCanvas->updated=true;
    return 0;
}
AddGuiFunction(Lua_fillrect,FillRect);

int Lua_drawRectangle(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);
    melonDS::u32 linecolor = luaL_optinteger(L,5,0);
    melonDS::u32 fillcolor = luaL_optnumber(L,6,0);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    painter.setPen(linecolor);
    painter.fillRect(x,y,width,height,fillcolor);
    bundle->luaCanvas->updated=true;
    return 0;
}
AddGuiFunction(Lua_drawRectangle,drawRectangle);

int Lua_Ellipse(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    int x = luaL_checknumber(L,1);
    int y = luaL_checknumber(L,2);
    int width = luaL_checknumber(L,3);
    int height = luaL_checknumber(L,4);
    melonDS::u32 color = luaL_checknumber(L,5);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    painter.setPen(color);
    painter.drawEllipse(x,y,width,height);
    bundle->luaCanvas->updated=true;
    return 0;
}
AddGuiFunction(Lua_Ellipse,Ellipse);

QImage imageFetch(LuaBundle* bundle,const char* path){
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
    return image;
}

//DrawImage(string path, int source_x, int source_y,int source_width, int source_height=0, int dest_x,int dest_y,[int dest_width=-1],[int dest_dest_height=-1])
int Lua_DrawImageRegion(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    const char* path = luaL_checklstring(L,1,NULL);
    int source_x = luaL_checkinteger(L,2);
    int source_y = luaL_checkinteger(L,3);
    int source_width = luaL_checkinteger(L,4);
    int source_height = luaL_checkinteger(L,5);
    int dest_x = luaL_checkinteger(L,6);
    int dest_y = luaL_checkinteger(L,7);
    int dest_width = luaL_optinteger(L,8,-1);
    int dest_height = luaL_optinteger(L,9,-1);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    QImage image = imageFetch(bundle,path);

    if (dest_width<0) dest_width = source_width;
    if (dest_height<0) dest_height = source_height;
    QRect target(dest_x,dest_height,dest_width,dest_height);
    QRect source(source_x,source_y,source_width,source_height);
    painter.drawImage(target,image,source);
    bundle->luaCanvas->updated=true;
    return 0;
}
AddGuiFunction(Lua_DrawImageRegion,drawImageRegion);

//drawImage(string path, int x, int y, [int width=-1],[int height=-1])
int Lua_DrawImage(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    const char* path = luaL_checklstring(L,1,NULL);
    int x = luaL_checkinteger(L,2);
    int y = luaL_checkinteger(L,3);
    int width = luaL_optinteger(L,4,-1);
    int height = luaL_optinteger(L,5,-1);

    QPainter painter(bundle->luaCanvas->imageBuffer);
    QImage image = imageFetch(bundle,path);

    if (width<0) width = image.width();
    if (height<0) height = image.height();
    painter.drawImage(QRect(x,y,width,height),image);
    bundle->luaCanvas->updated=true;
    return 0;
}
AddGuiFunction(Lua_DrawImage,drawImage);

int Lua_ClearImageHash(lua_State* L)
{
    LuaBundle* bundle = get_bundle(L);
    bundle->imageHash->clear();
    return 0;
}
AddGuiFunction(Lua_ClearImageHash,clearImageCache);

}