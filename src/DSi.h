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

#ifndef DSI_H
#define DSI_H

#include "NDS.h"
#include "DSi_SD.h"

namespace DSi_NAND
{
    class NANDImage;
}

namespace DSi
{

extern u16 SCFG_BIOS;
extern u16 SCFG_Clock9;
extern u32 SCFG_EXT[2];


extern u8 ARM9iBIOS[0x10000];
extern u8 ARM7iBIOS[0x10000];

extern std::unique_ptr<DSi_NAND::NANDImage> NANDImage;
extern DSi_SDHost* SDMMC;
extern DSi_SDHost* SDIO;

const u32 NWRAMSize = 0x40000;

extern u8* NWRAM_A;
extern u8* NWRAM_B;
extern u8* NWRAM_C;

extern u8* NWRAMMap_A[2][4];
extern u8* NWRAMMap_B[3][8];
extern u8* NWRAMMap_C[3][8];

extern u32 NWRAMStart[2][3];
extern u32 NWRAMEnd[2][3];
extern u32 NWRAMMask[2][3];

bool Init();
void DeInit();
void Reset();
void Stop();

void DoSavestate(Savestate* file);

void SetCartInserted(bool inserted);

void SetupDirectBoot();
void SoftReset();

bool LoadNAND();

void RunNDMAs(u32 cpu);
void StallNDMAs();
bool NDMAsInMode(u32 cpu, u32 mode);
bool NDMAsRunning(u32 cpu);
void CheckNDMAs(u32 cpu, u32 mode);
void StopNDMAs(u32 cpu, u32 mode);

void MapNWRAM_A(u32 num, u8 val);
void MapNWRAM_B(u32 num, u8 val);
void MapNWRAM_C(u32 num, u8 val);
void MapNWRAMRange(u32 cpu, u32 num, u32 val);

u8 ARM9Read8(u32 addr);
u16 ARM9Read16(u32 addr);
u32 ARM9Read32(u32 addr);
void ARM9Write8(u32 addr, u8 val);
void ARM9Write16(u32 addr, u16 val);
void ARM9Write32(u32 addr, u32 val);

bool ARM9GetMemRegion(u32 addr, bool write, NDS::MemRegion* region);

u8 ARM7Read8(u32 addr);
u16 ARM7Read16(u32 addr);
u32 ARM7Read32(u32 addr);
void ARM7Write8(u32 addr, u8 val);
void ARM7Write16(u32 addr, u16 val);
void ARM7Write32(u32 addr, u32 val);

bool ARM7GetMemRegion(u32 addr, bool write, NDS::MemRegion* region);

u8 ARM9IORead8(u32 addr);
u16 ARM9IORead16(u32 addr);
u32 ARM9IORead32(u32 addr);
void ARM9IOWrite8(u32 addr, u8 val);
void ARM9IOWrite16(u32 addr, u16 val);
void ARM9IOWrite32(u32 addr, u32 val);

u8 ARM7IORead8(u32 addr);
u16 ARM7IORead16(u32 addr);
u32 ARM7IORead32(u32 addr);
void ARM7IOWrite8(u32 addr, u8 val);
void ARM7IOWrite16(u32 addr, u16 val);
void ARM7IOWrite32(u32 addr, u32 val);

}

#endif // DSI_H
