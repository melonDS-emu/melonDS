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


wxBEGIN_EVENT_TABLE(InputConfigDialog, wxDialog)
    EVT_COMMAND(1001, wxEVT_BUTTON, InputConfigDialog::OnDerp)

    EVT_COMMAND(100, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(101, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(102, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(103, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(104, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(105, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(106, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(107, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(108, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(109, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(110, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)
    EVT_COMMAND(111, wxEVT_BUTTON, InputConfigDialog::OnConfigureKey)

    EVT_COMMAND(200, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(201, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(202, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(203, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(204, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(205, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(206, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(207, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(208, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(209, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(210, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)
    EVT_COMMAND(211, wxEVT_BUTTON, InputConfigDialog::OnConfigureJoy)

    EVT_TIMER(wxID_ANY, InputConfigDialog::OnPoll)

    EVT_KEY_DOWN(InputConfigDialog::OnKeyDown)
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

                const char* keyname = SDL_GetScancodeName((SDL_Scancode)Config::KeyMapping[j]);
                wxButton* btn = new wxButton(p, 100+j, keyname);
                grid->Add(btn);
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

                const char* keyname = "zorp";//SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)Config::KeyMapping[j]));
                wxButton* btn = new wxButton(p, 200+j, keyname);
                grid->Add(btn);
            }

            p->SetSizer(grid);
            sizer->Add(p, 0, wxALL, 15);
            joyside->SetSizer(sizer);
        }
        sizer->Add(joyside);

        p->SetSizer(sizer);
        vboxmain->Add(p, 0, wxALL&(~wxBOTTOM), 15);
    }

    {
        wxPanel* p = new wxPanel(this);
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

        wxButton* derp = new wxButton(p, 1001, "derp");
        sizer->Add(derp);

        wxButton* boobs = new wxButton(p, 1002, "boobs");
        sizer->Add(3, 0);
        sizer->Add(boobs);

        p->SetSizer(sizer);
        vboxmain->Add(p, 0, wxALL|wxALIGN_RIGHT, 15);
    }

    SetSizer(vboxmain);
    Fit();

    polltimer = new wxTimer(this);
    pollid = 0;

    keystate = SDL_GetKeyboardState(&nkeys);
}

InputConfigDialog::~InputConfigDialog()
{
    delete polltimer;
}

void InputConfigDialog::OnDerp(wxCommandEvent& event)
{
    printf("OnDerp %d\n", event.GetId());
}

void InputConfigDialog::OnConfigureKey(wxCommandEvent& event)
{
    pollid = event.GetId();
    //pollbtn = (wxButton*)event.GetOwner();
    polltimer->Start(100);
}

void InputConfigDialog::OnConfigureJoy(wxCommandEvent& event)
{
    pollid = event.GetId();
    //pollbtn = (wxButton*)event.GetSource();
    polltimer->Start(100);
}

void InputConfigDialog::OnPoll(wxTimerEvent& event)
{
    if (pollid < 100) return;

    SDL_PumpEvents();
    keystate = SDL_GetKeyboardState(&nkeys);

    if (keystate[SDL_SCANCODE_ESCAPE])
    {
        polltimer->Stop();
        //pollbtn->Enable(true);
        pollid = 0;
        return;
    }

    if (pollid >= 200)
    {
        //
    }
    else
    {printf("poll kbd %d, %d, %d\n", pollid, nkeys, keystate[SDL_SCANCODE_A]);
        int id = pollid - 100;
        if (id >= 12) return;

        for (int i = 0; i < nkeys; i++)
        {
            if (keystate[i])
            {
                keymapping[id] = i;

                //pollbtn->Enable(true);

                const char* keyname = SDL_GetScancodeName((SDL_Scancode)i);
                //pollbtn->SetText(keyname);
                printf("%s\n", keyname);

                polltimer->Stop();
                pollid = 0;

                break;
            }
        }
    }
}

void InputConfigDialog::OnKeyDown(wxKeyEvent& event)
{
    printf("!!\n");
}
