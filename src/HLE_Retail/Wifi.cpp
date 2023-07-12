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
    u16 Extra[280];
};

u32 IPCCmdAddr;
FIFO<IPCReply, 16> IPCReplyQueue;

s32 TimerError;

u32 SharedMem[2];

u8 MAC[6];
u16 TXSeqNo;
u64 Timestamp;
u16 Channel;

u8 TXBuffer[2048];
u8 RXBuffer[2048];
u8 MPReplyBuffer[15*1024];

u16 BeaconCount;
u16 BeaconInterval;
u8 BeaconFrame[1024];
u8* BeaconTagDD;

u32 ClientMask;
int ClientStatus[16];
u8 ClientMAC[16][6];

u16 ClientID;

u16 HostScanCount;
u32 HostScanBuffer;

u32 MPFrameMetadata[6];
u16 MPSeqNo;

u64 CmdTimestamp;
u32 CmdCount;
u16 CmdClientTime;
u16 CmdClientMask;
u16 CmdClientDone;
u16 CmdClientFail;

void ScheduleTimer(bool first);
void MPSignalTX(u16 seqno);
void MPSignalRX(u32 rxbuffer, u32 frameaddr, u16 srcID, u16 cmdflags);
void SetCmdFrameSize(u16 size);
void SetReplyFrameSize(u16 size);
void WifiIPCReply(u16 cmd, u16 status, int numextra=0, u16* extra=nullptr);


void Reset()
{
    IPCCmdAddr = 0;
    IPCReplyQueue.Clear();

    TimerError = 0;

    SharedMem[0] = 0;
    SharedMem[1] = 0;

    u8* mac = SPI_Firmware::GetWifiMAC();
    memcpy(MAC, mac, 6);
    TXSeqNo = 0;
    Timestamp = 0;
    Channel = 0;

    memset(TXBuffer, 0, sizeof(TXBuffer));
    memset(RXBuffer, 0, sizeof(RXBuffer));
    memset(MPReplyBuffer, 0, sizeof(MPReplyBuffer));

    BeaconCount = 0;
    BeaconInterval = 0;
    memset(BeaconFrame, 0, sizeof(BeaconFrame));
    BeaconTagDD = nullptr;

    ClientMask = 0;
    memset(ClientStatus, 0, sizeof(ClientStatus));
    memset(ClientMAC, 0, sizeof(ClientMAC));

    ClientID = 0;

    HostScanCount = 0;
    HostScanBuffer = 0;

    memset(MPFrameMetadata, 0, sizeof(MPFrameMetadata));
    MPSeqNo = 0;

    CmdTimestamp = 0;
    CmdCount = 0;
    CmdClientTime = 0;
    CmdClientMask = 0;
    CmdClientDone = 0;
    CmdClientFail = 0;

    u16 chanmask = 0x2082;
    NDS::ARM7Write16(0x027FFCFA, chanmask);
}



int ReceiveFrame()
{
    int rxlen = Platform::MP_RecvPacket(RXBuffer, nullptr);
    if (rxlen <= (12+24)) return -1;

    u16 framelen = *(u16*)&RXBuffer[10];
    if (framelen != rxlen-12)
    {
        //Log(LogLevel::Error, "bad frame length %d/%d\n", framelen, rxlen-12);
        return -1;
    }

    return rxlen;
}

void SendHostBeacon()
{
    *(u16*)&BeaconFrame[12 + 22] = (TXSeqNo << 4);
    TXSeqNo++;

    *(u64*)&BeaconFrame[12 + 24] = Timestamp;

    u16 syncval = ((GPU::VCount * 0x7F) - (Timestamp * 2)) >> 7;
    *(u16*)&BeaconTagDD[8] = syncval;

    Platform::MP_SendPacket(BeaconFrame, 12+*(u16*)&BeaconFrame[10], Timestamp);

    u16 extra = 2;
    WifiIPCReply(0x8, 0, 1, &extra);
}

bool ReceiveHostBeacon()
{
    int rxlen = ReceiveFrame();
    if (rxlen <= 0)
        return false;

    u16 framectl = *(u16*)&RXBuffer[12+0];
    if ((framectl & 0xE7FF) != 0x0080)
        return false;

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

void SendAuthFrame(u8* dest, int algo, int seq, int status)
{
    u8 frame[12+64];
    u8* ptr = &frame[0xC];

    *(u16*)&frame[0x0] = 0;
    *(u16*)&frame[0x2] = 0;
    *(u16*)&frame[0x4] = 0;
    *(u16*)&frame[0x6] = 0;
    *(u16*)&frame[0x8] = 0x14;

    *(u16*)ptr = 0x00B0;       ptr += 2; // frame control
    *(u16*)ptr = 0;            ptr += 2; // duration
    memcpy(ptr, dest, 6);      ptr += 6; // destination MAC
    memcpy(ptr, MAC, 6);       ptr += 6; // source MAC
    memcpy(ptr, (seq&1) ? dest:MAC, 6); ptr += 6; // BSSID
    *(u16*)ptr = TXSeqNo << 4; ptr += 2; // sequence number
    TXSeqNo++;

    *(u16*)ptr = algo;    ptr += 2; // auth algorithm
    *(u16*)ptr = seq;     ptr += 2; // auth sequence
    *(u16*)ptr = status;  ptr += 2; // status code

    int len = (int)(ptr - (&frame[0xC]));
    len = (len + 1) & ~1;

    // FCS placeholder
    *(u32*)&frame[0xC+len] = 0x1D46B6B8;
    len += 4;

    // frame length
    *(u16*)&frame[0xA] = len;

    Platform::MP_SendPacket(frame, 12+len, Timestamp);
}

void SendAssocRequest(u8* dest)
{
    u8 frame[12+64];
    u8* ptr = &frame[0xC];

    *(u16*)&frame[0x0] = 0;
    *(u16*)&frame[0x2] = 0;
    *(u16*)&frame[0x4] = 0;
    *(u16*)&frame[0x6] = 0;
    *(u16*)&frame[0x8] = 0x14;

    *(u16*)ptr = 0x0000;       ptr += 2; // frame control
    *(u16*)ptr = 0;            ptr += 2; // duration
    memcpy(ptr, dest, 6);      ptr += 6; // destination MAC
    memcpy(ptr, MAC, 6);       ptr += 6; // source MAC
    memcpy(ptr, dest, 6);      ptr += 6; // BSSID
    *(u16*)ptr = TXSeqNo << 4; ptr += 2; // sequence number
    TXSeqNo++;

    *(u16*)ptr = 0x0021;    ptr += 2; // capability
    *(u16*)ptr = 0x0001;    ptr += 2; // listen interval

    // SSID
    *ptr++ = 0x00; *ptr++ = 0x20;
    *(u16*)ptr = NDS::ARM7Read16(HostScanBuffer + 0x40); ptr += 2;
    *(u16*)ptr = NDS::ARM7Read16(HostScanBuffer + 0x42); ptr += 2;
    *(u16*)ptr = NDS::ARM7Read16(HostScanBuffer + 0x44); ptr += 2;
    memset(ptr, 0, 0x1A); ptr += 0x1A;

    // supported rates
    *ptr++ = 0x01; *ptr++ = 0x02;
    *ptr++ = 0x82;
    *ptr++ = 0x84;

    int len = (int)(ptr - (&frame[0xC]));
    len = (len + 1) & ~1;

    // FCS placeholder
    *(u32*)&frame[0xC+len] = 0x1D46B6B8;
    len += 4;

    // frame length
    *(u16*)&frame[0xA] = len;

    Platform::MP_SendPacket(frame, 12+len, Timestamp);
}

void SendAssocResponse(u8* dest, u16 status, u16 aid)
{
    u8 frame[12+64];
    u8* ptr = &frame[0xC];

    *(u16*)&frame[0x0] = 0;
    *(u16*)&frame[0x2] = 0;
    *(u16*)&frame[0x4] = 0;
    *(u16*)&frame[0x6] = 0;
    *(u16*)&frame[0x8] = 0x14;

    *(u16*)ptr = 0x0010;       ptr += 2; // frame control
    *(u16*)ptr = 0;            ptr += 2; // duration
    memcpy(ptr, dest, 6);      ptr += 6; // destination MAC
    memcpy(ptr, MAC, 6);       ptr += 6; // source MAC
    memcpy(ptr, MAC, 6);       ptr += 6; // BSSID
    *(u16*)ptr = TXSeqNo << 4; ptr += 2; // sequence number
    TXSeqNo++;

    *(u16*)ptr = 0x0021;    ptr += 2; // capability
    *(u16*)ptr = status;    ptr += 2; // status
    *(u16*)ptr = aid;       ptr += 2; // association ID

    // supported rates
    *ptr++ = 0x01; *ptr++ = 0x02;
    *ptr++ = 0x82;
    *ptr++ = 0x84;

    int len = (int)(ptr - (&frame[0xC]));
    len = (len + 1) & ~1;

    // FCS placeholder
    *(u32*)&frame[0xC+len] = 0x1D46B6B8;
    len += 4;

    // frame length
    *(u16*)&frame[0xA] = len;

    Platform::MP_SendPacket(frame, 12+len, Timestamp);
}

void SendCmdFrame(u32 bodyaddr, u32 bodylen, u16 clientmask, u32 stream)
{
    u8* frame = &TXBuffer[0];
    u8* ptr = &frame[0xC];
    u8 dest[6] = {0x03, 0x09, 0xBF, 0x00, 0x00, 0x00};

    if (bodylen & 1) bodylen++;

    u16 clientlen = bodylen ? NDS::ARM7Read16(SharedMem[1]+0x3A) : 8;
    u16 clienttime = (clientlen*4) + 0xE6;

    *(u16*)&frame[0x0] = 0;
    *(u16*)&frame[0x2] = clientmask;
    *(u16*)&frame[0x4] = 0;
    *(u16*)&frame[0x6] = 0;
    *(u16*)&frame[0x8] = 0x14;

    *(u16*)ptr = 0x0228;       ptr += 2; // frame control
    *(u16*)ptr = 0;            ptr += 2; // duration (TODO)
    memcpy(ptr, dest, 6);      ptr += 6; // destination MAC
    memcpy(ptr, MAC, 6);       ptr += 6; // BSSID
    memcpy(ptr, MAC, 6);       ptr += 6; // source MAC
    *(u16*)ptr = TXSeqNo << 4; ptr += 2; // sequence number
    TXSeqNo++;

    *(u16*)ptr = clienttime;    ptr += 2;
    *(u16*)ptr = clientmask;    ptr += 2;

printf("SEND CMD: BODYLEN=%04X\n", bodylen);
    if (bodylen)
    {
        u16 flags = (bodylen >> 1) & 0xFF;
        flags |= ((stream & 0xF) << 8);
        flags |= 0x9000;
        *(u16*)ptr = flags;         ptr += 2;

        for (int i = 0; i < bodylen; i+=2)
        {
            *(u16*)ptr = NDS::ARM7Read16(bodyaddr+i);
            ptr += 2;
        }

        u16 clientID = NDS::ARM7Read16(SharedMem[1]+0x188);
        u32 snptr = SharedMem[1] + 0x1F8 + (clientID << 4) + ((stream & 0x7) << 1);
        MPSeqNo = NDS::ARM7Read16(snptr);
        MPSeqNo++;
        NDS::ARM7Write16(snptr, MPSeqNo);

        *(u16*)ptr = MPSeqNo; ptr += 2;
        *(u16*)ptr = clientmask; ptr += 2;
    }
    else
    {
        *(u16*)ptr = 0x8000;      ptr += 2;
    }

    int len = (int)(ptr - (&frame[0xC]));
    len = (len + 1) & ~1;

    // FCS placeholder
    *(u32*)&frame[0xC+len] = 0x1D46B6B8;
    len += 4;

    // frame length
    *(u16*)&frame[0xA] = len;

    CmdTimestamp = Timestamp;
    CmdCount = 4; // TODO not hardcode this!
    CmdClientTime = clienttime;
    CmdClientMask = clientmask;
    CmdClientDone = 0;
    CmdClientFail = 0;

    Platform::MP_SendCmd(frame, 12+len, Timestamp);

    u16 curbuf = NDS::ARM7Read16(SharedMem[1]+0x70);
    u32 dstaddr = NDS::ARM7Read32(SharedMem[1]+0x74 + (curbuf<<2));

    u16 rxlen = clientlen + 0xE;
    u16 nclients = 0;
    for (int i = 1; i < 16; i++)
    {
        if (!(clientmask & (1<<i))) continue;

        // CHECKME
        u32 rxbuf = dstaddr+0x8 + (rxlen*nclients);
        NDS::ARM7Write16(rxbuf+0x00, 0x0001);
        NDS::ARM7Write16(rxbuf+0x02, 0xFFFF);
        nclients++;
    }

    NDS::ARM7Write32(dstaddr, 0); // ??
    NDS::ARM7Write16(dstaddr+0x4, nclients);
    NDS::ARM7Write16(dstaddr+0x6, rxlen);
}

void SendReplyFrame(u32 bodyaddr, u32 bodylen, u32 stream)
{
    u8* frame = &TXBuffer[0];
    u8* ptr = &frame[0xC];
    u8 dest[6] = {0x03, 0x09, 0xBF, 0x00, 0x00, 0x10};

    if (bodylen & 1) bodylen++;

    u16 clienttime = NDS::ARM7Read16(SharedMem[1]+0x3A);
    clienttime = (clienttime*4) + 0xE2;

    *(u16*)&frame[0x0] = 0;
    *(u16*)&frame[0x2] = 0;
    *(u16*)&frame[0x4] = 0;
    *(u16*)&frame[0x6] = 0;
    *(u16*)&frame[0x8] = 0x14;

    *(u16*)ptr = 0x0118;       ptr += 2; // frame control
    *(u16*)ptr = 0;            ptr += 2; // duration (TODO)
    memcpy(ptr, ClientMAC[0], 6); ptr += 6; // BSSID
    memcpy(ptr, MAC, 6);       ptr += 6; // source MAC
    memcpy(ptr, dest, 6);      ptr += 6; // destination MAC
    *(u16*)ptr = TXSeqNo << 4; ptr += 2; // sequence number
    TXSeqNo++;

printf("SEND REPLY: BODYLEN=%04X\n", bodylen);
    u16 clientID = NDS::ARM7Read16(SharedMem[1]+0x188);
    if (bodylen)
    {
        u16 flags = (bodylen >> 1) & 0xFF;
        flags |= ((stream & 0xF) << 8);
        flags |= 0x8000;
        *(u16*)ptr = flags;         ptr += 2;

        for (int i = 0; i < bodylen; i+=2)
        {
            *(u16*)ptr = NDS::ARM7Read16(bodyaddr+i);
            ptr += 2;
        }

        u32 snptr = SharedMem[1] + 0x1F8 + (clientID << 4) + ((stream & 0x7) << 1);
        MPSeqNo = NDS::ARM7Read16(snptr);
        MPSeqNo++;
        NDS::ARM7Write16(snptr, MPSeqNo);

        *(u16*)ptr = MPSeqNo; ptr += 2;
    }
    else
    {
        *(u16*)ptr = 0x8000; ptr += 2;
    }

    int len = (int)(ptr - (&frame[0xC]));
    len = (len + 1) & ~1;

    // FCS placeholder
    *(u32*)&frame[0xC+len] = 0x1D46B6B8;
    len += 4;

    // frame length
    *(u16*)&frame[0xA] = len;

    Platform::MP_SendReply(frame, 12+len, Timestamp, clientID);
}

void ReceiveReplyFrames()
{
    u16 remmask = CmdClientMask & ~CmdClientDone;
    if (!remmask) return;

    // TODO: only block on the last attempt
    u16 res = Platform::MP_RecvReplies(MPReplyBuffer, CmdTimestamp, remmask);
    if (!res) return;

    res &= remmask;
    for (int i = 1; i < 16; i++)
    {
        if (!(res & (1<<i))) continue;

        u8* frame = &MPReplyBuffer[(i-1)*1024];
        u16 framelen = *(u16*)&frame[10];

        u16 frametime = (framelen * 4) + 0x60;
        if (frametime > CmdClientTime) continue;

        u8* dstmac = &frame[12+16];
        if (*(u16*)&dstmac[0] != 0x0903) continue;
        if (*(u16*)&dstmac[2] != 0x00BF) continue;
        if (*(u16*)&dstmac[4] != 0x1000) continue;

        // TODO check source MAC and all that

        u16 curbuf = NDS::ARM7Read16(SharedMem[1]+0x70);
        u32 _dstaddr = NDS::ARM7Read32(SharedMem[1]+0x74 + (curbuf<<2));
        u16 dstlen = NDS::ARM7Read16(SharedMem[1]+0x72);

        u16 nclients = NDS::ARM7Read16(_dstaddr+0x4);
        u16 rxlen = NDS::ARM7Read16(_dstaddr+0x6);
        if (i > nclients) continue;

        u16 bodylen;
        if (framelen >= 0x26) bodylen = framelen - 0x26;
        else bodylen = 0;

        u16 writelen = framelen - 0x1A;
        if ((writelen+0xA) > rxlen) continue;

        u32 dstaddr = _dstaddr + (0x8 + (rxlen * (i-1)));

        // TODO a lot more of the stuff

        NDS::ARM7Write16(dstaddr+0x00, 0);
        NDS::ARM7Write16(dstaddr+0x02, 0x0040);
        NDS::ARM7Write16(dstaddr+0x04, 0x8014);
        NDS::ARM7Write16(dstaddr+0x06, i);
        NDS::ARM7Write16(dstaddr+0x08, 0);

        for (u32 i = 0; i < writelen; i+=2)
        {
            NDS::ARM7Write16(dstaddr+0xA + i, *(u16*)&frame[12+24 + i]);
        }

        u16 tmp = NDS::ARM7Read16(SharedMem[1]+0x86);
        tmp |= (1<<i);
        NDS::ARM7Write16(SharedMem[1]+0x86, tmp);

        /*u16 extra[3];
        extra[0] = 0xC;
        *(u32*)&extra[1] = dstaddr;
        WifiIPCReply(0xE, 0, 3, extra);*/

        if (bodylen)
        {
            u16 cmdflags = *(u16*)&frame[12+24];
            MPSignalRX(_dstaddr, dstaddr, i, cmdflags);
        }
    }

    CmdClientDone |= res;
}

void SendAckFrame()
{
    u8 frame[12+32];
    u8* ptr = &frame[0xC];
    u8 dest[6] = {0x03, 0x09, 0xBF, 0x00, 0x00, 0x03};

    CmdClientFail = CmdClientMask & ~CmdClientDone;

    *(u16*)&frame[0x0] = 0;
    *(u16*)&frame[0x2] = 0;
    *(u16*)&frame[0x4] = 0;
    *(u16*)&frame[0x6] = 0;
    *(u16*)&frame[0x8] = 0x14;

    *(u16*)ptr = 0x0218;       ptr += 2; // frame control
    *(u16*)ptr = 0;            ptr += 2; // duration
    memcpy(ptr, dest, 6);      ptr += 6; // destination MAC
    memcpy(ptr, MAC, 6);       ptr += 6; // BSSID
    memcpy(ptr, MAC, 6);       ptr += 6; // source MAC
    *(u16*)ptr = TXSeqNo << 4; ptr += 2; // sequence number
    TXSeqNo++;

    *(u16*)ptr = 0x0033;        ptr += 2; // ???
    *(u16*)ptr = CmdClientFail; ptr += 2;

    int len = (int)(ptr - (&frame[0xC]));
    len = (len + 1) & ~1;

    // FCS placeholder
    *(u32*)&frame[0xC+len] = 0x1D46B6B8;
    len += 4;

    // frame length
    *(u16*)&frame[0xA] = len;

    Platform::MP_SendAck(frame, 12+len, Timestamp);
printf("MP: ACK\n");
    if (MPFrameMetadata[2])
    {
        MPSignalTX(MPSeqNo);
    }

    u16 curbuf = NDS::ARM7Read16(SharedMem[1]+0x70);
    u32 dstaddr = NDS::ARM7Read32(SharedMem[1]+0x74 + (curbuf<<2));
    //u16 dstlen = NDS::ARM7Read16(SharedMem[1]+0x72);

    u16 extra[3];
    extra[0] = 0xB;
    *(u32*)&extra[1] = dstaddr;
    WifiIPCReply(0xE, 0, 3, extra);

    curbuf ^= 1;
    NDS::ARM7Write16(SharedMem[1]+0x70, curbuf);
}

void MPSignalTX(u16 seqno)
{
    u16 extra[15];
    extra[0] = 0x14;
    extra[1] = MPFrameMetadata[3];
    extra[2] = MPFrameMetadata[2];
    extra[3] = CmdClientFail; // CHECKME
    extra[4] = MPFrameMetadata[2] & ~CmdClientFail; // CHECKME
    *(u32*)&extra[5] = MPFrameMetadata[0];
    extra[7] = MPFrameMetadata[1];

    if (seqno == 0xFFFF)
        extra[8] = 0xFFFF;
    else
        extra[8] = (seqno << 1); // TODO: retransmit flag in bit0?

    *(u32*)&extra[9] = MPFrameMetadata[4];
    *(u32*)&extra[11] = MPFrameMetadata[5];

    u16 clientID = NDS::ARM7Read16(SharedMem[1]+0x188);
    if (clientID == 0)
    {
        extra[13] = NDS::ARM7Read16(SharedMem[1]+0x30); // CMD
        extra[14] = NDS::ARM7Read16(SharedMem[1]+0x32); // REPLY
    }
    else
    {
        extra[13] = NDS::ARM7Read16(SharedMem[1]+0x32); // REPLY
        extra[14] = NDS::ARM7Read16(SharedMem[1]+0x30); // CMD
    }

    WifiIPCReply(0x81, 0, 15, extra);
}

void MPSignalRX(u32 rxbuffer, u32 frameaddr, u16 srcID, u16 cmdflags)
{
    u16 extra[12];
    extra[0] = 0x15;
    extra[1] = (cmdflags >> 8) & 0xF;
    *(u32*)&extra[2] = rxbuffer;
    *(u32*)&extra[4] = frameaddr;
    extra[6] = (cmdflags & 0xFF) << 1;
    extra[7] = srcID;
    extra[8] = 6; // remaining time? TODO

    u16 clientID = NDS::ARM7Read16(SharedMem[1]+0x188);
    extra[9] = clientID;
    if (clientID == 0)
    {
        extra[10] = NDS::ARM7Read16(SharedMem[1]+0x30); // CMD
        extra[11] = NDS::ARM7Read16(SharedMem[1]+0x32); // REPLY
    }
    else
    {
        extra[10] = NDS::ARM7Read16(SharedMem[1]+0x32); // REPLY
        extra[11] = NDS::ARM7Read16(SharedMem[1]+0x30); // CMD
    }

    WifiIPCReply(0x82, 0, 12, extra);
}

void HostScan()
{
    int numextra = 1;
    u16 extra[8+128];
    extra[0] = Channel;

    bool res = ReceiveHostBeacon();
    if (res)
    {
        // fill beacon info buffer

        u16 buf_len = 0x41;

        u8* dd_offset = nullptr;
        u16 dd_len = 0;

        NDS::ARM7Write16(HostScanBuffer+0x02, RXBuffer[12]); // CHECKME
        NDS::ARM7Write16(HostScanBuffer+0x04, *(u16*)&RXBuffer[12+10]);
        NDS::ARM7Write16(HostScanBuffer+0x06, *(u16*)&RXBuffer[12+12]);
        NDS::ARM7Write16(HostScanBuffer+0x08, *(u16*)&RXBuffer[12+14]);
        NDS::ARM7Write16(HostScanBuffer+0x2C, *(u16*)&RXBuffer[12+24+8+2]);
        NDS::ARM7Write16(HostScanBuffer+0x32, *(u16*)&RXBuffer[12+24+8]);

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
        extra[1] = *(u16*)&RXBuffer[12+10];
        extra[2] = *(u16*)&RXBuffer[12+12];
        extra[3] = *(u16*)&RXBuffer[12+14];
        extra[4] = dd_len;
        memcpy(&extra[5], dd_offset, dd_len);
        numextra = 5 + ((dd_len+1) >> 1);

        WifiIPCReply(0xA, 0, numextra, extra);

        NDS::ARM7Write32(SharedMem[1]+0x4, 0);
        NDS::ARM7Write16(IPCCmdAddr, 0x800A);
    }
    else if (HostScanCount == 0)
    {
        WifiIPCReply(0xA, 0, numextra, extra);

        NDS::ARM7Write32(SharedMem[1]+0x4, 0);
        NDS::ARM7Write16(IPCCmdAddr, 0x800A);
    }
}

bool HostConnect()
{
    switch (ClientStatus[0])
    {
    case 0: // waiting for beacon
        {
            if (!ReceiveHostBeacon())
                return false;

            u8 wanted_mac[6];
            *(u16*)&wanted_mac[0] = NDS::ARM7Read16(SharedMem[0]+0x14);
            *(u16*)&wanted_mac[2] = NDS::ARM7Read16(SharedMem[0]+0x16);
            *(u16*)&wanted_mac[4] = NDS::ARM7Read16(SharedMem[0]+0x18);

            // check MAC
            if (memcmp(&RXBuffer[12+10], wanted_mac, 6))
                return false;

            // TODO: other checks?

            ClientStatus[0] = 1;
            SendAuthFrame(wanted_mac, 0, 1, 0);
        }
        return false;

    case 1: // waiting for auth frame
        {
            if (ReceiveFrame() <= 0)
                return false;

            u16 framectl = *(u16*)&RXBuffer[12+0];
            framectl &= 0xE7FF;

            if ((framectl & 0x00FC) != 0xB0)
                return false;

            // TODO check shito!

            u8* srcmac = &RXBuffer[12+10];

            ClientStatus[0] = 2;
            SendAssocRequest(srcmac);
        }
        return false;

    case 2: // waiting for assoc frame
        {
            if (ReceiveFrame() <= 0)
                return false;

            u16 framectl = *(u16*)&RXBuffer[12+0];
            framectl &= 0xE7FF;

            if ((framectl & 0x00FC) != 0x10)
                return false;

            // TODO check shito!

            u16 aid = *(u16*)&RXBuffer[12+24+4];
            printf("wifi HLE: client connected, AID=%04X\n", aid);

            ClientID = aid & 0xF;
            ClientMask |= (1<<0);
            u8* srcmac = &RXBuffer[12+10];
            memcpy(ClientMAC[0], srcmac, 6);
            ClientStatus[0] = 3;

            NDS::ARM7Write16(SharedMem[1], 8);

            NDS::ARM7Write16(SharedMem[1]+0x188, ClientID);
            NDS::ARM7Write16(SharedMem[1]+0x182, 1);
            NDS::ARM7Write16(SharedMem[1]+0x86, 1);

            u8 flags = NDS::ARM7Read8(SharedMem[0]+0x5B);
            u16 cmd_len = NDS::ARM7Read16(SharedMem[0]+0x5C);
            u16 reply_len = NDS::ARM7Read16(SharedMem[0]+0x5E);
            if (flags & (1<<2))
            {
                cmd_len += 0x2A;
                reply_len += 6;
            }
            SetCmdFrameSize(cmd_len);
            SetReplyFrameSize(reply_len);

            u16 extra[7];
            extra[0] = 7;
            extra[1] = ClientID;
            memcpy(&extra[2], srcmac, 6);
            // TODO: not right!
            extra[5] = cmd_len;
            extra[6] = reply_len;
            WifiIPCReply(0xC, 0, 7, extra);

            NDS::ARM7Write32(SharedMem[1]+0x4, 0);
            NDS::ARM7Write16(IPCCmdAddr, 0x800C);
        }
        return true;
    }

    return false;
}


void HostComm(int status)
{
    if (ReceiveFrame() <= 0)
        return;

    u16 framectl = *(u16*)&RXBuffer[12+0];
    framectl &= 0xE7FF;

    switch ((framectl >> 2) & 0x3)
    {
    case 0: // management frame
        {
            u8* srcmac = &RXBuffer[12+10];

            switch ((framectl >> 4) & 0xF)
            {
            case 0x0: // association request
                {
                    // TODO check shito here

                    int clientnum = -1;
                    for (int i = 1; i < 16; i++)
                    {
                        if (!memcmp(srcmac, ClientMAC[i], 6))
                        {
                            clientnum = i;
                            break;
                        }
                    }

                    if (clientnum < 0)
                    {
                        printf("wifi HLE: bad client MAC\n");
                        break;
                    }

                    if ((!(ClientMask & (0x10000 << clientnum))) ||
                        (ClientStatus[clientnum] != 1))
                    {
                        printf("wifi HLE: ?????\n");
                        break;
                    }

                    ClientMask &= ~(0x10000 << clientnum);
                    ClientMask |= (1 << clientnum);
                    ClientStatus[clientnum] = 3;
                    SendAssocResponse(srcmac, 0, 0xC000|clientnum);

                    u16 temp = NDS::ARM7Read16(SharedMem[1]+0x182);
                    temp |= (1 << clientnum);
                    NDS::ARM7Write16(SharedMem[1]+0x182, temp);
                    temp = NDS::ARM7Read16(SharedMem[1]+0x86);
                    temp &= ~(1 << clientnum);
                    NDS::ARM7Write16(SharedMem[1]+0x86, temp);

                    u32 mac_addr = SharedMem[1] + 0x128 + ((clientnum-1) * 6);
                    NDS::ARM7Write16(mac_addr+0, *(u16*)&srcmac[0]);
                    NDS::ARM7Write16(mac_addr+2, *(u16*)&srcmac[2]);
                    NDS::ARM7Write16(mac_addr+4, *(u16*)&srcmac[4]);

                    u32 tmp_addr = SharedMem[1] + 0x1F8 + (clientnum << 4);
                    for (int i = 0; i < 16; i+=2)
                        NDS::ARM7Write16(tmp_addr+i, 0);

                    u16 extra[19];
                    extra[0] = 7;
                    memcpy(&extra[1], srcmac, 6);
                    extra[4] = clientnum;
                    memset(&extra[5], 0, 0x18); // TODO -- what is this? WEP related?
                    extra[17] = NDS::ARM7Read16(SharedMem[1]+0x30);
                    extra[18] = NDS::ARM7Read16(SharedMem[1]+0x32);
                    WifiIPCReply(0x8, 0, 19, extra);
                }
                break;

            case 0xB: // auth frame
                {
                    // TODO check shito here

                    int clientnum = -1;
                    for (int i = 1; i < 16; i++)
                    {
                        if (!memcmp(srcmac, ClientMAC[i], 6))
                        {
                            clientnum = i;
                            break;
                        }
                    }
                    if (clientnum < 0)
                    {
                        u16 mask = ClientMask | (ClientMask >> 16);
                        for (int i = 1; i < 16; i++)
                        {
                            if (!(mask & (1<<i)))
                            {
                                clientnum = i;
                                break;
                            }
                        }
                    }

                    if (clientnum < 0)
                    {
                        printf("wifi HLE: ran out of client IDs\n");
                        break;
                    }

                    ClientMask |= (0x10000 << clientnum);
                    memcpy(ClientMAC[clientnum], srcmac, 6);
                    ClientStatus[clientnum] = 1;
                    SendAuthFrame(ClientMAC[clientnum], 0, 2, 0);

                    u16 extra = 0x13;
                    WifiIPCReply(0x80, 0, 1, &extra);
                }
                break;
            }
        }
        break;

    case 2: // data frame
        {
            printf("HOST DATA FRAME: %04X\n", framectl);
        }
        break;
    }
}

void ClientComm(int status)
{
    if (ReceiveFrame() <= 0)
        return;

    u16 framectl = *(u16*)&RXBuffer[12+0];
    framectl &= 0xE7FF;

    u16 framelen = *(u16*)&RXBuffer[10];

    switch ((framectl >> 2) & 0x3)
    {
    case 0: // management frame
        {
            u8* srcmac = &RXBuffer[12+10];

            switch ((framectl >> 4) & 0xF)
            {
                // TODO.
            }
        }
        break;

    case 2: // data frame
        {
            if (status != 10)
                break;

            u8* dstmac = &RXBuffer[12+4];
            if (*(u16*)&dstmac[0] != 0x0903) break;
            if (*(u16*)&dstmac[2] != 0x00BF) break;

            // TODO check source MAC and all that

            if (*(u16*)&dstmac[4] == 0x0000)
            {
                // CMD frame

                u16 curbuf = NDS::ARM7Read16(SharedMem[1]+0x70);
                curbuf ^= 1;
                NDS::ARM7Write16(SharedMem[1]+0x70, curbuf);

                u32 dstaddr = NDS::ARM7Read32(SharedMem[1]+0x74 + (curbuf<<2));
                u16 dstlen = NDS::ARM7Read16(SharedMem[1]+0x72);

                u16 bodylen;
                if (framelen >= 0x22) bodylen = framelen - 0x22;
                else bodylen = 0;

                u16 writelen = framelen - 4;
                if (writelen > (dstlen-0x14))
                    writelen = (dstlen-0x14);

                // TODO a lot more of the stuff

                NDS::ARM7Write16(dstaddr+0x00, 0);
                NDS::ARM7Write16(dstaddr+0x02, 1);
                NDS::ARM7Write16(dstaddr+0x04, 0);
                NDS::ARM7Write16(dstaddr+0x06, bodylen);
                // TODO figure this out
                NDS::ARM7Write16(dstaddr+0x08, bodylen ? 0xE81C : 0xA01C);
                NDS::ARM7Write16(dstaddr+0x0A, 0);
                NDS::ARM7Write16(dstaddr+0x0C, 0);
                NDS::ARM7Write16(dstaddr+0x0E, 0x8014);
                NDS::ARM7Write16(dstaddr+0x10, 0x4080);
                NDS::ARM7Write16(dstaddr+0x12, writelen);

                for (u32 i = 0; i < writelen; i+=2)
                {
                    NDS::ARM7Write16(dstaddr+0x14 + i, *(u16*)&RXBuffer[12 + i]);
                }

                u16 extra[3];
                extra[0] = 0xC;
                *(u32*)&extra[1] = dstaddr;
                WifiIPCReply(0xE, 0, 3, extra);

                if (bodylen)
                {
                    u16 cmdflags = *(u16*)&RXBuffer[12+24 + 4];
                    MPSignalRX(dstaddr, dstaddr+0x32, 0, cmdflags);
                }

                bool doreply = false;
                u16 clientID = NDS::ARM7Read16(SharedMem[1]+0x188);
                u16 clienttime = *(u16*)&RXBuffer[12+24];
                u16 clientmask = *(u16*)&RXBuffer[12+24 + 2];

                if (clientmask & (1<<clientID))
                {
                    u16 framelen = MPFrameMetadata[1];
                    framelen = (framelen*4) + 0xE0;
                    if (framelen <= clienttime)
                        doreply = true;
                }

                if (doreply)
                {
                    SendReplyFrame(MPFrameMetadata[0], MPFrameMetadata[1], MPFrameMetadata[3]);
                }
                else
                {
                    Platform::MP_SendReply(nullptr, 0, Timestamp, clientID);
                }

                MPFrameMetadata[1] = 0;
            }
            else if (*(u16*)&dstmac[4] == 0x0300)
            {
                // ack frame

                u16 extra[14];
                extra[0] = 0xD;
                extra[1] = 0;
                extra[2] = 0;
                extra[3] = 0; //
                extra[4] = 0x8014; //
                memcpy(&extra[5], dstmac, 6);
                memcpy(&extra[8], &RXBuffer[12+10], 6);
                extra[11] = *(u16*)&RXBuffer[12+22];
                extra[12] = *(u16*)&RXBuffer[12+24];
                extra[13] = *(u16*)&RXBuffer[12+26];
                WifiIPCReply(0xE, 0, 14, extra);
            }
        }
        break;
    }
}


void MSTimer(u32 param)
{
    u16 status = NDS::ARM7Read16(SharedMem[1]);

    Timestamp += 1024;

    switch (status)
    {
    case 3: // connecting to host
        {
            if (!HostConnect())
                ScheduleTimer(false);
        }
        break;

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

            HostComm(status);

            if ((status == 9) && (CmdCount > 0))
            {
                CmdCount--;
                ReceiveReplyFrames();
                if (CmdCount == 0)
                    SendAckFrame();
            }

            ScheduleTimer(false);
        }
        break;

    case 8:
    case 10: // client comm
        {
            ClientComm(status);

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


void SetCmdFrameSize(u16 size)
{
    NDS::ARM7Write16(SharedMem[1]+0x30, size);
    NDS::ARM7Write16(SharedMem[1]+0x34, size);
printf("SET CMD FRAME SIZE: %04X\n", size);
    size += 4;
    u16 clientID = NDS::ARM7Read16(SharedMem[1]+0x188);
    if (clientID)
    {printf("schprout %04X\n", size);
        NDS::ARM7Write16(SharedMem[1]+0x3E, size);
        NDS::ARM7Write16(SharedMem[1]+0x3A, size);
    }
    else
    {
        NDS::ARM7Write16(SharedMem[1]+0x3C, size);
        NDS::ARM7Write16(SharedMem[1]+0x38, size);
    }
}

void SetReplyFrameSize(u16 size)
{
    NDS::ARM7Write16(SharedMem[1]+0x36, size);
    NDS::ARM7Write16(SharedMem[1]+0x32, size);

    size += 2;
    u16 clientID = NDS::ARM7Read16(SharedMem[1]+0x188);
    if (clientID)
    {
        NDS::ARM7Write16(SharedMem[1]+0x3C, size);
        NDS::ARM7Write16(SharedMem[1]+0x38, size);
    }
    else
    {
        NDS::ARM7Write16(SharedMem[1]+0x3E, size);
        NDS::ARM7Write16(SharedMem[1]+0x3A, size);
    }
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

    u16 flags[4];
    flags[0] = NDS::ARM7Read16(paramblock + 0x0E);
    flags[1] = NDS::ARM7Read16(paramblock + 0x12);
    flags[2] = NDS::ARM7Read16(paramblock + 0x14);
    flags[3] = NDS::ARM7Read16(paramblock + 0x16);

    u8 beacontype = 0;
    if (flags[0] & 0x1) beacontype |= (1<<0);
    if (flags[1] & 0x1) beacontype |= (1<<1);
    if (flags[2] & 0x1) beacontype |= (1<<2);
    if (flags[3] & 0x1) beacontype |= (1<<3);

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
    len = (len + 1) & ~1;

    // FCS placeholder
    *(u32*)&BeaconFrame[0xC+len] = 0x1D46B6B8;
    len += 4;

    // frame length
    *(u16*)&BeaconFrame[0xA] = len;


    // setup extra stuff

    SetCmdFrameSize(cmd_len + (flags[2] ? 0x2A : 0));
    SetReplyFrameSize(reply_len + (flags[2] ? 6 : 0));
}


void WifiIPCRetry(u32 param)
{
    u16 flag = NDS::ARM7Read16(0x027FFF96);
    if (flag & 0x1)
    {printf("PRONSCHMO %04X\n", param);
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
        IPCReplyQueue.Write(reply);printf("PRONCHIASSE %04X\n", cmd);
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
        else if (numextra == 19)
        {
            NDS::ARM7Write16(replybuf+0xA, extra[1]); // client MAC
            NDS::ARM7Write16(replybuf+0xC, extra[2]);
            NDS::ARM7Write16(replybuf+0xE, extra[3]);
            NDS::ARM7Write16(replybuf+0x10, extra[4]); // client ID
            for (int i = 0; i < 12; i++)
            {
                NDS::ARM7Write16(replybuf+0x14+(i<<1), extra[5+i]);
            }
            NDS::ARM7Write16(replybuf+0x2C, extra[17]);
            NDS::ARM7Write16(replybuf+0x2E, extra[18]);
        }
    }
    else if (cmd == 0xA)
    {
        if (numextra >= 5)
        {
            NDS::ARM7Write16(replybuf+0x8, 5);
            NDS::ARM7Write16(replybuf+0x10, extra[0]); // channel
            NDS::ARM7Write16(replybuf+0x12, 0);

            // source MAC
            NDS::ARM7Write16(replybuf+0xA, extra[1]);
            NDS::ARM7Write16(replybuf+0xC, extra[2]);
            NDS::ARM7Write16(replybuf+0xE, extra[3]);

            NDS::ARM7Write16(replybuf+0x14, 0); // ???

            NDS::ARM7Write16(replybuf+0x36, extra[4]);
            for (int i = 0; i < extra[4]; i+=2)
            {
                NDS::ARM7Write16(replybuf+0x38+i, extra[5+(i>>1)]);
            }
        }
        else if (numextra >= 1)
        {
            NDS::ARM7Write16(replybuf+0x8, 4);
            NDS::ARM7Write16(replybuf+0x10, extra[0]);
            NDS::ARM7Write16(replybuf+0x12, 0);
        }
    }
    else if (cmd == 0xC && status == 0)
    {
        NDS::ARM7Write16(replybuf+0x8, extra[0]);
        if (extra[0] == 7)
        {
            NDS::ARM7Write16(replybuf+0xA, extra[1]);
            NDS::ARM7Write16(replybuf+0x10, extra[2]);
            NDS::ARM7Write16(replybuf+0x12, extra[3]);
            NDS::ARM7Write16(replybuf+0x14, extra[4]);
            NDS::ARM7Write16(replybuf+0x16, extra[5]);
            NDS::ARM7Write16(replybuf+0x18, extra[6]);
        }
    }
    else if (cmd == 0xE && status == 0)
    {
        NDS::ARM7Write16(replybuf+0x04, extra[0]);
        for (int i = 1; i < numextra; i++)
        {
            NDS::ARM7Write16(replybuf+0x8+(i*2), extra[i]);
        }
    }
    else if (cmd == 0x80)
    {
        NDS::ARM7Write16(replybuf+0x04, extra[0]);
    }
    else if (cmd == 0x81)
    {
        NDS::ARM7Write16(replybuf+0x8, extra[0]);
        NDS::ARM7Write16(replybuf+0xA, extra[1]);
        NDS::ARM7Write16(replybuf+0xC, extra[2]);
        NDS::ARM7Write16(replybuf+0xE, extra[3]);
        NDS::ARM7Write16(replybuf+0x10, extra[4]);
        NDS::ARM7Write16(replybuf+0x18, extra[7]);
        NDS::ARM7Write32(replybuf+0x14, *(u32*)&extra[5]);
        NDS::ARM7Write32(replybuf+0x1C, *(u32*)&extra[9]);
        NDS::ARM7Write32(replybuf+0x20, *(u32*)&extra[11]);
        NDS::ARM7Write16(replybuf+0x1A, extra[8]);
        NDS::ARM7Write16(replybuf+0x24, extra[13]);
        NDS::ARM7Write16(replybuf+0x26, extra[14]);
    }
    else if (cmd == 0x82)
    {
        NDS::ARM7Write16(replybuf+0x4, extra[0]);
        NDS::ARM7Write16(replybuf+0x6, extra[1]);
        NDS::ARM7Write32(replybuf+0x8, *(u32*)&extra[2]);
        NDS::ARM7Write32(replybuf+0xC, *(u32*)&extra[4]);
        NDS::ARM7Write16(replybuf+0x10, extra[6]);
        NDS::ARM7Write16(replybuf+0x12, extra[7]);
        NDS::ARM7Write16(replybuf+0x20, extra[9]);
        NDS::ARM7Write16(replybuf+0x1A, extra[8]);
        NDS::ARM7Write16(replybuf+0x40, extra[10]);
        NDS::ARM7Write16(replybuf+0x42, extra[11]);
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
    IPCCmdAddr = addr;

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
                // SHAREDMEM LAYOUT
                //
                // SHAREDMEM 0
                // offset  size  desc.
                // 00      4     pointer to sharedmem 1
                // 08      4     pointer to IPC response buffer
                // 10      0xC0  host data buffer (see command 0xC)
                // D0      32*4  ??? initialized to 0x8000 each
                //
                // SHAREDMEM 1
                // offset  size  desc.
                // 00      2     status
                //                   0 = disabled
                //                   1 = enabled
                //                   2 = inited, ready for operation
                //                   3 = connecting to host
                //                       (command 0xA also briefly transitions to state 3 before state 5)
                //                   5 = scanning for hosts
                //                   7 = host comm, waiting for clients
                //                   8 = client comm, connected to host
                //                   9 = host comm, local MP comm started
                //                   10 = client comm, local MP comm started
                // 02      2     last valid IPC command
                // 04      4     busy flag (1 = processing IPC command)
                // 08
                // 0C      4     init flag for something??
                // 10      4     ???
                // 14      4     ???
                // 18
                // 1C      4     ???
                // 20
                // 30      2     CMD frame size
                // 32      2     REPLY frame size
                // 34      2     CMD frame size
                // 36      2     REPLY frame size
                // 38      2     TX frame size (ie. CMD on host side, REPLY on client side)
                // 3A      2     RX frame size
                // 3C      2     TX frame size
                // 3E      2     RX frame size
                // 40      2     ??? inited to 0104
                // 42      2     ??? inited to 00F0
                // 44      2     ??? inited to 03E8
                // 46      2     ???
                // 48      4     ??? inited to 020B
                // 4C      4     ???
                // 50      4     ???
                // 54      4     ???
                // 58      2     ??? inited to 1
                // 5A      2     ??? inited to 1
                // 5C      2     ??? inited to 6
                // 60
                // 70      2     current RX buffer (0/1)
                // 72      2     length of RX buffer (see command 0xE)
                // 74      4     pointer to RX buffer 0
                // 78      4     pointer to RX buffer 1
                // 7C      4     pointer to TX buffer
                // 80      2     length of TX buffer
                // 86      2     host: ready clients (bitmask)
                // 88
                // 92      2     ???
                // 94      2     ???
                // 96
                // 98      2     ???
                // 9A      2     ???
                // 9C      2     ???
                // 9E
                // C2      2     ???
                // C4
                // C6      2     ???
                // C8
                // CE      2     ???
                // D0
                // E8      0x40  host: param block for host beacon (see command 0x7)
                // 128     6*15  host: client MAC addresses
                // 182     2     host: connected clients (bitmask)
                // 184
                // 188     2     client: client ID
                // 18A     6     client: host MAC address
                // 1EC     2     TX rate? (1/2)
                // 1EE     2     ??? inited to 1
                // 1F0
                // 1F4     2     allowed channels (bitmask)
                // 1F6     2     allowed channels (bitmask) (again?)
                // 1F8     16*16 sequence numbers (2 bytes, per stream, 8 streams per client)
                // 2F8
                // 71C     4*4   ???
                // 738     8*16  tick count at client connection (ID 0 on client side, 1-15 on host side)

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
                // PARAM BLOCK FORMAT -- 0x40 bytes
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
                extra[1] = NDS::ARM7Read16(SharedMem[1]+0x30);
                extra[2] = NDS::ARM7Read16(SharedMem[1]+0x32);
                WifiIPCReply(0x8, 0, 3, extra);

                NDS::ARM7Write16(SharedMem[1]+0xC2, 1);
                ScheduleTimer(true);
            }
            break;

        case 0xA: // start host scan
            {
                // scan for viable host beacons
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
                // BEACON DATA BUFFER -- 0xC0 bytes
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
            return;

        case 0xB: // stop host scan
            {
                u16 status = NDS::ARM7Read16(SharedMem[1]);
                if (status != 5)
                {
                    WifiIPCReply(0xB, 3);
                    break;
                }

                NDS::ARM7Write16(SharedMem[1], 2);
                HostScanCount = 0;

                WifiIPCReply(0xB, 0);

                // CHECKME
                WifiIPCReply(0xA, 0, 1, &Channel);
            }
            break;

        case 0xC: // connect to host
            {
                // COMMAND BUFFER
                // offset  size  desc.
                // 04      4     host data buffer
                //
                // HOST DATA BUFFER -- 0xC0 bytes
                // same format as beacon data buffer returned by command 0xA
                // except first word of tag DD is changed to 00000100?

                u16 status = NDS::ARM7Read16(SharedMem[1]);
                if (status != 2)
                {
                    u16 ext = 6;
                    WifiIPCReply(0xC, 3, 1, &ext);
                    break;
                }

                // copy host data buffer to sharedmem
                u32 hostdata = NDS::ARM7Read32(addr+0x4);
                for (int i = 0; i < 0xC0; i+=2)
                {
                    u16 tmp = NDS::ARM7Read16(hostdata+i);
                    NDS::ARM7Write16(SharedMem[0]+0x10+i, tmp);
                }

                if ((NDS::ARM7Read16(SharedMem[0]+0x4C) >= 16) &&
                    (!(NDS::ARM7Read8(SharedMem[0]+0x5B) & 0x01)))
                {
                    u16 ext = 6;
                    WifiIPCReply(0xC, 11, 1, &ext);
                    break;
                }

                // TODO check against 1F4 (allowed channels)
                // and other shit (TX rate)

                u16 ext = 6;
                WifiIPCReply(0xC, 0, 1, &ext);

                NDS::ARM7Write16(SharedMem[1], 3);

                u32 flag = NDS::ARM7Read32(addr+0x20);
                NDS::ARM7Write16(SharedMem[1]+0xC6, flag?1:0);

                ClientStatus[0] = 0;
                ScheduleTimer(true);

                // continue process after receiving host beacon
                // TODO: add timeouts to this
            }
            return;

        case 0xE: // start local MP
            {
                // COMMAND BUFFER
                // offset  size  desc.
                // 04      4     pointer to RX buffers (two contiguous buffers)
                // 08      4     RX buffer size
                // 0C      4     pointer to TX buffer
                // 10      4     TX buffer size
                // 14      28    parameter block (see command 0x23)

                // TODO all the checks and shito

                u32 rxbuf = NDS::ARM7Read32(addr+0x4);
                u32 rxlen = NDS::ARM7Read32(addr+0x8);
                u32 txbuf = NDS::ARM7Read32(addr+0xC);
                u32 txlen = NDS::ARM7Read32(addr+0x10);

                NDS::ARM7Write32(SharedMem[1]+0x74, rxbuf);
                NDS::ARM7Write16(SharedMem[1]+0x72, rxlen);
                NDS::ARM7Write32(SharedMem[1]+0x78, rxbuf+rxlen);
                NDS::ARM7Write16(SharedMem[1]+0x70, 0); // current RX buffer

                NDS::ARM7Write32(SharedMem[1]+0x7C, txbuf);
                NDS::ARM7Write16(SharedMem[1]+0x80, txlen);

                // TODO apply parameter block (addr+0x14 and up)

                u16 status = NDS::ARM7Read16(SharedMem[1]);
                if (status == 8)
                    NDS::ARM7Write16(SharedMem[1], 10);
                else if (status == 7)
                    NDS::ARM7Write16(SharedMem[1], 9);

                ScheduleTimer(true);

                printf("WIFI HLE: START MP\n");

                u16 extra = 0xA;
                WifiIPCReply(0xE, 0, 1, &extra);
            }
            break;

        case 0xF: // send MP data
            {
                // COMMAND BUFFER
                // offset  size  desc.
                // 04      4     pointer to frame data
                // 08      4     frame size
                // 0C      4     destination bitmask (forced to 1 on clients)
                // 10      4     bit0-2: stream number
                //               bit3: include sequence number
                // 14      4     ??
                // 18      4     callback
                // 1C      4     callback parameter
                //
                // REPLY 1:
                // REPLY BUFFER
                // offset  size  desc.
                // 00      2     0x81
                // 02      2     status (0)
                // 08      2     0x14
                // 0A      2     stream#/flag (from command buffer)
                // 0C      2     destination bitmask (from command buffer; 1 if client)
                // 0E      2     transmit failure bitmask?
                // 10      2     success bitmask?
                // 14      4     pointer to frame data (from command buffer)
                // 18      2     frame size (from command buffer)
                // 1A      2     success bitmask? (FFFF if destination bitmask is zero)
                // 1C      4     callback (from command buffer)
                // 20      4     callback parameter (from command buffer)
                // 24      2     TX frame size
                // 26      2     RX frame size
                //
                // REPLY 2: end of exchange
                // REPLY BUFFER
                // offset  size  desc.
                // 00      2     0xE
                // 02      2     status (0)
                // 04      2     0xB
                // 08      4     received frame data

                u32 framedata = NDS::ARM7Read32(addr+0x4);
                u32 framesize = NDS::ARM7Read32(addr+0x8);
                u32 destmask = NDS::ARM7Read32(addr+0xC);
                u32 flags = NDS::ARM7Read32(addr+0x10);
                u32 cb = NDS::ARM7Read32(addr+0x18);
                u32 cbparam = NDS::ARM7Read32(addr+0x1C);

                MPFrameMetadata[0] = framedata;
                MPFrameMetadata[1] = framesize;
                MPFrameMetadata[3] = flags;
                MPFrameMetadata[4] = cb;
                MPFrameMetadata[5] = cbparam;
printf("PROOO %08X %08X %08X\n", framedata, framesize, destmask);
                u16 clientID = NDS::ARM7Read16(SharedMem[1]+0x188);
                if (clientID)
                {
                    destmask = 0x0001;
                    MPFrameMetadata[2] = destmask;
printf("CLIENT FRAME SEND\n");
                    // client side TODO
                }
                else
                {
                    u16 connmask = NDS::ARM7Read16(SharedMem[1]+0x182);
                    destmask &= connmask;
                    MPFrameMetadata[2] = destmask; // CHECKME

                    if (destmask)
                    {
                        SendCmdFrame(framedata, framesize, destmask, flags);
                    }
                    else
                    {
                        SendCmdFrame(0, 0, connmask, flags);
                        MPSignalTX(0xFFFF);
                    }
                }
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
