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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "Wifi.h"
#include "WifiAP.h"
#include "Platform.h"

#ifndef __WIN32__
#include <stddef.h>
#endif

namespace WifiAP
{

template<typename T>
constexpr void PWrite(u8* p, T v)
{
    *(T*)p = v;
    v += sizeof(T)/8;
}

constexpr void PWriteMAC(u8* p, u8 a, u8 b, u8 c, u8 d, u8 e, u8 f)
{
    *p++ = a;
    *p++ = b;
    *p++ = c;
    *p++ = d;
    *p++ = e;
    *p++ = f;
}

constexpr void PWriteMAC2(u8* p, const u8 m[6])
{
    *p++ = m[0];
    *p++ = m[1];
    *p++ = m[2];
    *p++ = m[3];
    *p++ = m[4];
    *p++ = m[5];
}

constexpr void PWriteSeqNo(u8* p, u16 v)
{
    PWrite<u16>(p, v);
    v += 0x10;
}

constexpr void PWriteTXH(u8* p, u8 len, u16 rate)
{
    PWrite<u64>(p, 0);
    PWrite<u8>(p, rate);
    PWrite<u8>(p, 0);
    PWrite<u16>(p, len);

}

//#define PAlign_4(p, base)  p += ((4 - ((ptrdiff_t)(p-base) & 0x3)) & 0x3);
// no idea what is the ideal padding there
// but in the case of management frames the padding shouldn't be counted as an information element
// (theory: the hardware just doesn't touch the space between the frame and the FCS)
constexpr int PLen(u8* p, u8* base)
{
    return (int)(ptrdiff_t)(p - base);
}

constexpr void PAlign4(u8* p, u8* base)
{
    while (PLen(p, base) & 0x3)
    {
        *p++ = 0xFF;
    }
}


u64 USCounter;

u16 SeqNo;

bool BeaconDue;

u8 PacketBuffer[2048];
int PacketLen;
int RXNum;

u8 LANBuffer[2048];

// this is a lazy AP, we only keep track of one client
// 0=disconnected 1=authenticated 2=associated
int ClientStatus;


bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    // random starting point for the counter
    USCounter = 0x428888017ULL;
    SeqNo = 0x0120;

    BeaconDue = false;

    memset(PacketBuffer, 0, sizeof(PacketBuffer));
    PacketLen = 0;
    RXNum = 0;

    ClientStatus = 0;
}


bool MACEqual(u8* a, u8* b)
{
    return (*(u32*)&a[0] == *(u32*)&b[0]) && (*(u16*)&a[4] == *(u16*)&b[4]);
}

bool MACIsBroadcast(u8* a)
{
    return (*(u32*)&a[0] == 0xFFFFFFFF) && (*(u16*)&a[4] == 0xFFFF);
}


void USTimer()
{
    USCounter++;

    u32 chk = (u32)USCounter;
    if (!(chk & 0x1FFFF))
    {
        // send beacon every 128ms
        BeaconDue = true;
    }
}

void MSTimer()
{
    USCounter += 0x400;

    u32 chk = (u32)USCounter;
    if (!(chk & 0x1FC00))
    {
        // send beacon every 128ms
        BeaconDue = true;
    }
}


int HandleManagementFrame(u8* data, int len)
{
    // TODO: perfect this
    // noting that frames sent pre-auth/assoc don't have a proper BSSID
    //if (!MACEqual(&data[16], (u8*)APMac)) // check BSSID
    //    return 0;

    if (RXNum)
    {
        printf("wifiAP: can't reply!!\n");
        return 0;
    }

    u16 framectl = *(u16*)&data[0];

    u8* base = &PacketBuffer[0];
    u8* p = base;

    switch ((framectl >> 4) & 0xF)
    {
    case 0x0: // assoc request
        {
            if (!MACEqual(&data[16], (u8*)APMac)) // check BSSID
                return 0;

            if (ClientStatus != 1)
            {
                printf("wifiAP: bad assoc request, needs auth prior\n");
                return 0;
            }

            ClientStatus = 2;
            printf("wifiAP: client associated\n");

            PWrite<u16>(p, 0x0010);
            PWrite<u16>(p, 0x0000); // duration??
            PWriteMAC2(p, (&data[10])); // recv
            PWriteMAC2(p, APMac); // sender
            PWriteMAC2(p, APMac); // BSSID
            PWriteSeqNo(p, SeqNo);

            PWrite<u16>(p, 0x0021); // capability
            PWrite<u16>(p, 0); // status (success)
            PWrite<u16>(p, 0xC001); // assoc ID
            PWrite<u8>(p, 0x01); PWrite<u8>(p, 0x02); PWrite<u8>(p, 0x82); PWrite<u8>(p, 0x84); // rates

            PacketLen = PLen(p, base);
            RXNum = 1;
        }
        return len;

    case 0x4: // probe request
        {
            // Nintendo's WFC setup util sends probe requests when searching for APs
            // these should be replied with a probe response, which is almost like a beacon

            PWrite<u16>(p, 0x0050);
            PWrite<u16>(p, 0x0000); // duration??
            PWriteMAC2(p, (&data[10])); // recv
            PWriteMAC2(p, APMac); // sender
            PWriteMAC2(p, APMac); // BSSID (checkme)
            PWriteSeqNo(p, SeqNo);

            PWrite<u64>(p, USCounter);
            PWrite<u16>(p, 128); // beacon interval
            PWrite<u16>(p, 0x0021); // capability
            PWrite<u8>(p, 0x01); PWrite<u8>(p, 0x02); PWrite<u8>(p, 0x82); PWrite<u8>(p, 0x84); // rates
            PWrite<u8>(p, 0x03); PWrite<u8>(p, 0x01); PWrite<u8>(p, 0x06); // current channel
            PWrite<u8>(p, 0x00); PWrite<u8>(p, strlen(APName));
            memcpy(p, APName, strlen(APName)); p += strlen(APName);

            PacketLen = PLen(p, base);
            RXNum = 1;
        }
        return len;

    case 0xA: // deassoc
        {
            if (!MACEqual(&data[16], (u8*)APMac)) // check BSSID
                return 0;

            ClientStatus = 1;
            printf("wifiAP: client deassociated\n");

            PWrite<u16>(p, 0x00A0);
            PWrite<u16>(p, 0x0000); // duration??
            PWriteMAC2(p, (&data[10])); // recv
            PWriteMAC2(p, APMac); // sender
            PWriteMAC2(p, APMac); // BSSID
            PWriteSeqNo(p, SeqNo);

            PWrite<u16>(p, 3); // reason code

            PacketLen = PLen(p, base);
            RXNum = 1;
        }
        return len;

    case 0xB: // auth
        {
            if (!MACEqual(&data[16], (u8*)APMac)) // check BSSID
                return 0;

            ClientStatus = 1;
            printf("wifiAP: client authenticated\n");

            PWrite<u16>(p, 0x00B0);
            PWrite<u16>(p, 0x0000); // duration??
            PWriteMAC2(p, (&data[10])); // recv
            PWriteMAC2(p, APMac); // sender
            PWriteMAC2(p, APMac); // BSSID
            PWriteSeqNo(p, SeqNo);

            PWrite<u16>(p, 0); // auth algorithm (open)
            PWrite<u16>(p, 2); // auth sequence
            PWrite<u16>(p, 0); // status code (success)

            PacketLen = PLen(p, base);
            RXNum = 1;
        }
        return len;

    case 0xC: // deauth
        {
            if (!MACEqual(&data[16], (u8*)APMac)) // check BSSID
                return 0;

            ClientStatus = 0;
            printf("wifiAP: client deauthenticated\n");

            PWrite<u16>(p, 0x00C0);
            PWrite<u16>(p, 0x0000); // duration??
            PWriteMAC2(p, (&data[10])); // recv
            PWriteMAC2(p, APMac); // sender
            PWriteMAC2(p, APMac); // BSSID
            PWriteSeqNo(p, SeqNo);

            PWrite<u16>(p, 3); // reason code

            PacketLen = PLen(p, base);
            RXNum = 1;
        }
        return len;

    default:
        printf("wifiAP: unknown management frame type %X\n", (framectl>>4)&0xF);
        return 0;
    }
}


int SendPacket(u8* data, int len)
{
    data += 12;

    u16 framectl = *(u16*)&data[0];
    switch ((framectl >> 2) & 0x3)
    {
    case 0: // management
        return HandleManagementFrame(data, len);

    case 1: // control
        // TODO ???
        return 0;

    case 2: // data
        {
            if ((framectl & 0x0300) != 0x0100)
            {
                printf("wifiAP: got data frame with bad fromDS/toDS bits %04X\n", framectl);
                return 0;
            }

            // TODO: WFC patch??

            if (*(u32*)&data[24] == 0x0003AAAA && *(u16*)&data[28] == 0x0000)
            {
                if (ClientStatus != 2)
                {
                    printf("wifiAP: trying to send shit without being associated\n");
                    return 0;
                }

                int lan_len = (len - 30 - 4) + 14;

                memcpy(&LANBuffer[0], &data[16], 6); // destination MAC
                memcpy(&LANBuffer[6], &data[10], 6); // source MAC
                *(u16*)&LANBuffer[12] = *(u16*)&data[30]; // type
                memcpy(&LANBuffer[14], &data[32], lan_len - 14);

                Platform::LAN_SendPacket(LANBuffer, lan_len);
            }
        }
        return len;
    }

    return 0;
}

int RecvPacket(u8* data)
{
    if (BeaconDue)
    {
        BeaconDue = false;

        // craft beacon
        u8* base = data + 12;
        u8* p = base;

        PWrite<u16>(p, 0x0080);
        PWrite<u16>(p, 0x0000); // duration??
        PWriteMAC(p, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF); // recv
        PWriteMAC2(p, APMac); // sender
        PWriteMAC2(p, APMac); // BSSID
        PWriteSeqNo(p, SeqNo);

        PWrite<u64>(p, USCounter);
        PWrite<u16>(p, 128); // beacon interval
        PWrite<u16>(p, 0x0021); // capability
        PWrite<u8>(p, 0x01); PWrite<u8>(p, 0x02); PWrite<u8>(p, 0x82); PWrite<u8>(p, 0x84); // rates
        PWrite<u8>(p, 0x03); PWrite<u8>(p, 0x01); PWrite<u8>(p, 0x06); // current channel
        PWrite<u8>(p, 0x05); PWrite<u8>(p, 0x04); PWrite<u8>(p, 0); PWrite<u8>(p, 0); PWrite<u8>(p, 0); PWrite<u8>(p, 0); // TIM
        PWrite<u8>(p, 0x00); PWrite<u8>(p, strlen(APName));
        memcpy(p, APName, strlen(APName)); p += strlen(APName);

        PAlign4(p, base);
        PWrite<u32>(p, 0xDEADBEEF); // checksum. doesn't matter for now

        int len = PLen(p, base);
        p = data;
        PWriteTXH(p, len, 20);

        return len+12;
    }

    if (RXNum)
    {
        RXNum = 0;

        u8* base = data + 12;
        u8* p = base;

        memcpy(p, PacketBuffer, PacketLen);
        p += PacketLen;

        PAlign4(p, base);
        PWrite<u32>(p, 0xDEADBEEF);

        int len = PLen(p, base);
        p = data;
        PWriteTXH(p, len, 20);

        return len+12;
    }

    if (ClientStatus < 2) return 0;

    int rxlen = Platform::LAN_RecvPacket(LANBuffer);
    if (rxlen > 0)
    {
        // check destination MAC
        if (!MACIsBroadcast(&LANBuffer[0]))
        {
            if (!MACEqual(&LANBuffer[0], Wifi::GetMAC()))
                return 0;
        }

        // packet is good

        u8* base = data + 12;
        u8* p = base;

        PWrite<u16>(p, 0x0208);
        PWrite<u16>(p, 0x0000); // duration??
        PWriteMAC2(p, (&LANBuffer[0])); // recv
        PWriteMAC2(p, APMac); // BSSID
        PWriteMAC2(p, (&LANBuffer[6])); // sender
        PWriteSeqNo(p, SeqNo);

        PWrite<u32>(p, 0x0003AAAA);
        PWrite<u16>(p, 0x0000);
        PWrite<u16>(p, *(u16*)&LANBuffer[12]);
        memcpy(p, &LANBuffer[14], rxlen-14); p += rxlen-14;

        PAlign4(p, base);
        PWrite<u32>(p, 0xDEADBEEF); // checksum. doesn't matter for now

        int len = PLen(p, base);
        p = data;
        PWriteTXH(p, len, 20);

        return len+12;
    }

    return 0;
}

}
