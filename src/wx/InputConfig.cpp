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
wxEND_EVENT_TABLE()


InputConfigDialog::InputConfigDialog(wxWindow* parent)
    : wxDialog(parent, -1, "Input configuration - melonDS")
{
    int keyorder[12] = {0, 1, 10, 11, 5, 4, 6, 7, 9, 8, 3, 2};
    char keylabels[12][8] = {"A:", "B:", "Select:", "Start:", "Right:", "Left:", "Up:", "Down:", "R:", "L:", "X:", "Y:"};

    wxBoxSizer* vboxmain = new wxBoxSizer(wxVERTICAL);

    {
        wxPanel* p = new wxPanel(this);
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticBox* kbdside = new wxStaticBox(p, wxID_ANY, "Keyboard");
        {
            wxGridSizer* grid = new wxGridSizer(2, 3, 0);

            for (int i = 0; i < 12; i++)
            {
                int j = keyorder[i];

                wxStaticText* label = new wxStaticText(kbdside, wxID_ANY, keylabels[j]);
                grid->Add(label);

                const char* keyname = SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)Config::KeyMapping[j]));
                wxButton* btn = new wxButton(kbdside, 100+i, keyname);
                grid->Add(btn);
            }

            kbdside->SetSizer(grid);
        }
        sizer->Add(kbdside);

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
}
