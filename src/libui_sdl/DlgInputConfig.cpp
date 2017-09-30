/*
    Copyright 2016-2017 StapleButter

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

#include "libui/ui.h"

#include "../types.h"
#include "../Config.h"

#include "DlgInputConfig.h"


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

int OnAreaKeyEvent(uiAreaHandler* handler, uiArea* area, uiAreaKeyEvent* evt)
{
    if (pollid < 0 || pollid > 12)
        return 0;

    if (evt->Scancode == 0x38) // ALT
        return 0;
    if (evt->Modifiers == 0x2) // ALT+key
        return 0;

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


void OnKeyStartConfig(uiButton* btn, void* data)
{
    if (pollid != -1)
    {
        // TODO: handle this better?
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


int OnCloseWindow(uiWindow* window, void* blarg)
{
    return 1;
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

    win = uiNewWindow("Input config - melonDS", 600, 400, 0);
    uiWindowSetMargined(win, 1);
    uiWindowOnClosing(win, OnCloseWindow, NULL);

    areahandler.Draw = OnAreaDraw;
    areahandler.MouseEvent = OnAreaMouseEvent;
    areahandler.MouseCrossed = OnAreaMouseCrossed;
    areahandler.DragBroken = OnAreaDragBroken;
    areahandler.KeyEvent = OnAreaKeyEvent;

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(win, uiControl(top));
    uiControlHide(uiControl(top));

    {
        uiBox* in_ctrl = uiNewHorizontalBox();
        uiBoxAppend(top, uiControl(in_ctrl), 0);


        uiGroup* g_key = uiNewGroup("Keyboard");
        uiBoxAppend(in_ctrl, uiControl(g_key), 1);
        uiBox* b_key = uiNewVerticalBox();
        uiGroupSetChild(g_key, uiControl(b_key));

        for (int i = 0; i < 12; i++)
        {
            int j = keyorder[i];

            uiBox* box = uiNewHorizontalBox();
            uiBoxAppend(b_key, uiControl(box), 0);

            uiLabel* label = uiNewLabel(keylabels[j]);
            uiBoxAppend(box, uiControl(label), 1);

            char* keyname = uiKeyName(Config::KeyMapping[j]);

            uiButton* btn = uiNewButton(keyname);
            uiBoxAppend(box, uiControl(btn), 1);
            uiButtonOnClicked(btn, OnKeyStartConfig, &keyorder[i]);

            uiFreeText(keyname);
        }

        uiGroup* g_joy = uiNewGroup("Joystick");
        uiBoxAppend(in_ctrl, uiControl(g_joy), 1);
        uiBox* b_joy = uiNewVerticalBox();
        uiGroupSetChild(g_joy, uiControl(b_joy));

        for (int i = 0; i < 12; i++)
        {
            int j = keyorder[i];

            uiBox* box = uiNewHorizontalBox();
            uiBoxAppend(b_joy, uiControl(box), 0);

            uiLabel* label = uiNewLabel(keylabels[j]);
            uiBoxAppend(box, uiControl(label), 1);

            char keyname[16];
            JoyMappingName(Config::JoyMapping[j], keyname);

            uiButton* btn = uiNewButton(keyname);
            uiBoxAppend(box, uiControl(btn), 1);
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

