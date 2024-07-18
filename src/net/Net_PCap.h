/*
    Copyright 2016-2024 melonDS team

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

#ifndef NET_PCAP_H
#define NET_PCAP_H

#include <string_view>
#include "types.h"

namespace Net_PCap
{

using namespace melonDS;
struct AdapterData
{
    char DeviceName[128];
    char FriendlyName[128];
    char Description[128];

    u8 MAC[6];
    u8 IP_v4[4];
};


extern AdapterData* Adapters;
extern int NumAdapters;


bool InitAdapterList();
bool Init(std::string_view devicename);
void DeInit();

int SendPacket(u8* data, int len);
void RecvCheck();

}

#endif // NET_PCAP_H
