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

// indirect LAN interface, powered by BSD sockets.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Wifi.h"
#include "LAN_Socket.h"
#include "../Config.h"

#ifdef __WIN32__
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define socket_t    SOCKET
	#define sockaddr_t  SOCKADDR
#else
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <sys/select.h>
	#include <sys/socket.h>
	#define socket_t    int
	#define sockaddr_t  struct sockaddr
	#define closesocket close
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET  (socket_t)-1
#endif


namespace LAN_Socket
{

const u32 kSubnet   = 0x0A400000;
const u32 kServerIP = kSubnet | 0x01;
const u32 kDNSIP    = kSubnet | 0x02;
const u32 kClientIP = kSubnet | 0x10;

const u8 kServerMAC[6] = {0x00, 0xAB, 0x33, 0x28, 0x99, 0x44};
const u8 kDNSMAC[6]    = {0x00, 0xAB, 0x33, 0x28, 0x99, 0x55};

u8 PacketBuffer[2048];
int PacketLen;
volatile int RXNum;

u16 IPv4ID;


// TODO: UDP sockets
// * use FIFO list
// * assign new socket when seeing new IP/port


typedef struct
{
    u8 DestIP[4];
    u16 SourcePort;
    u16 DestPort;

    u32 SeqNum; // sequence number for incoming frames
    u32 AckNum;

    // 0: unused
    // 1: connected
    u8 Status;

    SOCKET Backend;

} TCPSocket;

typedef struct
{
    u8 DestIP[4];
    u16 SourcePort;
    u16 DestPort;

    SOCKET Backend;
    struct sockaddr_in BackendAddr;

} UDPSocket;

TCPSocket TCPSocketList[16];
UDPSocket UDPSocketList[4];

int UDPSocketID = 0;


bool Init()
{
    // TODO: how to deal with cases where an adapter is unplugged or changes config??
    //if (PCapLib) return true;

    //Lib = NULL;
    PacketLen = 0;
    RXNum = 0;

    IPv4ID = 1;

    memset(TCPSocketList, 0, sizeof(TCPSocketList));
    memset(UDPSocketList, 0, sizeof(UDPSocketList));

    UDPSocketID = 0;

    return true;
}

void DeInit()
{
    for (int i = 0; i < (sizeof(TCPSocketList)/sizeof(TCPSocket)); i++)
    {
        TCPSocket* sock = &TCPSocketList[i];
        if (sock->Backend) closesocket(sock->Backend);
    }

    for (int i = 0; i < (sizeof(UDPSocketList)/sizeof(UDPSocket)); i++)
    {
        UDPSocket* sock = &UDPSocketList[i];
        if (sock->Backend) closesocket(sock->Backend);
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

void FinishTCPFrame(u8* data, int len)
{
    u8* ipheader = &data[0xE];
    u8* tcpheader = &data[0x22];

    // lengths
    *(u16*)&ipheader[2] = htons(len - 0xE);

    // IP checksum
    u32 tmp = 0;

    for (int i = 0; i < 20; i += 2)
        tmp += ntohs(*(u16*)&ipheader[i]);
    while (tmp >> 16)
        tmp = (tmp & 0xFFFF) + (tmp >> 16);
    tmp ^= 0xFFFF;
    *(u16*)&ipheader[10] = htons(tmp);

    u32 tcplen = ntohs(*(u16*)&ipheader[2]) - 0x14;

    // TCP checksum
    tmp = 0;
    tmp += ntohs(*(u16*)&ipheader[12]);
    tmp += ntohs(*(u16*)&ipheader[14]);
    tmp += ntohs(*(u16*)&ipheader[16]);
    tmp += ntohs(*(u16*)&ipheader[18]);
    tmp += ntohs(0x0600);
    tmp += tcplen;
    for (u8* i = tcpheader; i < &tcpheader[tcplen-1]; i += 2)
        tmp += ntohs(*(u16*)i);
    if (tcplen & 1)
        tmp += ntohs((u_short)tcpheader[tcplen-1]);
    while (tmp >> 16)
        tmp = (tmp & 0xFFFF) + (tmp >> 16);
    tmp ^= 0xFFFF;
    *(u16*)&tcpheader[16] = htons(tmp);
}


void HandleDHCPFrame(u8* data, int len)
{
    u8 type = 0xFF;

    u32 transid = *(u32*)&data[0x2E];

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
        return;
    }

    printf("DHCP: frame type %d, transid %08X\n", type, transid);

    if (type == 1 || // discover
        type == 3)   // request
    {
        u8 resp[512];
        u8* out = &resp[0];

        // ethernet
        memcpy(out, &data[6], 6); out += 6;
        memcpy(out, kServerMAC, 6); out += 6;
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
        *(u32*)out = htonl(kServerIP); out += 4; // source IP
        if (type == 1)
        {
            *(u32*)out = htonl(0xFFFFFFFF); out += 4; // destination IP
        }
        else if (type == 3)
        {
            *(u32*)out = htonl(kClientIP); out += 4; // destination IP
        }

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
        *(u32*)out = htonl(kClientIP); out += 4; // your IP
        *(u32*)out = htonl(kServerIP); out += 4; // server IP
        *(u32*)out = htonl(0x00000000); out += 4; // gateway IP
        memcpy(out, &data[6], 6); out += 6;
        memset(out, 0, 10); out += 10;
        memset(out, 0, 192); out += 192;
        *(u32*)out = 0x63538263; out += 4; // DHCP magic

        // DHCP options
        *out++ = 53; *out++ = 1;
        *out++ = (type==1) ? 2 : 5; // DHCP type: offer/ack
        *out++ = 1; *out++ = 4;
        *(u32*)out = htonl(0xFFFFFF00); out += 4; // subnet mask
        *out++ = 3; *out++ = 4;
        *(u32*)out = htonl(kServerIP); out += 4; // router
        *out++ = 51; *out++ = 4;
        *(u32*)out = htonl(442030); out += 4; // lease time
        *out++ = 54; *out++ = 4;
        *(u32*)out = htonl(kServerIP); out += 4; // DHCP server
        *out++ = 6; *out++ = 4;
        *(u32*)out = htonl(kDNSIP); out += 4; // DNS (hax)

        *out++ = 0xFF;
        memset(out, 0, 20); out += 20;

        u32 framelen = (u32)(out - &resp[0]);
        if (framelen & 1) { *out++ = 0; framelen++; }
        FinishUDPFrame(resp, framelen);

        // TODO: if there is already a packet queued, this will overwrite it
        // that being said, this will only happen during DHCP setup, so probably
        // not a big deal

        PacketLen = framelen;
        memcpy(PacketBuffer, resp, PacketLen);
        RXNum = 1;
    }
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

    printf("DNS: ID=%04X, flags=%04X, Q=%d, A=%d, auth=%d, add=%d\n",
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
                printf(" -> %d.%d.%d.%d",
                       addr->sin_addr.S_un.S_un_b.s_b1, addr->sin_addr.S_un.S_un_b.s_b2,
                       addr->sin_addr.S_un.S_un_b.s_b3, addr->sin_addr.S_un.S_un_b.s_b4);

                addr_res = addr->sin_addr.S_un.S_addr;
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

    // TODO: if there is already a packet queued, this will overwrite it
    // that being said, this will only happen during DHCP setup, so probably
    // not a big deal

    PacketLen = framelen;
    memcpy(PacketBuffer, resp, PacketLen);
    RXNum = 1;
}

void UDP_BuildIncomingFrame(UDPSocket* sock, u8* data, int len)
{
    u8 resp[2048];
    u8* out = &resp[0];

    if (len > 1536) return;

    // ethernet
    memcpy(out, Wifi::GetMAC(), 6); out += 6; // hurf
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
    memcpy(out, sock->DestIP, 4); out += 4; // source IP
    *(u32*)out = htonl(kClientIP); out += 4; // destination IP

    // UDP
    u8* resp_tcpheader = out;
    *(u16*)out = htons(sock->DestPort); out += 2; // source port
    *(u16*)out = htons(sock->SourcePort); out += 2; // destination port
    *(u16*)out = htons(len+8); out += 2; // length of header+data
    *(u16*)out = 0; out += 2; // checksum

    memcpy(out, data, len); out += len;

    u32 framelen = (u32)(out - &resp[0]);
    FinishUDPFrame(resp, framelen);

    // TODO: if there is already a packet queued, this will overwrite it
    // that being said, this will only happen during DHCP setup, so probably
    // not a big deal

    PacketLen = framelen;
    memcpy(PacketBuffer, resp, PacketLen);
    RXNum = 1;
}

void HandleUDPFrame(u8* data, int len)
{
    u8* ipheader = &data[0xE];
    u8* udpheader = &data[0x22];

    // debug
    /*for (int j = 0; j < len; j += 16)
    {
        int rem = len - j;
        if (rem > 16) rem = 16;
        for (int i = 0; i < rem; i++)
        {
            printf("%02X ", data[i+j]);
        }
        printf("\n");
    }*/

    u16 srcport = ntohs(*(u16*)&udpheader[0]);
    u16 dstport = ntohs(*(u16*)&udpheader[2]);

    int sockid = -1;
    UDPSocket* sock;
    for (int i = 0; i < (sizeof(UDPSocketList)/sizeof(UDPSocket)); i++)
    {
        sock = &UDPSocketList[i];
        if (sock->Backend != 0 && !memcmp(&sock->DestIP, &ipheader[16], 4) &&
            sock->SourcePort == srcport && sock->DestPort == dstport)
        {
            sockid = i;
            break;
        }
    }

    if (sockid == -1)
    {
        sockid = UDPSocketID;
        sock = &UDPSocketList[sockid];

        UDPSocketID++;
        if (UDPSocketID >= (sizeof(UDPSocketList)/sizeof(UDPSocket)))
            UDPSocketID = 0;

        if (sock->Backend != 0)
        {
            printf("LANMAGIC: closing previous UDP socket #%d\n", sockid);
            closesocket(sock->Backend);
        }

        sock->Backend = socket(AF_INET, SOCK_DGRAM, 0);

        memcpy(sock->DestIP, &ipheader[16], 4);
        sock->SourcePort = srcport;
        sock->DestPort = dstport;

        memset(&sock->BackendAddr, 0, sizeof(sock->BackendAddr));
        sock->BackendAddr.sin_family = AF_INET;
        sock->BackendAddr.sin_port = htons(dstport);
        memcpy(&sock->BackendAddr.sin_addr, &ipheader[16], 4);
        /*if (bind(sock->Backend, (struct sockaddr*)&sock->BackendAddr, sizeof(sock->BackendAddr)) == SOCKET_ERROR)
        {
            printf("bind() shat itself :(\n");
        }*/

        printf("LANMAGIC: opening UDP socket #%d to %d.%d.%d.%d:%d, srcport %d\n",
               sockid,
               ipheader[16], ipheader[17], ipheader[18], ipheader[19],
               dstport, srcport);
    }

    u16 udplen = ntohs(*(u16*)&udpheader[4]) - 8;

    printf("UDP: socket %d sending %d bytes\n", sockid, udplen);
    sendto(sock->Backend, (char*)&udpheader[8], udplen, 0,
           (struct sockaddr*)&sock->BackendAddr, sizeof(sock->BackendAddr));
}

void TCP_SYNACK(TCPSocket* sock, u8* data, int len)
{
    u8 resp[128];
    u8* out = &resp[0];

    u8* ipheader = &data[0xE];
    u8* tcpheader = &data[0x22];

    u32 seqnum = htonl(*(u32*)&tcpheader[4]);
    seqnum++;
    sock->AckNum = seqnum;

    printf("SYNACK  SEQ=%08X|%08X\n", sock->SeqNum, sock->AckNum);

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
    *out++ = 0x06; // protocol (TCP)
    *(u16*)out = 0; out += 2; // checksum
    *(u32*)out = *(u32*)&ipheader[16]; out += 4; // source IP
    *(u32*)out = *(u32*)&ipheader[12]; out += 4; // destination IP

    // TCP
    u8* resp_tcpheader = out;
    *(u16*)out = *(u16*)&tcpheader[2]; out += 2; // source port
    *(u16*)out = *(u16*)&tcpheader[0]; out += 2; // destination port
    *(u32*)out = htonl(sock->SeqNum); out += 4; sock->SeqNum++; // seq number
    *(u32*)out = htonl(seqnum); out += 4; // ack seq number
    *(u16*)out = htons(0x8012); out += 2; // flags (SYN+ACK)
    *(u16*)out = htons(0x7000); out += 2; // window size (uuuh)
    *(u16*)out = 0; out += 2; // checksum
    *(u16*)out = 0; out += 2; // urgent pointer

    // TCP options
    *out++ = 0x02; *out++ = 0x04; // max segment size
    *(u16*)out = htons(0x05B4); out += 2;
    *out++ = 0x01;
    *out++ = 0x01;
    *out++ = 0x04; *out++ = 0x02; // SACK permitted
    *out++ = 0x01;
    *out++ = 0x03; *out++ = 0x03; // window size
    *out++ = 0x08;

    u32 framelen = (u32)(out - &resp[0]);
    //if (framelen & 1) { *out++ = 0; framelen++; }
    FinishTCPFrame(resp, framelen);

    // TODO: if there is already a packet queued, this will overwrite it
    // that being said, this will only happen during DHCP setup, so probably
    // not a big deal

    PacketLen = framelen;
    memcpy(PacketBuffer, resp, PacketLen);
    RXNum = 1;
}

void TCP_ACK(TCPSocket* sock, bool fin)
{
    u8 resp[64];
    u8* out = &resp[0];

    u16 flags       = 0x5010;
    if (fin) flags |= 0x0001;

    printf("%sACK  SEQ=%08X|%08X\n", fin?"FIN":"   ", sock->SeqNum, sock->AckNum);

    // ethernet
    memcpy(out, Wifi::GetMAC(), 6); out += 6;
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
    *out++ = 0x06; // protocol (TCP)
    *(u16*)out = 0; out += 2; // checksum
    *(u32*)out = *(u32*)&sock->DestIP; out += 4; // source IP
    *(u32*)out = htonl(kClientIP); out += 4; // destination IP

    // TCP
    u8* resp_tcpheader = out;
    *(u16*)out = htonl(sock->DestPort); out += 2; // source port
    *(u16*)out = htonl(sock->SourcePort); out += 2; // destination port
    *(u32*)out = htonl(sock->SeqNum); out += 4; // seq number
    *(u32*)out = htonl(sock->AckNum); out += 4; // ack seq number
    *(u16*)out = htons(flags); out += 2; // flags
    *(u16*)out = htons(0x7000); out += 2; // window size (uuuh)
    *(u16*)out = 0; out += 2; // checksum
    *(u16*)out = 0; out += 2; // urgent pointer

    u32 framelen = (u32)(out - &resp[0]);
    //if (framelen & 1) { *out++ = 0; framelen++; }
    FinishTCPFrame(resp, framelen);

    // TODO: if there is already a packet queued, this will overwrite it
    // that being said, this will only happen during DHCP setup, so probably
    // not a big deal

    PacketLen = framelen;
    memcpy(PacketBuffer, resp, PacketLen);
    RXNum = 1;
}

void TCP_BuildIncomingFrame(TCPSocket* sock, u8* data, int len)
{
    u8 resp[2048];
    u8* out = &resp[0];

    if (len > 1536) return;
printf("INCOMING SEQ=%08X|%08X\n", sock->SeqNum, sock->AckNum);
    // ethernet
    memcpy(out, Wifi::GetMAC(), 6); out += 6; // hurf
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
    *out++ = 0x06; // protocol (TCP)
    *(u16*)out = 0; out += 2; // checksum
    memcpy(out, sock->DestIP, 4); out += 4; // source IP
    *(u32*)out = htonl(kClientIP); out += 4; // destination IP

    // TCP
    u8* resp_tcpheader = out;
    *(u16*)out = htons(sock->DestPort); out += 2; // source port
    *(u16*)out = htons(sock->SourcePort); out += 2; // destination port
    *(u32*)out = htonl(sock->SeqNum); out += 4; // seq number
    *(u32*)out = htonl(sock->AckNum); out += 4; // ack seq number
    *(u16*)out = htons(0x5018); out += 2; // flags (ACK, PSH)
    *(u16*)out = htons(0x7000); out += 2; // window size (uuuh)
    *(u16*)out = 0; out += 2; // checksum
    *(u16*)out = 0; out += 2; // urgent pointer

    memcpy(out, data, len); out += len;

    u32 framelen = (u32)(out - &resp[0]);
    FinishTCPFrame(resp, framelen);

    // TODO: if there is already a packet queued, this will overwrite it
    // that being said, this will only happen during DHCP setup, so probably
    // not a big deal

    PacketLen = framelen;
    memcpy(PacketBuffer, resp, PacketLen);
    RXNum = 1;

    sock->SeqNum += len;
}

void HandleTCPFrame(u8* data, int len)
{
    u8* ipheader = &data[0xE];
    u8* tcpheader = &data[0x22];

    u16 srcport = ntohs(*(u16*)&tcpheader[0]);
    u16 dstport = ntohs(*(u16*)&tcpheader[2]);
    u16 flags = ntohs(*(u16*)&tcpheader[12]);

    u32 tcpheaderlen = 4 * (flags >> 12);
    u32 tcplen = ntohs(*(u16*)&ipheader[2]) - 0x14;
    u32 tcpdatalen = tcplen - tcpheaderlen;

    printf("tcpflags=%04X header=%d data=%d seq=%08X|%08X\n",
           flags, tcpheaderlen, tcpdatalen,
           ntohl(*(u32*)&tcpheader[4]),
           ntohl(*(u32*)&tcpheader[8]));

    if (flags & 0x002) // SYN
    {
        int sockid = -1;
        TCPSocket* sock;
        for (int i = 0; i < (sizeof(TCPSocketList)/sizeof(TCPSocket)); i++)
        {
            sock = &TCPSocketList[i];
            if (sock->Status != 0 && !memcmp(&sock->DestIP, &ipheader[16], 4) &&
                sock->SourcePort == srcport && sock->DestPort == dstport)
            {
                printf("LANMAGIC: duplicate TCP socket\n");
                sockid = i;
                break;
            }
        }

        if (sockid == -1)
        {
            for (int i = 0; i < (sizeof(TCPSocketList)/sizeof(TCPSocket)); i++)
            {
                sock = &TCPSocketList[i];
                if (sock->Status == 0)
                {
                    sockid = i;
                    break;
                }
            }
        }

        if (sockid == -1)
        {
            printf("LANMAGIC: !! TCP SOCKET LIST FULL\n");
            return;
        }

        printf("LANMAGIC: opening TCP socket #%d to %d.%d.%d.%d:%d, srcport %d\n",
               sockid,
               ipheader[16], ipheader[17], ipheader[18], ipheader[19],
               dstport, srcport);

        // keep track of it
        sock->Status = 1;
        memcpy(sock->DestIP, &ipheader[16], 4);
        sock->DestPort = dstport;
        sock->SourcePort = srcport;
        sock->SeqNum = 0x13370000;
        sock->AckNum = 0;

        // open backend socket
        if (!sock->Backend)
        {
            sock->Backend = socket(AF_INET, SOCK_STREAM, 0);
        }

        struct sockaddr_in conn_addr;
        memset(&conn_addr, 0, sizeof(conn_addr));
        conn_addr.sin_family = AF_INET;
        memcpy(&conn_addr.sin_addr, &ipheader[16], 4);
        conn_addr.sin_port = htons(dstport);
        if (connect(sock->Backend, (sockaddr*)&conn_addr, sizeof(conn_addr)) == SOCKET_ERROR)
        {
            printf("connect() shat itself :(\n");
        }
        else
        {
            // acknowledge it
            TCP_SYNACK(sock, data, len);
        }
    }
    else
    {
        int sockid = -1;
        TCPSocket* sock;
        for (int i = 0; i < (sizeof(TCPSocketList)/sizeof(TCPSocket)); i++)
        {
            sock = &TCPSocketList[i];
            if (sock->Status != 0 && !memcmp(&sock->DestIP, &ipheader[16], 4) &&
                sock->SourcePort == srcport && sock->DestPort == dstport)
            {
                sockid = i;
                break;
            }
        }

        if (sockid == -1)
        {
            printf("LANMAGIC: bad TCP packet\n");
            return;
        }

        // TODO: check those
        u32 seqnum = ntohl(*(u32*)&tcpheader[4]);
        u32 acknum = ntohl(*(u32*)&tcpheader[8]);
        sock->SeqNum = acknum;
        sock->AckNum = seqnum + tcpdatalen;

        // send data over the socket
        if (tcpdatalen > 0)
        {
            u8* tcpdata = &tcpheader[tcpheaderlen];

            printf("TCP: socket %d sending %d bytes (flags=%04X)\n", sockid, tcpdatalen, flags);
            send(sock->Backend, (char*)tcpdata, tcpdatalen, 0);

            // kind of a hack, there
            TCP_ACK(sock, false);
        }

        if (flags & 0x001) // FIN
        {
            // TODO: timeout etc
            printf("TCP: socket %d closing\n", sockid);

            sock->Status = 0;
            closesocket(sock->Backend);
            sock->Backend = 0;
        }
    }
}

void HandleARPFrame(u8* data, int len)
{
    u16 protocol = ntohs(*(u16*)&data[0x10]);
    if (protocol != 0x0800) return;

    u16 op = ntohs(*(u16*)&data[0x14]);
    u32 targetip = ntohl(*(u32*)&data[0x26]);

    // TODO: handle ARP to the client
    // this only handles ARP to the DHCP/router

    if (op == 1)
    {
        // opcode 1=req 2=reply
        // sender MAC
        // sender IP
        // target MAC
        // target IP

        const u8* targetmac;
        if (targetip == kServerIP)   targetmac = kServerMAC;
        else if (targetip == kDNSIP) targetmac = kDNSMAC;
        else return;

        u8 resp[64];
        u8* out = &resp[0];

        // ethernet
        memcpy(out, &data[6], 6); out += 6;
        memcpy(out, kServerMAC, 6); out += 6;
        *(u16*)out = htons(0x0806); out += 2;

        // ARP
        *(u16*)out = htons(0x0001); out += 2; // hardware type
        *(u16*)out = htons(0x0800); out += 2; // protocol
        *out++ = 6; // MAC address size
        *out++ = 4; // IP address size
        *(u16*)out = htons(0x0002); out += 2; // opcode
        memcpy(out, targetmac, 6); out += 6;
        *(u32*)out = htonl(targetip); out += 4;
        memcpy(out, &data[0x16], 6+4); out += 6+4;

        u32 framelen = (u32)(out - &resp[0]);

        // TODO: if there is already a packet queued, this will overwrite it
        // that being said, this will only happen during DHCP setup, so probably
        // not a big deal

        PacketLen = framelen;
        memcpy(PacketBuffer, resp, PacketLen);
        RXNum = 1;
    }
    else
    {
        printf("wat??\n");
    }
}

void HandlePacket(u8* data, int len)
{
    u16 ethertype = ntohs(*(u16*)&data[0xC]);

    if (ethertype == 0x0800) // IPv4
    {
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
            else if (dstport == 53 && htonl(*(u32*)&data[0x1E]) == kDNSIP) // DNS
            {
                printf("LANMAGIC: DNS packet\n");
                return HandleDNSFrame(data, len);
            }

            printf("LANMAGIC: UDP packet %d->%d\n", srcport, dstport);
            return HandleUDPFrame(data, len);
        }
        else if (protocol == 0x06) // TCP
        {
            printf("LANMAGIC: TCP packet\n");
            return HandleTCPFrame(data, len);
        }
        else
            printf("LANMAGIC: unsupported IP protocol %02X\n", protocol);
    }
    else if (ethertype == 0x0806) // ARP
    {
        printf("LANMAGIC: ARP packet\n");
        return HandleARPFrame(data, len);
    }
    else
        printf("LANMAGIC: unsupported ethernet type %04X\n", ethertype);
}

int SendPacket(u8* data, int len)
{
    if (len > 2048)
    {
        printf("LAN_SendPacket: error: packet too long (%d)\n", len);
        return 0;
    }

    HandlePacket(data, len);
    return len;
}

int RecvPacket(u8* data)
{
    int ret = 0;
    if (RXNum > 0)
    {
        memcpy(data, PacketBuffer, PacketLen);
        ret = PacketLen;
        RXNum = 0;
    }

    for (int i = 0; i < (sizeof(TCPSocketList)/sizeof(TCPSocket)); i++)
    {
        TCPSocket* sock = &TCPSocketList[i];
        if (sock->Status != 1) continue;

        fd_set fd;
        struct timeval tv;

        FD_ZERO(&fd);
        FD_SET(sock->Backend, &fd);
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        if (!select(sock->Backend+1, &fd, 0, 0, &tv))
        {
            continue;
        }

        u8 recvbuf[1024];
        int recvlen = recv(sock->Backend, (char*)recvbuf, 1024, 0);
        if (recvlen < 1)
        {
            if (recvlen == 0)
            {
                // socket has closed from the other side
                printf("TCP: socket %d closed from other side\n", i);
                sock->Status = 2;
                TCP_ACK(sock, true);
            }
            continue;
        }

        printf("TCP: socket %d receiving %d bytes\n", i, recvlen);
        TCP_BuildIncomingFrame(sock, recvbuf, recvlen);

        // debug
        for (int j = 0; j < recvlen; j += 16)
        {
            int rem = recvlen - j;
            if (rem > 16) rem = 16;
            for (int k = 0; k < rem; k++)
            {
                printf("%02X ", recvbuf[k+j]);
            }
            printf("\n");
        }

        //recvlen = recv(sock->Backend, (char*)recvbuf, 1024, 0);
        //if (recvlen == 0) printf("it closed immediately after\n");
    }

    for (int i = 0; i < (sizeof(UDPSocketList)/sizeof(UDPSocket)); i++)
    {
        UDPSocket* sock = &UDPSocketList[i];
        if (sock->Backend == 0) continue;

        fd_set fd;
        struct timeval tv;

        FD_ZERO(&fd);
        FD_SET(sock->Backend, &fd);
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        if (!select(sock->Backend+1, &fd, 0, 0, &tv))
        {
            continue;
        }

        u8 recvbuf[1024];
        sockaddr_t fromAddr;
        socklen_t fromLen = sizeof(sockaddr_t);
        int recvlen = recvfrom(sock->Backend, (char*)recvbuf, 1024, 0, &fromAddr, &fromLen);
        if (recvlen < 1) continue;

        if (fromAddr.sa_family != AF_INET) continue;
        struct sockaddr_in* fromAddrIn = (struct sockaddr_in*)&fromAddr;
        if (memcmp(&fromAddrIn->sin_addr, sock->DestIP, 4)) continue;
        if (ntohs(fromAddrIn->sin_port) != sock->DestPort) continue;

        printf("UDP: socket %d receiving %d bytes\n", i, recvlen);
        UDP_BuildIncomingFrame(sock, recvbuf, recvlen);
    }

    return ret;
}

}
