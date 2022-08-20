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
    u32 SyncWriteOffset;
    u32 PacketWriteOffset;
};

struct MPPacketHeader
{
    u32 Magic;
    u32 SenderID;
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
u32 SyncReadOffset;
u32 PacketReadOffset;

const u32 kSyncStart = 0x0010;
const u32 kSyncEnd = 0x0100;
const u32 kPacketStart = 0x0100;
const u32 kPacketEnd = 0x10000;

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
        SemInited[num] = false;
    }
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

#else

// TODO: code semaphore shit for other platforms!

#endif // _WIN32


void _logpacket(bool tx, u8* data, int len)
{return;
    char path[256];
    sprintf(path, "framelog_%08X.log", MPUniqueID);
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

    fprintf(f, "%s PACKET: LEN=%0.4d FC=%04X SN=%04X CL=%04X/%04X\n", tx?"TX":"RX",
            len, *(u16*)&data[12], *(u16*)&data[12+22], *(u16*)&data[12+24], *(u16*)&data[12+26]);
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
        if (!MPQueue->create(65536))
        {
            printf("MP sharedmem create failed :(\n");
            return false;
        }

        MPQueue->lock();
        memset(MPQueue->data(), 0, MPQueue->size());
        MPQueueHeader* header = (MPQueueHeader*)MPQueue->data();
        header->SyncWriteOffset = kSyncStart;
        header->PacketWriteOffset = kPacketStart;
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

    SyncReadOffset = header->SyncWriteOffset;
    PacketReadOffset = header->PacketWriteOffset;

    MPQueue->unlock();

    /*for (int i = 0; i < 16; i++)
    {
        QString key = QString("melonSEMA%1").arg(i);
        MPQueueSem[i] = new QSystemSemaphore(key, 0, (i==InstanceID) ? QSystemSemaphore::Create : QSystemSemaphore::Open)
    }*/

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
    SemDeinit(InstanceID);
    SemDeinit(16+InstanceID);
}

void PacketFIFORead(void* buf, int len)
{
    u8* data = (u8*)MPQueue->data();

    u32 offset = PacketReadOffset;
    if ((offset + len) >= kPacketEnd)
    {
        u32 part1 = kPacketEnd - offset;
        memcpy(buf, &data[offset], part1);
        memcpy(&((u8*)buf)[part1], &data[kPacketStart], len - part1);
        offset = kPacketStart + len - part1;
    }
    else
    {
        memcpy(buf, &data[offset], len);
        offset += len;
    }

    PacketReadOffset = offset;
}

void PacketFIFOWrite(void* buf, int len)
{
    u8* data = (u8*)MPQueue->data();
    MPQueueHeader* header = (MPQueueHeader*)&data[0];

    u32 offset = header->PacketWriteOffset;
    if ((offset + len) >= kPacketEnd)
    {
        u32 part1 = kPacketEnd - offset;
        memcpy(&data[offset], buf, part1);
        memcpy(&data[kPacketStart], &((u8*)buf)[part1], len - part1);
        offset = kPacketStart + len - part1;
    }
    else
    {
        memcpy(&data[offset], buf, len);
        offset += len;
    }

    header->PacketWriteOffset = offset;
}

int SendPacket(u8* packet, int len, u64 timestamp)
{
    MPQueue->lock();
    u8* data = (u8*)MPQueue->data();
    MPQueueHeader* header = (MPQueueHeader*)&data[0];

    u16 mask = header->InstanceBitmask;

    // TODO: check if the FIFO is full!

    MPPacketHeader pktheader;
    pktheader.Magic = 0x4946494E;
    pktheader.SenderID = InstanceID;
    pktheader.Length = len;
    pktheader.Timestamp = timestamp;

    PacketFIFOWrite(&pktheader, sizeof(pktheader));
    PacketFIFOWrite(packet, len);

    MPQueue->unlock();

    for (int i = 0; i < 16; i++)
    {
        if (mask & (1<<i))
            SemPost(i);
    }

    return len;
}

int RecvPacket(u8* packet, bool block, u64* timestamp)
{
    for (;;)
    {
        if (!SemWait(InstanceID, block ? 500 : 0))
        {
            return 0;
        }

        MPQueue->lock();
        u8* data = (u8*)MPQueue->data();

        MPPacketHeader pktheader;
        PacketFIFORead(&pktheader, sizeof(pktheader));

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

        PacketFIFORead(packet, pktheader.Length);
        if (timestamp) *timestamp = pktheader.Timestamp;
        MPQueue->unlock();
        return pktheader.Length;
    }
}


void SyncFIFORead(MPSync* sync)
{
    u8* data = (u8*)MPQueue->data();

    int len = sizeof(MPSync);
    u32 offset = SyncReadOffset;
    if ((offset + len) >= kSyncEnd)
    {
        u32 part1 = kSyncEnd - offset;
        memcpy(sync, &data[offset], part1);
        memcpy(&((u8*)sync)[part1], &data[kSyncStart], len - part1);
        offset = kSyncStart + len - part1;
    }
    else
    {
        memcpy(sync, &data[offset], len);
        offset += len;
    }

    SyncReadOffset = offset;
}

void SyncFIFOWrite(MPSync* sync)
{
    u8* data = (u8*)MPQueue->data();
    MPQueueHeader* header = (MPQueueHeader*)&data[0];

    int len = sizeof(MPSync);
    u32 offset = header->SyncWriteOffset;
    if ((offset + len) >= kSyncEnd)
    {
        u32 part1 = kSyncEnd - offset;
        memcpy(&data[offset], sync, part1);
        memcpy(&data[kSyncStart], &((u8*)sync)[part1], len - part1);
        offset = kSyncStart + len - part1;
    }
    else
    {
        memcpy(&data[offset], sync, len);
        offset += len;
    }

    header->SyncWriteOffset = offset;
}

bool SendSync(u16 clientmask, u16 type, u64 timestamp)
{
    MPQueue->lock();
    u8* data = (u8*)MPQueue->data();
    MPQueueHeader* header = (MPQueueHeader*)&data[0];

    u16 mask = header->InstanceBitmask;

    // TODO: check if the FIFO is full!

    MPSync sync;
    sync.Magic = 0x434E5953;
    sync.SenderID = InstanceID;
    sync.ClientMask = clientmask;
    sync.Type = type;
    sync.Timestamp = timestamp;

    SyncFIFOWrite(&sync);

    MPQueue->unlock();

    for (int i = 0; i < 16; i++)
    {
        if (mask & (1<<i))
            SemPost(16+i);
    }

    return true;
}

bool WaitSync(u16 clientmask, u16* type, u64* timestamp)
{
    for (;;)
    {
        if (!SemWait(16+InstanceID, 500))
        {
            return false;
        }

        MPQueue->lock();
        u8* data = (u8*)MPQueue->data();

        MPSync sync;
        SyncFIFORead(&sync);

        if (sync.Magic != 0x434E5953)
        {
            printf("MP: !!!! SYNC FIFO IS CRAPOED\n");
            MPQueue->unlock();
            return false;
        }

        if (sync.SenderID == InstanceID)
        {
            MPQueue->unlock();
            continue;
        }

        if (!(sync.ClientMask & clientmask))
        {
            MPQueue->unlock();
            continue;
        }

        if (type) *type = sync.Type;
        if (timestamp) *timestamp = sync.Timestamp;
        MPQueue->unlock();
        return true;
    }
}

u16 WaitMultipleSyncs(u16 type, u16 clientmask, u64 curval)
{
    /*u8 syncbuf[32];

    if (!clientmask)
        return 0;

    if (MPSocket[1] < 0)
        return 0;

    fd_set fd;
    struct timeval tv;

    for (;;)
    {
        FD_ZERO(&fd);
        FD_SET(MPSocket[1], &fd);
        tv.tv_sec = 0;
        tv.tv_usec = 500*1000;

        if (!select(MPSocket[1]+1, &fd, 0, 0, &tv))
        {printf("[sync3] nope :(\n");
            return clientmask;
        }

        sockaddr_t fromAddr;
        socklen_t fromLen = sizeof(sockaddr_t);
        int rlen = recvfrom(MPSocket[1], (char*)syncbuf, 32, 0, &fromAddr, &fromLen);
        if (rlen != 8+16)
            continue;
        rlen -= 8;

        if (ntohl(*(u32*)&syncbuf[0]) != 0x4946494E)
            continue;

        if (syncbuf[4] != NIFI_VER || syncbuf[5] != 1)
            continue;
//printf("[sync3] atleast header is good\n");
        if (ntohs(*(u16*)&syncbuf[6]) != rlen)
            continue;

        if (*(u32*)&syncbuf[12] == MPUniqueID)
            continue;

        if (ntohs(*(u16*)&syncbuf[8]) != type)
            continue;

        u16 clientval = ntohs(*(u16*)&syncbuf[10]);
        //printf("[sync3] good rlen/type %04X %04X, clientmask=%04X \n", ntohs(*(u16*)&syncbuf[6]), ntohs(*(u16*)&syncbuf[8]), clientval);
        if (!(clientmask & clientval))
            continue;

        // check the sync val, it should be ahead of the current sync val
        u64 syncval = ntohl(*(u32*)&syncbuf[12]) | (((u64)ntohl(*(u32*)&syncbuf[16])) << 32);
        //if (syncval <= curval)
        //    continue;
//printf("[sync3] good\n");
        clientmask &= ~clientval;
        //if (!clientmask)
            return 0;
    }

    return clientmask;*/
    return 0;
}

}

