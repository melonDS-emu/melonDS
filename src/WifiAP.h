/*
    Copyright 2016-2022 melonDS team

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

namespace WifiAP
{

#define AP_MAC  0x00, 0xF0, 0x77, 0x77, 0x77, 0x77
#define AP_NAME "melonAP"

extern const u8 APMac[6];

bool Init();
void DeInit();
void Reset();

void MSTimer();

// packet format: 12-byte TX header + original 802.11 frame
int SendPacket(u8* data, int len);
int RecvPacket(u8* data);

}

#endif
