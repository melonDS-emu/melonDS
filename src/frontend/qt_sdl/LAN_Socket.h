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

#ifndef LAN_SOCKET_H
#define LAN_SOCKET_H

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "FIFO.h"
#include "types.h"

struct Slirp;
struct SlirpCb;

namespace melonDS
{
class LAN_Socket
{
public:
    LAN_Socket() noexcept;
    ~LAN_Socket() noexcept;
    LAN_Socket(const LAN_Socket&) = delete;
    LAN_Socket& operator=(const LAN_Socket&) = delete;
    // Can't move this because the Slirp ctx has a pointer to this object
    LAN_Socket(LAN_Socket&&) = delete;
    LAN_Socket& operator=(LAN_Socket&&) = delete;

    int SendPacket(u8* data, int len);
    int RecvPacket(u8* data);
private:
    static constexpr int PollListMax = 64;
    static SlirpCb cb;
    static ssize_t SlirpCbSendPacket(const void* buf, size_t len, void* opaque) noexcept;
    static void SlirpCbGuestError(const char* msg, void* opaque) noexcept;
    static int SlirpCbAddPoll(int fd, int events, void* opaque) noexcept;
    static int SlirpCbGetREvents(int idx, void* opaque) noexcept;
    void RXEnqueue(const void* buf, int len) noexcept;
    void HandleDNSFrame(u8* data, int len) noexcept;

    pollfd PollList[PollListMax] {};
    int PollListSize = 0;
    FIFO<u32, (0x8000 >> 2)> RXBuffer {};
    u32 IPv4ID = 0;
    Slirp* Ctx = nullptr;
};
}

#endif // LAN_SOCKET_H
