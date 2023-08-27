/*
    Copyright 2016-2022 melonDS team

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

// indirect LAN interface, powered by BSD sockets.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Wifi.h"
#include "LAN_Socket.h"
#include "FIFO.h"
#include "Platform.h"

#include <slirp/libslirp.h>

#ifdef __WIN32__
	#include <ws2tcpip.h>
#else
	#include <sys/socket.h>
	#include <netdb.h>
	#include <poll.h>
	#include <time.h>
#endif


namespace LAN_Socket
{

using Platform::Log;
using Platform::LogLevel;

const u32 kSubnet   = 0x0A400000;
const u32 kServerIP = kSubnet | 0x01;
const u32 kDNSIP    = kSubnet | 0x02;
const u32 kClientIP = kSubnet | 0x10;

const u8 kServerMAC[6] = {0x00, 0xAB, 0x33, 0x28, 0x99, 0x44};

FIFO<u32, (0x8000 >> 2)> RXBuffer;

u32 IPv4ID;

Slirp* Ctx = nullptr;

/*const int FDListMax = 64;
struct pollfd FDList[FDListMax];
int FDListSize;*/


#ifdef __WIN32__

#define poll WSAPoll

// https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows

struct timespec { long tv_sec; long tv_nsec; };
#define CLOCK_MONOTONIC 1312

int clock_gettime(int, struct timespec *spec)
{
    __int64 wintime;
    GetSystemTimeAsFileTime((FILETIME*)&wintime);
    wintime -=116444736000000000LL;                 //1jan1601 to 1jan1970
    spec->tv_sec  = wintime / 10000000LL;           //seconds
    spec->tv_nsec = wintime % 10000000LL * 100;     //nano-seconds
    return 0;
}

#endif // __WIN32__


void RXEnqueue(const void* buf, int len)
{
    int alignedlen = (len + 3) & ~3;
    int totallen = alignedlen + 4;

    if (!RXBuffer.CanFit(totallen >> 2))
    {
        Log(LogLevel::Warn, "slirp: !! NOT ENOUGH SPACE IN RX BUFFER\n");
        return;
    }

    u32 header = (alignedlen & 0xFFFF) | (len << 16);
    RXBuffer.Write(header);
    for (int i = 0; i < alignedlen; i += 4)
        RXBuffer.Write(((u32*)buf)[i>>2]);
}

ssize_t SlirpCbSendPacket(const void* buf, size_t len, void* opaque)
{
    if (len > 2048)
    {
        Log(LogLevel::Warn, "slirp: packet too big (%zu)\n", len);
        return 0;
    }

    Log(LogLevel::Debug, "slirp: response packet of %zu bytes, type %04X\n", len, ntohs(((u16*)buf)[6]));

    RXEnqueue(buf, len);

    return len;
}

void SlirpCbGuestError(const char* msg, void* opaque)
{
    Log(LogLevel::Error, "SLIRP: error: %s\n", msg);
}

int64_t SlirpCbClockGetNS(void* opaque)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void* SlirpCbTimerNew(SlirpTimerCb cb, void* cb_opaque, void* opaque)
{
    return nullptr;
}

void SlirpCbTimerFree(void* timer, void* opaque)
{
}

void SlirpCbTimerMod(void* timer, int64_t expire_time, void* opaque)
{
}

void SlirpCbRegisterPollFD(int fd, void* opaque)
{
    Log(LogLevel::Debug, "Slirp: register poll FD %d\n", fd);

    /*if (FDListSize >= FDListMax)
    {
        printf("!! SLIRP FD LIST FULL\n");
        return;
    }

    for (int i = 0; i < FDListSize; i++)
    {
        if (FDList[i].fd == fd) return;
    }

    FDList[FDListSize].fd = fd;
    FDListSize++;*/
}

void SlirpCbUnregisterPollFD(int fd, void* opaque)
{
    Log(LogLevel::Debug, "Slirp: unregister poll FD %d\n", fd);

    /*if (FDListSize < 1)
    {
        printf("!! SLIRP FD LIST EMPTY\n");
        return;
    }

    for (int i = 0; i < FDListSize; i++)
    {
        if (FDList[i].fd == fd)
        {
            FDListSize--;
            FDList[i] = FDList[FDListSize];
        }
    }*/
}

void SlirpCbNotify(void* opaque)
{
    Log(LogLevel::Debug, "Slirp: notify???\n");
}

SlirpCb cb =
{
    .send_packet = SlirpCbSendPacket,
    .guest_error = SlirpCbGuestError,
    .clock_get_ns = SlirpCbClockGetNS,
    .timer_new = SlirpCbTimerNew,
    .timer_free = SlirpCbTimerFree,
    .timer_mod = SlirpCbTimerMod,
    .register_poll_fd = SlirpCbRegisterPollFD,
    .unregister_poll_fd = SlirpCbUnregisterPollFD,
    .notify = SlirpCbNotify
};

bool Init()
{
    IPv4ID = 0;

    //FDListSize = 0;
    //memset(FDList, 0, sizeof(FDList));

    SlirpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.version = 1;

    cfg.in_enabled = true;
    *(u32*)&cfg.vnetwork = htonl(kSubnet);
    *(u32*)&cfg.vnetmask = htonl(0xFFFFFF00);
    *(u32*)&cfg.vhost = htonl(kServerIP);
    cfg.vhostname = "melonServer";
    *(u32*)&cfg.vdhcp_start = htonl(kClientIP);
    *(u32*)&cfg.vnameserver = htonl(kDNSIP);

    Ctx = slirp_new(&cfg, &cb, nullptr);

    return true;
}

void DeInit()
{
    if (Ctx)
    {
        slirp_cleanup(Ctx);
        Ctx = nullptr;
    }
}


void FinishUDPFrame(u8* data, int len)
{
    u8* ipheader = &data[0xE];
    u8* udpheader = &data[0x22];

    // lengths
    *(u16*)&ipheader[2] = htons(len - 0xE);
    *(u16*)&udpheader[4] = htons(len - (0xE + 0x14));

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
    tmp += (len-0x22);
    for (u8* i = udpheader; i < &udpheader[len-0x23]; i += 2)
        tmp += ntohs(*(u16*)i);
    if (len & 1)
        tmp += ntohs((u_short)udpheader[len-0x23]);
    while (tmp >> 16)
        tmp = (tmp & 0xFFFF) + (tmp >> 16);
    tmp ^= 0xFFFF;
    if (tmp == 0) tmp = 0xFFFF;
    *(u16*)&udpheader[6] = htons(tmp);
}

void HandleDNSFrame(u8* data, int len)
{
    u8* ipheader = &data[0xE];
    u8* udpheader = &data[0x22];
    u8* dnsbody = &data[0x2A];

    u32 srcip = ntohl(*(u32*)&ipheader[12]);
    u16 srcport = ntohs(*(u16*)&udpheader[0]);

    u16 id = ntohs(*(u16*)&dnsbody[0]);
    u16 flags = ntohs(*(u16*)&dnsbody[2]);
    u16 numquestions = ntohs(*(u16*)&dnsbody[4]);
    u16 numanswers = ntohs(*(u16*)&dnsbody[6]);
    u16 numauth = ntohs(*(u16*)&dnsbody[8]);
    u16 numadd = ntohs(*(u16*)&dnsbody[10]);

    Log(LogLevel::Debug, "DNS: ID=%04X, flags=%04X, Q=%d, A=%d, auth=%d, add=%d\n",
           id, flags, numquestions, numanswers, numauth, numadd);

    // for now we only take 'simple' DNS requests
    if (flags & 0x8000) return;
    if (numquestions != 1 || numanswers != 0) return;

    u8 resp[1024];
    u8* out = &resp[0];

    // ethernet
    memcpy(out, &data[6], 6); out += 6;
    memcpy(out, kServerMAC, 6); out += 6;
    *(u16*)out = htons(0x0800); out += 2;

    // IP
    u8* resp_ipheader = out;
    *out++ = 0x45;
    *out++ = 0x00;
    *(u16*)out = 0; out += 2; // total length
    *(u16*)out = htons(IPv4ID); out += 2; IPv4ID++;
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x80; // TTL
    *out++ = 0x11; // protocol (UDP)
    *(u16*)out = 0; out += 2; // checksum
    *(u32*)out = htonl(kDNSIP); out += 4; // source IP
    *(u32*)out = htonl(srcip); out += 4; // destination IP

    // UDP
    u8* resp_udpheader = out;
    *(u16*)out = htons(53); out += 2; // source port
    *(u16*)out = htons(srcport); out += 2; // destination port
    *(u16*)out = 0; out += 2; // length
    *(u16*)out = 0; out += 2; // checksum

    // DNS
    u8* resp_body = out;
    *(u16*)out = htons(id); out += 2; // ID
    *(u16*)out = htons(0x8000); out += 2; // flags
    *(u16*)out = htons(numquestions); out += 2; // num questions
    *(u16*)out = htons(numquestions); out += 2; // num answers
    *(u16*)out = 0; out += 2; // num authority
    *(u16*)out = 0; out += 2; // num additional

    u32 curoffset = 12;
    for (u16 i = 0; i < numquestions; i++)
    {
        if (curoffset >= (len-0x2A)) return;

        u8 bitlength = 0;
        while ((bitlength = dnsbody[curoffset++]) != 0)
            curoffset += bitlength;

        curoffset += 4;
    }

    u32 qlen = curoffset-12;
    if (qlen > 512) return;
    memcpy(out, &dnsbody[12], qlen); out += qlen;

    curoffset = 12;
	for (u16 i = 0; i < numquestions; i++)
	{
		// assemble the requested domain name
		u8 bitlength = 0;
		char domainname[256] = ""; int o = 0;
		while ((bitlength = dnsbody[curoffset++]) != 0)
		{
		    if ((o+bitlength) >= 255)
            {
                // welp. atleast try not to explode.
                domainname[o++] = '\0';
                break;
            }

			strncpy(&domainname[o], (const char *)&dnsbody[curoffset], bitlength);
			o += bitlength;

			curoffset += bitlength;
			if (dnsbody[curoffset] != 0)
				domainname[o++] = '.';
            else
                domainname[o++] = '\0';
		}

		u16 type = ntohs(*(u16*)&dnsbody[curoffset]);
		u16 cls = ntohs(*(u16*)&dnsbody[curoffset+2]);

		printf("- q%d: %04X %04X %s", i, type, cls, domainname);

		// get answer
		struct addrinfo dns_hint;
		struct addrinfo* dns_res;
		u32 addr_res;

		memset(&dns_hint, 0, sizeof(dns_hint));
		dns_hint.ai_family = AF_INET; // TODO: other address types (INET6, etc)
		if (getaddrinfo(domainname, "0", &dns_hint, &dns_res) == 0)
        {
            struct addrinfo* p = dns_res;
            while (p)
            {
                struct sockaddr_in* addr = (struct sockaddr_in*)p->ai_addr;
                addr_res = *(u32*)&addr->sin_addr;

                printf(" -> %d.%d.%d.%d",
                       addr_res & 0xFF, (addr_res >> 8) & 0xFF,
                       (addr_res >> 16) & 0xFF, addr_res >> 24);

                break;
                p = p->ai_next;
            }
        }
        else
        {
            printf(" shat itself :(");
            addr_res = 0;
        }

		printf("\n");
		curoffset += 4;

		// TODO: betterer support
		// (under which conditions does the C00C marker work?)
		*(u16*)out = htons(0xC00C); out += 2;
		*(u16*)out = htons(type); out += 2;
		*(u16*)out = htons(cls); out += 2;
		*(u32*)out = htonl(3600); out += 4; // TTL (hardcoded for now)
		*(u16*)out = htons(4); out += 2; // address length
		*(u32*)out = addr_res; out += 4; // address
    }

    u32 framelen = (u32)(out - &resp[0]);
    if (framelen & 1) { *out++ = 0; framelen++; }
    FinishUDPFrame(resp, framelen);

    RXEnqueue(resp, framelen);
}

int SendPacket(u8* data, int len)
{
    if (!Ctx) return 0;

    if (len > 2048)
    {
        Log(LogLevel::Error, "LAN_SendPacket: error: packet too long (%d)\n", len);
        return 0;
    }

    u16 ethertype = ntohs(*(u16*)&data[0xC]);

    if (ethertype == 0x800)
    {
        u8 protocol = data[0x17];
        if (protocol == 0x11) // UDP
        {
            u16 dstport = ntohs(*(u16*)&data[0x24]);
            if (dstport == 53 && htonl(*(u32*)&data[0x1E]) == kDNSIP) // DNS
            {
                HandleDNSFrame(data, len);
                return len;
            }
        }
    }

    slirp_input(Ctx, data, len);
    return len;
}

const int PollListMax = 64;
struct pollfd PollList[PollListMax];
int PollListSize;

int SlirpCbAddPoll(int fd, int events, void* opaque)
{
    if (PollListSize >= PollListMax)
    {
        Log(LogLevel::Error, "slirp: POLL LIST FULL\n");
        return -1;
    }

    int idx = PollListSize++;

    //printf("Slirp: add poll: fd=%d, idx=%d, events=%08X\n", fd, idx, events);

    u16 evt = 0;

    if (events & SLIRP_POLL_IN) evt |= POLLIN;
    if (events & SLIRP_POLL_OUT) evt |= POLLWRNORM;

#ifndef __WIN32__
    // CHECKME
    if (events & SLIRP_POLL_PRI) evt |= POLLPRI;
    if (events & SLIRP_POLL_ERR) evt |= POLLERR;
    if (events & SLIRP_POLL_HUP) evt |= POLLHUP;
#endif // !__WIN32__

    PollList[idx].fd = fd;
    PollList[idx].events = evt;

    return idx;
}

int SlirpCbGetREvents(int idx, void* opaque)
{
    if (idx < 0 || idx >= PollListSize)
        return 0;

    //printf("Slirp: get revents, idx=%d, res=%04X\n", idx, FDList[idx].revents);

    u16 evt = PollList[idx].revents;
    int ret = 0;

    if (evt & POLLIN) ret |= SLIRP_POLL_IN;
    if (evt & POLLWRNORM) ret |= SLIRP_POLL_OUT;
    if (evt & POLLPRI) ret |= SLIRP_POLL_PRI;
    if (evt & POLLERR) ret |= SLIRP_POLL_ERR;
    if (evt & POLLHUP) ret |= SLIRP_POLL_HUP;

    return ret;
}

int RecvPacket(u8* data)
{
    if (!Ctx) return 0;

    int ret = 0;

    //if (PollListSize > 0)
    {
        u32 timeout = 0;
        PollListSize = 0;
        slirp_pollfds_fill(Ctx, &timeout, SlirpCbAddPoll, nullptr);
        int res = poll(PollList, PollListSize, timeout);
        slirp_pollfds_poll(Ctx, res<0, SlirpCbGetREvents, nullptr);
    }

    if (!RXBuffer.IsEmpty())
    {
        u32 header = RXBuffer.Read();
        u32 len = header & 0xFFFF;

        for (int i = 0; i < len; i += 4)
            ((u32*)data)[i>>2] = RXBuffer.Read();

        ret = header >> 16;
    }

    return ret;
}

}
