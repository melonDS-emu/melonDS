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

#include "libui/ui.h"

#include "../types.h"
#include "PlatformConfig.h"

#include "DlgPathSettings.h"


void ApplyNewSettings(int type);


namespace DlgPathSettings
{

bool opened;
uiWindow* win;

uiEntry* txSavePath;
uiCheckbox* useSavePath;

int OnCloseWindow(uiWindow* window, void* blarg)
{
    opened = false;
    return 1;
}

void OnCancel(uiButton* btn, void* blarg)
{
    uiControlDestroy(uiControl(win));
    opened = false;
}

void OnOk(uiButton* btn, void* blarg)
{
    Config::UseSavePath = uiCheckboxChecked(useSavePath);
    char* savpath = uiEntryText(txSavePath);
    strncpy(Config::SavePath, savpath, 511);
    uiFreeText(savpath);

    Config::Save();

    uiControlDestroy(uiControl(win));
    opened = false;

    ApplyNewSettings(0);
}

void Open()
{
    if (opened)
    {
        uiControlSetFocus(uiControl(win));
        return;
    }

    opened = true;
    win = uiNewWindow("Path settings - melonDS", 300, 200, 0, 0, 0);
    uiWindowSetMargined(win, 1);
    uiWindowOnClosing(win, OnCloseWindow, NULL);

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(win, uiControl(top));

    {
        uiBox* in_ctrl = uiNewVerticalBox();
        uiBoxAppend(top, uiControl(in_ctrl), 1);

        useSavePath = uiNewCheckbox("Use Save Path");
        uiBoxAppend(in_ctrl, uiControl(useSavePath), 0);

        uiLabel* label_vol = uiNewLabel("Save Path:");
        uiBoxAppend(in_ctrl, uiControl(label_vol), 0);

        uiBox* path_box = uiNewHorizontalBox();
        uiBoxAppend(in_ctrl, uiControl(path_box), 0);

        txSavePath = uiNewEntry();
        uiBoxAppend(path_box, uiControl(txSavePath), 1);
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

    uiCheckboxSetChecked(useSavePath, Config::UseSavePath);
    uiEntrySetText(txSavePath, Config::SavePath);

    uiControlShow(uiControl(win));
}

}
