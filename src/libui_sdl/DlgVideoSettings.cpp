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


void ApplyNewSettings(int type);


namespace DlgVideoSettings
{

bool opened;
uiWindow* win;

uiRadioButtons* rbRenderer;
uiCheckbox* cbGLDisplay;
uiCheckbox* cbVSync;
uiCheckbox* cbThreaded3D;
uiCombobox* cbResolution;
uiCheckbox* cbAntialias;

int old_renderer;
int old_gldisplay;
int old_vsync;
int old_threaded3D;
int old_resolution;
int old_antialias;


void UpdateControls()
{
    int renderer = uiRadioButtonsSelected(rbRenderer);

    if (renderer == 0)
    {
        uiControlEnable(uiControl(cbGLDisplay));
        uiControlEnable(uiControl(cbThreaded3D));
        uiControlDisable(uiControl(cbResolution));
        //uiControlDisable(uiControl(cbAntialias));
    }
    else
    {
        uiControlDisable(uiControl(cbGLDisplay));
        uiControlDisable(uiControl(cbThreaded3D));
        uiControlEnable(uiControl(cbResolution));
        //uiControlEnable(uiControl(cbAntialias));
    }
}

void RevertSettings()
{
    bool apply0 = false;
    bool apply2 = false;
    bool apply3 = false;

    bool old_usegl = (old_gldisplay != 0) || (old_renderer != 0);
    bool new_usegl = (Config::ScreenUseGL != 0) || (Config::_3DRenderer != 0);

    if (old_renderer != Config::_3DRenderer)
    {
        Config::_3DRenderer = old_renderer;
        apply3 = true;
    }

    if (old_gldisplay != Config::ScreenUseGL)
    {
        Config::ScreenUseGL = old_gldisplay;
    }
    if (old_vsync != Config::ScreenVSync)
    {
        Config::ScreenVSync = old_vsync;
        //ApplyNewSettings(4);
    }
    if (old_usegl != new_usegl)
    {
        apply2 = true;
    }

    if (old_threaded3D != Config::Threaded3D)
    {
        Config::Threaded3D = old_threaded3D;
        apply0 = true;
    }

    if (old_resolution != Config::GL_ScaleFactor ||
        old_antialias != Config::GL_Antialias)
    {
        Config::GL_ScaleFactor = old_resolution;
        Config::GL_Antialias = old_antialias;
        apply0 = true;
    }

    if (apply2) ApplyNewSettings(2);
    else if (apply3) ApplyNewSettings(3);
    if (apply0) ApplyNewSettings(0);
}


int OnCloseWindow(uiWindow* window, void* blarg)
{
    RevertSettings();
    opened = false;
    return 1;
}

void OnRendererChanged(uiRadioButtons* rb, void* blarg)
{
    int id = uiRadioButtonsSelected(rb);
    bool old_usegl = (Config::ScreenUseGL != 0) || (Config::_3DRenderer != 0);

    Config::_3DRenderer = id;
    UpdateControls();

    bool new_usegl = (Config::ScreenUseGL != 0) || (Config::_3DRenderer != 0);

    if (new_usegl) uiControlEnable(uiControl(cbVSync));
    else           uiControlDisable(uiControl(cbVSync));

    if (new_usegl != old_usegl)
        ApplyNewSettings(2);
    else
        ApplyNewSettings(3);

    uiControlSetFocus(uiControl(win));
}

void OnGLDisplayChanged(uiCheckbox* cb, void* blarg)
{
    Config::ScreenUseGL = uiCheckboxChecked(cb);
    if (Config::ScreenUseGL) uiControlEnable(uiControl(cbVSync));
    else                     uiControlDisable(uiControl(cbVSync));
    ApplyNewSettings(2);
    uiControlSetFocus(uiControl(win));
}

void OnVSyncChanged(uiCheckbox* cb, void* blarg)
{
    Config::ScreenVSync = uiCheckboxChecked(cb);
    //ApplyNewSettings(4);
}

void OnThreaded3DChanged(uiCheckbox* cb, void* blarg)
{
    Config::Threaded3D = uiCheckboxChecked(cb);
    ApplyNewSettings(0);
}

void OnResolutionChanged(uiCombobox* cb, void* blarg)
{
    int id = uiComboboxSelected(cb);

    Config::GL_ScaleFactor = id+1;
    ApplyNewSettings(0);
}

void OnAntialiasChanged(uiCheckbox* cb, void* blarg)
{
    Config::GL_Antialias = uiCheckboxChecked(cb);
    ApplyNewSettings(0);
}

void OnCancel(uiButton* btn, void* blarg)
{
    RevertSettings();

    uiControlDestroy(uiControl(win));
    opened = false;
}

void OnOk(uiButton* btn, void* blarg)
{
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
        uiGroup* grp = uiNewGroup("Display settings");
        uiBoxAppend(left, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        uiLabel* lbl = uiNewLabel("3D renderer:");
        uiBoxAppend(in_ctrl, uiControl(lbl), 0);

        rbRenderer = uiNewRadioButtons();
        uiRadioButtonsAppend(rbRenderer, "Software");
        uiRadioButtonsAppend(rbRenderer, "OpenGL");
        uiRadioButtonsOnSelected(rbRenderer, OnRendererChanged, NULL);
        uiBoxAppend(in_ctrl, uiControl(rbRenderer), 0);

        lbl = uiNewLabel("");
        uiBoxAppend(in_ctrl, uiControl(lbl), 0);

        cbGLDisplay = uiNewCheckbox("OpenGL display");
        uiCheckboxOnToggled(cbGLDisplay, OnGLDisplayChanged, NULL);
        uiBoxAppend(in_ctrl, uiControl(cbGLDisplay), 0);

        cbVSync = uiNewCheckbox("VSync");
        uiCheckboxOnToggled(cbVSync, OnVSyncChanged, NULL);
        uiBoxAppend(in_ctrl, uiControl(cbVSync), 0);
    }

    {
        uiGroup* grp = uiNewGroup("Software renderer");
        uiBoxAppend(right, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        cbThreaded3D = uiNewCheckbox("Threaded");
        uiCheckboxOnToggled(cbThreaded3D, OnThreaded3DChanged, NULL);
        uiBoxAppend(in_ctrl, uiControl(cbThreaded3D), 0);
    }

    {
        uiGroup* grp = uiNewGroup("OpenGL renderer");
        uiBoxAppend(right, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        uiLabel* lbl = uiNewLabel("Internal resolution:");
        uiBoxAppend(in_ctrl, uiControl(lbl), 0);

        cbResolution = uiNewCombobox();
        uiComboboxOnSelected(cbResolution, OnResolutionChanged, NULL);
        for (int i = 1; i <= 8; i++)
        {
            char txt[64];
            sprintf(txt, "%dx native (%dx%d)", i, 256*i, 192*i);
            uiComboboxAppend(cbResolution, txt);
        }
        uiBoxAppend(in_ctrl, uiControl(cbResolution), 0);

        //lbl = uiNewLabel("");
        //uiBoxAppend(in_ctrl, uiControl(lbl), 0);

        //cbAntialias = uiNewCheckbox("Antialiasing");
        //uiCheckboxOnToggled(cbAntialias, OnAntialiasChanged, NULL);
        //uiBoxAppend(in_ctrl, uiControl(cbAntialias), 0);
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

    Config::_3DRenderer = Config::_3DRenderer ? 1 : 0;

    if      (Config::GL_ScaleFactor < 1) Config::GL_ScaleFactor = 1;
    else if (Config::GL_ScaleFactor > 8) Config::GL_ScaleFactor = 8;

    old_renderer = Config::_3DRenderer;
    old_gldisplay = Config::ScreenUseGL;
    old_vsync = Config::ScreenVSync;
    old_threaded3D = Config::Threaded3D;
    old_resolution = Config::GL_ScaleFactor;
    old_antialias = Config::GL_Antialias;

    uiCheckboxSetChecked(cbGLDisplay, Config::ScreenUseGL);
    uiCheckboxSetChecked(cbVSync, Config::ScreenVSync);
    uiCheckboxSetChecked(cbThreaded3D, Config::Threaded3D);
    uiComboboxSetSelected(cbResolution, Config::GL_ScaleFactor-1);
    //uiCheckboxSetChecked(cbAntialias, Config::GL_Antialias);
    uiRadioButtonsSetSelected(rbRenderer, Config::_3DRenderer);
    UpdateControls();

    if (Config::ScreenUseGL || Config::_3DRenderer != 0)
        uiControlEnable(uiControl(cbVSync));
    else
        uiControlDisable(uiControl(cbVSync));

    uiControlShow(uiControl(win));
}

void Close()
{
    if (!opened) return;
    uiControlDestroy(uiControl(win));
    opened = false;
}

}
