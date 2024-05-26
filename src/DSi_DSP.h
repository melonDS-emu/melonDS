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

// TODO: for actual sound output
// * audio callbacks

namespace Teakra { class Teakra; }

namespace melonDS
{
class DSi;
class DSi_DSP
{
public:
    DSi_DSP(melonDS::DSi& dsi);
    ~DSi_DSP();
    void Reset();
    void DoSavestate(Savestate* file);

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

    u16 ReadSNDExCnt() const { return SNDExCnt; }
    void WriteSNDExCnt(u16 val, u16 mask);

    // NOTE: checks SCFG_CLK9
    void Run(u32 cycles);

    void IrqRep0();
    void IrqRep1();
    void IrqRep2();
    void IrqSem();
    u16 DSPRead16(u32 addr);
    void DSPWrite16(u32 addr, u16 val);
    void AudioCb(std::array<s16, 2> frame);

private:
    melonDS::DSi& DSi;
    // not sure whether to not rather put it somewhere else
    u16 SNDExCnt;

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

