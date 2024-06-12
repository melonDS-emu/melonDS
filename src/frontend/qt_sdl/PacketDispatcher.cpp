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


PacketDispatcher::PacketDispatcher()
{
    instanceMask = 0;
    memset(packetQueues, 0, sizeof(packetQueues));
}

PacketDispatcher::~PacketDispatcher()
{
    //
}


void PacketDispatcher::registerInstance(int inst)
{
    mutex.lock();

    instanceMask |= (1 << inst);
    packetQueues[inst] = new PacketQueue();

    mutex.unlock();
}

void PacketDispatcher::unregisterInstance(int inst)
{
    mutex.lock();

    instanceMask &= ~(1 << inst);
    delete packetQueues[inst];

    mutex.unlock();
}


void PacketDispatcher::sendPacket(const void* header, int headerlen, const void* data, int datalen, int sender, u16 recv_mask)
{
    if (!header) headerlen = 0;
    if (!data) datalen = 0;
    if ((!headerlen) && (!datalen)) return;
    if ((sizeof(PacketHeader) + headerlen + datalen) >= 0x8000) return;
    if (sender < 0 || sender > 15) return;

    recv_mask &= instanceMask;
    recv_mask &= ~(1 << sender);
    if (!recv_mask) return;

    PacketHeader phdr;
    phdr.magic = kPacketMagic;
    phdr.senderID = sender;
    phdr.headerLength = headerlen;
    phdr.dataLength = datalen;

    mutex.lock();
    for (int i = 0; i < 16; i++)
    {
        if (!(recv_mask & (1 << i)))
            continue;

        PacketQueue* queue = packetQueues[i];
        queue->Write(&phdr, sizeof(phdr));
        if (headerlen) queue->Write(header, headerlen);
        if (datalen) queue->Write(data, datalen);
    }
    mutex.unlock();
}

bool PacketDispatcher::recvPacket(void *header, int *headerlen, void *data, int *datalen, int receiver)
{
    if ((!header) && (!data)) return false;
    if (receiver < 0 || receiver > 15) return false;

    mutex.lock();
    PacketQueue* queue = packetQueues[receiver];

    PacketHeader phdr;
    if (!queue->Read(&phdr, sizeof(phdr)))
    {
        mutex.unlock();
        return false;
    }

    if (phdr.magic != kPacketMagic)
    {
        mutex.unlock();
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

    mutex.unlock();
    return true;
}
