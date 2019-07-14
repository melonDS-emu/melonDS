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

#include "libui/ui.h"

#include "../types.h"
#include "PlatformConfig.h"

#include "DlgEmuSettings.h"


void ApplyNewSettings(int type);


namespace DlgEmuSettings
{

bool opened;
uiWindow* win;

uiCheckbox* cbDirectBoot;

uiCheckbox* cbJITEnabled;
uiEntry* enJITMaxBlockSize;

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
    Config::DirectBoot = uiCheckboxChecked(cbDirectBoot);

    Config::JIT_Enable = uiCheckboxChecked(cbJITEnabled);
    long blockSize = strtol(uiEntryText(enJITMaxBlockSize), NULL, 10);
    if (blockSize < 1)
        blockSize = 1;
    if (blockSize > 32)
        blockSize = 32;
    Config::JIT_MaxBlockSize = blockSize;

    Config::Save();

    uiControlDestroy(uiControl(win));
    opened = false;

    ApplyNewSettings(4);
}

void OnJITStateChanged(uiCheckbox* cb, void* blarg)
{
    if (uiCheckboxChecked(cb))
        uiControlEnable(uiControl(enJITMaxBlockSize));
    else
        uiControlDisable(uiControl(enJITMaxBlockSize));
}

void Open()
{
    if (opened)
    {
        uiControlSetFocus(uiControl(win));
        return;
    }

    opened = true;
    win = uiNewWindow("Emu settings - melonDS", 300, 170, 0, 0, 0);
    uiWindowSetMargined(win, 1);
    uiWindowOnClosing(win, OnCloseWindow, NULL);

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(win, uiControl(top));

    {
        uiBox* in_ctrl = uiNewVerticalBox();
        uiBoxAppend(top, uiControl(in_ctrl), 0);

        cbDirectBoot = uiNewCheckbox("Boot game directly");
        uiBoxAppend(in_ctrl, uiControl(cbDirectBoot), 0);
    }

    {
        uiLabel* dummy = uiNewLabel("");
        uiBoxAppend(top, uiControl(dummy), 0);
    }

    {
        uiGroup* grp = uiNewGroup("JIT");
        uiBoxAppend(top, uiControl(grp), 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        cbJITEnabled = uiNewCheckbox("Enable JIT recompiler");
        uiBoxAppend(in_ctrl, uiControl(cbJITEnabled), 0);

        uiCheckboxOnToggled(cbJITEnabled, OnJITStateChanged, NULL);

        {
            uiBox* row = uiNewHorizontalBox();
            uiBoxAppend(in_ctrl, uiControl(row), 0);

            uiLabel* lbl = uiNewLabel("Maximum block size (1-32): ");
            uiBoxAppend(row, uiControl(lbl), 0);

            enJITMaxBlockSize = uiNewEntry();
            uiBoxAppend(row, uiControl(enJITMaxBlockSize), 0);
        }
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

    uiCheckboxSetChecked(cbJITEnabled, Config::JIT_Enable);
    {
        char maxBlockSizeStr[10];
        sprintf(maxBlockSizeStr, "%d", Config::JIT_MaxBlockSize);
        uiEntrySetText(enJITMaxBlockSize, maxBlockSizeStr);
    }
    OnJITStateChanged(cbJITEnabled, NULL);

    uiControlShow(uiControl(win));
}

void Close()
{
    if (!opened) return;
    uiControlDestroy(uiControl(win));
    opened = false;
}

}
