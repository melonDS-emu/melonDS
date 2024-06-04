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
#include <stdlib.h>
#include <string.h>

#ifdef __WIN32__
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <semaphore.h>
    #include <time.h>
    #ifdef __APPLE__
        #include "sem_timedwait.h"
    #endif
#endif

#include <string>
#include <QSharedMemory>
#include <QMutex>
#include <QSemaphore>

#include "Config.h"
#include "LocalMP.h"
#include "Platform.h"

using namespace melonDS;
using namespace melonDS::Platform;

using Platform::Log;
using Platform::LogLevel;

namespace LocalMP
{

struct MPStatusData
{
    u16 ConnectedBitmask; // bitmask of which instances are ready to send/receive packets
    u32 PacketWriteOffset;
    u32 ReplyWriteOffset;
    u16 MPHostinst; // instance ID from which the last CMD frame was sent
    u16 MPReplyBitmask;   // bitmask of which clients replied in time
};

struct MPPacketHeader
{
    u32 Magic;
    u32 SenderID;
    u32 Type;       // 0=regular 1=CMD 2=reply 3=ack
    u32 Length;
    u64 Timestamp;
};

QMutex MPQueueLock;
MPStatusData MPStatus;
u8* MPQueue = nullptr;
u32 PacketReadOffset[16];
u32 ReplyReadOffset[16];

const u32 kQueueSize = 0x20000;
const u32 kMaxFrameSize = 0x800;
const u32 kPacketStart = 0;
const u32 kReplyStart = kQueueSize / 2;
const u32 kPacketEnd = kReplyStart;
const u32 kReplyEnd = kQueueSize;

int RecvTimeout;

int LastHostID;


QSemaphore SemPool[32];

void SemPoolInit()
{
    for (int i = 0; i < 32; i++)
    {
        SemPool[i].acquire(SemPool[i].available());
    }
}

bool SemPost(int num)
{
    SemPool[num].release(1);
    return true;
}

bool SemWait(int num, int timeout)
{
    if (!timeout)
        return SemPool[num].tryAcquire(1);

    return SemPool[num].tryAcquire(1, timeout);
}

void SemReset(int num)
{
    SemPool[num].acquire(SemPool[num].available());
}


bool Init()
{
    MPQueueLock.lock();

    MPQueue = new u8[kQueueSize];
    memset(MPQueue, 0, kQueueSize);
    memset(&MPStatus, 0, sizeof(MPStatus));
    memset(PacketReadOffset, 0, sizeof(PacketReadOffset));
    memset(ReplyReadOffset, 0, sizeof(ReplyReadOffset));

    MPQueueLock.unlock();

    // prepare semaphores
    // semaphores 0-15: regular frames; semaphore I is posted when instance I needs to process a new frame
    // semaphores 16-31: MP replies; semaphore I is posted when instance I needs to process a new MP reply

    SemPoolInit();

    LastHostID = -1;

    Log(LogLevel::Info, "MP comm init OK\n");

    RecvTimeout = 25;

    return true;
}

void DeInit()
{
    delete MPQueue;
    MPQueue = nullptr;
}

void SetRecvTimeout(int timeout)
{
    RecvTimeout = timeout;
}

void Begin(int inst)
{
    if (!MPQueue) return;

    MPQueueLock.lock();
    PacketReadOffset[inst] = MPStatus.PacketWriteOffset;
    ReplyReadOffset[inst] = MPStatus.ReplyWriteOffset;
    SemReset(inst);
    SemReset(16+inst);
    MPStatus.ConnectedBitmask |= (1 << inst);
    MPQueueLock.unlock();
}

void End(int inst)
{
    if (!MPQueue) return;

    MPQueueLock.lock();
    MPStatus.ConnectedBitmask &= ~(1 << inst);
    MPQueueLock.unlock();
}

void FIFORead(int inst, int fifo, void* buf, int len)
{
    u8* data = MPQueue;

    u32 offset, start, end;
    if (fifo == 0)
    {
        offset = PacketReadOffset[inst];
        start = kPacketStart;
        end = kPacketEnd;
    }
    else
    {
        offset = ReplyReadOffset[inst];
        start = kReplyStart;
        end = kReplyEnd;
    }

    if ((offset + len) >= end)
    {
        u32 part1 = end - offset;
        memcpy(buf, &data[offset], part1);
        memcpy(&((u8*)buf)[part1], &data[start], len - part1);
        offset = start + len - part1;
    }
    else
    {
        memcpy(buf, &data[offset], len);
        offset += len;
    }

    if (fifo == 0) PacketReadOffset[inst] = offset;
    else           ReplyReadOffset[inst] = offset;
}

void FIFOWrite(int inst, int fifo, void* buf, int len)
{
    u8* data = MPQueue;

    u32 offset, start, end;
    if (fifo == 0)
    {
        offset = MPStatus.PacketWriteOffset;
        start = kPacketStart;
        end = kPacketEnd;
    }
    else
    {
        offset = MPStatus.ReplyWriteOffset;
        start = kReplyStart;
        end = kReplyEnd;
    }

    if ((offset + len) >= end)
    {
        u32 part1 = end - offset;
        memcpy(&data[offset], buf, part1);
        memcpy(&data[start], &((u8*)buf)[part1], len - part1);
        offset = start + len - part1;
    }
    else
    {
        memcpy(&data[offset], buf, len);
        offset += len;
    }

    if (fifo == 0) MPStatus.PacketWriteOffset = offset;
    else           MPStatus.ReplyWriteOffset = offset;
}

int SendPacketGeneric(int inst, u32 type, u8* packet, int len, u64 timestamp)
{
    if (!MPQueue) return 0;
    MPQueueLock.lock();

    u16 mask = MPStatus.ConnectedBitmask;

    // TODO: check if the FIFO is full!

    MPPacketHeader pktheader;
    pktheader.Magic = 0x4946494E;
    pktheader.SenderID = inst;
    pktheader.Type = type;
    pktheader.Length = len;
    pktheader.Timestamp = timestamp;

    type &= 0xFFFF;
    int nfifo = (type == 2) ? 1 : 0;
    FIFOWrite(inst, nfifo, &pktheader, sizeof(pktheader));
    if (len)
        FIFOWrite(inst, nfifo, packet, len);

    if (type == 1)
    {
        // NOTE: this is not guarded against, say, multiple multiplay games happening on the same machine
        // we would need to pass the packet's SenderID through the wifi module for that
        MPStatus.MPHostinst = inst;
        MPStatus.MPReplyBitmask = 0;
        ReplyReadOffset[inst] = MPStatus.ReplyWriteOffset;
        SemReset(16 + inst);
    }
    else if (type == 2)
    {
        MPStatus.MPReplyBitmask |= (1 << inst);
    }

    MPQueueLock.unlock();

    if (type == 2)
    {
        SemPost(16 + MPStatus.MPHostinst);
    }
    else
    {
        for (int i = 0; i < 16; i++)
        {
            if (mask & (1<<i))
                SemPost(i);
        }
    }

    return len;
}

int RecvPacketGeneric(int inst, u8* packet, bool block, u64* timestamp)
{
    if (!MPQueue) return 0;
    for (;;)
    {
        if (!SemWait(inst, block ? RecvTimeout : 0))
        {
            return 0;
        }

        MPQueueLock.lock();

        MPPacketHeader pktheader;
        FIFORead(inst, 0, &pktheader, sizeof(pktheader));

        if (pktheader.Magic != 0x4946494E)
        {
            Log(LogLevel::Warn, "PACKET FIFO OVERFLOW\n");
            PacketReadOffset[inst] = MPStatus.PacketWriteOffset;
            SemReset(inst);
            MPQueueLock.unlock();
            return 0;
        }

        if (pktheader.SenderID == inst)
        {
            // skip this packet
            PacketReadOffset[inst] += pktheader.Length;
            if (PacketReadOffset[inst] >= kPacketEnd)
                PacketReadOffset[inst] += kPacketStart - kPacketEnd;

            MPQueueLock.unlock();
            continue;
        }

        if (pktheader.Length)
        {
            FIFORead(inst, 0, packet, pktheader.Length);

            if (pktheader.Type == 1)
                LastHostID = pktheader.SenderID;
        }

        if (timestamp) *timestamp = pktheader.Timestamp;
        MPQueueLock.unlock();
        return pktheader.Length;
    }
}

int SendPacket(int inst, u8* packet, int len, u64 timestamp)
{
    return SendPacketGeneric(inst, 0, packet, len, timestamp);
}

int RecvPacket(int inst, u8* packet, u64* timestamp)
{
    return RecvPacketGeneric(inst, packet, false, timestamp);
}


int SendCmd(int inst, u8* packet, int len, u64 timestamp)
{
    return SendPacketGeneric(inst, 1, packet, len, timestamp);
}

int SendReply(int inst, u8* packet, int len, u64 timestamp, u16 aid)
{
    return SendPacketGeneric(inst, 2 | (aid<<16), packet, len, timestamp);
}

int SendAck(int inst, u8* packet, int len, u64 timestamp)
{
    return SendPacketGeneric(inst, 3, packet, len, timestamp);
}

int RecvHostPacket(int inst, u8* packet, u64* timestamp)
{
    if (!MPQueue) return -1;

    if (LastHostID != -1)
    {
        // check if the host is still connected

        MPQueueLock.lock();
        u8* data = MPQueue;
        u16 curinstmask = MPStatus.ConnectedBitmask;
        MPQueueLock.unlock();

        if (!(curinstmask & (1 << LastHostID)))
            return -1;
    }

    return RecvPacketGeneric(inst, packet, true, timestamp);
}

u16 RecvReplies(int inst, u8* packets, u64 timestamp, u16 aidmask)
{
    if (!MPQueue) return 0;

    u16 ret = 0;
    u16 myinstmask = (1 << inst);
    u16 curinstmask;

    {
        //MPQueueLock.lock();
        curinstmask = MPStatus.ConnectedBitmask;
        //MPQueueLock.unlock();
    }

    // if all clients have left: return early
    if ((myinstmask & curinstmask) == curinstmask)
        return 0;

    for (;;)
    {
        if (!SemWait(16+inst, RecvTimeout))
        {
            // no more replies available
            return ret;
        }

        MPQueueLock.lock();

        MPPacketHeader pktheader;
        FIFORead(inst, 1, &pktheader, sizeof(pktheader));

        if (pktheader.Magic != 0x4946494E)
        {
            Log(LogLevel::Warn, "REPLY FIFO OVERFLOW\n");
            ReplyReadOffset[inst] = MPStatus.ReplyWriteOffset;
            SemReset(16+inst);
            MPQueueLock.unlock();
            return 0;
        }

        if ((pktheader.SenderID == inst) || // packet we sent out (shouldn't happen, but hey)
            (pktheader.Timestamp < (timestamp - 32))) // stale packet
        {
            // skip this packet
            ReplyReadOffset[inst] += pktheader.Length;
            if (ReplyReadOffset[inst] >= kReplyEnd)
                ReplyReadOffset[inst] += kReplyStart - kReplyEnd;

            MPQueueLock.unlock();
            continue;
        }

        if (pktheader.Length)
        {
            u32 aid = (pktheader.Type >> 16);
            FIFORead(inst, 1, &packets[(aid-1)*1024], pktheader.Length);
            ret |= (1 << aid);
        }

        myinstmask |= (1 << pktheader.SenderID);
        if (((myinstmask & curinstmask) == curinstmask) ||
            ((ret & aidmask) == aidmask))
        {
            // all the clients have sent their reply

            MPQueueLock.unlock();
            return ret;
        }

        MPQueueLock.unlock();
    }
}

}

