/*
    Copyright 2016-2020 Arisotura

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

#include "DlgFirmwareDumps.h"



namespace DlgFirmwareDumps
{

bool opened;
uiWindow* win;

uiEntry* txBios7Path;
uiEntry* txBios9Path;
uiEntry* txFirmwarePath;

char oldBios7[sizeof(Config::Bios7Path)];
char oldBios9[sizeof(Config::Bios9Path)];
char oldFirmware[sizeof(Config::FirmwarePath)];

void RevertSettings()
{
    strncpy(Config::Bios7Path, oldBios7, 511);
    strncpy(Config::Bios9Path, oldBios9, 511);
    strncpy(Config::FirmwarePath, oldFirmware, 511);
}


int OnCloseWindow(uiWindow* window, void* blarg)
{
    RevertSettings();
    opened = false;
    return 1;
}

void BrowsePath(uiEntry* path, uiButton* btn, void* blarg)
{
    const char* allowed = 
        "Any allowed file|*.bin;*.BIN;*.rom;*.ROM|"
        "BIN file (*.bin)|*.bin;*.BIN|"
        "ROM file (*.rom)|.*rom;.*ROM|"
        "Any file|*.*";
    char* file = uiOpenFile(win, allowed, NULL);
    if (!file)
    {
        return;
    }

    uiEntrySetText(path, file);
    uiFreeText(file);
}

void OnBrowseBios7(uiButton *btn, void* blarg) { BrowsePath(txBios7Path, btn, blarg); }
void OnBrowseBios9(uiButton *btn, void* blarg) { BrowsePath(txBios9Path, btn, blarg); }
void OnBrowseFirmware(uiButton *btn, void* blarg) { BrowsePath(txFirmwarePath, btn, blarg); }

void OnCancel(uiButton* btn, void* blarg)
{
    RevertSettings();

    uiControlDestroy(uiControl(win));
    opened = false;
}
void OnRestore(uiButton* btn, void* blarg)
{
    uiEntrySetText(txBios7Path, "bios7.bin");
    uiEntrySetText(txBios9Path, "bios9.bin");
    uiEntrySetText(txFirmwarePath, "firmware.bin");
    opened = false;
}
void OnOk(uiButton* btn, void* blarg)
{
    char* bios7 = uiEntryText(txBios7Path);
    strncpy(Config::Bios7Path, bios7, 511);
    
    char* bios9 = uiEntryText(txBios9Path);
    strncpy(Config::Bios9Path, bios9, 511);
    
    char* firmware = uiEntryText(txFirmwarePath);
    strncpy(Config::FirmwarePath, firmware, 511);
    
    uiFreeText(bios7);
    uiFreeText(bios9);
    uiFreeText(firmware);

    Config::Save();

    uiControlDestroy(uiControl(win));
    opened = false;
}

void createEntry(int idx, const char* labelTxt, uiGrid* grid, uiEntry** entry, void (*callback)(uiButton *b, void *data)) {
    uiLabel* label = uiNewLabel(labelTxt);
    uiGridAppend(grid, uiControl(label), 0, idx, 1, 1, 1, uiAlignStart, 1, uiAlignCenter);
    
    uiBox* box = uiNewHorizontalBox();
    *entry = uiNewEntry();
    uiControlSetMinSize(uiControl(*entry), 300, 0);
    uiBoxAppend(box, uiControl(*entry), 1);
    uiButton* browse = uiNewButton("...");
    uiButtonOnClicked(browse, callback, NULL);
    uiBoxAppend(box, uiControl(browse), 0);
    
    uiGridAppend(grid, uiControl(box), 1, idx, 1, 1, 1, uiAlignFill, 1, uiAlignCenter);
}

void Open()
{
    if (opened)
    {
        uiControlSetFocus(uiControl(win));
        return;
    }

    opened = true;
    win = uiNewWindow("Firmware dumps - melonDS", 800, 100, 0, 0, 0);
    uiWindowSetMargined(win, 1);
    uiWindowOnClosing(win, OnCloseWindow, NULL);

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(win, uiControl(top));
    uiBoxSetPadded(top, 1);
    
    {
        uiBox* in_ctrl = uiNewVerticalBox();
        uiBoxAppend(top, uiControl(in_ctrl), 0);
        uiBoxSetPadded(in_ctrl, 1);

        uiGroup* ds = uiNewGroup("DS/DS Lite");
        uiBoxAppend(in_ctrl, uiControl(ds), 1);

        uiGrid* ds_grid = uiNewGrid();
        uiGroupSetChild(ds, uiControl(ds_grid));
        createEntry(0, "ARM7 BIOS ROM (bios7.bin)", ds_grid, &txBios7Path, OnBrowseBios7);
        createEntry(1, "ARM9 BIOS ROM (bios9.bin)", ds_grid, &txBios9Path, OnBrowseBios9);
        createEntry(2, "SPI Flash (firmware.bin from a DS but not from a DSi)", ds_grid, &txFirmwarePath, OnBrowseFirmware);


        char helptext[512];
        sprintf(helptext,
        "NOTE: you can insert the file name without a path; the file will be searched in the current directory and in the config path.");
        uiLabel* help_label = uiNewLabel(helptext);
        uiBoxAppend(in_ctrl, uiControl(help_label), 0);
    }

    {
        uiBox* in_ctrl = uiNewHorizontalBox();
        uiBoxSetPadded(in_ctrl, 1);
        uiBoxAppend(top, uiControl(in_ctrl), 0);

        uiLabel* dummy = uiNewLabel("");
        uiBoxAppend(in_ctrl, uiControl(dummy), 1);


        uiButton* btnrestore = uiNewButton("Restore defaults");
        uiButtonOnClicked(btnrestore, OnRestore, NULL);
        uiBoxAppend(in_ctrl, uiControl(btnrestore), 0);

        uiButton* btncancel = uiNewButton("Cancel");
        uiButtonOnClicked(btncancel, OnCancel, NULL);
        uiBoxAppend(in_ctrl, uiControl(btncancel), 0);

        uiButton* btnok = uiNewButton("Ok");
        uiButtonOnClicked(btnok, OnOk, NULL);
        uiBoxAppend(in_ctrl, uiControl(btnok), 0);
    }

    strncpy(oldBios7, Config::Bios7Path, 511);
    strncpy(oldBios9, Config::Bios9Path, 511);
    strncpy(oldFirmware, Config::FirmwarePath, 511);

    uiEntrySetText(txBios7Path, Config::Bios7Path);
    uiEntrySetText(txBios9Path, Config::Bios9Path);
    uiEntrySetText(txFirmwarePath, Config::FirmwarePath);

    uiControlShow(uiControl(win));
}

void Close()
{
    if (!opened) return;
    uiControlDestroy(uiControl(win));
    opened = false;
}

}

