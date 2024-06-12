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

#include <stdio.h>
#include <string.h>
#include <QMutex>
#include "Net.h"
#include "FIFO.h"
#include "Platform.h"
#include "Config.h"

using namespace melonDS;

namespace Net
{

using Platform::Log;
using Platform::LogLevel;

bool Inited = false;
bool DirectMode;

QMutex RXMutex;
RingBuffer<0x10000> RXBuffer;


bool Init()
{
    if (Inited) DeInit();

    RXBuffer.Clear();

    Config::Table cfg = Config::GetGlobalTable();
    DirectMode = cfg.GetBool("LAN.DirectMode");

    bool ret = false;
    if (DirectMode)
        ret = Net_PCap::Init();
    else
        ret = Net_Slirp::Init();

    Inited = ret;
    return ret;
}

void DeInit()
{
    if (!Inited) return;

    if (DirectMode)
        Net_PCap::DeInit();
    else
        Net_Slirp::DeInit();

    Inited = false;
}


void RXEnqueue(const void* buf, int len)
{
    int totallen = len + 4;

    RXMutex.lock();

    if (!RXBuffer.CanFit(totallen))
    {
        RXMutex.unlock();
        Log(LogLevel::Warn, "Net: !! NOT ENOUGH SPACE IN RX BUFFER\n");
        return;
    }

    u32 header = (len & 0xFFFF) | (len << 16);
    RXBuffer.Write(&header, sizeof(u32));
    RXBuffer.Write(buf, len);
    RXMutex.unlock();
}


int SendPacket(u8* data, int len)
{
    if (DirectMode)
        return Net_PCap::SendPacket(data, len);
    else
        return Net_Slirp::SendPacket(data, len);
}

int RecvPacket(u8* data)
{
    if (DirectMode)
        Net_PCap::RecvCheck();
    else
        Net_Slirp::RecvCheck();

    // TODO: check MAC
    // FIFO header | destination MAC | source MAC | frame type
}

}
