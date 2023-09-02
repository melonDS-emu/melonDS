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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//

#include "LAN.h"
#include "Config.h"
//#include "main.h"
//


//extern EmuThread* emuThread;


namespace LAN
{

//


bool Init()
{
    //

    return true;
}

void DeInit()
{
    //
}


void SetMPRecvTimeout(int timeout)
{
    //MPRecvTimeout = timeout;
}

void MPBegin()
{
    //
}

void MPEnd()
{
    //
}

void SetActive(bool active)
{
    //
}

u16 GetInstanceBitmask()
{
    //
    return 0;
}


int SendMPPacket(u8* packet, int len, u64 timestamp)
{
    return 0;
}

int RecvMPPacket(u8* packet, u64* timestamp)
{
    return 0;
}


int SendMPCmd(u8* packet, int len, u64 timestamp)
{
    return 0;
}

int SendMPReply(u8* packet, int len, u64 timestamp, u16 aid)
{
    return 0;
}

int SendMPAck(u8* packet, int len, u64 timestamp)
{
    return 0;
}

int RecvMPHostPacket(u8* packet, u64* timestamp)
{
    return 0;
}

u16 RecvMPReplies(u8* packets, u64 timestamp, u16 aidmask)
{
    return 0;
}

}
