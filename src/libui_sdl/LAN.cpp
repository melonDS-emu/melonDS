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
#include "../Config.h"

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
volatile int PCapRXNum;

u16 IPv4ID;


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

    IPv4ID = 1;

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

    // open pcap device
    dev = (pcap_if_t*)Adapters[0].Internal;
    for (int i = 0; i < NumAdapters; i++)
    {
        if (!strncmp(Adapters[i].DeviceName, Config::LANDevice, 128))
            dev = (pcap_if_t*)Adapters[i].Internal;
    }

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

void RXCallback(u_char* blarg, const struct pcap_pkthdr* header, const u_char* data)
{
    while (PCapRXNum > 0);

    if (header->len > 2048-64) return;

    PCapPacketLen = header->len;
    memcpy(PCapPacketBuffer, data, PCapPacketLen);
    PCapRXNum = 1;
}
u32 zarp=0;
bool HandleDHCPFrame(u8* data, int len)
{
    const u32 serverip = 0x0A404001;
    const u32 clientip = 0x0A404010;

    u8 type = 0xFF;

    u32 transid = *(u32*)&data[0x2E];
zarp=transid;
    u8* options = &data[0x11A];
    for (;;)
    {
        if (options >= &data[len]) break;
        u8 opt = *options++;
        if (opt == 255) break;

        u8 len = *options++;
        switch (opt)
        {
        case 53: // frame type
            type = options[0];
            break;
        }

        options += len;
    }

    if (type == 0xFF)
    {
        printf("DHCP: bad frame\n");
        return false;
    }

    printf("DHCP: frame type %d, transid %08X\n", type, transid);

    if (type == 1) // discover
    {
        u8 resp[512];
        u8* out = &resp[0];

        // ethernet
        memcpy(out, &data[6], 6); out += 6;
        *out++ = 0x00; *out++ = 0xAB; *out++ = 0x33;
        *out++ = 0x28; *out++ = 0x99; *out++ = 0x44;
        *(u16*)out = htons(0x0800); out += 2;

        // IP
        u8* ipheader = out;
        *out++ = 0x45;
        *out++ = 0x00;
        *(u16*)out = 0; out += 2; // total length
        *(u16*)out = htons(IPv4ID); out += 2; IPv4ID++;
        *out++ = 0x00;
        *out++ = 0x00;
        *out++ = 0x80; // TTL
        *out++ = 0x11; // protocol (UDP)
        *(u16*)out = 0; out += 2; // checksum
        *(u32*)out = htonl(serverip); out += 4; // source IP
        *(u32*)out = htonl(0xFFFFFFFF); out += 4; // destination IP

        // UDP
        u8* udpheader = out;
        *(u16*)out = htons(67); out += 2; // source port
        *(u16*)out = htons(68); out += 2; // destination port
        *(u16*)out = 0; out += 2; // length
        *(u16*)out = 0; out += 2; // checksum

        // DHCP
        u8* body = out;
        *out++ = 0x02;
        *out++ = 0x01;
        *out++ = 0x06;
        *out++ = 0x00;
        *(u32*)out = transid; out += 4;
        *(u16*)out = 0; out += 2; // seconds elapsed
        *(u16*)out = 0; out += 2;
        *(u32*)out = htonl(0x00000000); out += 4; // client IP
        *(u32*)out = htonl(clientip); out += 4; // your IP
        *(u32*)out = htonl(serverip); out += 4; // server IP
        *(u32*)out = htonl(0x00000000); out += 4; // gateway IP
        memcpy(out, &data[6], 6); out += 6;
        memset(out, 0, 10); out += 10;
        memset(out, 0, 192); out += 192;
        *(u32*)out = 0x63538263; out += 4; // DHCP magic

        // DHCP options
        *out++ = 53; *out++ = 1;
        *out++ = 2; // DHCP type: offer
        *out++ = 1; *out++ = 4;
        *(u32*)out = htonl(0xFFFFFF00); out += 4; // subnet mask
        *out++ = 3; *out++ = 4;
        *(u32*)out = htonl(serverip); out += 4; // router
        *out++ = 51; *out++ = 4;
        *(u32*)out = htonl(442030); out += 4; // lease time
        *out++ = 54; *out++ = 4;
        *(u32*)out = htonl(serverip); out += 4; // DHCP server
        *out++ = 6; *out++ = 4;
        *(u32*)out = htonl(0x08080808); out += 4; // DNS (TODO!!)

        *out++ = 0xFF;
        memset(out, 0, 20); out += 20;

        // lengths
        u32 framelen = (u32)(out - &resp[0]);
        if (framelen & 1) { *out++ = 0; framelen++; }
        *(u16*)&ipheader[2] = htons(framelen - 0xE);
        *(u16*)&udpheader[4] = htons(framelen - (0xE + 0x14));

        // IP checksum
        u32 tmp = 0;

        for (int i = 0; i < 20; i += 2)
            tmp += ntohs(*(u16*)&ipheader[i]);
        while (tmp >> 16)
            tmp = (tmp & 0xFFFF) + (tmp >> 16);
        tmp ^= 0xFFFF;
        *(u16*)&ipheader[10] = htons(tmp);

        // UDP checksum
        // (note: normally not mandatory, but some older sgIP versions require it)
        tmp = 0;
        tmp += ntohs(*(u16*)&ipheader[12]);
        tmp += ntohs(*(u16*)&ipheader[14]);
        tmp += ntohs(*(u16*)&ipheader[16]);
        tmp += ntohs(*(u16*)&ipheader[18]);
        tmp += ntohs(0x1100);
        tmp += (u32)(out - udpheader);
        for (u8* i = udpheader; i < out; i += 2)
            tmp += ntohs(*(u16*)i);
        while (tmp >> 16)
            tmp = (tmp & 0xFFFF) + (tmp >> 16);
        tmp ^= 0xFFFF;
        if (tmp == 0) tmp = 0xFFFF;
        *(u16*)&udpheader[6] = htons(tmp);

        // TODO: if there is already a packet queued, this will overwrite it
        // that being said, this will only happen during DHCP setup, so probably
        // not a big deal

        PCapPacketLen = framelen;
        memcpy(PCapPacketBuffer, resp, PCapPacketLen);
        PCapRXNum = 1;

        // DEBUG!!
        //pcap_sendpacket(PCapAdapter, data, len);
        //pcap_sendpacket(PCapAdapter, resp, framelen);

        return true;
    }

    return false;
}

bool HandlePacket(u8* data, int len)
{
    if (ntohs(*(u16*)&data[0xC]) != 0x0800) // IPv4
        return false;

    u8 protocol = data[0x17];
    if (protocol == 0x11) // UDP
    {
        u16 srcport = ntohs(*(u16*)&data[0x22]);
        u16 dstport = ntohs(*(u16*)&data[0x24]);
        if (srcport == 68 && dstport == 67) // DHCP
        {
            printf("LANMAGIC: DHCP packet\n");
            return HandleDHCPFrame(data, len);
        }
    }

    return false;
}

int SendPacket(u8* data, int len)
{
    if (PCapAdapter == NULL)
        return 0;

    if (len > 2048)
    {
        printf("LAN_SendPacket: error: packet too long (%d)\n", len);
        return 0;
    }

    if (!Config::DirectLAN)
    {
        if (HandlePacket(data, len))
            return len;
    }

    pcap_sendpacket(PCapAdapter, data, len);
    // TODO: check success
    return len;
}

int RecvPacket(u8* data)
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

    //pcap_dispatch(PCapAdapter, 1, RXCallback, NULL);
    return ret;
}

}
