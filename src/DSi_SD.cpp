/*
    Copyright 2016-2021 Arisotura

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
#include "DSi.h"
#include "DSi_SD.h"
#include "DSi_NWifi.h"
#include "Platform.h"
#include "Config.h"


// observed IRQ behavior during transfers
//
// during reads:
// * bit23 is cleared during the first block, always set otherwise. weird
// * bit24 (RXRDY) gets set when the FIFO is full
//
// during reads with FIFO32:
// * FIFO16 drains directly into FIFO32
// * when bit24 is set, FIFO32 is already full (with contents from the other FIFO)
// * reading from an empty FIFO just wraps around (and sets bit21)
// * FIFO32 starts filling when bit24 would be set?
//
//
// TX:
// * when sending command, if current FIFO full
// * upon ContinueTransfer(), if current FIFO full
// * -> upon DataTX() if current FIFO full
// * when filling FIFO


#define SD_DESC  Num?"SDIO":"SD/MMC"


DSi_SDHost::DSi_SDHost(u32 num)
{
    Num = num;

    Ports[0] = NULL;
    Ports[1] = NULL;
}

DSi_SDHost::~DSi_SDHost()
{
    if (Ports[0]) delete Ports[0];
    if (Ports[1]) delete Ports[1];
}

void DSi_SDHost::Reset()
{
    if (Num == 0)
    {
        PortSelect = 0x0200; // CHECKME
    }
    else
    {
        PortSelect = 0x0100; // CHECKME
    }

    SoftReset = 0x0007; // CHECKME
    SDClock = 0;
    SDOption = 0;

    Command = 0;
    Param = 0;
    memset(ResponseBuffer, 0, sizeof(ResponseBuffer));

    DataFIFO[0].Clear();
    DataFIFO[1].Clear();
    CurFIFO = 0;
    DataFIFO32.Clear();

    IRQStatus = 0;
    IRQMask = 0x8B7F031D;

    CardIRQStatus = 0;
    CardIRQMask = 0xC007;
    CardIRQCtl = 0;

    DataCtl = 0;
    Data32IRQ = 0;
    DataMode = 0;
    BlockCount16 = 0; BlockCount32 = 0; BlockCountInternal = 0;
    BlockLen16 = 0;   BlockLen32 = 0;
    StopAction = 0;

    TXReq = false;

    if (Ports[0]) delete Ports[0];
    if (Ports[1]) delete Ports[1];
    Ports[0] = nullptr;
    Ports[1] = nullptr;

    if (Num == 0)
    {
        DSi_MMCStorage* sd;
        DSi_MMCStorage* mmc;

        if (Config::DSiSDEnable)
        {
            sd = new DSi_MMCStorage(this, false, DSi::SDIOFile);
            u8 sd_cid[16] = {0xBD, 0x12, 0x34, 0x56, 0x78, 0x03, 0x4D, 0x30, 0x30, 0x46, 0x50, 0x41, 0x00, 0x00, 0x15, 0x00};
            sd->SetCID(sd_cid);
        }
        else
            sd = nullptr;

        mmc = new DSi_MMCStorage(this, true, DSi::SDMMCFile);
        mmc->SetCID(DSi::eMMC_CID);

        Ports[0] = sd;
        Ports[1] = mmc;
    }
    else
    {
        DSi_NWifi* nwifi = new DSi_NWifi(this);

        Ports[0] = nwifi;
    }

    if (Ports[0]) Ports[0]->Reset();
    if (Ports[1]) Ports[1]->Reset();
}

void DSi_SDHost::DoSavestate(Savestate* file)
{
    // TODO!
}


void DSi_SDHost::UpdateData32IRQ()
{
    if (DataMode == 0) return;

    u32 oldflags = ((Data32IRQ >> 8) & 0x1) | (((~Data32IRQ) >> 8) & 0x2);
    oldflags &= (Data32IRQ >> 11);

    Data32IRQ &= ~0x0300;
    if (DataFIFO32.Level() >= (BlockLen32>>2)) Data32IRQ |= (1<<8);
    if (!DataFIFO32.IsEmpty())                 Data32IRQ |= (1<<9);

    u32 newflags = ((Data32IRQ >> 8) & 0x1) | (((~Data32IRQ) >> 8) & 0x2);
    newflags &= (Data32IRQ >> 11);

    if ((oldflags == 0) && (newflags != 0))
        NDS::SetIRQ2(Num ? NDS::IRQ2_DSi_SDIO : NDS::IRQ2_DSi_SDMMC);
}

void DSi_SDHost::ClearIRQ(u32 irq)
{
    IRQStatus &= ~(1<<irq);
}

void DSi_SDHost::SetIRQ(u32 irq)
{
    u32 oldflags = IRQStatus & ~IRQMask;

    IRQStatus |= (1<<irq);
    u32 newflags = IRQStatus & ~IRQMask;

    if ((oldflags == 0) && (newflags != 0))
        NDS::SetIRQ2(Num ? NDS::IRQ2_DSi_SDIO : NDS::IRQ2_DSi_SDMMC);
}

void DSi_SDHost::UpdateIRQ(u32 oldmask)
{
    u32 oldflags = IRQStatus & ~oldmask;
    u32 newflags = IRQStatus & ~IRQMask;

    if ((oldflags == 0) && (newflags != 0))
        NDS::SetIRQ2(Num ? NDS::IRQ2_DSi_SDIO : NDS::IRQ2_DSi_SDMMC);
}

void DSi_SDHost::SetCardIRQ()
{
    if (!(CardIRQCtl & (1<<0))) return;

    u16 oldflags = CardIRQStatus & ~CardIRQMask;
    DSi_SDDevice* dev = Ports[PortSelect & 0x1];

    if (dev->IRQ) CardIRQStatus |=  (1<<0);
    else          CardIRQStatus &= ~(1<<0);

    u16 newflags = CardIRQStatus & ~CardIRQMask;

    if ((oldflags == 0) && (newflags != 0)) // checkme
    {
        NDS::SetIRQ2(Num ? NDS::IRQ2_DSi_SDIO : NDS::IRQ2_DSi_SDMMC);
        NDS::SetIRQ2(Num ? NDS::IRQ2_DSi_SDIO_Data1 : NDS::IRQ2_DSi_SD_Data1);
    }
}

void DSi_SDHost::UpdateCardIRQ(u16 oldmask)
{
    u16 oldflags = CardIRQStatus & ~oldmask;
    u16 newflags = CardIRQStatus & ~CardIRQMask;

    if ((oldflags == 0) && (newflags != 0)) // checkme
    {
        NDS::SetIRQ2(Num ? NDS::IRQ2_DSi_SDIO : NDS::IRQ2_DSi_SDMMC);
        NDS::SetIRQ2(Num ? NDS::IRQ2_DSi_SDIO_Data1 : NDS::IRQ2_DSi_SD_Data1);
    }
}

void DSi_SDHost::SendResponse(u32 val, bool last)
{
    *(u32*)&ResponseBuffer[6] = *(u32*)&ResponseBuffer[4];
    *(u32*)&ResponseBuffer[4] = *(u32*)&ResponseBuffer[2];
    *(u32*)&ResponseBuffer[2] = *(u32*)&ResponseBuffer[0];
    *(u32*)&ResponseBuffer[0] = val;

    if (last) SetIRQ(0);
}

void DSi_SDHost::FinishRX(u32 param)
{
    DSi_SDHost* host = (param & 0x1) ? DSi::SDIO : DSi::SDMMC;

    host->CheckSwapFIFO();

    if (host->DataMode == 1)
        host->UpdateFIFO32();
    else
        host->SetIRQ(24);
}

u32 DSi_SDHost::DataRX(u8* data, u32 len)
{
    if (len != BlockLen16) { printf("!! BAD BLOCKLEN\n"); len = BlockLen16; }

    bool last = (BlockCountInternal == 0);

    u32 f = CurFIFO ^ 1;
    for (u32 i = 0; i < len; i += 2)
        DataFIFO[f].Write(*(u16*)&data[i]);

    //CurFIFO = f;
    //SetIRQ(24);
    // TODO: determine what the delay should be!
    // for now, this is a placeholder
    // we need a delay because DSi boot2 will send a command and then wait for IRQ0
    // but if IRQ24 is thrown instantly, the handler clears IRQ0 before the
    // send-command function starts polling IRQ status
    u32 param = Num | (last << 1);
    NDS::ScheduleEvent(Num ? NDS::Event_DSi_SDIOTransfer : NDS::Event_DSi_SDMMCTransfer,
                       false, 512, FinishRX, param);

    return len;
}

void DSi_SDHost::FinishTX(u32 param)
{
    DSi_SDHost* host = (param & 0x1) ? DSi::SDIO : DSi::SDMMC;
    DSi_SDDevice* dev = host->Ports[host->PortSelect & 0x1];

    if (host->BlockCountInternal == 0)
    {
        if (host->StopAction & (1<<8))
        {
            if (dev) dev->SendCMD(12, 0);
        }

        // CHECKME: presumably IRQ2 should not trigger here, but rather
        // when the data transfer is done
        //SetIRQ(0);
        host->SetIRQ(2);
        host->TXReq = false;
    }
    else
    {
        if (dev) dev->ContinueTransfer();
    }
}

u32 DSi_SDHost::DataTX(u8* data, u32 len)
{
    TXReq = true;

    u32 f = CurFIFO;

    if (DataMode == 1)
    {
        if ((DataFIFO32.Level() << 2) < len)
        {
            if (DataFIFO32.IsEmpty())
            {
                SetIRQ(25);
                DSi::CheckNDMAs(1, Num ? 0x29 : 0x28);
            }
            return 0;
        }

        // drain FIFO32 into FIFO16

        if (!DataFIFO[f].IsEmpty()) printf("VERY BAD!! TRYING TO DRAIN FIFO32 INTO FIFO16 BUT IT CONTAINS SHIT ALREADY\n");
        for (;;)
        {
            u32 f = CurFIFO;
            if ((DataFIFO[f].Level() << 1) >= BlockLen16) break;
            if (DataFIFO32.IsEmpty()) break;

            u32 val = DataFIFO32.Read();
            DataFIFO[f].Write(val & 0xFFFF);
            DataFIFO[f].Write(val >> 16);
        }

        UpdateData32IRQ();

        if (BlockCount32 > 1)
            BlockCount32--;
    }
    else
    {
        if ((DataFIFO[f].Level() << 1) < len)
        {
            if (DataFIFO[f].IsEmpty()) SetIRQ(25);
            return 0;
        }
    }

    for (u32 i = 0; i < len; i += 2)
        *(u16*)&data[i] = DataFIFO[f].Read();

    CurFIFO ^= 1;
    BlockCountInternal--;

    NDS::ScheduleEvent(Num ? NDS::Event_DSi_SDIOTransfer : NDS::Event_DSi_SDMMCTransfer,
                       false, 512, FinishTX, Num);

    return len;
}

u32 DSi_SDHost::GetTransferrableLen(u32 len)
{
    if (len > BlockLen16) len = BlockLen16; // checkme
    return len;
}

void DSi_SDHost::CheckRX()
{
    DSi_SDDevice* dev = Ports[PortSelect & 0x1];

    CheckSwapFIFO();

    if (BlockCountInternal <= 1)
    {
        if (StopAction & (1<<8))
        {
            if (dev) dev->SendCMD(12, 0);
        }

        // CHECKME: presumably IRQ2 should not trigger here, but rather
        // when the data transfer is done
        //SetIRQ(0);
        SetIRQ(2);
    }
    else
    {
        BlockCountInternal--;

        if (dev) dev->ContinueTransfer();
    }
}

void DSi_SDHost::CheckTX()
{
    if (!TXReq) return;

    if (DataMode == 1)
    {
        if ((DataFIFO32.Level() << 2) < BlockLen32)
            return;
    }
    else
    {
        u32 f = CurFIFO;
        if ((DataFIFO[f].Level() << 1) < BlockLen16)
            return;
    }

    DSi_SDDevice* dev = Ports[PortSelect & 0x1];
    if (dev) dev->ContinueTransfer();
}


u16 DSi_SDHost::Read(u32 addr)
{
    switch (addr & 0x1FF)
    {
    case 0x000: return Command;
    case 0x002: return PortSelect & 0x030F;
    case 0x004: return Param & 0xFFFF;
    case 0x006: return Param >> 16;

    case 0x008: return StopAction;
    case 0x00A: return BlockCount16;

    case 0x00C: return ResponseBuffer[0];
    case 0x00E: return ResponseBuffer[1];
    case 0x010: return ResponseBuffer[2];
    case 0x012: return ResponseBuffer[3];
    case 0x014: return ResponseBuffer[4];
    case 0x016: return ResponseBuffer[5];
    case 0x018: return ResponseBuffer[6];
    case 0x01A: return ResponseBuffer[7];

    case 0x01C:
        {
            u16 ret = (IRQStatus & 0x031D);

            if (!Num)
            {
                if (Ports[0]) // basic check of whether the SD card is inserted
                    ret |= 0x00B0;
                else
                    ret |= 0x0008;
            }
            else
            {
                // SDIO wifi is always inserted, I guess
                ret |= 0x00B0;
            }
            return ret;
        }
    case 0x01E: return ((IRQStatus >> 16) & 0x8B7F);
    case 0x020: return IRQMask & 0x031D;
    case 0x022: return (IRQMask >> 16) & 0x8B7F;

    case 0x024: return SDClock;
    case 0x026: return BlockLen16;
    case 0x028: return SDOption;

    case 0x02C: return 0; // TODO

    case 0x034: return CardIRQCtl;
    case 0x036: return CardIRQStatus;
    case 0x038: return CardIRQMask;

    case 0x030: return ReadFIFO16();

    case 0x0D8: return DataCtl;

    case 0x0E0: return SoftReset;

    case 0x0F6: return 0; // MMC write protect (always 0)

    case 0x100: return Data32IRQ;
    case 0x102: return 0;
    case 0x104: return BlockLen32;
    case 0x108: return BlockCount32;

    // dunno
    case 0x106: return 0;
    case 0x10A: return 0;
    }

    printf("unknown %s read %08X @ %08X\n", SD_DESC, addr, NDS::GetPC(1));
    return 0;
}

u16 DSi_SDHost::ReadFIFO16()
{
    u32 f = CurFIFO;
    if (DataFIFO[f].IsEmpty())
    {
        // TODO
        // on hardware it seems to wrap around. underflow bit is set upon the first 'empty' read.
        return 0;
    }

    DSi_SDDevice* dev = Ports[PortSelect & 0x1];
    u16 ret = DataFIFO[f].Read();

    if (DataFIFO[f].IsEmpty())
    {
        CheckRX();
    }

    return ret;
}

u32 DSi_SDHost::ReadFIFO32()
{
    if (DataMode != 1) return 0;

    if (DataFIFO32.IsEmpty())
    {
        // TODO
        return 0;
    }

    DSi_SDDevice* dev = Ports[PortSelect & 0x1];
    u32 ret = DataFIFO32.Read();

    if (DataFIFO32.IsEmpty())
    {
        CheckRX();
    }

    UpdateData32IRQ();

    return ret;
}

void DSi_SDHost::Write(u32 addr, u16 val)
{
    switch (addr & 0x1FF)
    {
    case 0x000:
        {
            Command = val;
            u8 cmd = Command & 0x3F;

            DSi_SDDevice* dev = Ports[PortSelect & 0x1];
            if (dev)
            {
                // CHECKME
                // "Setting Command Type to "ACMD" is automatically sending an APP_CMD prefix prior to the command number"
                // except DSi boot2 manually sends an APP_CMD prefix AND sets the next command to be ACMD
                switch ((Command >> 6) & 0x3)
                {
                case 0: dev->SendCMD(cmd, Param); break;
                case 1: /*dev->SendCMD(55, 0);*/ dev->SendCMD(cmd, Param); break;
                default:
                    printf("%s: unknown command type %d, %02X %08X\n", SD_DESC, (Command>>6)&0x3, cmd, Param);
                    break;
                }
            }
            else printf("%s: SENDING CMD %04X TO NULL DEVICE\n", SD_DESC, val);
        }
        return;

    case 0x002: PortSelect = (val & 0x040F) | (PortSelect & 0x0300); return;
    case 0x004: Param = (Param & 0xFFFF0000) | val; return;
    case 0x006: Param = (Param & 0x0000FFFF) | (val << 16); return;

    case 0x008: StopAction = val & 0x0101; return;
    case 0x00A: BlockCount16 = val; BlockCountInternal = val; return;

    case 0x01C: IRQStatus &= (val | 0xFFFF0000); return;
    case 0x01E: IRQStatus &= ((val << 16) | 0xFFFF); return;
    case 0x020:
        {
            u32 oldmask = IRQMask;
            IRQMask = (IRQMask & 0x8B7F0000) | (val & 0x031D);
            UpdateIRQ(oldmask);
        }
        return;
    case 0x022:
        {
            u32 oldmask = IRQMask;
            IRQMask = (IRQMask & 0x0000031D) | ((val & 0x8B7F) << 16);
            UpdateIRQ(oldmask);
            //if (!DataFIFO[CurFIFO]->IsEmpty()) SetIRQ(24); // checkme
            //if (DataFIFO[CurFIFO]->IsEmpty()) SetIRQ(25); // checkme
        }
        return;

    case 0x024: SDClock = val & 0x03FF; return;
    case 0x026:
        BlockLen16 = val & 0x03FF;
        if (BlockLen16 > 0x200) BlockLen16 = 0x200;
        return;
    case 0x028: SDOption = val & 0xC1FF; return;

    case 0x030: WriteFIFO16(val); return;

    case 0x034:
        CardIRQCtl = val & 0x0305;
        SetCardIRQ();
        return;
    case 0x036:
        CardIRQStatus &= val;
        return;
    case 0x038:
        {
            u16 oldmask = CardIRQMask;
            CardIRQMask = val & 0xC007;
            UpdateCardIRQ(oldmask);
        }
        //CardIRQMask = val & 0xC007;
        //SetCardIRQ();
        return;

    case 0x0D8:
        DataCtl = (val & 0x0022);
        DataMode = ((DataCtl >> 1) & 0x1) & ((Data32IRQ >> 1) & 0x1);
        return;

    case 0x0E0:
        if ((SoftReset & 0x0001) && !(val & 0x0001))
        {
            printf("%s: RESET\n", SD_DESC);
            StopAction = 0;
            memset(ResponseBuffer, 0, sizeof(ResponseBuffer));
            IRQStatus = 0;
            // TODO: ERROR_DETAIL_STATUS
            SDClock &= ~0x0500;
            SDOption = 0x40EE;
            // TODO: CARD_IRQ_STAT
            // TODO: FIFO16 shit

            if (Ports[0]) Ports[0]->Reset();
            if (Ports[1]) Ports[1]->Reset();
        }
        SoftReset = 0x0006 | (val & 0x0001);
        return;

    case 0x100:
        Data32IRQ = (val & 0x1802) | (Data32IRQ & 0x0300);
        if (val & (1<<10)) DataFIFO32.Clear();
        DataMode = ((DataCtl >> 1) & 0x1) & ((Data32IRQ >> 1) & 0x1);
        return;
    case 0x102: return;
    case 0x104: BlockLen32 = val & 0x03FF; return;
    case 0x108: BlockCount32 = val; return;

    // dunno
    case 0x106: return;
    case 0x10A: return;
    }

    printf("unknown %s write %08X %04X\n", SD_DESC, addr, val);
}

void DSi_SDHost::WriteFIFO16(u16 val)
{
    DSi_SDDevice* dev = Ports[PortSelect & 0x1];
    u32 f = CurFIFO;
    if (DataFIFO[f].IsFull())
    {
        // TODO
        printf("!!!! %s FIFO (16) FULL\n", SD_DESC);
        return;
    }

    DataFIFO[f].Write(val);

    CheckTX();
}

void DSi_SDHost::WriteFIFO32(u32 val)
{
    if (DataMode != 1) return;

    if (DataFIFO32.IsFull())
    {
        // TODO
        printf("!!!! %s FIFO (32) FULL\n", SD_DESC);
        return;
    }

    DataFIFO32.Write(val);

    CheckTX();

    UpdateData32IRQ();
}

void DSi_SDHost::UpdateFIFO32()
{
    // check whether we can drain FIFO32 into FIFO16, or vice versa

    if (DataMode != 1) return;

    if (!DataFIFO32.IsEmpty()) printf("VERY BAD!! TRYING TO DRAIN FIFO16 INTO FIFO32 BUT IT CONTAINS SHIT ALREADY\n");
    for (;;)
    {
        u32 f = CurFIFO;
        if ((DataFIFO32.Level() << 2) >= BlockLen32) break;
        if (DataFIFO[f].IsEmpty()) break;

        u32 val = DataFIFO[f].Read();
        val |= (DataFIFO[f].Read() << 16);
        DataFIFO32.Write(val);
    }

    UpdateData32IRQ();

    if ((DataFIFO32.Level() << 2) >= BlockLen32)
    {
        DSi::CheckNDMAs(1, Num ? 0x29 : 0x28);
    }
}

void DSi_SDHost::CheckSwapFIFO()
{
    // check whether we can swap the FIFOs

    u32 f = CurFIFO;
    bool cur_empty = (DataMode == 1) ? DataFIFO32.IsEmpty() : DataFIFO[f].IsEmpty();
    if (cur_empty && ((DataFIFO[f^1].Level() << 1) >= BlockLen16))
    {
        CurFIFO ^= 1;
    }
}


#define MMC_DESC  (Internal?"NAND":"SDcard")

DSi_MMCStorage::DSi_MMCStorage(DSi_SDHost* host, bool internal, FILE* file) : DSi_SDDevice(host)
{
    Internal = internal;
    File = file;
}

DSi_MMCStorage::~DSi_MMCStorage()
{}

void DSi_MMCStorage::Reset()
{
    // TODO: reset file access????

    CSR = 0x00000100; // checkme

    // TODO: busy bit
    // TODO: SDHC/SDXC bit
    OCR = 0x80FF8000;

    // TODO: customize based on card size etc
    u8 csd_template[16] = {0x40, 0x40, 0x96, 0xE9, 0x7F, 0xDB, 0xF6, 0xDF, 0x01, 0x59, 0x0F, 0x2A, 0x01, 0x26, 0x90, 0x00};
    memcpy(CSD, csd_template, 16);

    // checkme
    memset(SCR, 0, 8);
    *(u32*)&SCR[0] = 0x012A0000;

    memset(SSR, 0, 64);

    BlockSize = 0;
    RWAddress = 0;
    RWCommand = 0;
}

void DSi_MMCStorage::SendCMD(u8 cmd, u32 param)
{
    if (CSR & (1<<5))
    {
        CSR &= ~(1<<5);
        return SendACMD(cmd, param);
    }

    switch (cmd)
    {
    case 0: // reset/etc
        Host->SendResponse(CSR, true);
        return;

    case 1: // SEND_OP_COND
        // CHECKME!!
        // also TODO: it's different for the SD card
        if (Internal)
        {
            param &= ~(1<<30);
            OCR &= 0xBF000000;
            OCR |= (param & 0x40FFFFFF);
            Host->SendResponse(OCR, true);
            SetState(0x01);
        }
        else
        {
            printf("CMD1 on SD card!!\n");
        }
        return;

    case 2:
    case 10: // get CID
        Host->SendResponse(*(u32*)&CID[12], false);
        Host->SendResponse(*(u32*)&CID[8], false);
        Host->SendResponse(*(u32*)&CID[4], false);
        Host->SendResponse(*(u32*)&CID[0], true);
        if (cmd == 2) SetState(0x02);
        return;

    case 3: // get/set RCA
        if (Internal)
        {
            RCA = param >> 16;
            Host->SendResponse(CSR|0x10000, true); // huh??
        }
        else
        {
            // TODO
            printf("CMD3 on SD card: TODO\n");
            Host->SendResponse((CSR & 0x1FFF) | ((CSR >> 6) & 0x2000) | ((CSR >> 8) & 0xC000) | (1 << 16), true);
        }
        return;

    case 6: // MMC: 'SWITCH'
        // TODO!
        Host->SendResponse(CSR, true);
        return;

    case 7: // select card (by RCA)
        Host->SendResponse(CSR, true);
        return;

    case 8: // set voltage
        Host->SendResponse(param, true);
        return;

    case 9: // get CSD
        Host->SendResponse(*(u32*)&CSD[12], false);
        Host->SendResponse(*(u32*)&CSD[8], false);
        Host->SendResponse(*(u32*)&CSD[4], false);
        Host->SendResponse(*(u32*)&CSD[0], true);
        return;

    case 12: // stop operation
        SetState(0x04);
        if (File) fflush(File);
        RWCommand = 0;
        Host->SendResponse(CSR, true);
        return;

    case 13: // get status
        Host->SendResponse(CSR, true);
        return;

    case 16: // set block size
        BlockSize = param;
        if (BlockSize > 0x200)
        {
            // TODO! raise error
            printf("!! SD/MMC: BAD BLOCK LEN %d\n", BlockSize);
            BlockSize = 0x200;
        }
        SetState(0x04); // CHECKME
        Host->SendResponse(CSR, true);
        return;

    case 18: // read multiple blocks
        //printf("READ_MULTIPLE_BLOCKS addr=%08X size=%08X\n", param, BlockSize);
        RWAddress = param;
        if (OCR & (1<<30))
        {
            RWAddress <<= 9;
            BlockSize = 512;
        }
        RWCommand = 18;
        Host->SendResponse(CSR, true);
        RWAddress += ReadBlock(RWAddress);
        SetState(0x05);
        return;

    case 25: // write multiple blocks
        //printf("WRITE_MULTIPLE_BLOCKS addr=%08X size=%08X\n", param, BlockSize);
        RWAddress = param;
        if (OCR & (1<<30))
        {
            RWAddress <<= 9;
            BlockSize = 512;
        }
        RWCommand = 25;
        Host->SendResponse(CSR, true);
        RWAddress += WriteBlock(RWAddress);
        SetState(0x04);
        return;

    case 55: // appcmd prefix
        CSR |= (1<<5);
        Host->SendResponse(CSR, true);
        return;
    }

    printf("MMC: unknown CMD %d %08X\n", cmd, param);
}

void DSi_MMCStorage::SendACMD(u8 cmd, u32 param)
{
    switch (cmd)
    {
    case 6: // set bus width (TODO?)
        //printf("SET BUS WIDTH %08X\n", param);
        Host->SendResponse(CSR, true);
        return;

    case 13: // get SSR
        Host->SendResponse(CSR, true);
        Host->DataRX(SSR, 64);
        return;

    case 41: // set operating conditions
        // CHECKME:
        // DSi boot2 sets this to 0x40100000 (hardcoded)
        // then has two codepaths depending on whether bit30 did get set
        // is it settable at all on the MMC? probably not.
        if (Internal) param &= ~(1<<30);
        OCR &= 0xBF000000;
        OCR |= (param & 0x40FFFFFF);
        Host->SendResponse(OCR, true);
        SetState(0x01);
        return;

    case 42: // ???
        Host->SendResponse(CSR, true);
        return;

    case 51: // get SCR
        Host->SendResponse(CSR, true);
        Host->DataRX(SCR, 8);
        return;
    }

    printf("MMC: unknown ACMD %d %08X\n", cmd, param);
}

void DSi_MMCStorage::ContinueTransfer()
{
    if (RWCommand == 0) return;

    u32 len = 0;

    switch (RWCommand)
    {
    case 18:
        len = ReadBlock(RWAddress);
        break;

    case 25:
        len = WriteBlock(RWAddress);
        break;
    }

    RWAddress += len;
}

u32 DSi_MMCStorage::ReadBlock(u64 addr)
{
    u32 len = BlockSize;
    len = Host->GetTransferrableLen(len);

    u8 data[0x200];
    if (File)
    {
        fseek(File, addr, SEEK_SET);
        fread(data, 1, len, File);
    }

    return Host->DataRX(data, len);
}

u32 DSi_MMCStorage::WriteBlock(u64 addr)
{
    u32 len = BlockSize;
    len = Host->GetTransferrableLen(len);

    u8 data[0x200];
    if ((len = Host->DataTX(data, len)))
    {
        if (File)
        {
            fseek(File, addr, SEEK_SET);
            fwrite(data, 1, len, File);
        }
    }

    return len;
}
