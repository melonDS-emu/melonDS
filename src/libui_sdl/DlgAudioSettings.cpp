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
#include "../Config.h"

#include "DlgAudioSettings.h"


void MicLoadWav(char* path);


namespace DlgAudioSettings
{

bool opened;
uiWindow* win;

uiSlider* slVolume;
uiRadioButtons* rbMicInputType;
uiEntry* txMicWavPath;

int oldvolume;


int OnCloseWindow(uiWindow* window, void* blarg)
{
    opened = false;
    return 1;
}

void OnVolumeChanged(uiSlider* slider, void* blarg)
{
    Config::AudioVolume = uiSliderValue(slVolume);
}

void OnMicWavBrowse(uiButton* btn, void* blarg)
{
    char* file = uiOpenFile(win, "WAV file (*.wav)|*.wav|Any file|*.*", NULL);
    if (!file)
    {
        return;
    }

    uiEntrySetText(txMicWavPath, file);
    uiFreeText(file);
}

void OnCancel(uiButton* btn, void* blarg)
{
    Config::AudioVolume = oldvolume;

    uiControlDestroy(uiControl(win));
    opened = false;
}

void OnOk(uiButton* btn, void* blarg)
{
    Config::AudioVolume = uiSliderValue(slVolume);
    Config::MicInputType = uiRadioButtonsSelected(rbMicInputType);

    char* wavpath = uiEntryText(txMicWavPath);
    strncpy(Config::MicWavPath, wavpath, 511);
    uiFreeText(wavpath);

    Config::Save();

    if (Config::MicInputType == 3) MicLoadWav(Config::MicWavPath);

    uiControlDestroy(uiControl(win));
    opened = false;
}

void Open()
{
    if (opened)
    {
        uiControlSetFocus(uiControl(win));
        return;
    }

    opened = true;
    win = uiNewWindow("Audio settings - melonDS", 400, 100, 0, 0, 0);
    uiWindowSetMargined(win, 1);
    uiWindowOnClosing(win, OnCloseWindow, NULL);

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(win, uiControl(top));
    uiBoxSetPadded(top, 1);

    {
        uiGroup* grp = uiNewGroup("Audio output");
        uiBoxAppend(top, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        uiLabel* label_vol = uiNewLabel("Volume:");
        uiBoxAppend(in_ctrl, uiControl(label_vol), 0);

        slVolume = uiNewSlider(0, 256);
        uiSliderOnChanged(slVolume, OnVolumeChanged, NULL);
        uiBoxAppend(in_ctrl, uiControl(slVolume), 0);
    }

    {
        uiGroup* grp = uiNewGroup("Microphone input");
        uiBoxAppend(top, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        rbMicInputType = uiNewRadioButtons();
        uiRadioButtonsAppend(rbMicInputType, "None");
        uiRadioButtonsAppend(rbMicInputType, "Microphone");
        uiRadioButtonsAppend(rbMicInputType, "White noise");
        uiRadioButtonsAppend(rbMicInputType, "WAV file:");
        uiBoxAppend(in_ctrl, uiControl(rbMicInputType), 0);

        uiBox* path_box = uiNewHorizontalBox();
        uiBoxAppend(in_ctrl, uiControl(path_box), 0);

        txMicWavPath = uiNewEntry();
        uiBoxAppend(path_box, uiControl(txMicWavPath), 1);

        uiButton* path_browse = uiNewButton("...");
        uiButtonOnClicked(path_browse, OnMicWavBrowse, NULL);
        uiBoxAppend(path_box, uiControl(path_browse), 0);
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

    if      (Config::AudioVolume < 0)   Config::AudioVolume = 0;
    else if (Config::AudioVolume > 256) Config::AudioVolume = 256;

    oldvolume = Config::AudioVolume;

    uiSliderSetValue(slVolume, Config::AudioVolume);
    uiRadioButtonsSetSelected(rbMicInputType, Config::MicInputType);
    uiEntrySetText(txMicWavPath, Config::MicWavPath);

    uiControlShow(uiControl(win));
}

}
