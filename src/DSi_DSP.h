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

namespace DSi_DSP
{

extern u16 SNDExCnt;

extern u16 DSP_PDATA;
extern u16 DSP_PADR;
extern u16 DSP_PCFG;
extern u16 DSP_PSTS;
extern u16 DSP_PSEM;
extern u16 DSP_PMASK;
extern u16 DSP_PCLEAR;
extern u16 DSP_SEM;
extern u16 DSP_CMD[3];
extern u16 DSP_REP[3];

bool Init();
void DeInit();
void Reset();

void DoSavestate(Savestate* file);

void DSPCatchUpU32(u32 _);

// SCFG_RST bit0
bool IsRstReleased();
void SetRstLine(bool release);

// DSP_* regs (0x040043xx) (NOTE: checks SCFG_EXT)
u8 Read8(u32 addr);
void Write8(u32 addr, u8 val);

u16 Read16(u32 addr);
void Write16(u32 addr, u16 val);

u32 Read32(u32 addr);
void Write32(u32 addr, u32 val);

void WriteSNDExCnt(u16 val);

// NOTE: checks SCFG_CLK9
void Run(u32 cycles);

}

#endif // DSI_DSP_H

