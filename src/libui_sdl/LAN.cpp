/*
    Copyright 2016-2019 Arisotura

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

// LAN interface. Currently powered by libpcap, may change.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <pcap/pcap.h>
#include "LAN.h"


// welp
#ifndef PCAP_OPENFLAG_PROMISCUOUS
#define PCAP_OPENFLAG_PROMISCUOUS 1
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


namespace LAN
{

const char* PCapLibNames[] =
{
#ifdef __WIN32__
    // TODO: name for npcap in non-WinPCap mode
    "wpcap.dll",
#else
    // Linux lib names
    "libpcap.so.1",
    "libpcap.so",
#endif
    NULL
};

void* PCapLib = NULL;
pcap_t* PCapAdapter = NULL;

u8 PCapPacketBuffer[2048];
int PCapPacketLen;
int PCapRXNum;


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

bool Init()
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

void DeInit()
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

}
