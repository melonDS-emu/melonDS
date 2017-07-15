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
#include "EmuConfig.h"
#include "../Config.h"


wxBEGIN_EVENT_TABLE(EmuConfigDialog, wxDialog)
    EVT_COMMAND(1001, wxEVT_BUTTON, EmuConfigDialog::OnOk)
    EVT_COMMAND(1002, wxEVT_BUTTON, EmuConfigDialog::OnCancel)
wxEND_EVENT_TABLE()


EmuConfigDialog::EmuConfigDialog(wxWindow* parent)
    : wxDialog(parent, -1, "Emulation settings - melonDS")
{
    wxBoxSizer* vboxmain = new wxBoxSizer(wxVERTICAL);

    cbDirectBoot = new wxCheckBox(this, wxID_ANY, "Boot game directly");
    vboxmain->Add(cbDirectBoot, 0, wxALL&(~wxBOTTOM), 15);
    cbDirectBoot->SetValue(Config::DirectBoot != 0);

    cbThreaded3D = new wxCheckBox(this, wxID_ANY, "Threaded 3D renderer");
    vboxmain->Add(cbThreaded3D, 0, wxALL&(~wxBOTTOM), 15);
    cbThreaded3D->SetValue(Config::Threaded3D != 0);

    cbBindAnyAddr = new wxCheckBox(this, wxID_ANY, "Wifi: bind socket to any address");
    vboxmain->Add(cbBindAnyAddr, 0, wxALL&(~wxBOTTOM), 15);
    cbBindAnyAddr->SetValue(Config::SocketBindAnyAddr != 0);

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
}

EmuConfigDialog::~EmuConfigDialog()
{
}

void EmuConfigDialog::OnOk(wxCommandEvent& event)
{
    Config::DirectBoot = cbDirectBoot->GetValue() ? 1:0;
    Config::Threaded3D = cbThreaded3D->GetValue() ? 1:0;
    Config::SocketBindAnyAddr = cbBindAnyAddr->GetValue() ? 1:0;
    Config::Save();

    Close();
}

void EmuConfigDialog::OnCancel(wxCommandEvent& event)
{
    Close();
}

