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

#include "libui/ui.h"

#include "../types.h"
#include "../Config.h"

#include "DlgEmuSettings.h"


void ApplyNewSettings();


namespace DlgEmuSettings
{

uiWindow* win;

uiCheckbox* cbDirectBoot;
uiCheckbox* cbThreaded3D;
uiCheckbox* cbBindAnyAddr;


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
    Config::DirectBoot = uiCheckboxChecked(cbDirectBoot);
    Config::Threaded3D = uiCheckboxChecked(cbThreaded3D);
    Config::SocketBindAnyAddr = uiCheckboxChecked(cbBindAnyAddr);

    Config::Save();

    uiControlDestroy(uiControl(win));

    ApplyNewSettings();
}

void Open()
{
    win = uiNewWindow("Emu settings - melonDS", 300, 200, 0, 0);
    uiWindowSetMargined(win, 1);
    uiWindowOnClosing(win, OnCloseWindow, NULL);

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(win, uiControl(top));

    {
        uiBox* in_ctrl = uiNewVerticalBox();
        uiBoxAppend(top, uiControl(in_ctrl), 1);

        cbDirectBoot = uiNewCheckbox("Boot game directly");
        uiBoxAppend(in_ctrl, uiControl(cbDirectBoot), 0);

        cbThreaded3D = uiNewCheckbox("Threaded 3D renderer");
        uiBoxAppend(in_ctrl, uiControl(cbThreaded3D), 0);

        cbBindAnyAddr = uiNewCheckbox("Wifi: bind socket to any address");
        uiBoxAppend(in_ctrl, uiControl(cbBindAnyAddr), 0);
    }

    {
        uiBox* in_ctrl = uiNewHorizontalBox();
        uiBoxSetPadded(in_ctrl, 1);
        uiBoxAppend(top, uiControl(in_ctrl), 0);

        uiLabel* dummy = uiNewLabel("");
        uiBoxAppend(in_ctrl, uiControl(dummy), 1);

        uiButton* btncancel = uiNewButton("Cancel");
        uiButtonOnClicked(btncancel, OnCancel, NULL);
        uiBoxAppend(in_ctrl, uiControl(btncancel), 0);

        uiButton* btnok = uiNewButton("Ok");
        uiButtonOnClicked(btnok, OnOk, NULL);
        uiBoxAppend(in_ctrl, uiControl(btnok), 0);
    }

    uiCheckboxSetChecked(cbDirectBoot, Config::DirectBoot);
    uiCheckboxSetChecked(cbThreaded3D, Config::Threaded3D);
    uiCheckboxSetChecked(cbBindAnyAddr, Config::SocketBindAnyAddr);

    uiControlShow(uiControl(win));
}

}
