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

using Platform::Log;
using Platform::LogLevel;

namespace WifiAP
{

const u8 APMac[6] = {AP_MAC};

#define PWRITE_8(p, v)      *p++ = v;
#define PWRITE_16(p, v)     *(u16*)p = v; p += 2;
#define PWRITE_32(p, v)     *(u32*)p = v; p += 4;
#define PWRITE_64(p, v)     *(u64*)p = v; p += 8;

#define PWRITE_MAC(p, a,b,c,d,e,f) \
    *p++ = a; *p++ = b; *p++ = c; *p++ = d; *p++ = e; *p++ = f;

#define PWRITE_MAC2(p, m) \
    *p++ = m[0]; *p++ = m[1]; *p++ = m[2]; *p++ = m[3]; *p++ = m[4]; *p++ = m[5];

#define PWRITE_SEQNO(p)     PWRITE_16(p, SeqNo); SeqNo += 0x10;

#define PWRITE_TXH(p, len, rate) \
    PWRITE_16(p, 0); \
    PWRITE_16(p, 0); \
    PWRITE_16(p, 0); \
    PWRITE_16(p, 0); \
    PWRITE_8(p, rate); \
    PWRITE_8(p, 0); \
    PWRITE_16(p, len);

//#define PALIGN_4(p, base)  p += ((4 - ((ptrdiff_t)(p-base) & 0x3)) & 0x3);
// no idea what is the ideal padding there
// but in the case of management frames the padding shouldn't be counted as an information element
// (theory: the hardware just doesn't touch the space between the frame and the FCS)
#define PLEN(p, base)      (int)(ptrdiff_t)(p-base)
#define PALIGN_4(p, base)  while (PLEN(p,base) & 0x3) *p++ = 0xFF;


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
    USCounter = 0x428888000ULL;
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
        Log(LogLevel::Warn, "wifiAP: can't reply!!\n");
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
                Log(LogLevel::Error, "wifiAP: bad assoc request, needs auth prior\n");
                return 0;
            }

            ClientStatus = 2;
            Log(LogLevel::Debug, "wifiAP: client associated\n");

            PWRITE_16(p, 0x0010);
            PWRITE_16(p, 0x0000); // duration??
            PWRITE_MAC2(p, (&data[10])); // recv
            PWRITE_MAC2(p, APMac); // sender
            PWRITE_MAC2(p, APMac); // BSSID
            PWRITE_SEQNO(p);

            PWRITE_16(p, 0x0021); // capability
            PWRITE_16(p, 0); // status (success)
            PWRITE_16(p, 0xC001); // assoc ID
            PWRITE_8(p, 0x01); PWRITE_8(p, 0x02); PWRITE_8(p, 0x82); PWRITE_8(p, 0x84); // rates

            PacketLen = PLEN(p, base);
            RXNum = 1;
        }
        return len;

    case 0x4: // probe request
        {
            // Nintendo's WFC setup util sends probe requests when searching for APs
            // these should be replied with a probe response, which is almost like a beacon

            PWRITE_16(p, 0x0050);
            PWRITE_16(p, 0x0000); // duration??
            PWRITE_MAC2(p, (&data[10])); // recv
            PWRITE_MAC2(p, APMac); // sender
            PWRITE_MAC2(p, APMac); // BSSID (checkme)
            PWRITE_SEQNO(p);

            PWRITE_64(p, USCounter);
            PWRITE_16(p, 128); // beacon interval
            PWRITE_16(p, 0x0021); // capability
            PWRITE_8(p, 0x01); PWRITE_8(p, 0x02); PWRITE_8(p, 0x82); PWRITE_8(p, 0x84); // rates
            PWRITE_8(p, 0x03); PWRITE_8(p, 0x01); PWRITE_8(p, 0x06); // current channel
            PWRITE_8(p, 0x00); PWRITE_8(p, strlen(AP_NAME));
            memcpy(p, AP_NAME, strlen(AP_NAME)); p += strlen(AP_NAME);

            PacketLen = PLEN(p, base);
            RXNum = 1;
        }
        return len;

    case 0xA: // deassoc
        {
            if (!MACEqual(&data[16], (u8*)APMac)) // check BSSID
                return 0;

            ClientStatus = 1;
            Log(LogLevel::Debug, "wifiAP: client deassociated\n");

            PWRITE_16(p, 0x00A0);
            PWRITE_16(p, 0x0000); // duration??
            PWRITE_MAC2(p, (&data[10])); // recv
            PWRITE_MAC2(p, APMac); // sender
            PWRITE_MAC2(p, APMac); // BSSID
            PWRITE_SEQNO(p);

            PWRITE_16(p, 3); // reason code

            PacketLen = PLEN(p, base);
            RXNum = 1;
        }
        return len;

    case 0xB: // auth
        {
            if (!MACEqual(&data[16], (u8*)APMac)) // check BSSID
                return 0;

            ClientStatus = 1;
            Log(LogLevel::Debug, "wifiAP: client authenticated\n");

            PWRITE_16(p, 0x00B0);
            PWRITE_16(p, 0x0000); // duration??
            PWRITE_MAC2(p, (&data[10])); // recv
            PWRITE_MAC2(p, APMac); // sender
            PWRITE_MAC2(p, APMac); // BSSID
            PWRITE_SEQNO(p);

            PWRITE_16(p, 0); // auth algorithm (open)
            PWRITE_16(p, 2); // auth sequence
            PWRITE_16(p, 0); // status code (success)

            PacketLen = PLEN(p, base);
            RXNum = 1;
        }
        return len;

    case 0xC: // deauth
        {
            if (!MACEqual(&data[16], (u8*)APMac)) // check BSSID
                return 0;

            ClientStatus = 0;
            Log(LogLevel::Debug, "wifiAP: client deauthenticated\n");

            PWRITE_16(p, 0x00C0);
            PWRITE_16(p, 0x0000); // duration??
            PWRITE_MAC2(p, (&data[10])); // recv
            PWRITE_MAC2(p, APMac); // sender
            PWRITE_MAC2(p, APMac); // BSSID
            PWRITE_SEQNO(p);

            PWRITE_16(p, 3); // reason code

            PacketLen = PLEN(p, base);
            RXNum = 1;
        }
        return len;

    default:
        Log(LogLevel::Warn, "wifiAP: unknown management frame type %X\n", (framectl>>4)&0xF);
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
                Log(LogLevel::Error, "wifiAP: got data frame with bad fromDS/toDS bits %04X\n", framectl);
                return 0;
            }

            // TODO: WFC patch??

            if (*(u32*)&data[24] == 0x0003AAAA && *(u16*)&data[28] == 0x0000)
            {
                if (ClientStatus != 2)
                {
                    Log(LogLevel::Warn, "wifiAP: trying to send shit without being associated\n");
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

        PWRITE_16(p, 0x0080);
        PWRITE_16(p, 0x0000); // duration??
        PWRITE_MAC(p, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF); // recv
        PWRITE_MAC2(p, APMac); // sender
        PWRITE_MAC2(p, APMac); // BSSID
        PWRITE_SEQNO(p);

        PWRITE_64(p, USCounter);
        PWRITE_16(p, 128); // beacon interval
        PWRITE_16(p, 0x0021); // capability
        PWRITE_8(p, 0x01); PWRITE_8(p, 0x02); PWRITE_8(p, 0x82); PWRITE_8(p, 0x84); // rates
        PWRITE_8(p, 0x03); PWRITE_8(p, 0x01); PWRITE_8(p, 0x06); // current channel
        PWRITE_8(p, 0x05); PWRITE_8(p, 0x04); PWRITE_8(p, 0); PWRITE_8(p, 0); PWRITE_8(p, 0); PWRITE_8(p, 0); // TIM
        PWRITE_8(p, 0x00); PWRITE_8(p, strlen(AP_NAME));
        memcpy(p, AP_NAME, strlen(AP_NAME)); p += strlen(AP_NAME);

        PALIGN_4(p, base);
        PWRITE_32(p, 0xDEADBEEF); // checksum. doesn't matter for now

        int len = PLEN(p, base);
        p = data;
        PWRITE_TXH(p, len, 20);

        return len+12;
    }

    if (RXNum)
    {
        RXNum = 0;

        u8* base = data + 12;
        u8* p = base;

        memcpy(p, PacketBuffer, PacketLen);
        p += PacketLen;

        PALIGN_4(p, base);
        PWRITE_32(p, 0xDEADBEEF);

        int len = PLEN(p, base);
        p = data;
        PWRITE_TXH(p, len, 20);

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

        PWRITE_16(p, 0x0208);
        PWRITE_16(p, 0x0000); // duration??
        PWRITE_MAC2(p, (&LANBuffer[0])); // recv
        PWRITE_MAC2(p, APMac); // BSSID
        PWRITE_MAC2(p, (&LANBuffer[6])); // sender
        PWRITE_SEQNO(p);

        PWRITE_32(p, 0x0003AAAA);
        PWRITE_16(p, 0x0000);
        PWRITE_16(p, *(u16*)&LANBuffer[12]);
        memcpy(p, &LANBuffer[14], rxlen-14); p += rxlen-14;

        PALIGN_4(p, base);
        PWRITE_32(p, 0xDEADBEEF); // checksum. doesn't matter for now

        int len = PLEN(p, base);
        p = data;
        PWRITE_TXH(p, len, 20);

        return len+12;
    }

    return 0;
}

}
