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

#ifndef LAN_PCAP_H
#define LAN_PCAP_H

#include "types.h"

#include <optional>
#include <vector>

#include <pcap/pcap.h>

namespace melonDS
{
namespace Platform { struct DynamicLibrary; }

struct AdapterData
{
    char DeviceName[128];
    char FriendlyName[128];
    char Description[128];

    u8 MAC[6];
    u8 IP_v4[4];

    pcap_if_t* Internal;
};

using pcap_findalldevs_t = int (*)(pcap_if_t** alldevs, char* errbuf);
using pcap_freealldevs_t = void (*)(pcap_if_t* alldevs);
using pcap_open_live_t = pcap_t* (*)(const char* src, int snaplen, int flags, int readtimeout, char* errbuf);
using pcap_close_t = void (*)(pcap_t* dev);
using pcap_setnonblock_t = int (*)(pcap_t* dev, int nonblock, char* errbuf);
using pcap_sendpacket_t = int (*)(pcap_t* dev, const u_char* data, int len);
using pcap_dispatch_t = int (*)(pcap_t* dev, int num, pcap_handler callback, u_char* data);
using pcap_next_t = const u_char* (*)(pcap_t* dev, struct pcap_pkthdr* hdr);

class LAN_PCap;

#define DECL_PCAP_FUNC(ret, name, args, args2) \
    typedef ret (*type_##name) args; \
    type_##name ptr_##name = NULL; \
    ret name args { return ptr_##name args2; }

class PCap
{
public:
    static std::optional<PCap> New() noexcept;
    ~PCap() noexcept;
    PCap(const PCap&) = delete;
    PCap& operator=(const PCap&) = delete;
    PCap(PCap&&) noexcept;
    PCap& operator=(PCap&&) noexcept;

    [[nodiscard]] const std::vector<AdapterData>& GetAdapters() const noexcept { return Adapters; }
    std::optional<LAN_PCap> OpenAdapter(std::string_view landevice) noexcept;
    std::optional<LAN_PCap> OpenAdapter(const AdapterData& landevice) noexcept;
private:
    PCap() noexcept = default;
    bool TryLoadPCap(Platform::DynamicLibrary *lib) noexcept;
    friend class LAN_PCap;
    std::vector<AdapterData> Adapters {};
    Platform::DynamicLibrary* PCapLib = nullptr;
    pcap_if_t* AllDevices = nullptr;

    pcap_findalldevs_t pcap_findalldevs = nullptr;
    pcap_freealldevs_t pcap_freealldevs = nullptr;
    pcap_open_live_t pcap_open_live = nullptr;
    pcap_close_t pcap_close = nullptr;
    pcap_setnonblock_t pcap_setnonblock = nullptr;
    pcap_sendpacket_t pcap_sendpacket = nullptr;
    pcap_dispatch_t pcap_dispatch = nullptr;
};

class LAN_PCap
{
public:
    ~LAN_PCap() noexcept;
    LAN_PCap(const LAN_PCap&) = delete;
    LAN_PCap& operator=(const LAN_PCap&) = delete;
    LAN_PCap(LAN_PCap&&) noexcept;
    LAN_PCap& operator=(LAN_PCap&&) noexcept;
    int SendPacket(u8* data, int len);
    int RecvPacket(u8* data);
private:
    friend class PCap;
    LAN_PCap(const PCap& pcap, const AdapterData& data, pcap_t* adapter) noexcept;
    static void RXCallback(u_char* blarg, const struct pcap_pkthdr* header, const u_char* data) noexcept;

    pcap_close_t pcap_close = nullptr;
    pcap_sendpacket_t pcap_sendpacket = nullptr;
    pcap_dispatch_t pcap_dispatch = nullptr;

    u8 PacketBuffer[2048] {};
    int PacketLen = 0;
    volatile int RXNum = 0;
    pcap_t* PCapAdapter = nullptr;
    AdapterData PCapAdapterData;
};

#undef DECL_PCAP_FUNC

}

#endif // LAN_PCAP_H
