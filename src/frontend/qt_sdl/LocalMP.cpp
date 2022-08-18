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

QSharedMemory* MPQueue;
QSystemSemaphore* MPQueueSem[16];
u32 SyncReadOffset;
u32 PacketReadOffset;

#define NIFI_VER 2



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
    int opt_true = 1;
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

    return true;

    /*MPQueue = new QSharedMemory("melonNIFI");

    if (!MPQueue->attach())
    {
        printf("sharedmem doesn't exist. creating\n");
        if (!MPQueue->create(65536))
        {
            printf("sharedmem create failed :(\n");
            return false;
        }

        MPQueue->lock();
        memset(MPQueue->data(), 0, MPQueue->size());
        MPQueueHeader* header = (MPQueueHeader*)MPQueue->data();
        header->NumInstances = 1;
        header->InstanceBitmask = 0x0001;
        header->SyncWriteOffset = 0x0010;
        header->PacketWriteOffset = 0x0100;
        MPQueue->unlock();
    }
    printf("sharedmem good\n");*/



    return true;
}

void DeInit()
{
    if (MPSocket[0] >= 0)
        closesocket(MPSocket[0]);
    if (MPSocket[1] >= 0)
        closesocket(MPSocket[1]);

#ifdef __WIN32__
    WSACleanup();
#endif // __WIN32__
}

int SendPacket(u8* data, int len)
{
    if (MPSocket[0] < 0)
        return 0;

    if (len > 2048-12)
    {
        printf("MP_SendPacket: error: packet too long (%d)\n", len);
        return 0;
    }

    *(u32*)&PacketBuffer[0] = htonl(0x4946494E); // NIFI
    PacketBuffer[4] = NIFI_VER;
    PacketBuffer[5] = 0;
    *(u16*)&PacketBuffer[6] = htons(len);
    *(u32*)&PacketBuffer[8] = MPUniqueID;
    memcpy(&PacketBuffer[12], data, len);

    _logpacket(true, data, len);

    int slen = sendto(MPSocket[0], (const char*)PacketBuffer, len+12, 0, &MPSendAddr[0], sizeof(sockaddr_t));
    if (slen < 12) return 0;
    return slen - 12;
}

int RecvPacket(u8* data, bool block)
{
    if (MPSocket[0] < 0)
        return 0;

    fd_set fd;
    struct timeval tv;

    for (;;)
    {
        FD_ZERO(&fd);
        FD_SET(MPSocket[0], &fd);
        tv.tv_sec = 0;
        //tv.tv_usec = block ? 5000 : 0;
        tv.tv_usec = block ? 500*1000 : 0;

        if (!select(MPSocket[0]+1, &fd, 0, 0, &tv))
        {
            return 0;
        }

        sockaddr_t fromAddr;
        socklen_t fromLen = sizeof(sockaddr_t);
        int rlen = recvfrom(MPSocket[0], (char*)PacketBuffer, 2048, 0, &fromAddr, &fromLen);
        if (rlen < 12+24)
        {
            continue;
        }
        rlen -= 12;

        if (ntohl(*(u32*)&PacketBuffer[0]) != 0x4946494E)
        {
            continue;
        }

        if (PacketBuffer[4] != NIFI_VER || PacketBuffer[5] != 0)
        {
            continue;
        }

        if (ntohs(*(u16*)&PacketBuffer[6]) != rlen)
        {
            continue;
        }

        if (*(u32*)&PacketBuffer[8] == MPUniqueID)
        {
            continue;
        }

        zanf = *(u16*)&PacketBuffer[12+6];
        _logpacket(false, &PacketBuffer[12], rlen);

        memcpy(data, &PacketBuffer[12], rlen);
        return rlen;
    }
}

bool SendSync(u16 clientmask, u16 type, u64 val)
{
    u8 syncbuf[32];

    if (MPSocket[1] < 0)
        return false;

    int len = 16;
    *(u32*)&syncbuf[0] = htonl(0x4946494E); // NIFI
    syncbuf[4] = NIFI_VER;
    syncbuf[5] = 1;
    *(u16*)&syncbuf[6] = htons(len);
    *(u16*)&syncbuf[8] = htons(type);
    *(u16*)&syncbuf[10] = htons(clientmask);
    *(u32*)&syncbuf[12] = MPUniqueID;
    *(u32*)&syncbuf[16] = htonl((u32)val);
    *(u32*)&syncbuf[20] = htonl((u32)(val>>32));

    int slen = sendto(MPSocket[1], (const char*)syncbuf, len+8, 0, &MPSendAddr[1], sizeof(sockaddr_t));
    return slen == len+8;
}

bool WaitSync(u16 clientmask, u16* type, u64* val)
{
    u8 syncbuf[32];

    if (MPSocket[1] < 0)
        return false;

    fd_set fd;
    struct timeval tv;

    for (;;)
    {
        FD_ZERO(&fd);
        FD_SET(MPSocket[1], &fd);
        tv.tv_sec = 0;
        tv.tv_usec = 500*1000;

        if (!select(MPSocket[1]+1, &fd, 0, 0, &tv))
        {printf("sync fail\n");
            return false;
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

        if (ntohs(*(u16*)&syncbuf[6]) != rlen)
            continue;

        if (*(u32*)&syncbuf[12] == MPUniqueID)
            continue;

        u16 clientval = ntohs(*(u16*)&syncbuf[10]);
        if (!(clientmask & clientval))
            continue;

        // check the sync val, it should be ahead of the current sync val
        u64 syncval = ntohl(*(u32*)&syncbuf[16]) | (((u64)ntohl(*(u32*)&syncbuf[20])) << 32);
        //if (syncval <= curval)
        //    continue;

        if (type) *type = ntohs(*(u16*)&syncbuf[8]);
        if (val) *val = syncval;

        return true;
    }
}

u16 WaitMultipleSyncs(u16 type, u16 clientmask, u64 curval)
{
    u8 syncbuf[32];

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

    return clientmask;
}

}

