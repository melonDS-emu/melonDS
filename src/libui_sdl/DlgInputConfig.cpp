/*
    Copyright 2016-2019 Arisotura

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
#include "PlatformConfig.h"

#include "DlgInputConfig.h"


extern int JoystickID;
extern SDL_Joystick* Joystick;

extern void OpenJoystick();


namespace DlgInputConfig
{

typedef struct
{
    int type;
    uiWindow* win;

    uiAreaHandler areahandler;
    uiArea* keypresscatcher;

    int numkeys;
    int keymap[32];
    int joymap[32];

    int pollid;
    uiButton* pollbtn;
    SDL_TimerID timer;

    int axes_rest[16];

} InputDlgData;


int dskeyorder[12] = {0, 1, 10, 11, 5, 4, 6, 7, 9, 8, 3, 2};
char dskeylabels[12][8] = {"A:", "B:", "Select:", "Start:", "Right:", "Left:", "Up:", "Down:", "R:", "L:", "X:", "Y:"};

int identity[32] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

char hotkeylabels[HK_MAX][32] =
{
    "Close/open lid:",
    "Microphone:",
    "Pause/resume:",
    "Reset:",
    "Fast forward:",
    "Fast forward (toggle):"
};

int openedmask;
InputDlgData inputdlg[2];


void KeyMappingName(int id, char* str)
{
    if (id < 0)
    {
        strcpy(str, "None");
        return;
    }

    int key = id & 0xFFFF;
    char* keyname = uiKeyName(key);
    strncpy(str, keyname, 63); str[63] = '\0';
    uiFreeText(keyname);

    int mod = id >> 16;

    if      (key == 0x11D) mod = 0;
    else if (key == 0x138) mod = 0;
    else if (key == 0x036) mod = 0;

    if (mod != 0)
    {
        // CTRL / ALT / SHIFT
        const int modscan[] = {0x1D, 0x38, 0x2A};
        char tmp[64];

        for (int m = 2; m >= 0; m--)
        {
            if (!(mod & (1<<m))) continue;

            char* modname = uiKeyName(modscan[m]);
            memcpy(tmp, str, 64);
            snprintf(str, 64, "%s+%s", modname, tmp);
            uiFreeText(modname);
        }
    }

    str[63] = '\0';
}

void JoyMappingName(int id, char* str)
{
    if (id < 0)
    {
        strcpy(str, "None");
        return;
    }

    if (id & 0x100)
    {
        int hatnum = ((id >> 4) & 0xF) + 1;

        switch (id & 0xF)
        {
        case 0x1: sprintf(str, "Hat %d up", hatnum); break;
        case 0x2: sprintf(str, "Hat %d right", hatnum); break;
        case 0x4: sprintf(str, "Hat %d down", hatnum); break;
        case 0x8: sprintf(str, "Hat %d left", hatnum); break;
        }
    }
    else
    {
        sprintf(str, "Button %d", (id & 0xFFFF) + 1);
    }

    if (id & 0x10000)
    {
        int axisnum = ((id >> 24) & 0xF) + 1;

        char tmp[64];
        memcpy(tmp, str, 64);

        switch ((id >> 20) & 0xF)
        {
        case 0: sprintf(str, "%s / Axis %d +", tmp, axisnum); break;
        case 1: sprintf(str, "%s / Axis %d -", tmp, axisnum); break;
        case 2: sprintf(str, "%s / Trigger %d", tmp, axisnum); break;
        }
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
    InputDlgData* dlg = (InputDlgData*)uiControl(area)->UserData;

    if (dlg->pollid < 0)
        return 0;

    if (evt->Scancode == 0x1D) // CTRL
        return 1;
    if (evt->Scancode == 0x38) // ALT
        return 1;
    if (evt->Scancode == 0x2A) // SHIFT
        return 1;

    if (dlg->pollid > 12)
    {
        if (dlg->pollid < 0x100) return 0;
        int id = dlg->pollid & 0xFF;
        if (id > 12) return 0;
        if (evt->Scancode != 0x1 || evt->Modifiers != 0) // ESC
        {
            if (evt->Scancode == 0xE && evt->Modifiers == 0) // backspace
                dlg->joymap[id] = -1;
            else
                return 1;
        }

        char keyname[64];
        JoyMappingName(dlg->joymap[id], keyname);
        uiButtonSetText(dlg->pollbtn, keyname);
        uiControlEnable(uiControl(dlg->pollbtn));

        dlg->pollid = -1;

        uiControlSetFocus(uiControl(dlg->pollbtn));

        return 1;
    }

    if (!evt->Up)
    {
        // set key.
        if (evt->Scancode != 0x1 || evt->Modifiers != 0) // ESC
        {
            if (evt->Scancode == 0xE && evt->Modifiers == 0) // backspace
                dlg->keymap[dlg->pollid] = -1;
            else
                dlg->keymap[dlg->pollid] = evt->Scancode | (evt->Modifiers << 16);
        }

        char keyname[64];
        KeyMappingName(dlg->keymap[dlg->pollid], keyname);
        uiButtonSetText(dlg->pollbtn, keyname);
        uiControlEnable(uiControl(dlg->pollbtn));

        dlg->pollid = -1;

        uiControlSetFocus(uiControl(dlg->pollbtn));
    }

    return 1;
}

void FinishJoyMapping(void* param)
{
    InputDlgData* dlg = (InputDlgData*)param;
    int id = dlg->pollid & 0xFF;

    char keyname[64];
    JoyMappingName(dlg->joymap[id], keyname);
    uiButtonSetText(dlg->pollbtn, keyname);
    uiControlEnable(uiControl(dlg->pollbtn));

    dlg->pollid = -1;

    uiControlSetFocus(uiControl(dlg->pollbtn));
}

Uint32 JoyPoll(Uint32 interval, void* param)
{
    InputDlgData* dlg = (InputDlgData*)param;

    if (dlg->pollid < 0x100)
    {
        dlg->timer = 0;
        return 0;
    }

    int id = dlg->pollid & 0xFF;
    if (id > 12)
    {
        dlg->timer = 0;
        return 0;
    }

    SDL_Joystick* joy = Joystick;
    if (!joy)
    {
        dlg->timer = 0;
        return 0;
    }
    if (!SDL_JoystickGetAttached(joy))
    {
        dlg->timer = 0;
        return 0;
    }

    int oldmap;
    if (dlg->joymap[id] == -1) oldmap = 0;
    else                       oldmap = dlg->joymap[id];

    int nbuttons = SDL_JoystickNumButtons(joy);
    for (int i = 0; i < nbuttons; i++)
    {
        if (SDL_JoystickGetButton(joy, i))
        {
            dlg->joymap[id] = (oldmap & 0xFFFF0000) | i;
            uiQueueMain(FinishJoyMapping, dlg);
            dlg->timer = 0;
            return 0;
        }
    }

    int nhats = SDL_JoystickNumHats(joy);
    if (nhats > 16) nhats = 16;
    for (int i = 0; i < nhats; i++)
    {
        Uint8 blackhat = SDL_JoystickGetHat(joy, i);
        if (blackhat)
        {
            if      (blackhat & 0x1) blackhat = 0x1;
            else if (blackhat & 0x2) blackhat = 0x2;
            else if (blackhat & 0x4) blackhat = 0x4;
            else                     blackhat = 0x8;

            dlg->joymap[id] = (oldmap & 0xFFFF0000) | 0x100 | blackhat | (i << 4);
            uiQueueMain(FinishJoyMapping, dlg);
            dlg->timer = 0;
            return 0;
        }
    }

    int naxes = SDL_JoystickNumAxes(joy);
    if (naxes > 16) naxes = 16;
    for (int i = 0; i < naxes; i++)
    {
        Sint16 axisval = SDL_JoystickGetAxis(joy, i);
        int diff = abs(axisval - dlg->axes_rest[i]);

        if (dlg->axes_rest[i] < -16384 && axisval >= 0)
        {
            dlg->joymap[id] = (oldmap & 0xFFFF) | 0x10000 | (2 << 20) | (i << 24);
            uiQueueMain(FinishJoyMapping, dlg);
            dlg->timer = 0;
            return 0;
        }
        else if (diff > 16384)
        {
            int axistype;
            if (axisval > 0) axistype = 0;
            else             axistype = 1;

            dlg->joymap[id] = (oldmap & 0xFFFF) | 0x10000 | (axistype << 20) | (i << 24);
            uiQueueMain(FinishJoyMapping, dlg);
            dlg->timer = 0;
            return 0;
        }
    }

    return interval;
}


void OnKeyStartConfig(uiButton* btn, void* data)
{
    InputDlgData* dlg = (InputDlgData*)uiControl(btn)->UserData;

    if (dlg->pollid != -1)
    {
        // TODO: handle this better?
        if (dlg->pollid <= 12)
            uiControlSetFocus(uiControl(dlg->keypresscatcher));
        return;
    }

    int id = *(int*)data;
    dlg->pollid = id;
    dlg->pollbtn = btn;

    uiButtonSetText(btn, "[press key]");
    uiControlDisable(uiControl(btn));

    uiControlSetFocus(uiControl(dlg->keypresscatcher));
}

void OnJoyStartConfig(uiButton* btn, void* data)
{
    InputDlgData* dlg = (InputDlgData*)uiControl(btn)->UserData;

    if (dlg->pollid != -1)
    {
        // TODO: handle this better?
        if (dlg->pollid <= 12)
            uiControlSetFocus(uiControl(dlg->keypresscatcher));
        return;
    }

    int naxes = SDL_JoystickNumAxes(Joystick);
    if (naxes > 16) naxes = 16;
    for (int a = 0; a < naxes; a++)
    {
        dlg->axes_rest[a] = SDL_JoystickGetAxis(Joystick, a);
    }

    int id = *(int*)data;
    dlg->pollid = id | 0x100;
    dlg->pollbtn = btn;

    uiButtonSetText(btn, "[press button / axis]");
    uiControlDisable(uiControl(btn));

    dlg->timer = SDL_AddTimer(100, JoyPoll, dlg);
    uiControlSetFocus(uiControl(dlg->keypresscatcher));
}


void OnJoystickChanged(uiCombobox* cb, void* data)
{
    JoystickID = uiComboboxSelected(cb);
    OpenJoystick();
}


int OnCloseWindow(uiWindow* window, void* blarg)
{
    InputDlgData* dlg = (InputDlgData*)(uiControl(window)->UserData);
    openedmask &= ~(1 << dlg->type);
    if (dlg->timer) SDL_RemoveTimer(dlg->timer);

    JoystickID = Config::JoystickID;
    OpenJoystick();

    return 1;
}

void OnGetFocus(uiWindow* window, void* blarg)
{
    InputDlgData* dlg = (InputDlgData*)(uiControl(window)->UserData);

    if (dlg->pollid >= 0)
        uiControlSetFocus(uiControl(dlg->keypresscatcher));
}

void OnLoseFocus(uiWindow* window, void* blarg)
{
}

void OnCancel(uiButton* btn, void* data)
{
    InputDlgData* dlg = (InputDlgData*)data;

    uiControlDestroy(uiControl(dlg->win));
    openedmask &= ~(1 << dlg->type);
    if (dlg->timer) SDL_RemoveTimer(dlg->timer);

    JoystickID = Config::JoystickID;
    OpenJoystick();
}

void OnOk(uiButton* btn, void* data)
{
    InputDlgData* dlg = (InputDlgData*)data;

    if (dlg->type == 0)
    {
        memcpy(Config::KeyMapping, dlg->keymap, sizeof(int)*12);
        memcpy(Config::JoyMapping, dlg->joymap, sizeof(int)*12);
    }
    else if (dlg->type == 1)
    {
        memcpy(Config::HKKeyMapping, dlg->keymap, sizeof(int)*HK_MAX);
        memcpy(Config::HKJoyMapping, dlg->joymap, sizeof(int)*HK_MAX);
    }

    Config::JoystickID = JoystickID;

    Config::Save();

    uiControlDestroy(uiControl(dlg->win));
    openedmask &= ~(1 << dlg->type);
    if (dlg->timer) SDL_RemoveTimer(dlg->timer);
}

void Open(int type)
{
    InputDlgData* dlg = &inputdlg[type];

    int mask = 1 << type;
    if (openedmask & mask)
    {
        uiControlSetFocus(uiControl(dlg->win));
        return;
    }

    openedmask |= mask;

    dlg->type = type;
    dlg->pollid = -1;
    dlg->timer = 0;

    if (type == 0)
    {
        dlg->numkeys = 12;
        memcpy(dlg->keymap, Config::KeyMapping, sizeof(int)*12);
        memcpy(dlg->joymap, Config::JoyMapping, sizeof(int)*12);

        dlg->win = uiNewWindow("Input config - melonDS", 600, 100, 0, 0, 0);
    }
    else if (type == 1)
    {
        dlg->numkeys = HK_MAX;
        memcpy(dlg->keymap, Config::HKKeyMapping, sizeof(int)*HK_MAX);
        memcpy(dlg->joymap, Config::HKJoyMapping, sizeof(int)*HK_MAX);

        dlg->win = uiNewWindow("Hotkey config - melonDS", 600, 100, 0, 0, 0);
    }

    uiControl(dlg->win)->UserData = dlg;

    uiWindowSetMargined(dlg->win, 1);
    uiWindowOnClosing(dlg->win, OnCloseWindow, NULL);
    uiWindowOnGetFocus(dlg->win, OnGetFocus, NULL);
    uiWindowOnLoseFocus(dlg->win, OnLoseFocus, NULL);

    dlg->areahandler.Draw = OnAreaDraw;
    dlg->areahandler.MouseEvent = OnAreaMouseEvent;
    dlg->areahandler.MouseCrossed = OnAreaMouseCrossed;
    dlg->areahandler.DragBroken = OnAreaDragBroken;
    dlg->areahandler.KeyEvent = OnAreaKeyEvent;
    dlg->areahandler.Resize = OnAreaResize;

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(dlg->win, uiControl(top));
    uiControlHide(uiControl(top));

    {
        uiBox* in_ctrl = uiNewHorizontalBox();
        uiBoxAppend(top, uiControl(in_ctrl), 0);
        uiBoxSetPadded(in_ctrl, 1);

        uiGroup* g_key = uiNewGroup("Keyboard");
        uiBoxAppend(in_ctrl, uiControl(g_key), 1);
        uiGrid* b_key = uiNewGrid();
        uiGroupSetChild(g_key, uiControl(b_key));

        const int width = 240;

        for (int i = 0; i < dlg->numkeys; i++)
        {
            int j = (type==0) ? dskeyorder[i] : i;

            uiLabel* label = uiNewLabel((type==0) ? dskeylabels[j] : hotkeylabels[j]);
            uiGridAppend(b_key, uiControl(label), 0, i, 1, 1, 1, uiAlignStart, 1, uiAlignCenter);
            uiControlSetMinSize(uiControl(label), width, 1);

            char keyname[64];
            KeyMappingName(dlg->keymap[j], keyname);

            uiButton* btn = uiNewButton(keyname);
            uiControl(btn)->UserData = dlg;
            uiGridAppend(b_key, uiControl(btn), 1, i, 1, 1, 1, uiAlignFill, 1, uiAlignCenter);
            uiButtonOnClicked(btn, OnKeyStartConfig, (type==0) ? &dskeyorder[i] : &identity[i]);
            uiControlSetMinSize(uiControl(btn), width, 1);
        }

        uiGroup* g_joy = uiNewGroup("Joystick");
        uiBoxAppend(in_ctrl, uiControl(g_joy), 1);
        uiGrid* b_joy = uiNewGrid();
        uiGroupSetChild(g_joy, uiControl(b_joy));

        for (int i = 0; i < dlg->numkeys; i++)
        {
            int j = (type==0) ? dskeyorder[i] : i;

            uiLabel* label = uiNewLabel((type==0) ? dskeylabels[j] : hotkeylabels[j]);
            uiGridAppend(b_joy, uiControl(label), 0, i, 1, 1, 1, uiAlignStart, 1, uiAlignCenter);
            uiControlSetMinSize(uiControl(label), width, 1);

            char keyname[64];
            JoyMappingName(dlg->joymap[j], keyname);

            uiButton* btn = uiNewButton(keyname);
            uiControl(btn)->UserData = dlg;
            uiGridAppend(b_joy, uiControl(btn), 1, i, 1, 1, 1, uiAlignFill, 1, uiAlignCenter);
            uiButtonOnClicked(btn, OnJoyStartConfig, (type==0) ? &dskeyorder[i] : &identity[i]);
            uiControlSetMinSize(uiControl(btn), width, 1);
        }

        if (type == 0)
        {
            uiLabel* dummy = uiNewLabel(" ");
            uiGridAppend(b_key, uiControl(dummy), 0, dlg->numkeys, 2, 1, 1, uiAlignFill, 1, uiAlignCenter);

            uiCombobox* joycombo = uiNewCombobox();
            uiGridAppend(b_joy, uiControl(joycombo), 0, dlg->numkeys, 2, 1, 1, uiAlignFill, 1, uiAlignCenter);

            int numjoys = SDL_NumJoysticks();
            if (numjoys < 1)
            {
                uiComboboxAppend(joycombo, "(no joysticks available)");
                uiControlDisable(uiControl(joycombo));
            }
            else
            {
                for (int i = 0; i < numjoys; i++)
                {
                    const char* joyname = SDL_JoystickNameForIndex(i);
                    char fullname[256];
                    snprintf(fullname, 256, "%d. %s", i+1, joyname);

                    uiComboboxAppend(joycombo, fullname);
                }

                uiComboboxSetSelected(joycombo, JoystickID);
                uiComboboxOnSelected(joycombo, OnJoystickChanged, NULL);
            }
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

        dlg->keypresscatcher = uiNewArea(&dlg->areahandler);
        uiControl(dlg->keypresscatcher)->UserData = dlg;
        uiBoxAppend(in_ctrl, uiControl(dlg->keypresscatcher), 0);

        uiButton* btncancel = uiNewButton("Cancel");
        uiButtonOnClicked(btncancel, OnCancel, dlg);
        uiBoxAppend(in_ctrl, uiControl(btncancel), 0);

        uiButton* btnok = uiNewButton("Ok");
        uiButtonOnClicked(btnok, OnOk, dlg);
        uiBoxAppend(in_ctrl, uiControl(btnok), 0);
    }

    uiControlShow(uiControl(top));

    uiControlShow(uiControl(dlg->win));
}

void Close(int type)
{
    if (openedmask & (1<<type))
        uiControlDestroy(uiControl(inputdlg[type].win));

    openedmask &= ~(1<<type);
    if (inputdlg[type].timer) SDL_RemoveTimer(inputdlg[type].timer);
}

}

