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

#include "../types.h"
#include "InputConfig.h"
#include "../Config.h"
#include "scancode_wx2sdl.h"


wxBEGIN_EVENT_TABLE(InputConfigDialog, wxDialog)
    EVT_COMMAND(1001, wxEVT_BUTTON, InputConfigDialog::OnOk)
    EVT_COMMAND(1002, wxEVT_BUTTON, InputConfigDialog::OnCancel)

    EVT_TIMER(wxID_ANY, InputConfigDialog::OnPoll)
    EVT_CHAR_HOOK(InputConfigDialog::OnKeyDown)
wxEND_EVENT_TABLE()


InputConfigDialog::InputConfigDialog(wxWindow* parent)
    : wxDialog(parent, -1, "Input configuration - melonDS")
{
    int keyorder[12] = {0, 1, 10, 11, 5, 4, 6, 7, 9, 8, 3, 2};
    char keylabels[12][8] = {"A:", "B:", "Select:", "Start:", "Right:", "Left:", "Up:", "Down:", "R:", "L:", "X:", "Y:"};

    memcpy(keymapping, Config::KeyMapping, 12*sizeof(int));
    memcpy(joymapping, Config::JoyMapping, 12*sizeof(int));

    wxBoxSizer* vboxmain = new wxBoxSizer(wxVERTICAL);

    {
        wxPanel* p = new wxPanel(this);
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticBox* kbdside = new wxStaticBox(p, wxID_ANY, "Keyboard");
        {
            wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
            wxPanel* p = new wxPanel(kbdside);
            wxFlexGridSizer* grid = new wxFlexGridSizer(2, 3, 8);

            for (int i = 0; i < 12; i++)
            {
                int j = keyorder[i];

                wxStaticText* label = new wxStaticText(p, wxID_ANY, keylabels[j]);
                grid->Add(label, 0, wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL);

                // SDL_GetScancodeName() doesn't take the keyboard layout into account,
                // which can be inconvenient
                const char* keyname = SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)keymapping[j]));

                wxStaticText* btn = new wxStaticText(p, 100+j, keyname,
                    wxDefaultPosition, wxDefaultSize,
                    wxALIGN_CENTRE_HORIZONTAL | wxBORDER_SUNKEN | wxST_NO_AUTORESIZE);
                btn->SetMinSize(wxSize(120, btn->GetSize().GetHeight()));
                grid->Add(btn);

                btn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
                btn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));

                btn->Connect(100+j, wxEVT_LEFT_UP, wxMouseEventHandler(InputConfigDialog::OnConfigureKey));
                btn->Connect(100+j, wxEVT_ENTER_WINDOW, wxMouseEventHandler(InputConfigDialog::OnFancybuttonHover));
                btn->Connect(100+j, wxEVT_LEAVE_WINDOW, wxMouseEventHandler(InputConfigDialog::OnFancybuttonHover));
            }

            p->SetSizer(grid);
            sizer->Add(p, 0, wxALL, 15);
            kbdside->SetSizer(sizer);
        }
        sizer->Add(kbdside);

        wxStaticBox* joyside = new wxStaticBox(p, wxID_ANY, "Joystick");
        {
            wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
            wxPanel* p = new wxPanel(joyside);
            wxFlexGridSizer* grid = new wxFlexGridSizer(2, 3, 8);

            for (int i = 0; i < 12; i++)
            {
                int j = keyorder[i];

                wxStaticText* label = new wxStaticText(p, wxID_ANY, keylabels[j]);
                grid->Add(label, 0, wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL);

                char keyname[16];
                JoyMappingName(joymapping[j], keyname);

                wxStaticText* btn = new wxStaticText(p, 200+j, keyname,
                    wxDefaultPosition, wxDefaultSize,
                    wxALIGN_CENTRE_HORIZONTAL | wxBORDER_SUNKEN | wxST_NO_AUTORESIZE);
                btn->SetMinSize(wxSize(120, btn->GetSize().GetHeight()));
                grid->Add(btn);

                btn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
                btn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));

                btn->Connect(200+j, wxEVT_LEFT_UP, wxMouseEventHandler(InputConfigDialog::OnConfigureJoy));
                btn->Connect(200+j, wxEVT_ENTER_WINDOW, wxMouseEventHandler(InputConfigDialog::OnFancybuttonHover));
                btn->Connect(200+j, wxEVT_LEAVE_WINDOW, wxMouseEventHandler(InputConfigDialog::OnFancybuttonHover));
            }

            p->SetSizer(grid);
            sizer->Add(p, 0, wxALL, 15);

            /*wxComboBox* joycombo = new wxComboBox(joyside, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);

            int numjoys = SDL_NumJoysticks();
            if (numjoys > 0)
            {
                for (int i = 0; i < numjoys; i++)
                {
                    const char* name = SDL_JoystickNameForIndex(i);
                    joycombo->Insert(name, i);
                }
            }
            else
            {
                joycombo->Append("(no joystick)");
                joycombo->Enable(false);
            }

            sizer->Add(joycombo, 0, wxALL&(~wxTOP), 15);*/

            joyside->SetSizer(sizer);
        }
        sizer->Add(15, 0);
        sizer->Add(joyside);

        p->SetSizer(sizer);
        vboxmain->Add(p, 0, wxALL&(~wxBOTTOM), 15);
    }

    {
        wxPanel* p = new wxPanel(this);
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

        wxButton* ok = new wxButton(p, 1001, "OK");
        sizer->Add(ok);

        wxButton* cancel = new wxButton(p, 1002, "Cancel");
        sizer->Add(3, 0);
        sizer->Add(cancel);

        p->SetSizer(sizer);
        vboxmain->Add(p, 0, wxALL|wxALIGN_RIGHT, 15);
    }

    SetSizer(vboxmain);
    Fit();

    polltimer = new wxTimer(this);
    pollid = 0;

    njoys = SDL_NumJoysticks();
    if (njoys) joy = SDL_JoystickOpen(0);
}

InputConfigDialog::~InputConfigDialog()
{
    delete polltimer;

    if (njoys) SDL_JoystickClose(0);
}

void InputConfigDialog::OnOk(wxCommandEvent& event)
{
    memcpy(Config::KeyMapping, keymapping, 12*sizeof(int));
    memcpy(Config::JoyMapping, joymapping, 12*sizeof(int));
    Config::Save();

    Close();
}

void InputConfigDialog::OnCancel(wxCommandEvent& event)
{
    Close();
}

void InputConfigDialog::OnKeyDown(wxKeyEvent& event)
{
    if (pollid < 100) return;
    int id = pollid - 100;

    SDL_Scancode code = scancode_wx2sdl(event);

    if (code == SDL_SCANCODE_ESCAPE)
    {
        if (pollid >= 200) polltimer->Stop();
        pollbtn->SetLabel(pollbtntext);
        pollid = 0;

        pollbtn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
        pollbtn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
        pollbtn->Refresh();
        return;
    }
    else if ((code == SDL_SCANCODE_BACKSPACE) && (pollid >= 200))
    {
        id = pollid - 200;
        if (id >= 12) return;

        joymapping[id] = -1;

        char keyname[16];
        JoyMappingName(joymapping[id], keyname);
        pollbtn->SetLabel(keyname);

        polltimer->Stop();
        pollid = 0;

        pollbtn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
        pollbtn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
        pollbtn->Refresh();
        return;
    }

    if (id >= 12) return;

    keymapping[id] = code;

    pollbtn->Enable(true);

    const char* keyname = SDL_GetKeyName(SDL_GetKeyFromScancode(code));
    pollbtn->SetLabel(keyname);

    polltimer->Stop();
    pollid = 0;

    pollbtn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    pollbtn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    pollbtn->Refresh();
}

// black magic going on there
// these two event handlers are in the InputConfigDialog class for convenience,
// but when they're called, 'this' points to a wxStaticText instance

void InputConfigDialog::OnConfigureKey(wxMouseEvent& event)
{
    wxStaticText* btn = (wxStaticText*)this;
    InputConfigDialog* parent = (InputConfigDialog*)btn->GetParent()->GetParent()->GetParent()->GetParent();
    if (parent->pollid != 0) return;

    parent->pollbtn = btn;
    parent->pollbtntext = btn->GetLabel();
    parent->pollid = event.GetId();

    btn->SetLabel("[press key]");
    btn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT));
    btn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    btn->Refresh();
}

void InputConfigDialog::OnConfigureJoy(wxMouseEvent& event)
{
    wxStaticText* btn = (wxStaticText*)this;
    InputConfigDialog* parent = (InputConfigDialog*)btn->GetParent()->GetParent()->GetParent()->GetParent();
    if (parent->pollid != 0) return;

    parent->pollbtn = btn;
    parent->pollbtntext = btn->GetLabel();
    parent->pollid = event.GetId();
    parent->polltimer->Start(50);

    btn->SetLabel("[press button]");
    btn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT));
    btn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    btn->Refresh();
}

void InputConfigDialog::OnPoll(wxTimerEvent& event)
{
    if (pollid < 200) return;

    int id = pollid - 200;
    if (id >= 12) return;

    int nbuttons = SDL_JoystickNumButtons(joy);
    for (int i = 0; i < nbuttons; i++)
    {
        if (SDL_JoystickGetButton(joy, i))
        {
            joymapping[id] = i;

            char keyname[16];
            JoyMappingName(joymapping[id], keyname);
            pollbtn->SetLabel(keyname);

            polltimer->Stop();
            pollid = 0;

            pollbtn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
            pollbtn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
            pollbtn->Refresh();
            return;
        }
    }

    u8 blackhat = SDL_JoystickGetHat(joy, 0);
    if (blackhat)
    {
        if      (blackhat & 0x1) blackhat = 0x1;
        else if (blackhat & 0x2) blackhat = 0x2;
        else if (blackhat & 0x4) blackhat = 0x4;
        else                     blackhat = 0x8;

        joymapping[id] = 0x100 | blackhat;

        char keyname[16];
        JoyMappingName(joymapping[id], keyname);
        pollbtn->SetLabel(keyname);

        polltimer->Stop();
        pollid = 0;

        pollbtn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
        pollbtn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
        pollbtn->Refresh();
        return;
    }
}

void InputConfigDialog::OnFancybuttonHover(wxMouseEvent& event)
{
    wxStaticText* btn = (wxStaticText*)this;
    InputConfigDialog* parent = (InputConfigDialog*)btn->GetParent()->GetParent()->GetParent()->GetParent();
    if (event.GetId() == parent->pollid) return;

    if (event.Entering())
    {
        btn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT));
        btn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    }
    else
    {
        btn->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
        btn->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    }
    btn->Refresh();
}

void InputConfigDialog::JoyMappingName(int id, char* str)
{
    if (id < 0)
    {
        strcpy(str, "None");
        return;
    }

    if (id & 0x100)
    {
        switch (id & 0xF)
        {
        case 0x1: strcpy(str, "Up"); break;
        case 0x2: strcpy(str, "Right"); break;
        case 0x4: strcpy(str, "Down"); break;
        case 0x8: strcpy(str, "Left"); break;
        }
    }
    else
    {
        sprintf(str, "Button %d", id+1);
    }
}
