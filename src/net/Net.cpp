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

#include <stdio.h>
#include <string.h>
#include "Net.h"
#include "Net_PCap.h"
#include "Net_Slirp.h"
#include "PacketDispatcher.h"
#include "Platform.h"

using namespace melonDS;

namespace Net
{

using Platform::Log;
using Platform::LogLevel;

bool Inited = false;
bool DirectMode;

PacketDispatcher Dispatcher;


bool Init()
{
    if (Inited) DeInit();

    Dispatcher.clear();

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


void RegisterInstance(int inst)
{
    Dispatcher.registerInstance(inst);
}

void UnregisterInstance(int inst)
{
    Dispatcher.unregisterInstance(inst);
}


void RXEnqueue(const void* buf, int len)
{
    Dispatcher.sendPacket(nullptr, 0, buf, len, 16, 0xFFFF);
}


int SendPacket(u8* data, int len, int inst)
{
    if (DirectMode)
        return Net_PCap::SendPacket(data, len);
    else
        return Net_Slirp::SendPacket(data, len);
}

int RecvPacket(u8* data, int inst)
{
    if (DirectMode)
        Net_PCap::RecvCheck();
    else
        Net_Slirp::RecvCheck();

    int ret = 0;
    if (!Dispatcher.recvPacket(nullptr, nullptr, data, &ret, inst))
        return 0;

    return ret;
}

}
