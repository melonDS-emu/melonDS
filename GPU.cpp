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
#include "GPU.h"


namespace GPU
{

#define LINE_CYCLES  (355*6 * 2)
#define FRAME_CYCLES  (LINE_CYCLES * 263)

u16 VCount;

u16 DispStat[2], VMatch[2];

u8 Palette[2*1024];
u8 OAM[2*1024];

u8 VRAM_A[128*1024];
u8 VRAM_B[128*1024];
u8 VRAM_C[128*1024];
u8 VRAM_D[128*1024];
u8 VRAM_E[ 64*1024];
u8 VRAM_F[ 16*1024];
u8 VRAM_G[ 16*1024];
u8 VRAM_H[ 32*1024];
u8 VRAM_I[ 16*1024];
u8* VRAM[9] = {VRAM_A, VRAM_B, VRAM_C, VRAM_D, VRAM_E, VRAM_F, VRAM_G, VRAM_H, VRAM_I};

u8 VRAMCNT[9];
u8 VRAMSTAT;

u8* VRAM_ABG[128];
u8* VRAM_AOBJ[128];
u8* VRAM_BBG[128];
u8* VRAM_BOBJ[128];
u8* VRAM_LCD[128];
u8* VRAM_ARM7[2];

u16 Framebuffer[256*192*2];

GPU2D* GPU2D_A;
GPU2D* GPU2D_B;


void Init()
{
    GPU2D_A = new GPU2D(0);
    GPU2D_B = new GPU2D(1);
}

void Reset()
{
    VCount = 0;

    DispStat[0] = 0;
    DispStat[1] = 0;
    VMatch[0] = 0;
    VMatch[1] = 0;

    memset(Palette, 0, 2*1024);
    memset(OAM, 0, 2*1024);

    memset(VRAM_A, 0, 128*1024);
    memset(VRAM_B, 0, 128*1024);
    memset(VRAM_C, 0, 128*1024);
    memset(VRAM_D, 0, 128*1024);
    memset(VRAM_E, 0,  64*1024);
    memset(VRAM_F, 0,  16*1024);
    memset(VRAM_G, 0,  16*1024);
    memset(VRAM_H, 0,  32*1024);
    memset(VRAM_I, 0,  16*1024);

    memset(VRAMCNT, 0, 9);
    VRAMSTAT = 0;

    memset(VRAM_ABG, 0, sizeof(u8*)*128);
    memset(VRAM_AOBJ, 0, sizeof(u8*)*128);
    memset(VRAM_BBG, 0, sizeof(u8*)*128);
    memset(VRAM_BOBJ, 0, sizeof(u8*)*128);
    memset(VRAM_LCD, 0, sizeof(u8*)*128);
    memset(VRAM_ARM7, 0, sizeof(u8*)*2);

    for (int i = 0; i < 256*192*2; i++)
    {
        Framebuffer[i] = 0x7FFF;
    }

    GPU2D_A->Reset();
    GPU2D_B->Reset();

    GPU2D_A->SetFramebuffer(&Framebuffer[256*0]);
    GPU2D_B->SetFramebuffer(&Framebuffer[256*192]);
}


// VRAM mapping shit.
// TODO eventually: work out priority orders in case of overlaps. there _are_ games that map overlapping banks.

void MapVRAM_AB(u32 bank, u8 cnt)
{
    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u8 oldofs = (oldcnt >> 3) & 0x3;
    u8 ofs = (cnt >> 3) & 0x3;

    u8* vram = VRAM[bank];

    if (oldcnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (oldcnt & 0x3)
        {
        case 0:
            vrammap = &VRAM_LCD[bank<<3];
            break;

        case 1:
            vrammap = &VRAM_ABG[oldofs<<3];
            break;

        case 2:
            oldofs &= 0x1;
            vrammap = &VRAM_AOBJ[oldofs<<3];
            break;

        case 3:
            // not mapped to memory
            break;
        }

        if (vrammap)
        {
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap   = NULL;
        }
    }

    if (cnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (cnt & 0x3)
        {
        case 0:
            vrammap = &VRAM_LCD[bank<<3];
            break;

        case 1:
            vrammap = &VRAM_ABG[ofs<<3];
            break;

        case 2:
            ofs &= 0x1;
            vrammap = &VRAM_AOBJ[ofs<<3];
            break;

        case 3:
            // not mapped to memory
            break;
        }

        if (vrammap)
        {
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap   = vram;
        }
    }
}

void MapVRAM_CD(u32 bank, u8 cnt)
{
    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    VRAMSTAT &= ~(1 << (bank-2));

    if (oldcnt == cnt) return;

    u8 oldofs = (oldcnt >> 3) & 0x7;
    u8 ofs = (cnt >> 3) & 0x7;

    u8* vram = VRAM[bank];

    if (oldcnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (oldcnt & 0x7)
        {
        case 0:
            vrammap = &VRAM_LCD[bank<<3];
            break;

        case 1:
            vrammap = &VRAM_ABG[oldofs<<3];
            break;

        case 2:
            oldofs &= 0x1;
            VRAM_ARM7[oldofs] = NULL;
            break;

        case 3:
            // not mapped to memory
            break;

        case 4:
            if (bank == 2)
                vrammap = &VRAM_BBG[0];
            else
                vrammap = &VRAM_BOBJ[0];
            break;
        }

        if (vrammap)
        {
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap   = NULL;
        }
    }

    if (cnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (cnt & 0x7)
        {
        case 0:
            vrammap = &VRAM_LCD[bank<<3];
            break;

        case 1:
            vrammap = &VRAM_ABG[ofs<<3];
            break;

        case 2:
            ofs &= 0x1;
            VRAM_ARM7[ofs] = vram;
            VRAMSTAT |= (1 << (bank-2));
            break;

        case 3:
            // not mapped to memory
            break;

        case 4:
            if (bank == 2)
                vrammap = &VRAM_BBG[0];
            else
                vrammap = &VRAM_BOBJ[0];
            break;
        }

        if (vrammap)
        {
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap++ = vram; vram += 0x4000;
            *vrammap   = vram;
        }
    }
}

void MapVRAM_E(u32 bank, u8 cnt)
{
    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u8 oldofs = (oldcnt >> 3) & 0x7;
    u8 ofs = (cnt >> 3) & 0x7;

    u8* vram = VRAM[bank];

    if (oldcnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (oldcnt & 0x7)
        {
        case 0:
            VRAM_LCD[0x20] = NULL;
            VRAM_LCD[0x21] = NULL;
            VRAM_LCD[0x22] = NULL;
            VRAM_LCD[0x23] = NULL;
            break;

        case 1:
            vrammap = &VRAM_ABG[0];
            break;

        case 2:
            vrammap = &VRAM_AOBJ[0];
            break;

        case 3:
            // not mapped to memory
            break;

        case 4:
            // BG EXTPAL -- TODO
            break;

        case 5:
            // OBJ EXTPAL -- TODO
            break;
        }

        if (vrammap)
        {
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap   = NULL;
        }
    }

    if (cnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (cnt & 0x7)
        {
        case 0:
            VRAM_LCD[0x20] = &vram[0x0000];
            VRAM_LCD[0x21] = &vram[0x4000];
            VRAM_LCD[0x22] = &vram[0x8000];
            VRAM_LCD[0x23] = &vram[0xC000];
            break;

        case 1:
            vrammap = &VRAM_ABG[0];
            break;

        case 2:
            vrammap = &VRAM_AOBJ[0];
            break;

        case 3:
            // not mapped to memory
            break;

        case 4:
            // BG EXTPAL -- TODO
            break;

        case 5:
            // OBJ EXTPAL -- TODO
            break;
        }

        if (vrammap)
        {
            *vrammap++ = &vram[0x0000];
            *vrammap++ = &vram[0x4000];
            *vrammap++ = &vram[0x8000];
            *vrammap++ = &vram[0xC000];
            *vrammap++ = &vram[0x0000];
            *vrammap++ = &vram[0x4000];
            *vrammap++ = &vram[0x8000];
            *vrammap   = &vram[0xC000];
        }
    }
}

void MapVRAM_FG(u32 bank, u8 cnt)
{
    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u8 oldofs = (oldcnt >> 3) & 0x7;
    u8 ofs = (cnt >> 3) & 0x7;

    u8* vram = VRAM[bank];
    bank -= 5;

    if (oldcnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (oldcnt & 0x7)
        {
        case 0:
            VRAM_LCD[0x24 + bank] = NULL;
            break;

        case 1:
            vrammap = &VRAM_ABG[(oldofs & 0x1) | ((oldofs & 0x2) << 1)];
            break;

        case 2:
            vrammap = &VRAM_AOBJ[(oldofs & 0x1) | ((oldofs & 0x2) << 1)];
            break;

        case 3:
            // not mapped to memory
            break;

        case 4:
            // BG EXTPAL TODO
            break;

        case 5:
            // OBJ EXTPAL TODO
            break;
        }

        if (vrammap)
        {
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap   = NULL;
        }
    }

    if (cnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (cnt & 0x7)
        {
        case 0:
            VRAM_LCD[0x24 + bank] = vram;
            break;

        case 1:
            vrammap = &VRAM_ABG[(ofs & 0x1) | ((ofs & 0x2) << 1)];
            break;

        case 2:
            vrammap = &VRAM_AOBJ[(ofs & 0x1) | ((ofs & 0x2) << 1)];
            break;

        case 3:
            // not mapped to memory
            break;

        case 4:
            // BG EXTPAL TODO
            break;

        case 5:
            // OBJ EXTPAL TODO
            break;
        }

        if (vrammap)
        {
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap   = vram;
        }
    }
}

void MapVRAM_H(u32 bank, u8 cnt)
{
    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u8 oldofs = (oldcnt >> 3) & 0x7;
    u8 ofs = (cnt >> 3) & 0x7;

    u8* vram = VRAM[bank];

    if (oldcnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (oldcnt & 0x3)
        {
        case 0:
            VRAM_LCD[0x26] = NULL;
            VRAM_LCD[0x27] = NULL;
            break;

        case 1:
            vrammap = &VRAM_BBG[0x00];
            break;

        case 2:
            // BG EXTPAL TODO
            break;
        }

        if (vrammap)
        {
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap   = NULL;
        }
    }

    if (cnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (cnt & 0x3)
        {
        case 0:
            VRAM_LCD[0x26] = &vram[0x0000];
            VRAM_LCD[0x27] = &vram[0x4000];
            break;

        case 1:
            vrammap = &VRAM_BBG[0x00];
            break;

        case 2:
            // BG EXTPAL TODO
            break;
        }

        if (vrammap)
        {
            *vrammap++ = &vram[0x0000];
            *vrammap++ = &vram[0x4000];
            *vrammap++ = &vram[0x0000];
            *vrammap++ = &vram[0x4000];
            *vrammap++ = &vram[0x0000];
            *vrammap++ = &vram[0x4000];
            *vrammap++ = &vram[0x0000];
            *vrammap   = &vram[0x4000];
        }
    }
}

void MapVRAM_I(u32 bank, u8 cnt)
{
    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u8 oldofs = (oldcnt >> 3) & 0x7;
    u8 ofs = (cnt >> 3) & 0x7;

    u8* vram = VRAM[bank];
    bank -= 5;

    if (oldcnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (oldcnt & 0x3)
        {
        case 0:
            VRAM_LCD[0x28] = NULL;
            break;

        case 1:
            vrammap = &VRAM_BBG[0x02];
            break;

        case 2:
            vrammap = &VRAM_BOBJ[0x00];
            break;

        case 3:
            // not mapped to memory
            break;
        }

        if (vrammap)
        {
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap++ = NULL;
            *vrammap   = NULL;
        }
    }

    if (cnt & (1<<7))
    {
        u8** vrammap = NULL;

        switch (cnt & 0x3)
        {
        case 0:
            VRAM_LCD[0x28] = vram;
            break;

        case 1:
            vrammap = &VRAM_BBG[0x02];
            break;

        case 2:
            vrammap = &VRAM_BOBJ[0x00];
            break;

        case 3:
            // not mapped to memory
            break;
        }

        if (vrammap)
        {
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap++ = vram;
            *vrammap   = vram;
        }
    }
}


void StartFrame()
{
    StartScanline(0);
}

void StartScanline(u32 line)
{
    VCount = line;

    if (line == VMatch[0])
    {
        DispStat[0] |= (1<<2);

        if (DispStat[0] & (1<<5)) NDS::TriggerIRQ(0, NDS::IRQ_VCount);
    }
    else
        DispStat[0] &= ~(1<<2);

    if (line == VMatch[1])
    {
        DispStat[1] |= (1<<2);

        if (DispStat[1] & (1<<5)) NDS::TriggerIRQ(1, NDS::IRQ_VCount);
    }
    else
        DispStat[1] &= ~(1<<2);

    if (line < 192)
    {
        // draw
        GPU2D_A->DrawScanline(line);
        GPU2D_B->DrawScanline(line);

        NDS::ScheduleEvent(LINE_CYCLES, StartScanline, line+1);
    }
    else if (line == 262)
    {
        // frame end

        DispStat[0] &= ~(1<<0);
        DispStat[1] &= ~(1<<0);
    }
    else
    {
        if (line == 192)
        {
            // VBlank
            DispStat[0] |= (1<<0);
            DispStat[1] |= (1<<0);

            if (DispStat[0] & (1<<3)) NDS::TriggerIRQ(0, NDS::IRQ_VBlank);
            if (DispStat[1] & (1<<3)) NDS::TriggerIRQ(1, NDS::IRQ_VBlank);
        }

        NDS::ScheduleEvent(LINE_CYCLES, StartScanline, line+1);
    }
}


void SetDispStat(u32 cpu, u16 val)
{
    val &= 0xFFB8;
    DispStat[cpu] &= 0x0047;
    DispStat[cpu] |= val;

    VMatch[cpu] = (val >> 8) | ((val & 0x80) << 1);

    if (val & 0x10) printf("!! HBLANK ENABLED\n");
}

}
