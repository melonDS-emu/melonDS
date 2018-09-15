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
#include <SDL2/SDL.h>
#include <pcap/pcap.h>
#include "../Platform.h"
#include "../Config.h"

#ifdef __WIN32__
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define socket_t    SOCKET
	#define sockaddr_t  SOCKADDR
#else
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <sys/select.h>
	#include <sys/socket.h>
	#define socket_t    int
	#define sockaddr_t  struct sockaddr
	#define closesocket close
	#define PCAP_OPENFLAG_PROMISCUOUS 1
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET  (socket_t)-1
#endif


#define DECL_PCAP_FUNC(ret, name, args, args2) \
    typedef ret (*type_##name) args; \
    type_##name ptr_##name = NULL; \
    ret name args { return ptr_##name args2; }

DECL_PCAP_FUNC(int, pcap_findalldevs, (pcap_if_t** alldevs, char* errbuf), (alldevs,errbuf))
DECL_PCAP_FUNC(void, pcap_freealldevs, (pcap_if_t* alldevs), (alldevs))
DECL_PCAP_FUNC(pcap_t*, pcap_open_live, (const char* src, int snaplen, int flags, int readtimeout, char* errbuf), (src,snaplen,flags,readtimeout,errbuf))
DECL_PCAP_FUNC(void, pcap_close, (pcap_t* dev), (dev))
DECL_PCAP_FUNC(int, pcap_setnonblock, (pcap_t* dev, int nonblock, char* errbuf), (dev,nonblock,errbuf))
DECL_PCAP_FUNC(int, pcap_sendpacket, (pcap_t* dev, const u_char* data, int len), (dev,data,len))
DECL_PCAP_FUNC(int, pcap_dispatch, (pcap_t* dev, int num, pcap_handler callback, u_char* data), (dev,num,callback,data))


void Stop(bool internal);


namespace Platform
{


typedef struct
{
    SDL_Thread* ID;
    void (*Func)();

} ThreadData;

int ThreadEntry(void* data)
{
    ThreadData* thread = (ThreadData*)data;
    thread->Func();
    return 0;
}


socket_t MPSocket;
sockaddr_t MPSendAddr;
u8 PacketBuffer[2048];

#define NIFI_VER 1


const char* PCapLibNames[] =
{
#ifdef __WIN32__
    // TODO: name for npcap in non-WinPCap mode
    "wpcap.dll",
#else
    // TODO: Linux lib names
#endif
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


void* Thread_Create(void (*func)())
{
    ThreadData* data = new ThreadData;
    data->Func = func;
    data->ID = SDL_CreateThread(ThreadEntry, "melonDS core thread", data);
    return data;
}

void Thread_Free(void* thread)
{
    delete (ThreadData*)thread;
}

void Thread_Wait(void* thread)
{
    SDL_WaitThread((SDL_Thread*)((ThreadData*)thread)->ID, NULL);
}


void* Semaphore_Create()
{
    return SDL_CreateSemaphore(0);
}

void Semaphore_Free(void* sema)
{
    SDL_DestroySemaphore((SDL_sem*)sema);
}

void Semaphore_Reset(void* sema)
{
    while (SDL_SemTryWait((SDL_sem*)sema) == 0);
}

void Semaphore_Wait(void* sema)
{
    SDL_SemWait((SDL_sem*)sema);
}

void Semaphore_Post(void* sema)
{
    SDL_SemPost((SDL_sem*)sema);
}


bool MP_Init()
{
    int opt_true = 1;
    int res;

#ifdef __WIN32__
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
    {
        return false;
    }
#endif // __WIN32__

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

#ifdef __WIN32__
    WSACleanup();
#endif // __WIN32__
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


// LAN interface. Currently powered by libpcap, may change.

#define LOAD_PCAP_FUNC(sym) \
    ptr_##sym = (type_##sym)SDL_LoadFunction(lib, #sym); \
    if (!ptr_##sym) return false;

bool TryLoadPCap(void* lib)
{
    LOAD_PCAP_FUNC(pcap_findalldevs)
    LOAD_PCAP_FUNC(pcap_freealldevs)
    LOAD_PCAP_FUNC(pcap_open_live)
    LOAD_PCAP_FUNC(pcap_close)
    LOAD_PCAP_FUNC(pcap_setnonblock)
    LOAD_PCAP_FUNC(pcap_sendpacket)
    LOAD_PCAP_FUNC(pcap_dispatch)

    return true;
}

bool LAN_Init()
{
    PCapLib = NULL;
    PCapAdapter = NULL;
    PCapPacketLen = 0;
    PCapRXNum = 0;

    for (int i = 0; PCapLibNames[i]; i++)
    {
        void* lib = SDL_LoadObject(PCapLibNames[i]);
        if (!lib) continue;

        if (!TryLoadPCap(lib))
        {
            SDL_UnloadObject(lib);
            continue;
        }

        printf("PCap: lib %s, init successful\n", PCapLibNames[i]);
        PCapLib = lib;
        break;
    }

    if (PCapLib == NULL)
    {
        printf("PCap: init failed\n");
        return false;
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    int ret;

    pcap_if_t* alldevs;
    ret = pcap_findalldevs(&alldevs, errbuf);
    if (ret < 0 || alldevs == NULL)
    {
        printf("PCap: no devices available\n");
        return false;
    }
/*while (alldevs){
    printf("picking dev %08X %s | %s\n", alldevs->flags, alldevs->name, alldevs->description);
    alldevs = alldevs->next;}*/
    // temp hack
    // TODO: ADAPTER SELECTOR!!
    pcap_if_t* dev = alldevs->next;

    PCapAdapter = pcap_open_live(dev->name, 2048, PCAP_OPENFLAG_PROMISCUOUS, 1, errbuf);
    if (!PCapAdapter)
    {
        printf("PCap: failed to open adapter\n");
        return false;
    }

    pcap_freealldevs(alldevs);

    if (pcap_setnonblock(PCapAdapter, 1, errbuf) < 0)
    {
        printf("PCap: failed to set nonblocking mode\n");
        pcap_close(PCapAdapter); PCapAdapter = NULL;
        return false;
    }

    return true;
}

void LAN_DeInit()
{
    if (PCapLib)
    {
        if (PCapAdapter)
        {
            pcap_close(PCapAdapter);
            PCapAdapter = NULL;
        }

        SDL_UnloadObject(PCapLib);
        PCapLib = NULL;
    }
}

int LAN_SendPacket(u8* data, int len)
{
    if (PCapAdapter == NULL)
        return 0;

    if (len > 2048)
    {
        printf("LAN_SendPacket: error: packet too long (%d)\n", len);
        return 0;
    }

    pcap_sendpacket(PCapAdapter, data, len);
    // TODO: check success
    return len;
}

void LAN_RXCallback(u_char* blarg, const struct pcap_pkthdr* header, const u_char* data)
{
    while (PCapRXNum > 0);

    if (header->len > 2048-64) return;

    PCapPacketLen = header->len;
    memcpy(PCapPacketBuffer, data, PCapPacketLen);
    PCapRXNum = 1;
}

int LAN_RecvPacket(u8* data)
{
    if (PCapAdapter == NULL)
        return 0;

    int ret = 0;
    if (PCapRXNum > 0)
    {
        memcpy(data, PCapPacketBuffer, PCapPacketLen);
        ret = PCapPacketLen;
        PCapRXNum = 0;
    }

    pcap_dispatch(PCapAdapter, 1, LAN_RXCallback, NULL);
    return ret;
}


}
