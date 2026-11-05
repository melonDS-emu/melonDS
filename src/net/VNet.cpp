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

#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include <enet/enet.h>

#include "VNet.h"

// real platform socket functions, see enet/enet_platform_real.c
extern "C"
{
ENetSocket enet_real_socket_create(ENetSocketType);
int        enet_real_socket_bind(ENetSocket, const ENetAddress*);
int        enet_real_socket_get_address(ENetSocket, ENetAddress*);
int        enet_real_socket_listen(ENetSocket, int);
ENetSocket enet_real_socket_accept(ENetSocket, ENetAddress*);
int        enet_real_socket_connect(ENetSocket, const ENetAddress*);
int        enet_real_socket_send(ENetSocket, const ENetAddress*, const ENetBuffer*, size_t);
int        enet_real_socket_receive(ENetSocket, ENetAddress*, ENetBuffer*, size_t);
int        enet_real_socket_wait(ENetSocket, enet_uint32*, enet_uint32);
int        enet_real_socket_set_option(ENetSocket, ENetSocketOption, int);
int        enet_real_socket_get_option(ENetSocket, ENetSocketOption, int*);
int        enet_real_socket_shutdown(ENetSocket, ENetSocketShutdown);
void       enet_real_socket_destroy(ENetSocket);
int        enet_real_socketset_select(ENetSocket, ENetSocketSet*, ENetSocketSet*, enet_uint32);
}

namespace VNet
{

// relay protocol
// every packet starts with 'M' 'R' <version> <type>
// IPv4 addresses are sent as 4 raw bytes (network order), ports as big-endian u16

const uint8_t kProtoVersion = 1;
const int kDefaultRelayPort = 7070;
const int kMaxDatagram = 2048;

enum
{
    Pkt_Join = 1,       // c->s: nonce(4) wantip(4) roomlen(1) room
    Pkt_Welcome,        // s->c: nonce(4) vip(4)
    Pkt_Send,           // c->s: dstip(4) dstport(2) srcport(2) payload
    Pkt_Recv,           // s->c: srcip(4) srcport(2) dstport(2) payload
    Pkt_Ping,           // c->s: time(4)
    Pkt_Pong,           // s->c: time(4)
    Pkt_Leave,          // c->s
    Pkt_Error,          // s->c: code(1)
};

enum
{
    Err_NotJoined = 1,
    Err_RoomFull,
    Err_BadRoom,
};

// virtual socket handles live far above anything the OS hands out
const uintptr_t kVirtualBase = 0x7FFF0000;
const int kMaxVirtualSockets = 64;

const uint16_t kPreferredEphemeralPort = 7064;
const uint16_t kEphemeralBase = 50000;

struct Datagram
{
    uint32_t SrcIP;
    uint16_t SrcPort;
    std::vector<uint8_t> Data;
};

struct VSocket
{
    bool Used = false;
    uint16_t Port = 0; // 0 = not bound yet
    std::deque<Datagram> RX;
};

std::mutex Lock;
std::condition_variable RXCond;

std::atomic<bool> Active(false);
std::atomic<bool> ThreadRunning(false);
std::thread RecvThread;

ENetSocket RelaySocket = ENET_SOCKET_NULL;
ENetAddress RelayAddress;
std::string RoomName;
uint32_t JoinNonce = 0;
std::atomic<uint32_t> VirtualIP(0);
std::atomic<int> RelayPing(-1);

VSocket Sockets[kMaxVirtualSockets];


static uint32_t GetMS()
{
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static void Put16(uint8_t* p, uint16_t v) { p[0] = v >> 8; p[1] = v & 0xFF; }
static void Put32(uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = (v >> 16) & 0xFF; p[2] = (v >> 8) & 0xFF; p[3] = v & 0xFF; }
static uint16_t Get16(const uint8_t* p) { return (p[0] << 8) | p[1]; }
static uint32_t Get32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

static void WriteHeader(uint8_t* p, uint8_t type)
{
    p[0] = 'M'; p[1] = 'R'; p[2] = kProtoVersion; p[3] = type;
}

static int SendRaw(const uint8_t* data, size_t len)
{
    if (RelaySocket == ENET_SOCKET_NULL) return -1;

    ENetBuffer buf;
    buf.data = (void*)data;
    buf.dataLength = len;
    return enet_real_socket_send(RelaySocket, &RelayAddress, &buf, 1);
}

static void SendJoin()
{
    uint8_t pkt[4+4+4+1+255];
    WriteHeader(pkt, Pkt_Join);
    Put32(&pkt[4], JoinNonce);
    uint32_t vip = VirtualIP;
    memcpy(&pkt[8], &vip, 4);
    size_t roomlen = RoomName.size();
    if (roomlen > 255) roomlen = 255;
    pkt[12] = (uint8_t)roomlen;
    memcpy(&pkt[13], RoomName.data(), roomlen);
    SendRaw(pkt, 13 + roomlen);
}

static void SendPing()
{
    uint8_t pkt[8];
    WriteHeader(pkt, Pkt_Ping);
    Put32(&pkt[4], GetMS());
    SendRaw(pkt, 8);
}

static VSocket* GetVSocket(ENetSocket sock)
{
    uintptr_t h = (uintptr_t)sock;
    if (h < kVirtualBase || h >= kVirtualBase + kMaxVirtualSockets) return nullptr;
    VSocket* s = &Sockets[h - kVirtualBase];
    if (!s->Used) return nullptr;
    return s;
}

static bool IsVirtual(ENetSocket sock)
{
    uintptr_t h = (uintptr_t)sock;
    return (h >= kVirtualBase && h < kVirtualBase + kMaxVirtualSockets);
}

static bool PortInUse(uint16_t port)
{
    for (int i = 0; i < kMaxVirtualSockets; i++)
    {
        if (Sockets[i].Used && Sockets[i].Port == port)
            return true;
    }
    return false;
}

// must be called with Lock held
static void AutoBind(VSocket* s)
{
    if (s->Port) return;

    // each player has their own virtual IP, so the LAN port is usually free
    // binding to it lets other clients reach us directly (mesh connections)
    if (!PortInUse(kPreferredEphemeralPort))
    {
        s->Port = kPreferredEphemeralPort;
        return;
    }

    for (uint16_t p = kEphemeralBase; p != 0; p++)
    {
        if (!PortInUse(p))
        {
            s->Port = p;
            return;
        }
    }
}

static void HandleRelayPacket(const uint8_t* data, int len)
{
    if (len < 4) return;
    if (data[0] != 'M' || data[1] != 'R' || data[2] != kProtoVersion) return;

    switch (data[3])
    {
    case Pkt_Welcome:
        {
            if (len < 12) return;
            if (Get32(&data[4]) != JoinNonce) return;
            uint32_t vip;
            memcpy(&vip, &data[8], 4);
            {
                std::lock_guard<std::mutex> lk(Lock);
                VirtualIP = vip;
            }
            RXCond.notify_all();
        }
        break;

    case Pkt_Recv:
        {
            if (len < 12) return;

            Datagram dgram;
            memcpy(&dgram.SrcIP, &data[4], 4);
            dgram.SrcPort = Get16(&data[8]);
            uint16_t dstport = Get16(&data[10]);
            dgram.Data.assign(&data[12], &data[len]);

            std::lock_guard<std::mutex> lk(Lock);
            for (int i = 0; i < kMaxVirtualSockets; i++)
            {
                VSocket* s = &Sockets[i];
                if (!s->Used || s->Port != dstport) continue;

                if (s->RX.size() < 256)
                    s->RX.push_back(std::move(dgram));
                break;
            }
        }
        RXCond.notify_all();
        break;

    case Pkt_Pong:
        {
            if (len < 8) return;
            uint32_t sent = Get32(&data[4]);
            uint32_t rtt = GetMS() - sent;
            if (rtt < 10000) RelayPing = (int)rtt;
        }
        break;

    case Pkt_Error:
        {
            if (len < 5) return;
            if (data[4] == Err_NotJoined)
            {
                // the relay forgot about us (restart, NAT rebinding...): join again
                SendJoin();
            }
        }
        break;
    }
}

static void RecvThreadFunc()
{
    uint8_t buf[kMaxDatagram + 16];
    uint32_t lastping = 0;

    while (ThreadRunning)
    {
        uint32_t now = GetMS();
        if ((now - lastping) >= 2000)
        {
            lastping = now;
            if (VirtualIP)
                SendPing();
            else
                SendJoin();
        }

        enet_uint32 cond = ENET_SOCKET_WAIT_RECEIVE;
        if (enet_real_socket_wait(RelaySocket, &cond, 100) != 0)
            continue;
        if (!(cond & ENET_SOCKET_WAIT_RECEIVE))
            continue;

        for (;;)
        {
            ENetAddress from;
            ENetBuffer ebuf;
            ebuf.data = buf;
            ebuf.dataLength = sizeof(buf);
            int len = enet_real_socket_receive(RelaySocket, &from, &ebuf, 1);
            if (len <= 0) break;
            if (from.host != RelayAddress.host || from.port != RelayAddress.port) continue;

            HandleRelayPacket(buf, len);
        }
    }
}


bool Start(const char* server, const char* room, std::string& error)
{
    Stop();

    if (!room || !room[0])
    {
        error = "Please enter a room name.";
        return false;
    }
    if (strlen(room) > 64)
    {
        error = "The room name is too long.";
        return false;
    }

    std::string host = server ? server : "";
    int port = kDefaultRelayPort;
    size_t colon = host.rfind(':');
    if (colon != std::string::npos)
    {
        port = atoi(host.substr(colon + 1).c_str());
        host = host.substr(0, colon);
    }
    if (host.empty() || port <= 0 || port > 65535)
    {
        error = "Invalid relay server address.";
        return false;
    }

    if (enet_initialize() != 0)
    {
        error = "Failed to initialize networking.";
        return false;
    }

    if (enet_address_set_host(&RelayAddress, host.c_str()) != 0)
    {
        error = "Could not resolve the relay server address.";
        enet_deinitialize();
        return false;
    }
    RelayAddress.port = (enet_uint16)port;

    RelaySocket = enet_real_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (RelaySocket == ENET_SOCKET_NULL)
    {
        error = "Failed to create a socket.";
        enet_deinitialize();
        return false;
    }
    enet_real_socket_set_option(RelaySocket, ENET_SOCKOPT_NONBLOCK, 1);
    enet_real_socket_set_option(RelaySocket, ENET_SOCKOPT_RCVBUF, 256 * 1024);
    enet_real_socket_set_option(RelaySocket, ENET_SOCKOPT_SNDBUF, 256 * 1024);

    RoomName = room;
    JoinNonce = (uint32_t)std::chrono::high_resolution_clock::now().time_since_epoch().count() ^ 0x5A5A1234;
    VirtualIP = 0;
    RelayPing = -1;

    ThreadRunning = true;
    RecvThread = std::thread(RecvThreadFunc);

    // wait for the relay to assign us a virtual IP
    {
        std::unique_lock<std::mutex> lk(Lock);
        RXCond.wait_for(lk, std::chrono::milliseconds(4000), [] { return VirtualIP != 0; });
    }

    if (!VirtualIP)
    {
        error = "No answer from the relay server. Check the server address and your internet connection.";
        Stop();
        return false;
    }

    SendPing();
    Active = true;
    return true;
}

void Stop()
{
    if (ThreadRunning)
    {
        if (VirtualIP)
        {
            uint8_t pkt[4];
            WriteHeader(pkt, Pkt_Leave);
            SendRaw(pkt, 4);
        }

        ThreadRunning = false;
        RecvThread.join();
    }

    Active = false;

    if (RelaySocket != ENET_SOCKET_NULL)
    {
        enet_real_socket_destroy(RelaySocket);
        RelaySocket = ENET_SOCKET_NULL;
        enet_deinitialize();
    }

    {
        std::lock_guard<std::mutex> lk(Lock);
        VirtualIP = 0;
        for (int i = 0; i < kMaxVirtualSockets; i++)
            Sockets[i].RX.clear();
    }
    RXCond.notify_all();

    RelayPing = -1;
}

bool IsActive()
{
    return Active;
}

unsigned int GetVirtualIP()
{
    return VirtualIP;
}

std::string GetVirtualIPString()
{
    uint32_t vip = VirtualIP;
    const uint8_t* b = (const uint8_t*)&vip;
    char str[16];
    snprintf(str, sizeof(str), "%d.%d.%d.%d", b[0], b[1], b[2], b[3]);
    return str;
}

int GetRelayPing()
{
    return RelayPing;
}


// virtual socket implementation

static ENetSocket VCreate()
{
    std::lock_guard<std::mutex> lk(Lock);
    for (int i = 0; i < kMaxVirtualSockets; i++)
    {
        VSocket* s = &Sockets[i];
        if (s->Used) continue;

        s->Used = true;
        s->Port = 0;
        s->RX.clear();
        return (ENetSocket)(kVirtualBase + i);
    }
    return ENET_SOCKET_NULL;
}

static int VBind(ENetSocket sock, const ENetAddress* addr)
{
    std::lock_guard<std::mutex> lk(Lock);
    VSocket* s = GetVSocket(sock);
    if (!s) return -1;

    if (!addr || addr->port == 0)
    {
        AutoBind(s);
        return 0;
    }

    if (PortInUse(addr->port)) return -1;
    s->Port = addr->port;
    return 0;
}

static int VGetAddress(ENetSocket sock, ENetAddress* addr)
{
    std::lock_guard<std::mutex> lk(Lock);
    VSocket* s = GetVSocket(sock);
    if (!s) return -1;

    AutoBind(s);
    addr->host = VirtualIP;
    addr->port = s->Port;
    return 0;
}

static int VSend(ENetSocket sock, const ENetAddress* addr, const ENetBuffer* buffers, size_t count)
{
    uint8_t pkt[12 + kMaxDatagram];
    size_t len = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (len + buffers[i].dataLength > kMaxDatagram) return -1;
        memcpy(&pkt[12 + len], buffers[i].data, buffers[i].dataLength);
        len += buffers[i].dataLength;
    }

    {
        std::lock_guard<std::mutex> lk(Lock);
        VSocket* s = GetVSocket(sock);
        if (!s || !addr) return -1;

        AutoBind(s);
        WriteHeader(pkt, Pkt_Send);
        memcpy(&pkt[4], &addr->host, 4);
        Put16(&pkt[8], addr->port);
        Put16(&pkt[10], s->Port);
    }

    if (SendRaw(pkt, 12 + len) < 0) return -1;
    return (int)len;
}

static int VReceive(ENetSocket sock, ENetAddress* addr, ENetBuffer* buffers, size_t count)
{
    std::lock_guard<std::mutex> lk(Lock);
    VSocket* s = GetVSocket(sock);
    if (!s) return -1;
    if (s->RX.empty()) return 0;

    Datagram dgram = std::move(s->RX.front());
    s->RX.pop_front();

    size_t pos = 0;
    for (size_t i = 0; i < count && pos < dgram.Data.size(); i++)
    {
        size_t chunk = dgram.Data.size() - pos;
        if (chunk > buffers[i].dataLength) chunk = buffers[i].dataLength;
        memcpy(buffers[i].data, &dgram.Data[pos], chunk);
        pos += chunk;
    }
    if (pos < dgram.Data.size()) return -2; // truncated

    if (addr)
    {
        addr->host = dgram.SrcIP;
        addr->port = dgram.SrcPort;
    }
    return (int)dgram.Data.size();
}

static int VWait(ENetSocket sock, enet_uint32* condition, enet_uint32 timeout)
{
    enet_uint32 want = *condition;
    *condition = ENET_SOCKET_WAIT_NONE;

    // sending never blocks
    if (want & ENET_SOCKET_WAIT_SEND)
    {
        *condition |= ENET_SOCKET_WAIT_SEND;
        timeout = 0;
    }

    if (want & ENET_SOCKET_WAIT_RECEIVE)
    {
        std::unique_lock<std::mutex> lk(Lock);
        VSocket* s = GetVSocket(sock);
        if (!s) return -1;

        RXCond.wait_for(lk, std::chrono::milliseconds(timeout), [s] { return !s->RX.empty() || !s->Used; });
        if (!s->RX.empty())
            *condition |= ENET_SOCKET_WAIT_RECEIVE;
    }

    return 0;
}

static void VDestroy(ENetSocket sock)
{
    {
        std::lock_guard<std::mutex> lk(Lock);
        VSocket* s = GetVSocket(sock);
        if (!s) return;

        s->Used = false;
        s->Port = 0;
        s->RX.clear();
    }
    RXCond.notify_all();
}

}


// public ENet socket API: virtual sockets while VNet is active, real sockets otherwise

using namespace VNet;

extern "C"
{

ENetSocket enet_socket_create(ENetSocketType type)
{
    if (Active && type == ENET_SOCKET_TYPE_DATAGRAM)
        return VCreate();

    return enet_real_socket_create(type);
}

int enet_socket_bind(ENetSocket sock, const ENetAddress* addr)
{
    if (IsVirtual(sock)) return VBind(sock, addr);
    return enet_real_socket_bind(sock, addr);
}

int enet_socket_get_address(ENetSocket sock, ENetAddress* addr)
{
    if (IsVirtual(sock)) return VGetAddress(sock, addr);
    return enet_real_socket_get_address(sock, addr);
}

int enet_socket_listen(ENetSocket sock, int backlog)
{
    if (IsVirtual(sock)) return -1;
    return enet_real_socket_listen(sock, backlog);
}

ENetSocket enet_socket_accept(ENetSocket sock, ENetAddress* addr)
{
    if (IsVirtual(sock)) return ENET_SOCKET_NULL;
    return enet_real_socket_accept(sock, addr);
}

int enet_socket_connect(ENetSocket sock, const ENetAddress* addr)
{
    if (IsVirtual(sock)) return -1;
    return enet_real_socket_connect(sock, addr);
}

int enet_socket_send(ENetSocket sock, const ENetAddress* addr, const ENetBuffer* buffers, size_t count)
{
    if (IsVirtual(sock)) return VSend(sock, addr, buffers, count);
    return enet_real_socket_send(sock, addr, buffers, count);
}

int enet_socket_receive(ENetSocket sock, ENetAddress* addr, ENetBuffer* buffers, size_t count)
{
    if (IsVirtual(sock)) return VReceive(sock, addr, buffers, count);
    return enet_real_socket_receive(sock, addr, buffers, count);
}

int enet_socket_wait(ENetSocket sock, enet_uint32* condition, enet_uint32 timeout)
{
    if (IsVirtual(sock)) return VWait(sock, condition, timeout);
    return enet_real_socket_wait(sock, condition, timeout);
}

int enet_socket_set_option(ENetSocket sock, ENetSocketOption option, int value)
{
    if (IsVirtual(sock)) return 0;
    return enet_real_socket_set_option(sock, option, value);
}

int enet_socket_get_option(ENetSocket sock, ENetSocketOption option, int* value)
{
    if (IsVirtual(sock))
    {
        *value = 0;
        return 0;
    }
    return enet_real_socket_get_option(sock, option, value);
}

int enet_socket_shutdown(ENetSocket sock, ENetSocketShutdown how)
{
    if (IsVirtual(sock)) return 0;
    return enet_real_socket_shutdown(sock, how);
}

void enet_socket_destroy(ENetSocket sock)
{
    if (IsVirtual(sock))
    {
        VDestroy(sock);
        return;
    }
    enet_real_socket_destroy(sock);
}

int enet_socketset_select(ENetSocket maxsock, ENetSocketSet* readset, ENetSocketSet* writeset, enet_uint32 timeout)
{
    return enet_real_socketset_select(maxsock, readset, writeset, timeout);
}

}
