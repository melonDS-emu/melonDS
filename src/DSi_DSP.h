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

#ifndef DSI_DSP_H
#define DSI_DSP_H

#include "types.h"
#include "Savestate.h"
#include "FIFO.h"

// TODO: for actual sound output
// * audio callbacks

namespace Teakra { class Teakra; }

namespace melonDS
{

class DSi;
class DSPHLE_UcodeBase;

class DSPInterface
{
public:
    virtual ~DSPInterface() {};

    virtual void Reset() = 0;
    virtual void DoSavestate(Savestate* file) = 0;

    virtual u32 GetID() = 0;

    // APBP Data
    virtual bool SendDataIsEmpty(std::uint8_t index) const = 0;
    virtual void SendData(std::uint8_t index, std::uint16_t value) = 0;
    virtual bool RecvDataIsReady(std::uint8_t index) const = 0;
    virtual std::uint16_t RecvData(std::uint8_t index) = 0;
    //virtual std::uint16_t PeekRecvData(std::uint8_t index) = 0;

    // APBP Semaphore
    virtual void SetSemaphore(std::uint16_t value) = 0;
    virtual void ClearSemaphore(std::uint16_t value) = 0;
    virtual void MaskSemaphore(std::uint16_t value) = 0;
    virtual std::uint16_t GetSemaphore() const = 0;

    // for implementing DSP_PDATA/PADR DMA transfers
    virtual std::uint16_t ProgramRead(std::uint32_t address) const = 0;
    virtual void ProgramWrite(std::uint32_t address, std::uint16_t value) = 0;
    //virtual std::uint16_t DataRead(std::uint16_t address, bool bypass_mmio = false) = 0;
    //virtual void DataWrite(std::uint16_t address, std::uint16_t value, bool bypass_mmio = false) = 0;
    virtual std::uint16_t DataReadA32(std::uint32_t address) const = 0;
    virtual void DataWriteA32(std::uint32_t address, std::uint16_t value) = 0;
    virtual std::uint16_t MMIORead(std::uint16_t address) = 0;
    virtual void MMIOWrite(std::uint16_t address, std::uint16_t value) = 0;

    // DSP_PADR is only 16-bit, so this is where the DMA interface gets the
    // upper 16-bits from
    virtual std::uint16_t DMAChan0GetSrcHigh() = 0;
    virtual std::uint16_t DMAChan0GetDstHigh() = 0;

    virtual std::uint16_t AHBMGetUnitSize(std::uint16_t i) const = 0;
    virtual std::uint16_t AHBMGetDirection(std::uint16_t i) const = 0;
    virtual std::uint16_t AHBMGetDmaChannel(std::uint16_t i) const = 0;
    // we need these as AHBM does some weird stuff on unaligned accesses internally
    virtual std::uint16_t AHBMRead16(std::uint32_t addr) = 0;
    virtual void AHBMWrite16(std::uint32_t addr, std::uint16_t value) = 0;
    virtual std::uint16_t AHBMRead32(std::uint32_t addr) = 0;
    virtual void AHBMWrite32(std::uint32_t addr, std::uint32_t value) = 0;

    // core
    virtual void Start() {};
    virtual void Run(unsigned cycle) {};

    virtual void SampleClock(s16 output[2], s16 input) = 0;
};

class DSi_DSP
{
public:
    DSi_DSP(melonDS::DSi& dsi);
    ~DSi_DSP();
    void Reset();
    void DoSavestate(Savestate* file);

    bool GetDSPHLE() { return DSPHLE; }
    void SetDSPHLE(bool hle);

    void StartDSPLLE();
    void StartDSPHLE();
    void StopDSP();

    void DSPCatchUpU32(u32 _);

    // SCFG_RST bit0
    bool IsRstReleased() const;
    void SetRstLine(bool release);

    // DSP_* regs (0x040043xx) (NOTE: checks SCFG_EXT)
    u8 Read8(u32 addr);
    void Write8(u32 addr, u8 val);

    u16 Read16(u32 addr);
    void Write16(u32 addr, u16 val);

    u32 Read32(u32 addr);
    void Write32(u32 addr, u32 val);

    // NOTE: checks SCFG_CLK9
    void Run(u32 cycles);

    void IrqRep0();
    void IrqRep1();
    void IrqRep2();
    void IrqSem();
    u16 DSPRead16(u32 addr);
    void DSPWrite16(u32 addr, u16 val);

    void SampleClock(s16 output[2], s16 input);

private:
    melonDS::DSi& DSi;

    bool DSPHLE;
    DSPInterface* DSPCore;

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
    int PDataDMALen;

    static const u32 DataMemoryOffset;

    u16 GetPSTS() const;

    inline bool IsDSPCoreEnabled() const;
    inline bool IsDSPIOEnabled() const;

    bool DSPCatchUp();

    void PDataDMAWrite(u16 wrval);
    u16 PDataDMARead();
    void PDataDMAFetch();
    void PDataDMAStart();
    void PDataDMACancel();
    u16 PDataDMAReadMMIO();
};

}
#endif // DSI_DSP_H

