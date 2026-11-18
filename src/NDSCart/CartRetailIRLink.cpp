/*
    Copyright 2016-2026 melonDS team

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

#include <chrono>
#include <cstring>
#include <deque>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #define socket_t    SOCKET
#else
    #include <errno.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/socket.h>

    #define socket_t    int
    #define closesocket close
#endif

#ifndef INVALID_SOCKET
    #define INVALID_SOCKET  (socket_t)-1
#endif

#ifndef MSG_NOSIGNAL
    #define MSG_NOSIGNAL 0
#endif

#include "CartRetailIRLink.h"
#include "../Platform.h"

namespace melonDS::NDSCart::IRLink
{
using Platform::Log;
using Platform::LogLevel;

namespace
{

constexpr u16 IRPort = 0xAAAA;
constexpr int IRRecordSize = 8;

// packets wait here until the game reads them; while the game isn't
// listening, the oldest are dropped so it doesn't later see stale ones
constexpr size_t IRMaxPackets = 8;

// timestamps on the link count the device's CPU clock, 3.6864 MHz
constexpr u32 IRDeviceClock = 3686400;
using IRCycles = std::chrono::duration<u64, std::ratio<1, IRDeviceClock>>;

// IR runs at 115200 baud, with 10 bits per character (start, 8 data, stop)
constexpr u32 IRBaudRate = 115200;
constexpr u32 IRCharBits = 10;

// one character, in device CPU cycles
constexpr u32 IRCharTime = IRDeviceClock * IRCharBits / IRBaudRate;

// the least gap between packets sent back to back, in characters
constexpr u32 IRPacketGap = 16;

socket_t IRSocket = INVALID_SOCKET;
std::chrono::steady_clock::time_point IRNextConnect;
u32 IRClock = 0;

// whether a packet is being sent
bool IRSending = false;

// real time in device CPU cycles, so the device sees the gaps the game leaves
// between packets
u32 IRRealClock(void)
{
    static const auto start = std::chrono::steady_clock::now();
    return (u32)std::chrono::duration_cast<IRCycles>(
        std::chrono::steady_clock::now() - start).count();
}

u8 IRRecord[IRRecordSize];
int IRRecordLen = 0;
std::vector<u8> IRBurst;
std::deque<std::vector<u8>> IRPackets;

void IRDisconnect(void)
{
    if (IRSocket == INVALID_SOCKET)
        return;

    closesocket(IRSocket);
    IRSocket = INVALID_SOCKET;
    IRRecordLen = 0;
    IRBurst.clear();
    Log(LogLevel::Info, "IR: disconnected\n");
}

bool IRConnect(void)
{
    if (IRSocket != INVALID_SOCKET)
        return true;

    // don't try more than once a second while nothing is listening
    auto now = std::chrono::steady_clock::now();
    if (now < IRNextConnect)
        return false;
    IRNextConnect = now + std::chrono::seconds(1);

#ifdef _WIN32
    static bool wsaStarted = false;
    if (!wsaStarted)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return false;
        wsaStarted = true;
    }
#endif

    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(IRPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // connecting to localhost succeeds or fails right away
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != 0)
    {
        closesocket(sock);
        return false;
    }

    // IR replies are expected within milliseconds, so don't let TCP batch them
    int nodelay = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

#ifdef _WIN32
    u_long nonblocking = 1;
    ioctlsocket(sock, FIONBIO, &nonblocking);
#else
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
    int nosigpipe = 1;
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
#endif
#endif

    IRSocket = sock;
    IRRecordLen = 0;
    IRBurst.clear();
    Log(LogLevel::Info, "IR: connected to port %d\n", IRPort);
    return true;
}

void IRSendRecord(u8 type, u8 val)
{
    if (!IRConnect())
        return;

    u8 record[IRRecordSize] =
    {
        type, val, 'I', 'R',
        (u8)(IRClock >> 24), (u8)(IRClock >> 16), (u8)(IRClock >> 8), (u8)IRClock
    };

    if (send(IRSocket, (const char*)record, IRRecordSize, MSG_NOSIGNAL) != IRRecordSize)
        IRDisconnect();
}

// reads whatever has arrived, and queues each completed burst as a packet
void IRPoll(void)
{
    if (!IRConnect())
        return;

    for (;;)
    {
        u8 data[256];
        int len = recv(IRSocket, (char*)data, sizeof(data), 0);

        if (len == 0)
        {
            IRDisconnect();
            return;
        }
        if (len < 0)
        {
#ifdef _WIN32
            bool wouldblock = WSAGetLastError() == WSAEWOULDBLOCK;
#else
            bool wouldblock = errno == EAGAIN || errno == EWOULDBLOCK;
#endif
            if (!wouldblock)
                IRDisconnect();
            return;
        }

        for (int i = 0; i < len; i++)
        {
            IRRecord[IRRecordLen++] = data[i];
            if (IRRecordLen < IRRecordSize)
                continue;

            if ((IRRecord[0] != 'B' && IRRecord[0] != 'T') ||
                IRRecord[2] != 'I' || IRRecord[3] != 'R')
            {
                // out of step with the stream: slide forward a byte and retry
                memmove(IRRecord, IRRecord + 1, IRRecordSize - 1);
                IRRecordLen--;
                continue;
            }
            IRRecordLen = 0;

            if (IRRecord[0] == 'B')
            {
                // the length is reported in one byte
                if (IRBurst.size() < 0xFF)
                    IRBurst.push_back(IRRecord[1]);
            }
            else if (!IRBurst.empty())
            {
                if (IRPackets.size() >= IRMaxPackets)
                    IRPackets.pop_front();
                IRPackets.push_back(std::move(IRBurst));
                IRBurst.clear();
            }
        }
    }
}

}

void Reset(void)
{
    IRPackets.clear();
}

void SendByte(u8 val)
{
    if (!IRSending)
    {
        // a new packet starts no earlier than now
        u32 now = IRRealClock();
        if ((s32)(now - IRClock) > 0)
            IRClock = now;
        IRSending = true;
    }

    IRSendRecord('B', val);
    IRClock += IRCharTime;
}

void EndPacket(void)
{
    if (!IRSending)
        return;

    IRSendRecord('T', 0);
    IRSending = false;

    // packets sent back to back still need a gap to stay apart
    IRClock += IRCharTime * IRPacketGap;
}

u32 Receive(void)
{
    IRPoll();
    return IRPackets.empty() ? 0 : IRPackets.front().size();
}

u8 ReceivedByte(u32 pos)
{
    if (IRPackets.empty() || pos >= IRPackets.front().size())
        return 0;
    return IRPackets.front()[pos];
}

void DropPacket(void)
{
    if (!IRPackets.empty())
        IRPackets.pop_front();
}

}
