/*
    Copyright 2016-2025 melonDS team

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
#include "PacketDispatcher.h"
#include "Platform.h"

namespace melonDS
{

using Platform::Log;
using Platform::LogLevel;

void Net::RegisterInstance(int inst)
{
    Dispatcher.registerInstance(inst);
}

void Net::UnregisterInstance(int inst)
{
    Dispatcher.unregisterInstance(inst);
}


void Net::RXEnqueue(const void* buf, int len)
{
    Dispatcher.sendPacket(nullptr, 0, buf, len, 16, 0xFFFF);
}


int Net::SendPacket(u8* data, int len, int inst)
{
    if (!Driver)
        return 0;

    return Driver->SendPacket(data, len);
}

int Net::RecvPacket(u8* data, int inst)
{
    if (!Driver)
        return 0;

    Driver->RecvCheck();

    int ret = 0;
    if (!Dispatcher.recvPacket(nullptr, nullptr, data, &ret, inst))
        return 0;

    return ret;
}

}
