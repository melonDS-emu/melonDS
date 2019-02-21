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

#ifdef __WIN32__
	#include <iphlpapi.h>
#else
	// Linux includes go here
#endif


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

AdapterData* Adapters = NULL;
int NumAdapters = 0;

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
    // TODO: how to deal with cases where an adapter is unplugged or changes config??
    if (PCapLib) return true;

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

    pcap_if_t* dev = alldevs;
    while (dev) { NumAdapters++; dev = dev->next; }

    Adapters = new AdapterData[NumAdapters];
    memset(Adapters, 0, sizeof(AdapterData)*NumAdapters);

    AdapterData* adata = &Adapters[0];
    dev = alldevs;
    while (dev)
    {
        adata->Internal = dev;

        // hax
        int len = strlen(dev->name);
        len -= 12; if (len > 127) len = 127;
        strncpy(adata->DeviceName, &dev->name[12], len);
        adata->DeviceName[len] = '\0';

        dev = dev->next;
        adata++;
    }

#ifdef __WIN32__

    ULONG bufsize = 16384;
    IP_ADAPTER_ADDRESSES* buf = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, bufsize);
    ULONG uret = GetAdaptersAddresses(AF_INET, 0, NULL, buf, &bufsize);
    if (uret == ERROR_BUFFER_OVERFLOW)
    {
        HeapFree(GetProcessHeap(), 0, buf);
        buf = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, bufsize);
        uret = GetAdaptersAddresses(AF_INET, 0, NULL, buf, &bufsize);
    }
    if (uret != ERROR_SUCCESS)
    {
        printf("GetAdaptersAddresses() shat itself: %08X\n", ret);
        return false;
    }

    for (int i = 0; i < NumAdapters; i++)
    {
        adata = &Adapters[i];
        IP_ADAPTER_ADDRESSES* addr = buf;
        while (addr)
        {
            if (strcmp(addr->AdapterName, adata->DeviceName))
            {
                addr = addr->Next;
                continue;
            }

            WideCharToMultiByte(CP_UTF8, 0, addr->FriendlyName, 127, adata->FriendlyName, 127, NULL, NULL);
            adata->FriendlyName[127] = '\0';

            WideCharToMultiByte(CP_UTF8, 0, addr->Description, 127, adata->Description, 127, NULL, NULL);
            adata->Description[127] = '\0';

            if (addr->PhysicalAddressLength != 6)
            {
                printf("weird MAC addr length %d for %s\n", addr->PhysicalAddressLength, addr->AdapterName);
            }
            else
                memcpy(adata->MAC, addr->PhysicalAddress, 6);

            IP_ADAPTER_UNICAST_ADDRESS* ipaddr = addr->FirstUnicastAddress;
            while (ipaddr)
            {
                SOCKADDR* sa = ipaddr->Address.lpSockaddr;
                if (sa->sa_family == AF_INET)
                {
                    struct in_addr sa4 = ((sockaddr_in*)sa)->sin_addr;
                    memcpy(adata->IP_v4, &sa4.S_un.S_addr, 4);
                }

                ipaddr = ipaddr->Next;
            }

            IP_ADAPTER_DNS_SERVER_ADDRESS* dnsaddr = addr->FirstDnsServerAddress;
            int ndns = 0;
            while (dnsaddr)
            {
                SOCKADDR* sa = dnsaddr->Address.lpSockaddr;
                if (sa->sa_family == AF_INET)
                {
                    struct in_addr sa4 = ((sockaddr_in*)sa)->sin_addr;
                    memcpy(adata->DNS[ndns++], &sa4.S_un.S_addr, 4);
                }

                if (ndns >= 8) break;
                dnsaddr = dnsaddr->Next;
            }

            break;
        }
    }

    HeapFree(GetProcessHeap(), 0, buf);

#else

    // TODO

#endif // __WIN32__

    printf("devices: %d\n", NumAdapters);
    for (int i = 0; i < NumAdapters; i++)
    {
        AdapterData* zog = &Adapters[i];

        printf("%s:\n", ((pcap_if_t*)zog->Internal)->name);

        printf("* %s\n", zog->FriendlyName);
        printf("* %s\n", zog->Description);

        printf("* "); for (int j = 0; j < 6; j++) printf("%02X:", zog->MAC[j]); printf("\n");
        printf("* "); for (int j = 0; j < 4; j++) printf("%d.", zog->IP_v4[j]); printf("\n");

        for (int k = 0; k < 8; k++)
        {
             printf("* "); for (int j = 0; j < 4; j++) printf("%d.", zog->DNS[k][j]); printf("\n");
        }
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
