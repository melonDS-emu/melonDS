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
#include "ARM.h"
#include "GPU.h"


namespace Wifi
{

//#define WIFI_LOG printf
#define WIFI_LOG(...) {}

#define PRINT_MAC(pf, mac) printf("%s: %02X:%02X:%02X:%02X:%02X:%02X\n", pf, (mac)[0], (mac)[1], (mac)[2], (mac)[3], (mac)[4], (mac)[5]);

u8 RAM[0x2000];
u16 IO[0x1000>>1];

#define IOPORT(x) IO[(x)>>1]
#define IOPORT8(x) ((u8*)IO)[x]

// destination MACs for MP frames
const u8 MPCmdMAC[6]   = {0x03, 0x09, 0xBF, 0x00, 0x00, 0x00};
const u8 MPReplyMAC[6] = {0x03, 0x09, 0xBF, 0x00, 0x00, 0x10};
const u8 MPAckMAC[6]   = {0x03, 0x09, 0xBF, 0x00, 0x00, 0x03};

bool Enabled;
bool PowerOn;

u16 Random;

// general, always-on microsecond counter
u64 USTimestamp;

u64 SchedTimestamp;
SchedEvent SchedList[Event_MAX];
u32 SchedListMask;
int NextEvent;

const u32 kRXCheckPeriod = 512;

u64 USCounter;
u64 USCompare;
bool BlockBeaconIRQ14;

u32 CmdCounter;

u16 BBCnt;
u8 BBWrite;
u8 BBRegs[0x100];
u8 BBRegsRO[0x100];

u8 RFVersion;
u16 RFCnt;
u16 RFData1;
u16 RFData2;
u32 RFRegs[0x40];

struct TXSlot
{
    bool Valid;
    u16 Addr;
    u16 Length;
    u8 Rate;
    u32 Preamble;
    u8 CurPhase;
    u32 CurPhaseTime;
    u32 HalfwordTimeMask;
};

TXSlot TXSlots[6];

u8 RXBuffer[2048];
u32 RXBufferPtr;
u64 RXTimestamp;
u32 RXTime;
u32 RXHalfwordTimeMask;
u16 RXEndAddr;

u64 RXStartTime;
u16 RXRate;
u16 RXCount;

u32 ComStatus; // 0=waiting for packets  1=receiving  2=sending
u32 TXCurSlot;
u32 RXCounter;

int MPReplyTimer;
u16 MPCurClient;
u16 MPClientMask, MPClientFail;

u8 MPClientReplies[15*1024];

bool MPInited;
bool LANInited;

int USUntilPowerOn;
bool ForcePowerOn;

// MULTIPLAYER SYNC APPARATUS
bool IsMPClient;
u64 TimeOffsetToHost;   // clienttime - hosttime
u64 NextSync;           // for clients: timestamp for next forced sync
u32 NextSyncType;
bool SyncBack;          // for clients: whether to send the host a sync once the sync is reached
const u64 kMaxRunahead = 4096;

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


void UpdateTimers();
void Reschedule();
void ScheduleEvent(u32 id, bool periodic, s32 delay, void (*func)(u32), u32 param);
void CancelEvent(u32 id);

void TRX_Next();

void TXPhase_Start(u32 num);
void TXPhase_Finish(u32 num);
void TXPhase_MP_RXReply(u32 nclient);
void TXPhase_MP_StartAck(u32 param);
void TXPhase_MP_FinishAck(u32 param);

void StartTX_Beacon();
void PeriodicRXCheck(u32 time);

void FinishRX(u32 param);
void MPClientSync(u32 param);


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

    RFVersion = SPI_Firmware::GetRFVersion();
    memset(RFRegs, 0, 4*0x40);

    u8 console = SPI_Firmware::GetConsoleType();
    if (console == 0xFF)
        IOPORT(0x000) = 0x1440;
    else if (console == 0x20)
        IOPORT(0x000) = 0xC340;
    else if (NDS::ConsoleType == 1 && console == 0x57)
        IOPORT(0x000) = 0xC340; // DSi has the modern DS-wifi variant
    else
    {
        printf("wifi: unknown console type %02X\n", console);
        IOPORT(0x000) = 0x1440;
    }

    memset(&IOPORT(0x018), 0xFF, 6);
    memset(&IOPORT(0x020), 0xFF, 6);

    // TODO: find out what the initial values are
    IOPORT(W_PowerUS) = 0x0001;

    USTimestamp = 0;
    RXTimestamp = 0;

    SchedTimestamp = 0;
    memset(SchedList, 0, sizeof(SchedList));
    SchedListMask = 0;
    NextEvent = -1;

    USCounter = 0;
    USCompare = 0;
    BlockBeaconIRQ14 = false;

    memset(TXSlots, 0, sizeof(TXSlots));
    ComStatus = 0;
    TXCurSlot = -1;
    RXCounter = 0;
    RXTimestamp = 0;

    MPReplyTimer = 0;
    MPClientMask = 0;
    MPClientFail = 0;

    CmdCounter = 0;

    IsMPClient = false;
    TimeOffsetToHost = 0;
    NextSync = 0;
    SyncBack = false;

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

    file->Var16(&Random);

    file->VarArray(BBRegs, 0x100);
    file->VarArray(BBRegsRO, 0x100);

    file->Var8(&RFVersion);
    file->VarArray(RFRegs, 4*0x40);

    file->Var64(&USCounter);
    file->Var64(&USCompare);
    file->Bool32(&BlockBeaconIRQ14);

    file->Var32(&ComStatus);
    file->Var32(&TXCurSlot);
    file->Var32(&RXCounter);

    file->Var32((u32*)&MPReplyTimer);
    //file->Var32((u32*)&MPNumReplies);

    file->Var32(&CmdCounter);
}


void UpdatePowerOn();

void SetPowerCnt(u32 val)
{
    Enabled = val & (1<<1);
    UpdatePowerOn();
}


// wifi scheduling
// NOTE: this is technically inaccurate
// in the DS, the wifi system is driven by its own 22MHz clock, not by the system clock
// but in melonDS, we do need to drive it from the system scheduler, so it stays in sync
// with the rest of the system

inline u64 USToSysCycles(u64 us)
{
    return (us * 33513982) / 1000000;
}

inline u64 SysCyclesToUS(u64 sys)
{
    return ((sys + 16) * 1000000) / 33513982;
}

void UpdateTimers()
{
    u64 oldts = SchedTimestamp;
    SchedTimestamp = SysCyclesToUS(NDS::GetSysClockCycles(0));
    u64 diff = SchedTimestamp - oldts;
    if (diff == 0) return;

    USTimestamp += diff;

    if (IOPORT(W_USCountCnt))
    {
        USCounter += diff;
    }

    if (IOPORT(W_ContentFree) <= diff)
        IOPORT(W_ContentFree) = 0;
    else
        IOPORT(W_ContentFree) -= diff;

    if (IOPORT(W_CmdCountCnt))
    {
        if (CmdCounter <= diff)
            CmdCounter = 0;
        else
            CmdCounter -= diff;
    }
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
        printf("WIFI: ON\n");

        u64 oldts = SchedTimestamp;
        SchedTimestamp = SysCyclesToUS(NDS::GetSysClockCycles(0));
        u64 diff = SchedTimestamp - oldts;

        for (int i = 0; i < Event_MAX; i++)
        {
            if (!(SchedListMask & (1<<i)))
                continue;

            if (SchedList[i].Timestamp >= oldts)
            {
                SchedList[i].Timestamp += diff;
            }
        }

        ScheduleEvent(Event_RXCheck, false, kRXCheckPeriod, PeriodicRXCheck, 0);

        Platform::MP_Begin();
    }
    else
    {
        printf("WIFI: OFF\n");

        CancelEvent(Event_RXCheck);

        UpdateTimers();
        NDS::CancelEvent(NDS::Event_Wifi);

        Platform::MP_End();
    }
}

void RunEvents(u32 param)
{
    UpdateTimers();

    NextEvent = -1;

    u32 mask = SchedListMask;
    for (int i = 0; i < Event_MAX; i++)
    {
        if (!mask) break;
        if (mask & 0x1)
        {
            if (SchedList[i].Timestamp <= SchedTimestamp)
            {
                SchedListMask &= ~(1<<i);
                SchedList[i].Func(SchedList[i].Param);
            }
        }

        mask >>= 1;
    }

    if (NextEvent == -1)
        Reschedule();
}

void Reschedule()
{
    u64 next = UINT64_MAX;
    NextEvent = -1;

    u32 mask = SchedListMask;
    for (int i = 0; i < Event_MAX; i++)
    {
        if (!mask) break;
        if (mask & 0x1)
        {
            if (SchedList[i].Timestamp < next)
            {
                next = SchedList[i].Timestamp;
                NextEvent = i;
            }
        }

        mask >>= 1;
    }

    NDS::CancelEvent(NDS::Event_Wifi);
    NDS::ScheduleEvent(NDS::Event_Wifi, USToSysCycles(next), RunEvents, 0);
}

void ScheduleEvent(u32 id, bool periodic, s32 delay, void (*func)(u32), u32 param)
{
    if (SchedListMask & (1<<id))
    {
        printf("wifi: !! EVENT %d ALREADY SCHEDULED\n", id);
        return;
    }

    SchedEvent* evt = &SchedList[id];

    if (periodic)
    {
        evt->Timestamp += delay;
    }
    else
    {
        UpdateTimers();
        evt->Timestamp = SchedTimestamp + delay;
    }

    evt->Func = func;
    evt->Param = param;

    SchedListMask |= (1<<id);

    Reschedule();
}

void CancelEvent(u32 id)
{
    SchedListMask &= ~(1<<id);
}

bool EventScheduled(u32 id)
{
    return (SchedListMask & (1<<id)) != 0;
}


void PowerDown();

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
        printf("wifi: weird forced IRQ14\n");

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

void SetIRQ15(u32 param)
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


bool MACEqual(u8* a, u8* b)
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

void StartTX_LocN(int nslot, int loc)
{
    TXSlot* slot = &TXSlots[nslot];

    if (IOPORT(W_TXSlotLoc1 + (loc*4)) & 0x7000)
        printf("wifi: unusual loc%d bits set %04X\n", loc, IOPORT(W_TXSlotLoc1 + (loc*4)));

    slot->Addr = (IOPORT(W_TXSlotLoc1 + (loc*4)) & 0x0FFF) << 1;
    slot->Length = *(u16*)&RAM[slot->Addr + 0xA] & 0x3FFF;

    u8 rate = RAM[slot->Addr + 0x8];
    if (rate == 0x14) slot->Rate = 2;
    else              slot->Rate = 1;

    slot->Preamble = PreambleLen(slot->Rate);
}

void StartTX_Cmd()
{
    TXSlot* slot = &TXSlots[1];

    // TODO: cancel the transfer if there isn't enough time left (check CMDCOUNT)

    if (IOPORT(W_TXSlotCmd) & 0x3000)
        printf("wifi: !! unusual TXSLOT_CMD bits set %04X\n", IOPORT(W_TXSlotCmd));

    slot->Addr = (IOPORT(W_TXSlotCmd) & 0x0FFF) << 1;
    slot->Length = *(u16*)&RAM[slot->Addr + 0xA] & 0x3FFF;

    u8 rate = RAM[slot->Addr + 0x8];
    if (rate == 0x14) slot->Rate = 2;
    else              slot->Rate = 1;

    slot->Preamble = PreambleLen(slot->Rate);
}

void StartTX_Beacon()
{
    TXSlot* slot = &TXSlots[4];

    slot->Addr = (IOPORT(W_TXSlotBeacon) & 0x0FFF) << 1;
    slot->Length = *(u16*)&RAM[slot->Addr + 0xA] & 0x3FFF;

    u8 rate = RAM[slot->Addr + 0x8];
    if (rate == 0x14) slot->Rate = 2;
    else              slot->Rate = 1;

    slot->Preamble = PreambleLen(slot->Rate);

    IOPORT(W_TXBusy) |= 0x0010;
}

// TODO eventually: there is a small delay to firing TX
void FireTX()
{
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
    }
    else if (txstart & 0x0004)
    {
        StartTX_LocN(2, 1);
    }
    else if (txstart & 0x0002)
    {
        StartTX_Cmd();
    }
    else if (txstart & 0x0001)
    {
        StartTX_LocN(0, 0);
    }

    if (!ComStatus)
    {
        TRX_Next();
    }
}

void SendMPDefaultReply()
{
    u8 reply[12 + 32];

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

    //if (RAM[slot->Addr+4] > 0)
    //    printf("REPLY RETRY COUNTER %d (%04X)\n", RAM[slot->Addr+4], IOPORT(W_TXSlotReply2));

    // this seems to be set upon IRQ0
    // TODO: how does it behave if the packet addr is changed before it gets sent? (maybe just not possible)
    if (slot->Valid)
    {
        //*(u16*)&RAM[slot->Addr + 0x4] = 0x0001;
        IncrementTXCount(slot);

        slot->CurPhase = 0;
        int txlen = Platform::MP_SendReply(&RAM[slot->Addr], 12 + slot->Length, USTimestamp, IOPORT(W_AIDLow));
        WIFI_LOG("wifi: sent %d/%d bytes of MP reply\n", txlen, 12 + slot->Length);
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

    slot->Preamble = 16 + ((clienttime + 10) * clientnum) + PreambleLen(slot->Rate);

    IOPORT(W_TXBusy) |= 0x0080;
}

void SendMPAck(u16 clientfail)
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
    *(u16*)&ack[0xC + 0x18] = 0x0033; // ???
    *(u16*)&ack[0xC + 0x1A] = clientfail;
    *(u32*)&ack[0xC + 0x1C] = 0;

    int txlen = Platform::MP_SendAck(ack, 12+32, USTimestamp);
    WIFI_LOG("wifi: sent %d/44 bytes of MP ack, %d %d\n", txlen, ComStatus, RXTime);
}

bool CheckRX(int type);
void MPClientReplyRX(int client);

bool ProcessTX(TXSlot* slot, int num)
{
    slot->CurPhaseTime--;
    if (slot->CurPhaseTime > 0)
    {
        if (slot->CurPhase == 1)
        {
            if (!(slot->CurPhaseTime & slot->HalfwordTimeMask))
                IOPORT(W_RXTXAddr)++;
        }
        else if (slot->CurPhase == 2)
        {
            MPReplyTimer--;
            if (MPReplyTimer == 0 && MPClientMask != 0)
            {
                int nclient = 1;
                while (!(MPClientMask & (1 << nclient))) nclient++;

                u32 curclient = 1 << nclient;

                /*if (CheckRX(1))
                {
                    // we received a reply, mark it as such
                    // TODO: is any received packet considered a good reply?
                    // hardware probably requires a specific frame-control and/or destination MAC

                    MPClientFail &= ~curclient;
                }
                else printf("REPLY %04X NOT RECEIVED\n");*/
                if (!(MPClientFail & curclient))
                    MPClientReplyRX(nclient);

                MPReplyTimer = 10 + IOPORT(W_CmdReplyTime);
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
            {
                // MP reply slot
                // setup needs to be done now as port 098 can get changed in the meantime

                SetStatus(8);

                //slot->Addr = (IOPORT(W_TXSlotReply2) & 0x0FFF) << 1;
                //slot->Length = *(u16*)&RAM[slot->Addr + 0xA] & 0x3FFF;

                /*u8 rate = RAM[slot->Addr + 0x8];
                if (rate == 0x14) slot->Rate = 2;
                else              slot->Rate = 1;*/

                // TODO: duration should be set by hardware
                // doesn't seem to be important
                //RAM[slot->Addr + 0xC + 2] = 0x00F0;
            }
            else
                SetStatus(3);

            u32 len = slot->Length;
            if (slot->Rate == 2)
            {
                len *= 4;
                slot->HalfwordTimeMask = 0x7;
            }
            else
            {
                len *= 8;
                slot->HalfwordTimeMask = 0xF;
            }

            slot->CurPhase = 1;
            slot->CurPhaseTime = len;

            u64 oldts;
            if (num == 4)
            {
                // beacon timestamp
                oldts = *(u64*)&RAM[slot->Addr + 0xC + 24];
                *(u64*)&RAM[slot->Addr + 0xC + 24] = USCounter;
            }

            u32 noseqno = 0;
            if (num == 1) noseqno = (IOPORT(W_TXSlotCmd) & 0x4000);

            if (!noseqno)
            {
                *(u16*)&RAM[slot->Addr + 0xC + 22] = IOPORT(W_TXSeqNo) << 4;
                IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;
            }

            if ((num != 5) && (RAM[slot->Addr+4] > 0))
                printf("SLOT %d RETRY COUNTER %d\n", RAM[slot->Addr+4]);

            // set TX addr
            IOPORT(W_RXTXAddr) = slot->Addr >> 1;

            if (num == 1)
            {
                // send
                int txlen = Platform::MP_SendCmd(&RAM[slot->Addr], 12 + slot->Length, USTimestamp);
                WIFI_LOG("wifi: sent %d/%d bytes of slot%d packet, addr=%04X, framectl=%04X, %04X %04X\n",
                         txlen, slot->Length+12, num, slot->Addr, *(u16*)&RAM[slot->Addr + 0xC],
                         *(u16*)&RAM[slot->Addr + 0x24], *(u16*)&RAM[slot->Addr + 0x26]);
            }
            else if (num == 5)
            {
                // send
                /*int txlen = Platform::MP_SendReply(&RAM[slot->Addr], 12 + slot->Length, USTimestamp, IOPORT(W_AIDLow));
                WIFI_LOG("wifi: sent %d/%d bytes of slot%d packet, addr=%04X, framectl=%04X, %04X %04X\n",
                         txlen, slot->Length+12, num, slot->Addr, *(u16*)&RAM[slot->Addr + 0xC],
                         *(u16*)&RAM[slot->Addr + 0x24], *(u16*)&RAM[slot->Addr + 0x26]);*/
            }
            else //if (num != 5)
            {
                // send
                int txlen = Platform::MP_SendPacket(&RAM[slot->Addr], 12 + slot->Length, USTimestamp);
                WIFI_LOG("wifi: sent %d/%d bytes of slot%d packet, addr=%04X, framectl=%04X, %04X %04X\n",
                         txlen, slot->Length+12, num, slot->Addr, *(u16*)&RAM[slot->Addr + 0xC],
                         *(u16*)&RAM[slot->Addr + 0x24], *(u16*)&RAM[slot->Addr + 0x26]);
            }

            // if the packet is being sent via LOC1..3, send it to the AP
            // any packet sent via CMD/REPLY/BEACON isn't going to have much use outside of local MP
            if (num == 0 || num == 2 || num == 3)
            {
                u16 framectl = *(u16*)&RAM[slot->Addr + 0xC];
                if ((framectl & 0x00FF) == 0x0010)
                {
                    u16 aid = *(u16*)&RAM[slot->Addr + 0xC + 24 + 4];
                    if (aid) printf("[HOST] syncing client %04X, sync=%016llX\n", aid, USTimestamp);
                }

                WifiAP::SendPacket(&RAM[slot->Addr], 12 + slot->Length);
            }

            if (num == 4)
            {
                *(u64*)&RAM[slot->Addr + 0xC + 24] = oldts;
            }
        }
        break;

    case 10: // preamble done (default empty MP reply)
        {
            SetIRQ(7);
            SetStatus(8);

            //SendMPDefaultReply();

            //slot->Addr = 0;
            //slot->Length = 28;
            slot->CurPhase = 4;
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

                u16 clientmask = *(u16*)&RAM[slot->Addr + 12 + 24 + 2];
                //MPNumReplies = NumClients(clientmask);
                MPReplyTimer = 16 + PreambleLen(slot->Rate);
                MPClientMask = clientmask;
                MPClientFail = clientmask;

                u16 res = Platform::MP_RecvReplies(MPClientReplies, USTimestamp, clientmask);
                MPClientFail &= ~res;

                // TODO: 112 likely includes the ack preamble, which needs adjusted
                // for long-preamble settings
                slot->CurPhase = 2;
                slot->CurPhaseTime = 112 + ((10 + IOPORT(W_CmdReplyTime)) * NumClients(clientmask));

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

    case 2: // MP host transfer done
        {
            SetIRQ(7);
            SetStatus(8);

            IOPORT(W_RXTXAddr) = 0xFC0;

            if (slot->Rate == 2) slot->CurPhaseTime = 32 * 4;
            else                 slot->CurPhaseTime = 32 * 8;

            ReportMPReplyErrors(MPClientFail);

            // send
            SendMPAck(MPClientFail);

            slot->CurPhase = 3;
        }
        break;

    case 3: // MP host ack transfer (reply wait done)
        {
            // checkme
            IOPORT(W_TXBusy) &= ~(1<<1);
            IOPORT(W_TXSlotCmd) &= 0x7FFF; // confirmed

            if (!MPClientFail)
                *(u16*)&RAM[slot->Addr] = 0x0001;
            else
                *(u16*)&RAM[slot->Addr] = 0x0005;

            // this is set to indicate which clients failed to reply
            *(u16*)&RAM[slot->Addr + 0x2] = MPClientFail;
            IncrementTXCount(slot);

            IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;

            if (IOPORT(W_TXStatCnt) & 0x2000)
            {
                IOPORT(W_TXStat) = 0x0B01;
                SetIRQ(1);
            }
            SetStatus(1);

            // TODO: retry the whole cycle if some clients failed to respond
            // AND if there is enough time left in CMDCOUNT
            // (games seem to always configure CMDCOUNT such that there is no time for retries)
            SetIRQ(12);

            FireTX();
        }
        return true;

    case 4: // MP default reply transfer finished
        {
            IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;

            IOPORT(W_TXBusy) &= ~0x80;
            SetStatus(1);
            FireTX();
        }
        return true;
    }

    return false;
}


void TXPhase_Start(u32 num)
{
    TXSlot* slot = &TXSlots[num];

    SetIRQ(7);

    if (num == 5)
    {
        SetStatus(8);

        if (!slot->Valid)
        {
            // if this is a default empty reply: we don't need to do all the setup

            u32 len = 28;
            if (slot->Rate == 2) len *= 4;
            else                 len *= 8;

            ScheduleEvent(Event_TX, true, len, TXPhase_Finish, num);
            return;
        }
    }
    else
        SetStatus(3);

    u64 oldts;
    if (num == 4)
    {
        // beacon timestamp
        oldts = *(u64*)&RAM[slot->Addr + 0xC + 24];
        *(u64*)&RAM[slot->Addr + 0xC + 24] = USCounter;
    }

    u32 noseqno = 0;
    if (num == 1) noseqno = (IOPORT(W_TXSlotCmd) & 0x4000);

    if (!noseqno)
    {
        *(u16*)&RAM[slot->Addr + 0xC + 22] = IOPORT(W_TXSeqNo) << 4;
        IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;
    }

    // TODO: inhibit seqno adjustment and set framectl.bit11 if txheader[04] is nonzero

    // set TX addr
    IOPORT(W_RXTXAddr) = slot->Addr >> 1;

    if (num == 1)
    {
        // send
        int txlen = Platform::MP_SendCmd(&RAM[slot->Addr], 12 + slot->Length, USTimestamp);
        WIFI_LOG("wifi: sent %d/%d bytes of CMD packet, addr=%04X, framectl=%04X, %04X %04X\n",
                 txlen, slot->Length+12, slot->Addr, *(u16*)&RAM[slot->Addr + 0xC],
                 *(u16*)&RAM[slot->Addr + 0x24], *(u16*)&RAM[slot->Addr + 0x26]);
    }
    else if (num == 5)
    {
        // MP reply frames are handled separately
    }
    else
    {
        // send
        int txlen = Platform::MP_SendPacket(&RAM[slot->Addr], 12 + slot->Length, USTimestamp);
        WIFI_LOG("wifi: sent %d/%d bytes of slot%d packet, addr=%04X, framectl=%04X\n",
                 txlen, slot->Length+12, num, slot->Addr, *(u16*)&RAM[slot->Addr + 0xC]);
    }

    // if the packet is being sent via LOC1..3, send it to the AP
    // any packet sent via CMD/REPLY/BEACON isn't going to have much use outside of local MP
    if (num == 0 || num == 2 || num == 3)
    {
        WifiAP::SendPacket(&RAM[slot->Addr], 12 + slot->Length);
    }

    if (num == 4)
    {
        *(u64*)&RAM[slot->Addr + 0xC + 24] = oldts;
    }

    u32 len = slot->Length;
    if (slot->Rate == 2) len *= 4;
    else                 len *= 8;

    ScheduleEvent(Event_TX, true, len, TXPhase_Finish, num);
}

void TXPhase_Finish(u32 num)
{
    TXSlot* slot = &TXSlots[num];

    if ((num == 5) && (!slot->Valid))
    {
        IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;

        IOPORT(W_TXBusy) &= ~0x80;
        TRX_Next();
        return;
    }

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

        u16 clientmask = *(u16*)&RAM[slot->Addr + 12 + 24 + 2];
        //MPNumReplies = NumClients(clientmask);
        //MPReplyTimer = 16 + PreambleLen(slot->Rate);
        MPClientMask = clientmask;
        MPClientFail = clientmask;

        u16 res = Platform::MP_RecvReplies(MPClientReplies, USTimestamp, clientmask);
        MPClientFail &= ~res;

        // TODO: 112 likely includes the ack preamble, which needs adjusted
        // for long-preamble settings
        /*slot->CurPhase = 2;
        slot->CurPhaseTime = 112 + ((10 + IOPORT(W_CmdReplyTime)) * NumClients(clientmask));
        aaaaaaaaaaaaaaaaaaaaaaa*/
        //printf("MP CMD SHITO TODO\n");
        // slot->Preamble = 16 + ((clienttime + 10) * clientnum) + PreambleLen(slot->Rate);
        // 16 + ((10 + IOPORT(W_CmdReplyTime)) * NumClients(clientmask)) + preamble(ack);
        if (MPClientMask)
        {
            int nclient = 1;
            while (!(MPClientMask & (1 << nclient))) nclient++;
            ScheduleEvent(Event_TX, true, 16+PreambleLen(slot->Rate), TXPhase_MP_RXReply, nclient);
        }
        else
        {
            ScheduleEvent(Event_TX, true, 16+PreambleLen(slot->Rate), TXPhase_MP_StartAck, 0);
        }

        return;
    }
    else if (num == 5)
    {
        if (IOPORT(W_TXStatCnt) & 0x1000)
        {
            IOPORT(W_TXStat) = 0x0401;
            SetIRQ(1);
        }

        IOPORT(W_TXBusy) &= ~0x80;
        TRX_Next();
        return;
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

    TRX_Next();
}

void TXPhase_MP_RXReply(u32 nclient)
{
    TXSlot* slot = &TXSlots[1];

    u32 curclient = 1 << nclient;

    /*if (CheckRX(1))
    {
        // we received a reply, mark it as such
        // TODO: is any received packet considered a good reply?
        // hardware probably requires a specific frame-control and/or destination MAC

        MPClientFail &= ~curclient;
    }
    else printf("REPLY %04X NOT RECEIVED\n");*/
    if (!(MPClientFail & curclient))
        MPClientReplyRX(nclient);

    //MPReplyTimer = 10 + IOPORT(W_CmdReplyTime);
    MPClientMask &= ~curclient;
    if (MPClientMask)
    {
        do { nclient++; } while (!(MPClientMask & (1 << nclient)));
        ScheduleEvent(Event_TX, true, 10+IOPORT(W_CmdReplyTime), TXPhase_MP_RXReply, nclient);
    }
    else
    {
        ScheduleEvent(Event_TX, true, 10+IOPORT(W_CmdReplyTime), TXPhase_MP_StartAck, 0);
    }
}

void TXPhase_MP_StartAck(u32 param)
{
    TXSlot* slot = &TXSlots[1];

    SetIRQ(7);
    SetStatus(8);

    IOPORT(W_RXTXAddr) = 0xFC0;

    ReportMPReplyErrors(MPClientFail);

    // send
    SendMPAck(MPClientFail);

    u32 len = 32;
    if (slot->Rate == 2) len *= 4;
    else                 len *= 8;

    ScheduleEvent(Event_TX, true, len, TXPhase_MP_FinishAck, 0);
}

void TXPhase_MP_FinishAck(u32 param)
{
    TXSlot* slot = &TXSlots[1];

    // checkme
    IOPORT(W_TXBusy) &= ~(1<<1);
    IOPORT(W_TXSlotCmd) &= 0x7FFF; // confirmed

    if (!MPClientFail)
        *(u16*)&RAM[slot->Addr] = 0x0001;
    else
        *(u16*)&RAM[slot->Addr] = 0x0005;

    // this is set to indicate which clients failed to reply
    *(u16*)&RAM[slot->Addr + 0x2] = MPClientFail;
    IncrementTXCount(slot);

    IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;

    if (IOPORT(W_TXStatCnt) & 0x2000)
    {
        IOPORT(W_TXStat) = 0x0B01;
        SetIRQ(1);
    }

    // TODO: retry the whole cycle if some clients failed to respond
    // AND if there is enough time left in CMDCOUNT
    // (games seem to always configure CMDCOUNT such that there is no time for retries)
    SetIRQ(12);

    TRX_Next();
}


void TRX_Next()
{
    if (IOPORT(W_PowerState) & 0x0300)
    {
        // if we are powering down, cancel anything
        IOPORT(W_TXBusy) = 0;
        SetStatus(9);
        return;
    }

    u16 txbusy = IOPORT(W_TXBusy);

    if (!(txbusy & 0x0080))
    {
        if ((!IsMPClient) || (USTimestamp > NextSync))
        {
            if (CheckRX(0))
                return;
        }
    }

    if (txbusy)
    {u32 dorp = ComStatus;
        ComStatus = 0x2;

        int slot = -1;
        if      (txbusy & 0x0080) slot = 5;
        else if (txbusy & 0x0010) slot = 4;
        else if (txbusy & 0x0008) slot = 3;
        else if (txbusy & 0x0004) slot = 2;
        else if (txbusy & 0x0002) slot = 1;
        else if (txbusy & 0x0001) slot = 0;

        ScheduleEvent(Event_TX, false, TXSlots[slot].Preamble, TXPhase_Start, slot);
        return;
    }

    ComStatus = 0;
    SetStatus(1);
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

void StartRX(u32 type)
{
    u16 framelen = *(u16*)&RXBuffer[8];
    u32 frametime = framelen;

    RXRate = *(u16*)&RXBuffer[6];
    if (RXRate == 0x14) frametime *= 4;
    else                frametime *= 8;

    u16 addr = IOPORT(W_RXBufWriteCursor) << 1;
    IncrementRXAddr(addr, 12);
    IOPORT(W_RXTXAddr) = addr >> 1;

    RXBufferPtr = 12;

    SetIRQ(6);
    SetStatus(6);
    ComStatus |= 0x1;

    RXStartTime = SchedTimestamp;
    RXCount = 0;

    ScheduleEvent(Event_RX, false, frametime, FinishRX, type);
}

void AdvanceRX()
{
    u32 ntime = (u32)(SchedTimestamp - RXStartTime);
    RXStartTime = SchedTimestamp;

    u32 ndata;
    if (RXRate == 0x14)
        ndata = ntime >> 3;
    else
        ndata = ntime >> 4;

    u16 framelen = *(u16*)&RXBuffer[8];
    if ((RXCount + ndata) > framelen)
        ndata = framelen - RXCount;

    u16 addr = IOPORT(W_RXTXAddr) << 1;
    for (u32 i = 0; i < ndata; i++)
    {
        if (addr == (IOPORT(W_RXBufReadCursor) << 1))
        {
            // TODO!!!
            printf("!!! RX BUFFER FULL\n");
            return;
        }

        *(u16*)&RAM[addr] = *(u16*)&RXBuffer[RXBufferPtr];

        IncrementRXAddr(addr);
        RXBufferPtr += 2;
    }
    IOPORT(W_RXTXAddr) = addr >> 1;

    RXCount += ndata;
}

void FinishRX(u32 type)
{
    AdvanceRX();

    u16 addr = IOPORT(W_RXTXAddr) << 1;
    if (addr & 0x2) IncrementRXAddr(addr);

    // copy the RX header
    u16 headeraddr = IOPORT(W_RXBufWriteCursor) << 1;
    *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[0]; IncrementRXAddr(headeraddr);
    *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[2]; IncrementRXAddr(headeraddr, 4);
    *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[6]; IncrementRXAddr(headeraddr);
    *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[8]; IncrementRXAddr(headeraddr);
    *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[10];

    IOPORT(W_RXBufWriteCursor) = (addr & ~0x3) >> 1;

    SetIRQ(0);

    WIFI_LOG("wifi: finished receiving packet %04X\n", *(u16*)&RXBuffer[12]);

    ComStatus &= ~0x1;
    //RXCounter = 0;

    if ((RXBuffer[0] & 0x0F) == 0x0C)
    {
        u16 clientmask = *(u16*)&RXBuffer[0xC + 26];
        if (IOPORT(W_AIDLow) && (RXBuffer[0xC + 4] & 0x01) && (clientmask & (1 << IOPORT(W_AIDLow))))
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
    else if ((*(u16*)&RXBuffer[0] & 0x800F) == 0x8001)
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

    if (type == 0) TRX_Next();
}

void MPClientReplyRX(int client)
{
    if (IOPORT(W_PowerState) & 0x0300)
        return;

    if (!(IOPORT(W_RXCnt) & 0x8000))
        return;

    if (IOPORT(W_RXBufBegin) == IOPORT(W_RXBufEnd))
        return;

    u16 framelen;
    u16 framectl;
    u8 txrate;
    bool bssidmatch;
    u16 rxflags;

    u8* reply = &MPClientReplies[(client-1)*1024];
    framelen = *(u16*)&reply[10];
    framelen -= 4;

    // TODO rework RX system so we don't need this (by reading directly into MPClientReplies)
    memcpy(RXBuffer, reply, 12+framelen);

    framectl = *(u16*)&RXBuffer[12+0];
    txrate = RXBuffer[8];

    u32 a_src, a_dst, a_bss;
    rxflags = 0x0010;
    switch (framectl & 0x000C)
    {
    case 0x0000: // management
        a_src = 10;
        a_dst = 4;
        a_bss = 16;
        if ((framectl & 0x00F0) == 0x0080)
            rxflags |= 0x0001;
        break;

    case 0x0004: // control
        printf("blarg\n");
        return;

    case 0x0008: // data
        switch (framectl & 0x0300)
        {
        case 0x0000: // STA to STA
            a_src = 10;
            a_dst = 4;
            a_bss = 16;
            break;
        case 0x0100: // STA to DS
            a_src = 10;
            a_dst = 16;
            a_bss = 4;
            break;
        case 0x0200: // DS to STA
            a_src = 16;
            a_dst = 4;
            a_bss = 10;
            break;
        case 0x0300: // DS to DS
            printf("blarg\n");
            return;
        }
        // TODO: those also trigger on other framectl values
        // like 0208 -> C
        framectl &= 0xE7FF;
        if      (framectl == 0x0228) rxflags |= 0x000C; // MP host frame
        else if (framectl == 0x0218) rxflags |= 0x000D; // MP ack frame
        else if (framectl == 0x0118) rxflags |= 0x000E; // MP reply frame
        else if (framectl == 0x0158) rxflags |= 0x000F; // empty MP reply frame
        else                         rxflags |= 0x0008;
        break;
    }

    // TODO get rid of this cruft!!!

    if (MACEqual(&RXBuffer[12 + a_src], (u8*)&IOPORT(W_MACAddr0)))
        {
            printf("MAC equal??\n");
            return; // oops. we received a packet we just sent.
        }

    bssidmatch = MACEqual(&RXBuffer[12 + a_bss], (u8*)&IOPORT(W_BSSID0));
    //if (!(IOPORT(W_BSSID0) & 0x0001) && !(RXBuffer[12 + a_bss] & 0x01) &&
    if (!MACEqual(&RXBuffer[12 + a_dst], (u8*)&IOPORT(W_MACAddr0)) &&
        !(RXBuffer[12 + a_dst] & 0x01))
    {
        printf("dst MAC bad\n");
        PRINT_MAC("frame: ", &RXBuffer[12+a_dst]);
        PRINT_MAC("mac: ", (u8*)&IOPORT(W_MACAddr0));
        return;
    }

    // make RX header

    if (bssidmatch) rxflags |= 0x8000;

    *(u16*)&RXBuffer[0] = rxflags;
    *(u16*)&RXBuffer[2] = 0x0040; // ???
    *(u16*)&RXBuffer[6] = txrate;
    *(u16*)&RXBuffer[8] = framelen;
    *(u16*)&RXBuffer[10] = 0x4080; // min/max RSSI. dunno

    //RXTimestamp = 0;
    StartRX(1);
}

bool CheckRX(int type) // 0=regular 1=MP replies 2=MP host frames
{
    if (IOPORT(W_PowerState) & 0x0300)
        return false;

    if (!(IOPORT(W_RXCnt) & 0x8000))
        return false;

    if (IOPORT(W_RXBufBegin) == IOPORT(W_RXBufEnd))
        return false;
if (IsMPClient) printf("CheckRX(%d)\n", type);
    int rxlen;
    u16 framelen;
    u16 framectl;
    u8 txrate;
    bool bssidmatch;
    u16 rxflags;
    u64 timestamp;

    for (;;)
    {
        timestamp = 0;

        if (type == 0)
        {
            rxlen = Platform::MP_RecvPacket(RXBuffer, &timestamp);
            if (rxlen <= 0)
                rxlen = WifiAP::RecvPacket(RXBuffer);
        }
        else
        {
            rxlen = Platform::MP_RecvHostPacket(RXBuffer, &timestamp);
            if (rxlen < 0)
            {
                // host is gone
                IsMPClient = false;
            }
        }

        if (rxlen <= 0) return false;
        if (rxlen < 12+24) continue;

        framelen = *(u16*)&RXBuffer[10];
        if (framelen != rxlen-12)
        {
            printf("bad frame length %d/%d\n", framelen, rxlen-12);
            continue;
        }
        framelen -= 4;

        framectl = *(u16*)&RXBuffer[12+0];
        txrate = RXBuffer[8];

        u32 a_src, a_dst, a_bss;
        rxflags = 0x0010;
        switch (framectl & 0x000C)
        {
        case 0x0000: // management
            a_src = 10;
            a_dst = 4;
            a_bss = 16;
            if ((framectl & 0x00F0) == 0x0080)
                rxflags |= 0x0001;
            break;

        case 0x0004: // control
            printf("blarg\n");
            continue;

        case 0x0008: // data
            switch (framectl & 0x0300)
            {
            case 0x0000: // STA to STA
                a_src = 10;
                a_dst = 4;
                a_bss = 16;
                break;
            case 0x0100: // STA to DS
                a_src = 10;
                a_dst = 16;
                a_bss = 4;
                break;
            case 0x0200: // DS to STA
                a_src = 16;
                a_dst = 4;
                a_bss = 10;
                break;
            case 0x0300: // DS to DS
                printf("blarg\n");
                continue;
            }
            // TODO: those also trigger on other framectl values
            // like 0208 -> C
            // actually these don't depend on the framectl but on the destination MAC!!
            framectl &= 0xE7FF;
            if      (framectl == 0x0228) rxflags |= 0x000C; // MP host frame
            else if (framectl == 0x0218) rxflags |= 0x000D; // MP ack frame
            else if (framectl == 0x0118) rxflags |= 0x000E; // MP reply frame
            else if (framectl == 0x0158) rxflags |= 0x000F; // empty MP reply frame
            else                         rxflags |= 0x0008;
            break;
        }

        // TODO get rid of this
        // ensuring we don't receive our own crap is the responsibility of the comm layer!!
        if (MACEqual(&RXBuffer[12 + a_src], (u8*)&IOPORT(W_MACAddr0)))
            continue; // oops. we received a packet we just sent.

        bssidmatch = MACEqual(&RXBuffer[12 + a_bss], (u8*)&IOPORT(W_BSSID0));
        //if (!(IOPORT(W_BSSID0) & 0x0001) && !(RXBuffer[12 + a_bss] & 0x01) &&
        if (!MACEqual(&RXBuffer[12 + a_dst], (u8*)&IOPORT(W_MACAddr0)) &&
            !(RXBuffer[12 + a_dst] & 0x01))
        {
            printf("received packet %04X but it didn't pass the MAC check\n", framectl);
            printf("DST: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   RXBuffer[12 + a_dst + 0], RXBuffer[12 + a_dst + 1], RXBuffer[12 + a_dst + 2],
                   RXBuffer[12 + a_dst + 3], RXBuffer[12 + a_dst + 4], RXBuffer[12 + a_dst + 5]);
            printf("BSS: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   RXBuffer[12 + a_bss + 0], RXBuffer[12 + a_bss + 1], RXBuffer[12 + a_bss + 2],
                   RXBuffer[12 + a_bss + 3], RXBuffer[12 + a_bss + 4], RXBuffer[12 + a_bss + 5]);
            continue;
        }

        break;
    }

    WIFI_LOG("wifi: received packet FC:%04X SN:%04X CL:%04X RXT:%d CMT:%d\n",
             framectl, *(u16*)&RXBuffer[12+4+6+6+6], *(u16*)&RXBuffer[12+4+6+6+6+2+2], framelen*4, IOPORT(W_CmdReplyTime));

    // make RX header

    /*if ((rxflags&0xF)==0xD)
    {
        u16 clientfail = *(u16*)&RXBuffer[12+0x1A];
        if (clientfail) printf("CLIENT FAIL: %04X\n", clientfail);
    }*/

    if (bssidmatch) rxflags |= 0x8000;

    *(u16*)&RXBuffer[0] = rxflags;
    *(u16*)&RXBuffer[2] = 0x0040; // ???
    *(u16*)&RXBuffer[6] = txrate;
    *(u16*)&RXBuffer[8] = framelen;
    *(u16*)&RXBuffer[10] = 0x4080; // min/max RSSI. dunno

    if (((framectl & 0x00FF) == 0x0010) && timestamp)
    {
        // if receiving an association response: get the sync value from the host

        u16 aid = *(u16*)&RXBuffer[12+24+4];

        if (aid)
        {
            printf("[CLIENT %01X] host sync=%016llX\n", aid&0xF, timestamp);

            IsMPClient = true;
            USTimestamp = timestamp;
            NextSync = USTimestamp;// + 1024;//512; // TODO: tweak this!

            u32 syncdelay = (framelen * (txrate==0x14 ? 4:8));
            ScheduleEvent(Event_MPClientSync, false, syncdelay, MPClientSync, 0);
        }

        //RXTimestamp = 0;
        StartRX(0);
    }
    else if (((framectl & 0x00FF) == 0x00C0) && timestamp && IsMPClient)
    {
        IsMPClient = false;
        NextSync = 0;

        //RXTimestamp = 0;
        StartRX(0);
    }
    else if (IsMPClient)
    {
        // if we are being a MP client, we need to delay this frame until we reach the
        // timestamp it came with
        // we also need to determine how far we can run after having received this frame

        //RXTimestamp = timestamp;
        //if (RXTimestamp < USTimestamp) RXTimestamp = USTimestamp;
        //NextSync = RXTimestamp + (framelen * (txrate==0x14 ? 4:8));
        u64 rxtime = timestamp;
        if (rxtime <= USTimestamp)
        {
            rxtime = USTimestamp;
            StartRX(0);
        }
        else
        {
            u32 delay = (u32)(rxtime - USTimestamp);
            ScheduleEvent(Event_RX, false, delay, StartRX, 0);
        }

        NextSync = rxtime + (framelen * (txrate==0x14 ? 4:8));

        if ((rxflags & 0xF) == 0xC)
        {
            u16 clienttime = *(u16*)&RXBuffer[12+24];
            u16 clientmask = *(u16*)&RXBuffer[12+26];

            // include the MP reply time window
            NextSync += 112 + ((clienttime + 10) * NumClients(clientmask));
        }

        u32 syncdelay = (u32)(NextSync - USTimestamp);
        ScheduleEvent(Event_MPClientSync, false, syncdelay, MPClientSync, 0);
    }
    else
    {
        // otherwise, just start receiving this frame now

        //RXTimestamp = 0;
        StartRX(0);
    }

    return true;
}


void MPClientSync(u32 param)
{
    if (!ComStatus)
    {
        CheckRX(2);
    }
    else
        printf("MP CLIENT SYNC BUT ALREADY TRXING SHIT??\n");
}

void PeriodicRXCheck(u32 time)
{
    if (!time) WifiAP::MSTimer();

    if ((!IsMPClient) || (USTimestamp > NextSync))
    {
        if (!ComStatus)
        {
            CheckRX(0);
        }
    }

    time += kRXCheckPeriod;
    time &= 0x3FF;
    ScheduleEvent(Event_RXCheck, true, kRXCheckPeriod, PeriodicRXCheck, time);
}

void MSTimer(u32 param)
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
    if (IOPORT(W_BeaconCount1) == ((IOPORT(W_PreBeacon) >> 10) + 1))
    {
        s32 delay = (IOPORT(W_BeaconCount1) << 10) - IOPORT(W_PreBeacon);
        CancelEvent(Event_IRQ15);
        ScheduleEvent(Event_IRQ15, false, delay, SetIRQ15, 0);
    }
    if (IOPORT(W_BeaconCount1) == 0)
    {
        SetIRQ14(1);
    }

    if (IOPORT(W_BeaconCount2) != 0)
    {
        IOPORT(W_BeaconCount2)--;
        if (IOPORT(W_BeaconCount2) == 0) SetIRQ13();
    }

    ScheduleEvent(Event_MSTimer, true, 1024, MSTimer, 0);
}

void RFWakeup(u32 param)
{
    bool switchOffPowerSaving = ((IOPORT(W_PowerUnk) & 0x0001) || ForcePowerOn);
    if ((IOPORT(W_PowerState) & 0x0002) || switchOffPowerSaving)
    {
        IOPORT(W_PowerState) = 0;
        IOPORT(W_RFPins) = 1;
        IOPORT(W_RFPins) = 0x0084;
        SetIRQ(11);
    }
}

void USTimer(u32 param)
{
    /*USTimestamp++;

    u64 sys = NDS::GetSysClockCycles(0);
    u64 ts = SysCyclesToUS(sys);
    ts = USTimestamp - ts;
    printf("diff=%016llX\n", ts);

    if (IsMPClient)
    {
        if (RXTimestamp && (USTimestamp >= RXTimestamp))
        {
            RXTimestamp = 0;
            StartRX();
        }

        if (USTimestamp >= NextSync)
        {
            if (!(ComStatus & 1))
            {
                CheckRX(2);
            }
        }
    }*/


    //WifiAP::USTimer();

    /*bool switchOffPowerSaving = false;
    if (USUntilPowerOn < 0)
    {
        USUntilPowerOn++;

        switchOffPowerSaving = USUntilPowerOn == 0 && (IOPORT(W_PowerUnk) & 0x0001 || ForcePowerOn);
    }
    if (USUntilPowerOn == 0 && (IOPORT(W_PowerState) & 0x0002 || switchOffPowerSaving))
    {
        IOPORT(W_PowerState) = 0;
        IOPORT(W_RFPins) = 1;
        IOPORT(W_RFPins) = 0x0084;
        SetIRQ(11);
    }*/

    /*if (IOPORT(W_USCountCnt))
    {
        USCounter++;
        u32 uspart = (USCounter & 0x3FF);

        if (IOPORT(W_USCompareCnt))
        {
            u32 beaconus = (IOPORT(W_BeaconCount1) << 10) | (0x3FF - uspart);
            if (beaconus == IOPORT(W_PreBeacon)) SetIRQ15();
        }

        if (!uspart) MSTimer();
    }*/

    /*if (IOPORT(W_CmdCountCnt) & 0x0001)
    {
        if (CmdCounter > 0)
        {
            CmdCounter--;
        }
    }

    if (IOPORT(W_ContentFree) != 0)
        IOPORT(W_ContentFree)--;*/

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
            /*if ((!IsMPClient) || (USTimestamp > NextSync))
            {
                if ((!(RXCounter & 0x1FF)))
                {
                    CheckRX(0);
                }
            }

            RXCounter++;*/
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
        RXTime--;
        if (!(RXTime & RXHalfwordTimeMask))
        {
            u16 addr = IOPORT(W_RXTXAddr) << 1;
            if (addr < 0x1FFF) *(u16*)&RAM[addr] = *(u16*)&RXBuffer[RXBufferPtr];

            IncrementRXAddr(addr);
            RXBufferPtr += 2;

            if (RXTime == 0) // finished receiving
            {
                if (addr & 0x2) IncrementRXAddr(addr);

                // copy the RX header
                u16 headeraddr = IOPORT(W_RXBufWriteCursor) << 1;
                *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[0]; IncrementRXAddr(headeraddr);
                *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[2]; IncrementRXAddr(headeraddr, 4);
                *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[6]; IncrementRXAddr(headeraddr);
                *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[8]; IncrementRXAddr(headeraddr);
                *(u16*)&RAM[headeraddr] = *(u16*)&RXBuffer[10];

                IOPORT(W_RXBufWriteCursor) = (addr & ~0x3) >> 1;

                SetIRQ(0);
                SetStatus(1);

                WIFI_LOG("wifi: finished receiving packet %04X\n", *(u16*)&RXBuffer[12]);

                ComStatus &= ~0x1;
                RXCounter = 0;

                if ((RXBuffer[0] & 0x0F) == 0x0C)
                {
                    u16 clientmask = *(u16*)&RXBuffer[0xC + 26];
                    if (IOPORT(W_AIDLow) && (RXBuffer[0xC + 4] & 0x01) && (clientmask & (1 << IOPORT(W_AIDLow))))
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
                else if ((*(u16*)&RXBuffer[0] & 0x800F) == 0x8001)
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

                if ((!ComStatus) && (IOPORT(W_PowerState) & 0x0300))
                {
                    SetStatus(9);
                }
            }

            if (addr == (IOPORT(W_RXBufReadCursor) << 1))
            {
                printf("wifi: RX buffer full\n");
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

            IOPORT(W_RXTXAddr) = addr >> 1;
        }
    }

    // TODO: make it more accurate, eventually
    // in the DS, the wifi system has its own 22MHz clock and doesn't use the system clock
    // measurement: 16715us per frame
    // calculation: 16715.113113088143330744761992174us per frame
    //NDS::ScheduleEvent(NDS::Event_Wifi, true, 33, USTimer, 0);
    //NDS::ScheduleEvent(NDS::Event_WifiLegacy, true, 34, USTimer, 0);
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

    case W_USCount0: UpdateTimers(); return (u16)(USCounter & 0xFFFF);
    case W_USCount1: UpdateTimers(); return (u16)((USCounter >> 16) & 0xFFFF);
    case W_USCount2: UpdateTimers(); return (u16)((USCounter >> 32) & 0xFFFF);
    case W_USCount3: UpdateTimers(); return (u16)(USCounter >> 48);

    case W_USCompare0: return (u16)(USCompare & 0xFFFF);
    case W_USCompare1: return (u16)((USCompare >> 16) & 0xFFFF);
    case W_USCompare2: return (u16)((USCompare >> 32) & 0xFFFF);
    case W_USCompare3: return (u16)(USCompare >> 48);

    case W_ContentFree: UpdateTimers(); return IOPORT(W_ContentFree);

    case W_CmdCount: UpdateTimers(); return (CmdCounter + 9) / 10;

    case W_BBRead:
        if ((IOPORT(W_BBCnt) & 0xF000) != 0x6000)
        {
            printf("WIFI: bad BB read, CNT=%04X\n", IOPORT(W_BBCnt));
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

            //if (*(u16*)&RAM[(rdaddr - 0x1C) & 0x1FFE] == 0x0228)
            //    printf("READ SHITTY CMD HEADER %08X\n", NDS::GetPC(1));
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
                //if (!(USUntilPowerOn < 0 && ForcePowerOn))
                if (!(EventScheduled(Event_RFWakeup) && ForcePowerOn))
                {
                    //printf("mode reset power on %08x\n", NDS::ARM7->R[15]);
                    IOPORT(0x034) = 0x0002;
                    IOPORT(0x27C) = 0x0005;
                    // TODO: 02A2??

                    if (IOPORT(W_PowerUnk) & 0x0002)
                    {
                        //USUntilPowerOn = -2048;
                        ScheduleEvent(Event_RFWakeup, false, 2048, RFWakeup, 0);
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
        printf("wifi: force-setting IF %04X\n", val);
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
                //USUntilPowerOn = -2048;
                ScheduleEvent(Event_RFWakeup, false, 2048, RFWakeup, 0);
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
            //USUntilPowerOn = -2048;
            ScheduleEvent(Event_RFWakeup, false, 2048, RFWakeup, 0);
            ForcePowerOn = false;
        }
        if (val == 0x8000)
        {
            //printf("force power on\n");
            IOPORT(W_PowerState) |= 0x100;
            //USUntilPowerOn = -2048;
            ScheduleEvent(Event_RFWakeup, false, 2048, RFWakeup, 0);
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

    case W_USCountCnt:
        val &= 0x0001;
        if (val && (!IOPORT(W_USCountCnt)))
        {
            ScheduleEvent(Event_MSTimer, false, 1024 - (USCounter & 0x3FF), MSTimer, 0);
            IOPORT(W_USCountCnt) = val;
        }
        else if ((!val) && IOPORT(W_USCountCnt))
        {
            UpdateTimers();
            IOPORT(W_USCountCnt) = val;
            CancelEvent(Event_MSTimer);
        }
        return;
    case W_USCompareCnt:
        if (val & 0x0002)
        {
            UpdateTimers();
            SetIRQ14(2);
        }
        val &= 0x0001;
        break;
    case W_CmdCountCnt:
        val &= 0x0001;
        if ((!val) && IOPORT(W_CmdCountCnt))
        {
            UpdateTimers();
        }
        IOPORT(W_CmdCountCnt) = val;
        return;

    case W_USCount0: UpdateTimers(); USCounter = (USCounter & 0xFFFFFFFFFFFF0000) | (u64)val; return;
    case W_USCount1: UpdateTimers(); USCounter = (USCounter & 0xFFFFFFFF0000FFFF) | ((u64)val << 16); return;
    case W_USCount2: UpdateTimers(); USCounter = (USCounter & 0xFFFF0000FFFFFFFF) | ((u64)val << 32); return;
    case W_USCount3: UpdateTimers(); USCounter = (USCounter & 0x0000FFFFFFFFFFFF) | ((u64)val << 48); return;

    case W_USCompare0:
        USCompare = (USCompare & 0xFFFFFFFFFFFF0000) | (u64)(val & 0xFC00);
        if (val & 0x0001) BlockBeaconIRQ14 = true;
        return;
    case W_USCompare1: USCompare = (USCompare & 0xFFFFFFFF0000FFFF) | ((u64)val << 16); return;
    case W_USCompare2: USCompare = (USCompare & 0xFFFF0000FFFFFFFF) | ((u64)val << 32); return;
    case W_USCompare3: USCompare = (USCompare & 0x0000FFFFFFFFFFFF) | ((u64)val << 48); return;

    case W_ContentFree: UpdateTimers(); IOPORT(W_ContentFree) = val; return;

    case W_CmdCount: UpdateTimers(); CmdCounter = val * 10; return;

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
        val &= 0xFF0E;
        if (val & 0x7FFF) printf("wifi: unknown RXCNT bits set %04X\n", val);
        break;

    case W_RXBufDataRead:
        printf("wifi: writing to RXBUF_DATA_READ. wat\n");
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
        if ((val & 0xFF30) && (val != 0xFFFF)) printf("unusual TXSLOTRESET %04X\n", val);
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

    case W_TXSlotLoc1:
    case W_TXSlotLoc2:
    case W_TXSlotLoc3:
    case W_TXSlotCmd:
        // checkme: is it possible to cancel a queued transfer that hasn't started yet
        // by clearing bit15 here?
        // TODO: "W_TXBUF_CMD.Bit15 can be set ONLY while W_CMD_COUNT is non-zero."
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
