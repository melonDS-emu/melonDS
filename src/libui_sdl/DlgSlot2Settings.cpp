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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libui/ui.h"

#include "../types.h"
#include "PlatformConfig.h"

#include "DlgSlot2Settings.h"


void ApplyNewSettings(int type);


namespace DlgSlot2Settings
{

bool opened;
uiWindow* win;

uiCombobox* cbType;
uiEntry* cbDiskImagePath;

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
    Config::Slot2Type = uiComboboxSelected(cbType);
    strncpy(Config::Slot2DiskImagePath, uiEntryText(cbDiskImagePath), 511);

    Config::Save();

    uiControlDestroy(uiControl(win));
    opened = false;

    ApplyNewSettings(5);
}

void OnOpenDiskImagePath(uiButton* btn, void* blarg)
{
    char *path = uiOpenFile(win, "Any file|*.*", NULL);

    if (path != NULL) {
        uiEntrySetText(cbDiskImagePath, path);
    }
}

void Open()
{
    if (opened)
    {
        uiControlSetFocus(uiControl(win));
        return;
    }

    opened = true;
    win = uiNewWindow("Slot-2 settings - melonDS", 300, 200, 0, 0, 0);
    uiWindowSetMargined(win, 1);
    uiWindowOnClosing(win, OnCloseWindow, NULL);

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(win, uiControl(top));

    {
        uiBox* in_ctrl = uiNewVerticalBox();
        uiBoxAppend(top, uiControl(in_ctrl), 1);

        uiLabel* lbl = uiNewLabel("Inserted cartridge:");
        uiBoxAppend(in_ctrl, uiControl(lbl), 0);

        cbType = uiNewCombobox();
        for (int i = 0; Config::Slot2Types[i] != NULL; i++) {
            uiComboboxAppend(cbType, Config::Slot2Types[i]);
        }
        uiBoxAppend(in_ctrl, uiControl(cbType), 0);
    }

    {
        uiBox* in_ctrl = uiNewVerticalBox();
        uiBoxAppend(top, uiControl(in_ctrl), 1);

        uiLabel* lbl = uiNewLabel("Disk image (CompactFlash/SD):");
        uiBoxAppend(in_ctrl, uiControl(lbl), 0);

        cbDiskImagePath = uiNewEntry();
        uiEntrySetText(cbDiskImagePath, Config::Slot2DiskImagePath);
        uiBoxAppend(in_ctrl, uiControl(cbDiskImagePath), 0);

        uiButton* openBtn = uiNewButton("Open...");
        uiButtonOnClicked(openBtn, OnOpenDiskImagePath, NULL);
        uiBoxAppend(in_ctrl, uiControl(openBtn), 0);
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

    uiComboboxSetSelected(cbType, Config::Slot2Type);

    uiControlShow(uiControl(win));
}

void Close()
{
    if (!opened) return;
    uiControlDestroy(uiControl(win));
    opened = false;
}

}
