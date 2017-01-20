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


GPU2D::GPU2D(u32 num)
{
    Num = num;
}

GPU2D::~GPU2D()
{
}

void GPU2D::Reset()
{
    DispCnt = 0;
    memset(BGCnt, 0, 4*2);
}

void GPU2D::SetFramebuffer(u16* buf)
{
    // framebuffer is 256x192 16bit.
    // might eventually support other framebuffer types/sizes
    Framebuffer = buf;
}


u8 GPU2D::Read8(u32 addr)
{
    printf("!! GPU2D READ8 %08X\n", addr);
    return 0;
}

u16 GPU2D::Read16(u32 addr)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000: return DispCnt&0xFFFF;
    case 0x002: return DispCnt>>16;

    case 0x008: return BGCnt[0];
    case 0x00A: return BGCnt[1];
    case 0x00C: return BGCnt[2];
    case 0x00E: return BGCnt[3];
    }

    printf("unknown GPU read16 %08X\n", addr);
    return 0;
}

u32 GPU2D::Read32(u32 addr)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000: return DispCnt;
    }

    return Read16(addr) | (Read16(addr+2) << 16);
}

void GPU2D::Write8(u32 addr, u8 val)
{
    printf("!! GPU2D WRITE8 %08X %02X\n", addr, val);
}

void GPU2D::Write16(u32 addr, u16 val)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000:
        DispCnt = (DispCnt & 0xFFFF0000) | val;
        //printf("[L] DISPCNT=%08X\n", DispCnt);
        return;
    case 0x002:
        DispCnt = (DispCnt & 0x0000FFFF) | (val << 16);
        //printf("[H] DISPCNT=%08X\n", DispCnt);
        return;

    case 0x008: BGCnt[0] = val; return;
    case 0x00A: BGCnt[1] = val; return;
    case 0x00C: BGCnt[2] = val; return;
    case 0x00E: BGCnt[3] = val; return;
    }

    printf("unknown GPU write16 %08X %04X\n", addr, val);
}

void GPU2D::Write32(u32 addr, u32 val)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000:
        //printf("DISPCNT=%08X\n", val);
        DispCnt = val;
        return;
    }

    Write16(addr, val&0xFFFF);
    Write16(addr+2, val>>16);
}


void GPU2D::DrawScanline(u32 line)
{
    u16* dst = &Framebuffer[256*line];

    u32 dispmode = DispCnt >> 16;
    dispmode &= (Num ? 0x1 : 0x3);

    switch (dispmode)
    {
    case 0: // screen off
        {
            for (int i = 0; i < 256>>1; i++)
                ((u32*)dst)[i] = 0x7FFF7FFF;
        }
        break;

    case 1: // regular display
        {
            DrawScanline_Mode1(line, dst);
        }
        break;

    case 2: // VRAM display
        {
            u32* vram = (u32*)GPU::VRAM[(DispCnt >> 18) & 0x3];
            vram = &vram[line << 7];

            for (int i = 0; i < 256>>1; i++)
                ((u32*)dst)[i] = vram[i];
        }
        break;

    case 3: // FIFO display
        {
            // uh, is there even anything that uses this?
        }
        break;
    }
}

void GPU2D::DrawScanline_Mode1(u32 line, u16* dst)
{
    u32 backdrop;
    if (Num) backdrop = *(u16*)&GPU::Palette[0x400];
    else     backdrop = *(u16*)&GPU::Palette[0];

    // TODO: color effect for backdrop

    backdrop |= (backdrop<<16);
    for (int i = 0; i < 256>>1; i++)
        ((u32*)dst)[i] = backdrop;

    switch (DispCnt & 0x7)
    {
    case 0:
        //printf("disp %08X %04X %04X %04X %04X\n", DispCnt, BGCnt[0], BGCnt[1], BGCnt[2], BGCnt[3]);
        for (int i = 3; i >= 0; i--)
        {
            if ((BGCnt[3] & 0x3) == i)
            {
                if (DispCnt & 0x0800) DrawBG_Text_4bpp(line, dst, 3);
                // todo: sprites
            }
            if ((BGCnt[2] & 0x3) == i)
            {
                if (DispCnt & 0x0400) DrawBG_Text_4bpp(line, dst, 2);
                // todo: sprites
            }
            if ((BGCnt[1] & 0x3) == i)
            {
                if (DispCnt & 0x0200) DrawBG_Text_4bpp(line, dst, 1);
                // todo: sprites
            }
            if ((BGCnt[0] & 0x3) == i)
            {
                if (DispCnt & 0x0100) DrawBG_Text_4bpp(line, dst, 0);
                // todo: sprites
            }
        }
        break;
    }
}

// char   06218000
// screen 06208000
void GPU2D::DrawBG_Text_4bpp(u32 line, u16* dst, u32 bgnum)
{
    u16 bgcnt = BGCnt[bgnum];

    u8* tileset;
    u16* tilemap;
    u16* pal;

    // TODO scroll
    u16 xoff = 0;
    u16 yoff = line;

    u32 widexmask = (bgcnt & 0x4000) ? 0x100 : 0;
    //u32 ymask = (bgcnt & 0x8000) ? 0x1FF : 0xFF;

    if (Num)
    {
        tileset = (u8*)GPU::VRAM_BBG[((bgcnt & 0x003C) >> 2)];
        tilemap = (u16*)GPU::VRAM_BBG[((bgcnt & 0x1800) >> 11)];
        tilemap += ((bgcnt & 0x0700) << 2);

        pal = (u16*)&GPU::Palette[0x400];
    }
    else
    {
        tileset = (u8*)GPU::VRAM_ABG[((DispCnt & 0x07000000) >> 22) + ((bgcnt & 0x003C) >> 2)];
        tilemap = (u16*)GPU::VRAM_ABG[((DispCnt & 0x38000000) >> 27) + ((bgcnt & 0x1800) >> 11)];
        tilemap += ((bgcnt & 0x0700) << 2);

        pal = (u16*)&GPU::Palette[0];
    }

    // adjust Y position in tilemap
    if (bgcnt & 0x8000)
    {
        tilemap += ((yoff & 0x1F8) << 2);
        if (bgcnt & 0x4000)
            tilemap += ((yoff & 0x100) << 2);
    }
    else
        tilemap += ((yoff & 0xF8) << 2);

    u16 curtile;
    u16* curpal;
    u8* pixels;

    // preload shit as needed
    if (xoff & 0x7)
    {
        // load a new tile
        curtile = tilemap[((xoff & 0xFF) >> 3) + ((xoff & widexmask) << 2)];
        curpal = pal + ((curtile & 0xF000) >> 8);
        pixels = tileset + ((curtile & 0x01FF) << 5) + ((yoff & 0x7) << 2);
        pixels += ((xoff & 0x7) >> 1);
    }

    for (int i = 0; i < 256; i++)
    {
        if (!(xoff & 0x7))
        {
            // load a new tile
            curtile = tilemap[((xoff & 0xFF) >> 3) + ((xoff & widexmask) << 2)];
            curpal = pal + ((curtile & 0xF000) >> 8);
            pixels = tileset + ((curtile & 0x01FF) << 5) + ((yoff & 0x7) << 2);
        }

        // draw pixel
        u8 color;
        if (xoff & 0x1)
        {
            color = *pixels >> 4;
            pixels++;
        }
        else
        {
            color = *pixels & 0x0F;
        }
        //color = (i >> 4) + ((line >> 4) << 4);
        //if (Num) color = 0;
        //if (yoff>127) color=0;
        if (color)
            dst[i] = curpal[color];

        xoff++;
    }
}
