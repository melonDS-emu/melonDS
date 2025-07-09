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

#include "PacketDispatcher.h"

using namespace melonDS;

struct PacketHeader
{
    u32 magic;
    u32 senderID;
    u32 headerLength;
    u32 dataLength;
};

const u32 kPacketMagic = 0x4B504C4D;


PacketDispatcher::PacketDispatcher() : mutex(Platform::Mutex_Create())
{
    instanceMask = 0;
}

PacketDispatcher::~PacketDispatcher()
{
    Platform::Mutex_Free(mutex);
}


void PacketDispatcher::registerInstance(int inst)
{
    Mutex_Lock(mutex);

    instanceMask |= (1 << inst);
    packetQueues[inst] = std::make_unique<PacketQueue>();

    Mutex_Unlock(mutex);
}

void PacketDispatcher::unregisterInstance(int inst)
{
    Mutex_Lock(mutex);

    instanceMask &= ~(1 << inst);
    packetQueues[inst] = nullptr;

    Mutex_Unlock(mutex);
}


void PacketDispatcher::clear()
{
    Mutex_Lock(mutex);
    for (int i = 0; i < 16; i++)
    {
        if (!(instanceMask & (1 << i)))
            continue;

        packetQueues[i]->Clear();
    }
    Mutex_Unlock(mutex);
}


void PacketDispatcher::sendPacket(const void* header, int headerlen, const void* data, int datalen, int sender, u16 recv_mask)
{
    if (!header) headerlen = 0;
    if (!data) datalen = 0;
    if ((!headerlen) && (!datalen)) return;
    if ((sizeof(PacketHeader) + headerlen + datalen) >= 0x8000) return;
    if (sender < 0 || sender > 16) return;

    recv_mask &= instanceMask;
    if (sender < 16) recv_mask &= ~(1 << sender);
    if (!recv_mask) return;

    PacketHeader phdr;
    phdr.magic = kPacketMagic;
    phdr.senderID = sender;
    phdr.headerLength = headerlen;
    phdr.dataLength = datalen;

    int totallen = sizeof(phdr) + headerlen + datalen;

    Mutex_Lock(mutex);
    for (int i = 0; i < 16; i++)
    {
        if (!(recv_mask & (1 << i)))
            continue;

        PacketQueue* queue = packetQueues[i].get();

        // if we run out of space: discard old packets
        while (!queue->CanFit(totallen))
        {
            PacketHeader tmp;
            queue->Read(&tmp, sizeof(tmp));
            queue->Skip(tmp.headerLength + tmp.dataLength);
        }

        queue->Write(&phdr, sizeof(phdr));
        if (headerlen) queue->Write(header, headerlen);
        if (datalen) queue->Write(data, datalen);
    }
    Mutex_Unlock(mutex);
}

bool PacketDispatcher::recvPacket(void *header, int *headerlen, void *data, int *datalen, int receiver)
{
    if ((!header) && (!data)) return false;
    if (receiver < 0 || receiver > 15) return false;

    Mutex_Lock(mutex);
    PacketQueue* queue = packetQueues[receiver].get();

    PacketHeader phdr;
    if (!queue->Read(&phdr, sizeof(phdr)))
    {
        Mutex_Unlock(mutex);
        return false;
    }

    if (phdr.magic != kPacketMagic)
    {
        Mutex_Unlock(mutex);
        return false;
    }

    if (phdr.headerLength)
    {
        if (headerlen) *headerlen = phdr.headerLength;
        if (header) queue->Read(header, phdr.headerLength);
        else queue->Skip(phdr.headerLength);
    }

    if (phdr.dataLength)
    {
        if (datalen) *datalen = phdr.dataLength;
        if (data) queue->Read(data, phdr.dataLength);
        else queue->Skip(phdr.dataLength);
    }

    Mutex_Unlock(mutex);
    return true;
}
