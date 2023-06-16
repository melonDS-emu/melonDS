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

#include "../NDS.h"
#include "../NDSCart.h"
#include "../HLE.h"
#include "../FIFO.h"

#include "IPC.h"
#include "Wifi.h"


namespace HLE
{
namespace Retail
{
namespace Wifi
{

struct IPCReply
{
    u16 Command;
    u16 Status;
    int NumExtra;
    u16 Extra[16];
};

FIFO<IPCReply, 16> IPCReplyQueue;

u32 SharedMem[2];

u8 MAC[6];

u16 BeaconInterval;
u8 BeaconFrame[1024];

void WifiIPCReply(u16 cmd, u16 status, int numextra=0, u16* extra=nullptr);


void Reset()
{
    IPCReplyQueue.Clear();

    SharedMem[0] = 0;
    SharedMem[1] = 0;

    u8* mac = SPI_Firmware::GetWifiMAC();
    memcpy(MAC, mac, 6);

    BeaconInterval = 0;
    memset(BeaconFrame, 0, sizeof(BeaconFrame));

    u16 chanmask = 0x2082;
    NDS::ARM7Write16(0x027FFCFA, chanmask);
}


void StartHostComm()
{
    memset(BeaconFrame, 0xFF, sizeof(BeaconFrame));

    u32 paramblock = SharedMem[1] + 0xE8;
    u8* ptr = &BeaconFrame[0];

    u32 dd_addr = NDS::ARM7Read32(paramblock + 0x00);
    u16 dd_len = NDS::ARM7Read16(paramblock + 0x04);

    u32 gameid = NDS::ARM7Read32(paramblock + 0x08);
    u16 streamcode = NDS::ARM7Read16(paramblock + 0x0C);

    BeaconInterval = NDS::ARM7Read16(paramblock + 0x18);
    u16 channel = NDS::ARM7Read16(paramblock + 0x32);

    u8 beacontype = 0;
    if (NDS::ARM7Read16(paramblock + 0x0E) & 0x1) beacontype |= (1<<0);
    if (NDS::ARM7Read16(paramblock + 0x12) & 0x1) beacontype |= (1<<1);
    if (NDS::ARM7Read16(paramblock + 0x14) & 0x1) beacontype |= (1<<2);
    if (NDS::ARM7Read16(paramblock + 0x16) & 0x1) beacontype |= (1<<3);

    // TX header (required by comm layer)
    *(u16*)ptr = 0;         ptr += 2;
    *(u16*)ptr = 0;         ptr += 2;
    *(u16*)ptr = 0;         ptr += 2;
    *(u16*)ptr = 0;         ptr += 2;
    *(u16*)ptr = 0x14;      ptr += 2; // TX rate
    *(u16*)ptr = 0;         ptr += 2; // length, set later

    // 802.11 header
    *(u16*)ptr = 0x0080;    ptr += 2; // frame control
    *(u16*)ptr = 0;         ptr += 2; // duration
    memset(ptr, 0xFF, 6);   ptr += 6; // destination MAC
    memcpy(ptr, MAC, 6);    ptr += 6; // source MAC
    memcpy(ptr, MAC, 6);    ptr += 6; // BSSID
    *(u16*)ptr = 0;         ptr += 2; // sequence number, set later

    // beacon body
    *(u64*)ptr = 0;              ptr += 8; // timestamp, set later
    *(u16*)ptr = BeaconInterval; ptr += 2; // beacon interval
    *(u16*)ptr = 0x0021;         ptr += 2; // capability

    // SSID
    /* *ptr++ = 0x00; *ptr++ = 0x20;
    *(u16*)ptr = NDS::ARM7Read16(paramblock + 0x8); ptr += 2;
    *(u16*)ptr = NDS::ARM7Read16(paramblock + 0xA); ptr += 2;
    *(u16*)ptr = NDS::ARM7Read16(paramblock + 0xC); ptr += 2;
    memset(ptr, 0, 0x1A); ptr += 0x1A;*/

    // supported rates
    *ptr++ = 0x01; *ptr++ = 0x02;
    *ptr++ = 0x82;
    *ptr++ = 0x84;

    // channel
    *ptr++ = 0x03; *ptr++ = 0x01;
    *ptr++ = (channel & 0xFF);

    // TIM
    *ptr++ = 0x05; *ptr++ = 0x05;
    *ptr++ = 0x00;
    *ptr++ = 0x02;
    *ptr++ = 0x00;
    *ptr++ = 0x00;
    *ptr++ = 0x00;

    // tag DD
    *ptr++ = 0xDD; *ptr++ = (0x18 + dd_len);
    *(u32*)ptr = 0x00BF0900;     ptr += 4;
    *(u16*)ptr = 0x000A;         ptr += 2; // stepping (checkme)
    *(u16*)ptr = 0;              ptr += 2; // sync value (fixed later)
    //*(u32*)ptr = 0x00400001;     ptr += 4;
    *(u32*)ptr = 0x00000100;     ptr += 4;
    *(u32*)ptr = gameid;         ptr += 4; // game ID
    *(u16*)ptr = streamcode;     ptr += 2; // random stream code
    *ptr++ = dd_len;
    *ptr++ = beacontype;
    *(u16*)ptr = cmd_len;        ptr += 2; // CMD frame length
    *(u16*)ptr = reply_len;      ptr += 2; // REPLY frame length
    for (int i = 0; i < dd_len; i++)
    {
        *ptr++ = NDS::ARM7Read8(dd_addr + i);
    }

    int len = (int)(ptr - (&BeaconFrame[0xC]));
    len = (len + 3) & ~3;

    // FCS placeholder
    *(u32*)&BeaconFrame[0xC+len] = 0x1D46B6B8;
    len += 4;

    // frame length
    *(u16*&)BeaconFrame[0xA] = len;
}


void WifiIPCRetry(u32 param)
{
    u16 flag = NDS::ARM7Read16(0x027FFF96);
    if (flag & 0x1)
    {
        NDS::ScheduleEvent(NDS::Event_HLE_WifiIPCRetry, true, 1024, WifiIPCRetry, 0);
        return;
    }

    IPCReply reply = IPCReplyQueue.Read();
    WifiIPCReply(reply.Command, reply.Status, reply.NumExtra, reply.Extra);
}

void WifiIPCReply(u16 cmd, u16 status, int numextra, u16* extra)
{
    u16 flag = NDS::ARM7Read16(0x027FFF96);
    if (flag & 0x1)
    {
        IPCReply reply;
        reply.Command = cmd;
        reply.Status = status;
        reply.NumExtra = numextra;
        if (numextra) memcpy(reply.Extra, extra, numextra*sizeof(u16));
        IPCReplyQueue.Write(reply);
        NDS::ScheduleEvent(NDS::Event_HLE_WifiIPCRetry, false, 1024, WifiIPCRetry, 0);
        return;
    }

    flag |= 0x1;
    NDS::ARM7Write16(0x027FFF96, flag);

    u32 replybuf = NDS::ARM7Read32(SharedMem[0]+0x8);
    NDS::ARM7Write16(replybuf+0x00, cmd);
    NDS::ARM7Write16(replybuf+0x02, status);

    if (cmd == 0x8)
    {
        NDS::ARM7Write16(replybuf+0x8, extra[0]);
        if (numextra == 3)
        {
            NDS::ARM7Write16(replybuf+0x2C, extra[1]);
            NDS::ARM7Write16(replybuf+0x2E, extra[2]);
        }
    }
    else
    {
        for (int i = 0; i < numextra; i++)
        {
            NDS::ARM7Write16(replybuf+0x8+(i*2), extra[i]);
        }
    }

    SendIPCReply(0xA, replybuf, 0);

    if (!IPCReplyQueue.IsEmpty())
        NDS::ScheduleEvent(NDS::Event_HLE_WifiIPCRetry, false, 1024, WifiIPCRetry, 0);
}

void OnIPCRequest(u32 addr)
{
    u16 cmd = NDS::ARM7Read16(addr);

    /*printf("WIFI HLE: cmd %04X\n", cmd);
    for (u32 i = 0; i < 16; i++)
    {
        for (u32 j = 0; j < 16; j+=4)
        {
            u32 varp = NDS::ARM7Read32(addr+(i*16)+j);
            printf("%08X ", varp);
        }
        printf("\n");
    }*/

    switch (cmd)
    {
    case 0x0: // init
        {
            SharedMem[0] = NDS::ARM7Read32(addr+0x4);
            SharedMem[1] = NDS::ARM7Read32(addr+0x8);
            u32 respbuf = NDS::ARM7Read32(addr+0xC);

            NDS::ARM7Write32(SharedMem[0], SharedMem[1]);
            NDS::ARM7Write32(SharedMem[0]+0x8, respbuf);

            // TODO init the sharedmem buffers
            // TODO other shito too!!

            NDS::ARM7Write16(SharedMem[1], 2);

            WifiIPCReply(0x0, 0);
        }
        break;

    case 0x2: // deinit
        {
            u16 status = NDS::ARM7Read16(SharedMem[1]);
            if (status == 2)
            {
                NDS::ARM7Write16(SharedMem[1], 0);
                WifiIPCReply(0x2, 0);
            }
            else
            {
                WifiIPCReply(0x2, 3);
            }
        }
        break;

    case 0x3: // enable
        {
            SharedMem[0] = NDS::ARM7Read32(addr+0x4);
            SharedMem[1] = NDS::ARM7Read32(addr+0x8);
            u32 respbuf = NDS::ARM7Read32(addr+0xC);

            NDS::ARM7Write32(SharedMem[0], SharedMem[1]);
            NDS::ARM7Write32(SharedMem[0]+0x8, respbuf);

            // TODO init the sharedmem buffers

            NDS::ARM7Write16(SharedMem[1], 1);

            WifiIPCReply(0x3, 0);
        }
        break;

    case 0x4: // disable
        {
            u16 status = NDS::ARM7Read16(SharedMem[1]);
            if (status == 1)
            {
                NDS::ARM7Write16(SharedMem[1], 0);
                WifiIPCReply(0x4, 0);
            }
            else
            {
                WifiIPCReply(0x4, 3);
            }
        }
        break;

    case 0x7: // set host param
        {
            // PARAM BLOCK FORMAT
            // offset  size  desc.
            // 00      4     tag DD: pointer to extra data
            // 04      2     tag DD: length of extra data (tag DD length minus 0x18)
            // 06      2
            // 08      6     SSID bytes 0..5 (SSID is 32 bytes, rest is all zeroes)
            // 08      4     tag DD: game ID
            // 0C      2     tag DD: stream code
            // 0E      2     tag DD: beacon type bit0
            // 10      2     ???
            // 12      2     tag DD: beacon type bit1
            // 14      2     tag DD: beacon type bit2
            // 16      2     tag DD: beacon type bit3
            // 18      2     beacon interval
            // 32      2     channel #
            // 34      2     tag DD: CMD data length
            // 36      2     tag DD: REPLY data length

            // for now, the param block is just copied to sharedmem

            u32 paramblock = NDS::ARM7Read32(addr+0x4);
            u32 dst = SharedMem[1] + 0xE8;
            for (u32 i = 0; i < 0x40; i+=4)
            {
                NDS::ARM7Write32(dst+i, NDS::ARM7Read32(paramblock+i));
            }

            WifiIPCReply(0x7, 0);
        }
        break;

    case 0x8: // start host comm
        {
            u16 status = NDS::ARM7Read16(SharedMem[1]);
            if (status != 2)
            {
                u16 ext = 0;
                WifiIPCReply(0x8, 3, 1, &ext);
                break;
            }

            StartHostComm();

            NDS::ARM7Write16(SharedMem[1], 7);

            u16 extra[3];
            extra[0] = 0;
            extra[1] = NDS::ARM7Read16(SharedMem[1]+0xE8+0x34);
            extra[2] = NDS::ARM7Read16(SharedMem[1]+0xE8+0x36);
            WifiIPCReply(0x8, 0, 3, &extra);

            NDS::ARM7Write16(SharedMem[1]+0xC2, 1);
        }
        break;

    case 0xA: // start host scan
        {
            // scan for viable host beacons
            // timeout: ~20480*64 cycles (with value 0x22)
            //
            // COMMAND BUFFER
            // offset  size  desc.
            // 04      4     pointer to beacon data buffer
            // 08      2     timeout in milliseconds
            // 0A      6     desired source MAC? seen set to FFFFFFFFFFFF
            //
            // REPLY BUFFER
            // offset  size  desc.
            // 08      2     ??? seen set to 4
            // 0A      6     source MAC from beacon (zero if none)
            // 10      ?     channel
            // 36      2     length of tag DD data (minus first 8 bytes) (zero if none)
            // 38      X     tag DD data (minus first 8 bytes)
            //
            // BEACON DATA BUFFER
            // offset  size  desc.
            // 00      2     buffer length in halfwords ((tag DD length - 8 + 0x41) >> 1)
            // 02      2     frame control? AND 0xFF -- 0080
            // 04      6     source MAC
            // 0A      2     SSID length (0 if none)
            // 0C      32    SSID (if present)
            // 2C      2     beacon capability field
            // 2E      2     supported rate bitmask (when bit7=1)
            //                   bit0 = 82 / 1Mbps
            //                   bit1 = 84 / 2Mbps
            //                   bit2 = 8B
            //                   bit3 = 8C
            //                   bit4 = 92
            //                   bit5 = 96
            //                   bit6 = B0
            //                   bit7 = C8
            //                   bit8 = E0
            //                   bit9 = EC
            //                   bit15 = any other rate (with bit7=1)
            // 30      2     supported rate bitmask
            //                   bit0 = 02/82 / 1Mbps
            //                   bit1 = 04/84 / 2Mbps
            //                   bit2 = 0B/8B
            //                   bit3 = 0C/8C
            //                   bit4 = 12/92
            //                   bit5 = 16/96
            //                   bit6 = 30/B0
            //                   bit7 = 48/C8
            //                   bit8 = 60/E0
            //                   bit9 = 6C/EC
            //                   bit15 = any other rate
            // 32      2     beacon interval
            // 34      2     beacon TIM period field (0 if none)
            // 36      2     beacon channel field
            // 38      2     beacon CF period field (0 if none)
            // 3A      2     ??
            // 3C      2     length of tag DD data (minus first 8 bytes)
            // 3E      2     number of bad tag DD's (when first 4 bytes are =/= 00:09:BF:00)
            // 40      X     tag DD data (minus first 8 bytes)
        }
        break;

    case 0x1E: // measure channel
        {
            u16 status = NDS::ARM7Read16(SharedMem[1]);
            if (status != 2)
            {
                WifiIPCReply(0x1E, 3);
                break;
            }

            u16 chan = NDS::ARM7Read16(addr+0x6);
            u16 extra[2] = {chan, 1};

            WifiIPCReply(0x1E, 0, 2, extra);
        }
        break;

    default:
        printf("WIFI HLE: unknown command %04X\n", cmd);
        break;
    }
}

}
}
}
