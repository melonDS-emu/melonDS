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

#include "LAN_Socket.h"
#include "LAN_PCap.h"
#include "DlgWifiSettings.h"


#ifdef __WIN32__
#define PCAP_NAME "winpcap/npcap"
#else
#define PCAP_NAME "libpcap"
#endif // __WIN32__


void ApplyNewSettings(int type);


namespace DlgWifiSettings
{

bool opened;
uiWindow* win;

bool haspcap;

uiCheckbox* cbBindAnyAddr;

uiLabel* lbAdapterList;
uiCombobox* cmAdapterList;
uiCheckbox* cbDirectLAN;

uiLabel* lbAdapterMAC;
uiLabel* lbAdapterIP;
uiLabel* lbAdapterDNS0;
uiLabel* lbAdapterDNS1;


void UpdateAdapterControls()
{
    bool enable = haspcap && uiCheckboxChecked(cbDirectLAN);

    if (enable)
    {
        uiControlEnable(uiControl(lbAdapterList));
        uiControlEnable(uiControl(cmAdapterList));
        uiControlEnable(uiControl(lbAdapterMAC));
        uiControlEnable(uiControl(lbAdapterIP));
    }
    else
    {
        uiControlDisable(uiControl(lbAdapterList));
        uiControlDisable(uiControl(cmAdapterList));
        uiControlDisable(uiControl(lbAdapterMAC));
        uiControlDisable(uiControl(lbAdapterIP));
    }
}

void UpdateAdapterInfo()
{
    if (!haspcap) return;

    int sel = uiComboboxSelected(cmAdapterList);
    if (sel < 0 || sel >= LAN_PCap::NumAdapters) return;
    if (LAN_PCap::NumAdapters < 1) return;

    LAN_PCap::AdapterData* adapter = &LAN_PCap::Adapters[sel];
    char tmp[64];

    sprintf(tmp, "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
            adapter->MAC[0], adapter->MAC[1], adapter->MAC[2],
            adapter->MAC[3], adapter->MAC[4], adapter->MAC[5]);
    uiLabelSetText(lbAdapterMAC, tmp);

    sprintf(tmp, "IP: %d.%d.%d.%d",
            adapter->IP_v4[0], adapter->IP_v4[1],
            adapter->IP_v4[2], adapter->IP_v4[3]);
    uiLabelSetText(lbAdapterIP, tmp);

    /*sprintf(tmp, "Primary DNS: %d.%d.%d.%d",
            adapter->DNS[0][0], adapter->DNS[0][1],
            adapter->DNS[0][2], adapter->DNS[0][3]);
    uiLabelSetText(lbAdapterDNS0, tmp);

    sprintf(tmp, "Secondary DNS: %d.%d.%d.%d",
            adapter->DNS[1][0], adapter->DNS[1][1],
            adapter->DNS[1][2], adapter->DNS[1][3]);
    uiLabelSetText(lbAdapterDNS1, tmp);*/
}

int OnCloseWindow(uiWindow* window, void* blarg)
{
    opened = false;
    return 1;
}

void OnDirectModeToggle(uiCheckbox* c, void* blarg)
{
    UpdateAdapterControls();
}

void OnAdapterSelect(uiCombobox* c, void* blarg)
{
    UpdateAdapterInfo();
}

void OnCancel(uiButton* btn, void* blarg)
{
    uiControlDestroy(uiControl(win));
    opened = false;
}

void OnOk(uiButton* btn, void* blarg)
{
    Config::SocketBindAnyAddr = uiCheckboxChecked(cbBindAnyAddr);
    Config::DirectLAN = uiCheckboxChecked(cbDirectLAN);

    int sel = uiComboboxSelected(cmAdapterList);
    if (sel < 0 || sel >= LAN_PCap::NumAdapters) sel = 0;
    if (LAN_PCap::NumAdapters < 1)
    {
        Config::LANDevice[0] = '\0';
    }
    else
    {
        strncpy(Config::LANDevice, LAN_PCap::Adapters[sel].DeviceName, 127);
        Config::LANDevice[127] = '\0';
    }

    Config::Save();

    uiControlDestroy(uiControl(win));
    opened = false;

    ApplyNewSettings(1);
}

void Open()
{
    if (opened)
    {
        uiControlSetFocus(uiControl(win));
        return;
    }

    LAN_Socket::Init();
    haspcap = LAN_PCap::Init(false);

    opened = true;
    win = uiNewWindow("Wifi settings - melonDS", 400, 100, 0, 0, 0);
    uiWindowSetMargined(win, 1);
    uiWindowOnClosing(win, OnCloseWindow, NULL);

    uiBox* top = uiNewVerticalBox();
    uiWindowSetChild(win, uiControl(top));
    uiBoxSetPadded(top, 1);

    {
        uiGroup* grp = uiNewGroup("Local");
        uiBoxAppend(top, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        cbBindAnyAddr = uiNewCheckbox("Bind socket to any address");
        uiBoxAppend(in_ctrl, uiControl(cbBindAnyAddr), 0);
    }

    {
        uiLabel* lbl;

        uiGroup* grp = uiNewGroup("Online");
        uiBoxAppend(top, uiControl(grp), 0);
        uiGroupSetMargined(grp, 1);

        uiBox* in_ctrl = uiNewVerticalBox();
        uiGroupSetChild(grp, uiControl(in_ctrl));

        cbDirectLAN = uiNewCheckbox("Direct mode (requires " PCAP_NAME " and ethernet connection)");
        uiCheckboxOnToggled(cbDirectLAN, OnDirectModeToggle, NULL);
        uiBoxAppend(in_ctrl, uiControl(cbDirectLAN), 0);

        lbAdapterList = uiNewLabel("Network adapter:");
        uiBoxAppend(in_ctrl, uiControl(lbAdapterList), 0);

        cmAdapterList = uiNewCombobox();
        uiComboboxOnSelected(cmAdapterList, OnAdapterSelect, NULL);
        uiBoxAppend(in_ctrl, uiControl(cmAdapterList), 0);

        lbAdapterMAC = uiNewLabel("MAC: ??");
        uiBoxAppend(in_ctrl, uiControl(lbAdapterMAC), 0);
        lbAdapterIP = uiNewLabel("IP: ??");
        uiBoxAppend(in_ctrl, uiControl(lbAdapterIP), 0);
        /*lbAdapterDNS0 = uiNewLabel("DNS0");
        uiBoxAppend(in_ctrl, uiControl(lbAdapterDNS0), 0);
        lbAdapterDNS1 = uiNewLabel("DNS1");
        uiBoxAppend(in_ctrl, uiControl(lbAdapterDNS1), 0);*/
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

    uiCheckboxSetChecked(cbBindAnyAddr, Config::SocketBindAnyAddr);

    int sel = 0;
    for (int i = 0; i < LAN_PCap::NumAdapters; i++)
    {
        LAN_PCap::AdapterData* adapter = &LAN_PCap::Adapters[i];

        uiComboboxAppend(cmAdapterList, adapter->FriendlyName);

        if (!strncmp(adapter->DeviceName, Config::LANDevice, 128))
            sel = i;
    }
    uiComboboxSetSelected(cmAdapterList, sel);
    UpdateAdapterInfo();

    uiCheckboxSetChecked(cbDirectLAN, Config::DirectLAN);
    if (!haspcap) uiControlDisable(uiControl(cbDirectLAN));
    UpdateAdapterControls();

    uiControlShow(uiControl(win));
}

}
