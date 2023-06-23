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
#include "../SPI.h"
#include "../GPU.h"

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

s32 TimerError;

u32 SharedMem[2];

u8 MAC[6];
u16 TXSeqNo;
u64 Timestamp;
u16 Channel;

u8 RXBuffer[2048];

u16 BeaconCount;
u16 BeaconInterval;
u8 BeaconFrame[1024];
u8* BeaconTagDD;

u16 HostScanCount;
u32 HostScanBuffer;

void ScheduleTimer(bool first);
void WifiIPCReply(u16 cmd, u16 status, int numextra=0, u16* extra=nullptr);


void Reset()
{
    IPCReplyQueue.Clear();

    TimerError = 0;

    SharedMem[0] = 0;
    SharedMem[1] = 0;

    u8* mac = SPI_Firmware::GetWifiMAC();
    memcpy(MAC, mac, 6);
    TXSeqNo = 0;
    Timestamp = 0;
    Channel = 0;

    memset(RXBuffer, 0, sizeof(RXBuffer));

    BeaconCount = 0;
    BeaconInterval = 0;
    memset(BeaconFrame, 0, sizeof(BeaconFrame));
    BeaconTagDD = nullptr;

    HostScanCount = 0;
    HostScanBuffer = 0;

    u16 chanmask = 0x2082;
    NDS::ARM7Write16(0x027FFCFA, chanmask);
}


void SendHostBeacon()
{printf("HOST BEACON\n");
    *(u16*)&BeaconFrame[12 + 22] = (TXSeqNo << 4);
    TXSeqNo++;

    *(u64*)&BeaconFrame[12 + 24] = Timestamp;

    u16 syncval = ((GPU::VCount * 0x7F) - (Timestamp * 2)) >> 7;
    *(u16*)&BeaconTagDD[8] = syncval;

    Platform::MP_SendPacket(BeaconFrame, 12+*(u16*)&BeaconFrame[10], Timestamp);
}

bool ReceiveHostBeacon()
{
    int rxlen = Platform::MP_RecvPacket(RXBuffer, nullptr);
    if (rxlen <= (12+24)) return false;

    u16 framelen = *(u16*)&RXBuffer[10];
    if (framelen != rxlen-12)
    {
        //Log(LogLevel::Error, "bad frame length %d/%d\n", framelen, rxlen-12);
        return false;
    }

    u16 framectl = *(u16*)&RXBuffer[12+0];
    if ((framectl & 0xE7FF) != 0x0080) return false;

    u8* tagpos = &RXBuffer[12+24+8+2+2];
    u8* frameend = &RXBuffer[rxlen];
    while (tagpos < frameend)
    {
        u8 tag = *tagpos++;
        u8 len = *tagpos++;

        if (tag == 0xDD)
        {
            u32 oui = *(u32*)tagpos;
            if (oui == 0x00BF0900)
                return true;
        }

        tagpos += len;
    }

    return false;
}

void HostScan()
{
    bool res = ReceiveHostBeacon();
    if (res)
    {
        // fill beacon info buffer

        u16 buf_len = 0x41;

        u8* dd_offset = nullptr;
        u16 dd_len = 0;

        NDS::ARM7Write16(HostScanBuffer+0x02, RXBuffer[12]); // CHECKME
        NDS::ARM7Write16(HostScanBuffer+0x04, RXBuffer[12+10]);
        NDS::ARM7Write16(HostScanBuffer+0x06, RXBuffer[12+12]);
        NDS::ARM7Write16(HostScanBuffer+0x08, RXBuffer[12+14]);
        NDS::ARM7Write16(HostScanBuffer+0x2C, RXBuffer[12+24+8+2]);
        NDS::ARM7Write16(HostScanBuffer+0x32, RXBuffer[12+24+8]);

        NDS::ARM7Write16(HostScanBuffer+0x0A, 0);
        NDS::ARM7Write16(HostScanBuffer+0x34, 0);
        NDS::ARM7Write16(HostScanBuffer+0x38, 0);
        NDS::ARM7Write16(HostScanBuffer+0x3C, 0);
        NDS::ARM7Write16(HostScanBuffer+0x3E, 0);

        u16 rxlen = *(u16*)&RXBuffer[10] + 12;
        u8* tagpos = &RXBuffer[12+24+8+2+2];
        u8* frameend = &RXBuffer[rxlen];
        while (tagpos < frameend)
        {
            u8 tag = *tagpos++;
            u8 len = *tagpos++;

            switch (tag)
            {
            case 0x00: // SSID
                {
                    u8 ssidlen = len;
                    if (ssidlen > 0x20) ssidlen = 0x20;

                    NDS::ARM7Write16(HostScanBuffer+0x0A, ssidlen);
                    for (int i = 0; i < ssidlen; i++)
                        NDS::ARM7Write8(HostScanBuffer+0xC+i, tagpos[i]);
                }
                break;

            case 0x01: // supported rates
                {
                    u16 mask1 = 0;
                    u16 mask2 = 0;

                    for (int i = 0; i < len; i++)
                    {
                        u8 val = tagpos[i];
                        u16 bit;
                        switch (val & 0x7F)
                        {
                        case 0x02: bit = (1<<0); break;
                        case 0x04: bit = (1<<1); break;
                        case 0x0B: bit = (1<<2); break;
                        case 0x0C: bit = (1<<3); break;
                        case 0x12: bit = (1<<4); break;
                        case 0x16: bit = (1<<5); break;
                        case 0x30: bit = (1<<6); break;
                        case 0x48: bit = (1<<7); break;
                        case 0x60: bit = (1<<8); break;
                        case 0x6C: bit = (1<<9); break;
                        default: bit = (1<<15); break;
                        }

                        if (val & 0x80) mask1 |= bit;
                        mask2 |= bit;
                    }

                    NDS::ARM7Write16(HostScanBuffer+0x2E, mask1);
                    NDS::ARM7Write16(HostScanBuffer+0x30, mask2);
                }
                break;

            case 0x03: // channel
                {
                    if (len != 1) break;

                    NDS::ARM7Write16(HostScanBuffer+0x36, tagpos[0]);
                }
                break;

            case 0x04: // CF parameters
                {
                    if (len != 6) break;

                    NDS::ARM7Write16(HostScanBuffer+0x38, tagpos[1]);
                }
                break;

            case 0x05: // TIM
                {
                    if (len < 4) break;

                    NDS::ARM7Write16(HostScanBuffer+0x34, tagpos[1]);
                }
                break;

            case 0xDD: // tag DD
                {
                    // TODO count bad tag DD's
                    if (len < 8) break;
                    if (*(u32*)&tagpos[0] != 0x00BF0900) break;

                    dd_offset = &tagpos[8];
                    dd_len = len - 8;

                    NDS::ARM7Write16(HostScanBuffer+0x3C, dd_len);
                    for (int i = 0; i < dd_len; i++)
                        NDS::ARM7Write8(HostScanBuffer+0x40+i, tagpos[8+i]);

                    buf_len += dd_len;
                }
                break;
            }

            tagpos += len;
        }

        NDS::ARM7Write16(HostScanBuffer+0x00, (buf_len >> 1));

        // fill reply buffer

        WifiIPCReply(0xA, 0, dd_len, (u16*)dd_offset);
    }
    else if (HostScanCount == 0)
    {
        WifiIPCReply(0xA, 0);
    }
}


void MSTimer(u32 param)
{
    u16 status = NDS::ARM7Read16(SharedMem[1]);

    Timestamp += 1024;

    switch (status)
    {
    case 5: // scanning for host beacons
        {
            if (HostScanCount > 0)
            {
                HostScanCount--;
                HostScan();
            }

            if (HostScanCount > 0)
                ScheduleTimer(false);
        }
        break;

    case 7:
    case 9: // host comm
        {
            BeaconCount++;
            if (BeaconCount >= BeaconInterval)
            {
                BeaconCount = 0;
                SendHostBeacon();
            }

            ScheduleTimer(false);
        }
        break;
    }
}

void ScheduleTimer(bool first)
{
    if (first)
    {
        NDS::CancelEvent(NDS::Event_Wifi);
        TimerError = 0;
    }

    s64 cycles = 33513982LL * 1024LL;
    cycles -= TimerError;
    s64 delay = (cycles + 999999LL) / 1000000LL;
    TimerError = (s32)((delay * 1000000LL) - cycles);

    NDS::ScheduleEvent(NDS::Event_Wifi, !first, (s32)delay, MSTimer, 0);
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

    BeaconCount = 0;
    BeaconInterval = NDS::ARM7Read16(paramblock + 0x18);
    u16 channel = NDS::ARM7Read16(paramblock + 0x32);

    u16 cmd_len = NDS::ARM7Read16(paramblock + 0x34);
    u16 reply_len = NDS::ARM7Read16(paramblock + 0x36);

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
    BeaconTagDD = ptr;
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
    *(u16*)&BeaconFrame[0xA] = len;
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
    else if (cmd == 0xA)
    {
        if (numextra > 0)
        {
            NDS::ARM7Write16(replybuf+0x8, 5);
            NDS::ARM7Write16(replybuf+0x10, Channel);
            NDS::ARM7Write16(replybuf+0x12, 0);

            // source MAC
            NDS::ARM7Write16(replybuf+0xA, *(u16*)&RXBuffer[12+10]);
            NDS::ARM7Write16(replybuf+0xC, *(u16*)&RXBuffer[12+12]);
            NDS::ARM7Write16(replybuf+0xE, *(u16*)&RXBuffer[12+14]);

            NDS::ARM7Write16(replybuf+0x14, 0); // ???

            NDS::ARM7Write16(replybuf+0x36, numextra);
            for (int i = 0; i < numextra; i+=2)
            {
                NDS::ARM7Write16(replybuf+0x38+i, extra[i>>1]);
            }
        }
        else
        {
            NDS::ARM7Write16(replybuf+0x8, 4);
            NDS::ARM7Write16(replybuf+0x10, Channel);
            NDS::ARM7Write16(replybuf+0x12, 0);
        }
    }
    else if (cmd == 0xE && status == 0)
    {
        NDS::ARM7Write16(replybuf+0x04, 0xA);
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
    cmd &= ~0x8000;

    if (cmd < 0x2E)
    {
        NDS::ARM7Write32(SharedMem[1]+0x4, 1);
        NDS::ARM7Write16(SharedMem[1]+0x2, cmd);

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
                Platform::MP_Begin();

                WifiIPCReply(0x0, 0);
            }
            break;

        case 0x2: // deinit
            {
                u16 status = NDS::ARM7Read16(SharedMem[1]);
                if (status == 2)
                {
                    NDS::ARM7Write16(SharedMem[1], 0);
                    Platform::MP_End();
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
                WifiIPCReply(0x8, 0, 3, extra);

                NDS::ARM7Write16(SharedMem[1]+0xC2, 1);
                ScheduleTimer(true);
            }
            break;

        case 0xA: // start host scan
            {
                // scan for viable host beacons
                // timeout: ~20480*64 cycles (with value 0x22)
                //
                // COMMAND BUFFER
                // offset  size  desc.
                // 02      2     channel
                // 04      4     pointer to beacon data buffer
                // 08      2     timeout in milliseconds
                // 0A      6     desired source MAC? seen set to FFFFFFFFFFFF
                //
                // REPLY BUFFER
                // offset  size  desc.
                // 08      2     5 to indicate a new beacon, 4 otherwise
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

                u16 status = NDS::ARM7Read16(SharedMem[1]);
                if (status != 2 && status != 3 && status != 5)
                {
                    u16 ext = 4;
                    WifiIPCReply(0xA, 3, 1, &ext);
                    break;
                }

                // TODO: check incoming parameters

                Channel = NDS::ARM7Read16(addr+0x2);
                HostScanCount = NDS::ARM7Read16(addr+0x8);
                HostScanBuffer = NDS::ARM7Read32(addr+0x4);

                NDS::ARM7Write16(SharedMem[1], 5);
                ScheduleTimer(true);
            }
            break;

        case 0xE: // start local MP
            {
                // TODO!!

                u16 status = NDS::ARM7Read16(SharedMem[1]);
                if (status == 8)
                    NDS::ARM7Write16(SharedMem[1], 10);
                else if (status == 7)
                    NDS::ARM7Write16(SharedMem[1], 9);

                printf("WIFI HLE: START MP\n");

                WifiIPCReply(0xE, 0);
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

        NDS::ARM7Write32(SharedMem[1]+0x4, 0);
    }

    cmd |= 0x8000;
    NDS::ARM7Write16(addr, cmd);
}

}
}
}
