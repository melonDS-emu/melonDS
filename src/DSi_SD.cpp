/*
    Copyright 2016-2019 Arisotura

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
#include "Platform.h"


#define SD_DESC  (Num?"SDIO":"SD/MMC")


DSi_SDHost::DSi_SDHost(u32 num)
{
    Num = num;

    DataFIFO[0] = new FIFO<u16>(0x100);
    DataFIFO[1] = new FIFO<u16>(0x100);

    Ports[0] = NULL;
    Ports[1] = NULL;
}

DSi_SDHost::~DSi_SDHost()
{
    delete DataFIFO[0];
    delete DataFIFO[1];

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

    DataFIFO[0]->Clear();
    DataFIFO[1]->Clear();
    CurFIFO = 0;

    IRQStatus = 0;
    IRQMask = 0x8B7F031D;

    DataCtl = 0;
    Data32IRQ = 0;
    DataMode = 0;
    BlockCount16 = 0; BlockCount32 = 0; BlockCountInternal = 0;
    BlockLen16 = 0;   BlockLen32 = 0;
    StopAction = 0;

    if (Ports[0]) delete Ports[0];
    if (Ports[1]) delete Ports[1];
    Ports[0] = NULL;
    Ports[1] = NULL;

    if (Num == 0)
    {
        DSi_MMCStorage* mmc = new DSi_MMCStorage(this, true, "nand.bin");
        mmc->SetCID(DSi::eMMC_CID);

        // TODO: port 0 (SD)
        Ports[1] = mmc;
    }
    else
    {
        // TODO: SDIO (wifi)
    }
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
    if (IRQStatus & (1<<24)) Data32IRQ |= (1<<8);
    if (!(IRQStatus & (1<<25))) Data32IRQ |= (1<<9);

    u32 newflags = ((Data32IRQ >> 8) & 0x1) | (((~Data32IRQ) >> 8) & 0x2);
    newflags &= (Data32IRQ >> 11);

    if ((oldflags == 0) && (newflags != 0))
        NDS::SetIRQ2(Num ? NDS::IRQ2_DSi_SDIO : NDS::IRQ2_DSi_SDMMC);
}

void DSi_SDHost::ClearIRQ(u32 irq)
{
    IRQStatus &= ~(1<<irq);

    if (irq == 24 || irq == 25) UpdateData32IRQ();
}

void DSi_SDHost::SetIRQ(u32 irq)
{
    u32 oldflags = IRQStatus & ~IRQMask;

    IRQStatus |= (1<<irq);
    u32 newflags = IRQStatus & ~IRQMask;

    if ((oldflags == 0) && (newflags != 0))
        NDS::SetIRQ2(Num ? NDS::IRQ2_DSi_SDIO : NDS::IRQ2_DSi_SDMMC);

    if (irq == 24 || irq == 25) UpdateData32IRQ();
}

void DSi_SDHost::SendResponse(u32 val, bool last)
{
    *(u32*)&ResponseBuffer[6] = *(u32*)&ResponseBuffer[4];
    *(u32*)&ResponseBuffer[4] = *(u32*)&ResponseBuffer[2];
    *(u32*)&ResponseBuffer[2] = *(u32*)&ResponseBuffer[0];
    *(u32*)&ResponseBuffer[0] = val;

    if (last) SetIRQ(0);
}

void DSi_SDHost::FinishSend(u32 param)
{
    DSi_SDHost* host = (param & 0x1) ? DSi::SDIO : DSi::SDMMC;

    host->CurFIFO ^= 1;

    host->ClearIRQ(25);
    host->SetIRQ(24);
    if (param & 0x2) host->SetIRQ(2);
}

void DSi_SDHost::SendData(u8* data, u32 len)
{
    printf("%s: data RX, len=%d, blkcnt=%d blklen=%d, irq=%08X\n", SD_DESC, len, BlockCount16, BlockLen16, IRQMask);
    if (len != BlockLen16) printf("!! BAD BLOCKLEN\n");

    bool last = (BlockCountInternal == 0);

    u32 f = CurFIFO ^ 1;
    for (u32 i = 0; i < len; i += 2)
        DataFIFO[f]->Write(*(u16*)&data[i]);

    //CurFIFO = f;
    //SetIRQ(24);
    // TODO: determine what the delay should be!
    // for now, this is a placeholder
    // we need a delay because DSi boot2 will send a command and then wait for IRQ0
    // but if IRQ24 is thrown instantly, the handler clears IRQ0 before the
    // send-command function starts polling IRQ status
    u32 param = Num | (last << 1);
    NDS::ScheduleEvent(NDS::Event_DSi_SDTransfer, false, 512, FinishSend, param);
}


u16 DSi_SDHost::Read(u32 addr)
{
    //printf("SDMMC READ %08X %08X\n", addr, NDS::GetPC(1));

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

    case 0x01C: return (IRQStatus & 0x031D) | 0x0030; // TODO: adjust insert flags for SD card
    case 0x01E: return ((IRQStatus >> 16) & 0x8B7F);
    case 0x020: return IRQMask & 0x031D;
    case 0x022: return (IRQMask >> 16) & 0x8B7F;

    case 0x024: return SDClock;
    case 0x026: return BlockLen16;
    case 0x028: return SDOption;

    case 0x030: // FIFO16
        {
            // TODO: decrement BlockLen????

            u32 f = CurFIFO;
            if (DataFIFO[f]->IsEmpty())
            {
                // TODO
                return 0;
            }

            DSi_SDDevice* dev = Ports[PortSelect & 0x1];
            u16 ret = DataFIFO[f]->Read();

            if (DataFIFO[f]->IsEmpty())
            {
                ClearIRQ(24);

                if (BlockCountInternal == 0)
                {
                    printf("%s: data RX complete", SD_DESC);

                    if (StopAction & (1<<8))
                    {
                        printf(", sending CMD12");
                        if (dev) dev->SendCMD(12, 0);
                    }

                    printf("\n");

                    // CHECKME: presumably IRQ2 should not trigger here, but rather
                    // when the data transfer is done
                    //SetIRQ(0);
                    //SetIRQ(2);
                }
                else
                {
                    BlockCountInternal--;

                    if (dev) dev->ContinueTransfer();
                }

                SetIRQ(25);
            }

            return ret;
        }

    case 0x0D8: return DataCtl;

    case 0x0E0: return SoftReset;

    case 0x100: return Data32IRQ;
    case 0x104: return BlockLen32;
    case 0x108: return BlockCount32;
    }

    printf("unknown %s read %08X\n", SD_DESC, addr);
    return 0;
}

u32 DSi_SDHost::ReadFIFO32()
{
    if (DataMode != 1) return 0;

    // TODO: decrement BlockLen????

    u32 f = CurFIFO;
    if (DataFIFO[f]->IsEmpty())
    {
        // TODO
        return 0;
    }

    DSi_SDDevice* dev = Ports[PortSelect & 0x1];
    u32 ret = DataFIFO[f]->Read();
    ret |= (DataFIFO[f]->Read() << 16);

    if (DataFIFO[f]->IsEmpty())
    {
        ClearIRQ(24);

        if (BlockCountInternal == 0)
        {
            printf("%s: data32 RX complete", SD_DESC);

            if (StopAction & (1<<8))
            {
                printf(", sending CMD12");
                if (dev) dev->SendCMD(12, 0);
            }

            printf("\n");

            // CHECKME: presumably IRQ2 should not trigger here, but rather
            // when the data transfer is done
            //SetIRQ(0);
            //SetIRQ(2);
        }
        else
        {
            BlockCountInternal--;

            if (dev) dev->ContinueTransfer();
        }

        SetIRQ(25);
    }

    return ret;
}

void DSi_SDHost::Write(u32 addr, u16 val)
{
    //printf("SDMMC WRITE %08X %04X %08X\n", addr, val, NDS::GetPC(1));

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
        }
        return;

    case 0x002: PortSelect = val; printf("%s: PORT SELECT %04X\n", SD_DESC, val); return;
    case 0x004: Param = (Param & 0xFFFF0000) | val; return;
    case 0x006: Param = (Param & 0x0000FFFF) | (val << 16); return;

    case 0x008: StopAction = val & 0x0101; return;
    case 0x00A: BlockCount16 = val; BlockCountInternal = val; printf("%s: BLOCK COUNT %d\n", SD_DESC, val); return;

    case 0x01C: IRQStatus &= (val | 0xFFFF0000); return;
    case 0x01E: IRQStatus &= ((val << 16) | 0xFFFF); return;
    case 0x020: IRQMask = (IRQMask & 0x8B7F0000) | (val & 0x031D); return;
    case 0x022: IRQMask = (IRQMask & 0x0000031D) | ((val & 0x8B7F) << 16); return;

    case 0x024: SDClock = val & 0x03FF; return;
    case 0x026:
        BlockLen16 = val & 0x03FF;
        if (BlockLen16 > 0x200) BlockLen16 = 0x200;
        return;
    case 0x028: SDOption = val & 0xC1FF; return;

    case 0x0D8:
        DataCtl = (val & 0x0022);
        DataMode = ((DataCtl >> 1) & 0x1) & ((Data32IRQ >> 1) & 0x1);
        printf("%s: data mode %d-bit\n", SD_DESC, DataMode?32:16);
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
        }
        SoftReset = 0x0006 | (val & 0x0001);
        return;

    case 0x100:
        Data32IRQ = (val & 0x1802) | (Data32IRQ & 0x0300);
        if (val & (1<<10))
        {
            // kind of hacky
            u32 f = CurFIFO;
            DataFIFO[f]->Clear();
        }
        DataMode = ((DataCtl >> 1) & 0x1) & ((Data32IRQ >> 1) & 0x1);
        printf("%s: data mode %d-bit\n", SD_DESC, DataMode?32:16);
        return;
    case 0x104: BlockLen32 = val & 0x03FF; return;
    case 0x108: BlockCount32 = val; return;
    }

    printf("unknown %s write %08X %04X\n", SD_DESC, addr, val);
}

void DSi_SDHost::WriteFIFO32(u32 val)
{
    //
}


DSi_MMCStorage::DSi_MMCStorage(DSi_SDHost* host, bool internal, const char* path) : DSi_SDDevice(host)
{
    Internal = internal;
    strncpy(FilePath, path, 1023); FilePath[1023] = '\0';

    File = Platform::OpenLocalFile(path, "r+b");

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

DSi_MMCStorage::~DSi_MMCStorage()
{
    if (File) fclose(File);
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

    case 2:
    case 10: // get CID
        Host->SendResponse(*(u32*)&CID[12], false);
        Host->SendResponse(*(u32*)&CID[8], false);
        Host->SendResponse(*(u32*)&CID[4], false);
        Host->SendResponse(*(u32*)&CID[0], true);
        //if (cmd == 2) SetState(0x02);
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
        }
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
        // TODO
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
        Host->SendResponse(CSR, true);
        return;

    case 18: // read multiple blocks
        printf("READ_MULTIPLE_BLOCKS addr=%08X size=%08X\n", param, BlockSize);
        RWAddress = param;
        RWCommand = 18;
        Host->SendResponse(CSR, true);
        ReadBlock(RWAddress);
        RWAddress += BlockSize;
        return;

    case 55: // ??
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
        printf("SET BUS WIDTH %08X\n", param);
        Host->SendResponse(CSR, true);
        return;

    case 13: // get SSR
        Host->SendResponse(CSR, true);
        Host->SendData(SSR, 64);
        return;

    case 41: // set operating conditions
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
        Host->SendData(SCR, 8);
        return;
    }

    printf("MMC: unknown ACMD %d %08X\n", cmd, param);
}

void DSi_MMCStorage::ContinueTransfer()
{
    ReadBlock(RWAddress);
    RWAddress += BlockSize;
}

void DSi_MMCStorage::ReadBlock(u32 addr)
{
    if (!File) return;

    printf("SD/MMC: reading block @ %08X, len=%08X\n", addr, BlockSize);

    u8 data[0x200];
    fseek(File, addr, SEEK_SET); // TODO: adjust for SDHC/etc
    fread(data, 1, BlockSize, File);
    Host->SendData(data, BlockSize);
}
