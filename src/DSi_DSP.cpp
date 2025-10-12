/*
    Copyright 2020 PoroCYon

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

#include "teakra/include/teakra/teakra.h"
#include "DSP_HLE/AACUcode.h"
#include "DSP_HLE/G711Ucode.h"
#include "DSP_HLE/GraphicsUcode.h"

#include "DSi.h"
#include "DSi_DSP.h"
#include "FIFO.h"
#include "NDS.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;


const u32 DSi_DSP::DataMemoryOffset = 0x20000; // from Teakra memory_interface.h
// NOTE: ^ IS IN DSP WORDS, NOT IN BYTES!


DSi_DSP::DSi_DSP(melonDS::DSi& dsi) : DSi(dsi)
{
    DSi.RegisterEventFuncs(Event_DSi_DSP, this, {MakeEventThunk(DSi_DSP, DSPCatchUpU32)});

    DSPCore = nullptr;
    SCFG_RST = false;

    //PDATAReadFifo = new FIFO<u16>(16);
    //PDATAWriteFifo = new FIFO<u16>(16);
}

DSi_DSP::~DSi_DSP()
{
    //if (PDATAWriteFifo) delete PDATAWriteFifo;
    StopDSP();

    //PDATAReadFifo = NULL;
    //PDATAWriteFifo = NULL;

    DSi.UnregisterEventFuncs(Event_DSi_DSP);
}

void DSi_DSP::Reset()
{
    DSPTimestamp = 0;

    DSP_PADR = 0;
    DSP_PCFG = 0;
    DSP_PSTS = 0;
    DSP_PSEM = 0;
    DSP_PMASK = 0xff;
    DSP_PCLEAR = 0;
    DSP_CMD[2] = DSP_CMD[1] = DSP_CMD[0] = 0;
    DSP_REP[2] = DSP_REP[1] = DSP_REP[0] = 0;
    PDataDMALen = 0;

    PDATAReadFifo.Clear();
    //PDATAWriteFifo->Clear();
    if (DSPHLE)
        StopDSP();
    else if (DSPCore)
        DSPCore->Reset();

    DSi.CancelEvent(Event_DSi_DSP);
}

void DSi_DSP::DoSavestate(Savestate* file)
{
    file->Section("DSPi");

    PDATAReadFifo.DoSavestate(file);

    file->Var64(&DSPTimestamp);
    file->Var32((u32*)&PDataDMALen);

    file->Var16(&DSP_PADR);
    file->Var16(&DSP_PCFG);
    file->Var16(&DSP_PSTS);
    file->Var16(&DSP_PSEM);
    file->Var16(&DSP_PMASK);
    file->Var16(&DSP_PCLEAR);
    file->Var16(&DSP_CMD[0]);
    file->Var16(&DSP_CMD[1]);
    file->Var16(&DSP_CMD[2]);
    file->Var16(&DSP_REP[0]);
    file->Var16(&DSP_REP[1]);
    file->Var16(&DSP_REP[2]);
    file->Var8((u8*)&SCFG_RST);

    if (DSPHLE)
    {
        // for DSP HLE, handle the specific core as needed

        if (file->Saving)
        {
            u32 id = DSPCore ? DSPCore->GetID() : 0xFFFFFFFF;
            file->Var32(&id);
        }
        else
        {
            u32 id;
            file->Var32(&id);

            StopDSP();

            if (id == Teakra::ID)
            {
                StartDSPLLE();
            }
            else
            {
                u32 uclass = id >> 16;
                int uver = (int)(s16)(id & 0xFFFF);
                switch (uclass)
                {
                case DSP_HLE::UcodeBase::Class_AAC:
                    DSPCore = new DSP_HLE::AACUcode(DSi, uver);
                    break;

                case DSP_HLE::UcodeBase::Class_G711:
                    DSPCore = new DSP_HLE::G711Ucode(DSi, uver);
                    break;

                case DSP_HLE::UcodeBase::Class_Graphics:
                    DSPCore = new DSP_HLE::GraphicsUcode(DSi, uver);
                    break;
                }
            }
        }
    }

    if (DSPCore)
        DSPCore->DoSavestate(file);
}

void DSi_DSP::SetDSPHLE(bool hle)
{
    StopDSP();
    DSPHLE = hle;
    if (!DSPHLE)
        StartDSPLLE();
}


void DSi_DSP::StartDSPHLE()
{
    u32 crc = 0;

    // Hash NWRAM B, which contains the DSP program
    // The hash should be in the DSP's memory view
    for (u32 addr = 0; addr < 0x40000; addr += 0x8000)
    {
        const u8* ptr = DSi.NWRAMMap_B[2][(addr >> 15) & 0x7];
        crc = CRC32(ptr, 0x8000, crc);
    }

    Log(LogLevel::Info, "DSP_HLE: CRC = %08X\n", crc);

    switch (crc)
    {
    case 0x7867C94B: // DSi sound app AAC ucode
        DSPCore = new DSP_HLE::AACUcode(DSi, -1);
        break;

    case 0x0CAFEF48: // AAC SDK ucode v0
        DSPCore = new DSP_HLE::AACUcode(DSi, 0x00);
        break;

    case 0xEF5174AA: // AAC SDK ucode v0 patch
        DSPCore = new DSP_HLE::AACUcode(DSi, 0x01);
        break;

    case 0x1D320185: // AAC SDK ucode v2
        DSPCore = new DSP_HLE::AACUcode(DSi, 0x20);
        break;

    case 0xAE11D2FB: // AAC SDK ucode v4
        DSPCore = new DSP_HLE::AACUcode(DSi, 0x40);
        break;


    case 0xFAA1B612: // G711 SDK ucode v0
        DSPCore = new DSP_HLE::G711Ucode(DSi, 0x00);
        break;

    case 0x7EEE19FE: // G711 SDK ucode v1
        DSPCore = new DSP_HLE::G711Ucode(DSi, 0x10);
        break;

    case 0x6056C6FF: // G711 SDK ucode v2
        DSPCore = new DSP_HLE::G711Ucode(DSi, 0x20);
        break;

    case 0x2C281DAE: // G711 SDK ucode v3
        DSPCore = new DSP_HLE::G711Ucode(DSi, 0x30);
        break;

    case 0x2A1D7F94: // G711 SDK ucode v4
        DSPCore = new DSP_HLE::G711Ucode(DSi, 0x40);
        break;

    case 0x4EBEB519: // G711 SDK ucode v5
        DSPCore = new DSP_HLE::G711Ucode(DSi, 0x50);
        break;


    case 0xCD2A8B1B: // Graphics SDK ucode v0
        DSPCore = new DSP_HLE::GraphicsUcode(DSi, 0x00);
        break;

    case 0x7323B75B: // Graphics SDK ucode v1
        DSPCore = new DSP_HLE::GraphicsUcode(DSi, 0x10);
        break;

    case 0xBD4B63B6: // Graphics SDK ucode v1 patch
        DSPCore = new DSP_HLE::GraphicsUcode(DSi, 0x11);
        break;

    case 0x448BB6A2: // Graphics SDK ucode v2
        DSPCore = new DSP_HLE::GraphicsUcode(DSi, 0x20);
        break;

    case 0x63CAEC33: // Graphics SDK ucode v3
        DSPCore = new DSP_HLE::GraphicsUcode(DSi, 0x30);
        break;

    case 0x1451EB84: // Graphics SDK ucode v4
        DSPCore = new DSP_HLE::GraphicsUcode(DSi, 0x40);
        break;

    case 0x2C974FC8: // Graphics SDK ucode v5
        DSPCore = new DSP_HLE::GraphicsUcode(DSi, 0x50);
        break;

    default:
        Log(LogLevel::Info, "DSP_HLE: unknown ucode, falling back to Teakra\n");
        StartDSPLLE();
        break;
    }

    if (DSPCore)
    {
        DSPCore->Reset();
        DSPCore->Start();
    }
}

void DSi_DSP::StopDSP()
{
    if (DSPCore) delete DSPCore;
    DSPCore = nullptr;
}

void DSi_DSP::StartDSPLLE()
{
    auto teakra = new Teakra::Teakra();
    DSPCore = teakra;

    using namespace std::placeholders;

    teakra->SetRecvDataHandler(0, std::bind(&DSi_DSP::IrqRep0, this));
    teakra->SetRecvDataHandler(1, std::bind(&DSi_DSP::IrqRep1, this));
    teakra->SetRecvDataHandler(2, std::bind(&DSi_DSP::IrqRep2, this));

    teakra->SetSemaphoreHandler(std::bind(&DSi_DSP::IrqSem, this));

    Teakra::SharedMemoryCallback smcb;
    smcb.read16 = std::bind(&DSi_DSP::DSPRead16, this, _1);
    smcb.write16 = std::bind(&DSi_DSP::DSPWrite16, this, _1, _2);
    teakra->SetSharedMemoryCallback(smcb);

    // these happen instantaneously and without too much regard for bus aribtration
    // rules, so, this might have to be changed later on
    Teakra::AHBMCallback cb;
    cb.read8 = [this](auto addr) { return DSi.ARM9Read8(addr); };
    cb.write8 = [this](auto addr, auto val) { DSi.ARM9Write8(addr, val); };
    cb.read16 = [this](auto addr) { return DSi.ARM9Read16(addr); };
    cb.write16 = [this](auto addr, auto val) { DSi.ARM9Write16(addr, val); };
    cb.read32 = [this](auto addr) { return DSi.ARM9Read32(addr); };
    cb.write32 = [this](auto addr, auto val) { DSi.ARM9Write32(addr, val); };
    teakra->SetAHBMCallback(cb);

    teakra->SetMicEnableCallback([this](bool enable)
     {
         if (enable)
             DSi.Mic.Start(Mic_DSi_DSP);
         else
             DSi.Mic.Stop(Mic_DSi_DSP);
     });
}


void DSi_DSP::IrqRep0()
{
    if (DSP_PCFG & (1<< 9)) DSi.SetIRQ(0, IRQ_DSi_DSP);
}
void DSi_DSP::IrqRep1()
{
    if (DSP_PCFG & (1<<10)) DSi.SetIRQ(0, IRQ_DSi_DSP);
}
void DSi_DSP::IrqRep2()
{
    if (DSP_PCFG & (1<<11)) DSi.SetIRQ(0, IRQ_DSi_DSP);
}
void DSi_DSP::IrqSem()
{
    DSP_PSTS |= 1<<9;
    // apparently these are always fired?
    DSi.SetIRQ(0, IRQ_DSi_DSP);
}

u16 DSi_DSP::DSPRead16(u32 addr)
{
    if (!(addr & 0x40000))
    {
        u8* ptr = DSi.NWRAMMap_B[2][(addr >> 15) & 0x7];
        return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
    }
    else
    {
        u8* ptr = DSi.NWRAMMap_C[2][(addr >> 15) & 0x7];
        return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
    }
}

void DSi_DSP::DSPWrite16(u32 addr, u16 val)
{
    // TODO: does the rule for overlapping NWRAM slots also apply to the DSP side?

    if (!(addr & 0x40000))
    {
        u8* ptr = DSi.NWRAMMap_B[2][(addr >> 15) & 0x7];
        if (ptr) *(u16*)&ptr[addr & 0x7FFF] = val;
    }
    else
    {
        u8* ptr = DSi.NWRAMMap_C[2][(addr >> 15) & 0x7];
        if (ptr) *(u16*)&ptr[addr & 0x7FFF] = val;
    }
}

void DSi_DSP::SampleClock(s16 output[2], s16 input)
{
    if (DSPCore)
    {
        DSPCore->SampleClock(output, input);
    }
    else
    {
        output[0] = 0;
        output[1] = 0;
    }
}


bool DSi_DSP::IsRstReleased() const
{
    return SCFG_RST;
}

void DSi_DSP::SetRstLine(bool release)
{
    SCFG_RST = release;
    Reset();
    DSPTimestamp = DSi.ARM9Timestamp; // only start now!
}

inline bool DSi_DSP::IsDSPCoreEnabled() const
{
    return (DSi.SCFG_Clock9 & (1<<1)) && SCFG_RST && (!(DSP_PCFG & (1<<0)));
}

inline bool DSi_DSP::IsDSPIOEnabled() const
{
    return (DSi.SCFG_Clock9 & (1<<1)) && SCFG_RST;
}


u16 DSi_DSP::GetPSTS() const
{
    u16 r = DSP_PSTS & (1<<9); // this is the only sticky bit
    //r &= ~((1<<2)|(1<<7)); // we support instant resets and wrfifo xfers
    r |= (1<<8); // write fifo is always empty (inf. speed)

    if ( PDATAReadFifo.IsFull ()) r |= 1<<5;
    if (!PDATAReadFifo.IsEmpty()) r |=(1<<6)|(1<<0);

    if (DSPCore)
    {
        if (!DSPCore->SendDataIsEmpty(0)) r |= 1 << 13;
        if (!DSPCore->SendDataIsEmpty(1)) r |= 1 << 14;
        if (!DSPCore->SendDataIsEmpty(2)) r |= 1 << 15;
        if  (DSPCore->RecvDataIsReady(0)) r |= 1 << 10;
        if  (DSPCore->RecvDataIsReady(1)) r |= 1 << 11;
        if  (DSPCore->RecvDataIsReady(2)) r |= 1 << 12;
    }

    return r;
}


void DSi_DSP::PDataDMAWrite(u16 wrval)
{
    u32 addr = DSP_PADR;

    if (DSPCore)
    {
        switch (DSP_PCFG & (7<<12)) // memory region select
        {
        case 0<<12: // data
            addr |= (u32)DSPCore->DMAChan0GetDstHigh() << 16;
            DSPCore->DataWriteA32(addr, wrval);
            break;
        case 1<<12: // mmio
            DSPCore->MMIOWrite(addr & 0x7FF, wrval);
            break;
        case 5<<12: // program
            addr |= (u32)DSPCore->DMAChan0GetDstHigh() << 16;
            DSPCore->ProgramWrite(addr, wrval);
            break;
        case 7<<12:
            addr |= (u32)DSPCore->DMAChan0GetDstHigh() << 16;
            // only do stuff when AHBM is configured correctly
            if (DSPCore->AHBMGetDmaChannel(0) == 0 && DSPCore->AHBMGetDirection(0) == 1/*W*/)
            {
                switch (DSPCore->AHBMGetUnitSize(0))
                {
                case 0: /* 8bit */      DSi.ARM9Write8 (addr, (u8)wrval); break;
                case 1: /* 16 b */ DSPCore->AHBMWrite16(addr,     wrval); break;
                    // does it work like this, or should it first buffer two u16's
                    // until it has enough data to write to the actual destination?
                    // -> this seems to be correct behavior!
                case 2: /* 32 b */ DSPCore->AHBMWrite32(addr,     wrval); break;
                }
            }
            break;
        default: return;
        }
    }

    if (DSP_PCFG & (1<<1)) // auto-increment
        ++DSP_PADR; // overflows and stays within a 64k 'page' // TODO: is this +1 or +2?

    DSi.SetIRQ(0, IRQ_DSi_DSP); // wrfifo empty
}

// TODO: FIFO interrupts! (rd full, nonempty)
u16 DSi_DSP::PDataDMARead()
{
    u16 r = 0;
    u32 addr = DSP_PADR;

    if (DSPCore)
    {
        switch (DSP_PCFG & (7<<12)) // memory region select
        {
        case 0<<12: // data
            addr |= (u32)DSPCore->DMAChan0GetDstHigh() << 16;
            r = DSPCore->DataReadA32(addr);
            break;
        case 1<<12: // mmio
            r = DSPCore->MMIORead(addr & 0x7FF);
            break;
        case 5<<12: // program
            addr |= (u32)DSPCore->DMAChan0GetDstHigh() << 16;
            r = DSPCore->ProgramRead(addr);
            break;
        case 7<<12:
            addr |= (u32)DSPCore->DMAChan0GetDstHigh() << 16;
            // only do stuff when AHBM is configured correctly
            if (DSPCore->AHBMGetDmaChannel(0) == 0 && DSPCore->AHBMGetDirection(0) == 0/*R*/)
            {
                switch (DSPCore->AHBMGetUnitSize(0))
                {
                case 0: /* 8bit */ r =           DSi.ARM9Read8 (addr); break;
                case 1: /* 16 b */ r =      DSPCore->AHBMRead16(addr); break;
                case 2: /* 32 b */ r = (u16)DSPCore->AHBMRead32(addr); break;
                }
            }
            break;
        default: return r;
        }
    }

    if (DSP_PCFG & (1<<1)) // auto-increment
        ++DSP_PADR; // overflows and stays within a 64k 'page' // TODO: is this +1 or +2?

    return r;
}
void DSi_DSP::PDataDMAFetch()
{
    if (!PDataDMALen) return;

    PDATAReadFifo.Write(PDataDMARead());

    if (PDataDMALen > 0) --PDataDMALen;
}
void DSi_DSP::PDataDMAStart()
{
    switch ((DSP_PCFG & (3<<2)) >> 2)
    {
    case 0: PDataDMALen = 1; break;
    case 1: PDataDMALen = 8; break;
    case 2: PDataDMALen =16; break;
    case 3: PDataDMALen =-1; break;
    }

    // fill a single fifo
    int amt = PDataDMALen;
    if (amt < 0) amt = 16;
    for (int i = 0; i < amt; ++i)
        PDataDMAFetch();

    DSi.SetIRQ(0, IRQ_DSi_DSP);

}
void DSi_DSP::PDataDMACancel()
{
    PDataDMALen = 0;
    PDATAReadFifo.Clear();

}
u16 DSi_DSP::PDataDMAReadMMIO()
{
    u16 ret = 0; // TODO: is this actually 0, or just open bus?

    if (!PDATAReadFifo.IsEmpty())
        ret = PDATAReadFifo.Read();

    // aha, there's more to come
    if (PDataDMALen != 0)
    {
        int left = 16 - PDATAReadFifo.Level();
        if (PDataDMALen > 0 && PDataDMALen < left)
            left = PDataDMALen;

        for (int i = 0; i < left; ++i)
            PDataDMAFetch();
    }

    // TODO only trigger IRQ if enabled!!
    if (!PDATAReadFifo.IsEmpty() || PDATAReadFifo.IsFull())
        DSi.SetIRQ(0, IRQ_DSi_DSP);

    return ret;
}


u8 DSi_DSP::Read8(u32 addr)
{
    DSPCatchUp();

    addr &= 0x3F; // mirroring wheee

    // ports are a bit weird, 16-bit regs in 32-bit spaces
    switch (addr)
    {
    // no 8-bit PDATA read
    // no DSP_PADR read
    case 0x08: return DSP_PCFG & 0xFF;
    case 0x09: return DSP_PCFG >> 8;
    case 0x0C: return GetPSTS() & 0xFF;
    case 0x0D: return GetPSTS() >> 8;
    case 0x10: return DSP_PSEM & 0xFF;
    case 0x11: return DSP_PSEM >> 8;
    case 0x14: return DSP_PMASK & 0xFF;
    case 0x15: return DSP_PMASK >> 8;
    // no DSP_PCLEAR read
    case 0x1C:
        if (!DSPCore) return 0;
        return DSPCore->GetSemaphore() & 0xFF; // SEM
    case 0x1D:
        if (!DSPCore) return 0;
        return DSPCore->GetSemaphore() >> 8;
    }

    return 0;
}

u16 DSi_DSP::Read16(u32 addr)
{
    DSPCatchUp();

    addr &= 0x3E; // mirroring wheee

    // ports are a bit weird, 16-bit regs in 32-bit spaces
    switch (addr)
    {
    case 0x00: return PDataDMAReadMMIO();
    // no DSP_PADR read
    case 0x08: return DSP_PCFG;
    case 0x0C: return GetPSTS();
    case 0x10: return DSP_PSEM;
    case 0x14: return DSP_PMASK;
    // no DSP_PCLEAR read
    case 0x1C:
        if (!DSPCore) return 0;
        return DSPCore->GetSemaphore(); // SEM

    case 0x20: return DSP_CMD[0];
    case 0x28: return DSP_CMD[1];
    case 0x30: return DSP_CMD[2];

    case 0x24:
        {
            if (!DSPCore) return 0;
            u16 r = DSPCore->RecvData(0);
            return r;
        }
    case 0x2C:
        {
            if (!DSPCore) return 0;
            u16 r = DSPCore->RecvData(1);
            return r;
        }
    case 0x34:
        {
            if (!DSPCore) return 0;
            u16 r = DSPCore->RecvData(2);
            return r;
        }
    }

    return 0;
}

u32 DSi_DSP::Read32(u32 addr)
{
    addr &= 0x3C;
    return Read16(addr); // *shrug* (doesn't do anything unintended due to the
                         // 4byte spacing between regs while they're all 16bit)
}

void DSi_DSP::Write8(u32 addr, u8 val)
{
    DSPCatchUp();

    addr &= 0x3F;
    switch (addr)
    {
    // no 8-bit PDATA or PADR writes
    case 0x08:
        DSP_PCFG = (DSP_PCFG & 0xFF00) | (val << 0);
        break;
    case 0x09:
        DSP_PCFG = (DSP_PCFG & 0x00FF) | (val << 8);
        break;
    // no PSTS writes
    // no 8-bit semaphore writes
    // no 8-bit CMDx writes
    // no REPx writes
    }
}

void DSi_DSP::Write16(u32 addr, u16 val)
{
    DSPCatchUp();

    addr &= 0x3E;
    switch (addr)
    {
    case 0x00: PDataDMAWrite(val); break;
    case 0x04: DSP_PADR = val; break;

    case 0x08:
        if ((DSP_PCFG & (1<<0)) && (!(val & (1<<0))))
        {
            if (DSPHLE)
                StartDSPHLE();
        }
        else if ((!(DSP_PCFG & (1<<0))) && (val & (1<<0)))
        {
            if (DSPHLE)
                StopDSP();
            else if (DSPCore)
                DSPCore->Reset();
        }
        DSP_PCFG = val;
        if (DSP_PCFG & (1<<4))
            PDataDMAStart();
        else
            PDataDMACancel();
        break;
    // no PSTS writes
    case 0x10:
        DSP_PSEM = val;
        if (DSPCore)
            DSPCore->SetSemaphore(val);
        break;
    case 0x14:
        DSP_PMASK = val;
        if (DSPCore)
            DSPCore->MaskSemaphore(val);
        break;
    case 0x18: // PCLEAR
        if (DSPCore)
        {
            DSPCore->ClearSemaphore(val);
            if (DSPCore->GetSemaphore() == 0)
                DSP_PSTS &= ~(1<<9);
        }
        else
            DSP_PSTS &= ~(1<<9);
        break;
    // SEM not writable

    case 0x20: // CMD0
        DSP_CMD[0] = val;
        if (DSPCore)
            DSPCore->SendData(0, val);
        break;
    case 0x28: // CMD1
        DSP_CMD[1] = val;
        if (DSPCore)
            DSPCore->SendData(1, val);
        break;
    case 0x30: // CMD2
        DSP_CMD[2] = val;
        if (DSPCore)
            DSPCore->SendData(2, val);
        break;

    // no REPx writes
    }
}

void DSi_DSP::Write32(u32 addr, u32 val)
{
    addr &= 0x3C;
    Write16(addr, val & 0xFFFF);
}


bool DSi_DSP::DSPCatchUp()
{
    if (!IsDSPCoreEnabled())
    {
        // nothing to do, but advance the current time so that we don't do an
        // unreasonable amount of cycles when rst is released
        if (DSPTimestamp < DSi.ARM9Timestamp)
            DSPTimestamp = DSi.ARM9Timestamp;

        return false;
    }

    u64 curtime = DSi.ARM9Timestamp;

    if (DSPTimestamp >= curtime) return true; // ummmm?!

    u64 backlog = curtime - DSPTimestamp;

    while (backlog & (1uLL<<32)) // god I hope this never happens
    {
        Run((u32)(backlog & ((1uLL<<32)-1)));
        backlog = curtime - DSPTimestamp;
    }
    Run((u32)backlog);

    return true;
}
void DSi_DSP::DSPCatchUpU32(u32 _) { DSPCatchUp(); }

void DSi_DSP::Run(u32 cycles)
{
    if (!IsDSPCoreEnabled())
    {
        DSPTimestamp += cycles;
        return;
    }

    if (DSPCore)
        DSPCore->Run(cycles);

    DSPTimestamp += cycles;

    DSi.CancelEvent(Event_DSi_DSP);
    DSi.ScheduleEvent(Event_DSi_DSP, false, 4096, 0, 0);
}

}