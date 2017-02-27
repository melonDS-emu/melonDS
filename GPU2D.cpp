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


// notes on color conversion
//
// * BLDCNT special effects are applied on 18bit colors
// -> layers are converted to 18bit before being composited
// * colors are converted as follows: 18bit = 15bit * 2
// -> white comes out as 62,62,62 and not 63,63,63
// * VRAM/FIFO display modes convert colors the same way
// * 3D engine converts colors differently (18bit = 15bit * 2 + 1, except 0 = 0)
// * 'screen disabled' white is 63,63,63
//
// oh also, changing DISPCNT bit16-17 midframe doesn't work (ignored? applied for next frame?)
// TODO, eventually: check whether other DISPCNT bits can be changed midframe
//
// for VRAM display mode, VRAM must be mapped to LCDC
//
// sprite blending rules
// * destination must be selected as 2nd target
// * sprite must be semitransparent or bitmap sprite
// * blending is applied instead of the selected color effect, even if it is 'none'.
// * for bitmap sprites: EVA = alpha+1, EVB = 16-EVA
// * for bitmap sprites: alpha=0 is always transparent, even if blending doesn't apply
//
// 3D blending rules
//
// 3D/3D blending seems to follow these equations:
//   dstColor = srcColor*srcAlpha + dstColor*(1-srcAlpha)
//   dstAlpha = max(srcAlpha, dstAlpha)
// blending isn't applied if dstAlpha is zero.
//
// 3D/2D blending rules
// * if destination selected as 2nd target:
//   blending is applied instead of the selected color effect, using full 31bit alpha from 3D layer
//   this even if the selected color effect is 'none'.
//   apparently this works even if BG0 isn't selected as 1st target
// * if BG0 is selected as 1st target, destination not selected as 2nd target:
//   brightness up/down effect is applied if selected. if blending is selected, it doesn't apply.
// * 3D layer pixels with alpha=0 are always transparent.


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
    memset(BGXCenter, 0, 2*4);
    memset(BGYCenter, 0, 2*4);
    memset(BGRotA, 0, 2*2);
    memset(BGRotB, 0, 2*2);
    memset(BGRotC, 0, 2*2);
    memset(BGRotD, 0, 2*2);

    BGExtPalStatus[0] = 0;
    BGExtPalStatus[1] = 0;
    BGExtPalStatus[2] = 0;
    BGExtPalStatus[3] = 0;
    OBJExtPalStatus = 0;
}

void GPU2D::SetFramebuffer(u32* buf)
{
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

    case 0x020: BGRotA[0] = val; return;
    case 0x022: BGRotB[0] = val; return;
    case 0x024: BGRotC[0] = val; return;
    case 0x026: BGRotD[0] = val; return;

    case 0x030: BGRotA[1] = val; return;
    case 0x032: BGRotB[1] = val; return;
    case 0x034: BGRotC[1] = val; return;
    case 0x036: BGRotD[1] = val; return;
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

    case 0x028:
        if (val & 0x08000000) val |= 0xF0000000;
        BGXCenter[0] = val;
        return;
    case 0x02C:
        if (val & 0x08000000) val |= 0xF0000000;
        BGYCenter[0] = val;
        return;

    case 0x038:
        if (val & 0x08000000) val |= 0xF0000000;
        BGXCenter[1] = val;
        return;
    case 0x03C:
        if (val & 0x08000000) val |= 0xF0000000;
        BGYCenter[1] = val;
        return;
    }

    Write16(addr, val&0xFFFF);
    Write16(addr+2, val>>16);
}


void GPU2D::DrawScanline(u32 line)
{
    u32* dst = &Framebuffer[256*line];

    u32 dispmode = DispCnt >> 16;
    dispmode &= (Num ? 0x1 : 0x3);

    switch (dispmode)
    {
    case 0: // screen off
        {
            for (int i = 0; i < 256; i++)
                dst[i] = 0xFF3F3F3F;
        }
        break;

    case 1: // regular display
        {
            DrawScanline_Mode1(line, dst);
        }
        break;

    case 2: // VRAM display
        {
            u32 vrambank = (DispCnt >> 18) & 0x3;
            if (GPU::VRAMMap_LCDC & (1<<vrambank))
            {
                u16* vram = (u16*)GPU::VRAM[vrambank];
                vram = &vram[line * 256];

                for (int i = 0; i < 256; i++)
                {
                    u16 color = vram[i];
                    u8 r = (color & 0x001F) << 1;
                    u8 g = (color & 0x03E0) >> 4;
                    u8 b = (color & 0x7C00) >> 9;

                    dst[i] = r | (g << 8) | (b << 16);
                }
            }
            else
            {
                for (int i = 0; i < 256; i++)
                {
                    dst[i] = 0;
                }
            }
        }
        break;

    case 3: // FIFO display
        {
            // uh, is there even anything that uses this?
        }
        break;
    }

    // convert to 32-bit RGBA
    for (int i = 0; i < 256; i++)
        dst[i] = ((dst[i] & 0x003F3F3F) << 2) |
                 ((dst[i] & 0x00303030) >> 4) |
                 0xFF000000;
}

void GPU2D::VBlank()
{
    //
}


void GPU2D::BGExtPalDirty(u32 base)
{
    BGExtPalStatus[base] = 0;
    BGExtPalStatus[base+1] = 0;
}

void GPU2D::OBJExtPalDirty()
{
    OBJExtPalStatus = 0;
}


u16* GPU2D::GetBGExtPal(u32 slot, u32 pal)
{
    u16* dst = &BGExtPalCache[slot][pal << 8];

    if (!(BGExtPalStatus[slot] & (1<<pal)))
    {
        if (Num)
        {
            if (GPU::VRAMMap_BBGExtPal[slot] & (1<<7))
                memcpy(dst, &GPU::VRAM_H[(slot << 13) + (pal << 9)], 256*2);
            else
                memset(dst, 0, 256*2);
        }
        else
        {
            memset(dst, 0, 256*2);

            if (GPU::VRAMMap_ABGExtPal[slot] & (1<<4))
                for (int i = 0; i < 256; i+=2)
                    *(u32*)&dst[i] |= *(u32*)&GPU::VRAM_E[(slot << 13) + (pal << 9) + (i << 1)];

            if (GPU::VRAMMap_ABGExtPal[slot] & (1<<5))
                for (int i = 0; i < 256; i+=2)
                    *(u32*)&dst[i] |= *(u32*)&GPU::VRAM_F[((slot&1) << 13) + (pal << 9) + (i << 1)];

            if (GPU::VRAMMap_ABGExtPal[slot] & (1<<6))
                for (int i = 0; i < 256; i+=2)
                    *(u32*)&dst[i] |= *(u32*)&GPU::VRAM_G[((slot&1) << 13) + (pal << 9) + (i << 1)];
        }

        BGExtPalStatus[slot] |= (1<<pal);
    }

    return dst;
}

u16* GPU2D::GetOBJExtPal(u32 pal)
{
    u16* dst = &OBJExtPalCache[pal << 8];

    if (!(OBJExtPalStatus & (1<<pal)))
    {
        if (Num)
        {
            if (GPU::VRAMMap_BOBJExtPal & (1<<7))
                memcpy(dst, &GPU::VRAM_I[(pal << 9)], 256*2);
            else
                memset(dst, 0, 256*2);
        }
        else
        {
            memset(dst, 0, 256*2);

            if (GPU::VRAMMap_AOBJExtPal & (1<<5))
                for (int i = 0; i < 256; i+=2)
                    *(u32*)&dst[i] |= *(u32*)&GPU::VRAM_F[(pal << 9) + (i << 1)];

            if (GPU::VRAMMap_AOBJExtPal & (1<<6))
                for (int i = 0; i < 256; i+=2)
                    *(u32*)&dst[i] |= *(u32*)&GPU::VRAM_G[(pal << 9) + (i << 1)];
        }

        OBJExtPalStatus |= (1<<pal);
    }

    return dst;
}


template<u32 bgmode>
void GPU2D::DrawScanlineBGMode(u32 line, u32* spritebuf, u32* dst)
{
    for (int i = 3; i >= 0; i--)
    {
        if ((BGCnt[3] & 0x3) == i)
        {
            if (DispCnt & 0x0800)
            {
                if (bgmode >= 3)
                    DrawBG_Extended(line, dst, 3);
                else if (bgmode >= 1)
                    {} // todo: rotscale
                else
                    DrawBG_Text(line, dst, 3);
            }
        }
        if ((BGCnt[2] & 0x3) == i)
        {
            if (DispCnt & 0x0400)
            {
                if (bgmode == 5)
                    DrawBG_Extended(line, dst, 2);
                else if (bgmode == 4 || bgmode == 2)
                    {} // todo: rotscale
                else
                    DrawBG_Text(line, dst, 2);
            }
        }
        if ((BGCnt[1] & 0x3) == i)
        {
            if (DispCnt & 0x0200)
            {
                DrawBG_Text(line, dst, 1);
            }
        }
        if ((BGCnt[0] & 0x3) == i)
        {
            if (DispCnt & 0x0100)
            {
                if ((!Num) && (DispCnt & 0x8))
                    DrawBG_3D(line, dst);
                else
                    DrawBG_Text(line, dst, 0);
            }
        }
        if (DispCnt & 0x1000)
            InterleaveSprites(spritebuf, 0x8000 | (i<<16), dst);
    }
}

void GPU2D::DrawScanline_Mode1(u32 line, u32* dst)
{
    u32 backdrop;
    if (Num) backdrop = *(u16*)&GPU::Palette[0x400];
    else     backdrop = *(u16*)&GPU::Palette[0];

    {
        u8 r = (backdrop & 0x001F) << 1;
        u8 g = (backdrop & 0x03E0) >> 4;
        u8 b = (backdrop & 0x7C00) >> 9;

        // TODO: color effect for backdrop

        backdrop = r | (g << 8) | (b << 16) | 0x20000000;

        for (int i = 0; i < 256; i++)
            dst[i] = backdrop;
    }

    // prerender sprites
    u32 spritebuf[256];
    memset(spritebuf, 0, 256*4);
    if (DispCnt & 0x1000) DrawSprites(line, spritebuf);

    switch (DispCnt & 0x7)
    {
    case 0: DrawScanlineBGMode<0>(line, spritebuf, dst); break;
    case 1: DrawScanlineBGMode<1>(line, spritebuf, dst); break;
    case 2: DrawScanlineBGMode<2>(line, spritebuf, dst); break;
    case 3: DrawScanlineBGMode<3>(line, spritebuf, dst); break;
    case 4: DrawScanlineBGMode<4>(line, spritebuf, dst); break;
    case 5: DrawScanlineBGMode<5>(line, spritebuf, dst); break;
    }

    // debug crap
    //for (int i = 0; i < 256; i++)
    //    dst[i] = *(u16*)&GPU::Palette[Num*0x400 + (i>>4)*2 + (line>>4)*32];
}


typedef void (*DrawPixelFunc)(u32 bgnum, u32* dst, u16 color, u32 blendfunc);

void GPU2D::DrawPixel_Normal(u32 bgnum, u32* dst, u16 color, u32 blendfunc)
{
    u8 r = (color & 0x001F) << 1;
    u8 g = (color & 0x03E0) >> 4;
    u8 b = (color & 0x7C00) >> 9;

    *dst = r | (g << 8) | (b << 16) | (0x01000000 << bgnum);
}

void GPU2D::DrawBG_3D(u32 line, u32* dst)
{
    // TODO: scroll, etc

    u8* src = GPU3D::GetLine(line);
    for (int i = 0; i < 256; i++)
    {
        u8 r = *src++;
        u8 g = *src++;
        u8 b = *src++;
        u8 a = *src++;
        if (a == 0) continue;

        // TODO: blending
        // alpha is 6bit too....?

        dst[i] = r | (g << 8) | (b << 16);
    }
}

void GPU2D::DrawBG_Text(u32 line, u32* dst, u32 bgnum)
{
    u16 bgcnt = BGCnt[bgnum];

    u32 tilesetaddr, tilemapaddr;
    u16* pal;
    u32 extpal, extpalslot;

    u16 xoff = BGXPos[bgnum];
    u16 yoff = BGYPos[bgnum] + line;

    u32 widexmask = (bgcnt & 0x4000) ? 0x100 : 0;

    DrawPixelFunc drawpixelfn = DrawPixel_Normal;

    extpal = (DispCnt & 0x40000000);
    if (extpal) extpalslot = ((bgnum<2) && (bgcnt&0x2000)) ? (2+bgnum) : bgnum;

    if (Num)
    {
        tilesetaddr = 0x06200000 + ((bgcnt & 0x003C) << 12);
        tilemapaddr = 0x06200000 + ((bgcnt & 0x1F00) << 3);

        pal = (u16*)&GPU::Palette[0x400];
    }
    else
    {
        tilesetaddr = 0x06000000 + ((DispCnt & 0x07000000) >> 8) + ((bgcnt & 0x003C) << 12);
        tilemapaddr = 0x06000000 + ((DispCnt & 0x38000000) >> 11) + ((bgcnt & 0x1F00) << 3);

        pal = (u16*)&GPU::Palette[0];
    }

    // adjust Y position in tilemap
    if (bgcnt & 0x8000)
    {
        tilemapaddr += ((yoff & 0x1F8) << 3);
        if (bgcnt & 0x4000)
            tilemapaddr += ((yoff & 0x100) << 3);
    }
    else
        tilemapaddr += ((yoff & 0xF8) << 3);

    u16 curtile;
    u16* curpal;
    u32 pixelsaddr;

    if (bgcnt & 0x0080)
    {
        // 256-color

        // preload shit as needed
        if (xoff & 0x7)
        {
            // load a new tile
            curtile = GPU::ReadVRAM_BG<u16>(tilemapaddr + ((xoff & 0xF8) >> 2) + ((xoff & widexmask) << 3));

            if (extpal) curpal = GetBGExtPal(extpalslot, curtile>>12);
            else        curpal = pal;

            pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 6)
                                     + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 3);
        }

        for (int i = 0; i < 256; i++)
        {
            if (!(xoff & 0x7))
            {
                // load a new tile
                curtile = GPU::ReadVRAM_BG<u16>(tilemapaddr + ((xoff & 0xF8) >> 2) + ((xoff & widexmask) << 3));

                if (extpal) curpal = GetBGExtPal(extpalslot, curtile>>12);
                else        curpal = pal;

                pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 6)
                                         + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 3);
            }

            // draw pixel
            u8 color;
            u32 tilexoff = (curtile & 0x0400) ? (7-(xoff&0x7)) : (xoff&0x7);
            color = GPU::ReadVRAM_BG<u8>(pixelsaddr + tilexoff);

            if (color)
                drawpixelfn(bgnum, &dst[i], curpal[color], BlendFunc);

            xoff++;
        }
    }
    else
    {
        // 16-color

        // preload shit as needed
        if (xoff & 0x7)
        {
            // load a new tile
            curtile = GPU::ReadVRAM_BG<u16>(tilemapaddr + ((xoff & 0xF8) >> 2) + ((xoff & widexmask) << 3));
            curpal = pal + ((curtile & 0xF000) >> 8);
            pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 5)
                                     + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 2);
        }

        for (int i = 0; i < 256; i++)
        {
            if (!(xoff & 0x7))
            {
                // load a new tile
                curtile = GPU::ReadVRAM_BG<u16>(tilemapaddr + ((xoff & 0xF8) >> 2) + ((xoff & widexmask) << 3));
                curpal = pal + ((curtile & 0xF000) >> 8);
                pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 5)
                                         + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 2);
            }

            // draw pixel
            // TODO: optimize VRAM access
            u8 color;
            u32 tilexoff = (curtile & 0x0400) ? (7-(xoff&0x7)) : (xoff&0x7);
            if (tilexoff & 0x1)
            {
                color = GPU::ReadVRAM_BG<u8>(pixelsaddr + (tilexoff >> 1)) >> 4;
            }
            else
            {
                color = GPU::ReadVRAM_BG<u8>(pixelsaddr + (tilexoff >> 1)) & 0x0F;
            }

            if (color)
                drawpixelfn(bgnum, &dst[i], curpal[color], BlendFunc);

            xoff++;
        }
    }
}

void GPU2D::DrawBG_Extended(u32 line, u32* dst, u32 bgnum)
{
    u16 bgcnt = BGCnt[bgnum];

    u32 tilesetaddr, tilemapaddr;
    u16* pal;
    u32 extpal;

    u32 coordmask;
    u32 yshift;
    switch (bgcnt & 0xC000)
    {
    case 0x0000: coordmask = 0x07800; yshift = 7; break;
    case 0x4000: coordmask = 0x0F800; yshift = 8; break;
    case 0x8000: coordmask = 0x1F800; yshift = 9; break;
    case 0xC000: coordmask = 0x3F800; yshift = 10; break;
    }

    u32 overflowmask;
    if (bgcnt & 0x2000) overflowmask = 0;
    else                overflowmask = ~(coordmask | 0x7FF);

    DrawPixelFunc drawpixelfn = DrawPixel_Normal;

    extpal = (DispCnt & 0x40000000);

    s16 rotA = BGRotA[bgnum-2];
    s16 rotB = BGRotB[bgnum-2];
    s16 rotC = BGRotC[bgnum-2];
    s16 rotD = BGRotD[bgnum-2];

    s32 rotX = BGXCenter[bgnum-2];
    s32 rotY = BGYCenter[bgnum-2];

    // hax
    rotX += line*rotB;
    rotY += line*rotD;

    if (bgcnt & 0x0080)
    {
        // bitmap modes

        if (Num) tilesetaddr = 0x06200000 + ((bgcnt & 0x003C) << 12);
        else     tilesetaddr = 0x06000000 + ((bgcnt & 0x003C) << 12);

        coordmask |= 0x7FF;

        if (bgcnt & 0x0004)
        {
            // direct color bitmap

            for (int i = 0; i < 256; i++)
            {
                if (!((rotX|rotY) & overflowmask))
                {
                    u16 color = GPU::ReadVRAM_BG<u16>(tilesetaddr + (((((rotY & coordmask) >> 8) << yshift) + ((rotX & coordmask) >> 8)) << 1));

                    if (color & 0x8000)
                        drawpixelfn(bgnum, &dst[i], color, BlendFunc);
                }

                rotX += rotA;
                rotY += rotC;
            }
        }
        else
        {
            // 256-color bitmap

            if (Num) pal = (u16*)&GPU::Palette[0x400];
            else     pal = (u16*)&GPU::Palette[0];

            for (int i = 0; i < 256; i++)
            {
                if (!((rotX|rotY) & overflowmask))
                {
                    u8 color = GPU::ReadVRAM_BG<u8>(tilesetaddr + (((rotY & coordmask) >> 8) << yshift) + ((rotX & coordmask) >> 8));

                    if (color)
                        drawpixelfn(bgnum, &dst[i], pal[color], BlendFunc);
                }

                rotX += rotA;
                rotY += rotC;
            }
        }
    }
    else
    {
        // mixed affine/text mode

        if (Num)
        {
            tilesetaddr = 0x06200000 + ((bgcnt & 0x003C) << 12);
            tilemapaddr = 0x06200000 + ((bgcnt & 0x1F00) << 3);

            pal = (u16*)&GPU::Palette[0x400];
        }
        else
        {
            tilesetaddr = 0x06000000 + ((DispCnt & 0x07000000) >> 8) + ((bgcnt & 0x003C) << 12);
            tilemapaddr = 0x06000000 + ((DispCnt & 0x38000000) >> 11) + ((bgcnt & 0x1F00) << 3);

            pal = (u16*)&GPU::Palette[0];
        }

        u16 curtile;
        u16* curpal;

        yshift -= 3;

        for (int i = 0; i < 256; i++)
        {
            if (!((rotX|rotY) & overflowmask))
            {
                curtile = GPU::ReadVRAM_BG<u16>(tilemapaddr + (((((rotY & coordmask) >> 11) << yshift) + ((rotX & coordmask) >> 11)) << 1));

                if (extpal) curpal = GetBGExtPal(bgnum, curtile>>12);
                else        curpal = pal;

                // draw pixel
                u8 color;
                u32 tilexoff = (rotX >> 8) & 0x7;
                u32 tileyoff = (rotY >> 8) & 0x7;

                if (curtile & 0x0400) tilexoff = 7-tilexoff;
                if (curtile & 0x0800) tileyoff = 7-tileyoff;

                color = GPU::ReadVRAM_BG<u8>(tilesetaddr + ((curtile & 0x03FF) << 6) + (tileyoff << 3) + tilexoff);

                if (color)
                    drawpixelfn(bgnum, &dst[i], curpal[color], BlendFunc);
            }

            rotX += rotA;
            rotY += rotC;
        }
    }

    //BGXCenter[bgnum-2] += rotB;
    //BGYCenter[bgnum-2] += rotD;
}

void GPU2D::InterleaveSprites(u32* buf, u32 prio, u32* dst)
{
    DrawPixelFunc drawpixelfn = DrawPixel_Normal;

    for (u32 i = 0; i < 256; i++)
    {
        if ((buf[i] & 0xF8000) == prio)
        {
            u32 blendfunc = 0;
            drawpixelfn(4, &dst[i], buf[i], blendfunc);
        }
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
        u32 pixelsaddr = (Num ? 0x06600000 : 0x06400000) + tilenum;

        u16* pal = (u16*)&GPU::Palette[Num ? 0x600 : 0x200];
        pal += (attrib[2] & 0xF000) >> 8;

        for (; xoff < boundwidth;)
        {
            if ((u32)rotX < width && (u32)rotY < height)
            {
                u8 color;

                // blaaaarg
                color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr + ((rotY>>11)*ytilefactor) + ((rotY&0x700)>>6) + ((rotX>>11)*32) + ((rotX&0x700)>>9));

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
        tilenum += ((ypos >> 3) * (width >> 3)) << ((attrib[0] & 0x2000) ? 1:0);
    }
    else
    {
        tilenum += ((ypos >> 3) * 0x20) << ((attrib[0] & 0x2000) ? 1:0);
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
        tilenum <<= 5;
        u32 pixelsaddr = (Num ? 0x06600000 : 0x06400000) + tilenum;
        pixelsaddr += ((ypos & 0x7) << 3);

        u32 extpal = (DispCnt & 0x80000000);

        u16* pal;
        if (extpal) pal = GetOBJExtPal(attrib[2] >> 12);
        else        pal = (u16*)&GPU::Palette[Num ? 0x600 : 0x200];

        if (attrib[1] & 0x1000) // xflip. TODO: do better? oh well for now this works
        {
            pixelsaddr += (((width-1 - xoff) & wmask) << 3);
            pixelsaddr += ((width-1 - xoff) & 0x7);

            for (; xoff < width;)
            {
                u8 color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr);
                pixelsaddr--;

                if (color)
                    dst[xpos] = pal[color] | prio;

                xoff++;
                xpos++;
                if (!(xoff & 0x7)) pixelsaddr -= 56;
            }
        }
        else
        {
            pixelsaddr += ((xoff & wmask) << 3);
            pixelsaddr += (xoff & 0x7);

            for (; xoff < width;)
            {
                u8 color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr);
                pixelsaddr++;

                if (color)
                    dst[xpos] = pal[color] | prio;

                xoff++;
                xpos++;
                if (!(xoff & 0x7)) pixelsaddr += 56;
            }
        }
    }
    else
    {
        // 16-color
        tilenum <<= 5;
        u32 pixelsaddr = (Num ? 0x06600000 : 0x06400000) + tilenum;
        pixelsaddr += ((ypos & 0x7) << 2);

        u16* pal = (u16*)&GPU::Palette[Num ? 0x600 : 0x200];
        pal += (attrib[2] & 0xF000) >> 8;

        if (attrib[1] & 0x1000) // xflip. TODO: do better? oh well for now this works
        {
            pixelsaddr += (((width-1 - xoff) & wmask) << 2);
            pixelsaddr += (((width-1 - xoff) & 0x7) >> 1);

            for (; xoff < width;)
            {
                u8 color;
                if (xoff & 0x1)
                {
                    color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr) & 0x0F;
                    pixelsaddr--;
                }
                else
                {
                    color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr) >> 4;
                }

                if (color)
                    dst[xpos] = pal[color] | prio;

                xoff++;
                xpos++;
                if (!(xoff & 0x7)) pixelsaddr -= 28;
            }
        }
        else
        {
            pixelsaddr += ((xoff & wmask) << 2);
            pixelsaddr += ((xoff & 0x7) >> 1);

            for (; xoff < width;)
            {
                u8 color;
                if (xoff & 0x1)
                {
                    color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr) >> 4;
                    pixelsaddr++;
                }
                else
                {
                    color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr) & 0x0F;
                }

                if (color)
                    dst[xpos] = pal[color] | prio;

                xoff++;
                xpos++;
                if (!(xoff & 0x7)) pixelsaddr += 28;
            }
        }
    }
}
