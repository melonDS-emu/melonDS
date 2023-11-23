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
    static const char* APName;
    static const u8 APMac[6];

    explicit WifiAP(Wifi& client) noexcept;
    void Reset();
    void MSTimer();

    // packet format: 12-byte TX header + original 802.11 frame
    int SendPacket(u8* data, int len);
    int RecvPacket(u8* data);

private:
    Wifi& Client;

    u64 USCounter = 0x428888000ULL;

    u16 SeqNo = 0x0120;

    bool BeaconDue = false;

    std::array<u8, 2048> PacketBuffer {};
    int PacketLen = 0;
    int RXNum = 0;

    std::array<u8, 2048> LANBuffer {};

    // this is a lazy AP, we only keep track of one client
    // 0=disconnected 1=authenticated 2=associated
    int ClientStatus = 0;

    int HandleManagementFrame(u8* data, int len);
};

}
#endif
