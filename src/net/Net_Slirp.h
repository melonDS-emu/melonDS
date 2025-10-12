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

#ifndef NET_SLIRP_H
#define NET_SLIRP_H

#include "types.h"
#include "FIFO.h"
#include "Platform.h"
#include "NetDriver.h"

#include <libslirp.h>

#ifdef __WIN32__
    #include <ws2tcpip.h>
#else
    #include <poll.h>
#endif

struct Slirp;

namespace melonDS
{
class Net_Slirp : public NetDriver
{
public:
    explicit Net_Slirp(const Platform::SendPacketCallback& callback) noexcept;
    Net_Slirp(const Net_Slirp&) = delete;
    Net_Slirp& operator=(const Net_Slirp&) = delete;
    Net_Slirp(Net_Slirp&& other) noexcept;
    Net_Slirp& operator=(Net_Slirp&& other) noexcept;
    ~Net_Slirp() noexcept override;

    int SendPacket(u8* data, int len) noexcept override;
    void RecvCheck() noexcept override;
private:
    static constexpr int PollListMax = 64;
    static const SlirpCb cb;
    static int SlirpCbGetREvents(int idx, void* opaque) noexcept;
    static int SlirpCbAddPoll(int fd, int events, void* opaque) noexcept;
    static ssize_t SlirpCbSendPacket(const void* buf, size_t len, void* opaque) noexcept;
    void HandleDNSFrame(u8* data, int len) noexcept;

    Platform::SendPacketCallback Callback;
    pollfd PollList[PollListMax] {};
    int PollListSize = 0;
    FIFO<u32, (0x8000 >> 2)> RXBuffer {};
    u32 IPv4ID = 0;
    Slirp* Ctx = nullptr;
};
}
#endif // NET_SLIRP_H
