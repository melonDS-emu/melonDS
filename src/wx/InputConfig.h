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

#ifndef WX_INPUTCONFIG_H
#define WX_INPUTCONFIG_H

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <SDL2/SDL.h>

class InputConfigDialog : public wxDialog
{
public:
    InputConfigDialog(wxWindow* parent);
    ~InputConfigDialog();

private:
    wxDECLARE_EVENT_TABLE();

    void OnDerp(wxCommandEvent& event);

    void OnConfigureKey(wxCommandEvent& event);
    void OnConfigureJoy(wxCommandEvent& event);

    void OnPoll(wxTimerEvent& event);

    void OnKeyDown(wxKeyEvent& event);

    const u8* keystate;
    int nkeys;

    wxTimer* polltimer;
    int pollid;
    wxButton* pollbtn;

    int keymapping[12];
    int joymapping[12];
};

#endif // WX_INPUTCONFIG_H

