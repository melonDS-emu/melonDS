/*
    Copyright 2016-2017 StapleButter

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


namespace WifiAP
{

#define AP_MAC  0x00, 0xF0, 0x77, 0x77, 0x77, 0x77
#define AP_NAME "melonAP"

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
#define PLEN(p, base)      (int)(ptrdiff_t)(p-base)
#define PALIGN_4(p, base)  while (PLEN(p,base) & 0x3) *p++ = 0xFF;


u64 USCounter;

u16 SeqNo;

bool BeaconDue;

u8 PacketBuffer[2048];
int PacketLen;
int RXNum;


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


int SendPacket(u8* data, int len)
{
    //
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

        /*static int zorp=0;
        zorp++;
        char derp[55];
        sprintf(derp, "derpo%d.bin", zorp);
        FILE* f = fopen(derp, "wb");
        fwrite(data, len+12, 1, f);
        fclose(f);*/

        return len+12;
    }

    return 0;
}

}
