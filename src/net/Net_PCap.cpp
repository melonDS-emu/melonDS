/*
    Copyright 2016-2025 melonDS team

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

#include <string.h>
#include <pcap/pcap.h>
#include "Net.h"
#include "Net_PCap.h"
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
    nullptr
};

std::optional<LibPCap> LibPCap::New() noexcept
{
    for (int i = 0; PCapLibNames[i]; i++)
    {
        Platform::DynamicLibrary* lib = Platform::DynamicLibrary_Load(PCapLibNames[i]);
        if (!lib) continue;

        LibPCap pcap;
        // Use a custom deleter to clean up the DLL automatically
        // (in this case, the deleter is the DynamicLibrary_Unload function)
        pcap.PCapLib = std::shared_ptr<Platform::DynamicLibrary>(lib, Platform::DynamicLibrary_Unload);

        if (!TryLoadPCap(pcap, lib))
        {
            Platform::DynamicLibrary_Unload(lib);
            continue;
        }

        Log(LogLevel::Info, "PCap: lib %s, init successful\n", PCapLibNames[i]);
        return pcap;
    }

    Log(LogLevel::Error, "PCap: init failed\n");
    return std::nullopt;
}

LibPCap::LibPCap(LibPCap&& other) noexcept
{
    PCapLib = std::move(other.PCapLib);
    findalldevs = other.findalldevs;
    freealldevs = other.freealldevs;
    open_live = other.open_live;
    close = other.close;
    setnonblock = other.setnonblock;
    sendpacket = other.sendpacket;
    dispatch = other.dispatch;
    next = other.next;

    other.PCapLib = nullptr;
    other.findalldevs = nullptr;
    other.freealldevs = nullptr;
    other.open_live = nullptr;
    other.close = nullptr;
    other.setnonblock = nullptr;
    other.sendpacket = nullptr;
    other.dispatch = nullptr;
    other.next = nullptr;
}

LibPCap& LibPCap::operator=(LibPCap&& other) noexcept
{
    if (this != &other)
    {
        PCapLib = nullptr;
        // Unloads the DLL due to the custom deleter

        PCapLib = std::move(other.PCapLib);
        findalldevs = other.findalldevs;
        freealldevs = other.freealldevs;
        open_live = other.open_live;
        close = other.close;
        setnonblock = other.setnonblock;
        sendpacket = other.sendpacket;
        dispatch = other.dispatch;
        next = other.next;

        other.PCapLib = nullptr;
        other.findalldevs = nullptr;
        other.freealldevs = nullptr;
        other.open_live = nullptr;
        other.close = nullptr;
        other.setnonblock = nullptr;
        other.sendpacket = nullptr;
        other.dispatch = nullptr;
        other.next = nullptr;
    }

    return *this;
}

bool LibPCap::TryLoadPCap(LibPCap& pcap, Platform::DynamicLibrary *lib) noexcept
{
    pcap.findalldevs = (pcap_findalldevs_t)Platform::DynamicLibrary_LoadFunction(lib, "pcap_findalldevs");
    if (!pcap.findalldevs) return false;

    pcap.freealldevs = (pcap_freealldevs_t)Platform::DynamicLibrary_LoadFunction(lib, "pcap_freealldevs");
    if (!pcap.freealldevs) return false;

    pcap.open_live = (pcap_open_live_t)Platform::DynamicLibrary_LoadFunction(lib, "pcap_open_live");
    if (!pcap.open_live) return false;

    pcap.close = (pcap_close_t)Platform::DynamicLibrary_LoadFunction(lib, "pcap_close");
    if (!pcap.close) return false;

    pcap.setnonblock = (pcap_setnonblock_t)Platform::DynamicLibrary_LoadFunction(lib, "pcap_setnonblock");
    if (!pcap.setnonblock) return false;

    pcap.sendpacket = (pcap_sendpacket_t)Platform::DynamicLibrary_LoadFunction(lib, "pcap_sendpacket");
    if (!pcap.sendpacket) return false;

    pcap.dispatch = (pcap_dispatch_t)Platform::DynamicLibrary_LoadFunction(lib, "pcap_dispatch");
    if (!pcap.dispatch) return false;

    pcap.next = (pcap_next_t)Platform::DynamicLibrary_LoadFunction(lib, "pcap_next");
    if (!pcap.next) return false;

    return true;
}

std::vector<AdapterData> LibPCap::GetAdapters() const noexcept
{
    // TODO: how to deal with cases where an adapter is unplugged or changes config??
    if (!IsValid())
    {
        Log(LogLevel::Error, "PCap: instance not initialized\n");
        return {};
    }

    char errbuf[PCAP_ERRBUF_SIZE];

    pcap_if_t* alldevs = nullptr;
    if (int ret = findalldevs(&alldevs, errbuf); ret < 0)
    { // If there was an error...
        errbuf[PCAP_ERRBUF_SIZE - 1] = '\0';
        Log(LogLevel::Error, "PCap: Error %d finding devices: %s\n", ret, errbuf);
    }

    if (alldevs == nullptr)
    { // If no devices were found...
        Log(LogLevel::Warn, "PCap: no devices available\n");
        return {};
    }

    std::vector<AdapterData> adapters;
    for (pcap_if_t* dev = alldevs; dev != nullptr; dev = dev->next)
    {
        adapters.emplace_back(); // Add a new (empty) adapter to the list
        AdapterData& adata = adapters.back();
        strncpy(adata.DeviceName, dev->name, 127);
        adata.DeviceName[127] = '\0';
        adata.Flags = dev->flags;

#ifndef __WIN32__
        strncpy(adata.FriendlyName, adata.DeviceName, 127);
        adata.FriendlyName[127] = '\0';
#endif // __WIN32__
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
        freealldevs(alldevs);
        return {};
    }

    for (AdapterData& adata : adapters)
    {
        IP_ADAPTER_ADDRESSES* addr = buf;
        while (addr)
        {
            if (strcmp(addr->AdapterName, &adata.DeviceName[12]))
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
        return {};
    }

    for (const AdapterData& adata : adapters)
    {
        struct ifaddrs* curaddr = addrs;
        while (curaddr)
        {
            if (strcmp(curaddr->ifa_name, adata.DeviceName))
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
                memcpy((void*)adata.IP_v4, &sa->sin_addr, 4);
            }
#ifdef __linux__
            else if (af == AF_PACKET)
            {
                struct sockaddr_ll* sa = (sockaddr_ll*)curaddr->ifa_addr;
                if (sa->sll_halen != 6)
                    Log(LogLevel::Warn, "weird MAC length %d for %s\n", sa->sll_halen, curaddr->ifa_name);
                else
                    memcpy((void*)adata.MAC, sa->sll_addr, 6);
            }
#else
            else if (af == AF_LINK)
            {
                struct sockaddr_dl* sa = (sockaddr_dl*)curaddr->ifa_addr;
                if (sa->sdl_alen != 6)
                    Log(LogLevel::Warn, "weird MAC length %d for %s\n", sa->sdl_alen, curaddr->ifa_name);
                else
                    memcpy((void*)adata.MAC, LLADDR(sa), 6);
            }
#endif
            curaddr = curaddr->ifa_next;
        }
    }

    freeifaddrs(addrs);

#endif // __WIN32__

    freealldevs(alldevs);
    return adapters;
}

std::unique_ptr<Net_PCap> LibPCap::Open(const AdapterData& device, const Platform::SendPacketCallback& handler) const noexcept
{
    return Open(device.DeviceName, handler);
}

std::unique_ptr<Net_PCap> LibPCap::Open(std::string_view devicename, const Platform::SendPacketCallback& handler) const noexcept
{
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* adapter = open_live(devicename.data(), 2048, PCAP_OPENFLAG_PROMISCUOUS, 1, errbuf);
    if (!adapter)
    {
        errbuf[PCAP_ERRBUF_SIZE - 1] = '\0';
        Log(LogLevel::Error, "PCap: failed to open adapter: %s\n", errbuf);
        return nullptr;
    }

    if (int err = setnonblock(adapter, 1, errbuf); err < 0)
    {
        errbuf[PCAP_ERRBUF_SIZE - 1] = '\0';
        Log(LogLevel::Error, "PCap: failed to set nonblocking mode with %d: %s\n", err, errbuf);
        close(adapter);
        return nullptr;
    }

    std::unique_ptr<Net_PCap> pcap = std::make_unique<Net_PCap>();
    pcap->PCapAdapter = adapter;
    pcap->Callback = handler;
    pcap->PCapLib = PCapLib;
    pcap->close = close;
    pcap->sendpacket = sendpacket;
    pcap->dispatch = dispatch;

    return pcap;
}

Net_PCap::Net_PCap(Net_PCap&& other) noexcept
{
    PCapAdapter = other.PCapAdapter;
    PCapLib = std::move(other.PCapLib);
    close = other.close;
    sendpacket = other.sendpacket;
    dispatch = other.dispatch;
    Callback = std::move(other.Callback);

    other.PCapAdapter = nullptr;
    other.close = nullptr;
    other.PCapLib = nullptr;
    other.sendpacket = nullptr;
    other.dispatch = nullptr;
    other.Callback = nullptr;
}

Net_PCap& Net_PCap::operator=(Net_PCap&& other) noexcept
{
    if (this != &other)
    {
        if (close && PCapAdapter)
        {
            close(PCapAdapter);
            PCapAdapter = nullptr;
        }

        PCapAdapter = other.PCapAdapter;
        PCapLib = std::move(other.PCapLib);
        close = other.close;
        sendpacket = other.sendpacket;
        dispatch = other.dispatch;
        Callback = std::move(other.Callback);

        other.PCapAdapter = nullptr;
        other.close = nullptr;
        other.PCapLib = nullptr;
        other.sendpacket = nullptr;
        other.dispatch = nullptr;
        other.Callback = nullptr;
    }

    return *this;
}

Net_PCap::~Net_PCap() noexcept
{
    if (close && PCapAdapter)
    {
        close(PCapAdapter);
        PCapAdapter = nullptr;
    }
    // PCapLib will be freed at this point (shared_ptr + custom deleter)
}

void Net_PCap::RXCallback(u_char* userdata, const struct pcap_pkthdr* header, const u_char* data) noexcept
{
    Net_PCap& self = *reinterpret_cast<Net_PCap*>(userdata);
    if (self.Callback)
        self.Callback(data, header->len);
}

int Net_PCap::SendPacket(u8* data, int len) noexcept
{
    if (PCapAdapter == nullptr || data == nullptr)
        return 0;

    if (len > 2048)
    {
        Log(LogLevel::Error, "Net_SendPacket: error: packet too long (%d)\n", len);
        return 0;
    }

    sendpacket(PCapAdapter, data, len);
    // TODO: check success
    return len;
}

void Net_PCap::RecvCheck() noexcept
{
    if (PCapAdapter == nullptr || dispatch == nullptr)
        return;

    dispatch(PCapAdapter, 1, RXCallback, reinterpret_cast<u_char*>(this));
}

}
