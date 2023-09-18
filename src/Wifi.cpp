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
#include "SPI.h"
#include "Wifi.h"
#include "WifiAP.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace Wifi
{

//#define WIFI_LOG printf
#define WIFI_LOG(...) {}

#define PRINT_MAC(pf, mac) Log(LogLevel::Debug, "%s: %02X:%02X:%02X:%02X:%02X:%02X\n", pf, (mac)[0], (mac)[1], (mac)[2], (mac)[3], (mac)[4], (mac)[5]);

u8 RAM[0x2000];
u16 IO[0x1000>>1];

#define IOPORT(x) IO[(x)>>1]
#define IOPORT8(x) ((u8*)IO)[x]

// destination MACs for MP frames
const u8 MPCmdMAC[6]   = {0x03, 0x09, 0xBF, 0x00, 0x00, 0x00};
const u8 MPReplyMAC[6] = {0x03, 0x09, 0xBF, 0x00, 0x00, 0x10};
const u8 MPAckMAC[6]   = {0x03, 0x09, 0xBF, 0x00, 0x00, 0x03};

const int kTimerInterval = 8;
const u32 kTimeCheckMask = ~(kTimerInterval - 1);

bool Enabled;
bool PowerOn;

s32 TimerError;

u16 Random;

// general, always-on microsecond counter
u64 USTimestamp;

u64 USCounter;
u64 USCompare;
bool BlockBeaconIRQ14;

u32 CmdCounter;

u8 BBRegs[0x100];
u8 BBRegsRO[0x100];

u8 RFVersion;
u32 RFRegs[0x40];

struct TXSlot
{
    bool Valid;
    u16 Addr;
    u16 Length;
    u8 Rate;
    u8 CurPhase;
    int CurPhaseTime;
    u32 HalfwordTimeMask;
};

TXSlot TXSlots[6];
u8 TXBuffer[0x2000];

u8 RXBuffer[2048];
u32 RXBufferPtr;
int RXTime;
u32 RXHalfwordTimeMask;

u32 ComStatus; // 0=waiting for packets  1=receiving  2=sending
u32 TXCurSlot;
u32 RXCounter;

int MPReplyTimer;
u16 MPClientMask, MPClientFail;

u8 MPClientReplies[15*1024];

u16 MPLastSeqno;

bool MPInited;
bool LANInited;

int USUntilPowerOn;
bool ForcePowerOn;

// MULTIPLAYER SYNC APPARATUS
bool IsMP;
bool IsMPClient;
u64 NextSync;           // for clients: timestamp for next sync point
u64 RXTimestamp;

// multiplayer host TX sequence:
// 1. preamble
// 2. IRQ7
// 3. send data
// 4. optional IRQ1
// 5. wait for client replies (duration: 16 + ((10 * CMD_REPLYTIME) * numclients) + ack preamble (96))
// 6. IRQ7
// 7. send ack (32 bytes)
// 8. optional IRQ1, along with IRQ12 if the transfer was successful or if
//    there's no time left for a retry
//
// if the transfer has to be retried (for example, didn't get replies from all clients)
// and there is time, it repeats the sequence
//
// if there isn't enough time left on CMD_COUNT, IRQ12 is triggered alone when
// CMD_COUNT is 10, and the packet txheader[0] is set to 5
//
// RFSTATUS values:
// 0 = initial
// 1 = waiting for incoming packets
// 2 = switching from RX to TX
// 3 = TX
// 4 = switching from TX to RX
// 5 = MP host data sent, waiting for replies (RFPINS=0x0084)
// 6 = RX
// 7 = switching from RX reply to TX ack
// 8 = MP client sending reply, MP host sending ack (RFPINS=0x0046)
// 9 = idle


// wifi TODO:
// * RXSTAT
// * TX errors (if applicable)


bool Init()
{
    //MPInited = false;
    //LANInited = false;

    Platform::MP_Init();
    MPInited = true;

    Platform::LAN_Init();
    LANInited = true;

    WifiAP::Init();

    return true;
}

void DeInit()
{
    if (MPInited)
        Platform::MP_DeInit();
    if (LANInited)
        Platform::LAN_DeInit();

    WifiAP::DeInit();
}

void Reset()
{
    using namespace SPI_Firmware;
    memset(RAM, 0, 0x2000);
    memset(IO, 0, 0x1000);

    Enabled = false;
    PowerOn = false;

    Random = 1;

    memset(BBRegs, 0, 0x100);
    memset(BBRegsRO, 0, 0x100);

    #define BBREG_FIXED(id, val)  BBRegs[id] = val; BBRegsRO[id] = 1;
    BBREG_FIXED(0x00, 0x6D);
    BBREG_FIXED(0x0D, 0x00);
    BBREG_FIXED(0x0E, 0x00);
    BBREG_FIXED(0x0F, 0x00);
    BBREG_FIXED(0x10, 0x00);
    BBREG_FIXED(0x11, 0x00);
    BBREG_FIXED(0x12, 0x00);
    BBREG_FIXED(0x16, 0x00);
    BBREG_FIXED(0x17, 0x00);
    BBREG_FIXED(0x18, 0x00);
    BBREG_FIXED(0x19, 0x00);
    BBREG_FIXED(0x1A, 0x00);
    BBREG_FIXED(0x27, 0x00);
    BBREG_FIXED(0x4D, 0x00); // 00 or BF
    BBREG_FIXED(0x5D, 0x01);
    BBREG_FIXED(0x5E, 0x00);
    BBREG_FIXED(0x5F, 0x00);
    BBREG_FIXED(0x60, 0x00);
    BBREG_FIXED(0x61, 0x00);
    BBREG_FIXED(0x64, 0xFF); // FF or 3F
    BBREG_FIXED(0x66, 0x00);
    for (int i = 0x69; i < 0x100; i++)
    {
        BBREG_FIXED(i, 0x00);
    }
    #undef BBREG_FIXED

    RFVersion = GetFirmware()->Header().RFChipType;
    memset(RFRegs, 0, 4*0x40);

    FirmwareConsoleType console = GetFirmware()->Header().ConsoleType;
    if (console == FirmwareConsoleType::DS)
        IOPORT(0x000) = 0x1440;
    else if (console == FirmwareConsoleType::DSLite)
        IOPORT(0x000) = 0xC340;
    else if (NDS::ConsoleType == 1 && console == FirmwareConsoleType::DSi)
        IOPORT(0x000) = 0xC340; // DSi has the modern DS-wifi variant
    else
    {
        Log(LogLevel::Warn, "wifi: unknown console type %02X\n", console);
        IOPORT(0x000) = 0x1440;
    }

    memset(&IOPORT(0x018), 0xFF, 6);
    memset(&IOPORT(0x020), 0xFF, 6);

    // TODO: find out what the initial values are
    IOPORT(W_PowerUS) = 0x0001;

    USTimestamp = 0;

    USCounter = 0;
    USCompare = 0;
    BlockBeaconIRQ14 = false;

    memset(TXSlots, 0, sizeof(TXSlots));
    memset(TXBuffer, 0, sizeof(TXBuffer));
    ComStatus = 0;
    TXCurSlot = -1;
    RXCounter = 0;

    memset(RXBuffer, 0, sizeof(RXBuffer));
    RXBufferPtr = 0;
    RXTime = 0;
    RXHalfwordTimeMask = 0xFFFFFFFF;

    MPReplyTimer = 0;
    MPClientMask = 0;
    MPClientFail = 0;
    memset(MPClientReplies, 0, sizeof(MPClientReplies));

    MPLastSeqno = 0xFFFF;

    CmdCounter = 0;

    USUntilPowerOn = 0;
    ForcePowerOn = false;

    IsMP = false;
    IsMPClient = false;
    NextSync = 0;
    RXTimestamp = 0;

    WifiAP::Reset();
}

void DoSavestate(Savestate* file)
{
    file->Section("WIFI");

    // berp.
    // not sure we're saving enough shit at all there.
    // also: savestate and wifi can't fucking work together!!
    // or it can but you would be disconnected

    file->VarArray(RAM, 0x2000);
    file->VarArray(IO, 0x1000);

    file->Bool32(&Enabled);
    file->Bool32(&PowerOn);

    file->Var16(&Random);

    file->Var32((u32*)&TimerError);

    file->VarArray(BBRegs, 0x100);
    file->VarArray(BBRegsRO, 0x100);

    file->Var8(&RFVersion);
    file->VarArray(RFRegs, 4*0x40);

    file->Var64(&USCounter);
    file->Var64(&USCompare);
    file->Bool32(&BlockBeaconIRQ14);

    file->Var32(&CmdCounter);

    file->Var64(&USTimestamp);

    for (int i = 0; i < 6; i++)
    {
        TXSlot* slot = &TXSlots[i];

        file->Bool32(&slot->Valid);
        file->Var16(&slot->Addr);
        file->Var16(&slot->Length);
        file->Var8(&slot->Rate);
        file->Var8(&slot->CurPhase);
        file->Var32((u32*)&slot->CurPhaseTime);
        file->Var32(&slot->HalfwordTimeMask);
    }

    file->VarArray(TXBuffer, sizeof(TXBuffer));

    file->VarArray(RXBuffer, sizeof(RXBuffer));
    file->Var32(&RXBufferPtr);
    file->Var32((u32*)&RXTime);
    file->Var32(&RXHalfwordTimeMask);

    file->Var32(&ComStatus);
    file->Var32(&TXCurSlot);
    file->Var32(&RXCounter);

    file->Var32((u32*)&MPReplyTimer);
    file->Var16(&MPClientMask);
    file->Var16(&MPClientFail);

    file->VarArray(MPClientReplies, sizeof(MPClientReplies));

    file->Var16(&MPLastSeqno);

    file->Var32((u32*)&USUntilPowerOn);
    file->Bool32(&ForcePowerOn);

    file->Bool32(&IsMP);
    file->Bool32(&IsMPClient);
    file->Var64(&NextSync);
    file->Var64(&RXTimestamp);
}


void ScheduleTimer(bool first)
{
    if (first) TimerError = 0;

    s32 cycles = 33513982 * kTimerInterval;
    cycles -= TimerError;
    s32 delay = (cycles + 999999) / 1000000;
    TimerError = (delay * 1000000) - cycles;

    NDS::ScheduleEvent(NDS::Event_Wifi, !first, delay, USTimer, 0);
}

void UpdatePowerOn()
{
    bool on = Enabled;

    if (NDS::ConsoleType == 1)
    {
        // TODO for DSi:
        // * W_POWER_US doesn't work (atleast on DWM-W024)
        // * other registers like GPIO_WIFI may also control wifi power/clock
        // * turning wifi off via POWCNT2 while sending breaks further attempts at sending frames
    }
    else
    {
        on = on && ((IOPORT(W_PowerUS) & 0x1) == 0);
    }

    if (on == PowerOn)
        return;

    PowerOn = on;
    if (on)
    {
        Log(LogLevel::Debug, "WIFI: ON\n");

        ScheduleTimer(true);

        Platform::MP_Begin();
    }
    else
    {
        Log(LogLevel::Debug, "WIFI: OFF\n");

        NDS::CancelEvent(NDS::Event_Wifi);

        Platform::MP_End();
    }
}

void SetPowerCnt(u32 val)
{
    Enabled = val & (1<<1);
    UpdatePowerOn();
}


void PowerDown();
void StartTX_Beacon();

void SetIRQ(u32 irq)
{
    u32 oldflags = IOPORT(W_IF) & IOPORT(W_IE);

    IOPORT(W_IF) |= (1<<irq);
    u32 newflags = IOPORT(W_IF) & IOPORT(W_IE);

    if ((oldflags == 0) && (newflags != 0))
        NDS::SetIRQ(1, NDS::IRQ_Wifi);
}

void SetIRQ13()
{
    SetIRQ(13);

    if (!(IOPORT(W_PowerTX) & 0x0002))
    {
        IOPORT(0x034) = 0x0002;
        //PowerDown();
        // FIXME!!
        IOPORT(W_RFPins) = 0x0046;
        IOPORT(W_RFStatus) = 9;
    }
}

void SetIRQ14(int source) // 0=USCOMPARE 1=BEACONCOUNT 2=forced
{
    if (source != 2)
        IOPORT(W_BeaconCount1) = IOPORT(W_BeaconInterval);

    if (BlockBeaconIRQ14 && source == 1)
        return;
    if (!(IOPORT(W_USCompareCnt) & 0x0001))
        return;

    SetIRQ(14);

    if (source == 2)
        Log(LogLevel::Debug, "wifi: weird forced IRQ14\n");

    IOPORT(W_BeaconCount2) = 0xFFFF;
    IOPORT(W_TXReqRead) &= 0xFFF2;

    if (IOPORT(W_TXSlotBeacon) & 0x8000)
    {
        StartTX_Beacon();
    }

    if (IOPORT(W_ListenCount) == 0)
        IOPORT(W_ListenCount) = IOPORT(W_ListenInterval);

    IOPORT(W_ListenCount)--;
}

void SetIRQ15()
{
    SetIRQ(15);

    if (IOPORT(W_PowerTX) & 0x0001)
    {
        IOPORT(W_RFPins) |= 0x0080;
        IOPORT(W_RFStatus) = 1;
    }
}


void SetStatus(u32 status)
{
    // TODO, eventually: states 2/4/7
    u16 rfpins[10] = {0x04, 0x84, 0, 0x46, 0, 0x84, 0x87, 0, 0x46, 0x04};
    IOPORT(W_RFStatus) = status;
    IOPORT(W_RFPins) = rfpins[status];
}


void PowerDown()
{
    IOPORT(W_TXReqRead) &= ~0x000F;
    IOPORT(W_PowerState) |= 0x0200;

    // if the RF hardware is powered down while still sending or receiving,
    // the current frame is completed before going idle
    if (!ComStatus)
    {
        SetStatus(9);
    }
}


bool MACEqual(const u8* a, const u8* b)
{
    return (*(u32*)&a[0] == *(u32*)&b[0]) && (*(u16*)&a[4] == *(u16*)&b[4]);
}


int PreambleLen(int rate)
{
    if (rate == 1) return 192;
    if (IOPORT(W_Preamble) & 0x0004) return 96;
    return 192;
}

u32 NumClients(u16 bitmask)
{
    u32 ret = 0;
    for (int i = 1; i < 16; i++)
    {
        if (bitmask & (1<<i)) ret++;
    }
    return ret;
}

void IncrementTXCount(TXSlot* slot)
{
    u8 cnt = RAM[slot->Addr + 0x4];
    if (cnt < 0xFF) cnt++;
    *(u16*)&RAM[slot->Addr + 0x4] = cnt;
}

void ReportMPReplyErrors(u16 clientfail)
{
    // TODO: do these trigger any IRQ?

    for (int i = 1; i < 16; i++)
    {
        if (!(clientfail & (1<<i)))
            continue;

        IOPORT8(W_CMDStat0 + i)++;
    }
}

void TXSendFrame(TXSlot* slot, int num)
{
    u32 noseqno = 0;

    if (RAM[slot->Addr + 0x4])
    {
        noseqno = 2;
    }
    else
    {
        if (num == 1) noseqno = (IOPORT(W_TXSlotCmd) & 0x4000) ? 1:0;
    }

    if (!noseqno)
    {
        if (!(IOPORT(W_TXHeaderCnt) & (1<<2)))
            *(u16*)&RAM[slot->Addr + 0xC + 22] = IOPORT(W_TXSeqNo) << 4;
        IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;
    }

    u16 framectl = *(u16*)&RAM[slot->Addr + 0xC];
    if (framectl & (1<<14))
    {
        // WEP frame
        // TODO: what happens when sending a WEP frame while WEP processing is off?
        // TODO: some form of actual WEP processing?
        // for now we just set the WEP FCS to a nonzero value, because some games require it

        if (IOPORT(W_WEPCnt) & (1<<15))
        {
            u32 wep_fcs = (slot->Addr + 0xC + slot->Length - 7) & ~0x1;
            *(u32*)&RAM[wep_fcs] = 0x22334466;
        }
    }

    int len = slot->Length;
    if ((slot->Addr + len) > 0x1FF4)
        len = 0x1FF4 - slot->Addr;

    memcpy(TXBuffer, &RAM[slot->Addr], 12+len);

    if (noseqno == 2)
        *(u16*)&TXBuffer[0xC] |= (1<<11);

    switch (num)
    {
    case 0:
    case 2:
    case 3:
        Platform::MP_SendPacket(TXBuffer, 12+len, USTimestamp);
        if (!IsMP) WifiAP::SendPacket(TXBuffer, 12+len);
        break;

    case 1:
        *(u16*)&TXBuffer[12 + 24+2] = MPClientMask;
        Platform::MP_SendCmd(TXBuffer, 12+len, USTimestamp);
        break;

    case 5:
        IncrementTXCount(slot);
        Platform::MP_SendReply(TXBuffer, 12+len, USTimestamp, IOPORT(W_AIDLow));
        break;

    case 4:
        *(u64*)&TXBuffer[0xC + 24] = USCounter;
        Platform::MP_SendPacket(TXBuffer, 12+len, USTimestamp);
        break;
    }
}

void StartTX_LocN(int nslot, int loc)
{
    TXSlot* slot = &TXSlots[nslot];

    if (IOPORT(W_TXSlotLoc1 + (loc*4)) & 0x7000)
        Log(LogLevel::Warn, "wifi: unusual loc%d bits set %04X\n", loc, IOPORT(W_TXSlotLoc1 + (loc*4)));

    slot->Valid = true;

    slot->Addr = (IOPORT(W_TXSlotLoc1 + (loc*4)) & 0x0FFF) << 1;
    slot->Length = *(u16*)&RAM[slot->Addr + 0xA] & 0x3FFF;

    u8 rate = RAM[slot->Addr + 0x8];
    if (rate == 0x14) slot->Rate = 2;
    else              slot->Rate = 1;

    slot->CurPhase = 0;
    slot->CurPhaseTime = PreambleLen(slot->Rate);
}

void StartTX_Cmd()
{
    TXSlot* slot = &TXSlots[1];

    if (IOPORT(W_TXSlotCmd) & 0x3000)
        Log(LogLevel::Warn,"wifi: !! unusual TXSLOT_CMD bits set %04X\n", IOPORT(W_TXSlotCmd));

    slot->Valid = true;

    slot->Addr = (IOPORT(W_TXSlotCmd) & 0x0FFF) << 1;
    slot->Length = *(u16*)&RAM[slot->Addr + 0xA] & 0x3FFF;

    u8 rate = RAM[slot->Addr + 0x8];
    if (rate == 0x14) slot->Rate = 2;
    else              slot->Rate = 1;

    MPClientMask = *(u16*)&RAM[slot->Addr + 12 + 24 + 2] & MPClientFail;
    MPClientFail &= MPClientMask;

    u32 duration = PreambleLen(slot->Rate) + (slot->Length * (slot->Rate==2 ? 4:8));
    duration += 112 + ((10 + IOPORT(W_CmdReplyTime)) * NumClients(MPClientMask));
    duration += (32 * (slot->Rate==2 ? 4:8));

    if (CmdCounter > (duration + 100))
    {
        slot->CurPhase = 0;
        slot->CurPhaseTime = PreambleLen(slot->Rate);
    }
    else
    {
        slot->CurPhase = 13;
        slot->CurPhaseTime = CmdCounter - 100;
    }
}

void StartTX_Beacon()
{
    TXSlot* slot = &TXSlots[4];

    slot->Valid = true;

    slot->Addr = (IOPORT(W_TXSlotBeacon) & 0x0FFF) << 1;
    slot->Length = *(u16*)&RAM[slot->Addr + 0xA] & 0x3FFF;

    u8 rate = RAM[slot->Addr + 0x8];
    if (rate == 0x14) slot->Rate = 2;
    else              slot->Rate = 1;

    slot->CurPhase = 0;
    slot->CurPhaseTime = PreambleLen(slot->Rate);

    IOPORT(W_TXBusy) |= 0x0010;
}

void FireTX()
{
    if (!(IOPORT(W_RXCnt) & 0x8000))
        return;

    u16 txbusy = IOPORT(W_TXBusy);

    u16 txreq = IOPORT(W_TXReqRead);
    u16 txstart = 0;
    if (IOPORT(W_TXSlotLoc1) & 0x8000) txstart |= 0x0001;
    if (IOPORT(W_TXSlotCmd ) & 0x8000) txstart |= 0x0002;
    if (IOPORT(W_TXSlotLoc2) & 0x8000) txstart |= 0x0004;
    if (IOPORT(W_TXSlotLoc3) & 0x8000) txstart |= 0x0008;

    txstart &= txreq;
    txstart &= ~txbusy;

    IOPORT(W_TXBusy) = txbusy | txstart;

    if (txstart & 0x0008)
    {
        StartTX_LocN(3, 2);
        return;
    }

    if (txstart & 0x0004)
    {
        StartTX_LocN(2, 1);
        return;
    }

    if (txstart & 0x0002)
    {
        MPClientFail = 0xFFFE;
        StartTX_Cmd();
        return;
    }

    if (txstart & 0x0001)
    {
        StartTX_LocN(0, 0);
        return;
    }
}

void SendMPDefaultReply()
{
    u8 reply[12 + 28];

    *(u16*)&reply[0xA] = 28; // length

    // rate
    //if (TXSlots[1].Rate == 2) reply[0x8] = 0x14;
    //else                      reply[0x8] = 0xA;
    // TODO
    reply[0x8] = 0x14;

    *(u16*)&reply[0xC + 0x00] = 0x0158;
    *(u16*)&reply[0xC + 0x02] = 0x00F0;//0; // TODO??
    *(u16*)&reply[0xC + 0x04] = IOPORT(W_BSSID0);
    *(u16*)&reply[0xC + 0x06] = IOPORT(W_BSSID1);
    *(u16*)&reply[0xC + 0x08] = IOPORT(W_BSSID2);
    *(u16*)&reply[0xC + 0x0A] = IOPORT(W_MACAddr0);
    *(u16*)&reply[0xC + 0x0C] = IOPORT(W_MACAddr1);
    *(u16*)&reply[0xC + 0x0E] = IOPORT(W_MACAddr2);
    *(u16*)&reply[0xC + 0x10] = 0x0903;
    *(u16*)&reply[0xC + 0x12] = 0x00BF;
    *(u16*)&reply[0xC + 0x14] = 0x1000;
    *(u16*)&reply[0xC + 0x16] = IOPORT(W_TXSeqNo) << 4;
    *(u32*)&reply[0xC + 0x18] = 0;

    int txlen = Platform::MP_SendReply(reply, 12+28, USTimestamp, IOPORT(W_AIDLow));
    WIFI_LOG("wifi: sent %d/40 bytes of MP default reply\n", txlen);
}

void SendMPReply(u16 clienttime, u16 clientmask)
{
    TXSlot* slot = &TXSlots[5];

    // mark the last packet as success. dunno what the MSB is, it changes.
    //if (slot->Valid)
    if (IOPORT(W_TXSlotReply2) & 0x8000)
        *(u16*)&RAM[slot->Addr] = 0x0001;

    // CHECKME!!
    // can the transfer rate for MP replies be set, or is it determined from the CMD transfer rate?
    // how does it work for default empty replies?
    slot->Rate = 2;

    IOPORT(W_TXSlotReply2) = IOPORT(W_TXSlotReply1);
    IOPORT(W_TXSlotReply1) = 0;

    if (!(IOPORT(W_TXSlotReply2) & 0x8000))
    {
        slot->Valid = false;
    }
    else
    {
        slot->Valid = true;

        slot->Addr = (IOPORT(W_TXSlotReply2) & 0x0FFF) << 1;
        slot->Length = *(u16*)&RAM[slot->Addr + 0xA] & 0x3FFF;

        // the packet is entirely ignored if it lasts longer than the maximum reply time
        u32 duration = PreambleLen(slot->Rate) + (slot->Length * (slot->Rate==2 ? 4:8));
        if (duration > clienttime)
            slot->Valid = false;
    }

    // this seems to be set upon IRQ0
    // TODO: how does it behave if the packet addr is changed before it gets sent? (maybe just not possible)
    if (slot->Valid)
    {
        slot->CurPhase = 0;

        TXSendFrame(slot, 5);
    }
    else
    {
        slot->CurPhase = 10;

        SendMPDefaultReply();
    }

    u16 clientnum = 0;
    for (int i = 1; i < IOPORT(W_AIDLow); i++)
    {
        if (clientmask & (1<<i))
            clientnum++;
    }

    slot->CurPhaseTime = 16 + ((clienttime + 10) * clientnum) + PreambleLen(slot->Rate);

    IOPORT(W_TXBusy) |= 0x0080;
}

void SendMPAck(u16 cmdcount, u16 clientfail)
{
    u8 ack[12 + 32];

    *(u16*)&ack[0xA] = 32; // length

    // rate
    if (TXSlots[1].Rate == 2) ack[0x8] = 0x14;
    else                      ack[0x8] = 0xA;

    *(u16*)&ack[0xC + 0x00] = 0x0218;
    *(u16*)&ack[0xC + 0x02] = 0;
    *(u16*)&ack[0xC + 0x04] = 0x0903;
    *(u16*)&ack[0xC + 0x06] = 0x00BF;
    *(u16*)&ack[0xC + 0x08] = 0x0300;
    *(u16*)&ack[0xC + 0x0A] = IOPORT(W_BSSID0);
    *(u16*)&ack[0xC + 0x0C] = IOPORT(W_BSSID1);
    *(u16*)&ack[0xC + 0x0E] = IOPORT(W_BSSID2);
    *(u16*)&ack[0xC + 0x10] = IOPORT(W_MACAddr0);
    *(u16*)&ack[0xC + 0x12] = IOPORT(W_MACAddr1);
    *(u16*)&ack[0xC + 0x14] = IOPORT(W_MACAddr2);
    *(u16*)&ack[0xC + 0x16] = IOPORT(W_TXSeqNo) << 4;
    *(u16*)&ack[0xC + 0x18] = cmdcount;
    *(u16*)&ack[0xC + 0x1A] = clientfail;
    *(u32*)&ack[0xC + 0x1C] = 0;

    if (!clientfail)
    {
        u32 nextbeacon;
        if (IOPORT(W_TXBusy) & 0x0010)
            nextbeacon = 0;
        else
            nextbeacon = ((IOPORT(W_BeaconCount1) - 1) << 10) + (0x400 - (USCounter & 0x3FF));
        int runahead = std::min(CmdCounter, nextbeacon);
        if (CmdCounter < 1000) runahead -= 210;
        *(u32*)&ack[0] = std::max(runahead - (32*(TXSlots[1].Rate==2?4:8)), 0);
    }
    else
    {
        *(u32*)&ack[0] = PreambleLen(TXSlots[1].Rate);
    }

    int txlen = Platform::MP_SendAck(ack, 12+32, USTimestamp);
    WIFI_LOG("wifi: sent %d/44 bytes of MP ack, %d %d\n", txlen, ComStatus, RXTime);
}

bool CheckRX(int type);
void MPClientReplyRX(int client);

bool ProcessTX(TXSlot* slot, int num)
{
    slot->CurPhaseTime -= kTimerInterval;
    if (slot->CurPhaseTime > 0)
    {
        if (slot->CurPhase == 1)
        {
            if (!(slot->CurPhaseTime & slot->HalfwordTimeMask))
                IOPORT(W_RXTXAddr)++;
        }
        else if (slot->CurPhase == 2)
        {
            MPReplyTimer -= kTimerInterval;
            if (MPReplyTimer <= 0 && MPClientMask != 0)
            {
                int nclient = 1;
                while (!(MPClientMask & (1 << nclient))) nclient++;

                u32 curclient = 1 << nclient;

                if (!(MPClientFail & curclient))
                    MPClientReplyRX(nclient);

                MPReplyTimer += 10 + IOPORT(W_CmdReplyTime);
                MPClientMask &= ~curclient;
            }
        }

        return false;
    }

    switch (slot->CurPhase)
    {
    case 0: // preamble done
        {
            SetIRQ(7);

            if (num == 5)
                SetStatus(8);
            else
                SetStatus(3);

            u32 len = slot->Length;
            if (slot->Rate == 2)
            {
                len *= 4;
                slot->HalfwordTimeMask = 0x7 & kTimeCheckMask;
            }
            else
            {
                len *= 8;
                slot->HalfwordTimeMask = 0xF & kTimeCheckMask;
            }

            slot->CurPhase = 1;
            slot->CurPhaseTime = len;

            // set TX addr
            IOPORT(W_RXTXAddr) = slot->Addr >> 1;

            if (num != 5)
            {
                TXSendFrame(slot, num);
            }

            // if the packet is being sent via LOC1..3, send it to the AP
            // any packet sent via CMD/REPLY/BEACON isn't going to have much use outside of local MP
            if (num == 0 || num == 2 || num == 3)
            {
                u16 framectl = *(u16*)&RAM[slot->Addr + 0xC];
                if ((framectl & 0x00FF) == 0x0010)
                {
                    u16 aid = *(u16*)&RAM[slot->Addr + 0xC + 24 + 4];
                    if (aid) Log(LogLevel::Debug, "[HOST] syncing client %04X, sync=%016llX\n", aid, USTimestamp);
                }
                else if ((framectl & 0x00FF) == 0x00C0)
                {
                    if (IsMPClient)
                    {
                        Log(LogLevel::Info, "[CLIENT] deauth\n");
                        IsMP = false;
                        IsMPClient = false;
                    }
                }
            }
        }
        break;

    case 10: // preamble done (default empty MP reply)
        {
            SetIRQ(7);
            SetStatus(8);

            slot->CurPhase = 11;
            slot->CurPhaseTime = 28*4;
            slot->HalfwordTimeMask = 0xFFFFFFFF;
        }
        break;

    case 1: // transmit done
        {
            // for the MP CMD and reply slots, this is set later
            if (num != 1 && num != 5)
                *(u16*)&RAM[slot->Addr] = 0x0001;
            RAM[slot->Addr + 5] = 0;

            if (num == 1)
            {
                if (IOPORT(W_TXStatCnt) & 0x4000)
                {
                    IOPORT(W_TXStat) = 0x0800;
                    SetIRQ(1);
                }
                SetStatus(5);

                MPReplyTimer = 16 + PreambleLen(slot->Rate);

                u16 res = 0;
                if (MPClientMask)
                    res = Platform::MP_RecvReplies(MPClientReplies, USTimestamp, MPClientMask);
                MPClientFail &= ~res;

                // TODO: 112 likely includes the ack preamble, which needs adjusted
                // for long-preamble settings
                slot->CurPhase = 2;
                slot->CurPhaseTime = 112 + ((10 + IOPORT(W_CmdReplyTime)) * NumClients(MPClientMask));

                break;
            }
            else if (num == 5)
            {
                if (IOPORT(W_TXStatCnt) & 0x1000)
                {
                    IOPORT(W_TXStat) = 0x0401;
                    SetIRQ(1);
                }
                SetStatus(1);

                IOPORT(W_TXBusy) &= ~0x80;
                FireTX();
                return true;
            }

            IOPORT(W_TXBusy) &= ~(1<<num);

            switch (num)
            {
            case 0:
            case 2:
            case 3:
                IOPORT(W_TXStat) = 0x0001 | ((num?(num-1):0)<<12);
                SetIRQ(1);
                IOPORT(W_TXSlotLoc1 + ((num?(num-1):0)*4)) &= 0x7FFF;
                break;

            case 4: // beacon
                if (IOPORT(W_TXStatCnt) & 0x8000)
                {
                    IOPORT(W_TXStat) = 0x0301;
                    SetIRQ(1);
                }
                break;
            }

            SetStatus(1);

            FireTX();
        }
        return true;

    case 11: // MP default reply transfer finished
        {
            IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;

            IOPORT(W_TXBusy) &= ~0x80;
            SetStatus(1);
            FireTX();
        }
        return true;

    case 2: // MP host transfer done
        {
            SetIRQ(7);
            SetStatus(8);

            IOPORT(W_RXTXAddr) = 0xFC0;

            if (slot->Rate == 2) slot->CurPhaseTime = 32 * 4;
            else                 slot->CurPhaseTime = 32 * 8;

            ReportMPReplyErrors(MPClientFail);

            // send
            u16 cmdcount = (CmdCounter + 9) / 10;
            SendMPAck(cmdcount, MPClientFail);

            slot->CurPhase = 3;
        }
        break;

    case 3: // MP host ack transfer (reply wait done)
        {
            if (!MPClientFail)
                *(u16*)&RAM[slot->Addr] = 0x0001;
            else
                *(u16*)&RAM[slot->Addr] = 0x0005;

            // this is set to indicate which clients failed to reply
            *(u16*)&RAM[slot->Addr + 0x2] = MPClientFail;
            if (!MPClientFail)
                IncrementTXCount(slot);

            IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;

            if (IOPORT(W_TXStatCnt) & 0x2000)
            {
                IOPORT(W_TXStat) = 0x0B01;
                SetIRQ(1);
            }

            if (MPClientFail && false)
            {
                // if some clients failed to respond: try again
                // TODO: fix this (causes instability)
                StartTX_Cmd();
                break;
            }
            else
            {
                IOPORT(W_TXBusy) &= ~(1<<1);
                IOPORT(W_TXSlotCmd) &= 0x7FFF;

                SetStatus(1);
                SetIRQ(12);

                FireTX();
            }
        }
        return true;

    case 13: // MP transfer failed (timeout)
        {
            IOPORT(W_TXBusy) &= ~(1<<1);
            IOPORT(W_TXSlotCmd) &= 0x7FFF;

            *(u16*)&RAM[slot->Addr] = 0x0005;

            IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;

            SetStatus(1);
            SetIRQ(12);

            FireTX();
        }
        return true;
    }

    return false;
}


inline void IncrementRXAddr(u16& addr, u16 inc = 2)
{
    for (u32 i = 0; i < inc; i += 2)
    {
        addr += 2;
        addr &= 0x1FFE;
        if (addr == (IOPORT(W_RXBufEnd) & 0x1FFE))
            addr = (IOPORT(W_RXBufBegin) & 0x1FFE);
    }
}

void StartRX()
{
    u16 framelen = *(u16*)&RXBuffer[8];
    RXTime = framelen;

    u16 txrate = *(u16*)&RXBuffer[6];
    if (txrate == 0x14)
    {
        RXTime *= 4;
        RXHalfwordTimeMask = 0x7 & kTimeCheckMask;
    }
    else
    {
        RXTime *= 8;
        RXHalfwordTimeMask = 0xF & kTimeCheckMask;
    }

    u16 addr = IOPORT(W_RXBufWriteCursor) << 1;
    IncrementRXAddr(addr, 12);
    IOPORT(W_RXTXAddr) = addr >> 1;

    RXBufferPtr = 12;

    SetIRQ(6);
    SetStatus(6);
    ComStatus |= 1;
}

void FinishRX()
{
    ComStatus &= ~0x1;
    RXCounter = 0;

    if (!ComStatus)
    {
        if (IOPORT(W_PowerState) & 0x0300)
            SetStatus(9);
        else
            SetStatus(1);
    }

    // TODO: RX stats

    u16 framectl = *(u16*)&RXBuffer[12];
    u16 seqno = *(u16*)&RXBuffer[12 + 22];

    // check the frame's destination address
    // note: the hardware always checks the first address field, regardless of the frame type/etc
    // similarly, the second address field is used to send acks to non-broadcast frames

    u8* dstmac = &RXBuffer[12 + 4];
    if (!(dstmac[0] & 0x01))
    {
        if (!MACEqual(dstmac, (u8*)&IOPORT(W_MACAddr0)))
            return;
    }

    // reject the frame if it's a WEP frame and WEP is off
    // TODO: check if sending WEP frames with WEP off works at all?

    if (framectl & (1<<14))
    {
        if (!(IOPORT(W_WEPCnt) & (1<<15)))
            return;
    }

    // apply RX filtering
    // TODO:
    // * RXFILTER bits 0, 9, 10, 12 not fully understood
    // * port 0D8 also affects reception of frames
    // * MP CMD frames with a duplicate sequence number are ignored

    u16 rxflags = 0x0010;
    bool cmd_dupe = false;

    switch ((framectl >> 2) & 0x3)
    {
    case 0: // management
        {
            u8* bssid = &RXBuffer[12 + 16];
            if (MACEqual(bssid, (u8*)&IOPORT(W_BSSID0)))
                rxflags |= 0x8000;

            u16 subtype = (framectl >> 4) & 0xF;
            if (subtype == 0x8) // beacon
            {
                if (!(rxflags & 0x8000))
                {
                    if (!(IOPORT(W_RXFilter) & (1<<0)))
                        return;
                }

                rxflags |= 0x0001;
            }
            else if ((subtype <= 0x5) ||
                     (subtype >= 0xA && subtype <= 0xC))
            {
                if (!(rxflags & 0x8000))
                {
                    // CHECKME!
                    if (!(IOPORT(W_RXFilter) & (3<<9)))
                        return;
                }
            }
        }
        break;

    case 1: // control
        {
            if ((framectl & 0xF0) == 0xA0) // PS-poll
            {
                u8* bssid = &RXBuffer[12 + 4];
                if (MACEqual(bssid, (u8*)&IOPORT(W_BSSID0)))
                    rxflags |= 0x8000;

                if (!(rxflags & 0x8000))
                {
                    if (!(IOPORT(W_RXFilter) & (1<<11)))
                        return;
                }

                rxflags |= 0x0005;
            }
            else
                return;
        }
        break;

    case 2: // data
        {
            u16 fromto = (framectl >> 8) & 0x3;
            if (IOPORT(W_RXFilter2) & (1<<fromto))
                return;

            int bssidoffset[4] = {16, 4, 10, 0};
            if (bssidoffset[fromto])
            {
                u8* bssid = &RXBuffer[12 + bssidoffset[fromto]];
                if (MACEqual(bssid, (u8*)&IOPORT(W_BSSID0)))
                    rxflags |= 0x8000;
            }

            u16 rxfilter = IOPORT(W_RXFilter);

            if (!(rxflags & 0x8000))
            {
                if (!(rxfilter & (1<<11)))
                    return;
            }

            if (framectl & (1<<11)) // retransmit
            {
                if (!(rxfilter & (1<<0)))
                    return;
            }

            // check for MP frames
            // the hardware simply checks for these specific MAC addresses
            // the reply check has priority over the other checks
            // TODO: it seems to be impossible to receive a MP reply outside of a CMD transfer's reply timeframe
            // if the framectl subtype field is 1 or 5
            // maybe one of the unknown registers controls that?
            // maybe it is impossible to receive CF-Ack frames outside of a CF-Poll period?
            // TODO: GBAtek says frame type F is for all empty packets?
            // my hardware tests say otherwise

            if (MACEqual(&RXBuffer[12 + 16], MPReplyMAC))
            {
                if ((framectl & 0xF0) == 0x50)
                    rxflags |= 0x000F;
                else
                    rxflags |= 0x000E;
            }
            else if (MACEqual(&RXBuffer[12 + 4], MPCmdMAC))
            {
                if (seqno == MPLastSeqno) cmd_dupe = true;
                MPLastSeqno = seqno;

                rxflags |= 0x000C;
            }
            else if (MACEqual(&RXBuffer[12 + 4], MPAckMAC))
            {
                rxflags |= 0x000D;
            }
            else
            {
                rxflags |= 0x0008;
            }

            switch ((framectl >> 4) & 0xF)
            {
            case 0x0: break;

            case 0x1:
                if ((rxflags & 0xF) == 0xD)
                {
                    if (!(rxfilter & (1<<7))) return;
                }
                else if ((rxflags & 0xF) != 0xE)
                {
                    if (!(rxfilter & (1<<1))) return;
                }
                break;

            case 0x2:
                if ((rxflags & 0xF) != 0xC)
                {
                    if (!(rxfilter & (1<<2))) return;
                }
                break;

            case 0x3:
                if (!(rxfilter & (1<<3))) return;
                break;

            case 0x4: break;

            case 0x5:
                if ((rxflags & 0xF) == 0xF)
                {
                    if (!(rxfilter & (1<<8))) return;
                }
                else
                {
                    if (!(rxfilter & (1<<4))) return;
                }
                break;

            case 0x6:
                if (!(rxfilter & (1<<5))) return;
                break;

            case 0x7:
                if (!(rxfilter & (1<<6))) return;
                break;

            default:
                return;
            }
        }
        break;
    }

    if (!cmd_dupe)
    {
        // build the RX header

        u16 headeraddr = IOPORT(W_RXBufWriteCursor) << 1;
        *(u16*)&RAM[headeraddr] = rxflags;
        IncrementRXAddr(headeraddr);
        *(u16*)&RAM[headeraddr] = 0x0040; // ???
        IncrementRXAddr(headeraddr, 4);
        *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[6]; // TX rate
        IncrementRXAddr(headeraddr);
        *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[8]; // frame length
        IncrementRXAddr(headeraddr);
        *(u16*)&RAM[headeraddr] = 0x4080; // RSSI

        // signal successful reception

        u16 addr = IOPORT(W_RXTXAddr) << 1;
        if (addr & 0x2) IncrementRXAddr(addr);
        IOPORT(W_RXBufWriteCursor) = (addr & ~0x3) >> 1;

        SetIRQ(0);
    }

    if ((rxflags & 0x800F) == 0x800C)
    {
        // reply to CMD frames

        u16 clientmask = *(u16*)&RXBuffer[0xC + 26];
        if (IOPORT(W_AIDLow) && (clientmask & (1 << IOPORT(W_AIDLow))))
        {
            SendMPReply(*(u16*)&RXBuffer[0xC + 24], clientmask);
        }
        else
        {
            // send a blank
            // this is just so the host can have something to receive, instead of hitting a timeout
            // in the case this client wasn't ready to send a reply
            // TODO: also send this if we have RX disabled

            Platform::MP_SendReply(nullptr, 0, USTimestamp, 0);
        }
    }
    else if ((rxflags & 0x800F) == 0x8001)
    {
        // when receiving a beacon with the right BSSID, the beacon's timestamp
        // is copied to USCOUNTER

        u32 len = *(u16*)&RXBuffer[8];
        u16 txrate = *(u16*)&RXBuffer[6];
        len *= ((txrate==0x14) ? 4 : 8);
        len -= 76; // CHECKME: is this offset fixed?

        u64 timestamp = *(u64*)&RXBuffer[12 + 24];
        timestamp += (u64)len;

        USCounter = timestamp;
    }
}

void MPClientReplyRX(int client)
{
    if (IOPORT(W_PowerState) & 0x0300)
        return;

    if (!(IOPORT(W_RXCnt) & 0x8000))
        return;

    if (IOPORT(W_RXBufBegin) == IOPORT(W_RXBufEnd))
        return;

    int framelen;
    u8 txrate;

    u8* reply = &MPClientReplies[(client-1)*1024];
    framelen = *(u16*)&reply[10];

    txrate = reply[8];

    // TODO: what are the maximum crop values?
    u16 framectl = *(u16*)&reply[12];
    if (framectl & (1<<14))
    {
        framelen -= (IOPORT(W_RXLenCrop) >> 7) & 0x1FE;
        if (framelen > 24) memmove(&RXBuffer[12+24], &RXBuffer[12+28], framelen);
    }
    else
        framelen -= (IOPORT(W_RXLenCrop) << 1) & 0x1FE;

    if (framelen < 0) framelen = 0;

    // TODO rework RX system so we don't need this (by reading directly into MPClientReplies)
    memcpy(RXBuffer, reply, 12+framelen);

    *(u16*)&RXBuffer[6] = txrate;
    *(u16*)&RXBuffer[8] = framelen;

    RXTimestamp = 0;
    StartRX();
}

bool CheckRX(int type) // 0=regular 1=MP replies 2=MP host frames
{
    if (IOPORT(W_PowerState) & 0x0300)
        return false;

    if (!(IOPORT(W_RXCnt) & 0x8000))
        return false;

    if (IOPORT(W_RXBufBegin) == IOPORT(W_RXBufEnd))
        return false;

    int rxlen;
    int framelen;
    u16 framectl;
    u8 txrate;
    u64 timestamp;

    for (;;)
    {
        timestamp = 0;

        if (type == 0)
        {
            rxlen = Platform::MP_RecvPacket(RXBuffer, &timestamp);
            if ((rxlen <= 0) && (!IsMP))
                rxlen = WifiAP::RecvPacket(RXBuffer);
        }
        else
        {
            rxlen = Platform::MP_RecvHostPacket(RXBuffer, &timestamp);
            if (rxlen < 0)
            {
                // host is gone
                // TODO: make this more resilient
                IsMP = false;
                IsMPClient = false;
            }
        }

        if (rxlen <= 0) return false;
        if (rxlen < 12+24) continue;

        framelen = *(u16*)&RXBuffer[10];
        if (framelen != rxlen-12)
        {
            Log(LogLevel::Error, "bad frame length %d/%d\n", framelen, rxlen-12);
            continue;
        }

        framectl = *(u16*)&RXBuffer[12+0];
        txrate = RXBuffer[8];

        // TODO: what are the maximum crop values?
        if (framectl & (1<<14))
        {
            framelen -= (IOPORT(W_RXLenCrop) >> 7) & 0x1FE;
            if (framelen > 24) memmove(&RXBuffer[12+24], &RXBuffer[12+28], framelen);
        }
        else
            framelen -= (IOPORT(W_RXLenCrop) << 1) & 0x1FE;

        if (framelen < 0) framelen = 0;

        break;
    }

    WIFI_LOG("wifi: received packet FC:%04X SN:%04X CL:%04X RXT:%d CMT:%d\n",
             framectl, *(u16*)&RXBuffer[12+4+6+6+6], *(u16*)&RXBuffer[12+4+6+6+6+2+2], framelen*4, IOPORT(W_CmdReplyTime));

    *(u16*)&RXBuffer[6] = txrate;
    *(u16*)&RXBuffer[8] = framelen;

    bool macgood = (RXBuffer[12 + 4] & 0x01) || MACEqual(&RXBuffer[12 + 4], (u8*)&IOPORT(W_MACAddr0));

    if (((framectl & 0x00FF) == 0x0010) && timestamp && macgood)
    {
        // if receiving an association response: get the sync value from the host

        u16 aid = *(u16*)&RXBuffer[12+24+4];

        if (aid)
        {
            Log(LogLevel::Debug, "[CLIENT %01X] host sync=%016llX\n", aid&0xF, timestamp);

            IsMP = true;
            IsMPClient = true;
            USTimestamp = timestamp;
            NextSync = RXTimestamp + (framelen * (txrate==0x14 ? 4:8));
        }

        RXTimestamp = 0;
        StartRX();
    }
    else if (((framectl & 0x00FF) == 0x00C0) && timestamp && macgood && IsMPClient)
    {
        IsMP = false;
        IsMPClient = false;
        NextSync = 0;

        RXTimestamp = 0;
        StartRX();
    }
    else if (macgood && IsMPClient)
    {
        // if we are being a MP client, we need to delay this frame until we reach the
        // timestamp it came with
        // we also need to determine how far we can run after having received this frame

        RXTimestamp = timestamp;
        //if (RXTimestamp < USTimestamp) printf("CRAP!! %04X %016llX %016llX\n", framectl, RXTimestamp, USTimestamp);
        if (RXTimestamp < USTimestamp) RXTimestamp = USTimestamp;
        NextSync = RXTimestamp + (framelen * (txrate==0x14 ? 4:8));

        if (MACEqual(&RXBuffer[12 + 4], MPCmdMAC))
        {
            u16 clienttime = *(u16*)&RXBuffer[12+24];
            u16 clientmask = *(u16*)&RXBuffer[12+26];

            // include the MP reply time window
            NextSync += 112 + ((clienttime + 10) * NumClients(clientmask));
        }
        else if (MACEqual(&RXBuffer[12 + 4], MPAckMAC))
        {
            u32 runahead = *(u32*)&RXBuffer[0];

            NextSync += runahead;
        }
    }
    else
    {
        // otherwise, just start receiving this frame now

        RXTimestamp = 0;
        StartRX();
    }

    return true;
}


void MSTimer()
{
    if (IOPORT(W_USCompareCnt))
    {
        if ((USCounter & ~0x3FF) == USCompare)
        {
            BlockBeaconIRQ14 = false;
            SetIRQ14(0);
        }
    }

    IOPORT(W_BeaconCount1)--;
    if (IOPORT(W_BeaconCount1) == 0)
    {
        SetIRQ14(1);
    }

    if (IOPORT(W_BeaconCount2) != 0)
    {
        IOPORT(W_BeaconCount2)--;
        if (IOPORT(W_BeaconCount2) == 0) SetIRQ13();
    }
}

void USTimer(u32 param)
{
    USTimestamp += kTimerInterval;

    if (IsMPClient && (!ComStatus))
    {
        if (RXTimestamp && (USTimestamp >= RXTimestamp))
        {
            RXTimestamp = 0;
            StartRX();
        }

        if (USTimestamp >= NextSync)
        {
            // TODO: not do this every tick if it fails to receive a frame!
            CheckRX(2);
        }
    }

    if (!(USTimestamp & 0x3FF & kTimeCheckMask))
        WifiAP::MSTimer();

    bool switchOffPowerSaving = false;
    if (USUntilPowerOn < 0)
    {
        USUntilPowerOn += kTimerInterval;

        switchOffPowerSaving = (USUntilPowerOn >= 0) && (IOPORT(W_PowerUnk) & 0x0001 || ForcePowerOn);
    }
    if ((USUntilPowerOn >= 0) && (IOPORT(W_PowerState) & 0x0002 || switchOffPowerSaving))
    {
        IOPORT(W_PowerState) = 0;
        IOPORT(W_RFPins) = 1;
        IOPORT(W_RFPins) = 0x0084;
        SetIRQ(11);
    }

    if (IOPORT(W_USCountCnt))
    {
        USCounter += kTimerInterval;
        u32 uspart = (USCounter & 0x3FF);

        if (IOPORT(W_USCompareCnt))
        {
            u32 beaconus = (IOPORT(W_BeaconCount1) << 10) | (0x3FF - uspart);
            if ((beaconus & kTimeCheckMask) == (IOPORT(W_PreBeacon) & kTimeCheckMask))
                SetIRQ15();
        }

        if (!(uspart & kTimeCheckMask))
            MSTimer();
    }

    if (IOPORT(W_CmdCountCnt) & 0x0001)
    {
        if (CmdCounter > 0)
        {
            if (CmdCounter < kTimerInterval)
                CmdCounter = 0;
            else
                CmdCounter -= kTimerInterval;
        }
    }

    if (IOPORT(W_ContentFree) != 0)
    {
        if (IOPORT(W_ContentFree) < kTimerInterval)
            IOPORT(W_ContentFree) = 0;
        else
            IOPORT(W_ContentFree) -= kTimerInterval;
    }

    if (ComStatus == 0)
    {
        u16 txbusy = IOPORT(W_TXBusy);
        if (txbusy)
        {
            if (IOPORT(W_PowerState) & 0x0300)
            {
                ComStatus = 0;
                TXCurSlot = -1;
            }
            else
            {
                ComStatus = 0x2;
                if      (txbusy & 0x0080) TXCurSlot = 5;
                else if (txbusy & 0x0010) TXCurSlot = 4;
                else if (txbusy & 0x0008) TXCurSlot = 3;
                else if (txbusy & 0x0004) TXCurSlot = 2;
                else if (txbusy & 0x0002) TXCurSlot = 1;
                else if (txbusy & 0x0001) TXCurSlot = 0;
            }
        }
        else
        {
            if ((!IsMPClient) || (USTimestamp > NextSync))
            {
                if ((!(RXCounter & 0x1FF & kTimeCheckMask)) && (!ComStatus))
                {
                    CheckRX(0);
                }
            }

            RXCounter += kTimerInterval;
        }
    }

    if (ComStatus & 0x2)
    {
        bool finished = ProcessTX(&TXSlots[TXCurSlot], TXCurSlot);
        if (finished)
        {
            if (IOPORT(W_PowerState) & 0x0300)
            {
                IOPORT(W_TXBusy) = 0;
                SetStatus(9);
            }

            // transfer finished, see if there's another slot to do
            // checkme: priority order of beacon/reply
            // TODO: for CMD, check CMDCOUNT
            u16 txbusy = IOPORT(W_TXBusy);
            if      (txbusy & 0x0080) TXCurSlot = 5;
            else if (txbusy & 0x0010) TXCurSlot = 4;
            else if (txbusy & 0x0008) TXCurSlot = 3;
            else if (txbusy & 0x0004) TXCurSlot = 2;
            else if (txbusy & 0x0002) TXCurSlot = 1;
            else if (txbusy & 0x0001) TXCurSlot = 0;
            else
            {
                TXCurSlot = -1;
                ComStatus = 0;
                RXCounter = 0;
            }
        }
    }
    if (ComStatus & 0x1)
    {
        RXTime -= kTimerInterval;
        if (!(RXTime & RXHalfwordTimeMask))
        {
            u16 addr = IOPORT(W_RXTXAddr) << 1;
            if (addr < 0x1FFF) *(u16*)&RAM[addr] = *(u16*)&RXBuffer[RXBufferPtr];

            IncrementRXAddr(addr);
            IOPORT(W_RXTXAddr) = addr >> 1;
            RXBufferPtr += 2;

            if (RXTime <= 0) // finished receiving
            {
                FinishRX();
            }
            else if (addr == (IOPORT(W_RXBufReadCursor) << 1))
            {
                // TODO: properly check the crossing of the read cursor
                // (for example, if it is outside of the RX buffer)

                Log(LogLevel::Debug, "wifi: RX buffer full (buf=%04X/%04X rd=%04X wr=%04X rxtx=%04X power=%04X com=%d rxcnt=%04X filter=%04X/%04X frame=%04X/%04X len=%d)\n",
                       (IOPORT(W_RXBufBegin)>>1)&0xFFF, (IOPORT(W_RXBufEnd)>>1)&0xFFF,
                       IOPORT(W_RXBufReadCursor), IOPORT(W_RXBufWriteCursor),
                       IOPORT(W_RXTXAddr), IOPORT(W_PowerState), ComStatus,
                       IOPORT(W_RXCnt), IOPORT(W_RXFilter), IOPORT(W_RXFilter2),
                       *(u16*)&RXBuffer[0], *(u16*)&RXBuffer[12], *(u16*)&RXBuffer[8]);
                RXTime = 0;
                SetStatus(1);
                if (TXCurSlot == 0xFFFFFFFF)
                {
                    ComStatus &= ~0x1;
                    RXCounter = 0;
                }
                // TODO: proper error management
                if ((!ComStatus) && (IOPORT(W_PowerState) & 0x0300))
                {
                    SetStatus(9);
                }
            }
        }
    }

    ScheduleTimer(false);
}


void RFTransfer_Type2()
{
    u32 id = (IOPORT(W_RFData2) >> 2) & 0x1F;

    if (IOPORT(W_RFData2) & 0x0080)
    {
        u32 data = RFRegs[id];
        IOPORT(W_RFData1) = data & 0xFFFF;
        IOPORT(W_RFData2) = (IOPORT(W_RFData2) & 0xFFFC) | ((data >> 16) & 0x3);
    }
    else
    {
        u32 data = IOPORT(W_RFData1) | ((IOPORT(W_RFData2) & 0x0003) << 16);
        RFRegs[id] = data;
    }
}

void RFTransfer_Type3()
{
    u32 id = (IOPORT(W_RFData1) >> 8) & 0x3F;

    u32 cmd = IOPORT(W_RFData2) & 0xF;
    if (cmd == 6)
    {
        IOPORT(W_RFData1) = (IOPORT(W_RFData1) & 0xFF00) | (RFRegs[id] & 0xFF);
    }
    else if (cmd == 5)
    {
        u32 data = IOPORT(W_RFData1) & 0xFF;
        RFRegs[id] = data;
    }
}


u16 Read(u32 addr)
{
    if (addr >= 0x04810000)
        return 0;

    addr &= 0x7FFE;

    if (addr >= 0x4000 && addr < 0x6000)
    {
        return *(u16*)&RAM[addr & 0x1FFE];
    }
    if (addr >= 0x2000 && addr < 0x4000)
        return 0xFFFF;

    bool activeread = (addr < 0x1000);

    switch (addr)
    {
    case W_Random: // random generator. not accurate
        Random = (Random & 0x1) ^ (((Random & 0x3FF) << 1) | (Random >> 10));
        return Random;

    case W_Preamble:
        return IOPORT(W_Preamble) & 0x0003;

    case W_USCount0: return (u16)(USCounter & 0xFFFF);
    case W_USCount1: return (u16)((USCounter >> 16) & 0xFFFF);
    case W_USCount2: return (u16)((USCounter >> 32) & 0xFFFF);
    case W_USCount3: return (u16)(USCounter >> 48);

    case W_USCompare0: return (u16)(USCompare & 0xFFFF);
    case W_USCompare1: return (u16)((USCompare >> 16) & 0xFFFF);
    case W_USCompare2: return (u16)((USCompare >> 32) & 0xFFFF);
    case W_USCompare3: return (u16)(USCompare >> 48);

    case W_CmdCount: return (CmdCounter + 9) / 10;

    case W_BBRead:
        if ((IOPORT(W_BBCnt) & 0xF000) != 0x6000)
        {
            Log(LogLevel::Error, "WIFI: bad BB read, CNT=%04X\n", IOPORT(W_BBCnt));
            return 0;
        }
        return BBRegs[IOPORT(W_BBCnt) & 0xFF];

    case W_BBBusy:
        return 0; // TODO eventually (BB busy flag)
    case W_RFBusy:
        return 0; // TODO eventually (RF busy flag)

    case W_RXBufDataRead:
        if (activeread)
        {
            u32 rdaddr = IOPORT(W_RXBufReadAddr);
            u16 ret = *(u16*)&RAM[rdaddr];

            rdaddr += 2;
            if (rdaddr == (IOPORT(W_RXBufEnd) & 0x1FFE))
                rdaddr = (IOPORT(W_RXBufBegin) & 0x1FFE);
            if (rdaddr == IOPORT(W_RXBufGapAddr))
            {
                rdaddr += (IOPORT(W_RXBufGapSize) << 1);
                if (rdaddr >= (IOPORT(W_RXBufEnd) & 0x1FFE))
                    rdaddr = rdaddr + (IOPORT(W_RXBufBegin) & 0x1FFE) - (IOPORT(W_RXBufEnd) & 0x1FFE);

                if (IOPORT(0x000) == 0xC340)
                    IOPORT(W_RXBufGapSize) = 0;
            }

            IOPORT(W_RXBufReadAddr) = rdaddr & 0x1FFE;
            IOPORT(W_RXBufDataRead) = ret;

            if (IOPORT(W_RXBufCount) > 0)
            {
                IOPORT(W_RXBufCount)--;
                if (IOPORT(W_RXBufCount) == 0)
                    SetIRQ(9);
            }
        }
        break;

    case W_TXBusy:
        return IOPORT(W_TXBusy) & 0x001F; // no bit for MP replies. odd

    case W_CMDStat0:
    case W_CMDStat1:
    case W_CMDStat2:
    case W_CMDStat3:
    case W_CMDStat4:
    case W_CMDStat5:
    case W_CMDStat6:
    case W_CMDStat7:
        {
            u16 ret = IOPORT(addr&0xFFF);
            IOPORT(addr&0xFFF) = 0;
            return ret;
        }
    }

    //printf("WIFI: read %08X\n", addr);
    return IOPORT(addr&0xFFF);
}

void Write(u32 addr, u16 val)
{
    if (addr >= 0x04810000)
        return;

    addr &= 0x7FFE;

    if (addr >= 0x4000 && addr < 0x6000)
    {
        *(u16*)&RAM[addr & 0x1FFE] = val;
        return;
    }
    if (addr >= 0x2000 && addr < 0x4000)
        return;

    switch (addr)
    {
    case W_ModeReset:
        {
            u16 oldval = IOPORT(W_ModeReset);

            if (!(oldval & 0x0001) && (val & 0x0001))
            {
                if (!(USUntilPowerOn < 0 && ForcePowerOn))
                {
                    //printf("mode reset power on %08x\n", NDS::ARM7->R[15]);
                    IOPORT(0x034) = 0x0002;
                    IOPORT(0x27C) = 0x0005;
                    // TODO: 02A2??

                    if (IOPORT(W_PowerUnk) & 0x0002)
                    {
                        USUntilPowerOn = -2048;
                        IOPORT(W_PowerState) |= 0x100;
                    }
                }
            }
            else if ((oldval & 0x0001) && !(val & 0x0001))
            {
                //printf("mode reset shutdown %08x\n", NDS::ARM7->R[15]);
                IOPORT(0x27C) = 0x000A;
                PowerDown();
            }

            if (val & 0x2000)
            {
                IOPORT(W_RXBufWriteAddr) = 0;
                IOPORT(W_CmdTotalTime) = 0;
                IOPORT(W_CmdReplyTime) = 0;
                IOPORT(0x1A4) = 0;
                IOPORT(0x278) = 0x000F;
                // TODO: other ports??
            }
            if (val & 0x4000)
            {
                IOPORT(W_ModeWEP) = 0;
                IOPORT(W_TXStatCnt) = 0;
                IOPORT(0x00A) = 0;
                IOPORT(W_MACAddr0) = 0;
                IOPORT(W_MACAddr1) = 0;
                IOPORT(W_MACAddr2) = 0;
                IOPORT(W_BSSID0) = 0;
                IOPORT(W_BSSID1) = 0;
                IOPORT(W_BSSID2) = 0;
                IOPORT(W_AIDLow) = 0;
                IOPORT(W_AIDFull) = 0;
                IOPORT(W_TXRetryLimit) = 0x0707;
                IOPORT(0x02E) = 0;
                IOPORT(W_RXBufBegin) = 0x4000;
                IOPORT(W_RXBufEnd) = 0x4800;
                IOPORT(W_TXBeaconTIM) = 0;
                IOPORT(W_Preamble) = 0x0001;
                IOPORT(W_RXFilter) = 0x0401;
                IOPORT(0x0D4) = 0x0001;
                IOPORT(W_RXFilter2) = 0x0008;
                IOPORT(0x0EC) = 0x3F03;
                IOPORT(W_TXHeaderCnt) = 0;
                IOPORT(0x198) = 0;
                IOPORT(0x1A2) = 0x0001;
                IOPORT(0x224) = 0x0003;
                IOPORT(0x230) = 0x0047;
            }
        }
        break;

    case W_ModeWEP:
        val &= 0x007F;
        //printf("writing mode web %x\n", val);
        if ((val & 0x7) == 1)
            IOPORT(W_PowerUnk) |= 0x0002;
        if ((val & 0x7) == 2)
            IOPORT(W_PowerUnk) = 0x0003;
        break;

    case W_IF:
        IOPORT(W_IF) &= ~val;
        return;
    case W_IFSet:
        IOPORT(W_IF) |= (val & 0xFBFF);
        Log(LogLevel::Debug, "wifi: force-setting IF %04X\n", val);
        return;

    case W_AIDLow:
        IOPORT(W_AIDLow) = val & 0x000F;
        return;
    case W_AIDFull:
        IOPORT(W_AIDFull) = val & 0x07FF;
        return;

    case W_PowerState:
        //printf("writing power state %x %08x\n", val, NDS::ARM7->R[15]);
        IOPORT(W_PowerState) |= val & 0x0002;

        if (IOPORT(W_ModeReset) & 0x0001 && IOPORT(W_PowerState) & 0x0002)
        {
            /*if (IOPORT(W_PowerState) & 0x100)
            {
                AlwaysPowerOn = true;
                USUntilPowerOn = -1;
            }
            else */
            if (IOPORT(W_PowerForce) == 1)
            {
                //printf("power on\n");
                IOPORT(W_PowerState) |= 0x100;
                USUntilPowerOn = -2048;
                ForcePowerOn = false;
            }
        }
        return;
    case W_PowerForce:
        //if ((val&0x8001)==0x8000) printf("WIFI: forcing power %04X\n", val);

        val &= 0x8001;
        //printf("writing power force %x %08x\n", val, NDS::ARM7->R[15]);
        if (val == 0x8001)
        {
            //printf("force power off\n");
            IOPORT(0x034) = 0x0002;
            IOPORT(W_PowerState) = 0x0200;
            IOPORT(W_TXReqRead) = 0;
            PowerDown();
        }
        if (val == 1 && IOPORT(W_PowerState) & 0x0002)
        {
            //printf("power on\n");
            IOPORT(W_PowerState) |= 0x100;
            USUntilPowerOn = -2048;
            ForcePowerOn = false;
        }
        if (val == 0x8000)
        {
            //printf("force power on\n");
            IOPORT(W_PowerState) |= 0x100;
            USUntilPowerOn = -2048;
            ForcePowerOn = true;
        }
        break;
    case W_PowerUS:
        IOPORT(W_PowerUS) = val & 0x0003;
        UpdatePowerOn();
        return;
    case W_PowerUnk:
        val &= 0x0003;
        //printf("writing power unk %x\n", val);
        if ((IOPORT(W_ModeWEP) & 0x7) == 1)
            val |= 2;
        else if ((IOPORT(W_ModeWEP) & 0x7) == 2)
            val = 3;
        break;

    case W_USCountCnt: val &= 0x0001; break;
    case W_USCompareCnt:
        if (val & 0x0002) SetIRQ14(2);
        val &= 0x0001;
        break;

    case W_USCount0: USCounter = (USCounter & 0xFFFFFFFFFFFF0000) | (u64)val; return;
    case W_USCount1: USCounter = (USCounter & 0xFFFFFFFF0000FFFF) | ((u64)val << 16); return;
    case W_USCount2: USCounter = (USCounter & 0xFFFF0000FFFFFFFF) | ((u64)val << 32); return;
    case W_USCount3: USCounter = (USCounter & 0x0000FFFFFFFFFFFF) | ((u64)val << 48); return;

    case W_USCompare0:
        USCompare = (USCompare & 0xFFFFFFFFFFFF0000) | (u64)(val & 0xFC00);
        if (val & 0x0001) BlockBeaconIRQ14 = true;
        return;
    case W_USCompare1: USCompare = (USCompare & 0xFFFFFFFF0000FFFF) | ((u64)val << 16); return;
    case W_USCompare2: USCompare = (USCompare & 0xFFFF0000FFFFFFFF) | ((u64)val << 32); return;
    case W_USCompare3: USCompare = (USCompare & 0x0000FFFFFFFFFFFF) | ((u64)val << 48); return;

    case W_CmdCount: CmdCounter = val * 10; return;

    case W_BBCnt:
        IOPORT(W_BBCnt) = val;
        if ((IOPORT(W_BBCnt) & 0xF000) == 0x5000)
        {
            u32 regid = IOPORT(W_BBCnt) & 0xFF;
            if (!BBRegsRO[regid])
                BBRegs[regid] = IOPORT(W_BBWrite) & 0xFF;
        }
        return;

    case W_RFData2:
        IOPORT(W_RFData2) = val;
        if (RFVersion == 3) RFTransfer_Type3();
        else                RFTransfer_Type2();
        return;
    case W_RFCnt:
        val &= 0x413F;
        break;


    case W_RXCnt:
        if (val & 0x0001)
        {
            IOPORT(W_RXBufWriteCursor) = IOPORT(W_RXBufWriteAddr);
        }
        if (val & 0x0080)
        {
            IOPORT(W_TXSlotReply2) = IOPORT(W_TXSlotReply1);
            IOPORT(W_TXSlotReply1) = 0;
        }
        if (val & 0x8000)
        {
            FireTX();
        }
        val &= 0xFF0E;
        if (val & 0x7FFF) Log(LogLevel::Warn, "wifi: unknown RXCNT bits set %04X\n", val);
        break;

    case W_RXBufDataRead:
        Log(LogLevel::Warn, "wifi: writing to RXBUF_DATA_READ. wat\n");
        if (IOPORT(W_RXBufCount) > 0)
        {
            IOPORT(W_RXBufCount)--;
            if (IOPORT(W_RXBufCount) == 0)
                SetIRQ(9);
        }
        return;

    case W_RXBufReadAddr:
    case W_RXBufGapAddr:
        val &= 0x1FFE;
        break;
    case W_RXBufGapSize:
    case W_RXBufCount:
    case W_RXBufWriteAddr:
    case W_RXBufReadCursor:
        val &= 0x0FFF;
        break;


    case W_TXReqReset:
        IOPORT(W_TXReqRead) &= ~val;
        return;
    case W_TXReqSet:
        IOPORT(W_TXReqRead) |= val;
        FireTX();
        return;

    case W_TXSlotReset:
        if (val & 0x0001) IOPORT(W_TXSlotLoc1) &= 0x7FFF;
        if (val & 0x0002) IOPORT(W_TXSlotCmd) &= 0x7FFF;
        if (val & 0x0004) IOPORT(W_TXSlotLoc2) &= 0x7FFF;
        if (val & 0x0008) IOPORT(W_TXSlotLoc3) &= 0x7FFF;
        // checkme: any bits affecting the beacon slot?
        if (val & 0x0040) IOPORT(W_TXSlotReply2) &= 0x7FFF;
        if (val & 0x0080) IOPORT(W_TXSlotReply1) &= 0x7FFF;
        if ((val & 0xFF30) && (val != 0xFFFF)) Log(LogLevel::Warn, "unusual TXSLOTRESET %04X\n", val);
        val = 0; // checkme (write-only port)
        break;

    case W_TXBufDataWrite:
        {
            u32 wraddr = IOPORT(W_TXBufWriteAddr);
            *(u16*)&RAM[wraddr] = val;

            wraddr += 2;
            if (wraddr == IOPORT(W_TXBufGapAddr))
                wraddr += (IOPORT(W_TXBufGapSize) << 1);

            //if (IOPORT(0x000) == 0xC340)
            //    IOPORT(W_TXBufGapSize) = 0;

            IOPORT(W_TXBufWriteAddr) = wraddr & 0x1FFE;

            if (IOPORT(W_TXBufCount) > 0)
            {
                IOPORT(W_TXBufCount)--;
                if (IOPORT(W_TXBufCount) == 0)
                    SetIRQ(8);
            }
        }
        return;

    case W_TXBufWriteAddr:
    case W_TXBufGapAddr:
        val &= 0x1FFE;
        break;
    case W_TXBufGapSize:
    case W_TXBufCount:
        val &= 0x0FFF;
        break;

    case W_TXSlotBeacon:
        IsMP = (val & 0x8000) != 0;
        break;

    case W_TXSlotCmd:
        if (CmdCounter == 0)
            val = (val & 0x7FFF) | (IOPORT(W_TXSlotCmd) & 0x8000);
        // fall-through
    case W_TXSlotLoc1:
    case W_TXSlotLoc2:
    case W_TXSlotLoc3:
        // checkme: is it possible to cancel a queued transfer that hasn't started yet
        // by clearing bit15 here?
        IOPORT(addr&0xFFF) = val;
        FireTX();
        return;

    case 0x228:
    case 0x244:
        //printf("wifi: write port%03X %04X\n", addr, val);
        break;

    // read-only ports
    case 0x000:
    case 0x044:
    case 0x054:
    case 0x098:
    case 0x0B0:
    case 0x0B6:
    case 0x0B8:
    case 0x15C:
    case 0x15E:
    case 0x180:
    case 0x19C:
    case 0x1A8:
    case 0x1AC:
    case 0x1C4:
    case 0x210:
    case 0x214:
    case 0x268:
        return;

    default:
        //printf("WIFI unk: write %08X %04X\n", addr, val);
        break;
    }

    IOPORT(addr&0xFFF) = val;
}


u8* GetMAC()
{
    return (u8*)&IOPORT(W_MACAddr0);
}

u8* GetBSSID()
{
    return (u8*)&IOPORT(W_BSSID0);
}

}
