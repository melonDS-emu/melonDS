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

#include "DlgVideoSettings.h"


//


namespace DlgVideoSettings
{

bool opened;
uiWindow* win;

//


int OnCloseWindow(uiWindow* window, void* blarg)
{
    opened = false;
    return 1;
}

//

void OnCancel(uiButton* btn, void* blarg)
{
    // restore old settings here

    uiControlDestroy(uiControl(win));
    opened = false;
}

void OnOk(uiButton* btn, void* blarg)
{
    // set config entries here

    Config::Save();

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
    win = uiNewWindow("Video settings - melonDS", 400, 100, 0, 0, 0);
    uiWindowSetMargined(win, 1);
    uiWindowOnClosing(win, OnCloseWindow, NULL);

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(win, uiControl(top));
    uiBoxSetPadded(top, 1);

    uiBox* splitter = uiNewHorizontalBox();
    uiBoxAppend(top, uiControl(splitter), 0);
    uiBoxSetPadded(splitter, 1);

    uiBox* left = uiNewVerticalBox();
    uiBoxAppend(splitter, uiControl(left), 1);
    uiBoxSetPadded(left, 1);
    uiBox* right = uiNewVerticalBox();
    uiBoxAppend(splitter, uiControl(right), 1);
    uiBoxSetPadded(right, 1);

    {
        uiGroup* grp = uiNewGroup("3D renderer");
        uiBoxAppend(left, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        uiRadioButtons* rbRenderer = uiNewRadioButtons();
        uiRadioButtonsAppend(rbRenderer, "Software");
        uiRadioButtonsAppend(rbRenderer, "OpenGL");
        uiBoxAppend(in_ctrl, uiControl(rbRenderer), 0);
    }

    {
        uiGroup* grp = uiNewGroup("Software renderer");
        uiBoxAppend(left, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        uiCheckbox* cbThreaded3D = uiNewCheckbox("Threaded");
        uiBoxAppend(in_ctrl, uiControl(cbThreaded3D), 0);
    }

    {
        uiGroup* grp = uiNewGroup("OpenGL renderer");
        uiBoxAppend(left, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        uiCheckbox* cbAntialias = uiNewCheckbox("Antialiasing");
        uiBoxAppend(in_ctrl, uiControl(cbAntialias), 0);
    }

    {
        uiGroup* grp = uiNewGroup("Display settings");
        uiBoxAppend(right, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        uiLabel* lbl = uiNewLabel("Resolution:");
        uiBoxAppend(in_ctrl, uiControl(lbl), 0);

        uiRadioButtons* rbResolution = uiNewRadioButtons();
        uiRadioButtonsAppend(rbResolution, "1x");
        uiRadioButtonsAppend(rbResolution, "2x");
        uiRadioButtonsAppend(rbResolution, "4x");
        uiBoxAppend(in_ctrl, uiControl(rbResolution), 0);

        uiCheckbox* cbWidescreen = uiNewCheckbox("Stretch to 16:9");
        uiBoxAppend(in_ctrl, uiControl(cbWidescreen), 0);

        lbl = uiNewLabel("");
        uiBoxAppend(in_ctrl, uiControl(lbl), 0);

        lbl = uiNewLabel("Apply upscaling to:");
        uiBoxAppend(in_ctrl, uiControl(lbl), 0);

        uiRadioButtons* rbApplyScalingTo = uiNewRadioButtons();
        uiRadioButtonsAppend(rbApplyScalingTo, "Both screens");
        uiRadioButtonsAppend(rbApplyScalingTo, "Emphasized screen");
        uiRadioButtonsAppend(rbApplyScalingTo, "Top screen");
        uiRadioButtonsAppend(rbApplyScalingTo, "Bottom screen");
        uiBoxAppend(in_ctrl, uiControl(rbApplyScalingTo), 0);
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

    //

    uiControlShow(uiControl(win));
}

}
