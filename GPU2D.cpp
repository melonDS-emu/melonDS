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
    memset(BGXPos, 0, 4*2);
    memset(BGYPos, 0, 4*2);
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

    case 0x010: BGXPos[0] = val; return;
    case 0x012: BGYPos[0] = val; return;
    case 0x014: BGXPos[1] = val; return;
    case 0x016: BGYPos[1] = val; return;
    case 0x018: BGXPos[2] = val; return;
    case 0x01A: BGYPos[2] = val; return;
    case 0x01C: BGXPos[3] = val; return;
    case 0x01E: BGYPos[3] = val; return;
    }

    //printf("unknown GPU write16 %08X %04X\n", addr, val);
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

// temp. hax
#define DrawBG_Text DrawBG_Text_4bpp

void GPU2D::DrawScanline_Mode1(u32 line, u16* dst)
{
    u32 backdrop;
    if (Num) backdrop = *(u16*)&GPU::Palette[0x400];
    else     backdrop = *(u16*)&GPU::Palette[0];

    // TODO: color effect for backdrop

    backdrop |= (backdrop<<16);
    for (int i = 0; i < 256>>1; i++)
        ((u32*)dst)[i] = backdrop;

    // prerender sprites
    u32 spritebuf[256];
    memset(spritebuf, 0, 256*4);
    if (DispCnt & 0x1000) DrawSprites(line, spritebuf);

    switch (DispCnt & 0x7)
    {
    case 0:
        for (int i = 3; i >= 0; i--)
        {
            if ((BGCnt[3] & 0x3) == i)
            {
                if (DispCnt & 0x0800)
                    DrawBG_Text(line, dst, 3);
            }
            if ((BGCnt[2] & 0x3) == i)
            {
                if (DispCnt & 0x0400)
                    DrawBG_Text(line, dst, 2);
            }
            if ((BGCnt[1] & 0x3) == i)
            {
                if (DispCnt & 0x0200)
                    DrawBG_Text(line, dst, 1);
            }
            if ((BGCnt[0] & 0x3) == i)
            {
                if (DispCnt & 0x0100)
                    DrawBG_Text(line, dst, 0);
            }
            if (DispCnt & 0x1000)
                InterleaveSprites(spritebuf, 0x8000 | (i<<16), dst);
        }
        break;

    case 5:
        for (int i = 3; i >= 0; i--)
        {
            if ((BGCnt[3] & 0x3) == i)
            {
                //if (DispCnt & 0x0800)
                    // ext todo
            }
            if ((BGCnt[2] & 0x3) == i)
            {
                //if (DispCnt & 0x0400)
                    // ext todo
            }
            if ((BGCnt[1] & 0x3) == i)
            {
                if (DispCnt & 0x0200)
                    DrawBG_Text(line, dst, 1);
            }
            if ((BGCnt[0] & 0x3) == i)
            {
                if (DispCnt & 0x0100)
                    DrawBG_Text(line, dst, 0);
            }
            if (DispCnt & 0x1000)
                InterleaveSprites(spritebuf, 0x8000 | (i<<16), dst);
        }
        break;
    }

    // debug crap
    //for (int i = 0; i < 256; i++)
    //    dst[i] = *(u16*)&GPU::Palette[Num*0x400 + (i>>4)*2 + (line>>4)*32];
}

// char   06218000
// screen 06208000
void GPU2D::DrawBG_Text_4bpp(u32 line, u16* dst, u32 bgnum)
{
    u16 bgcnt = BGCnt[bgnum];

    u8* tileset;
    u16* tilemap;
    u16* pal;

    u16 xoff = BGXPos[bgnum];
    u16 yoff = BGYPos[bgnum] + line;

    u32 widexmask = (bgcnt & 0x4000) ? 0x100 : 0;
    //u32 ymask = (bgcnt & 0x8000) ? 0x1FF : 0xFF;

    if (Num)
    {
        tileset = (u8*)GPU::VRAM_BBG[((bgcnt & 0x003C) >> 2)];
        tilemap = (u16*)GPU::VRAM_BBG[((bgcnt & 0x1800) >> 11)];
        if (!tileset || !tilemap) return;
        tilemap += ((bgcnt & 0x0700) << 2);

        pal = (u16*)&GPU::Palette[0x400];
    }
    else
    {
        tileset = (u8*)GPU::VRAM_ABG[((DispCnt & 0x07000000) >> 22) + ((bgcnt & 0x003C) >> 2)];
        tilemap = (u16*)GPU::VRAM_ABG[((DispCnt & 0x38000000) >> 25) + ((bgcnt & 0x1800) >> 11)];
        if (!tileset || !tilemap) return;
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
        pixels = tileset + ((curtile & 0x03FF) << 5) + ((yoff & 0x7) << 2);
        pixels += ((xoff & 0x7) >> 1);
    }

    for (int i = 0; i < 256; i++)
    {
        if (!(xoff & 0x7))
        {
            // load a new tile
            curtile = tilemap[((xoff & 0xFF) >> 3) + ((xoff & widexmask) << 2)];
            curpal = pal + ((curtile & 0xF000) >> 8);
            pixels = tileset + ((curtile & 0x03FF) << 5) + ((yoff & 0x7) << 2);
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

        if (color)
            dst[i] = curpal[color];

        xoff++;
    }
}

void GPU2D::InterleaveSprites(u32* buf, u32 prio, u16* dst)
{
    for (u32 i = 0; i < 256; i++)
    {
        if ((buf[i] & 0xF8000) == prio)
            dst[i] = buf[i] & 0x7FFF;
    }
}

void GPU2D::DrawSprites(u32 line, u32* dst)
{
    u16* oam = (u16*)&GPU::OAM[Num ? 0x400 : 0];

    const s32 spritewidth[16] =
    {
        8, 16, 8, 0,
        16, 32, 8, 0,
        32, 32, 16, 0,
        64, 64, 32, 0
    };
    const s32 spriteheight[16] =
    {
        8, 8, 16, 0,
        16, 8, 32, 0,
        32, 16, 32, 0,
        64, 32, 64, 0
    };

    for (int bgnum = 0x0C00; bgnum >= 0x0000; bgnum -= 0x0400)
    {
        for (int sprnum = 127; sprnum >= 0; sprnum--)
        {
            u16* attrib = &oam[sprnum*4];

            if ((attrib[2] & 0x0C00) != bgnum)
                continue;

            if (attrib[0] & 0x0100)
            {
                u32 sizeparam = (attrib[0] >> 14) | ((attrib[1] & 0xC000) >> 12);
                s32 width = spritewidth[sizeparam];
                s32 height = spriteheight[sizeparam];
                s32 boundwidth = width;
                s32 boundheight = height;

                if (attrib[0] & 0x0200)
                {
                    boundwidth <<= 1;
                    boundheight <<= 1;
                }

                u32 ypos = attrib[0] & 0xFF;
                ypos = (line - ypos) & 0xFF;
                if (ypos >= (u32)boundheight)
                    continue;

                s32 xpos = (s32)(attrib[1] << 23) >> 23;
                if (xpos <= -boundwidth)
                    continue;

                u32 rotparamgroup = (attrib[1] >> 9) & 0x1F;

                //DrawSprite_Normal(attrib, width, xpos, ypos, dst);
                DrawSprite_Rotscale(attrib, &oam[(rotparamgroup*16) + 3], boundwidth, boundheight, width, height, xpos, ypos, dst);
            }
            else
            {
                if (attrib[0] & 0x0200)
                    continue;

                u32 sizeparam = (attrib[0] >> 14) | ((attrib[1] & 0xC000) >> 12);
                s32 width = spritewidth[sizeparam];
                s32 height = spriteheight[sizeparam];

                u32 ypos = attrib[0] & 0xFF;
                ypos = (line - ypos) & 0xFF;
                if (ypos >= (u32)height)
                    continue;

                s32 xpos = (s32)(attrib[1] << 23) >> 23;
                if (xpos <= -width)
                    continue;

                // yflip
                if (attrib[1] & 0x2000)
                    ypos = height-1 - ypos;

                DrawSprite_Normal(attrib, width, xpos, ypos, dst);
            }
        }
    }
}

void GPU2D::DrawSprite_Rotscale(u16* attrib, u16* rotparams, u32 boundwidth, u32 boundheight, u32 width, u32 height, s32 xpos, u32 ypos, u32* dst)
{
    u32 prio = ((attrib[2] & 0x0C00) << 6) | 0x8000;
    u32 tilenum = attrib[2] & 0x03FF;
    u32 ytilefactor;
    if (DispCnt & 0x10)
    {
        tilenum <<= ((DispCnt >> 20) & 0x3);
        ytilefactor = (width >> 3);
    }
    else
    {
        ytilefactor = 0x20;
    }

    s32 centerX = boundwidth >> 1;
    s32 centerY = boundheight >> 1;

    u32 xoff;
    if (xpos >= 0)
    {
        xoff = 0;
        if ((xpos+boundwidth) > 256)
            boundwidth = 256-xpos;
    }
    else
    {
        xoff = -xpos;
        xpos = 0;
    }

    s16 rotA = (s16)rotparams[0];
    s16 rotB = (s16)rotparams[4];
    s16 rotC = (s16)rotparams[8];
    s16 rotD = (s16)rotparams[12];

    s32 rotX = ((xoff-centerX) * rotA) + ((ypos-centerY) * rotB) + (width << 7);
    s32 rotY = ((xoff-centerX) * rotC) + ((ypos-centerY) * rotD) + (height << 7);

    width <<= 8;
    height <<= 8;

    if (attrib[0] & 0x2000)
    {
        // 256-color
    }
    else
    {
        // 16-color
        tilenum <<= 5;
        ytilefactor <<= 5;
        u8* pixels = (Num ? GPU::VRAM_BOBJ : GPU::VRAM_AOBJ)[tilenum >> 14];
        if (!pixels) return;
        pixels += (tilenum & 0x3FFF);

        u16* pal = (u16*)&GPU::Palette[Num ? 0x600 : 0x200];
        pal += (attrib[2] & 0xF000) >> 8;

        for (; xoff < boundwidth;)
        {
            if ((u32)rotX < width && (u32)rotY < height)
            {
                u8 color;

                // blaaaarg
                color = pixels[((rotY>>11)*ytilefactor) + ((rotY&0x700)>>6) + ((rotX>>11)*32) + ((rotX&0x700)>>9)];

                if (rotX & 0x100)
                    color >>= 4;
                else
                    color &= 0x0F;

                if (color)
                    dst[xpos] = pal[color] | prio;
            }

            rotX += rotA;
            rotY += rotC;
            xoff++;
            xpos++;
        }
    }
}

void GPU2D::DrawSprite_Normal(u16* attrib, u32 width, s32 xpos, u32 ypos, u32* dst)
{
    u32 prio = ((attrib[2] & 0x0C00) << 6) | 0x8000;
    u32 tilenum = attrib[2] & 0x03FF;
    if (DispCnt & 0x10)
    {
        tilenum <<= ((DispCnt >> 20) & 0x3);
        tilenum += ((ypos >> 3) * (width >> 3));
    }
    else
    {
        tilenum += ((ypos >> 3) * 0x20);
    }

    u32 wmask = width - 8; // really ((width - 1) & ~0x7)

    u32 xoff;
    if (xpos >= 0)
    {
        xoff = 0;
        if ((xpos+width) > 256)
            width = 256-xpos;
    }
    else
    {
        xoff = -xpos;
        xpos = 0;
    }

    if (attrib[0] & 0x2000)
    {
        // 256-color
    }
    else
    {
        // 16-color
        tilenum <<= 5;
        u8* pixels = (Num ? GPU::VRAM_BOBJ : GPU::VRAM_AOBJ)[tilenum >> 14];
        if (!pixels) return;
        pixels += (tilenum & 0x3FFF);
        pixels += ((ypos & 0x7) << 2);

        u16* pal = (u16*)&GPU::Palette[Num ? 0x600 : 0x200];
        pal += (attrib[2] & 0xF000) >> 8;

        if (attrib[1] & 0x1000) // xflip. TODO: do better? oh well for now this works
        {
            pixels += (((width-1 - xoff) & wmask) << 2);
            pixels += (((width-1 - xoff) & 0x7) >> 1);

            for (; xoff < width;)
            {
                u8 color;
                if (xoff & 0x1)
                {
                    color = *pixels & 0x0F;
                    pixels--;
                }
                else
                {
                    color = *pixels >> 4;
                }

                if (color)
                    dst[xpos] = pal[color] | prio;

                xoff++;
                xpos++;
                if (!(xoff & 0x7)) pixels -= 28;
            }
        }
        else
        {
            pixels += ((xoff & wmask) << 2);
            pixels += ((xoff & 0x7) >> 1);

            for (; xoff < width;)
            {
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

                if (color)
                    dst[xpos] = pal[color] | prio;

                xoff++;
                xpos++;
                if (!(xoff & 0x7)) pixels += 28;
            }
        }
    }
}
