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

#ifndef NET_PCAP_H
#define NET_PCAP_H

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>
#include <pcap/pcap.h>

#include "types.h"
#include "Platform.h"
#include "NetDriver.h"


namespace melonDS
{
struct AdapterData
{
    char DeviceName[128];
    char FriendlyName[128];
    char Description[128];

    u8 MAC[6];
    u8 IP_v4[4];

    /// The flags on the pcap_if_t that was used to populate this struct
    u32 Flags;
};

typedef int (*pcap_findalldevs_t)(pcap_if_t** alldevs, char* errbuf);
typedef void (*pcap_freealldevs_t)(pcap_if_t* alldevs);
typedef pcap_t* (*pcap_open_live_t)(const char* src, int snaplen, int flags, int readtimeout, char* errbuf);
typedef void (*pcap_close_t)(pcap_t* dev);
typedef int (*pcap_setnonblock_t)(pcap_t* dev, int nonblock, char* errbuf);
typedef int (*pcap_sendpacket_t)(pcap_t* dev, const u_char* data, int len);
typedef int (*pcap_dispatch_t)(pcap_t* dev, int num, pcap_handler callback, u_char* data);
typedef const u_char* (*pcap_next_t)(pcap_t* dev, struct pcap_pkthdr* hdr);

class Net_PCap;

class LibPCap
{
public:
    static std::optional<LibPCap> New() noexcept;
    LibPCap(const LibPCap&) = delete;
    LibPCap& operator=(const LibPCap&) = delete;
    LibPCap(LibPCap&&) noexcept;
    LibPCap& operator=(LibPCap&&) noexcept;
    ~LibPCap() noexcept = default;

    [[nodiscard]] std::unique_ptr<Net_PCap> Open(std::string_view devicename, const Platform::SendPacketCallback& handler) const noexcept;
    [[nodiscard]] std::unique_ptr<Net_PCap> Open(const AdapterData& device, const Platform::SendPacketCallback& handler) const noexcept;

    // so that Net_PCap objects can safely outlive LibPCap
    // (because the actual DLL will be kept loaded until no shared_ptrs remain)
    std::shared_ptr<Platform::DynamicLibrary> PCapLib = nullptr;
    pcap_findalldevs_t findalldevs = nullptr;
    pcap_freealldevs_t freealldevs = nullptr;
    pcap_open_live_t open_live = nullptr;
    pcap_close_t close = nullptr;
    pcap_setnonblock_t setnonblock = nullptr;
    pcap_sendpacket_t sendpacket = nullptr;
    pcap_dispatch_t dispatch = nullptr;
    pcap_next_t next = nullptr;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return
            PCapLib != nullptr &&
            findalldevs != nullptr &&
            freealldevs != nullptr &&
            open_live != nullptr &&
            close != nullptr &&
            setnonblock != nullptr &&
            sendpacket != nullptr &&
            dispatch != nullptr &&
            next != nullptr
        ;
    }

    /// @return List of all network interfaces available at the time of the call
    [[nodiscard]] std::vector<AdapterData> GetAdapters() const noexcept;
private:
    static bool TryLoadPCap(LibPCap& pcap, Platform::DynamicLibrary *lib) noexcept;
    LibPCap() noexcept = default;
};

class Net_PCap : public NetDriver
{
public:
    Net_PCap() noexcept = default;
    ~Net_PCap() noexcept override;
    Net_PCap(const Net_PCap&) = delete;
    Net_PCap& operator=(const Net_PCap&) = delete;
    Net_PCap(Net_PCap&& other) noexcept;
    Net_PCap& operator=(Net_PCap&& other) noexcept;

    int SendPacket(u8* data, int len) noexcept override;
    void RecvCheck() noexcept override;
private:
    friend class LibPCap;
    static void RXCallback(u_char* userdata, const pcap_pkthdr* header, const u_char* data) noexcept;

    pcap_t* PCapAdapter = nullptr;
    Platform::SendPacketCallback Callback;

    // To avoid undefined behavior in case the original LibPCap object is destroyed
    // before this interface is cleaned up
    std::shared_ptr<Platform::DynamicLibrary> PCapLib = nullptr;
    pcap_close_t close = nullptr;
    pcap_sendpacket_t sendpacket = nullptr;
    pcap_dispatch_t dispatch = nullptr;
};

}

#endif // NET_PCAP_H
