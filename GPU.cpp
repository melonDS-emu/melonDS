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

#define LINE_CYCLES  (355*6)
#define HBLANK_CYCLES (256*6)
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

//u32 VRAM_Base[9];
//u32 VRAM_Mask[9];

u32 VRAMMap_LCDC;

u32 VRAMMap_ABG[0x20];
u32 VRAMMap_AOBJ[0x10];
u32 VRAMMap_BBG[0x8];
u32 VRAMMap_BOBJ[0x8];

u32 VRAMMap_ABGExtPal[4];
u32 VRAMMap_AOBJExtPal;
u32 VRAMMap_BBGExtPal[4];
u32 VRAMMap_BOBJExtPal;

u32 VRAMMap_Texture[4];
u32 VRAMMap_TexPal[6];

u32 VRAMMap_ARM7[2];

/*u8* VRAM_ABG[128];
u8* VRAM_AOBJ[128];
u8* VRAM_BBG[128];
u8* VRAM_BOBJ[128];
u8* VRAM_LCD[128];*/
/*u8* VRAM_ARM7[2];

u8* VRAM_ABGExtPal[4];
u8* VRAM_AOBJExtPal;
u8* VRAM_BBGExtPal[4];
u8* VRAM_BOBJExtPal;*/

u32 Framebuffer[256*192*2];

GPU2D* GPU2D_A;
GPU2D* GPU2D_B;


bool Init()
{
    GPU2D_A = new GPU2D(0);
    GPU2D_B = new GPU2D(1);
    if (!GPU3D::Init()) return false;

    return true;
}

void DeInit()
{
    delete GPU2D_A;
    delete GPU2D_B;
    GPU3D::DeInit();
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

    VRAMMap_LCDC = 0;

    memset(VRAMMap_ABG, 0, sizeof(VRAMMap_ABG));
    memset(VRAMMap_AOBJ, 0, sizeof(VRAMMap_AOBJ));
    memset(VRAMMap_BBG, 0, sizeof(VRAMMap_BBG));
    memset(VRAMMap_BOBJ, 0, sizeof(VRAMMap_BOBJ));

    memset(VRAMMap_ABGExtPal, 0, sizeof(VRAMMap_ABGExtPal));
    VRAMMap_AOBJExtPal = 0;
    memset(VRAMMap_BBGExtPal, 0, sizeof(VRAMMap_BBGExtPal));
    VRAMMap_BOBJExtPal = 0;

    memset(VRAMMap_Texture, 0, sizeof(VRAMMap_Texture));
    memset(VRAMMap_TexPal, 0, sizeof(VRAMMap_TexPal));

    VRAMMap_ARM7[0] = 0;
    VRAMMap_ARM7[1] = 0;

    //memset(VRAM_Base, 0, sizeof(VRAM_Base));
    //memset(VRAM_Mask, 0, sizeof(VRAM_Mask));

    /*memset(VRAM_ABG, 0, sizeof(u8*)*128);
    memset(VRAM_AOBJ, 0, sizeof(u8*)*128);
    memset(VRAM_BBG, 0, sizeof(u8*)*128);
    memset(VRAM_BOBJ, 0, sizeof(u8*)*128);
    memset(VRAM_LCD, 0, sizeof(u8*)*128);*/
    /*memset(VRAM_ARM7, 0, sizeof(u8*)*2);

    memset(VRAM_ABGExtPal, 0, sizeof(u8*)*4);
    VRAM_AOBJExtPal = NULL;
    memset(VRAM_BBGExtPal, 0, sizeof(u8*)*4);
    VRAM_BOBJExtPal = NULL;*/

    for (int i = 0; i < 256*192*2; i++)
    {
        Framebuffer[i] = 0xFFFFFFFF;
    }

    GPU2D_A->Reset();
    GPU2D_B->Reset();
    GPU3D::Reset();

    GPU2D_A->SetFramebuffer(&Framebuffer[256*192]);
    GPU2D_B->SetFramebuffer(&Framebuffer[256*0]);
}


// VRAM mapping notes
//
// mirroring:
// unmapped range reads zero
// LCD is mirrored every 0x100000 bytes, the gap between each mirror reads zero
// ABG:
//   bank A,B,C,D,E mirror every 0x80000 bytes
//   bank F,G mirror at base+0x8000, mirror every 0x80000 bytes
// AOBJ:
//   bank A,B,E mirror every 0x40000 bytes
//   bank F,G mirror at base+0x8000, mirror every 0x40000 bytes
// BBG:
//   bank C mirrors every 0x20000 bytes
//   bank H mirrors every 0x10000 bytes
//   bank I mirrors at base+0x4000, mirrors every 0x10000 bytes
// BOBJ:
//   bank D mirrors every 0x20000 bytes
//   bank I mirrors every 0x4000 bytes
//
// untested:
//   ARM7 (TODO)
//   extended palette (mirroring doesn't apply)
//   texture/texpal (does mirroring apply?)
//   -> trying to use extpal/texture/texpal with no VRAM mapped.
//      would likely read all black, but has to be tested.
//
// overlap:
// when reading: values are read from each bank and ORed together
// when writing: value is written to each bank

#define MAP_RANGE(map, base, n)  for (int i = 0; i < n; i++) map[(base)+i] |= bankmask;
#define UNMAP_RANGE(map, base, n)  for (int i = 0; i < n; i++) map[(base)+i] &= ~bankmask;

void MapVRAM_AB(u32 bank, u8 cnt)
{
    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u8 oldofs = (oldcnt >> 3) & 0x3;
    u8 ofs = (cnt >> 3) & 0x3;
    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // ABG
            UNMAP_RANGE(VRAMMap_ABG, oldofs<<3, 8);
            break;

        case 2: // AOBJ
            oldofs &= 0x1;
            UNMAP_RANGE(VRAMMap_AOBJ, oldofs<<3, 8);
            break;

        case 3: // texture
            VRAMMap_Texture[oldofs] &= ~bankmask;
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // ABG
            MAP_RANGE(VRAMMap_ABG, ofs<<3, 8);
            break;

        case 2: // AOBJ
            ofs &= 0x1;
            MAP_RANGE(VRAMMap_AOBJ, ofs<<3, 8);
            break;

        case 3: // texture
            VRAMMap_Texture[ofs] |= bankmask;
            break;
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
    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // ABG
            UNMAP_RANGE(VRAMMap_ABG, oldofs<<3, 8);
            break;

        case 2: // ARM7 VRAM
            oldofs &= 0x1;
            VRAMMap_ARM7[oldofs] &= ~bankmask;
            break;

        case 3: // texture
            VRAMMap_Texture[oldofs] &= ~bankmask;
            break;

        case 4: // BBG/BOBJ
            if (bank == 2)
            {
                UNMAP_RANGE(VRAMMap_BBG, 0, 8);
            }
            else
            {
                UNMAP_RANGE(VRAMMap_BOBJ, 0, 8);
            }
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // ABG
            MAP_RANGE(VRAMMap_ABG, ofs<<3, 8);
            break;

        case 2: // ARM7 VRAM
            ofs &= 0x1;
            VRAMMap_ARM7[ofs] |= bankmask;
            VRAMSTAT |= (1 << (bank-2));
            break;

        case 3: // texture
            VRAMMap_Texture[ofs] |= bankmask;
            break;

        case 4: // BBG/BOBJ
            if (bank == 2)
            {
                MAP_RANGE(VRAMMap_BBG, 0, 8);
            }
            else
            {
                MAP_RANGE(VRAMMap_BOBJ, 0, 8);
            }
            break;
        }
    }
}

void MapVRAM_E(u32 bank, u8 cnt)
{
    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // ABG
            UNMAP_RANGE(VRAMMap_ABG, 0, 4);
            break;

        case 2: // AOBJ
            UNMAP_RANGE(VRAMMap_AOBJ, 0, 4);
            break;

        case 3: // texture palette
            UNMAP_RANGE(VRAMMap_TexPal, 0, 4);
            break;

        case 4: // ABG ext palette
            UNMAP_RANGE(VRAMMap_ABGExtPal, 0, 4);
            GPU2D_A->BGExtPalDirty(0);
            GPU2D_A->BGExtPalDirty(2);
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // ABG
            MAP_RANGE(VRAMMap_ABG, 0, 4);
            break;

        case 2: // AOBJ
            MAP_RANGE(VRAMMap_AOBJ, 0, 4);
            break;

        case 3: // texture palette
            MAP_RANGE(VRAMMap_TexPal, 0, 4);
            break;

        case 4: // ABG ext palette
            MAP_RANGE(VRAMMap_ABGExtPal, 0, 4);
            GPU2D_A->BGExtPalDirty(0);
            GPU2D_A->BGExtPalDirty(2);
            break;
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
    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // ABG
            VRAMMap_ABG[(oldofs & 0x1) + ((oldofs & 0x2) << 1)] &= ~bankmask;
            VRAMMap_ABG[(oldofs & 0x1) + ((oldofs & 0x2) << 1) + 2] &= ~bankmask;
            break;

        case 2: // AOBJ
            VRAMMap_AOBJ[(oldofs & 0x1) + ((oldofs & 0x2) << 1)] &= ~bankmask;
            VRAMMap_AOBJ[(oldofs & 0x1) + ((oldofs & 0x2) << 1) + 2] &= ~bankmask;
            break;

        case 3: // texture palette
            VRAMMap_TexPal[(oldofs & 0x1) + ((oldofs & 0x2) << 1)] &= ~bankmask;
            break;

        case 4: // ABG ext palette
            VRAMMap_ABGExtPal[((oldofs & 0x1) << 1)] &= ~bankmask;
            VRAMMap_ABGExtPal[((oldofs & 0x1) << 1) + 1] &= ~bankmask;
            GPU2D_A->BGExtPalDirty(0);
            GPU2D_A->BGExtPalDirty(2);
            break;

        case 5: // AOBJ ext palette
            VRAMMap_AOBJExtPal &= ~bankmask;
            GPU2D_A->OBJExtPalDirty();
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // ABG
            VRAMMap_ABG[(ofs & 0x1) + ((ofs & 0x2) << 1)] |= bankmask;
            VRAMMap_ABG[(ofs & 0x1) + ((ofs & 0x2) << 1) + 2] |= bankmask;
            break;

        case 2: // AOBJ
            VRAMMap_AOBJ[(ofs & 0x1) + ((ofs & 0x2) << 1)] |= bankmask;
            VRAMMap_AOBJ[(ofs & 0x1) + ((ofs & 0x2) << 1) + 2] |= bankmask;
            break;

        case 3: // texture palette
            VRAMMap_TexPal[(ofs & 0x1) + ((ofs & 0x2) << 1)] |= bankmask;
            break;

        case 4: // ABG ext palette
            VRAMMap_ABGExtPal[((ofs & 0x1) << 1)] |= bankmask;
            VRAMMap_ABGExtPal[((ofs & 0x1) << 1) + 1] |= bankmask;
            GPU2D_A->BGExtPalDirty(0);
            GPU2D_A->BGExtPalDirty(2);
            break;

        case 5: // AOBJ ext palette
            VRAMMap_AOBJExtPal |= bankmask;
            GPU2D_A->OBJExtPalDirty();
            break;
        }
    }
}

void MapVRAM_H(u32 bank, u8 cnt)
{
    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // BBG
            VRAMMap_BBG[0] &= ~bankmask;
            VRAMMap_BBG[1] &= ~bankmask;
            VRAMMap_BBG[4] &= ~bankmask;
            VRAMMap_BBG[5] &= ~bankmask;
            break;

        case 2: // BBG ext palette
            UNMAP_RANGE(VRAMMap_BBGExtPal, 0, 4);
            GPU2D_B->BGExtPalDirty(0);
            GPU2D_B->BGExtPalDirty(2);
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // BBG
            VRAMMap_BBG[0] |= bankmask;
            VRAMMap_BBG[1] |= bankmask;
            VRAMMap_BBG[4] |= bankmask;
            VRAMMap_BBG[5] |= bankmask;
            break;

        case 2: // BBG ext palette
            MAP_RANGE(VRAMMap_BBGExtPal, 0, 4);
            GPU2D_B->BGExtPalDirty(0);
            GPU2D_B->BGExtPalDirty(2);
            break;
        }
    }
}

void MapVRAM_I(u32 bank, u8 cnt)
{
    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // BBG
            VRAMMap_BBG[2] &= ~bankmask;
            VRAMMap_BBG[3] &= ~bankmask;
            VRAMMap_BBG[6] &= ~bankmask;
            VRAMMap_BBG[7] &= ~bankmask;
            break;

        case 2: // BOBJ
            UNMAP_RANGE(VRAMMap_BOBJ, 0, 8);
            break;

        case 3: // BOBJ ext palette
            VRAMMap_BOBJExtPal &= ~bankmask;
            GPU2D_B->OBJExtPalDirty();
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // BBG
            VRAMMap_BBG[2] |= bankmask;
            VRAMMap_BBG[3] |= bankmask;
            VRAMMap_BBG[6] |= bankmask;
            VRAMMap_BBG[7] |= bankmask;
            break;

        case 2: // BOBJ
            MAP_RANGE(VRAMMap_BOBJ, 0, 8);
            break;

        case 3: // BOBJ ext palette
            VRAMMap_BOBJExtPal |= bankmask;
            GPU2D_B->OBJExtPalDirty();
            break;
        }
    }
}


void DisplaySwap(u32 val)
{
    if (val)
    {
        GPU2D_A->SetFramebuffer(&Framebuffer[256*0]);
        GPU2D_B->SetFramebuffer(&Framebuffer[256*192]);
    }
    else
    {
        GPU2D_A->SetFramebuffer(&Framebuffer[256*192]);
        GPU2D_B->SetFramebuffer(&Framebuffer[256*0]);
    }
}


void StartFrame()
{
    StartScanline(0);
}

void StartHBlank(u32 line)
{
    DispStat[0] |= (1<<1);
    DispStat[1] |= (1<<1);

    if (line < 192) NDS::CheckDMAs(0, 0x02);

    if (DispStat[0] & (1<<4)) NDS::SetIRQ(0, NDS::IRQ_HBlank);
    if (DispStat[1] & (1<<4)) NDS::SetIRQ(1, NDS::IRQ_HBlank);

    if (line < 262)
        NDS::ScheduleEvent(NDS::Event_LCD, true, (LINE_CYCLES - HBLANK_CYCLES), StartScanline, line+1);
}

void StartScanline(u32 line)
{
    VCount = line;

    DispStat[0] &= ~(1<<1);
    DispStat[1] &= ~(1<<1);

    if (line == VMatch[0])
    {
        DispStat[0] |= (1<<2);

        if (DispStat[0] & (1<<5)) NDS::SetIRQ(0, NDS::IRQ_VCount);
    }
    else
        DispStat[0] &= ~(1<<2);

    if (line == VMatch[1])
    {
        DispStat[1] |= (1<<2);

        if (DispStat[1] & (1<<5)) NDS::SetIRQ(1, NDS::IRQ_VCount);
    }
    else
        DispStat[1] &= ~(1<<2);

    if (line < 192)
    {
        // draw
        GPU2D_A->DrawScanline(line);
        GPU2D_B->DrawScanline(line);

        //NDS::ScheduleEvent(LINE_CYCLES, StartScanline, line+1);
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

            NDS::CheckDMAs(0, 0x01);
            NDS::CheckDMAs(1, 0x11);

            if (DispStat[0] & (1<<3)) NDS::SetIRQ(0, NDS::IRQ_VBlank);
            if (DispStat[1] & (1<<3)) NDS::SetIRQ(1, NDS::IRQ_VBlank);

            GPU2D_A->VBlank();
            GPU2D_B->VBlank();
            GPU3D::VBlank();
        }

        //NDS::ScheduleEvent(LINE_CYCLES, StartScanline, line+1);
        //NDS::ScheduleEvent(NDS::Event_LCD, true, LINE_CYCLES, StartScanline, line+1);
    }

    NDS::ScheduleEvent(NDS::Event_LCD, true, HBLANK_CYCLES, StartHBlank, line);
}


void SetDispStat(u32 cpu, u16 val)
{
    val &= 0xFFB8;
    DispStat[cpu] &= 0x0047;
    DispStat[cpu] |= val;

    VMatch[cpu] = (val >> 8) | ((val & 0x80) << 1);
}

}
