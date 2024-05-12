/*
    Copyright 2016-2023 melonDS team

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

#ifndef WIFIAP_H
#define WIFIAP_H

#include "types.h"

namespace melonDS
{
class Wifi;

class WifiAP
{
public:
    WifiAP(Wifi* client);
    ~WifiAP();
    void Reset();

    static const char* APName;
    static const u8 APMac[6];
    static const u8 APChannel;

    void MSTimer();

    // packet format: 12-byte TX header + original 802.11 frame
    int SendPacket(const u8* data, int len);
    int RecvPacket(u8* data);

private:
    Wifi* Client;

    u64 USCounter;

    u16 SeqNo;

    bool BeaconDue;

    u8 PacketBuffer[2048];
    int PacketLen;
    int RXNum;

    u8 LANBuffer[2048];

    // this is a lazy AP, we only keep track of one client
    // 0=disconnected 1=authenticated 2=associated
    int ClientStatus;

    int HandleManagementFrame(const u8* data, int len);
};

}
#endif
