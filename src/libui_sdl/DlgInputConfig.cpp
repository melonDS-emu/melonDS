/*
    Copyright 2016-2019 StapleButter

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>
#include "libui/ui.h"

#include "../types.h"
#include "../Config.h"

#include "DlgInputConfig.h"


extern SDL_Joystick* Joystick;


namespace DlgInputConfig
{

uiWindow* win;

uiAreaHandler areahandler;
uiArea* keypresscatcher;

int keyorder[12] = {0, 1, 10, 11, 5, 4, 6, 7, 9, 8, 3, 2};
char keylabels[12][8] = {"A:", "B:", "Select:", "Start:", "Right:", "Left:", "Up:", "Down:", "R:", "L:", "X:", "Y:"};

int keymap[12];
int joymap[12];

int pollid;
uiButton* pollbtn;


void JoyMappingName(int id, char* str)
{
    if (id < 0)
    {
        strcpy(str, "None");
        return;
    }

    if (id & 0x100)
    {
        switch (id & 0xF)
        {
        case 0x1: strcpy(str, "Up"); break;
        case 0x2: strcpy(str, "Right"); break;
        case 0x4: strcpy(str, "Down"); break;
        case 0x8: strcpy(str, "Left"); break;
        }
    }
    else
    {
        sprintf(str, "Button %d", id+1);
    }
}


void OnAreaDraw(uiAreaHandler* handler, uiArea* area, uiAreaDrawParams* params)
{
}

void OnAreaMouseEvent(uiAreaHandler* handler, uiArea* area, uiAreaMouseEvent* evt)
{
}

void OnAreaMouseCrossed(uiAreaHandler* handler, uiArea* area, int left)
{
}

void OnAreaDragBroken(uiAreaHandler* handler, uiArea* area)
{
}

void OnAreaResize(uiAreaHandler* handler, uiArea* area, int width, int height)
{
}

int OnAreaKeyEvent(uiAreaHandler* handler, uiArea* area, uiAreaKeyEvent* evt)
{
    if (pollid < 0)
        return 0;

    if (evt->Scancode == 0x38) // ALT
        return 0;
    if (evt->Modifiers == 0x2) // ALT+key
        return 0;

    if (pollid > 12)
    {
        if (pollid < 0x100) return 0;
        int id = pollid & 0xFF;
        if (id > 12) return 0;
        if (evt->Scancode != 0x1) // ESC
        {
            if (evt->Scancode == 0xE) // backspace
                joymap[id] = -1;
            else
                return 1;
        }

        char keyname[16];
        JoyMappingName(joymap[id], keyname);
        uiButtonSetText(pollbtn, keyname);
        uiControlEnable(uiControl(pollbtn));

        pollid = -1;

        uiControlSetFocus(uiControl(pollbtn));

        return 1;
    }

    if (!evt->Up)
    {
        // set key.
        if (evt->Scancode != 0x1) // ESC
            keymap[pollid] = evt->Scancode;

        char* keyname = uiKeyName(keymap[pollid]);
        uiButtonSetText(pollbtn, keyname);
        uiControlEnable(uiControl(pollbtn));
        uiFreeText(keyname);

        pollid = -1;

        uiControlSetFocus(uiControl(pollbtn));
    }

    return 1;
}

Uint32 JoyPoll(Uint32 interval, void* param)
{
    if (pollid < 0x100) return 0;
    int id = pollid & 0xFF;
    if (id > 12) return 0;

    SDL_JoystickUpdate();

    SDL_Joystick* joy = Joystick;
    if (!joy) return 0;

    int nbuttons = SDL_JoystickNumButtons(joy);
    for (int i = 0; i < nbuttons; i++)
    {
        if (SDL_JoystickGetButton(joy, i))
        {
            joymap[id] = i;

            char keyname[16];
            JoyMappingName(joymap[id], keyname);
            uiButtonSetText(pollbtn, keyname);
            uiControlEnable(uiControl(pollbtn));

            pollid = -1;

            uiControlSetFocus(uiControl(pollbtn));
            return 0;
        }
    }

    u8 blackhat = SDL_JoystickGetHat(joy, 0);
    if (blackhat)
    {
        if      (blackhat & 0x1) blackhat = 0x1;
        else if (blackhat & 0x2) blackhat = 0x2;
        else if (blackhat & 0x4) blackhat = 0x4;
        else                     blackhat = 0x8;

        joymap[id] = 0x100 | blackhat;

        char keyname[16];
        JoyMappingName(joymap[id], keyname);
        uiButtonSetText(pollbtn, keyname);
        uiControlEnable(uiControl(pollbtn));

        pollid = -1;

        uiControlSetFocus(uiControl(pollbtn));
        return 0;
    }

    return 100;
}


void OnKeyStartConfig(uiButton* btn, void* data)
{
    if (pollid != -1)
    {
        // TODO: handle this better?
        if (pollid <= 12)
            uiControlSetFocus(uiControl(keypresscatcher));
        return;
    }

    int id = *(int*)data;
    pollid = id;
    pollbtn = btn;

    uiButtonSetText(btn, "[press key]");
    uiControlDisable(uiControl(btn));

    uiControlSetFocus(uiControl(keypresscatcher));
}

void OnJoyStartConfig(uiButton* btn, void* data)
{
    if (pollid != -1)
    {
        // TODO: handle this better?
        if (pollid <= 12)
            uiControlSetFocus(uiControl(keypresscatcher));
        return;
    }

    int id = *(int*)data;
    pollid = id | 0x100;
    pollbtn = btn;

    uiButtonSetText(btn, "[press button]");
    uiControlDisable(uiControl(btn));

    SDL_AddTimer(100, JoyPoll, NULL);
    uiControlSetFocus(uiControl(keypresscatcher));
}


int OnCloseWindow(uiWindow* window, void* blarg)
{
    return 1;
}

void OnGetFocus(uiWindow* window, void* blarg)
{
    if (pollid >= 0)
        uiControlSetFocus(uiControl(keypresscatcher));
}

void OnLoseFocus(uiWindow* window, void* blarg)
{
}

void OnCancel(uiButton* btn, void* blarg)
{
    uiControlDestroy(uiControl(win));
}

void OnOk(uiButton* btn, void* blarg)
{
    memcpy(Config::KeyMapping, keymap, sizeof(int)*12);
    memcpy(Config::JoyMapping, joymap, sizeof(int)*12);

    Config::Save();

    uiControlDestroy(uiControl(win));
}

void Open()
{
    pollid = -1;

    memcpy(keymap, Config::KeyMapping, sizeof(int)*12);
    memcpy(joymap, Config::JoyMapping, sizeof(int)*12);

    win = uiNewWindow("Input config - melonDS", 600, 400, 0, 0);
    uiWindowSetMargined(win, 1);
    uiWindowOnClosing(win, OnCloseWindow, NULL);
    uiWindowOnGetFocus(win, OnGetFocus, NULL);
    uiWindowOnLoseFocus(win, OnLoseFocus, NULL);

    areahandler.Draw = OnAreaDraw;
    areahandler.MouseEvent = OnAreaMouseEvent;
    areahandler.MouseCrossed = OnAreaMouseCrossed;
    areahandler.DragBroken = OnAreaDragBroken;
    areahandler.KeyEvent = OnAreaKeyEvent;
    areahandler.Resize = OnAreaResize;

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(win, uiControl(top));
    uiControlHide(uiControl(top));

    {
        uiBox* in_ctrl = uiNewHorizontalBox();
        uiBoxAppend(top, uiControl(in_ctrl), 0);
        uiBoxSetPadded(in_ctrl, 1);

        uiGroup* g_key = uiNewGroup("Keyboard");
        uiBoxAppend(in_ctrl, uiControl(g_key), 1);
        uiGrid* b_key = uiNewGrid();
        uiGroupSetChild(g_key, uiControl(b_key));

        const int width = 120;

        for (int i = 0; i < 12; i++)
        {
            int j = keyorder[i];

            uiLabel* label = uiNewLabel(keylabels[j]);
            uiGridAppend(b_key, uiControl(label), 0, i, 1, 1, 1, uiAlignStart, 1, uiAlignCenter);
            uiControlSetMinSize(uiControl(label), width, 1);

            char* keyname = uiKeyName(Config::KeyMapping[j]);

            uiButton* btn = uiNewButton(keyname);
            uiGridAppend(b_key, uiControl(btn), 1, i, 1, 1, 1, uiAlignFill, 1, uiAlignCenter);
            uiButtonOnClicked(btn, OnKeyStartConfig, &keyorder[i]);
            uiControlSetMinSize(uiControl(btn), width, 1);

            uiFreeText(keyname);
        }

        uiGroup* g_joy = uiNewGroup("Joystick");
        uiBoxAppend(in_ctrl, uiControl(g_joy), 1);
        uiGrid* b_joy = uiNewGrid();
        uiGroupSetChild(g_joy, uiControl(b_joy));

        for (int i = 0; i < 12; i++)
        {
            int j = keyorder[i];

            uiLabel* label = uiNewLabel(keylabels[j]);
            uiGridAppend(b_joy, uiControl(label), 0, i, 1, 1, 1, uiAlignStart, 1, uiAlignCenter);
            uiControlSetMinSize(uiControl(label), width, 1);

            char keyname[16];
            JoyMappingName(Config::JoyMapping[j], keyname);

            uiButton* btn = uiNewButton(keyname);
            uiGridAppend(b_joy, uiControl(btn), 1, i, 1, 1, 1, uiAlignFill, 1, uiAlignCenter);
            uiButtonOnClicked(btn, OnJoyStartConfig, &keyorder[i]);
            uiControlSetMinSize(uiControl(btn), width, 1);
        }
    }

    uiLabel* filler = uiNewLabel("");
    uiBoxAppend(top, uiControl(filler), 1);

    {
        uiBox* in_ctrl = uiNewHorizontalBox();
        uiBoxSetPadded(in_ctrl, 1);
        uiBoxAppend(top, uiControl(in_ctrl), 0);

        uiLabel* dummy = uiNewLabel("");
        uiBoxAppend(in_ctrl, uiControl(dummy), 1);

        keypresscatcher = uiNewArea(&areahandler);
        uiBoxAppend(in_ctrl, uiControl(keypresscatcher), 0);

        uiButton* btncancel = uiNewButton("Cancel");
        uiButtonOnClicked(btncancel, OnCancel, NULL);
        uiBoxAppend(in_ctrl, uiControl(btncancel), 0);

        uiButton* btnok = uiNewButton("Ok");
        uiButtonOnClicked(btnok, OnOk, NULL);
        uiBoxAppend(in_ctrl, uiControl(btnok), 0);
    }

    uiControlShow(uiControl(top));

    uiControlShow(uiControl(win));
}

}

