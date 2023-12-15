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

// direct LAN interface. Currently powered by libpcap, may change.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap/pcap.h>
#include "Wifi.h"
#include "LAN_PCap.h"
#include "Config.h"
#include "Platform.h"

#ifdef __WIN32__
	#include <iphlpapi.h>
#else
	#include <sys/types.h>
	#include <ifaddrs.h>
	#include <netinet/in.h>
        #ifdef __linux__
            #include <linux/if_packet.h>
        #else
            #include <net/if.h>
            #include <net/if_dl.h>
        #endif
#endif

using namespace melonDS;
using Platform::Log;
using Platform::LogLevel;

// welp
#ifndef PCAP_OPENFLAG_PROMISCUOUS
#define PCAP_OPENFLAG_PROMISCUOUS 1
#endif

namespace melonDS
{

const char* PCapLibNames[] =
{
#ifdef __WIN32__
    // TODO: name for npcap in non-WinPCap mode
    "wpcap.dll",
#elif defined(__APPLE__)
    "libpcap.A.dylib",
    "libpcap.dylib",
#else
    // Linux lib names
    "libpcap.so.1",
    "libpcap.so",
#endif
    NULL
};

#define LOAD_PCAP_FUNC(sym) \
    sym = (sym##_t)DynamicLibrary_LoadFunction(lib, #sym); \
    if (!sym) return false;

bool PCap::TryLoadPCap(Platform::DynamicLibrary *lib) noexcept
{
    LOAD_PCAP_FUNC(pcap_findalldevs)
    LOAD_PCAP_FUNC(pcap_freealldevs)
    LOAD_PCAP_FUNC(pcap_open_live)
    LOAD_PCAP_FUNC(pcap_close)
    LOAD_PCAP_FUNC(pcap_setnonblock)
    LOAD_PCAP_FUNC(pcap_sendpacket)
    LOAD_PCAP_FUNC(pcap_dispatch)

    PCapLib = lib;
    return true;
}

std::optional<PCap> PCap::New() noexcept
{
    PCap pcap;

    // TODO: how to deal with cases where an adapter is unplugged or changes config??

    for (int i = 0; PCapLibNames[i]; i++)
    {
        Platform::DynamicLibrary* lib = Platform::DynamicLibrary_Load(PCapLibNames[i]);
        if (!lib) continue;

        if (!pcap.TryLoadPCap(lib))
        {
            Platform::DynamicLibrary_Unload(lib);
            continue;
        }

        Log(LogLevel::Info, "PCap: lib %s, init successful\n", PCapLibNames[i]);
        break;
    }

    if (pcap.PCapLib == nullptr)
    {
        Log(LogLevel::Error, "PCap: init failed\n");
        return std::nullopt;
    }

    char errbuf[PCAP_ERRBUF_SIZE] {};
    if (pcap.pcap_findalldevs(&pcap.AllDevices, errbuf) < 0 || pcap.AllDevices == nullptr)
    {
        Log(LogLevel::Warn, "PCap: no devices available\n");
        return std::nullopt;
    }

    for (pcap_if_t* dev = pcap.AllDevices; dev != nullptr; dev = dev->next)
    {
        AdapterData adata {};
        adata.Internal = dev;

#ifdef __WIN32__
        // hax
        int len = strlen(dev->name);
        len -= 12; if (len > 127) len = 127;
        strncpy(adata.DeviceName, &dev->name[12], len);
        adata.DeviceName[len] = '\0';
#else
        strncpy(adata.DeviceName, dev->name, 127);
        adata.DeviceName[127] = '\0';

        strncpy(adata.FriendlyName, adata.DeviceName, 127);
        adata.FriendlyName[127] = '\0';
#endif // __WIN32__

        pcap.Adapters.push_back(adata);
    }

#ifdef __WIN32__

    ULONG bufsize = 16384;
    IP_ADAPTER_ADDRESSES* buf = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, bufsize);
    ULONG uret = GetAdaptersAddresses(AF_INET, 0, nullptr, buf, &bufsize);
    if (uret == ERROR_BUFFER_OVERFLOW)
    {
        HeapFree(GetProcessHeap(), 0, buf);
        buf = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, bufsize);
        uret = GetAdaptersAddresses(AF_INET, 0, nullptr, buf, &bufsize);
    }
    if (uret != ERROR_SUCCESS)
    {
        Log(LogLevel::Error, "GetAdaptersAddresses() shat itself: %08X\n", uret);
        return std::nullopt; // PCap's destructor will handle cleanup
    }

    for (AdapterData& adata : pcap.Adapters)
    {
        IP_ADAPTER_ADDRESSES* addr = buf;
        while (addr)
        {
            if (strcmp(addr->AdapterName, adata.DeviceName))
            {
                addr = addr->Next;
                continue;
            }

            WideCharToMultiByte(CP_UTF8, 0, addr->FriendlyName, 127, adata.FriendlyName, 127, nullptr, nullptr);
            adata.FriendlyName[127] = '\0';

            WideCharToMultiByte(CP_UTF8, 0, addr->Description, 127, adata.Description, 127, nullptr, nullptr);
            adata.Description[127] = '\0';

            if (addr->PhysicalAddressLength != 6)
            {
                Log(LogLevel::Warn, "weird MAC addr length %d for %s\n", addr->PhysicalAddressLength, addr->AdapterName);
            }
            else
                memcpy(adata.MAC, addr->PhysicalAddress, 6);

            IP_ADAPTER_UNICAST_ADDRESS* ipaddr = addr->FirstUnicastAddress;
            while (ipaddr)
            {
                SOCKADDR* sa = ipaddr->Address.lpSockaddr;
                if (sa->sa_family == AF_INET)
                {
                    struct in_addr sa4 = ((sockaddr_in*)sa)->sin_addr;
                    memcpy(adata.IP_v4, &sa4, 4);
                }

                ipaddr = ipaddr->Next;
            }

            break;
        }
    }

    HeapFree(GetProcessHeap(), 0, buf);

#else

    struct ifaddrs* addrs;
    if (getifaddrs(&addrs) != 0)
    {
        Log(LogLevel::Error, "getifaddrs() shat itself :(\n");
        return std::nullopt; // PCap's destructor will handle cleanup
    }

    for (AdapterData& adata : pcap.Adapters)
    {
        struct ifaddrs* curaddr = addrs;
        while (curaddr)
        {
            if (strcmp(curaddr->ifa_name, adata.DeviceName) != 0)
            {
                curaddr = curaddr->ifa_next;
                continue;
            }

            if (!curaddr->ifa_addr)
            {
                Log(LogLevel::Error, "Device (%s) does not have an address :/\n", curaddr->ifa_name);
                curaddr = curaddr->ifa_next;
                continue;
            }

            u16 af = curaddr->ifa_addr->sa_family;
            if (af == AF_INET)
            {
                struct sockaddr_in* sa = (sockaddr_in*)curaddr->ifa_addr;
                memcpy(adata.IP_v4, &sa->sin_addr, 4);
            }
#ifdef __linux__
            else if (af == AF_PACKET)
            {
                struct sockaddr_ll* sa = (sockaddr_ll*)curaddr->ifa_addr;
                if (sa->sll_halen != 6)
                    Log(LogLevel::Warn, "weird MAC length %d for %s\n", sa->sll_halen, curaddr->ifa_name);
                else
                    memcpy(adata.MAC, sa->sll_addr, 6);
            }
#else
            else if (af == AF_LINK)
            {
                struct sockaddr_dl* sa = (sockaddr_dl*)curaddr->ifa_addr;
                if (sa->sdl_alen != 6)
                    Log(LogLevel::Warn, "weird MAC length %d for %s\n", sa->sdl_alen, curaddr->ifa_name);
                else
                    memcpy(adata.MAC, LLADDR(sa), 6);
            }
#endif
            curaddr = curaddr->ifa_next;
        }
    }

    freeifaddrs(addrs);

#endif // __WIN32__

    return std::make_optional(std::move(pcap));
}

PCap::PCap(PCap&& other) noexcept :
    Adapters(std::move(other.Adapters)),
    PCapLib(other.PCapLib),
    AllDevices(other.AllDevices),
    pcap_findalldevs(other.pcap_findalldevs),
    pcap_freealldevs(other.pcap_freealldevs),
    pcap_open_live(other.pcap_open_live),
    pcap_close(other.pcap_close),
    pcap_setnonblock(other.pcap_setnonblock),
    pcap_sendpacket(other.pcap_sendpacket),
    pcap_dispatch(other.pcap_dispatch)
{
    other.PCapLib = nullptr;
    other.AllDevices = nullptr;
    other.pcap_close = nullptr;
    other.pcap_dispatch = nullptr;
    other.pcap_findalldevs = nullptr;
    other.pcap_freealldevs = nullptr;
    other.pcap_open_live = nullptr;
    other.pcap_sendpacket = nullptr;
    other.pcap_setnonblock = nullptr;
}

PCap& PCap::operator=(PCap&& other) noexcept
{
    if (this != &other)
    {
        Adapters = std::move(other.Adapters);
        if (AllDevices)
        {
            pcap_freealldevs(AllDevices);
        }
        if (PCapLib)
        {
            Platform::DynamicLibrary_Unload(PCapLib);
        }
        PCapLib = other.PCapLib;
        AllDevices = other.AllDevices;
        pcap_close = other.pcap_close;
        pcap_dispatch = other.pcap_dispatch;
        pcap_findalldevs = other.pcap_findalldevs;
        pcap_freealldevs = other.pcap_freealldevs;
        pcap_open_live = other.pcap_open_live;
        pcap_sendpacket = other.pcap_sendpacket;
        pcap_setnonblock = other.pcap_setnonblock;

        other.pcap_close = nullptr;
        other.pcap_dispatch = nullptr;
        other.pcap_findalldevs = nullptr;
        other.pcap_freealldevs = nullptr;
        other.pcap_open_live = nullptr;
        other.pcap_sendpacket = nullptr;
        other.pcap_setnonblock = nullptr;
        other.PCapLib = nullptr;
        other.AllDevices = nullptr;
    }

    return *this;
}

PCap::~PCap() noexcept
{
    if (PCapLib)
    {
        if (AllDevices)
        {
            pcap_freealldevs(AllDevices);
            AllDevices = nullptr;
        }

        Platform::DynamicLibrary_Unload(PCapLib);
        PCapLib = nullptr;
    }
}

std::optional<LAN_PCap> PCap::OpenAdapter(std::string_view landevice) noexcept
{
    for (const AdapterData& adata : Adapters)
    {
        if (strncmp(landevice.data(), adata.DeviceName, 128) == 0)
            return OpenAdapter(adata);
    }

    return std::nullopt;
}

std::optional<LAN_PCap> PCap::OpenAdapter(const AdapterData& landevice) noexcept
{
    char errbuf[PCAP_ERRBUF_SIZE] {};

    // open pcap device
    pcap_t* adapter = pcap_open_live(landevice.Internal->name, 2048, PCAP_OPENFLAG_PROMISCUOUS, 1, errbuf);
    if (!adapter)
    {
        Log(LogLevel::Error, "PCap: failed to open adapter %s\n", errbuf);
        return std::nullopt;
    }

    if (pcap_setnonblock(adapter, 1, errbuf) < 0)
    {
        Log(LogLevel::Error, "PCap: failed to set nonblocking mode\n");
        pcap_close(adapter);
        return std::nullopt;
    }

    return LAN_PCap(*this, landevice, adapter);
}

LAN_PCap::LAN_PCap(const PCap& pcap, const AdapterData& data, pcap_t* adapter) noexcept :
    pcap_close(pcap.pcap_close),
    pcap_sendpacket(pcap.pcap_sendpacket),
    pcap_dispatch(pcap.pcap_dispatch),
    PCapAdapter(adapter),
    PCapAdapterData(data)
{
}

LAN_PCap::~LAN_PCap() noexcept
{
    if (PCapAdapter && pcap_close)
    {
        pcap_close(PCapAdapter);
        PCapAdapter = nullptr;
    }
}

LAN_PCap::LAN_PCap(LAN_PCap&& other) noexcept :
    pcap_close(other.pcap_close),
    pcap_sendpacket(other.pcap_sendpacket),
    pcap_dispatch(other.pcap_dispatch),
    PacketLen(other.PacketLen),
    RXNum(other.RXNum),
    PCapAdapter(other.PCapAdapter),
    PCapAdapterData(other.PCapAdapterData)
{
    other.PCapAdapter = nullptr;
    other.pcap_close = nullptr;
    other.pcap_dispatch = nullptr;
    other.pcap_sendpacket = nullptr;
    other.PCapAdapterData = {};
}

LAN_PCap& LAN_PCap::operator=(LAN_PCap&& other) noexcept
{
    if (this != &other)
    {
        if (PCapAdapter)
        {
            pcap_close(PCapAdapter);
            PCapAdapter = nullptr;
        }

        pcap_close = other.pcap_close;
        pcap_sendpacket = other.pcap_sendpacket;
        pcap_dispatch = other.pcap_dispatch;
        PacketLen = other.PacketLen;
        RXNum = other.RXNum;
        PCapAdapter = other.PCapAdapter;
        PCapAdapterData = other.PCapAdapterData;

        other.pcap_close = nullptr;
        other.pcap_dispatch = nullptr;
        other.pcap_sendpacket = nullptr;
        other.PCapAdapter = nullptr;
        other.PCapAdapterData = {};
    }

    return *this;
}

void LAN_PCap::RXCallback(u_char* userdata, const struct pcap_pkthdr* header, const u_char* data) noexcept
{
    LAN_PCap* lan = (LAN_PCap*)userdata;
    while (lan->RXNum > 0);

    if (header->len > 2048-64) return;

    lan->PacketLen = header->len;
    memcpy(lan->PacketBuffer, data, lan->PacketLen);
    lan->RXNum = 1;
}

int LAN_PCap::SendPacket(u8* data, int len)
{
    if (PCapAdapter == nullptr)
        return 0;

    if (len > 2048)
    {
        Log(LogLevel::Error, "LAN_SendPacket: error: packet too long (%d)\n", len);
        return 0;
    }

    pcap_sendpacket(PCapAdapter, data, len);
    // TODO: check success
    return len;
}

int LAN_PCap::RecvPacket(u8* data)
{
    if (PCapAdapter == nullptr)
        return 0;

    int ret = 0;
    if (RXNum > 0)
    {
        memcpy(data, PacketBuffer, PacketLen);
        ret = PacketLen;
        RXNum = 0;
    }

    pcap_dispatch(PCapAdapter, 1, RXCallback, reinterpret_cast<u_char*>(this));
    return ret;
}

}
