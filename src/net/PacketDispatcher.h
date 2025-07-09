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

#ifndef PACKETDISPATCHER_H
#define PACKETDISPATCHER_H

#include <array>
#include <memory>
#include "Platform.h"
#include "types.h"
#include "FIFO.h"

using PacketQueue = melonDS::RingBuffer<0x8000>;

class PacketDispatcher
{
public:
    PacketDispatcher();
    ~PacketDispatcher();

    void registerInstance(int inst);
    void unregisterInstance(int inst);

    void clear();

    void sendPacket(const void* header, int headerlen, const void* data, int datalen, int sender, melonDS::u16 recv_mask);
    bool recvPacket(void* header, int* headerlen, void* data, int* datalen, int receiver);

private:
    melonDS::Platform::Mutex* mutex;
    melonDS::u16 instanceMask;
    std::array<std::unique_ptr<PacketQueue>, 16> packetQueues {};
};

#endif // PACKETDISPATCHER_H
