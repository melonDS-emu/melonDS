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
// -> 'brightness up' effect does: x = x + (63-x)*factor
// * colors are converted as follows: 18bit = 15bit * 2
// -> white comes out as 62,62,62 and not 63,63,63
// * VRAM/FIFO display modes convert colors the same way
// * 3D engine converts colors differently (18bit = 15bit * 2 + 1, except 0 = 0)
// * 'screen disabled' white is 63,63,63
// * [Gericom] bit15 is used as bottom green bit for palettes. TODO: check where this applies.
//   tested on the normal BG palette and applies there
//
// oh also, changing DISPCNT bit16-17 midframe doesn't work (ignored? applied for next frame?)
// TODO, eventually: check whether other DISPCNT bits can be changed midframe
//
// for VRAM display mode, VRAM must be mapped to LCDC
//
// FIFO display mode:
// * the 'FIFO' is a circular buffer of 32 bytes (16 pixels)
// * the buffer doesn't get empty, the display controller keeps reading from it
// -> if it isn't updated, the contents will be repeated every 16 pixels
// * the write pointer is incremented when writing to the higher 16 bits of the FIFO register (0x04000068)
// * the write pointer is reset upon VBlank
// * FIFO DMA (mode 4) is triggered every 8 pixels. start bit is cleared upon VBlank.
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
//   blending is applied instead of the selected color effect, using full 5bit alpha from 3D layer
//   this even if the selected color effect is 'none'.
//   apparently this works even if BG0 isn't selected as 1st target
// * if BG0 is selected as 1st target, destination not selected as 2nd target:
//   brightness up/down effect is applied if selected. if blending is selected, it doesn't apply.
// * 3D layer pixels with alpha=0 are always transparent.
//
// mosaic:
// * mosaic grid starts at 0,0 regardless of the BG/sprite position


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
    memset(BGXRef, 0, 2*4);
    memset(BGYRef, 0, 2*4);
    memset(BGXRefInternal, 0, 2*4);
    memset(BGYRefInternal, 0, 2*4);
    memset(BGRotA, 0, 2*2);
    memset(BGRotB, 0, 2*2);
    memset(BGRotC, 0, 2*2);
    memset(BGRotD, 0, 2*2);

    memset(Win0Coords, 0, 4);
    memset(Win1Coords, 0, 4);
    memset(WinCnt, 0, 4);

    BlendCnt = 0;
    EVA = 16;
    EVB = 0;
    EVY = 0;

    memset(DispFIFOBuffer, 0, 256*2);

    CaptureCnt = 0;

    MasterBrightness = 0;

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
    switch (addr & 0x00000FFF)
    {
    case 0x048: return WinCnt[0];
    case 0x049: return WinCnt[1];
    case 0x04A: return WinCnt[2];
    case 0x04B: return WinCnt[3];
    }

    printf("unknown GPU read8 %08X\n", addr);
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

    case 0x048: return WinCnt[0] | (WinCnt[1] << 8);
    case 0x04A: return WinCnt[2] | (WinCnt[3] << 8);

    case 0x050: return BlendCnt;

    case 0x064: return CaptureCnt & 0xFFFF;
    case 0x066: return CaptureCnt >> 16;

    case 0x06C: return MasterBrightness;
    }

    printf("unknown GPU read16 %08X\n", addr);
    return 0;
}

u32 GPU2D::Read32(u32 addr)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000: return DispCnt;

    case 0x064: return CaptureCnt;
    }

    return Read16(addr) | (Read16(addr+2) << 16);
}

void GPU2D::Write8(u32 addr, u8 val)
{
    switch (addr & 0x00000FFF)
    {
    case 0x040: Win0Coords[1] = val; return;
    case 0x041: Win0Coords[0] = val; return;
    case 0x042: Win1Coords[1] = val; return;
    case 0x043: Win1Coords[0] = val; return;

    case 0x044: Win0Coords[3] = val; return;
    case 0x045: Win0Coords[2] = val; return;
    case 0x046: Win1Coords[3] = val; return;
    case 0x047: Win1Coords[2] = val; return;

    case 0x048: WinCnt[0] = val; return;
    case 0x049: WinCnt[1] = val; return;
    case 0x04A: WinCnt[2] = val; return;
    case 0x04B: WinCnt[3] = val; return;

    case 0x050: BlendCnt = (BlendCnt & 0xFF00) | val; return;
    case 0x051: BlendCnt = (BlendCnt & 0x00FF) | (val << 8); return;
    case 0x052:
        EVA = val & 0x1F;
        if (EVA > 16) EVA = 16;
        return;
    case 0x53:
        EVB = val & 0x1F;
        if (EVB > 16) EVB = 16;
        return;
    case 0x054:
        EVY = val & 0x1F;
        if (EVY > 16) EVY = 16;
        return;
    }

    printf("unknown GPU write8 %08X %02X\n", addr, val);
}

void GPU2D::Write16(u32 addr, u16 val)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000:
        DispCnt = (DispCnt & 0xFFFF0000) | val;
        return;
    case 0x002:
        DispCnt = (DispCnt & 0x0000FFFF) | (val << 16);
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
    case 0x028:
        BGXRef[0] = (BGXRef[0] & 0xFFFF0000) | val;
        if (GPU::VCount < 192) BGXRefInternal[0] = BGXRef[0];
        return;
    case 0x02A:
        if (val & 0x0800) val |= 0xF000;
        BGXRef[0] = (BGXRef[0] & 0xFFFF) | (val << 16);
        if (GPU::VCount < 192) BGXRefInternal[0] = BGXRef[0];
        return;
    case 0x02C:
        BGYRef[0] = (BGYRef[0] & 0xFFFF0000) | val;
        if (GPU::VCount < 192) BGYRefInternal[0] = BGYRef[0];
        return;
    case 0x02E:
        if (val & 0x0800) val |= 0xF000;
        BGYRef[0] = (BGYRef[0] & 0xFFFF) | (val << 16);
        if (GPU::VCount < 192) BGYRefInternal[0] = BGYRef[0];
        return;

    case 0x030: BGRotA[1] = val; return;
    case 0x032: BGRotB[1] = val; return;
    case 0x034: BGRotC[1] = val; return;
    case 0x036: BGRotD[1] = val; return;
    case 0x038:
        BGXRef[1] = (BGXRef[1] & 0xFFFF0000) | val;
        if (GPU::VCount < 192) BGXRefInternal[1] = BGXRef[1];
        return;
    case 0x03A:
        if (val & 0x0800) val |= 0xF000;
        BGXRef[1] = (BGXRef[1] & 0xFFFF) | (val << 16);
        if (GPU::VCount < 192) BGXRefInternal[1] = BGXRef[1];
        return;
    case 0x03C:
        BGYRef[1] = (BGYRef[1] & 0xFFFF0000) | val;
        if (GPU::VCount < 192) BGYRefInternal[1] = BGYRef[1];
        return;
    case 0x03E:
        if (val & 0x0800) val |= 0xF000;
        BGYRef[1] = (BGYRef[1] & 0xFFFF) | (val << 16);
        if (GPU::VCount < 192) BGYRefInternal[1] = BGYRef[1];
        return;

    case 0x040:
        Win0Coords[1] = val & 0xFF;
        Win0Coords[0] = val >> 8;
        return;
    case 0x042:
        Win1Coords[1] = val & 0xFF;
        Win1Coords[0] = val >> 8;
        return;

    case 0x044:
        Win0Coords[3] = val & 0xFF;
        Win0Coords[2] = val >> 8;
        return;
    case 0x046:
        Win1Coords[3] = val & 0xFF;
        Win1Coords[2] = val >> 8;
        return;

    case 0x048:
        WinCnt[0] = val & 0xFF;
        WinCnt[1] = val >> 8;
        return;
    case 0x04A:
        WinCnt[2] = val & 0xFF;
        WinCnt[3] = val >> 8;
        return;

    case 0x050: BlendCnt = val; return;
    case 0x052:
        EVA = val & 0x1F;
        if (EVA > 16) EVA = 16;
        EVB = (val >> 8) & 0x1F;
        if (EVB > 16) EVB = 16;
        return;
    case 0x054:
        EVY = val & 0x1F;
        if (EVY > 16) EVY = 16;
        return;

    case 0x06C: MasterBrightness = val; return;
    }

    //printf("unknown GPU write16 %08X %04X\n", addr, val);
}

void GPU2D::Write32(u32 addr, u32 val)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000:
        DispCnt = val;
        return;

    case 0x028:
        if (val & 0x08000000) val |= 0xF0000000;
        BGXRef[0] = val;
        if (GPU::VCount < 192) BGXRefInternal[0] = BGXRef[0];
        return;
    case 0x02C:
        if (val & 0x08000000) val |= 0xF0000000;
        BGYRef[0] = val;
        if (GPU::VCount < 192) BGYRefInternal[0] = BGYRef[0];
        return;

    case 0x038:
        if (val & 0x08000000) val |= 0xF0000000;
        BGXRef[1] = val;
        if (GPU::VCount < 192) BGXRefInternal[1] = BGXRef[1];
        return;
    case 0x03C:
        if (val & 0x08000000) val |= 0xF0000000;
        BGYRef[1] = val;
        if (GPU::VCount < 192) BGYRefInternal[1] = BGYRef[1];
        return;

    case 0x064:
        // TODO: check what happens when writing to it during display
        // esp. if a capture is happening
        CaptureCnt = val & 0xEF3F1F1F;
        return;
    }

    Write16(addr, val&0xFFFF);
    Write16(addr+2, val>>16);
}


void GPU2D::DrawScanline(u32 line)
{
    u32* dst = &Framebuffer[256*line];

    // request each 3D scanline in advance
    // this is required for the threaded mode of the software renderer
    // (alternately we could call GetLine() once and store the result somewhere)
    if (Num == 0)
        GPU3D::RequestLine(line);

    line = GPU::VCount;

    // scanlines that end up outside of the GPU drawing range
    // (as a result of writing to VCount) are filled white
    if (line > 192)
    {
        for (int i = 0; i < 256; i++)
            dst[i] = 0xFFFFFFFF;

        return;
    }

    u32 dispmode = DispCnt >> 16;
    dispmode &= (Num ? 0x1 : 0x3);

    // always render regular graphics
    DrawScanline_Mode1(line, dst);

    // capture
    if ((Num == 0) && (CaptureCnt & (1<<31)))
    {
        u32 capwidth, capheight;
        switch ((CaptureCnt >> 20) & 0x3)
        {
        case 0: capwidth = 128; capheight = 128; break;
        case 1: capwidth = 256; capheight = 64;  break;
        case 2: capwidth = 256; capheight = 128; break;
        case 3: capwidth = 256; capheight = 192; break;
        }

        if (line < capheight)
            DoCapture(line, capwidth, dst);
    }

    switch (dispmode)
    {
    case 0: // screen off
        {
            for (int i = 0; i < 256; i++)
                dst[i] = 0xFF3F3F3F;
        }
        break;

    case 1: // regular display, already taken care of
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

    case 3: // FIFO display (grossly inaccurate)
        {
            for (int i = 0; i < 256; i++)
            {
                u16 color = DispFIFOBuffer[i];
                u8 r = (color & 0x001F) << 1;
                u8 g = (color & 0x03E0) >> 4;
                u8 b = (color & 0x7C00) >> 9;

                dst[i] = r | (g << 8) | (b << 16);
            }
        }
        break;
    }

    // master brightness
    if (dispmode != 0)
    {
        if ((MasterBrightness >> 14) == 1)
        {
            // up
            u32 factor = MasterBrightness & 0x1F;
            if (factor > 16) factor = 16;

            for (int i = 0; i < 256; i++)
            {
                u32 val = dst[i];

                u32 r = val & 0x00003F;
                u32 g = val & 0x003F00;
                u32 b = val & 0x3F0000;

                r += (((0x00003F - r) * factor) >> 4);
                g += ((((0x003F00 - g) * factor) >> 4) & 0x003F00);
                b += ((((0x3F0000 - b) * factor) >> 4) & 0x3F0000);

                dst[i] = r | g | b;
            }
        }
        else if ((MasterBrightness >> 14) == 2)
        {
            // down
            u32 factor = MasterBrightness & 0x1F;
            if (factor > 16) factor = 16;

            for (int i = 0; i < 256; i++)
            {
                u32 val = dst[i];

                u32 r = val & 0x00003F;
                u32 g = val & 0x003F00;
                u32 b = val & 0x3F0000;

                r -= ((r * factor) >> 4);
                g -= (((g * factor) >> 4) & 0x003F00);
                b -= (((b * factor) >> 4) & 0x3F0000);

                dst[i] = r | g | b;
            }
        }
    }

    // convert to 32-bit RGBA
    for (int i = 0; i < 256; i++)
        dst[i] = ((dst[i] & 0x003F3F3F) << 2) |
                 ((dst[i] & 0x00303030) >> 4) |
                 0xFF000000;
}

void GPU2D::VBlank()
{
    CaptureCnt &= ~(1<<31);
}

void GPU2D::VBlankEnd()
{
    // TODO: find out the exact time this happens
    BGXRefInternal[0] = BGXRef[0];
    BGXRefInternal[1] = BGXRef[1];
    BGYRefInternal[0] = BGYRef[0];
    BGYRefInternal[1] = BGYRef[1];
}


void GPU2D::DoCapture(u32 line, u32 width, u32* src)
{
    u32 dstvram = (CaptureCnt >> 16) & 0x3;

    // TODO: confirm this
    // it should work like VRAM display mode, which requires VRAM to be mapped to LCDC
    if (!(GPU::VRAMMap_LCDC & (1<<dstvram)))
        return;

    u16* dst = (u16*)GPU::VRAM[dstvram];
    u32 dstaddr = (((CaptureCnt >> 18) & 0x3) << 14) + (line * width);

    if (CaptureCnt & (1<<24))
        src = (u32*)GPU3D::GetLine(line);

    u16* srcB = NULL;
    u32 srcBaddr = line * 256;

    if (CaptureCnt & (1<<25))
    {
        srcB = &DispFIFOBuffer[0];
        srcBaddr = 0;
    }
    else
    {
        u32 srcvram = (DispCnt >> 18) & 0x3;
        if (GPU::VRAMMap_LCDC & (1<<srcvram))
            srcB = (u16*)GPU::VRAM[srcvram];

        if (((DispCnt >> 16) & 0x3) != 2)
            srcBaddr += ((CaptureCnt >> 26) & 0x3) << 14;
    }

    dstaddr &= 0xFFFF;
    srcBaddr &= 0xFFFF;

    switch ((CaptureCnt >> 29) & 0x3)
    {
    case 0: // source A
        {
            for (u32 i = 0; i < width; i++)
            {
                u32 val = src[i];

                // TODO: check what happens when alpha=0

                u32 r = (val >> 1) & 0x1F;
                u32 g = (val >> 9) & 0x1F;
                u32 b = (val >> 17) & 0x1F;
                u32 a = ((val >> 24) != 0) ? 0x8000 : 0;

                dst[dstaddr] = r | (g << 5) | (b << 10) | a;
                dstaddr = (dstaddr + 1) & 0xFFFF;
            }
        }
        break;

    case 1: // source B
        {
            if (srcB)
            {
                for (u32 i = 0; i < width; i++)
                {
                    dst[dstaddr] = srcB[srcBaddr];
                    srcBaddr = (srcBaddr + 1) & 0xFFFF;
                    dstaddr = (dstaddr + 1) & 0xFFFF;
                }
            }
            else
            {
                for (u32 i = 0; i < width; i++)
                {
                    dst[dstaddr] = 0;
                    dstaddr = (dstaddr + 1) & 0xFFFF;
                }
            }
        }
        break;

    case 2: // sources A+B
    case 3:
        {
            u32 eva = CaptureCnt & 0x1F;
            u32 evb = (CaptureCnt >> 8) & 0x1F;

            // checkme
            if (eva > 16) eva = 16;
            if (evb > 16) evb = 16;

            if (srcB)
            {
                for (u32 i = 0; i < width; i++)
                {
                    u32 val = src[i];

                    // TODO: check what happens when alpha=0

                    u32 rA = (val >> 1) & 0x1F;
                    u32 gA = (val >> 9) & 0x1F;
                    u32 bA = (val >> 17) & 0x1F;
                    u32 aA = ((val >> 24) != 0) ? 1 : 0;

                    val = srcB[srcBaddr];

                    u32 rB = val & 0x1F;
                    u32 gB = (val >> 5) & 0x1F;
                    u32 bB = (val >> 10) & 0x1F;
                    u32 aB = val >> 15;

                    u32 rD = ((rA * aA * eva) + (rB * aB * evb)) >> 4;
                    u32 gD = ((gA * aA * eva) + (gB * aB * evb)) >> 4;
                    u32 bD = ((bA * aA * eva) + (bB * aB * evb)) >> 4;
                    u32 aD = (eva>0 ? aA : 0) | (evb>0 ? aB : 0);

                    if (rD > 0x1F) rD = 0x1F;
                    if (gD > 0x1F) gD = 0x1F;
                    if (bD > 0x1F) bD = 0x1F;

                    dst[dstaddr] = rD | (gD << 5) | (bD << 10) | (aD << 15);
                    srcBaddr = (srcBaddr + 1) & 0xFFFF;
                    dstaddr = (dstaddr + 1) & 0xFFFF;
                }
            }
            else
            {
                for (u32 i = 0; i < width; i++)
                {
                    u32 val = src[i];

                    // TODO: check what happens when alpha=0

                    u32 rA = (val >> 1) & 0x1F;
                    u32 gA = (val >> 9) & 0x1F;
                    u32 bA = (val >> 17) & 0x1F;
                    u32 aA = ((val >> 24) != 0) ? 1 : 0;

                    u32 rD = (rA * aA * eva) >> 4;
                    u32 gD = (gA * aA * eva) >> 4;
                    u32 bD = (bA * aA * eva) >> 4;
                    u32 aD = (eva>0 ? aA : 0);

                    dst[dstaddr] = rD | (gD << 5) | (bD << 10) | (aD << 15);
                    dstaddr = (dstaddr + 1) & 0xFFFF;
                }
            }
        }
        break;
    }
}

void GPU2D::FIFODMA(u32 addr)
{
    for (int i = 0; i < 256; i += 2)
    {
        u32 val = NDS::ARM9Read32(addr);
        addr += 4;
        DispFIFOBuffer[i] = val & 0xFFFF;
        DispFIFOBuffer[i+1] = val >> 16;
    }
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
            if (GPU::VRAMMap_BOBJExtPal & (1<<8))
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


void GPU2D::CheckWindows(u32 line)
{
    line &= 0xFF;
    if (line == Win0Coords[3])      Win0Active = false;
    else if (line == Win0Coords[2]) Win0Active = true;
    if (line == Win1Coords[3])      Win1Active = false;
    else if (line == Win1Coords[2]) Win1Active = true;
}

void GPU2D::CalculateWindowMask(u32 line, u8* mask)
{
    for (u32 i = 0; i < 256; i++)
        mask[i] = WinCnt[2]; // window outside

    if ((DispCnt & (1<<15)) && (DispCnt & (1<<12)))
    {
        // OBJ window
        u8 objwin[256];
        memset(objwin, 0, 256);
        DrawSpritesWindow(line, objwin);

        for (u32 i = 0; i < 256; i++)
        {
            if (objwin[i]) mask[i] = WinCnt[3];
        }
    }

    if ((DispCnt & (1<<14)) && Win1Active)
    {
        // window 1
        u32 x1 = Win1Coords[0];
        u32 x2 = Win1Coords[1];
        if (x2 == 0 && x1 > 0) x2 = 256;
        if (x1 > x2) x2 = 255; // checkme

        for (u32 i = x1; i < x2; i++)
            mask[i] = WinCnt[1];
    }

    if ((DispCnt & (1<<13)) && Win0Active)
    {
        // window 0
        u32 x1 = Win0Coords[0];
        u32 x2 = Win0Coords[1];
        if (x2 == 0 && x1 > 0) x2 = 256;
        if (x1 > x2) x2 = 255; // checkme

        for (u32 i = x1; i < x2; i++)
            mask[i] = WinCnt[0];
    }
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
                    DrawBG_Affine(line, dst, 3);
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
                    DrawBG_Affine(line, dst, 2);
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
    u32 linebuf[256*2 + 64];
    u8* windowmask = (u8*)&linebuf[256*2];

    u32 backdrop;
    if (Num) backdrop = *(u16*)&GPU::Palette[0x400];
    else     backdrop = *(u16*)&GPU::Palette[0];

    {
        u8 r = (backdrop & 0x001F) << 1;
        u8 g = (backdrop & 0x03E0) >> 4;
        u8 b = (backdrop & 0x7C00) >> 9;

        backdrop = r | (g << 8) | (b << 16) | 0x20000000;

        for (int i = 0; i < 256; i++)
            linebuf[i] = backdrop;
    }

    if (DispCnt & 0xE000)
        CalculateWindowMask(line, windowmask);
    else
        memset(windowmask, 0xFF, 256);

    // prerender sprites
    u32 spritebuf[256];
    memset(spritebuf, 0, 256*4);
    if (DispCnt & 0x1000) DrawSprites(line, spritebuf);

    switch (DispCnt & 0x7)
    {
    case 0: DrawScanlineBGMode<0>(line, spritebuf, linebuf); break;
    case 1: DrawScanlineBGMode<1>(line, spritebuf, linebuf); break;
    case 2: DrawScanlineBGMode<2>(line, spritebuf, linebuf); break;
    case 3: DrawScanlineBGMode<3>(line, spritebuf, linebuf); break;
    case 4: DrawScanlineBGMode<4>(line, spritebuf, linebuf); break;
    case 5: DrawScanlineBGMode<5>(line, spritebuf, linebuf); break;
    }

    // color special effects
    // can likely be optimized

    u32 bldcnteffect = (BlendCnt >> 6) & 0x3;

    for (int i = 0; i < 256; i++)
    {
        u32 val1 = linebuf[i];
        u32 val2 = linebuf[256+i];

        u32 coloreffect, eva, evb;

        u32 flag1 = val1 >> 24;
        if (!(windowmask[i] & 0x20))
        {
            coloreffect = 0;
        }
        else if ((flag1 & 0x80) && (BlendCnt & ((val2 >> 16) & 0xFF00)))
        {
            // sprite blending

            coloreffect = 1;

            if (flag1 & 0x40)
            {
                eva = flag1 & 0x1F;
                evb = 16 - eva;
            }
            else
            {
                eva = EVA;
                evb = EVB;
            }
        }
        else if ((flag1 & 0x40) && (BlendCnt & ((val2 >> 16) & 0xFF00)))
        {
            // 3D layer blending

            eva = (flag1 & 0x1F) + 1;
            evb = 32 - eva;

            u32 r = (((val1 & 0x00003F) * eva) + ((val2 & 0x00003F) * evb)) >> 5;
            u32 g = ((((val1 & 0x003F00) * eva) + ((val2 & 0x003F00) * evb)) >> 5) & 0x007F00;
            u32 b = ((((val1 & 0x3F0000) * eva) + ((val2 & 0x3F0000) * evb)) >> 5) & 0x7F0000;

            if (eva <= 16)
            {
                r += 0x000001;
                g += 0x000100;
                b += 0x010000;
            }

            if (r > 0x00003F) r = 0x00003F;
            if (g > 0x003F00) g = 0x003F00;
            if (b > 0x3F0000) b = 0x3F0000;

            dst[i] = r | g | b | 0xFF000000;

            continue;
        }
        else if (BlendCnt & flag1)
        {
            if ((bldcnteffect == 1) && (BlendCnt & ((val2 >> 16) & 0xFF00)))
            {
                coloreffect = 1;
                eva = EVA;
                evb = EVB;
            }
            else if (bldcnteffect >= 2)
                coloreffect = bldcnteffect;
            else
                coloreffect = 0;
        }
        else
            coloreffect = 0;

        switch (coloreffect)
        {
        case 0:
            dst[i] = val1;
            break;

        case 1:
            {
                u32 r = (((val1 & 0x00003F) * eva) + ((val2 & 0x00003F) * evb)) >> 4;
                u32 g = ((((val1 & 0x003F00) * eva) + ((val2 & 0x003F00) * evb)) >> 4) & 0x007F00;
                u32 b = ((((val1 & 0x3F0000) * eva) + ((val2 & 0x3F0000) * evb)) >> 4) & 0x7F0000;

                if (r > 0x00003F) r = 0x00003F;
                if (g > 0x003F00) g = 0x003F00;
                if (b > 0x3F0000) b = 0x3F0000;

                dst[i] = r | g | b | 0xFF000000;
            }
            break;

        case 2:
            {
                u32 r = val1 & 0x00003F;
                u32 g = val1 & 0x003F00;
                u32 b = val1 & 0x3F0000;

                r += ((0x00003F - r) * EVY) >> 4;
                g += (((0x003F00 - g) * EVY) >> 4) & 0x003F00;
                b += (((0x3F0000 - b) * EVY) >> 4) & 0x3F0000;

                dst[i] = r | g | b | 0xFF000000;
            }
            break;

        case 3:
            {
                u32 r = val1 & 0x00003F;
                u32 g = val1 & 0x003F00;
                u32 b = val1 & 0x3F0000;

                r -= (r * EVY) >> 4;
                g -= ((g * EVY) >> 4) & 0x003F00;
                b -= ((b * EVY) >> 4) & 0x3F0000;

                dst[i] = r | g | b | 0xFF000000;
            }
            break;
        }
    }
}


void GPU2D::DrawPixel(u32* dst, u16 color, u32 flag)
{
    u8 r = (color & 0x001F) << 1;
    u8 g = (color & 0x03E0) >> 4;
    u8 b = (color & 0x7C00) >> 9;

    *(dst+256) = *dst;
    *dst = r | (g << 8) | (b << 16) | flag;
}

void GPU2D::DrawBG_3D(u32 line, u32* dst)
{
    // TODO: check if window can prevent blending from happening

    u32* src = GPU3D::GetLine(line);
    u8* windowmask = (u8*)&dst[256*2];

    u16 xoff = BGXPos[0];
    int i = 0;
    int iend = 256;

    if (xoff & 0x100)
    {
        i = (0x100 - (xoff & 0xFF));
        xoff += i;
    }
    if ((xoff - i + iend - 1) & 0x100)
    {
        iend -= (xoff & 0xFF);
    }

    for (; i < iend; i++)
    {
        u32 c = src[xoff];
        xoff++;

        if ((c >> 24) == 0) continue;
        if (!(windowmask[i] & 0x01)) continue;

        dst[i+256] = dst[i];
        dst[i] = c | 0x40000000;
    }
}

void GPU2D::DrawBG_Text(u32 line, u32* dst, u32 bgnum)
{
    u8* windowmask = (u8*)&dst[256*2];
    u16 bgcnt = BGCnt[bgnum];

    u32 tilesetaddr, tilemapaddr;
    u16* pal;
    u32 extpal, extpalslot;

    u16 xoff = BGXPos[bgnum];
    u16 yoff = BGYPos[bgnum] + line;

    u32 widexmask = (bgcnt & 0x4000) ? 0x100 : 0;

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
            if (windowmask[i] & (1<<bgnum))
            {
                u8 color;
                u32 tilexoff = (curtile & 0x0400) ? (7-(xoff&0x7)) : (xoff&0x7);
                color = GPU::ReadVRAM_BG<u8>(pixelsaddr + tilexoff);

                if (color)
                    DrawPixel(&dst[i], curpal[color], 0x01000000<<bgnum);
            }

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
            if (windowmask[i] & (1<<bgnum))
            {
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
                    DrawPixel(&dst[i], curpal[color], 0x01000000<<bgnum);
            }

            xoff++;
        }
    }
}

void GPU2D::DrawBG_Affine(u32 line, u32* dst, u32 bgnum)
{
    u8* windowmask = (u8*)&dst[256*2];
    u16 bgcnt = BGCnt[bgnum];

    u32 tilesetaddr, tilemapaddr;
    u16* pal;

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

    s16 rotA = BGRotA[bgnum-2];
    s16 rotB = BGRotB[bgnum-2];
    s16 rotC = BGRotC[bgnum-2];
    s16 rotD = BGRotD[bgnum-2];

    s32 rotX = BGXRefInternal[bgnum-2];
    s32 rotY = BGYRefInternal[bgnum-2];

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

    yshift -= 3;

    for (int i = 0; i < 256; i++)
    {
        if ((!((rotX|rotY) & overflowmask)) && (windowmask[i] & (1<<bgnum)))
        {
            curtile = GPU::ReadVRAM_BG<u8>(tilemapaddr + ((((rotY & coordmask) >> 11) << yshift) + ((rotX & coordmask) >> 11)));

            // draw pixel
            u8 color;
            u32 tilexoff = (rotX >> 8) & 0x7;
            u32 tileyoff = (rotY >> 8) & 0x7;

            color = GPU::ReadVRAM_BG<u8>(tilesetaddr + (curtile << 6) + (tileyoff << 3) + tilexoff);

            if (color)
                DrawPixel(&dst[i], pal[color], 0x01000000<<bgnum);
        }

        rotX += rotA;
        rotY += rotC;
    }

    BGXRefInternal[bgnum-2] += rotB;
    BGYRefInternal[bgnum-2] += rotD;
}

void GPU2D::DrawBG_Extended(u32 line, u32* dst, u32 bgnum)
{
    u8* windowmask = (u8*)&dst[256*2];
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

    extpal = (DispCnt & 0x40000000);

    s16 rotA = BGRotA[bgnum-2];
    s16 rotB = BGRotB[bgnum-2];
    s16 rotC = BGRotC[bgnum-2];
    s16 rotD = BGRotD[bgnum-2];

    s32 rotX = BGXRefInternal[bgnum-2];
    s32 rotY = BGYRefInternal[bgnum-2];

    if (bgcnt & 0x0080)
    {
        // bitmap modes

        if (Num) tilemapaddr = 0x06200000 + ((bgcnt & 0x1F00) << 6);
        else     tilemapaddr = 0x06000000 + ((bgcnt & 0x1F00) << 6);

        coordmask |= 0x7FF;

        if (bgcnt & 0x0004)
        {
            // direct color bitmap

            for (int i = 0; i < 256; i++)
            {
                if ((!((rotX|rotY) & overflowmask)) && (windowmask[i] & (1<<bgnum)))
                {
                    u16 color = GPU::ReadVRAM_BG<u16>(tilemapaddr + (((((rotY & coordmask) >> 8) << yshift) + ((rotX & coordmask) >> 8)) << 1));

                    if (color & 0x8000)
                        DrawPixel(&dst[i], color, 0x01000000<<bgnum);
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
                if ((!((rotX|rotY) & overflowmask)) && (windowmask[i] & (1<<bgnum)))
                {
                    u8 color = GPU::ReadVRAM_BG<u8>(tilemapaddr + (((rotY & coordmask) >> 8) << yshift) + ((rotX & coordmask) >> 8));

                    if (color)
                        DrawPixel(&dst[i], pal[color], 0x01000000<<bgnum);
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
            if ((!((rotX|rotY) & overflowmask)) && (windowmask[i] & (1<<bgnum)))
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
                    DrawPixel(&dst[i], curpal[color], 0x01000000<<bgnum);
            }

            rotX += rotA;
            rotY += rotC;
        }
    }

    BGXRefInternal[bgnum-2] += rotB;
    BGYRefInternal[bgnum-2] += rotD;
}

void GPU2D::InterleaveSprites(u32* buf, u32 prio, u32* dst)
{
    u8* windowmask = (u8*)&dst[256*2];

    for (u32 i = 0; i < 256; i++)
    {
        if (((buf[i] & 0xF8000) == prio) && (windowmask[i] & 0x10))
        {
            u32 blendfunc = 0;
            DrawPixel(&dst[i], buf[i] & 0x7FFF, buf[i] & 0xFF000000);
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

            if (((attrib[0] >> 10) & 0x3) == 2)
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

                DrawSprite_Rotscale<false>(attrib, &oam[(rotparamgroup*16) + 3], boundwidth, boundheight, width, height, xpos, ypos, dst);
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

                DrawSprite_Normal<false>(attrib, width, xpos, ypos, dst);
            }
        }
    }
}

void GPU2D::DrawSpritesWindow(u32 line, u8* dst)
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

    for (int sprnum = 127; sprnum >= 0; sprnum--)
    {
        u16* attrib = &oam[sprnum*4];

        if (((attrib[0] >> 10) & 0x3) != 2)
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

            DrawSprite_Rotscale<true>(attrib, &oam[(rotparamgroup*16) + 3], boundwidth, boundheight, width, height, xpos, ypos, (u32*)dst);
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

            DrawSprite_Normal<true>(attrib, width, xpos, ypos, (u32*)dst);
        }
    }
}

template<bool window>
void GPU2D::DrawSprite_Rotscale(u16* attrib, u16* rotparams, u32 boundwidth, u32 boundheight, u32 width, u32 height, s32 xpos, u32 ypos, u32* dst)
{
    u32 prio = ((attrib[2] & 0x0C00) << 6) | 0x8000;
    u32 tilenum = attrib[2] & 0x03FF;
    u32 spritemode = window ? 0 : ((attrib[0] >> 10) & 0x3);

    u32 ytilefactor;

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

    if (spritemode == 3)
    {
        u32 alpha = attrib[2] >> 12;
        if (!alpha) return;
        alpha++;

        prio |= (0xC0000000 | (alpha << 24));

        if (DispCnt & 0x40)
        {
            if (DispCnt & 0x20)
            {
                // TODO ("reserved")
            }
            else
            {
                tilenum <<= (7 + ((DispCnt >> 22) & 0x1));
                ytilefactor = ((width >> 8) * 2);
            }
        }
        else
        {
            if (DispCnt & 0x20)
            {
                tilenum = ((tilenum & 0x01F) << 4) + ((tilenum & 0x3E0) << 7);
                ytilefactor = (256 * 2);
            }
            else
            {
                tilenum = ((tilenum & 0x00F) << 4) + ((tilenum & 0x3F0) << 7);
                ytilefactor = (128 * 2);
            }
        }

        u32 pixelsaddr = (Num ? 0x06600000 : 0x06400000) + tilenum;

        for (; xoff < boundwidth;)
        {
            if ((u32)rotX < width && (u32)rotY < height)
            {
                u8 color = GPU::ReadVRAM_OBJ<u16>(pixelsaddr + ((rotY >> 8) * ytilefactor) + ((rotX >> 8) << 1));

                if (color & 0x8000)
                {
                    if (window) ((u8*)dst)[xpos] = 1;
                    else        dst[xpos] = color | prio;
                }
            }

            rotX += rotA;
            rotY += rotC;
            xoff++;
            xpos++;
        }
    }
    else
    {
        if (DispCnt & 0x10)
        {
            tilenum <<= ((DispCnt >> 20) & 0x3);
            ytilefactor = (width >> 11) << ((attrib[0] & 0x2000) ? 1:0);
        }
        else
        {
            ytilefactor = 0x20;
        }

        if (spritemode == 1) prio |= 0x80000000;
        else                 prio |= 0x10000000;

        if (attrib[0] & 0x2000)
        {
            // 256-color
            tilenum <<= 5;
            ytilefactor <<= 5;
            u32 pixelsaddr = (Num ? 0x06600000 : 0x06400000) + tilenum;

            u32 extpal = (DispCnt & 0x80000000);

            u16* pal;
            if (!window)
            {
                if (extpal) pal = GetOBJExtPal(attrib[2] >> 12);
                else        pal = (u16*)&GPU::Palette[Num ? 0x600 : 0x200];
            }

            for (; xoff < boundwidth;)
            {
                if ((u32)rotX < width && (u32)rotY < height)
                {
                    u8 color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr + ((rotY>>11)*ytilefactor) + ((rotY&0x700)>>5) + ((rotX>>11)*64) + ((rotX&0x700)>>8));

                    if (color)
                    {
                        if (window) ((u8*)dst)[xpos] = 1;
                        else        dst[xpos] = pal[color] | prio;
                    }
                }

                rotX += rotA;
                rotY += rotC;
                xoff++;
                xpos++;
            }
        }
        else
        {
            // 16-color
            tilenum <<= 5;
            ytilefactor <<= 5;
            u32 pixelsaddr = (Num ? 0x06600000 : 0x06400000) + tilenum;

            u16* pal;
            if (!window)
            {
                pal = (u16*)&GPU::Palette[Num ? 0x600 : 0x200];
                pal += (attrib[2] & 0xF000) >> 8;
            }

            for (; xoff < boundwidth;)
            {
                if ((u32)rotX < width && (u32)rotY < height)
                {
                    u8 color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr + ((rotY>>11)*ytilefactor) + ((rotY&0x700)>>6) + ((rotX>>11)*32) + ((rotX&0x700)>>9));

                    if (rotX & 0x100)
                        color >>= 4;
                    else
                        color &= 0x0F;

                    if (color)
                    {
                        if (window) ((u8*)dst)[xpos] = 1;
                        else        dst[xpos] = pal[color] | prio;
                    }
                }

                rotX += rotA;
                rotY += rotC;
                xoff++;
                xpos++;
            }
        }
    }
}

template<bool window>
void GPU2D::DrawSprite_Normal(u16* attrib, u32 width, s32 xpos, u32 ypos, u32* dst)
{
    u32 prio = ((attrib[2] & 0x0C00) << 6) | 0x8000;
    u32 tilenum = attrib[2] & 0x03FF;
    u32 spritemode = window ? 0 : ((attrib[0] >> 10) & 0x3);

    u32 wmask = width - 8; // really ((width - 1) & ~0x7)

    u32 xoff;
    u32 xend = width;
    if (xpos >= 0)
    {
        xoff = 0;
        if ((xpos+xend) > 256)
            xend = 256-xpos;
    }
    else
    {
        xoff = -xpos;
        xpos = 0;
    }

    if (spritemode == 3)
    {
        // bitmap sprite

        u32 alpha = attrib[2] >> 12;
        if (!alpha) return;
        alpha++;

        prio |= (0xC0000000 | (alpha << 24));

        if (DispCnt & 0x40)
        {
            if (DispCnt & 0x20)
            {
                // TODO ("reserved")
            }
            else
            {
                tilenum <<= (7 + ((DispCnt >> 22) & 0x1));
                tilenum += (ypos * width * 2);
            }
        }
        else
        {
            if (DispCnt & 0x20)
            {
                tilenum = ((tilenum & 0x01F) << 4) + ((tilenum & 0x3E0) << 7);
                tilenum += (ypos * 256 * 2);
            }
            else
            {
                tilenum = ((tilenum & 0x00F) << 4) + ((tilenum & 0x3F0) << 7);
                tilenum += (ypos * 128 * 2);
            }
        }

        u32 pixelsaddr = (Num ? 0x06600000 : 0x06400000) + tilenum;

        if (attrib[1] & 0x1000)
        {
            pixelsaddr += ((width-1 - xoff) << 1);

            for (; xoff < xend;)
            {
                u16 color = GPU::ReadVRAM_OBJ<u16>(pixelsaddr);
                pixelsaddr -= 2;

                if (color & 0x8000)
                {
                    if (window) ((u8*)dst)[xpos] = 1;
                    else        dst[xpos] = color | prio;
                }

                xoff++;
                xpos++;
            }
        }
        else
        {
            pixelsaddr += (xoff << 1);

            for (; xoff < xend;)
            {
                u16 color = GPU::ReadVRAM_OBJ<u16>(pixelsaddr);
                pixelsaddr += 2;

                if (color & 0x8000)
                {
                    if (window) ((u8*)dst)[xpos] = 1;
                    else        dst[xpos] = color | prio;
                }

                xoff++;
                xpos++;
            }
        }
    }
    else
    {
        if (DispCnt & 0x10)
        {
            tilenum <<= ((DispCnt >> 20) & 0x3);
            tilenum += ((ypos >> 3) * (width >> 3)) << ((attrib[0] & 0x2000) ? 1:0);
        }
        else
        {
            tilenum += ((ypos >> 3) * 0x20);
        }

        if (spritemode == 1) prio |= 0x80000000;
        else                 prio |= 0x10000000;

        if (attrib[0] & 0x2000)
        {
            // 256-color
            tilenum <<= 5;
            u32 pixelsaddr = (Num ? 0x06600000 : 0x06400000) + tilenum;
            pixelsaddr += ((ypos & 0x7) << 3);

            u32 extpal = (DispCnt & 0x80000000);

            u16* pal;
            if (!window)
            {
                if (extpal) pal = GetOBJExtPal(attrib[2] >> 12);
                else        pal = (u16*)&GPU::Palette[Num ? 0x600 : 0x200];
            }

            if (attrib[1] & 0x1000) // xflip. TODO: do better? oh well for now this works
            {
                pixelsaddr += (((width-1 - xoff) & wmask) << 3);
                pixelsaddr += ((width-1 - xoff) & 0x7);

                for (; xoff < xend;)
                {
                    u8 color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr);
                    pixelsaddr--;

                    if (color)
                    {
                        if (window) ((u8*)dst)[xpos] = 1;
                        else        dst[xpos] = pal[color] | prio;
                    }

                    xoff++;
                    xpos++;
                    if (!(xoff & 0x7)) pixelsaddr -= 56;
                }
            }
            else
            {
                pixelsaddr += ((xoff & wmask) << 3);
                pixelsaddr += (xoff & 0x7);

                for (; xoff < xend;)
                {
                    u8 color = GPU::ReadVRAM_OBJ<u8>(pixelsaddr);
                    pixelsaddr++;

                    if (color)
                    {
                        if (window) ((u8*)dst)[xpos] = 1;
                        else        dst[xpos] = pal[color] | prio;
                    }

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

            u16* pal;
            if (!window)
            {
                pal = (u16*)&GPU::Palette[Num ? 0x600 : 0x200];
                pal += (attrib[2] & 0xF000) >> 8;
            }

            if (attrib[1] & 0x1000) // xflip. TODO: do better? oh well for now this works
            {
                pixelsaddr += (((width-1 - xoff) & wmask) << 2);
                pixelsaddr += (((width-1 - xoff) & 0x7) >> 1);

                for (; xoff < xend;)
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
                    {
                        if (window) ((u8*)dst)[xpos] = 1;
                        else        dst[xpos] = pal[color] | prio;
                    }

                    xoff++;
                    xpos++;
                    if (!(xoff & 0x7)) pixelsaddr -= 28;
                }
            }
            else
            {
                pixelsaddr += ((xoff & wmask) << 2);
                pixelsaddr += ((xoff & 0x7) >> 1);

                for (; xoff < xend;)
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
                    {
                        if (window) ((u8*)dst)[xpos] = 1;
                        else        dst[xpos] = pal[color] | prio;
                    }

                    xoff++;
                    xpos++;
                    if (!(xoff & 0x7)) pixelsaddr += 28;
                }
            }
        }
    }
}
