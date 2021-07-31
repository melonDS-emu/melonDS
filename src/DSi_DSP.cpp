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

#include "DSi.h"
#include "DSi_DSP.h"
#include "FIFO.h"
#include "NDS.h"


namespace DSi_DSP
{

Teakra::Teakra* TeakraCore;

bool SCFG_RST;

u16 DSP_PADR;
u16 DSP_PCFG;
u16 DSP_PSTS;
u16 DSP_PSEM;
u16 DSP_PMASK;
u16 DSP_PCLEAR;
u16 DSP_CMD[3];
u16 DSP_REP[3];

u64 DSPTimestamp;

FIFO<u16, 16> PDATAReadFifo/*, *PDATAWriteFifo*/;
int PDataDMALen = 0;

constexpr u32 DataMemoryOffset = 0x20000; // from Teakra memory_interface.h
// NOTE: ^ IS IN DSP WORDS, NOT IN BYTES!

u16 GetPSTS()
{
    u16 r = DSP_PSTS & (1<<9); // this is the only sticky bit
    //r &= ~((1<<2)|(1<<7)); // we support instant resets and wrfifo xfers
    r |= (1<<8); // write fifo is always empty (inf. speed)

    if ( PDATAReadFifo.IsFull ()) r |= 1<<5;
    if (!PDATAReadFifo.IsEmpty()) r |=(1<<6)|(1<<0);

    if (!TeakraCore->SendDataIsEmpty(0)) r |= 1<<13;
    if (!TeakraCore->SendDataIsEmpty(1)) r |= 1<<14;
    if (!TeakraCore->SendDataIsEmpty(2)) r |= 1<<15;
    if ( TeakraCore->RecvDataIsReady(0)) r |= 1<<10;
    if ( TeakraCore->RecvDataIsReady(1)) r |= 1<<11;
    if ( TeakraCore->RecvDataIsReady(2)) r |= 1<<12;

    return r;
}

void IrqRep0()
{
    if (DSP_PCFG & (1<< 9)) NDS::SetIRQ(0, NDS::IRQ_DSi_DSP);
}
void IrqRep1()
{
    if (DSP_PCFG & (1<<10)) NDS::SetIRQ(0, NDS::IRQ_DSi_DSP);
}
void IrqRep2()
{
    if (DSP_PCFG & (1<<11)) NDS::SetIRQ(0, NDS::IRQ_DSi_DSP);
}
void IrqSem()
{
    DSP_PSTS |= 1<<9;
    // apparently these are always fired?
    NDS::SetIRQ(0, NDS::IRQ_DSi_DSP);
}

void AudioCb(std::array<s16, 2> frame)
{
    // TODO
}

bool Init()
{
    TeakraCore = new Teakra::Teakra();
    SCFG_RST = false;

    if (!TeakraCore) return false;

    TeakraCore->SetRecvDataHandler(0, IrqRep0);
    TeakraCore->SetRecvDataHandler(1, IrqRep1);
    TeakraCore->SetRecvDataHandler(2, IrqRep2);

    TeakraCore->SetSemaphoreHandler(IrqSem);

    // these happen instantaneously and without too much regard for bus aribtration
    // rules, so, this might have to be changed later on
    Teakra::AHBMCallback cb;
    cb.read8 = DSi::ARM9Read8;
    cb.write8 = DSi::ARM9Write8;
    cb.read16 = DSi::ARM9Read16;
    cb.write16 = DSi::ARM9Write16;
    cb.read32 = DSi::ARM9Read32;
    cb.write32 = DSi::ARM9Write32;
    TeakraCore->SetAHBMCallback(cb);

    TeakraCore->SetAudioCallback(AudioCb);

    //PDATAReadFifo = new FIFO<u16>(16);
    //PDATAWriteFifo = new FIFO<u16>(16);

    return true;
}
void DeInit()
{
    //if (PDATAWriteFifo) delete PDATAWriteFifo;
    if (TeakraCore) delete TeakraCore;

    //PDATAReadFifo = NULL;
    //PDATAWriteFifo = NULL;
    TeakraCore = NULL;
}

void Reset()
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
    TeakraCore->Reset();

    NDS::CancelEvent(NDS::Event_DSi_DSP);
}

bool IsRstReleased()
{
    return SCFG_RST;
}
void SetRstLine(bool release)
{
    SCFG_RST = release;
    Reset();
    DSPTimestamp = NDS::ARM9Timestamp; // only start now!
}

void OnMBKCfg(char bank, u32 slot, u8 oldcfg, u8 newcfg, u8* nwrambacking)
{
    if (bank != 'B' && bank != 'C')
    {
        printf("WTF?? (DSP MBK recfg, nonsense bank '%c')\n", bank);
        return;
    }

    bool olddsp = (oldcfg & 3) >= 2, // was mapped to the DSP
         newdsp = (newcfg & 3) >= 2; // will be mapped to the DSP

    // nothing changes
    if (olddsp == newdsp)
        return;

    const u8* src;
    u8* dst;

    if (newdsp)
    {
        // need to map stuff to DSP memory (== Teakra-owned memory) from NWRAM
        src = nwrambacking;
        dst = &TeakraCore->GetDspMemory()[((newcfg >> 2) & 7) << 15];

        if (bank == 'C') // C: DSP data (B: DSP code)
            dst += DataMemoryOffset*2;
    }
    else //if (olddsp)
    {
        // it was mapped to the DSP, but now we have to copy it out, back to NWRAM
        src = &TeakraCore->GetDspMemory()[((oldcfg >> 2) & 7) << 15];
        dst = nwrambacking;

        if (bank == 'C') // C: DSP data (B: DSP code)
            src += DataMemoryOffset*2;
    }

    memcpy(dst, src, 1<<15); // 1 full slot
}

inline bool IsDSPCoreEnabled()
{
    return (DSi::SCFG_Clock9 & (1<<1)) && SCFG_RST && (DSP_PCFG & (1<<0));
}

bool DSPCatchUp()
{
    //asm volatile("int3");
    if (!IsDSPCoreEnabled())
    {
        // nothing to do, but advance the current time so that we don't do an
        // unreasonable amount of cycles when rst is released
        if (DSPTimestamp < NDS::ARM9Timestamp)
            DSPTimestamp = NDS::ARM9Timestamp;

        return false;
    }

    u64 curtime = NDS::ARM9Timestamp;

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
void DSPCatchUpU32(u32 _) { DSPCatchUp(); }

void PDataDMAWrite(u16 wrval)
{
    u32 addr = DSP_PADR;

    switch (DSP_PCFG & (7<<12)) // memory region select
    {
    case 0<<12: // data
        addr |= (u32)TeakraCore->DMAChan0GetDstHigh() << 16;
        TeakraCore->DataWriteA32(addr, wrval);
        break;
    case 1<<12: // mmio
        TeakraCore->MMIOWrite(addr & 0x7FF, wrval);
        break;
    case 5<<12: // program
        addr |= (u32)TeakraCore->DMAChan0GetDstHigh() << 16;
        TeakraCore->ProgramWrite(addr, wrval);
        break;
    case 7<<12:
        addr |= (u32)TeakraCore->DMAChan0GetDstHigh() << 16;
        // only do stuff when AHBM is configured correctly
        if (TeakraCore->AHBMGetDmaChannel(0) == 0 && TeakraCore->AHBMGetDirection(0) == 1/*W*/)
        {
            switch (TeakraCore->AHBMGetUnitSize(0))
            {
            case 0: /* 8bit */        DSi::ARM9Write8 (addr, (u8)wrval); break;
            case 1: /* 16 b */ TeakraCore->AHBMWrite16(addr,     wrval); break;
            // does it work like this, or should it first buffer two u16's
            // until it has enough data to write to the actual destination?
            // -> this seems to be correct behavior!
            case 2: /* 32 b */ TeakraCore->AHBMWrite32(addr,     wrval); break;
            }
        }
        break;
    default: return;
    }

    if (DSP_PCFG & (1<<1)) // auto-increment
        ++DSP_PADR; // overflows and stays within a 64k 'page' // TODO: is this +1 or +2?

    NDS::SetIRQ(0, NDS::IRQ_DSi_DSP); // wrfifo empty
}
// TODO: FIFO interrupts! (rd full, nonempty)
u16 PDataDMARead()
{
    u16 r = 0;
    u32 addr = DSP_PADR;
    switch (DSP_PCFG & (7<<12)) // memory region select
    {
    case 0<<12: // data
        addr |= (u32)TeakraCore->DMAChan0GetDstHigh() << 16;
        r = TeakraCore->DataReadA32(addr);
        break;
    case 1<<12: // mmio
        r = TeakraCore->MMIORead(addr & 0x7FF);
        break;
    case 5<<12: // program
        addr |= (u32)TeakraCore->DMAChan0GetDstHigh() << 16;
        r = TeakraCore->ProgramRead(addr);
        break;
    case 7<<12:
        addr |= (u32)TeakraCore->DMAChan0GetDstHigh() << 16;
        // only do stuff when AHBM is configured correctly
        if (TeakraCore->AHBMGetDmaChannel(0) == 0 && TeakraCore->AHBMGetDirection(0) == 0/*R*/)
        {
            switch (TeakraCore->AHBMGetUnitSize(0))
            {
            case 0: /* 8bit */ r =             DSi::ARM9Read8 (addr); break;
            case 1: /* 16 b */ r =      TeakraCore->AHBMRead16(addr); break;
            case 2: /* 32 b */ r = (u16)TeakraCore->AHBMRead32(addr); break;
            }
        }
        break;
    default: return r;
    }

    if (DSP_PCFG & (1<<1)) // auto-increment
        ++DSP_PADR; // overflows and stays within a 64k 'page' // TODO: is this +1 or +2?

    return r;
}
void PDataDMAFetch()
{
    if (!PDataDMALen) return;

    PDATAReadFifo.Write(PDataDMARead());

    if (PDataDMALen > 0) --PDataDMALen;
}
void PDataDMAStart()
{
    switch ((DSP_PSTS & (3<<2)) >> 2)
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

    NDS::SetIRQ(0, NDS::IRQ_DSi_DSP);

}
void PDataDMACancel()
{
    PDataDMALen = 0;
    PDATAReadFifo.Clear();

}
u16 PDataDMAReadMMIO()
{
    u16 ret;

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

        ret = PDATAReadFifo.Read();
    }
    else
    {
        // ah, crap
        ret = 0; // TODO: is this actually 0, or just open bus?
    }

    if (!PDATAReadFifo.IsEmpty() || PDATAReadFifo.IsFull())
        NDS::SetIRQ(0, NDS::IRQ_DSi_DSP);

    return ret;
}

u8 Read8(u32 addr)
{
    if (!(DSi::SCFG_EXT[0] & (1<<18)))
        return 0;

    if (!DSPCatchUp()) return 0;

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
    case 0x1C: return TeakraCore->GetSemaphore() & 0xFF; // SEM
    case 0x1D: return TeakraCore->GetSemaphore() >> 8;
    }

    return 0;
}
u16 Read16(u32 addr)
{
    if (!(DSi::SCFG_EXT[0] & (1<<18)))
        return 0;

    if (!DSPCatchUp()) return 0;

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
    case 0x1C: return TeakraCore->GetSemaphore(); // SEM

    case 0x20: return DSP_CMD[0];
    case 0x28: return DSP_CMD[1];
    case 0x30: return DSP_CMD[2];

    case 0x24:
        {
            u16 r = TeakraCore->RecvData(0);
            return r;
        }
    case 0x2C:
        {
            u16 r = TeakraCore->RecvData(1);
            return r;
        }
    case 0x34:
        {
            u16 r = TeakraCore->RecvData(2);
            return r;
        }
    }

    return 0;
}
u32 Read32(u32 addr)
{
    if (!(DSi::SCFG_EXT[0] & (1<<18))) return 0;

    addr &= 0x3C;
    return Read16(addr); // *shrug* (doesn't do anything unintended due to the
                         // 4byte spacing between regs while they're all 16bit)
}

void Write8(u32 addr, u8 val)
{
    if (!(DSi::SCFG_EXT[0] & (1<<18))) return;

    if (!DSPCatchUp()) return;

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
void Write16(u32 addr, u16 val)
{
    if (!(DSi::SCFG_EXT[0] & (1<<18))) return;

    if (!DSPCatchUp()) return;

    addr &= 0x3E;
    switch (addr)
    {
    case 0x00: PDataDMAWrite(val); break;
    case 0x04: DSP_PADR = val; break;

    case 0x08:
        DSP_PCFG = val;
        if (DSP_PCFG & (1<<4))
            PDataDMAStart();
        else
            PDataDMACancel();
        break;
    // no PSTS writes
    case 0x10:
        DSP_PSEM = val;
        TeakraCore->SetSemaphore(val);
        break;
    case 0x14:
        DSP_PMASK = val;
        TeakraCore->MaskSemaphore(val);
        break;
    case 0x18: // PCLEAR
        TeakraCore->ClearSemaphore(val);
        if (TeakraCore->GetSemaphore() == 0)
            DSP_PSTS &= ~(1<<9);

        break;
    // SEM not writable

    case 0x20: // CMD0
        DSP_CMD[0] = val;
        TeakraCore->SendData(0, val);
        break;
    case 0x28: // CMD1
        DSP_CMD[1] = val;
        TeakraCore->SendData(1, val);
        break;
    case 0x30: // CMD2
        DSP_CMD[2] = val;
        TeakraCore->SendData(2, val);
        break;

    // no REPx writes
    }
}

void Write32(u32 addr, u32 val)
{
    if (!(DSi::SCFG_EXT[0] & (1<<18))) return;

    addr &= 0x3C;
    Write16(addr, val & 0xFFFF);
}

void Run(u32 cycles)
{
    if (!IsDSPCoreEnabled())
    {
        DSPTimestamp += cycles;
        return;
    }

    TeakraCore->Run(cycles);

    DSPTimestamp += cycles;

    NDS::ScheduleEvent(NDS::Event_DSi_DSP, false,
            16384/*from citra (TeakraSlice)*/, DSPCatchUpU32, 0);
}

void DoSavestate(Savestate* file)
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
}

}

