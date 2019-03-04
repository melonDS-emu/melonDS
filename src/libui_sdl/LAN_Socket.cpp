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


namespace LAN_Socket
{

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
    u16 DestPort;

    // 0: unused
    // 1: connected
    u8 Status;

} TCPSocket;

TCPSocket TCPSocketList[16];


bool Init()
{
    // TODO: how to deal with cases where an adapter is unplugged or changes config??
    //if (PCapLib) return true;

    //Lib = NULL;
    PacketLen = 0;
    RXNum = 0;

    IPv4ID = 1;

    memset(TCPSocketList, 0, sizeof(TCPSocketList));

    return true;
}

void DeInit()
{
    //
}

bool HandleIncomingIPFrame(u8* data, int len)
{
    const u32 serverip = 0x0A404001;
    const u32 clientip = 0x0A404010;

    if (memcmp(&data[0x1E], PCapAdapterData->IP_v4, 4))
        return false;

    u8 protocol = data[0x17];

    //memcpy(&data[6], &PCapAdapterData->DHCP_MAC[0], 6);
    memcpy(&data[0], Wifi::GetMAC(), 6);
    data[6] = 0x00; data[7] = 0xAB; data[8] = 0x33;
    data[9] = 0x28; data[10] = 0x99; data[11] = 0x44;

    *(u32*)&data[0x1E] = htonl(clientip);

    u8* ipheader = &data[0xE];
    u8* protoheader = &data[0x22];

    // IP checksum
    u32 tmp = 0;

    *(u16*)&ipheader[10] = 0;
    for (int i = 0; i < 20; i += 2)
        tmp += ntohs(*(u16*)&ipheader[i]);
    while (tmp >> 16)
        tmp = (tmp & 0xFFFF) + (tmp >> 16);
    tmp ^= 0xFFFF;
    *(u16*)&ipheader[10] = htons(tmp);

    if (protocol == 0x11)
    {
        u32 udplen = ntohs(*(u16*)&protoheader[4]);

        // UDP checksum
        tmp = 0;
        *(u16*)&protoheader[6] = 0;
        tmp += ntohs(*(u16*)&ipheader[12]);
        tmp += ntohs(*(u16*)&ipheader[14]);
        tmp += ntohs(*(u16*)&ipheader[16]);
        tmp += ntohs(*(u16*)&ipheader[18]);
        tmp += ntohs(0x1100);
        tmp += udplen;
        for (u8* i = protoheader; i < &protoheader[udplen-1]; i += 2)
            tmp += ntohs(*(u16*)i);
        if (udplen & 1) tmp += (protoheader[udplen-1] << 8);
        while (tmp >> 16)
            tmp = (tmp & 0xFFFF) + (tmp >> 16);
        tmp ^= 0xFFFF;
        if (tmp == 0) tmp = 0xFFFF;
        *(u16*)&protoheader[6] = htons(tmp);
    }
    else if (protocol == 0x06)
    {
        u32 tcplen = ntohs(*(u16*)&ipheader[2]) - 0x14;

        u16 srcport = ntohs(*(u16*)&protoheader[0]);
        u16 dstport = ntohs(*(u16*)&protoheader[2]);
        u16 flags = ntohs(*(u16*)&protoheader[12]);

        // TODO: check if they send a FIN, I guess
        int sockid = -1;
        for (int i = 0; i < (sizeof(TCPSocketList)/sizeof(TCPSocket)); i++)
        {
            TCPSocket* sock = &TCPSocketList[i];
            if (sock->Status == 1 && !memcmp(&sock->DestIP, &ipheader[12], 4) && sock->DestPort == srcport)
            {
                sockid = i;
                break;
            }
        }

        if (sockid == -1)
        {
            return true;
        }

        // TCP checksum
        tmp = 0;
        *(u16*)&protoheader[16] = 0;
        tmp += ntohs(*(u16*)&ipheader[12]);
        tmp += ntohs(*(u16*)&ipheader[14]);
        tmp += ntohs(*(u16*)&ipheader[16]);
        tmp += ntohs(*(u16*)&ipheader[18]);
        tmp += ntohs(0x0600);
        tmp += tcplen;
        for (u8* i = protoheader; i < &protoheader[tcplen-1]; i += 2)
            tmp += ntohs(*(u16*)i);
        if (tcplen & 1) tmp += (protoheader[tcplen-1] << 8);
        while (tmp >> 16)
            tmp = (tmp & 0xFFFF) + (tmp >> 16);
        tmp ^= 0xFFFF;
        *(u16*)&protoheader[16] = htons(tmp);
    }

    return false;
}

void RXCallback(u_char* blarg, const struct pcap_pkthdr* header, const u_char* data)
{
    while (PCapRXNum > 0);

    if (header->len > 2048-64) return;

    PCapPacketLen = header->len;
    memcpy(PCapPacketBuffer, data, PCapPacketLen);
    PCapRXNum = 1;

    if (!Config::DirectLAN)
    {
        u16 ethertype = ntohs(*(u16*)&data[0xC]);

        if (ethertype == 0x0800) // IPv4
        {
            if (HandleIncomingIPFrame(PCapPacketBuffer, header->len))
                PCapRXNum = 0;
        }
    }
}
u32 zarp=0;
bool HandleDHCPFrame(u8* data, int len)
{
    const u32 serverip = 0x0A404001;
    const u32 clientip = 0x0A404010;

    u8 type = 0xFF;

    u32 transid = *(u32*)&data[0x2E];
zarp=transid;
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
        return false;
    }

    printf("DHCP: frame type %d, transid %08X\n", type, transid);

    if (type == 1 || // discover
        type == 3)   // request
    {
        u8 resp[512];
        u8* out = &resp[0];

        // ethernet
        memcpy(out, &data[6], 6); out += 6;
        *out++ = 0x00; *out++ = 0xAB; *out++ = 0x33;
        *out++ = 0x28; *out++ = 0x99; *out++ = 0x44;
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
        *(u32*)out = htonl(serverip); out += 4; // source IP
        if (type == 1)
        {
            *(u32*)out = htonl(0xFFFFFFFF); out += 4; // destination IP
        }
        else if (type == 3)
        {
            *(u32*)out = htonl(clientip); out += 4; // destination IP
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
        *(u32*)out = htonl(clientip); out += 4; // your IP
        *(u32*)out = htonl(serverip); out += 4; // server IP
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
        *(u32*)out = htonl(serverip); out += 4; // router
        *out++ = 51; *out++ = 4;
        *(u32*)out = htonl(442030); out += 4; // lease time
        *out++ = 54; *out++ = 4;
        *(u32*)out = htonl(serverip); out += 4; // DHCP server

        u8 numdns = 0;
        for (int i = 0; i < 8; i++)
        {
            if (*(u32*)&PCapAdapterData->DNS[i][0] != 0)
                numdns++;
        }
        *out++ = 6; *out++ = 4*numdns;
        for (int i = 0; i < 8; i++)
        {
            u32 dnsip = *(u32*)&PCapAdapterData->DNS[i][0];
            if (dnsip != 0)
            {
                *(u32*)out = dnsip; out += 4;
            }
        }

        *out++ = 0xFF;
        memset(out, 0, 20); out += 20;

        // lengths
        u32 framelen = (u32)(out - &resp[0]);
        if (framelen & 1) { *out++ = 0; framelen++; }
        *(u16*)&ipheader[2] = htons(framelen - 0xE);
        *(u16*)&udpheader[4] = htons(framelen - (0xE + 0x14));

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
        tmp += (u32)(out - udpheader);
        for (u8* i = udpheader; i < out; i += 2)
            tmp += ntohs(*(u16*)i);
        while (tmp >> 16)
            tmp = (tmp & 0xFFFF) + (tmp >> 16);
        tmp ^= 0xFFFF;
        if (tmp == 0) tmp = 0xFFFF;
        *(u16*)&udpheader[6] = htons(tmp);

        // TODO: if there is already a packet queued, this will overwrite it
        // that being said, this will only happen during DHCP setup, so probably
        // not a big deal

        PCapPacketLen = framelen;
        memcpy(PCapPacketBuffer, resp, PCapPacketLen);
        PCapRXNum = 1;

        // DEBUG!!
        //pcap_sendpacket(PCapAdapter, data, len);
        //pcap_sendpacket(PCapAdapter, resp, framelen);

        return true;
    }

    return false;
}

bool HandleIPFrame(u8* data, int len)
{
    const u32 serverip = 0x0A404001;
    const u32 clientip = 0x0A404010;

    // debug
    //pcap_sendpacket(PCapAdapter, data, len);

    u8 protocol = data[0x17];

    // any kind of IPv4 frame that isn't DHCP
    // we do NAT and forward it to the network

    // like:
    // melonRouter -> host
    // destination MAC set to host MAC
    // source MAC set to melonRouter MAC

    memcpy(&data[0], &PCapAdapterData->DHCP_MAC[0], 6);
    memcpy(&data[6], &PCapAdapterData->MAC[0], 6);

    *(u32*)&data[0x1A] = *(u32*)&PCapAdapterData->IP_v4[0];

    u8* ipheader = &data[0xE];
    u8* protoheader = &data[0x22];

    // IP checksum
    u32 tmp = 0;

    *(u16*)&ipheader[10] = 0;
    for (int i = 0; i < 20; i += 2)
        tmp += ntohs(*(u16*)&ipheader[i]);
    while (tmp >> 16)
        tmp = (tmp & 0xFFFF) + (tmp >> 16);
    tmp ^= 0xFFFF;
    *(u16*)&ipheader[10] = htons(tmp);

    if (protocol == 0x11)
    {
        u32 udplen = ntohs(*(u16*)&protoheader[4]);

        // UDP checksum
        tmp = 0;
        *(u16*)&protoheader[6] = 0;
        tmp += ntohs(*(u16*)&ipheader[12]);
        tmp += ntohs(*(u16*)&ipheader[14]);
        tmp += ntohs(*(u16*)&ipheader[16]);
        tmp += ntohs(*(u16*)&ipheader[18]);
        tmp += ntohs(0x1100);
        tmp += udplen;
        for (u8* i = protoheader; i < &protoheader[udplen]; i += 2)
            tmp += ntohs(*(u16*)i);
        while (tmp >> 16)
            tmp = (tmp & 0xFFFF) + (tmp >> 16);
        tmp ^= 0xFFFF;
        if (tmp == 0) tmp = 0xFFFF;
        *(u16*)&protoheader[6] = htons(tmp);
    }
    else if (protocol == 0x06)
    {
        u32 tcplen = ntohs(*(u16*)&ipheader[2]) - 0x14;

        u16 srcport = ntohs(*(u16*)&protoheader[0]);
        u16 dstport = ntohs(*(u16*)&protoheader[2]);
        u16 flags = ntohs(*(u16*)&protoheader[12]);

        if (flags & 0x002) // SYN
        {
            int sockid = -1;
            for (int i = 0; i < (sizeof(TCPSocketList)/sizeof(TCPSocket)); i++)
            {
                TCPSocket* sock = &TCPSocketList[i];
                if (sock->Status == 1 && !memcmp(&sock->DestIP, &ipheader[16], 4) && sock->DestPort == dstport)
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
                    TCPSocket* sock = &TCPSocketList[i];
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
                return true;
            }

            printf("LANMAGIC: opening TCP socket #%d to %d.%d.%d.%d:%d\n",
                   sockid,
                   ipheader[16], ipheader[17], ipheader[18], ipheader[19],
                   dstport);

            // keep track of it
            // (TODO: also keep track of source port?)

            TCPSocket* sock = &TCPSocketList[sockid];
            sock->Status = 1;
            memcpy(sock->DestIP, &ipheader[16], 4);
            sock->DestPort = dstport;
        }
        else
        {
            int sockid = -1;
            for (int i = 0; i < (sizeof(TCPSocketList)/sizeof(TCPSocket)); i++)
            {
                TCPSocket* sock = &TCPSocketList[i];
                if (sock->Status == 1 && !memcmp(&sock->DestIP, &ipheader[16], 4) && sock->DestPort == dstport)
                {
                    sockid = i;
                    break;
                }
            }

            if (sockid == -1)
            {
                printf("LANMAGIC: bad TCP packet\n");
                return true;
            }

            if (flags & 0x001) // FIN
            {
                // TODO: cleverer termination?
                // also timeout etc
                TCPSocketList[sockid].Status = 0;
            }
        }

        // TCP checksum
        tmp = 0;
        *(u16*)&protoheader[16] = 0;
        tmp += ntohs(*(u16*)&ipheader[12]);
        tmp += ntohs(*(u16*)&ipheader[14]);
        tmp += ntohs(*(u16*)&ipheader[16]);
        tmp += ntohs(*(u16*)&ipheader[18]);
        tmp += ntohs(0x0600);
        tmp += tcplen;
        for (u8* i = protoheader; i < &protoheader[tcplen]; i += 2)
            tmp += ntohs(*(u16*)i);
        while (tmp >> 16)
            tmp = (tmp & 0xFFFF) + (tmp >> 16);
        tmp ^= 0xFFFF;
        if (tmp == 0) tmp = 0xFFFF;
        *(u16*)&protoheader[16] = htons(tmp);
    }

    return false;
}

bool HandleARPFrame(u8* data, int len)
{
    const u32 serverip = 0x0A404001;
    const u32 clientip = 0x0A404010;

    u16 protocol = ntohs(*(u16*)&data[0x10]);
    if (protocol != 0x0800) return false;

    u16 op = ntohs(*(u16*)&data[0x14]);
    u32 targetip = ntohl(*(u32*)&data[0x26]);

    // TODO: handle ARP to the client
    // this only handles ARP to the DHCP/router

    if (op == 1 && targetip == serverip)
    {
        // opcode 1=req 2=reply
        // sender MAC
        // sender IP
        // target MAC
        // target IP

        u8 resp[64];
        u8* out = &resp[0];

        // ethernet
        memcpy(out, &data[6], 6); out += 6;
        *out++ = 0x00; *out++ = 0xAB; *out++ = 0x33;
        *out++ = 0x28; *out++ = 0x99; *out++ = 0x44;
        *(u16*)out = htons(0x0806); out += 2;

        // ARP
        *(u16*)out = htons(0x0001); out += 2; // hardware type
        *(u16*)out = htons(0x0800); out += 2; // protocol
        *out++ = 6; // MAC address size
        *out++ = 4; // IP address size
        *(u16*)out = htons(0x0002); out += 2; // opcode
        *out++ = 0x00; *out++ = 0xAB; *out++ = 0x33;
        *out++ = 0x28; *out++ = 0x99; *out++ = 0x44;
        *(u32*)out = htonl(targetip); out += 4;
        memcpy(out, &data[0x16], 6+4); out += 6+4;

        u32 framelen = (u32)(out - &resp[0]);

        // TODO: if there is already a packet queued, this will overwrite it
        // that being said, this will only happen during DHCP setup, so probably
        // not a big deal

        PCapPacketLen = framelen;
        memcpy(PCapPacketBuffer, resp, PCapPacketLen);
        PCapRXNum = 1;

        // also broadcast them to the network
        pcap_sendpacket(PCapAdapter, data, len);
        pcap_sendpacket(PCapAdapter, resp, framelen);

        return true;
    }

    return false;
}

bool HandlePacket(u8* data, int len)
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
        }

        printf("LANMAGIC: IP frame, doing NAT\n");
        return HandleIPFrame(data, len);
    }
    else if (ethertype == 0x0806) // ARP
    {
        printf("LANMAGIC: ARP\n");
        return HandleARPFrame(data, len);
    }

    return false;
}

int SendPacket(u8* data, int len)
{
    if (PCapAdapter == NULL)
        return 0;

    if (len > 2048)
    {
        printf("LAN_SendPacket: error: packet too long (%d)\n", len);
        return 0;
    }

    if (!Config::DirectLAN)
    {
        if (HandlePacket(data, len))
            return len;
    }

    pcap_sendpacket(PCapAdapter, data, len);
    // TODO: check success
    return len;
}

int RecvPacket(u8* data)
{
    if (PCapAdapter == NULL)
        return 0;

    int ret = 0;
    if (PCapRXNum > 0)
    {
        memcpy(data, PCapPacketBuffer, PCapPacketLen);
        ret = PCapPacketLen;
        PCapRXNum = 0;
    }

    pcap_dispatch(PCapAdapter, 1, RXCallback, NULL);
    return ret;
}

}
