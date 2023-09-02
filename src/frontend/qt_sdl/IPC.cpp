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

#include <QSharedMemory>

#include "IPC.h"
#include "Config.h"
#include "main.h"
#include "ROMManager.h"
#include "Netplay.h"
#include "NDS.h"


extern EmuThread* emuThread;


namespace IPC
{

QSharedMemory* Buffer = nullptr;
int InstanceID;

struct BufferHeader
{
    u16 NumInstances;     // total number of instances present
    u16 InstanceBitmask;  // bitmask of all instances present
    u16 ConnectedBitmask; // bitmask of which instances are ready to send/receive MP packets
    u16 ActiveBitmask;    // bitmask of which instances are currently active (ie. not halted externally)
    u32 CommandWriteOffset;
    u32 MPPacketWriteOffset;
    u32 MPReplyWriteOffset;
    u16 MPHostInstanceID; // instance ID from which the last CMD frame was sent
    u16 MPReplyBitmask;   // bitmask of which clients replied in time
};

struct CommandHeader
{
    u32 Magic;
    u16 SenderID;
    u16 Recipients;
    u16 Command;
    u16 Length;
};

struct MPPacketHeader
{
    u32 Magic;
    u32 SenderID;
    u32 Type;       // 0=regular 1=CMD 2=reply 3=ack
    u32 Length;
    u64 Timestamp;
};

u32 CommandReadOffset;
u32 MPPacketReadOffset;
u32 MPReplyReadOffset;

const u32 kBufferSize = 0x30000;
const u32 kMaxCommandSize = 0x800;
const u32 kMaxFrameSize = 0x800;
const u32 kCommandStart = sizeof(BufferHeader);
const u32 kCommandEnd = (kBufferSize / 3);
const u32 kMPPacketStart = kCommandEnd;
const u32 kMPPacketEnd = (2 * (kBufferSize / 3));
const u32 kMPReplyStart = kMPPacketEnd;
const u32 kMPReplyEnd = kBufferSize;

int MPRecvTimeout;
int MPLastHostID;

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

    HANDLE sem = CreateSemaphore(nullptr, 0, 64, semname);
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
    InstanceID = 0;

    Buffer = new QSharedMemory("melonIPC");

    if (!Buffer->attach())
    {
        printf("IPC sharedmem doesn't exist. creating\n");
        if (!Buffer->create(kBufferSize))
        {
            printf("IPC sharedmem create failed :(\n");
            delete Buffer;
            Buffer = nullptr;
            return false;
        }

        Buffer->lock();
        memset(Buffer->data(), 0, Buffer->size());
        Buffer->unlock();
    }

    Buffer->lock();
    u8* data = (u8*)Buffer->data();
    BufferHeader* header = (BufferHeader*)&data[0];

    if (header->NumInstances == 0)
    {
        // initialize the FIFOs
        header->CommandWriteOffset = kCommandStart;
        header->MPPacketWriteOffset = kMPPacketStart;
        header->MPReplyWriteOffset = kMPReplyStart;
    }

    CommandReadOffset = header->CommandWriteOffset;
    MPPacketReadOffset = header->MPPacketWriteOffset;
    MPReplyReadOffset = header->MPReplyWriteOffset;

    u16 mask = header->InstanceBitmask;
    for (int i = 0; i < 16; i++)
    {
        if (!(mask & (1<<i)))
        {
            InstanceID = i;
            header->InstanceBitmask |= (1<<i);
            header->NumInstances++;
            break;
        }
    }
    Buffer->unlock();

    MPLastHostID = -1;
    MPRecvTimeout = 25;

    printf("IPC: init OK, instance ID %d\n", InstanceID);
    return true;
}

bool InitSema()
{
    // prepare semaphores
    // semaphores 0-15: regular frames; semaphore I is posted when instance I needs to process a new frame
    // semaphores 16-31: MP replies; semaphore I is posted when instance I needs to process a new MP reply

    SemPoolInit();
    if (!SemInit(InstanceID)) return false;
    if (!SemInit(16+InstanceID)) return false;

    return true;
}

void DeInit()
{
    if (Buffer)
    {
        Buffer->lock();
        u8* data = (u8*)Buffer->data();
        BufferHeader* header = (BufferHeader*)&data[0];
        header->ConnectedBitmask &= ~(1 << InstanceID);
        header->ActiveBitmask &= ~(1 << InstanceID);
        header->InstanceBitmask &= ~(1 << InstanceID);
        header->NumInstances--;
        Buffer->unlock();

        Buffer->detach();
        delete Buffer;
    }
    Buffer = nullptr;
}

void DeInitSema()
{
    SemPoolDeinit();
}


void SetMPRecvTimeout(int timeout)
{
    MPRecvTimeout = timeout;
}

void MPBegin()
{
    Buffer->lock();
    BufferHeader* header = (BufferHeader*)Buffer->data();
    MPPacketReadOffset = header->MPPacketWriteOffset;
    MPReplyReadOffset = header->MPReplyWriteOffset;
    SemReset(InstanceID);
    SemReset(16+InstanceID);
    header->ConnectedBitmask |= (1 << InstanceID);
    Buffer->unlock();
}

void MPEnd()
{
    Buffer->lock();
    BufferHeader* header = (BufferHeader*)Buffer->data();
    //SemReset(InstanceID);
    //SemReset(16+InstanceID);
    header->ConnectedBitmask &= ~(1 << InstanceID);
    Buffer->unlock();
}

void SetActive(bool active)
{
    Buffer->lock();
    BufferHeader* header = (BufferHeader*)Buffer->data();
    if (active) header->ActiveBitmask |=  (1 << InstanceID);
    else        header->ActiveBitmask &= ~(1 << InstanceID);
    Buffer->unlock();
}

u16 GetInstanceBitmask()
{
    Buffer->lock();
    BufferHeader* header = (BufferHeader*)Buffer->data();
    u16 ret = header->InstanceBitmask;
    Buffer->unlock();
    return ret;
}


template<int fifo> void FIFORead(void* buf, int len)
{
    u8* data = (u8*)Buffer->data();

    u32 offset, start, end;
    if (fifo == 0)
    {
        offset = CommandReadOffset;
        start = kCommandStart;
        end = kCommandEnd;
    }
    else if (fifo == 1)
    {
        offset = MPPacketReadOffset;
        start = kMPPacketStart;
        end = kMPPacketEnd;
    }
    else if (fifo == 2)
    {
        offset = MPReplyReadOffset;
        start = kMPReplyStart;
        end = kMPReplyEnd;
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

    if      (fifo == 0) CommandReadOffset  = offset;
    else if (fifo == 1) MPPacketReadOffset = offset;
    else if (fifo == 2) MPReplyReadOffset  = offset;
}

template<int fifo> void FIFOWrite(void* buf, int len)
{
    u8* data = (u8*)Buffer->data();
    BufferHeader* header = (BufferHeader*)&data[0];

    u32 offset, start, end;
    if (fifo == 0)
    {
        offset = header->CommandWriteOffset;
        start = kCommandStart;
        end = kCommandEnd;
    }
    else if (fifo == 1)
    {
        offset = header->MPPacketWriteOffset;
        start = kMPPacketStart;
        end = kMPPacketEnd;
    }
    else if (fifo == 2)
    {
        offset = header->MPReplyWriteOffset;
        start = kMPReplyStart;
        end = kMPReplyEnd;
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

    if      (fifo == 0) header->CommandWriteOffset  = offset;
    else if (fifo == 1) header->MPPacketWriteOffset = offset;
    else if (fifo == 2) header->MPReplyWriteOffset  = offset;
}


void ProcessCommands()
{
    Buffer->lock();
    u8* data = (u8*)Buffer->data();
    BufferHeader* header = (BufferHeader*)&data[0];

    // check if we got new commands
    while (CommandReadOffset != header->CommandWriteOffset)
    {
        CommandHeader cmdheader;
        u8 cmddata[kMaxCommandSize];

        FIFORead<0>(&cmdheader, sizeof(cmdheader));

        if ((cmdheader.Magic != 0x4D434C4D) ||
            (cmdheader.Length > kMaxCommandSize) ||
            (cmdheader.Command >= Cmd_MAX))
        {
            printf("IPC: !!! COMMAND BUFFER IS FUCKED. RESETTING\n");
            CommandReadOffset = header->CommandWriteOffset;
            Buffer->unlock();
            return;
        }

        if (!(cmdheader.Recipients & (1<<InstanceID)))
        {
            // skip this command
            CommandReadOffset += cmdheader.Length;
            if (CommandReadOffset >= kCommandEnd)
                CommandReadOffset += kCommandStart - kCommandEnd;

        {
            // skip this command
            CommandReadOffset += cmdheader.Length;
            if (CommandReadOffset >= kCommandEnd)
                CommandReadOffset += kCommandStart - kCommandEnd;
        }

            continue;
        }

        // handle this command

        if (cmdheader.Length)
            FIFORead<0>(cmddata, cmdheader.Length);

        Buffer->unlock();

        switch (cmdheader.Command)
        {
        case Cmd_Pause:
            emuThread->IPCPause(cmddata[0] != 0);
            break;

        case Cmd_LoadROM:
            {
                u16 len = *(u16*)&cmddata[0];
                cmddata[2+len] = '\0';

                char* cstr = (char*)&cmddata[2];
                std::string str = cstr;
                ROMManager::LoadROM(QString::fromStdString(str).split('|'), true);
            }
            break;

        case Cmd_SetupNetplayMirror:
            {
                Netplay::Player* player = (Netplay::Player*)&cmddata[0];
                Netplay::StartMirror(player);
            }
            break;

        case Cmd_Start:
            Netplay::StartLocal();
            break;
        }

        Buffer->lock();
        // TODO: also what if, say, we get multiple pause commands before CommandReceived() is called?
    }

    Buffer->unlock();
}

bool SendCommand(u16 recipients, u16 command, u16 len, void* cmddata)
{
    if (command >= Cmd_MAX)
    {
        printf("IPC: invalid command %d\n", command);
        return false;
    }
    if (len && cmddata==nullptr)
    {
        printf("IPC: ????? sending command with NULL buffer\n");
        return false;
    }
    if (len > kMaxCommandSize)
    {
        printf("IPC: command too long\n");
        return false;
    }

    Buffer->lock();
    u8* data = (u8*)Buffer->data();
    BufferHeader* header = (BufferHeader*)&data[0];

    recipients &= header->InstanceBitmask;
    recipients &= ~(1<<InstanceID);
    if (!recipients)
    {
        Buffer->unlock();
        return false;
    }

    CommandHeader cmdheader;
    cmdheader.Magic = 0x4D434C4D;
    cmdheader.SenderID = InstanceID;
    cmdheader.Recipients = recipients;
    cmdheader.Command = command;
    cmdheader.Length = len;
    FIFOWrite<0>(&cmdheader, sizeof(cmdheader));
    if (len)
        FIFOWrite<0>(cmddata, len);

    Buffer->unlock();
    return true;
}

bool SendCommandU8(u16 recipients, u16 command, u8 arg)
{
    u8 data = arg;
    return SendCommand(recipients, command, 1, &data);
}

bool SendCommandStr(u16 recipients, u16 command, std::string str)
{
    u8 data[kMaxCommandSize] = {0};

    strncpy((char*)&data[2], str.c_str(), kMaxCommandSize-3);
    *(u16*)&data[0] = strlen((const char*)&data[2]) + 1;
    return SendCommand(recipients, command, 2+(*(u16*)&data[0]), data);
}


int SendMPPacketGeneric(u32 type, u8* packet, int len, u64 timestamp)
{
    Buffer->lock();
    u8* data = (u8*)Buffer->data();
    BufferHeader* header = (BufferHeader*)&data[0];

    u16 mask = header->ConnectedBitmask;

    // TODO: check if the FIFO is full!

    MPPacketHeader pktheader;
    pktheader.Magic = 0x4946494E;
    pktheader.SenderID = InstanceID;
    pktheader.Type = type;
    pktheader.Length = len;
    pktheader.Timestamp = timestamp;

    type &= 0xFFFF;

    if (type != 2)
    {
        FIFOWrite<1>(&pktheader, sizeof(pktheader));
        if (len) FIFOWrite<1>(packet, len);
    }

    if (type == 1)
    {
        // NOTE: this is not guarded against, say, multiple multiplay games happening on the same machine
        // we would need to pass the packet's SenderID through the wifi module for that
        header->MPHostInstanceID = InstanceID;
        header->MPReplyBitmask = 0;
        MPReplyReadOffset = header->MPReplyWriteOffset;
        SemReset(16 + InstanceID);
    }
    else if (type == 2)
    {
        FIFOWrite<2>(&pktheader, sizeof(pktheader));
        if (len) FIFOWrite<2>(packet, len);

        header->MPReplyBitmask |= (1 << InstanceID);
    }

    Buffer->unlock();

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

int RecvMPPacketGeneric(u8* packet, bool block, u64* timestamp)
{
    for (;;)
    {
        if (!SemWait(InstanceID, block ? MPRecvTimeout : 0))
        {
            return 0;
        }

        Buffer->lock();
        u8* data = (u8*)Buffer->data();
        BufferHeader* header = (BufferHeader*)&data[0];

        MPPacketHeader pktheader;
        FIFORead<1>(&pktheader, sizeof(pktheader));

        if (pktheader.Magic != 0x4946494E)
        {
            printf("PACKET FIFO OVERFLOW\n");
            MPPacketReadOffset = header->MPPacketWriteOffset;
            SemReset(InstanceID);
            Buffer->unlock();
            return 0;
        }

        if (pktheader.SenderID == InstanceID)
        {
            // skip this packet
            MPPacketReadOffset += pktheader.Length;
            if (MPPacketReadOffset >= kMPPacketEnd)
                MPPacketReadOffset += kMPPacketStart - kMPPacketEnd;

            Buffer->unlock();
            continue;
        }

        if (pktheader.Length)
        {
            FIFORead<1>(packet, pktheader.Length);

            if (pktheader.Type == 1)
                MPLastHostID = pktheader.SenderID;
        }

        if (timestamp) *timestamp = pktheader.Timestamp;
        Buffer->unlock();
        return pktheader.Length;
    }
}

int SendMPPacket(u8* packet, int len, u64 timestamp)
{
    return SendMPPacketGeneric(0, packet, len, timestamp);
}

int RecvMPPacket(u8* packet, u64* timestamp)
{
    return RecvMPPacketGeneric(packet, false, timestamp);
}


int SendMPCmd(u8* packet, int len, u64 timestamp)
{
    return SendMPPacketGeneric(1, packet, len, timestamp);
}

int SendMPReply(u8* packet, int len, u64 timestamp, u16 aid)
{
    return SendMPPacketGeneric(2 | (aid<<16), packet, len, timestamp);
}

int SendMPAck(u8* packet, int len, u64 timestamp)
{
    return SendMPPacketGeneric(3, packet, len, timestamp);
}

int RecvMPHostPacket(u8* packet, u64* timestamp)
{
    bool block = true;

    if (MPLastHostID != -1)
    {
        // check if the host is still connected

        Buffer->lock();
        u8* data = (u8*)Buffer->data();
        BufferHeader* header = (BufferHeader*)&data[0];
        u16 curinstmask = header->ConnectedBitmask;
        u16 actinstmask = header->ActiveBitmask;
        Buffer->unlock();

        if (!(curinstmask & (1 << MPLastHostID)))
            return -1;

        if (!(actinstmask & (1 << MPLastHostID)))
            block = false;
    }

    return RecvMPPacketGeneric(packet, block, timestamp);
}

u16 RecvMPReplies(u8* packets, u64 timestamp, u16 aidmask)
{
    u16 ret = 0;
    u16 myinstmask = (1 << InstanceID);
    u16 curinstmask, actinstmask;
    int timeout = MPRecvTimeout;

    {
        Buffer->lock();
        u8* data = (u8*)Buffer->data();
        BufferHeader* header = (BufferHeader*)&data[0];
        curinstmask = header->ConnectedBitmask;
        actinstmask = header->ActiveBitmask;
        Buffer->unlock();
    }

    // if all clients have left: return early
    if ((myinstmask & curinstmask) == curinstmask)
        return 0;

    if ((myinstmask & actinstmask) == actinstmask)
        timeout = 0;

    for (;;)
    {
        if (!SemWait(16+InstanceID, timeout))
        {
            // no more replies available
            return ret;
        }

        Buffer->lock();
        u8* data = (u8*)Buffer->data();
        BufferHeader* header = (BufferHeader*)&data[0];

        MPPacketHeader pktheader;
        FIFORead<2>(&pktheader, sizeof(pktheader));

        if (pktheader.Magic != 0x4946494E)
        {
            printf("REPLY FIFO OVERFLOW\n");
            MPReplyReadOffset = header->MPReplyWriteOffset;
            SemReset(16+InstanceID);
            Buffer->unlock();
            return 0;
        }

        if ((pktheader.SenderID == InstanceID) || // packet we sent out (shouldn't happen, but hey)
            (pktheader.Timestamp < (timestamp - 32))) // stale packet
        {
            // skip this packet
            MPReplyReadOffset += pktheader.Length;
            if (MPReplyReadOffset >= kMPReplyEnd)
                MPReplyReadOffset += kMPReplyStart - kMPReplyEnd;

            Buffer->unlock();
            continue;
        }

        if (pktheader.Length)
        {
            u32 aid = (pktheader.Type >> 16);
            FIFORead<2>(&packets[(aid-1)*1024], pktheader.Length);
            ret |= (1 << aid);
        }

        myinstmask |= (1 << pktheader.SenderID);
        if (((myinstmask & curinstmask) == curinstmask) ||
            ((ret & aidmask) == aidmask))
        {
            // all the clients have sent their reply

            Buffer->unlock();
            return ret;
        }

        Buffer->unlock();
    }
}

}
