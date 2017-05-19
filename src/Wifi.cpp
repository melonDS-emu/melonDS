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
#include "SPI.h"
#include "Wifi.h"
#include "Platform.h"


namespace Wifi
{

u8 RAM[0x2000];
u16 IO[0x1000>>1];

#define IOPORT(x) IO[(x)>>1]

u16 Random;

u64 USCounter;
u64 USCompare;

u16 BBCnt;
u8 BBWrite;
u8 BBRegs[0x100];
u8 BBRegsRO[0x100];

u8 RFVersion;
u16 RFCnt;
u16 RFData1;
u16 RFData2;
u32 RFRegs[0x40];

typedef struct
{
    u16 Addr;
    u16 Length;
    u8 Rate;
    u8 CurPhase;
    u32 CurPhaseTime;

} TXSlot;

TXSlot TXSlots[6];

u8 RXBuffer[2048];
u32 RXTime;
u16 RXEndAddr;

bool MPInited;



// multiplayer host TX sequence:
// 1. preamble
// 2. IRQ7
// 3. send data
// 4. wait for client replies (duration: 112 + ((10 * CMD_REPLYTIME) * numclients))
// 5. IRQ7
// 6. send ack (16 bytes)
// 7. IRQ12 (and optional IRQ1)
//
// RFSTATUS values:
// 0 = initial
// 1 = RX????
// 2 = switching from RX to TX
// 3 = TX
// 4 = switching from TX to RX
// 5 = MP host data sent, waiting for replies (RFPINS=0x0084)
// 6 = RX
// 7 = ??
// 8 = MP host sending ack (RFPINS=0x0046)
// 9 = idle


// wifi TODO:
// * work out how power saving works, there are oddities


bool Init()
{
    MPInited = false;

    return true;
}

void DeInit()
{
    if (MPInited)
        Platform::MP_DeInit();
}

void Reset()
{
    memset(RAM, 0, 0x2000);
    memset(IO, 0, 0x1000);

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
    else
    {
        printf("wifi: unknown console type %02X\n", console);
        IOPORT(0x000) = 0x1440;
    }

    memset(&IOPORT(0x018), 0xFF, 6);
    memset(&IOPORT(0x020), 0xFF, 6);
}


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
        // TODO: 03C
        IOPORT(W_RFPins) = 0x0046;
        IOPORT(W_RFStatus) = 9;
    }
}

void SetIRQ14(bool forced)
{
    SetIRQ(14);

    if (!forced)
        IOPORT(W_BeaconCount1) = IOPORT(W_BeaconInterval);
    else
        printf("wifi: weird forced IRQ14\n");

    IOPORT(W_BeaconCount2) = 0xFFFF;
    IOPORT(W_TXReqRead) &= 0xFFF2; // todo, eventually?

    if (IOPORT(W_TXSlotBeacon) & 0x8000)
    {
        StartTX_Beacon();
    }

    if (IOPORT(W_ListenCount) == 0)
        IOPORT(W_ListenCount) = IOPORT(W_ListenInterval);

    IOPORT(W_ListenCount)--;printf("IRQ14 prebeacon %04X\n", IOPORT(W_PreBeacon));
}

void SetIRQ15()
{
    SetIRQ(15);

    if (IOPORT(W_PowerTX) & 0x0001)
    {printf("wakeup\n");
        IOPORT(W_RFPins) |= 0x0080;
        IOPORT(W_RFStatus) = 1;
    }
}


bool MACEqual(u8* a, u8* b)
{
    return (*(u32*)&a[0] == *(u32*)&b[0]) && (*(u16*)&a[4] == *(u16*)&b[4]);
}


// TODO: set RFSTATUS/RFPINS

int PreambleLen(int rate)
{
    if (rate == 1) return 192;
    if (IOPORT(W_Preamble) & 0x0004) return 96;
    return 192;
}

void StartTX_LocN(int nslot, int loc)
{
    TXSlot* slot = &TXSlots[nslot];

    if (IOPORT(W_TXSlotLoc1 + (loc*4)) & 0x7000)
        printf("wifi: unusual loc%d bits set %04X\n", loc, IOPORT(W_TXSlotLoc1 + (loc*4)));

    slot->Addr = (IOPORT(W_TXSlotLoc1 + (loc*4)) & 0x0FFF) << 1;
    slot->Length = *(u16*)&RAM[slot->Addr + 0xA] & 0x3FFF;

    u16 rate = *(u16*)&RAM[slot->Addr];
    if (rate == 0x14) slot->Rate = 2;
    else              slot->Rate = 1;

    slot->CurPhase = 0;
    slot->CurPhaseTime = PreambleLen(slot->Rate);

    *(u16*)&RAM[slot->Addr + 0xC + 22] = IOPORT(W_TXSeqNo) << 4;

    int txlen = Platform::MP_SendPacket(&RAM[slot->Addr], 12 + slot->Length);
    printf("wifi: sent %d/%d bytes of loc%d packet\n", txlen, 12+slot->Length, loc);
}

void StartTX_Beacon()
{
    TXSlot* slot = &TXSlots[4];

    slot->Addr = (IOPORT(W_TXSlotBeacon) & 0x0FFF) << 1;
    slot->Length = *(u16*)&RAM[slot->Addr + 0xA] & 0x3FFF;

    u16 rate = *(u16*)&RAM[slot->Addr];
    if (rate == 0x14) slot->Rate = 2;
    else              slot->Rate = 1;

    slot->CurPhase = 0;
    slot->CurPhaseTime = PreambleLen(slot->Rate);

    *(u16*)&RAM[slot->Addr + 0xC + 22] = IOPORT(W_TXSeqNo) << 4;

    u64 oldval = *(u64*)&RAM[slot->Addr + 0xC + 24];
    *(u64*)&RAM[slot->Addr + 0xC + 24] = USCounter;

    int txlen = Platform::MP_SendPacket(&RAM[slot->Addr], 12 + slot->Length);
    //printf("wifi: sent %d/%d bytes of beacon packet\n", txlen, 12+slot->Length);

    *(u64*)&RAM[slot->Addr + 0xC + 24] = oldval;

    IOPORT(W_TXBusy) |= 0x0010;
}

void FireTX()
{
    u16 txbusy = IOPORT(W_TXBusy);
    printf("attempting to fire TX: %04X %04X\n", txbusy, IOPORT(W_TXSlotLoc2));

    if (txbusy & 0x0008)
    {
        if (IOPORT(W_TXSlotLoc3) & 0x8000)
        {
            StartTX_LocN(3, 2);
            return;
        }
    }

    if (txbusy & 0x0004)
    {
        if (IOPORT(W_TXSlotLoc2) & 0x8000)
        {
            StartTX_LocN(2, 1);
            return;
        }
    }

    if (txbusy & 0x0001)
    {
        if (IOPORT(W_TXSlotLoc1) & 0x8000)
        {
            StartTX_LocN(0, 0);
            return;
        }
    }

    IOPORT(W_TXBusy) = 0;
}

void ProcessTX(TXSlot* slot, int num)
{
    slot->CurPhaseTime--;
    if (slot->CurPhaseTime > 0) return;

    switch (slot->CurPhase)
    {
    case 0: // preamble done
        {
            u32 len = slot->Length;
            if (slot->Rate == 2) len *= 4;
            else                 len *= 8;

            slot->CurPhase = 1;
            slot->CurPhaseTime = len;

            SetIRQ(7);
            IOPORT(W_TXSeqNo) = (IOPORT(W_TXSeqNo) + 1) & 0x0FFF;
        }
        break;

    case 1: // transmit done
        {
            // checkme
            *(u16*)&RAM[slot->Addr] = 0x0001;
            RAM[slot->Addr + 5] = 0;

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

            FireTX();
        }
        break;
    }
}


void CheckRX()
{
    if (!(IOPORT(W_RXCnt) & 0x8000))
        return;

    int rxlen = Platform::MP_RecvPacket(RXBuffer, false);
    if (rxlen < 12+24) return;

    u16 framelen = *(u16*)&RXBuffer[10];
    if (framelen != rxlen-12)
    {
        printf("bad frame length\n");
        return;
    }
    framelen -= 4;

    u16 framectl = *(u16*)&RXBuffer[12+0];
    u8 txrate = RXBuffer[8];

    u32 a_src, a_dst, a_bss;
    u16 rxflags = 0x0010;
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
        rxflags |= 0x0008;
        break;
    }

    if (MACEqual(&RXBuffer[12 + a_src], (u8*)&IOPORT(W_MACAddr0)))
        return; // oops. we received a packet we just sent.

    bool bssidmatch = MACEqual(&RXBuffer[12 + a_bss], (u8*)&IOPORT(W_BSSID0));
    if (!(IOPORT(W_BSSID0) & 0x0001) && !(RXBuffer[12 + a_bss] & 0x01) &&
        !bssidmatch)
        return;

    if (framectl != 0x0080)
        printf("wifi: received packet. framectrl=%04X\n", framectl);

    // make RX header

    if (bssidmatch) rxflags |= 0x8000;

    *(u16*)&RXBuffer[0] = rxflags;
    *(u16*)&RXBuffer[2] = 0x0040; // ???
    *(u16*)&RXBuffer[4] = 0x7777;
    *(u16*)&RXBuffer[6] = txrate;
    *(u16*)&RXBuffer[8] = framelen;
    *(u16*)&RXBuffer[10] = 0x4080; // min/max RSSI. dunno

    RXTime = framelen * ((txrate==0x14) ? 4:8);

    // TODO: write packet progressively?
    // TODO: RX/TX addr register
    framelen += 12;
    u16 addr = IOPORT(W_RXBufWriteCursor) << 1;
    for (int i = 0; i < framelen; i += 2)
    {
        *(u16*)&RAM[addr] = *(u16*)&RXBuffer[i];
        addr += 2;

        if (addr == (IOPORT(W_RXBufEnd) & 0x1FFE))
            addr = (IOPORT(W_RXBufBegin) & 0x1FFE);

        if (addr == (IOPORT(W_RXBufReadCursor) << 1))
        {
            printf("wifi: RX buffer full\n");
            // TODO: proper error management
            return;
        }
    }

    RXEndAddr = ((addr >> 1) + 1) & ~0x1;

    SetIRQ(6);
}


void MSTimer()
{
    IOPORT(W_BeaconCount1)--;
    if (IOPORT(W_USCompareCnt))
    {
        if (IOPORT(W_BeaconCount1) == 0) SetIRQ14(false);
    }

    if (IOPORT(W_BeaconCount2) != 0)
    {
        IOPORT(W_BeaconCount2)--;
        if (IOPORT(W_BeaconCount2) == 0) SetIRQ13();
    }

    if (!IOPORT(W_TXBusy))
        CheckRX();
}

void USTimer(u32 param)
{
    if (IOPORT(W_USCountCnt))
    {
        USCounter++;
        u32 uspart = (USCounter & 0x3FF);

        if (IOPORT(W_USCompareCnt))
        {
            if (USCounter == USCompare) SetIRQ14(false);

            u32 beaconus = (IOPORT(W_BeaconCount1) << 10) | (0x3FF - uspart);
            if (beaconus == IOPORT(W_PreBeacon)) SetIRQ15();
        }

        if (!uspart) MSTimer();
    }

    if (IOPORT(W_ContentFree) != 0)
        IOPORT(W_ContentFree)--;

    u16 txbusy = IOPORT(W_TXBusy);
    if (txbusy)
    {
        if (txbusy & 0x0010) ProcessTX(&TXSlots[4], 4);
        else if (txbusy & 0x0008) ProcessTX(&TXSlots[3], 3);
        else if (txbusy & 0x0004) ProcessTX(&TXSlots[2], 2);
        // TODO CMD
        else if (txbusy & 0x0001) ProcessTX(&TXSlots[0], 0);
    }
    else if (RXTime)
    {
        RXTime--;
        if (RXTime == 0)
        {
            IOPORT(W_RXBufWriteCursor) = RXEndAddr;
            SetIRQ(0);
        }
    }

    // TODO: make it more accurate, eventually
    // in the DS, the wifi system has its own 22MHz clock and doesn't use the system clock
    NDS::ScheduleEvent(NDS::Event_Wifi, true, 33, USTimer, 0);
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


// TODO: wifi waitstates

u16 Read(u32 addr)
{
    if (addr >= 0x04810000)
        return 0;

    addr &= 0x7FFE;
    //printf("WIFI: read %08X\n", addr);
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
        //case 0x214: NDS::debug(0); break;
        //case 0x040: NDS::debug(0); break;
    }

    //printf("WIFI: read %08X\n", addr);
    return IOPORT(addr&0xFFF);
}

void Write(u32 addr, u16 val)
{
    if (addr >= 0x04810000)
        return;

    addr &= 0x7FFE;
    //printf("WIFI: write %08X %04X\n", addr, val);
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
                IOPORT(0x034) = 0x0002;
                IOPORT(W_RFPins) = 0x0046;
                IOPORT(W_RFStatus) = 9;
                IOPORT(0x27C) = 0x0005;
                // TODO: 02A2??
            }
            else if ((oldval & 0x0001) && !(val & 0x0001))
            {
                IOPORT(0x27C) = 0x000A;
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
        break;

    case W_IF:
        IOPORT(W_IF) &= ~val;
        return;
    case W_IFSet:
        IOPORT(W_IF) |= (val & 0xFBFF);
        printf("wifi: force-setting IF %04X\n", val);
        return;

    case W_PowerState:
        if (val & 0x0002)
        {
            // TODO: delay for this
            SetIRQ(11);
            IOPORT(W_PowerState) = 0x0000;

            // checkme
            IOPORT(W_RFPins) = 0x00C6;
            IOPORT(W_RFStatus) = 9;
        }
        return;
    case W_PowerForce:
        if ((val&0x8001)==0x8000) printf("WIFI: forcing power %04X\n", val);
        val &= 0x8001;
        if (val == 0x8001)
        {
            IOPORT(0x034) = 0x0002;
            IOPORT(W_PowerState) = 0x0200;
            IOPORT(W_TXReqRead) = 0;
            IOPORT(W_RFPins) = 0x0046;
            IOPORT(W_RFStatus) = 9;
        }
        break;
    case W_PowerUS:
        // schedule timer event when the clock is enabled
        // TODO: check whether this resets USCOUNT (and also which other events can reset it)
        if ((IOPORT(W_PowerUS) & 0x0001) && !(val & 0x0001))
        {
            NDS::ScheduleEvent(NDS::Event_Wifi, true, 33, USTimer, 0);
            if (!MPInited)
            {
                Platform::MP_Init();
                MPInited = true;
            }
        }
        else if (!(IOPORT(W_PowerUS) & 0x0001) && (val & 0x0001))
        {
            NDS::CancelEvent(NDS::Event_Wifi);
        }
        break;

    case W_USCountCnt: val &= 0x0001; break;
    case W_USCompareCnt:
        if (val & 0x0002) SetIRQ14(true);
        val &= 0x0001;
        break;

    case W_USCount0: USCounter = (USCounter & 0xFFFFFFFFFFFF0000) | (u64)val; return;
    case W_USCount1: USCounter = (USCounter & 0xFFFFFFFF0000FFFF) | ((u64)val << 16); return;
    case W_USCount2: USCounter = (USCounter & 0xFFFF0000FFFFFFFF) | ((u64)val << 32); return;
    case W_USCount3: USCounter = (USCounter & 0x0000FFFFFFFFFFFF) | ((u64)val << 48); return;

    case W_USCompare0:
        USCompare = (USCompare & 0xFFFFFFFFFFFF0000) | (u64)(val & 0xFC00);
        if (val & 0x03FF)
            printf("wifi: mysterious USCOMPARE bits set %08X%08X %04X\n", (u32)(USCompare>>32), (u32)USCompare, val); // TODO
        return;
    case W_USCompare1: USCompare = (USCompare & 0xFFFFFFFF0000FFFF) | ((u64)val << 16); return;
    case W_USCompare2: USCompare = (USCompare & 0xFFFF0000FFFFFFFF) | ((u64)val << 32); return;
    case W_USCompare3: USCompare = (USCompare & 0x0000FFFFFFFFFFFF) | ((u64)val << 48); return;

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
        IOPORT(W_TXBusy) |= (val & 0x000F);
        FireTX();
        // CHECKME!!!!!!!!!!!
        return;

    case W_TXSlotReset:
        printf("TX slot reset: %04X\n", val);
        if (val & 0x0001) IOPORT(W_TXSlotLoc1) &= 0x7FFF;
        if (val & 0x0002) IOPORT(W_TXSlotCmd) &= 0x7FFF;
        if (val & 0x0004) IOPORT(W_TXSlotLoc2) &= 0x7FFF;
        if (val & 0x0008) IOPORT(W_TXSlotLoc3) &= 0x7FFF;
        // checkme: any bits affecting the beacon slot?
        if (val & 0x0040) IOPORT(W_TXSlotReply2) &= 0x7FFF;
        if (val & 0x0080) IOPORT(W_TXSlotReply1) &= 0x7FFF;
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

    case 0x090:
    case 0x0A0:
    case 0x0A4:
    case 0x0A8:
    case 0x094:
        printf("wifi: trying to send packet. %08X=%04X\n", addr, val);
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
    }

    //printf("WIFI: write %08X %04X\n", addr, val);
    IOPORT(addr&0xFFF) = val;
}

}
