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
    #define NTDDI_VERSION        0x06000000 // GROSS FUCKING HACK
    #include <winsock2.h>
    #include <windows.h>
    //#include <knownfolders.h> // FUCK THAT SHIT
    #include <shlobj.h>
    #include <ws2tcpip.h>
    #include <io.h>
    #define dup _dup
    #define socket_t    SOCKET
    #define sockaddr_t  SOCKADDR
#else
    #include <unistd.h>
    #include <netinet/in.h>
    #include <sys/select.h>
    #include <sys/socket.h>

    #define socket_t    int
    #define sockaddr_t  struct sockaddr
    #define closesocket close
#endif

#include <string>
#include <QSharedMemory>
#include <QSystemSemaphore>

#include "Config.h"
#include "LocalMP.h"
#include "SPI.h"


#ifndef INVALID_SOCKET
    #define INVALID_SOCKET  (socket_t)-1
#endif

extern u16 zanf;

namespace LocalMP
{

u32 MPUniqueID;
socket_t MPSocket[2];
sockaddr_t MPSendAddr[2];
u8 PacketBuffer[2048];

struct MPQueueHeader
{
    u16 NumInstances;
    u16 InstanceBitmask;
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
//QSystemSemaphore* MPQueueSem[16];
int InstanceID;
u32 PacketReadOffset;
u32 ReplyReadOffset;

const u32 kQueueSize = 0x20000;
const u32 kMaxFrameSize = 0x800;
const u32 kPacketStart = 0x00010;
const u32 kReplyStart = kQueueSize / 2;
const u32 kPacketEnd = kReplyStart;
const u32 kReplyEnd = kQueueSize;

const int RecvTimeout = 500;

#define NIFI_VER 2


// we need to come up with our own abstraction layer for named semaphores
// because QSystemSemaphore doesn't support waiting with a timeout
// and, as such, is unsuitable to our needs

//#ifdef _WIN32
#if 1

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

/*bool SemWaitMultiple(int start, u16 bitmask, int timeout)
{
    HANDLE semlist[16];
    int numsem = 0;

    for (int i = 0; i < 16; i++)
    {
        if (bitmask & (1<<i))
        {
            SemInit(start+i);
            semlist[numsem] = SemPool[start+i];
            numsem++;
        }
    }

    DWORD res = WaitForMultipleObjects(numsem, semlist, TRUE, timeout);
    return (res >= WAIT_OBJECT_0) && (res < (WAIT_OBJECT_0+numsem));
}*/

#else

// TODO: code semaphore shit for other platforms!

#endif // _WIN32


void _logpacket(bool tx, u8* data, int len, u64 ts)
{//return;
    char path[256];
    sprintf(path, "framelog_%08X.log", InstanceID);
    static FILE* f = nullptr;
    if (!f) f = fopen(path, "a");

    /*fprintf(f, "---- %s PACKET LEN=%d ----\n", tx?"SENDING":"RECEIVING", len);

    for (int y = 0; y < len; y+=16)
    {
        fprintf(f, "%04X: ", y);
        int linelen = 16;
        if ((y+linelen) > len) linelen = len-y;
        for (int x = 0; x < linelen; x++)
        {
            fprintf(f, " %02X", data[y+x]);
        }
        fprintf(f, "\n");
    }

    fprintf(f, "-------------------------------------\n\n\n");*/

    fprintf(f, "[%016llX] %s PACKET: LEN=%0.4d FC=%04X SN=%04X CL=%04X/%04X\n", ts, tx?"TX":"RX",
            len, *(u16*)&data[12], *(u16*)&data[12+22], *(u16*)&data[12+24], *(u16*)&data[12+26]);
    fflush(f);
}

void _logstring(u64 ts, char* str)
{//return;
    char path[256];
    sprintf(path, "framelog_%08X.log", InstanceID);
    static FILE* f = nullptr;
    if (!f) f = fopen(path, "a");

    fprintf(f, "[%016llX] %s\n", ts, str);
    fflush(f);
}

void _logstring2(u64 ts, char* str, u32 arg, u64 arg2)
{//return;
    char path[256];
    sprintf(path, "framelog_%08X.log", InstanceID);
    static FILE* f = nullptr;
    if (!f) f = fopen(path, "a");

    fprintf(f, "[%016llX] %s %08X %016llX\n", ts, str, arg, arg2);
    fflush(f);
}

bool Init()
{
    /*int opt_true = 1;
    int res0, res1;

#ifdef __WIN32__
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
    {
        return false;
    }
#endif // __WIN32__

    MPSocket[0] = socket(AF_INET, SOCK_DGRAM, 0);
    if (MPSocket[0] < 0)
    {
        return false;
    }

    MPSocket[1] = socket(AF_INET, SOCK_DGRAM, 0);
    if (MPSocket[1] < 0)
    {
        closesocket(MPSocket[0]);
        MPSocket[0] = INVALID_SOCKET;
        return false;
    }

    res0 = setsockopt(MPSocket[0], SOL_SOCKET, SO_REUSEADDR, (const char*)&opt_true, sizeof(int));
    res1 = setsockopt(MPSocket[1], SOL_SOCKET, SO_REUSEADDR, (const char*)&opt_true, sizeof(int));
    if (res0 < 0 || res1 < 0)
    {
        closesocket(MPSocket[0]);
        MPSocket[0] = INVALID_SOCKET;
        closesocket(MPSocket[1]);
        MPSocket[1] = INVALID_SOCKET;
        return false;
    }

#if defined(BSD) || defined(__APPLE__)
    res0 = setsockopt(MPSocket[0], SOL_SOCKET, SO_REUSEPORT, (const char*)&opt_true, sizeof(int));
    res0 = setsockopt(MPSocket[1], SOL_SOCKET, SO_REUSEPORT, (const char*)&opt_true, sizeof(int));
    if (res0 < 0 || res1 < 0)
    {
        closesocket(MPSocket[0]);
        MPSocket[0] = INVALID_SOCKET;
        closesocket(MPSocket[1]);
        MPSocket[1] = INVALID_SOCKET;
        return false;
    }
#endif

    sockaddr_t saddr;
    saddr.sa_family = AF_INET;
    *(u32*)&saddr.sa_data[2] = htonl(Config::SocketBindAnyAddr ? INADDR_ANY : INADDR_LOOPBACK);
    *(u16*)&saddr.sa_data[0] = htons(7064);
    res0 = bind(MPSocket[0], &saddr, sizeof(sockaddr_t));
    *(u16*)&saddr.sa_data[0] = htons(7065);
    res1 = bind(MPSocket[1], &saddr, sizeof(sockaddr_t));
    if (res0 < 0 || res1 < 0)
    {
        closesocket(MPSocket[0]);
        MPSocket[0] = INVALID_SOCKET;
        closesocket(MPSocket[1]);
        MPSocket[1] = INVALID_SOCKET;
        return false;
    }

    res0 = setsockopt(MPSocket[0], SOL_SOCKET, SO_BROADCAST, (const char*)&opt_true, sizeof(int));
    res1 = setsockopt(MPSocket[1], SOL_SOCKET, SO_BROADCAST, (const char*)&opt_true, sizeof(int));
    if (res0 < 0 || res1 < 0)
    {
        closesocket(MPSocket[0]);
        MPSocket[0] = INVALID_SOCKET;
        closesocket(MPSocket[1]);
        MPSocket[1] = INVALID_SOCKET;
        return false;
    }

    MPSendAddr[0].sa_family = AF_INET;
    *(u32*)&MPSendAddr[0].sa_data[2] = htonl(INADDR_BROADCAST);
    *(u16*)&MPSendAddr[0].sa_data[0] = htons(7064);

    MPSendAddr[1].sa_family = AF_INET;
    *(u32*)&MPSendAddr[1].sa_data[2] = htonl(INADDR_BROADCAST);
    *(u16*)&MPSendAddr[1].sa_data[0] = htons(7065);

    u8* mac = SPI_Firmware::GetWifiMAC();
    MPUniqueID = *(u32*)&mac[0];
    MPUniqueID ^= *(u32*)&mac[2];
    printf("local MP unique ID: %08X\n", MPUniqueID);

    return true;*/

    /*u8* mac = SPI_Firmware::GetWifiMAC();
    MPUniqueID = *(u32*)&mac[0];
    MPUniqueID ^= *(u32*)&mac[2];
    printf("local MP unique ID: %08X\n", MPUniqueID);*/

    MPQueue = new QSharedMemory("melonNIFI_FIFO");

    if (!MPQueue->attach())
    {
        printf("MP sharedmem doesn't exist. creating\n");
        if (!MPQueue->create(kQueueSize))
        {
            printf("MP sharedmem create failed :(\n");
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
            break;
        }
    }
    header->NumInstances++;

    PacketReadOffset = header->PacketWriteOffset;
    ReplyReadOffset = header->ReplyWriteOffset;

    MPQueue->unlock();

    /*for (int i = 0; i < 16; i++)
    {
        QString key = QString("melonSEMA%1").arg(i);
        MPQueueSem[i] = new QSystemSemaphore(key, 0, (i==InstanceID) ? QSystemSemaphore::Create : QSystemSemaphore::Open)
    }*/

    // prepare semaphores
    // semaphores 0-15: regular frames; semaphore I is posted when instance I needs to process a new frame
    // semaphores 16-31: MP replies; semaphore I is posted by instance I when it sends a MP reply

    SemPoolInit();
    SemInit(InstanceID);
    SemInit(16+InstanceID);

    printf("MP comm init OK, instance ID %d\n", InstanceID);

    return true;
}

void DeInit()
{
    /*if (MPSocket[0] >= 0)
        closesocket(MPSocket[0]);
    if (MPSocket[1] >= 0)
        closesocket(MPSocket[1]);

#ifdef __WIN32__
    WSACleanup();
#endif // __WIN32__*/
    //SemDeinit(InstanceID);
    //SemDeinit(16+InstanceID);
    MPQueue->lock();
    MPQueueHeader* header = (MPQueueHeader*)MPQueue->data();
    header->InstanceBitmask &= ~(1 << InstanceID);
    MPQueue->unlock();

    SemPoolDeinit();

    MPQueue->detach();
    delete MPQueue;
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
    MPQueue->lock();
    u8* data = (u8*)MPQueue->data();
    MPQueueHeader* header = (MPQueueHeader*)&data[0];

    u16 mask = header->InstanceBitmask;

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

int SendPacket(u8* packet, int len, u64 timestamp)
{
    //_logpacket(true, packet, len, timestamp);
    return SendPacketGeneric(0, packet, len, timestamp);
}

int RecvPacket(u8* packet, bool block, u64* timestamp)
{
    for (;;)
    {
        if (!SemWait(InstanceID, block ? RecvTimeout : 0))
        {
            return 0;
        }

        MPQueue->lock();
        u8* data = (u8*)MPQueue->data();

        MPPacketHeader pktheader;
        FIFORead(0, &pktheader, sizeof(pktheader));

        if (pktheader.Magic != 0x4946494E)
        {
            printf("MP: !!!! PACKET FIFO IS CRAPOED\n");
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
            FIFORead(0, packet, pktheader.Length);
        //_logpacket(false, packet, pktheader.Length, pktheader.Timestamp);
        if (timestamp) *timestamp = pktheader.Timestamp;
        MPQueue->unlock();
        return pktheader.Length;
    }
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

u16 RecvReplies(u8* packets, u64 timestamp, u16 aidmask)
{
    u16 ret = 0;
    u16 instmask = (1 << InstanceID);

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
            printf("MP: !!!! REPLY FIFO IS CRAPOED\n");
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

        instmask |= (1 << pktheader.SenderID);
        if ((instmask & header->InstanceBitmask) == header->InstanceBitmask)
        {
            // all the clients have sent their reply

            MPQueue->unlock();
            return ret;
        }

        MPQueue->unlock();
    }
}

}

