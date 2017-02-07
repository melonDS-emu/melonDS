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

#ifndef GPU_H
#define GPU_H

#include "GPU2D.h"
#include "GPU3D.h"

namespace GPU
{

extern u16 VCount;

extern u16 DispStat[2];

extern u8 VRAMCNT[9];
extern u8 VRAMSTAT;

extern u8 Palette[2*1024];
extern u8 OAM[2*1024];

extern u8* VRAM[9];
extern u8* VRAM_ABG[128];
extern u8* VRAM_AOBJ[128];
extern u8* VRAM_BBG[128];
extern u8* VRAM_BOBJ[128];
extern u8* VRAM_LCD[128];
extern u8* VRAM_ARM7[2];

extern u8* VRAM_ABGExtPal[4];
extern u8* VRAM_AOBJExtPal;
extern u8* VRAM_BBGExtPal[4];
extern u8* VRAM_BOBJExtPal;

extern u16 Framebuffer[256*192*2];

extern GPU2D* GPU2D_A;
extern GPU2D* GPU2D_B;


bool Init();
void DeInit();
void Reset();

void MapVRAM_AB(u32 bank, u8 cnt);
void MapVRAM_CD(u32 bank, u8 cnt);
void MapVRAM_E(u32 bank, u8 cnt);
void MapVRAM_FG(u32 bank, u8 cnt);
void MapVRAM_H(u32 bank, u8 cnt);
void MapVRAM_I(u32 bank, u8 cnt);

void DisplaySwap(u32 val);

void StartFrame();
void StartScanline(u32 line);

void SetDispStat(u32 cpu, u16 val);

}

#endif
