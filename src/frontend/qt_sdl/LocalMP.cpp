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

#include "Config.h"
#include "LocalMP.h"
#include "Platform.h"

using namespace melonDS;
using namespace melonDS::Platform;

using Platform::Log;
using Platform::LogLevel;

namespace LocalMP
{

u32 MPUniqueID;
u8 PacketBuffer[2048];

struct MPQueueHeader
{
    u16 NumInstances;
    u16 InstanceBitmask;  // bitmask of all instances present
    u16 ConnectedBitmask; // bitmask of which instances are ready to send/receive packets
    u32 PacketWriteOffset;
    u32 ReplyWriteOffset;
    u16 MPHostInstanceID; // instance ID from which the last CMD frame was sent
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

struct MPSync
{
    u32 Magic;
    u32 SenderID;
    u16 ClientMask;
    u16 Type;
    u64 Timestamp;
};

QSharedMemory* MPQueue;
int InstanceID;
u32 PacketReadOffset;
u32 ReplyReadOffset;

const u32 kQueueSize = 0x20000;
const u32 kMaxFrameSize = 0x800;
const u32 kPacketStart = sizeof(MPQueueHeader);
const u32 kReplyStart = kQueueSize / 2;
const u32 kPacketEnd = kReplyStart;
const u32 kReplyEnd = kQueueSize;

int RecvTimeout;

int LastHostID;


// we need to come up with our own abstraction layer for named semaphores
// because QSystemSemaphore doesn't support waiting with a timeout
// and, as such, is unsuitable to our needs

#ifdef __WIN32__

bool SemInited[32];
HANDLE SemPool[32];

void SemPoolInit()
{
    for (int i = 0; i < 32; i++)
    {
        SemPool[i] = INVALID_HANDLE_VALUE;
        SemInited[i] = false;
    }
}

void SemDeinit(int num);

void SemPoolDeinit()
{
    for (int i = 0; i < 32; i++)
        SemDeinit(i);
}

bool SemInit(int num)
{
    if (SemInited[num])
        return true;

    char semname[64];
    sprintf(semname, "Local\\melonNIFI_Sem%02d", num);

    HANDLE sem = CreateSemaphoreA(nullptr, 0, 64, semname);
    SemPool[num] = sem;
    SemInited[num] = true;
    return sem != INVALID_HANDLE_VALUE;
}

void SemDeinit(int num)
{
    if (SemPool[num] != INVALID_HANDLE_VALUE)
    {
        CloseHandle(SemPool[num]);
        SemPool[num] = INVALID_HANDLE_VALUE;
    }

    SemInited[num] = false;
}

bool SemPost(int num)
{
    SemInit(num);
    return ReleaseSemaphore(SemPool[num], 1, nullptr) != 0;
}

bool SemWait(int num, int timeout)
{
    return WaitForSingleObject(SemPool[num], timeout) == WAIT_OBJECT_0;
}

void SemReset(int num)
{
    while (WaitForSingleObject(SemPool[num], 0) == WAIT_OBJECT_0);
}

#else

bool SemInited[32];
sem_t* SemPool[32];

void SemPoolInit()
{
    for (int i = 0; i < 32; i++)
    {
        SemPool[i] = SEM_FAILED;
        SemInited[i] = false;
    }
}

void SemDeinit(int num);

void SemPoolDeinit()
{
    for (int i = 0; i < 32; i++)
        SemDeinit(i);
}

bool SemInit(int num)
{
    if (SemInited[num])
        return true;

    char semname[64];
    sprintf(semname, "/melonNIFI_Sem%02d", num);

    sem_t* sem = sem_open(semname, O_CREAT, 0644, 0);
    SemPool[num] = sem;
    SemInited[num] = true;
    return sem != SEM_FAILED;
}

void SemDeinit(int num)
{
    if (SemPool[num] != SEM_FAILED)
    {
        sem_close(SemPool[num]);
        SemPool[num] = SEM_FAILED;
    }

    SemInited[num] = false;
}

bool SemPost(int num)
{
    SemInit(num);
    return sem_post(SemPool[num]) == 0;
}

bool SemWait(int num, int timeout)
{
    if (!timeout)
        return sem_trywait(SemPool[num]) == 0;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += timeout * 1000000;
    long sec = ts.tv_nsec / 1000000000;
    ts.tv_nsec -= sec * 1000000000;
    ts.tv_sec += sec;

    return sem_timedwait(SemPool[num], &ts) == 0;
}

void SemReset(int num)
{
    while (sem_trywait(SemPool[num]) == 0);
}

#endif


bool Init()
{
    MPQueue = new QSharedMemory("melonNIFI");

    if (!MPQueue->attach())
    {
        Log(LogLevel::Info, "MP sharedmem doesn't exist. creating\n");
        if (!MPQueue->create(kQueueSize))
        {
            Log(LogLevel::Error, "MP sharedmem create failed :( (%d)\n", MPQueue->error());
            delete MPQueue;
            MPQueue = nullptr;
            return false;
        }

        MPQueue->lock();
        memset(MPQueue->data(), 0, MPQueue->size());
        MPQueueHeader* header = (MPQueueHeader*)MPQueue->data();
        header->PacketWriteOffset = kPacketStart;
        header->ReplyWriteOffset = kReplyStart;
        MPQueue->unlock();
    }

    MPQueue->lock();
    MPQueueHeader* header = (MPQueueHeader*)MPQueue->data();

    u16 mask = header->InstanceBitmask;
    for (int i = 0; i < 16; i++)
    {
        if (!(mask & (1<<i)))
        {
            InstanceID = i;
            header->InstanceBitmask |= (1<<i);
            //header->ConnectedBitmask |= (1 << i);
            break;
        }
    }
    header->NumInstances++;

    PacketReadOffset = header->PacketWriteOffset;
    ReplyReadOffset = header->ReplyWriteOffset;

    MPQueue->unlock();

    // prepare semaphores
    // semaphores 0-15: regular frames; semaphore I is posted when instance I needs to process a new frame
    // semaphores 16-31: MP replies; semaphore I is posted when instance I needs to process a new MP reply

    SemPoolInit();
    SemInit(InstanceID);
    SemInit(16+InstanceID);

    LastHostID = -1;

    Log(LogLevel::Info, "MP comm init OK, instance ID %d\n", InstanceID);

    RecvTimeout = 25;

    return true;
}

void DeInit()
{
    if (MPQueue)
    {
        MPQueue->lock();
        if (MPQueue->data() != nullptr)
        {
            MPQueueHeader *header = (MPQueueHeader *) MPQueue->data();
            header->ConnectedBitmask &= ~(1 << InstanceID);
            header->InstanceBitmask &= ~(1 << InstanceID);
            header->NumInstances--;
        }
        MPQueue->unlock();

        SemPoolDeinit();

        MPQueue->detach();
    }

    delete MPQueue;
    MPQueue = nullptr;
}

void SetRecvTimeout(int timeout)
{
    RecvTimeout = timeout;
}

void Begin()
{
    if (!MPQueue) return;
    MPQueue->lock();
    MPQueueHeader* header = (MPQueueHeader*)MPQueue->data();
    PacketReadOffset = header->PacketWriteOffset;
    ReplyReadOffset = header->ReplyWriteOffset;
    SemReset(InstanceID);
    SemReset(16+InstanceID);
    header->ConnectedBitmask |= (1 << InstanceID);
    MPQueue->unlock();
}

void End()
{
    if (!MPQueue) return;
    MPQueue->lock();
    MPQueueHeader* header = (MPQueueHeader*)MPQueue->data();
    //SemReset(InstanceID);
    //SemReset(16+InstanceID);
    header->ConnectedBitmask &= ~(1 << InstanceID);
    MPQueue->unlock();
}

void FIFORead(int fifo, void* buf, int len)
{
    u8* data = (u8*)MPQueue->data();

    u32 offset, start, end;
    if (fifo == 0)
    {
        offset = PacketReadOffset;
        start = kPacketStart;
        end = kPacketEnd;
    }
    else
    {
        offset = ReplyReadOffset;
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

    if (fifo == 0) PacketReadOffset = offset;
    else           ReplyReadOffset = offset;
}

void FIFOWrite(int fifo, void* buf, int len)
{
    u8* data = (u8*)MPQueue->data();
    MPQueueHeader* header = (MPQueueHeader*)&data[0];

    u32 offset, start, end;
    if (fifo == 0)
    {
        offset = header->PacketWriteOffset;
        start = kPacketStart;
        end = kPacketEnd;
    }
    else
    {
        offset = header->ReplyWriteOffset;
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

    if (fifo == 0) header->PacketWriteOffset = offset;
    else           header->ReplyWriteOffset = offset;
}

int SendPacketGeneric(u32 type, u8* packet, int len, u64 timestamp)
{
    if (!MPQueue) return 0;
    MPQueue->lock();
    u8* data = (u8*)MPQueue->data();
    MPQueueHeader* header = (MPQueueHeader*)&data[0];

    u16 mask = header->ConnectedBitmask;

    // TODO: check if the FIFO is full!

    MPPacketHeader pktheader;
    pktheader.Magic = 0x4946494E;
    pktheader.SenderID = InstanceID;
    pktheader.Type = type;
    pktheader.Length = len;
    pktheader.Timestamp = timestamp;

    type &= 0xFFFF;
    int nfifo = (type == 2) ? 1 : 0;
    FIFOWrite(nfifo, &pktheader, sizeof(pktheader));
    if (len)
        FIFOWrite(nfifo, packet, len);

    if (type == 1)
    {
        // NOTE: this is not guarded against, say, multiple multiplay games happening on the same machine
        // we would need to pass the packet's SenderID through the wifi module for that
        header->MPHostInstanceID = InstanceID;
        header->MPReplyBitmask = 0;
        ReplyReadOffset = header->ReplyWriteOffset;
        SemReset(16 + InstanceID);
    }
    else if (type == 2)
    {
        header->MPReplyBitmask |= (1 << InstanceID);
    }

    MPQueue->unlock();

    if (type == 2)
    {
        SemPost(16 + header->MPHostInstanceID);
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

int RecvPacketGeneric(u8* packet, bool block, u64* timestamp)
{
    if (!MPQueue) return 0;
    for (;;)
    {
        if (!SemWait(InstanceID, block ? RecvTimeout : 0))
        {
            return 0;
        }

        MPQueue->lock();
        u8* data = (u8*)MPQueue->data();
        MPQueueHeader* header = (MPQueueHeader*)&data[0];

        MPPacketHeader pktheader;
        FIFORead(0, &pktheader, sizeof(pktheader));

        if (pktheader.Magic != 0x4946494E)
        {
            Log(LogLevel::Warn, "PACKET FIFO OVERFLOW\n");
            PacketReadOffset = header->PacketWriteOffset;
            SemReset(InstanceID);
            MPQueue->unlock();
            return 0;
        }

        if (pktheader.SenderID == InstanceID)
        {
            // skip this packet
            PacketReadOffset += pktheader.Length;
            if (PacketReadOffset >= kPacketEnd)
                PacketReadOffset += kPacketStart - kPacketEnd;

            MPQueue->unlock();
            continue;
        }

        if (pktheader.Length)
        {
            FIFORead(0, packet, pktheader.Length);

            if (pktheader.Type == 1)
                LastHostID = pktheader.SenderID;
        }

        if (timestamp) *timestamp = pktheader.Timestamp;
        MPQueue->unlock();
        return pktheader.Length;
    }
}

int SendPacket(u8* packet, int len, u64 timestamp)
{
    return SendPacketGeneric(0, packet, len, timestamp);
}

int RecvPacket(u8* packet, u64* timestamp)
{
    return RecvPacketGeneric(packet, false, timestamp);
}


int SendCmd(u8* packet, int len, u64 timestamp)
{
    return SendPacketGeneric(1, packet, len, timestamp);
}

int SendReply(u8* packet, int len, u64 timestamp, u16 aid)
{
    return SendPacketGeneric(2 | (aid<<16), packet, len, timestamp);
}

int SendAck(u8* packet, int len, u64 timestamp)
{
    return SendPacketGeneric(3, packet, len, timestamp);
}

int RecvHostPacket(u8* packet, u64* timestamp)
{
    if (!MPQueue) return -1;

    if (LastHostID != -1)
    {
        // check if the host is still connected

        MPQueue->lock();
        u8* data = (u8*)MPQueue->data();
        MPQueueHeader* header = (MPQueueHeader*)&data[0];
        u16 curinstmask = header->ConnectedBitmask;
        MPQueue->unlock();

        if (!(curinstmask & (1 << LastHostID)))
            return -1;
    }

    return RecvPacketGeneric(packet, true, timestamp);
}

u16 RecvReplies(u8* packets, u64 timestamp, u16 aidmask)
{
    if (!MPQueue) return 0;

    u16 ret = 0;
    u16 myinstmask = (1 << InstanceID);
    u16 curinstmask;

    {
        MPQueue->lock();
        u8* data = (u8*)MPQueue->data();
        MPQueueHeader* header = (MPQueueHeader*)&data[0];
        curinstmask = header->ConnectedBitmask;
        MPQueue->unlock();
    }

    // if all clients have left: return early
    if ((myinstmask & curinstmask) == curinstmask)
        return 0;

    for (;;)
    {
        if (!SemWait(16+InstanceID, RecvTimeout))
        {
            // no more replies available
            return ret;
        }

        MPQueue->lock();
        u8* data = (u8*)MPQueue->data();
        MPQueueHeader* header = (MPQueueHeader*)&data[0];

        MPPacketHeader pktheader;
        FIFORead(1, &pktheader, sizeof(pktheader));

        if (pktheader.Magic != 0x4946494E)
        {
            Log(LogLevel::Warn, "REPLY FIFO OVERFLOW\n");
            ReplyReadOffset = header->ReplyWriteOffset;
            SemReset(16+InstanceID);
            MPQueue->unlock();
            return 0;
        }

        if ((pktheader.SenderID == InstanceID) || // packet we sent out (shouldn't happen, but hey)
            (pktheader.Timestamp < (timestamp - 32))) // stale packet
        {
            // skip this packet
            ReplyReadOffset += pktheader.Length;
            if (ReplyReadOffset >= kReplyEnd)
                ReplyReadOffset += kReplyStart - kReplyEnd;

            MPQueue->unlock();
            continue;
        }

        if (pktheader.Length)
        {
            u32 aid = (pktheader.Type >> 16);
            FIFORead(1, &packets[(aid-1)*1024], pktheader.Length);
            ret |= (1 << aid);
        }

        myinstmask |= (1 << pktheader.SenderID);
        if (((myinstmask & curinstmask) == curinstmask) ||
            ((ret & aidmask) == aidmask))
        {
            // all the clients have sent their reply

            MPQueue->unlock();
            return ret;
        }

        MPQueue->unlock();
    }
}

}

