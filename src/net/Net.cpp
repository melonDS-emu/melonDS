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

namespace melonDS
{

using Platform::Log;
using Platform::LogLevel;

Net::Net() noexcept
{
    Slirp = std::make_unique<Net_Slirp>(
        [this](const void* buf, int len) {
            this->RXEnqueue(buf, len);
    });
}

Net::Net(const AdapterData& device) noexcept : Net(device.DeviceName)
{
}

Net::Net(std::string_view devicename) noexcept
{
    LibPCap = LibPCap::New();
    PCap = LibPCap->Open(devicename, [this](const void* buf, int len) {
        this->RXEnqueue(buf, len);
    });
}

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
    if (PCap)
        return PCap->SendPacket(data, len);
    else
        return Slirp->SendPacket(data, len);
}

int Net::RecvPacket(u8* data, int inst)
{
    if (PCap)
        PCap->RecvCheck();
    else
        Slirp->RecvCheck();

    int ret = 0;
    if (!Dispatcher.recvPacket(nullptr, nullptr, data, &ret, inst))
        return 0;

    return ret;
}

}
