/*
    Copyright 2016-2019 StapleButter

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
#include <semaphore.h>
#include <pthread.h>
#include "../pcap/pcap.h"
#include "../Platform.h"
#include "MelonDS.h"
#include "PlatformConfig.h"
#include "LAN_Socket.h"
#include "LAN_PCap.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#define socket_t    int
#define sockaddr_t  struct sockaddr
#define closesocket close

#ifndef INVALID_SOCKET
#define INVALID_SOCKET  (socket_t)-1
#endif

extern char* EmuDirectory;

void Stop(bool internal);

namespace Platform
{
    typedef struct
    {
        int val;
        pthread_mutex_t mutex;
        pthread_cond_t cond;
    } Semaphore;

    typedef struct
    {
        pthread_t ID;
        void (*Func)();

    } ThreadData;

    void* ThreadEntry(void* data)
    {
        ThreadData* thread = (ThreadData*)data;
        thread->Func();
        return NULL;
    }


    socket_t MPSocket;
    sockaddr_t MPSendAddr;
    u8 PacketBuffer[2048];

#define NIFI_VER 1


    const char* PCapLibNames[] =
            {
                    // TODO: Linux lib names
                    NULL
            };

    void* PCapLib = NULL;
    pcap_t* PCapAdapter = NULL;

    u8 PCapPacketBuffer[2048];
    int PCapPacketLen;
    int PCapRXNum;


    void StopEmu()
    {
        Stop(true);
    }

    FILE* OpenFile(const char* path, const char* mode, bool mustexist)
    {
        FILE* file = fopen(path, mode);

        if (file)
            return file;

        return NULL;
    }

    FILE* OpenLocalFile(const char* path, const char* mode)
    {
        const char* configDir = MelonDSAndroid::configDir;

        size_t configDirLength = strlen(configDir);
        size_t configFileLength = configDirLength + strlen(path);
        char* configFile = new char[configFileLength + 1];
        strcpy(configFile, configDir);
        strcpy(&configFile[configDirLength], path);
        configFile[configFileLength] = '\0';

        FILE* file = OpenFile(configFile, mode, false);
        delete[] configFile;
        return file;
    }

    void* Thread_Create(void (*func)())
    {
        ThreadData* data = new ThreadData;
        data->Func = func;
        pthread_create(&data->ID, 0, ThreadEntry, data);
        return data;
    }

    void Thread_Free(void* thread)
    {
        delete (ThreadData*)thread;
    }

    void Thread_Wait(void* thread)
    {
        pthread_t pthread = ((ThreadData*) thread)->ID;
        pthread_join(pthread, NULL);
    }

    void* Semaphore_Create()
    {
        Semaphore* semaphore = (Semaphore*) malloc(sizeof(Semaphore));
        pthread_mutex_init(&semaphore->mutex, NULL);
        pthread_cond_init(&semaphore->cond, NULL);
        return semaphore;
    }

    void Semaphore_Free(void* sema)
    {
        Semaphore* semaphore = (Semaphore*) sema;
        pthread_mutex_destroy(&semaphore->mutex);
        pthread_cond_destroy(&semaphore->cond);
        free(semaphore);
    }

    int Semaphore_TryWait(void* sema)
    {
        // TODO: is this properly implemented? everything seems to work OK so far

        Semaphore* semaphore = (Semaphore*) sema;
        pthread_mutex_lock(&semaphore->mutex);
        if (semaphore->val == 0) {
            pthread_mutex_unlock(&semaphore->mutex);
            return -1;
        }

        pthread_cond_wait(&semaphore->cond, &semaphore->mutex);
        semaphore->val--;
        pthread_mutex_unlock(&semaphore->mutex);
        return 0;
    }

    void Semaphore_Reset(void* sema)
    {
        while (Semaphore_TryWait(sema) == 0);
    }

    void Semaphore_Wait(void* sema)
    {
        Semaphore* semaphore = (Semaphore*) sema;
        pthread_mutex_lock(&semaphore->mutex);
        while (semaphore->val == 0)
            pthread_cond_wait(&semaphore->cond, &semaphore->mutex);

        semaphore->val--;
        pthread_mutex_unlock(&semaphore->mutex);
    }

    void Semaphore_Post(void* sema)
    {
        Semaphore* semaphore = (Semaphore*) sema;
        pthread_mutex_lock(&semaphore->mutex);
        semaphore->val++;
        pthread_cond_broadcast(&semaphore->cond);
        pthread_mutex_unlock(&semaphore->mutex);
    }


    bool MP_Init()
    {
        int opt_true = 1;
        int res;

        MPSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (MPSocket < 0)
        {
            return false;
        }

        res = setsockopt(MPSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt_true, sizeof(int));
        if (res < 0)
        {
            closesocket(MPSocket);
            MPSocket = INVALID_SOCKET;
            return false;
        }

        sockaddr_t saddr;
        saddr.sa_family = AF_INET;
        *(u32*)&saddr.sa_data[2] = htonl(Config::SocketBindAnyAddr ? INADDR_ANY : INADDR_LOOPBACK);
        *(u16*)&saddr.sa_data[0] = htons(7064);
        res = bind(MPSocket, &saddr, sizeof(sockaddr_t));
        if (res < 0)
        {
            closesocket(MPSocket);
            MPSocket = INVALID_SOCKET;
            return false;
        }

        res = setsockopt(MPSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&opt_true, sizeof(int));
        if (res < 0)
        {
            closesocket(MPSocket);
            MPSocket = INVALID_SOCKET;
            return false;
        }

        MPSendAddr.sa_family = AF_INET;
        *(u32*)&MPSendAddr.sa_data[2] = htonl(INADDR_BROADCAST);
        *(u16*)&MPSendAddr.sa_data[0] = htons(7064);

        return true;
    }

    void MP_DeInit()
    {
        if (MPSocket >= 0)
            closesocket(MPSocket);
    }

    int MP_SendPacket(u8* data, int len)
    {
        if (MPSocket < 0)
            return 0;

        if (len > 2048-8)
        {
            printf("MP_SendPacket: error: packet too long (%d)\n", len);
            return 0;
        }

        *(u32*)&PacketBuffer[0] = htonl(0x4946494E); // NIFI
        PacketBuffer[4] = NIFI_VER;
        PacketBuffer[5] = 0;
        *(u16*)&PacketBuffer[6] = htons(len);
        memcpy(&PacketBuffer[8], data, len);

        int slen = sendto(MPSocket, (const char*)PacketBuffer, len+8, 0, &MPSendAddr, sizeof(sockaddr_t));
        if (slen < 8) return 0;
        return slen - 8;
    }

    int MP_RecvPacket(u8* data, bool block)
    {
        if (MPSocket < 0)
            return 0;

        fd_set fd;
        struct timeval tv;

        FD_ZERO(&fd);
        FD_SET(MPSocket, &fd);
        tv.tv_sec = 0;
        tv.tv_usec = block ? 5000 : 0;

        if (!select(MPSocket+1, &fd, 0, 0, &tv))
        {
            return 0;
        }

        sockaddr_t fromAddr;
        socklen_t fromLen = sizeof(sockaddr_t);
        int rlen = recvfrom(MPSocket, (char*)PacketBuffer, 2048, 0, &fromAddr, &fromLen);
        if (rlen < 8+24)
        {
            return 0;
        }
        rlen -= 8;

        if (ntohl(*(u32*)&PacketBuffer[0]) != 0x4946494E)
        {
            return 0;
        }

        if (PacketBuffer[4] != NIFI_VER)
        {
            return 0;
        }

        if (ntohs(*(u16*)&PacketBuffer[6]) != rlen)
        {
            return 0;
        }

        memcpy(data, &PacketBuffer[8], rlen);
        return rlen;
    }



    bool LAN_Init()
    {
        if (Config::DirectLAN)
        {
            if (!LAN_PCap::Init(true))
                return false;
        }
        else
        {
            if (!LAN_Socket::Init())
                return false;
        }

        return true;
    }

    void LAN_DeInit()
    {
        // checkme. blarg
        //if (Config::DirectLAN)
        //    LAN_PCap::DeInit();
        //else
        //    LAN_Socket::DeInit();
        LAN_PCap::DeInit();
        LAN_Socket::DeInit();
    }

    int LAN_SendPacket(u8* data, int len)
    {
        if (Config::DirectLAN)
            return LAN_PCap::SendPacket(data, len);
        else
            return LAN_Socket::SendPacket(data, len);
    }

    int LAN_RecvPacket(u8* data)
    {
        if (Config::DirectLAN)
            return LAN_PCap::RecvPacket(data);
        else
            return LAN_Socket::RecvPacket(data);
    }
}
