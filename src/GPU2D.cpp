/*
    Copyright 2016-2020 Arisotura

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
// * when changing it midframe: new X setting is applied immediately, new Y setting is applied only
//   after the end of the current mosaic row (when Y counter needs reloaded)
// * for rotscaled sprites: coordinates that are inside the sprite are clamped to the sprite region
//   after being transformed for mosaic

// TODO: master brightness, display capture and mainmem FIFO are separate circuitry, distinct from
// the tile renderers.
// for example these aren't affected by POWCNT GPU-disable bits.
// to model the hardware more accurately, the relevant logic should be moved to GPU.cpp.


GPU2D::GPU2D(u32 num)
{
    Num = num;

    // initialize mosaic table
    for (int m = 0; m < 16; m++)
    {
        for (int x = 0; x < 256; x++)
        {
            int offset = x % (m+1);
            MosaicTable[m][x] = offset;
        }
    }
}

GPU2D::~GPU2D()
{
}

void GPU2D::Reset()
{
    Enabled = false;
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

    Win0Active = 0;
    Win1Active = 0;

    BGMosaicSize[0] = 0;
    BGMosaicSize[1] = 0;
    OBJMosaicSize[0] = 0;
    OBJMosaicSize[1] = 0;
    BGMosaicY = 0;
    BGMosaicYMax = 0;
    OBJMosaicY = 0;
    OBJMosaicYMax = 0;
    CurBGXMosaicTable = MosaicTable[0];
    CurOBJXMosaicTable = MosaicTable[0];

    BlendCnt = 0;
    EVA = 16;
    EVB = 0;
    EVY = 0;

    memset(DispFIFO, 0, 16*2);
    DispFIFOReadPtr = 0;
    DispFIFOWritePtr = 0;

    memset(DispFIFOBuffer, 0, 256*2);

    CaptureCnt = 0;

    MasterBrightness = 0;
}

void GPU2D::DoSavestate(Savestate* file)
{
    file->Section((char*)(Num ? "GP2B" : "GP2A"));

    file->Var32(&DispCnt);
    file->VarArray(BGCnt, 4*2);
    file->VarArray(BGXPos, 4*2);
    file->VarArray(BGYPos, 4*2);
    file->VarArray(BGXRef, 2*4);
    file->VarArray(BGYRef, 2*4);
    file->VarArray(BGXRefInternal, 2*4);
    file->VarArray(BGYRefInternal, 2*4);
    file->VarArray(BGRotA, 2*2);
    file->VarArray(BGRotB, 2*2);
    file->VarArray(BGRotC, 2*2);
    file->VarArray(BGRotD, 2*2);

    file->VarArray(Win0Coords, 4);
    file->VarArray(Win1Coords, 4);
    file->VarArray(WinCnt, 4);

    file->VarArray(BGMosaicSize, 2);
    file->VarArray(OBJMosaicSize, 2);
    file->Var8(&BGMosaicY);
    file->Var8(&BGMosaicYMax);
    file->Var8(&OBJMosaicY);
    file->Var8(&OBJMosaicYMax);

    file->Var16(&BlendCnt);
    file->Var16(&BlendAlpha);
    file->Var8(&EVA);
    file->Var8(&EVB);
    file->Var8(&EVY);

    file->Var16(&MasterBrightness);

    if (!Num)
    {
        file->VarArray(DispFIFO, 16*2);
        file->Var32(&DispFIFOReadPtr);
        file->Var32(&DispFIFOWritePtr);

        file->VarArray(DispFIFOBuffer, 256*2);

        file->Var32(&CaptureCnt);
    }

    file->Var32(&Win0Active);
    file->Var32(&Win1Active);

    if (!file->Saving)
    {
        CurBGXMosaicTable = MosaicTable[BGMosaicSize[0]];
        CurOBJXMosaicTable = MosaicTable[OBJMosaicSize[0]];
    }
}

void GPU2D::SetFramebuffer(u32* buf)
{
    Framebuffer = buf;
}

void GPU2D::SetRenderSettings(bool accel)
{
    Accelerated = accel;
}


u8 GPU2D::Read8(u32 addr)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000: return DispCnt & 0xFF;
    case 0x001: return (DispCnt >> 8) & 0xFF;
    case 0x002: return (DispCnt >> 16) & 0xFF;
    case 0x003: return DispCnt >> 24;

    case 0x008: return BGCnt[0] & 0xFF;
    case 0x009: return BGCnt[0] >> 8;
    case 0x00A: return BGCnt[1] & 0xFF;
    case 0x00B: return BGCnt[1] >> 8;
    case 0x00C: return BGCnt[2] & 0xFF;
    case 0x00D: return BGCnt[2] >> 8;
    case 0x00E: return BGCnt[3] & 0xFF;
    case 0x00F: return BGCnt[3] >> 8;

    case 0x048: return WinCnt[0];
    case 0x049: return WinCnt[1];
    case 0x04A: return WinCnt[2];
    case 0x04B: return WinCnt[3];

    // there are games accidentally trying to read those
    // those are write-only
    case 0x04C:
    case 0x04D: return 0;
    }

    printf("unknown GPU read8 %08X\n", addr);
    return 0;
}

u16 GPU2D::Read16(u32 addr)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000: return DispCnt & 0xFFFF;
    case 0x002: return DispCnt >> 16;

    case 0x008: return BGCnt[0];
    case 0x00A: return BGCnt[1];
    case 0x00C: return BGCnt[2];
    case 0x00E: return BGCnt[3];

    case 0x048: return WinCnt[0] | (WinCnt[1] << 8);
    case 0x04A: return WinCnt[2] | (WinCnt[3] << 8);

    case 0x050: return BlendCnt;
    case 0x052: return BlendAlpha;
    // BLDY is write-only

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
    case 0x000:
        DispCnt = (DispCnt & 0xFFFFFF00) | val;
        if (Num) DispCnt &= 0xC0B1FFF7;
        return;
    case 0x001:
        DispCnt = (DispCnt & 0xFFFF00FF) | (val << 8);
        if (Num) DispCnt &= 0xC0B1FFF7;
        return;
    case 0x002:
        DispCnt = (DispCnt & 0xFF00FFFF) | (val << 16);
        if (Num) DispCnt &= 0xC0B1FFF7;
        return;
    case 0x003:
        DispCnt = (DispCnt & 0x00FFFFFF) | (val << 24);
        if (Num) DispCnt &= 0xC0B1FFF7;
        return;
    }

    if (!Enabled) return;

    switch (addr & 0x00000FFF)
    {
    case 0x008: BGCnt[0] = (BGCnt[0] & 0xFF00) | val; return;
    case 0x009: BGCnt[0] = (BGCnt[0] & 0x00FF) | (val << 8); return;
    case 0x00A: BGCnt[1] = (BGCnt[1] & 0xFF00) | val; return;
    case 0x00B: BGCnt[1] = (BGCnt[1] & 0x00FF) | (val << 8); return;
    case 0x00C: BGCnt[2] = (BGCnt[2] & 0xFF00) | val; return;
    case 0x00D: BGCnt[2] = (BGCnt[2] & 0x00FF) | (val << 8); return;
    case 0x00E: BGCnt[3] = (BGCnt[3] & 0xFF00) | val; return;
    case 0x00F: BGCnt[3] = (BGCnt[3] & 0x00FF) | (val << 8); return;

    case 0x010: BGXPos[0] = (BGXPos[0] & 0xFF00) | val; return;
    case 0x011: BGXPos[0] = (BGXPos[0] & 0x00FF) | (val << 8); return;
    case 0x012: BGYPos[0] = (BGYPos[0] & 0xFF00) | val; return;
    case 0x013: BGYPos[0] = (BGYPos[0] & 0x00FF) | (val << 8); return;
    case 0x014: BGXPos[1] = (BGXPos[1] & 0xFF00) | val; return;
    case 0x015: BGXPos[1] = (BGXPos[1] & 0x00FF) | (val << 8); return;
    case 0x016: BGYPos[1] = (BGYPos[1] & 0xFF00) | val; return;
    case 0x017: BGYPos[1] = (BGYPos[1] & 0x00FF) | (val << 8); return;
    case 0x018: BGXPos[2] = (BGXPos[2] & 0xFF00) | val; return;
    case 0x019: BGXPos[2] = (BGXPos[2] & 0x00FF) | (val << 8); return;
    case 0x01A: BGYPos[2] = (BGYPos[2] & 0xFF00) | val; return;
    case 0x01B: BGYPos[2] = (BGYPos[2] & 0x00FF) | (val << 8); return;
    case 0x01C: BGXPos[3] = (BGXPos[3] & 0xFF00) | val; return;
    case 0x01D: BGXPos[3] = (BGXPos[3] & 0x00FF) | (val << 8); return;
    case 0x01E: BGYPos[3] = (BGYPos[3] & 0xFF00) | val; return;
    case 0x01F: BGYPos[3] = (BGYPos[3] & 0x00FF) | (val << 8); return;

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

    case 0x04C:
        BGMosaicSize[0] = val & 0xF;
        BGMosaicSize[1] = val >> 4;
        CurBGXMosaicTable = MosaicTable[BGMosaicSize[0]];
        return;
    case 0x04D:
        OBJMosaicSize[0] = val & 0xF;
        OBJMosaicSize[1] = val >> 4;
        CurOBJXMosaicTable = MosaicTable[OBJMosaicSize[0]];
        return;

    case 0x050: BlendCnt = (BlendCnt & 0x3F00) | val; return;
    case 0x051: BlendCnt = (BlendCnt & 0x00FF) | (val << 8); return;
    case 0x052:
        BlendAlpha = (BlendAlpha & 0x1F00) | (val & 0x1F);
        EVA = val & 0x1F;
        if (EVA > 16) EVA = 16;
        return;
    case 0x053:
        BlendAlpha = (BlendAlpha & 0x001F) | ((val & 0x1F) << 8);
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
        if (Num) DispCnt &= 0xC0B1FFF7;
        return;
    case 0x002:
        DispCnt = (DispCnt & 0x0000FFFF) | (val << 16);
        if (Num) DispCnt &= 0xC0B1FFF7;
        return;

    case 0x068:
        DispFIFO[DispFIFOWritePtr] = val;
        return;
    case 0x06A:
        DispFIFO[DispFIFOWritePtr+1] = val;
        DispFIFOWritePtr += 2;
        DispFIFOWritePtr &= 0xF;
        return;

    case 0x06C: MasterBrightness = val; return;
    }

    if (!Enabled) return;

    switch (addr & 0x00000FFF)
    {
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

    case 0x04C:
        BGMosaicSize[0] = val & 0xF;
        BGMosaicSize[1] = (val >> 4) & 0xF;
        CurBGXMosaicTable = MosaicTable[BGMosaicSize[0]];
        OBJMosaicSize[0] = (val >> 8) & 0xF;
        OBJMosaicSize[1] = val >> 12;
        CurOBJXMosaicTable = MosaicTable[OBJMosaicSize[0]];
        return;

    case 0x050: BlendCnt = val & 0x3FFF; return;
    case 0x052:
        BlendAlpha = val & 0x1F1F;
        EVA = val & 0x1F;
        if (EVA > 16) EVA = 16;
        EVB = (val >> 8) & 0x1F;
        if (EVB > 16) EVB = 16;
        return;
    case 0x054:
        EVY = val & 0x1F;
        if (EVY > 16) EVY = 16;
        return;
    }

    //printf("unknown GPU write16 %08X %04X\n", addr, val);
}

void GPU2D::Write32(u32 addr, u32 val)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000:
        DispCnt = val;
        if (Num) DispCnt &= 0xC0B1FFF7;
        return;

    case 0x064:
        // TODO: check what happens when writing to it during display
        // esp. if a capture is happening
        CaptureCnt = val & 0xEF3F1F1F;
        return;

    case 0x068:
        DispFIFO[DispFIFOWritePtr] = val & 0xFFFF;
        DispFIFO[DispFIFOWritePtr+1] = val >> 16;
        DispFIFOWritePtr += 2;
        DispFIFOWritePtr &= 0xF;
        return;
    }

    if (!Enabled) return;

    switch (addr & 0x00000FFF)
    {
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
    }

    Write16(addr, val&0xFFFF);
    Write16(addr+2, val>>16);
}


u32 GPU2D::ColorBlend4(u32 val1, u32 val2, u32 eva, u32 evb)
{
    u32 r = (((val1 & 0x00003F) * eva) + ((val2 & 0x00003F) * evb)) >> 4;
    u32 g = ((((val1 & 0x003F00) * eva) + ((val2 & 0x003F00) * evb)) >> 4) & 0x007F00;
    u32 b = ((((val1 & 0x3F0000) * eva) + ((val2 & 0x3F0000) * evb)) >> 4) & 0x7F0000;

    if (r > 0x00003F) r = 0x00003F;
    if (g > 0x003F00) g = 0x003F00;
    if (b > 0x3F0000) b = 0x3F0000;

    return r | g | b | 0xFF000000;
}

u32 GPU2D::ColorBlend5(u32 val1, u32 val2)
{
    u32 eva = ((val1 >> 24) & 0x1F) + 1;
    u32 evb = 32 - eva;

    if (eva == 32) return val1;

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

    return r | g | b | 0xFF000000;
}

u32 GPU2D::ColorBrightnessUp(u32 val, u32 factor)
{
    u32 rb = val & 0x3F003F;
    u32 g = val & 0x003F00;

    rb += ((((0x3F003F - rb) * factor) >> 4) & 0x3F003F);
    g += ((((0x003F00 - g) * factor) >> 4) & 0x003F00);

    return rb | g | 0xFF000000;
}

u32 GPU2D::ColorBrightnessDown(u32 val, u32 factor)
{
    u32 rb = val & 0x3F003F;
    u32 g = val & 0x003F00;

    rb -= (((rb * factor) >> 4) & 0x3F003F);
    g -= (((g * factor) >> 4) & 0x003F00);

    return rb | g | 0xFF000000;
}

u32 GPU2D::ColorComposite(int i, u32 val1, u32 val2)
{
    u32 coloreffect = 0;
    u32 eva, evb;

    u32 flag1 = val1 >> 24;
    u32 flag2 = val2 >> 24;

    u32 target2;
    if      (flag2 & 0x80) target2 = 0x1000;
    else if (flag2 & 0x40) target2 = 0x0100;
    else                   target2 = flag2 << 8;

    if ((flag1 & 0x80) && (BlendCnt & target2))
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
    else if ((flag1 & 0x40) && (BlendCnt & target2))
    {
        // 3D layer blending

        coloreffect = 4;
    }
    else
    {
        if      (flag1 & 0x80) flag1 = 0x10;
        else if (flag1 & 0x40) flag1 = 0x01;

        if ((BlendCnt & flag1) && (WindowMask[i] & 0x20))
        {
            coloreffect = (BlendCnt >> 6) & 0x3;

            if (coloreffect == 1)
            {
                if (BlendCnt & target2)
                {
                    eva = EVA;
                    evb = EVB;
                }
                else
                    coloreffect = 0;
            }
        }
    }

    switch (coloreffect)
    {
    case 0: return val1;
    case 1: return ColorBlend4(val1, val2, eva, evb);
    case 2: return ColorBrightnessUp(val1, EVY);
    case 3: return ColorBrightnessDown(val1, EVY);
    case 4: return ColorBlend5(val1, val2);
    }

    return val1;
}


void GPU2D::UpdateMosaicCounters(u32 line)
{
    // Y mosaic uses incrementing 4-bit counters
    // the transformed Y position is updated every time the counter matches the MOSAIC register

    if (OBJMosaicYCount == OBJMosaicSize[1])
    {
        OBJMosaicYCount = 0;
        OBJMosaicY = line + 1;
    }
    else
    {
        OBJMosaicYCount++;
        OBJMosaicYCount &= 0xF;
    }
}


void GPU2D::DrawScanline(u32 line)
{
    int stride = Accelerated ? (256*3 + 1) : 256;
    u32* dst = &Framebuffer[stride * line];

    int n3dline = line;
    line = GPU::VCount;

    if (Num == 0)
    {
        auto bgDirty = GPU::VRAMDirty_ABG.DeriveState(GPU::VRAMMap_ABG);
        GPU::MakeVRAMFlat_ABGCoherent(bgDirty);
        auto bgExtPalDirty = GPU::VRAMDirty_ABGExtPal.DeriveState(GPU::VRAMMap_ABGExtPal);
        GPU::MakeVRAMFlat_ABGExtPalCoherent(bgExtPalDirty);
        auto objExtPalDirty = GPU::VRAMDirty_AOBJExtPal.DeriveState(&GPU::VRAMMap_AOBJExtPal);
        GPU::MakeVRAMFlat_AOBJExtPalCoherent(objExtPalDirty);
    }
    else
    {
        auto bgDirty = GPU::VRAMDirty_BBG.DeriveState(GPU::VRAMMap_BBG);
        GPU::MakeVRAMFlat_BBGCoherent(bgDirty);
        auto bgExtPalDirty = GPU::VRAMDirty_BBGExtPal.DeriveState(GPU::VRAMMap_BBGExtPal);
        GPU::MakeVRAMFlat_BBGExtPalCoherent(bgExtPalDirty);
        auto objExtPalDirty = GPU::VRAMDirty_BOBJExtPal.DeriveState(&GPU::VRAMMap_BOBJExtPal);
        GPU::MakeVRAMFlat_BOBJExtPalCoherent(objExtPalDirty);
    }

    bool forceblank = false;

    // scanlines that end up outside of the GPU drawing range
    // (as a result of writing to VCount) are filled white
    if (line > 192) forceblank = true;

    // GPU B can be completely disabled by POWCNT1
    // oddly that's not the case for GPU A
    if (Num && !Enabled) forceblank = true;

    if (forceblank)
    {
        for (int i = 0; i < 256; i++)
            dst[i] = 0xFFFFFFFF;

        if (Accelerated)
        {
            dst[256*3] = 0;
        }
        return;
    }

    u32 dispmode = DispCnt >> 16;
    dispmode &= (Num ? 0x1 : 0x3);

    if (Num == 0)
    {
        if (!Accelerated)
            _3DLine = GPU3D::GetLine(n3dline);
        else if ((CaptureCnt & (1<<31)) && (((CaptureCnt >> 29) & 0x3) != 1))
        {
            _3DLine = GPU3D::GetLine(n3dline);
            //GPU3D::GLRenderer::PrepareCaptureFrame();
        }
    }

    // always render regular graphics
    DrawScanline_BGOBJ(line);
    UpdateMosaicCounters(line);

    switch (dispmode)
    {
    case 0: // screen off
        {
            for (int i = 0; i < 256; i++)
                dst[i] = 0x003F3F3F;
        }
        break;

    case 1: // regular display
        {
            int i = 0;
            for (; i < (stride & ~1); i+=2)
                *(u64*)&dst[i] = *(u64*)&BGOBJLine[i];
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
            DoCapture(line, capwidth);
    }

    if (Accelerated)
    {
        dst[256*3] = MasterBrightness | (DispCnt & 0x30000);
        return;
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
                dst[i] = ColorBrightnessUp(dst[i], factor);
            }
        }
        else if ((MasterBrightness >> 14) == 2)
        {
            // down
            u32 factor = MasterBrightness & 0x1F;
            if (factor > 16) factor = 16;

            for (int i = 0; i < 256; i++)
            {
                dst[i] = ColorBrightnessDown(dst[i], factor);
            }
        }
    }

    // convert to 32-bit BGRA
    // note: 32-bit RGBA would be more straightforward, but
    // BGRA seems to be more compatible (Direct2D soft, cairo...)
    for (int i = 0; i < 256; i+=2)
    {
        u64 c = *(u64*)&dst[i];

        u64 r = (c << 18) & 0xFC000000FC0000;
        u64 g = (c << 2) & 0xFC000000FC00;
        u64 b = (c >> 14) & 0xFC000000FC;
        c = r | g | b;

        *(u64*)&dst[i] = c | ((c & 0x00C0C0C000C0C0C0) >> 6) | 0xFF000000FF000000;
    }
}

void GPU2D::VBlank()
{
    CaptureCnt &= ~(1<<31);

    DispFIFOReadPtr = 0;
    DispFIFOWritePtr = 0;
}

void GPU2D::VBlankEnd()
{
    // TODO: find out the exact time this happens
    BGXRefInternal[0] = BGXRef[0];
    BGXRefInternal[1] = BGXRef[1];
    BGYRefInternal[0] = BGYRef[0];
    BGYRefInternal[1] = BGYRef[1];

    BGMosaicY = 0;
    BGMosaicYMax = BGMosaicSize[1];
    //OBJMosaicY = 0;
    //OBJMosaicYMax = OBJMosaicSize[1];
    //OBJMosaicY = 0;
    //OBJMosaicYCount = 0;

#ifdef OGLRENDERER_ENABLED
    if (Accelerated)
    {
        if ((Num == 0) && (CaptureCnt & (1<<31)) && (((CaptureCnt >> 29) & 0x3) != 1))
        {
            GPU3D::GLRenderer::PrepareCaptureFrame();
        }
    }
#endif
}


void GPU2D::DoCapture(u32 line, u32 width)
{
    u32 dstvram = (CaptureCnt >> 16) & 0x3;

    // TODO: confirm this
    // it should work like VRAM display mode, which requires VRAM to be mapped to LCDC
    if (!(GPU::VRAMMap_LCDC & (1<<dstvram)))
        return;

    u16* dst = (u16*)GPU::VRAM[dstvram];
    u32 dstaddr = (((CaptureCnt >> 18) & 0x3) << 14) + (line * width);

    static_assert(GPU::VRAMDirtyGranularity == 512);
    GPU::VRAMDirty[dstvram][(dstaddr & 0x1FFFF) / GPU::VRAMDirtyGranularity] = true;

    // TODO: handle 3D in accelerated mode!!

    u32* srcA;
    if (CaptureCnt & (1<<24))
    {
        srcA = _3DLine;
    }
    else
    {
        srcA = BGOBJLine;
        if (Accelerated)
        {
            // in accelerated mode, compositing is normally done on the GPU
            // but when doing display capture, we do need the composited output
            // so we do it here

            for (int i = 0; i < 256; i++)
            {
                u32 val1 = BGOBJLine[i];
                u32 val2 = BGOBJLine[256+i];
                u32 val3 = BGOBJLine[512+i];

                u32 compmode = (val3 >> 24) & 0xF;

                if (compmode == 4)
                {
                    // 3D on top, blending

                    u32 _3dval = _3DLine[val3 & 0xFF];
                    if ((_3dval >> 24) > 0)
                        val1 = ColorBlend5(_3dval, val1);
                    else
                        val1 = val2;
                }
                else if (compmode == 1)
                {
                    // 3D on bottom, blending

                    u32 _3dval = _3DLine[val3 & 0xFF];
                    if ((_3dval >> 24) > 0)
                    {
                        u32 eva = (val3 >> 8) & 0x1F;
                        u32 evb = (val3 >> 16) & 0x1F;

                        val1 = ColorBlend4(val1, _3dval, eva, evb);
                    }
                    else
                        val1 = val2;
                }
                else if (compmode <= 3)
                {
                    // 3D on top, normal/fade

                    u32 _3dval = _3DLine[val3 & 0xFF];
                    if ((_3dval >> 24) > 0)
                    {
                        u32 evy = (val3 >> 8) & 0x1F;

                        val1 = _3dval;
                        if      (compmode == 2) val1 = ColorBrightnessUp(val1, evy);
                        else if (compmode == 3) val1 = ColorBrightnessDown(val1, evy);
                    }
                    else
                        val1 = val2;
                }

                BGOBJLine[i] = val1;
            }
        }
    }

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
                u32 val = srcA[i];

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
                    u32 val = srcA[i];

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
                    u32 val = srcA[i];

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

void GPU2D::SampleFIFO(u32 offset, u32 num)
{
    for (u32 i = 0; i < num; i++)
    {
        u16 val = DispFIFO[DispFIFOReadPtr];
        DispFIFOReadPtr++;
        DispFIFOReadPtr &= 0xF;

        DispFIFOBuffer[offset+i] = val;
    }
}

u16* GPU2D::GetBGExtPal(u32 slot, u32 pal)
{
    const u32 PaletteSize = 256 * 2;
    const u32 SlotSize = PaletteSize * 16;
    return (u16*)&(Num == 0
         ? GPU::VRAMFlat_ABGExtPal
         : GPU::VRAMFlat_BBGExtPal)[slot * SlotSize + pal * PaletteSize];
}

u16* GPU2D::GetOBJExtPal()
{
    return Num == 0
         ? (u16*)GPU::VRAMFlat_AOBJExtPal
         : (u16*)GPU::VRAMFlat_BOBJExtPal;
}


void GPU2D::CheckWindows(u32 line)
{
    line &= 0xFF;
    if (line == Win0Coords[3])      Win0Active &= ~0x1;
    else if (line == Win0Coords[2]) Win0Active |=  0x1;
    if (line == Win1Coords[3])      Win1Active &= ~0x1;
    else if (line == Win1Coords[2]) Win1Active |=  0x1;
}

void GPU2D::CalculateWindowMask(u32 line)
{
    for (u32 i = 0; i < 256; i++)
        WindowMask[i] = WinCnt[2]; // window outside

    if (DispCnt & (1<<15))
    {
        // OBJ window
        for (int i = 0; i < 256; i++)
        {
            if (OBJWindow[i])
                WindowMask[i] = WinCnt[3];
        }
    }

    if (DispCnt & (1<<14))
    {
        // window 1
        u8 x1 = Win1Coords[0];
        u8 x2 = Win1Coords[1];

        for (int i = 0; i < 256; i++)
        {
            if (i == x2)      Win1Active &= ~0x2;
            else if (i == x1) Win1Active |=  0x2;

            if (Win1Active == 0x3) WindowMask[i] = WinCnt[1];
        }
    }

    if (DispCnt & (1<<13))
    {
        // window 0
        u8 x1 = Win0Coords[0];
        u8 x2 = Win0Coords[1];

        for (int i = 0; i < 256; i++)
        {
            if (i == x2)      Win0Active &= ~0x2;
            else if (i == x1) Win0Active |=  0x2;

            if (Win0Active == 0x3) WindowMask[i] = WinCnt[0];
        }
    }
}


#define DoDrawBG(type, line, num) \
    { \
        if ((BGCnt[num] & 0x0040) && (BGMosaicSize[0] > 0)) \
        { \
            if (Accelerated) DrawBG_##type<true, DrawPixel_Accel>(line, num); \
            else DrawBG_##type<true, DrawPixel_Normal>(line, num); \
        } \
        else \
        { \
            if (Accelerated) DrawBG_##type<false, DrawPixel_Accel>(line, num); \
            else DrawBG_##type<false, DrawPixel_Normal>(line, num); \
        } \
    }

#define DoDrawBG_Large(line) \
    do \
    { \
        if ((BGCnt[2] & 0x0040) && (BGMosaicSize[0] > 0)) \
        { \
            if (Accelerated) DrawBG_Large<true, DrawPixel_Accel>(line); \
            else DrawBG_Large<true, DrawPixel_Normal>(line); \
        } \
        else \
        { \
            if (Accelerated) DrawBG_Large<false, DrawPixel_Accel>(line); \
            else DrawBG_Large<false, DrawPixel_Normal>(line); \
        } \
    } while (false)

#define DoInterleaveSprites(prio) \
    if (Accelerated) InterleaveSprites<DrawPixel_Accel>(prio); else InterleaveSprites<DrawPixel_Normal>(prio);

template<u32 bgmode>
void GPU2D::DrawScanlineBGMode(u32 line)
{
    for (int i = 3; i >= 0; i--)
    {
        if ((BGCnt[3] & 0x3) == i)
        {
            if (DispCnt & 0x0800)
            {
                if (bgmode >= 3)
                    DoDrawBG(Extended, line, 3)
                else if (bgmode >= 1)
                    DoDrawBG(Affine, line, 3)
                else
                    DoDrawBG(Text, line, 3)
            }
        }
        if ((BGCnt[2] & 0x3) == i)
        {
            if (DispCnt & 0x0400)
            {
                if (bgmode == 5)
                    DoDrawBG(Extended, line, 2)
                else if (bgmode == 4 || bgmode == 2)
                    DoDrawBG(Affine, line, 2)
                else
                    DoDrawBG(Text, line, 2)
            }
        }
        if ((BGCnt[1] & 0x3) == i)
        {
            if (DispCnt & 0x0200)
            {
                DoDrawBG(Text, line, 1)
            }
        }
        if ((BGCnt[0] & 0x3) == i)
        {
            if (DispCnt & 0x0100)
            {
                if ((!Num) && (DispCnt & 0x8))
                    DrawBG_3D();
                else
                    DoDrawBG(Text, line, 0)
            }
        }
        if ((DispCnt & 0x1000) && NumSprites)
            DoInterleaveSprites(0x40000 | (i<<16));
    }
}

void GPU2D::DrawScanlineBGMode6(u32 line)
{
    for (int i = 3; i >= 0; i--)
    {
        if ((BGCnt[2] & 0x3) == i)
        {
            if (DispCnt & 0x0400)
            {
                DoDrawBG_Large(line);
            }
        }
        if ((BGCnt[0] & 0x3) == i)
        {
            if (DispCnt & 0x0100)
            {
                if ((!Num) && (DispCnt & 0x8))
                    DrawBG_3D();
            }
        }
        if ((DispCnt & 0x1000) && NumSprites)
            DoInterleaveSprites(0x40000 | (i<<16))
    }
}

void GPU2D::DrawScanlineBGMode7(u32 line)
{
    // mode 7 only has text-mode BG0 and BG1

    for (int i = 3; i >= 0; i--)
    {
        if ((BGCnt[1] & 0x3) == i)
        {
            if (DispCnt & 0x0200)
            {
                DoDrawBG(Text, line, 1)
            }
        }
        if ((BGCnt[0] & 0x3) == i)
        {
            if (DispCnt & 0x0100)
            {
                if ((!Num) && (DispCnt & 0x8))
                    DrawBG_3D();
                else
                    DoDrawBG(Text, line, 0)
            }
        }
        if ((DispCnt & 0x1000) && NumSprites)
            DoInterleaveSprites(0x40000 | (i<<16))
    }
}

void GPU2D::DrawScanline_BGOBJ(u32 line)
{
    // forced blank disables BG/OBJ compositing
    if (DispCnt & (1<<7))
    {
        for (int i = 0; i < 256; i++)
            BGOBJLine[i] = 0xFF3F3F3F;

        return;
    }

    u64 backdrop;
    if (Num) backdrop = *(u16*)&GPU::Palette[0x400];
    else     backdrop = *(u16*)&GPU::Palette[0];

    {
        u8 r = (backdrop & 0x001F) << 1;
        u8 g = (backdrop & 0x03E0) >> 4;
        u8 b = (backdrop & 0x7C00) >> 9;

        backdrop = r | (g << 8) | (b << 16) | 0x20000000;
        backdrop |= (backdrop << 32);

        for (int i = 0; i < 256; i+=2)
            *(u64*)&BGOBJLine[i] = backdrop;
    }

    if (DispCnt & 0xE000)
        CalculateWindowMask(line);
    else
        memset(WindowMask, 0xFF, 256);

    ApplySpriteMosaicX();

    switch (DispCnt & 0x7)
    {
    case 0: DrawScanlineBGMode<0>(line); break;
    case 1: DrawScanlineBGMode<1>(line); break;
    case 2: DrawScanlineBGMode<2>(line); break;
    case 3: DrawScanlineBGMode<3>(line); break;
    case 4: DrawScanlineBGMode<4>(line); break;
    case 5: DrawScanlineBGMode<5>(line); break;
    case 6: DrawScanlineBGMode6(line); break;
    case 7: DrawScanlineBGMode7(line); break;
    }

    // color special effects
    // can likely be optimized

    if (!Accelerated)
    {
        for (int i = 0; i < 256; i++)
        {
            u32 val1 = BGOBJLine[i];
            u32 val2 = BGOBJLine[256+i];

            BGOBJLine[i] = ColorComposite(i, val1, val2);
        }
    }
    else
    {
        if (Num == 0)
        {
            for (int i = 0; i < 256; i++)
            {
                u32 val1 = BGOBJLine[i];
                u32 val2 = BGOBJLine[256+i];
                u32 val3 = BGOBJLine[512+i];

                u32 flag1 = val1 >> 24;
                u32 flag2 = val2 >> 24;

                u32 bldcnteffect = (BlendCnt >> 6) & 0x3;

                u32 target1;
                if      (flag1 & 0x80) target1 = 0x0010;
                else if (flag1 & 0x40) target1 = 0x0001;
                else                   target1 = flag1;

                u32 target2;
                if      (flag2 & 0x80) target2 = 0x1000;
                else if (flag2 & 0x40) target2 = 0x0100;
                else                   target2 = flag2 << 8;

                if (((flag1 & 0xC0) == 0x40) && (BlendCnt & target2))
                {
                    // 3D on top, blending

                    BGOBJLine[i]     = val2;
                    BGOBJLine[256+i] = ColorComposite(i, val2, val3);
                    BGOBJLine[512+i] = 0x04000000 | (val1 & 0xFF);
                }
                else if ((flag1 & 0xC0) == 0x40)
                {
                    // 3D on top, normal/fade

                    if (bldcnteffect == 1)       bldcnteffect = 0;
                    if (!(BlendCnt & 0x0001))    bldcnteffect = 0;
                    if (!(WindowMask[i] & 0x20)) bldcnteffect = 0;

                    BGOBJLine[i]     = val2;
                    BGOBJLine[256+i] = ColorComposite(i, val2, val3);
                    BGOBJLine[512+i] = (bldcnteffect << 24) | (EVY << 8) | (val1 & 0xFF);
                }
                else if (((flag2 & 0xC0) == 0x40) && ((BlendCnt & 0x01C0) == 0x0140))
                {
                    // 3D on bottom, blending

                    u32 eva, evb;
                    if ((flag1 & 0xC0) == 0xC0)
                    {
                        eva = flag1 & 0x1F;
                        evb = 16 - eva;
                    }
                    else if (((BlendCnt & target1) && (WindowMask[i] & 0x20)) ||
                             ((flag1 & 0xC0) == 0x80))
                    {
                        eva = EVA;
                        evb = EVB;
                    }
                    else
                        bldcnteffect = 7;

                    BGOBJLine[i]     = val1;
                    BGOBJLine[256+i] = ColorComposite(i, val1, val3);
                    BGOBJLine[512+i] = (bldcnteffect << 24) | (EVB << 16) | (EVA << 8) | (val2 & 0xFF);
                }
                else
                {
                    // no potential 3D pixel involved

                    BGOBJLine[i]     = ColorComposite(i, val1, val2);
                    BGOBJLine[256+i] = 0;
                    BGOBJLine[512+i] = 0x07000000;
                }
            }
        }
        else
        {
            for (int i = 0; i < 256; i++)
            {
                u32 val1 = BGOBJLine[i];
                u32 val2 = BGOBJLine[256+i];

                BGOBJLine[i]     = ColorComposite(i, val1, val2);
                BGOBJLine[256+i] = 0;
                BGOBJLine[512+i] = 0x07000000;
            }
        }
    }

    if (BGMosaicY >= BGMosaicYMax)
    {
        BGMosaicY = 0;
        BGMosaicYMax = BGMosaicSize[1];
    }
    else
        BGMosaicY++;

    /*if (OBJMosaicY >= OBJMosaicYMax)
    {
        OBJMosaicY = 0;
        OBJMosaicYMax = OBJMosaicSize[1];
    }
    else
        OBJMosaicY++;*/
}


void GPU2D::DrawPixel_Normal(u32* dst, u16 color, u32 flag)
{
    u8 r = (color & 0x001F) << 1;
    u8 g = (color & 0x03E0) >> 4;
    u8 b = (color & 0x7C00) >> 9;
    //g |= ((color & 0x8000) >> 15);

    *(dst+256) = *dst;
    *dst = r | (g << 8) | (b << 16) | flag;
}

void GPU2D::DrawPixel_Accel(u32* dst, u16 color, u32 flag)
{
    u8 r = (color & 0x001F) << 1;
    u8 g = (color & 0x03E0) >> 4;
    u8 b = (color & 0x7C00) >> 9;

    *(dst+512) = *(dst+256);
    *(dst+256) = *dst;
    *dst = r | (g << 8) | (b << 16) | flag;
}

void GPU2D::DrawBG_3D()
{
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

    if (Accelerated)
    {
        for (; i < iend; i++)
        {
            int pos = xoff++;

            if (!(WindowMask[i] & 0x01)) continue;

            BGOBJLine[i+512] = BGOBJLine[i+256];
            BGOBJLine[i+256] = BGOBJLine[i];
            BGOBJLine[i] = 0x40000000 | pos; // 3D-layer placeholder
        }
    }
    else
    {
        for (; i < iend; i++)
        {
            u32 c = _3DLine[xoff];
            xoff++;

            if ((c >> 24) == 0) continue;
            if (!(WindowMask[i] & 0x01)) continue;

            BGOBJLine[i+256] = BGOBJLine[i];
            BGOBJLine[i] = c | 0x40000000;
        }
    }
}

void GetBGVRAM(u32 num, u8*& data, u32& mask)
{
    if (num == 0)
    {
        data = GPU::VRAMFlat_ABG;
        mask = 0x7FFFF;
    }
    else
    {
        data = GPU::VRAMFlat_BBG;
        mask = 0x1FFFF;
    }
}

template<bool mosaic, GPU2D::DrawPixel drawPixel>
void GPU2D::DrawBG_Text(u32 line, u32 bgnum)
{
    u16 bgcnt = BGCnt[bgnum];

    u32 tilesetaddr, tilemapaddr;
    u16* pal;
    u32 extpal, extpalslot;

    u16 xoff = BGXPos[bgnum];
    u16 yoff = BGYPos[bgnum] + line;

    if (bgcnt & 0x0040)
    {
        // vertical mosaic
        yoff -= BGMosaicY;
    }

    u32 widexmask = (bgcnt & 0x4000) ? 0x100 : 0;

    extpal = (DispCnt & 0x40000000);
    if (extpal) extpalslot = ((bgnum<2) && (bgcnt&0x2000)) ? (2+bgnum) : bgnum;

    u8* bgvram;
    u32 bgvrammask;
    GetBGVRAM(Num, bgvram, bgvrammask);
    if (Num)
    {
        tilesetaddr = ((bgcnt & 0x003C) << 12);
        tilemapaddr = ((bgcnt & 0x1F00) << 3);

        pal = (u16*)&GPU::Palette[0x400];
    }
    else
    {
        tilesetaddr = ((DispCnt & 0x07000000) >> 8) + ((bgcnt & 0x003C) << 12);
        tilemapaddr = ((DispCnt & 0x38000000) >> 11) + ((bgcnt & 0x1F00) << 3);

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
    u8 color;
    u32 lastxpos;

    if (bgcnt & 0x0080)
    {
        // 256-color

        // preload shit as needed
        if ((xoff & 0x7) || mosaic)
        {
            curtile = *(u16*)&bgvram[(tilemapaddr + ((xoff & 0xF8) >> 2) + ((xoff & widexmask) << 3)) & bgvrammask];

            if (extpal) curpal = GetBGExtPal(extpalslot, curtile>>12);
            else        curpal = pal;

            pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 6)
                                     + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 3);
        }

        if (mosaic) lastxpos = xoff;

        for (int i = 0; i < 256; i++)
        {
            u32 xpos;
            if (mosaic) xpos = xoff - CurBGXMosaicTable[i];
            else        xpos = xoff;

            if ((!mosaic && (!(xpos & 0x7))) ||
                (mosaic && ((xpos >> 3) != (lastxpos >> 3))))
            {
                // load a new tile
                curtile = *(u16*)&bgvram[(tilemapaddr + ((xpos & 0xF8) >> 2) + ((xpos & widexmask) << 3)) & bgvrammask];

                if (extpal) curpal = GetBGExtPal(extpalslot, curtile>>12);
                else        curpal = pal;

                pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 6)
                                         + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 3);

                if (mosaic) lastxpos = xpos;
            }

            // draw pixel
            if (WindowMask[i] & (1<<bgnum))
            {
                u32 tilexoff = (curtile & 0x0400) ? (7-(xpos&0x7)) : (xpos&0x7);
                color = bgvram[(pixelsaddr + tilexoff) & bgvrammask];

                if (color)
                    drawPixel(&BGOBJLine[i], curpal[color], 0x01000000<<bgnum);
            }

            xoff++;
        }
    }
    else
    {
        // 16-color

        // preload shit as needed
        if ((xoff & 0x7) || mosaic)
        {
            curtile = *(u16*)&bgvram[((tilemapaddr + ((xoff & 0xF8) >> 2) + ((xoff & widexmask) << 3))) & bgvrammask];
            curpal = pal + ((curtile & 0xF000) >> 8);
            pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 5)
                                     + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 2);
        }

        if (mosaic) lastxpos = xoff;

        for (int i = 0; i < 256; i++)
        {
            u32 xpos;
            if (mosaic) xpos = xoff - CurBGXMosaicTable[i];
            else        xpos = xoff;

            if ((!mosaic && (!(xpos & 0x7))) ||
                (mosaic && ((xpos >> 3) != (lastxpos >> 3))))
            {
                // load a new tile
                curtile = *(u16*)&bgvram[(tilemapaddr + ((xpos & 0xF8) >> 2) + ((xpos & widexmask) << 3)) & bgvrammask];
                curpal = pal + ((curtile & 0xF000) >> 8);
                pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 5)
                                         + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 2);

                if (mosaic) lastxpos = xpos;
            }

            // draw pixel
            if (WindowMask[i] & (1<<bgnum))
            {
                u32 tilexoff = (curtile & 0x0400) ? (7-(xpos&0x7)) : (xpos&0x7);
                if (tilexoff & 0x1)
                {
                    color = bgvram[(pixelsaddr + (tilexoff >> 1)) & bgvrammask] >> 4;
                }
                else
                {
                    color = bgvram[(pixelsaddr + (tilexoff >> 1)) & bgvrammask] & 0x0F;
                }

                if (color)
                    drawPixel(&BGOBJLine[i], curpal[color], 0x01000000<<bgnum);
            }

            xoff++;
        }
    }
}

template<bool mosaic, GPU2D::DrawPixel drawPixel>
void GPU2D::DrawBG_Affine(u32 line, u32 bgnum)
{
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

    if (bgcnt & 0x0040)
    {
        // vertical mosaic
        rotX -= (BGMosaicY * rotB);
        rotY -= (BGMosaicY * rotD);
    }

    u8* bgvram;
    u32 bgvrammask;

    if (Num)
    {
        tilesetaddr = ((bgcnt & 0x003C) << 12);
        tilemapaddr = ((bgcnt & 0x1F00) << 3);

        pal = (u16*)&GPU::Palette[0x400];
    }
    else
    {
        tilesetaddr = ((DispCnt & 0x07000000) >> 8) + ((bgcnt & 0x003C) << 12);
        tilemapaddr = ((DispCnt & 0x38000000) >> 11) + ((bgcnt & 0x1F00) << 3);

        pal = (u16*)&GPU::Palette[0];
    }

    u16 curtile;
    u8 color;

    yshift -= 3;

    for (int i = 0; i < 256; i++)
    {
        if (WindowMask[i] & (1<<bgnum))
        {
            s32 finalX, finalY;
            if (mosaic)
            {
                int im = CurBGXMosaicTable[i];
                finalX = rotX - (im * rotA);
                finalY = rotY - (im * rotC);
            }
            else
            {
                finalX = rotX;
                finalY = rotY;
            }

            if ((!((finalX|finalY) & overflowmask)))
            {
                curtile = bgvram[(tilemapaddr + ((((finalY & coordmask) >> 11) << yshift) + ((finalX & coordmask) >> 11))) & bgvrammask];

                // draw pixel
                u32 tilexoff = (finalX >> 8) & 0x7;
                u32 tileyoff = (finalY >> 8) & 0x7;

                color = bgvram[(tilesetaddr + (curtile << 6) + (tileyoff << 3) + tilexoff) & bgvrammask];

                if (color)
                    drawPixel(&BGOBJLine[i], pal[color], 0x01000000<<bgnum);
            }
        }

        rotX += rotA;
        rotY += rotC;
    }

    BGXRefInternal[bgnum-2] += rotB;
    BGYRefInternal[bgnum-2] += rotD;
}

template<bool mosaic, GPU2D::DrawPixel drawPixel>
void GPU2D::DrawBG_Extended(u32 line, u32 bgnum)
{
    u16 bgcnt = BGCnt[bgnum];

    u32 tilesetaddr, tilemapaddr;
    u16* pal;
    u32 extpal;

    u8* bgvram;
    u32 bgvrammask;
    GetBGVRAM(Num, bgvram, bgvrammask);

    extpal = (DispCnt & 0x40000000);

    s16 rotA = BGRotA[bgnum-2];
    s16 rotB = BGRotB[bgnum-2];
    s16 rotC = BGRotC[bgnum-2];
    s16 rotD = BGRotD[bgnum-2];

    s32 rotX = BGXRefInternal[bgnum-2];
    s32 rotY = BGYRefInternal[bgnum-2];

    if (bgcnt & 0x0040)
    {
        // vertical mosaic
        rotX -= (BGMosaicY * rotB);
        rotY -= (BGMosaicY * rotD);
    }

    if (bgcnt & 0x0080)
    {
        // bitmap modes

        u32 xmask, ymask;
        u32 yshift;
        switch (bgcnt & 0xC000)
        {
        case 0x0000: xmask = 0x07FFF; ymask = 0x07FFF; yshift = 7; break;
        case 0x4000: xmask = 0x0FFFF; ymask = 0x0FFFF; yshift = 8; break;
        case 0x8000: xmask = 0x1FFFF; ymask = 0x0FFFF; yshift = 9; break;
        case 0xC000: xmask = 0x1FFFF; ymask = 0x1FFFF; yshift = 9; break;
        }

        u32 ofxmask, ofymask;
        if (bgcnt & 0x2000)
        {
            ofxmask = 0;
            ofymask = 0;
        }
        else
        {
            ofxmask = ~xmask;
            ofymask = ~ymask;
        }

        if (Num) tilemapaddr = ((bgcnt & 0x1F00) << 6);
        else     tilemapaddr = ((bgcnt & 0x1F00) << 6);

        if (bgcnt & 0x0004)
        {
            // direct color bitmap

            u16 color;

            for (int i = 0; i < 256; i++)
            {
                if (WindowMask[i] & (1<<bgnum))
                {
                    s32 finalX, finalY;
                    if (mosaic)
                    {
                        int im = CurBGXMosaicTable[i];
                        finalX = rotX - (im * rotA);
                        finalY = rotY - (im * rotC);
                    }
                    else
                    {
                        finalX = rotX;
                        finalY = rotY;
                    }

                    if (!(finalX & ofxmask) && !(finalY & ofymask))
                    {
                        color = *(u16*)&bgvram[(tilemapaddr + (((((finalY & ymask) >> 8) << yshift) + ((finalX & xmask) >> 8)) << 1)) & bgvrammask];

                        if (color & 0x8000)
                            drawPixel(&BGOBJLine[i], color, 0x01000000<<bgnum);
                    }
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

            u8 color;

            for (int i = 0; i < 256; i++)
            {
                if (WindowMask[i] & (1<<bgnum))
                {
                    s32 finalX, finalY;
                    if (mosaic)
                    {
                        int im = CurBGXMosaicTable[i];
                        finalX = rotX - (im * rotA);
                        finalY = rotY - (im * rotC);
                    }
                    else
                    {
                        finalX = rotX;
                        finalY = rotY;
                    }

                    if (!(finalX & ofxmask) && !(finalY & ofymask))
                    {
                        color = bgvram[(tilemapaddr + (((finalY & ymask) >> 8) << yshift) + ((finalX & xmask) >> 8)) & bgvrammask];

                        if (color)
                            drawPixel(&BGOBJLine[i], pal[color], 0x01000000<<bgnum);
                    }
                }

                rotX += rotA;
                rotY += rotC;
            }
        }
    }
    else
    {
        // mixed affine/text mode

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

        if (Num)
        {
            tilesetaddr = ((bgcnt & 0x003C) << 12);
            tilemapaddr = ((bgcnt & 0x1F00) << 3);

            pal = (u16*)&GPU::Palette[0x400];
        }
        else
        {
            tilesetaddr = ((DispCnt & 0x07000000) >> 8) + ((bgcnt & 0x003C) << 12);
            tilemapaddr = ((DispCnt & 0x38000000) >> 11) + ((bgcnt & 0x1F00) << 3);

            pal = (u16*)&GPU::Palette[0];
        }

        u16 curtile;
        u16* curpal;
        u8 color;

        yshift -= 3;

        for (int i = 0; i < 256; i++)
        {
            if (WindowMask[i] & (1<<bgnum))
            {
                s32 finalX, finalY;
                if (mosaic)
                {
                    int im = CurBGXMosaicTable[i];
                    finalX = rotX - (im * rotA);
                    finalY = rotY - (im * rotC);
                }
                else
                {
                    finalX = rotX;
                    finalY = rotY;
                }

                if ((!((finalX|finalY) & overflowmask)))
                {
                    curtile = *(u16*)&bgvram[(tilemapaddr + (((((finalY & coordmask) >> 11) << yshift) + ((finalX & coordmask) >> 11)) << 1)) & bgvrammask];

                    if (extpal) curpal = GetBGExtPal(bgnum, curtile>>12);
                    else        curpal = pal;

                    // draw pixel
                    u32 tilexoff = (finalX >> 8) & 0x7;
                    u32 tileyoff = (finalY >> 8) & 0x7;

                    if (curtile & 0x0400) tilexoff = 7-tilexoff;
                    if (curtile & 0x0800) tileyoff = 7-tileyoff;

                    color = bgvram[(tilesetaddr + ((curtile & 0x03FF) << 6) + (tileyoff << 3) + tilexoff) & bgvrammask];

                    if (color)
                        drawPixel(&BGOBJLine[i], curpal[color], 0x01000000<<bgnum);
                }
            }

            rotX += rotA;
            rotY += rotC;
        }
    }

    BGXRefInternal[bgnum-2] += rotB;
    BGYRefInternal[bgnum-2] += rotD;
}

template<bool mosaic, GPU2D::DrawPixel drawPixel>
void GPU2D::DrawBG_Large(u32 line) // BG is always BG2
{
    u16 bgcnt = BGCnt[2];

    u32 tilesetaddr, tilemapaddr;
    u16* pal;

    // large BG sizes:
    // 0: 512x1024
    // 1: 1024x512
    // 2: 512x256
    // 3: 512x512
    u32 xmask, ymask;
    u32 yshift;
    switch (bgcnt & 0xC000)
    {
    case 0x0000: xmask = 0x1FFFF; ymask = 0x3FFFF; yshift = 9; break;
    case 0x4000: xmask = 0x3FFFF; ymask = 0x1FFFF; yshift = 10; break;
    case 0x8000: xmask = 0x1FFFF; ymask = 0x0FFFF; yshift = 9; break;
    case 0xC000: xmask = 0x1FFFF; ymask = 0x1FFFF; yshift = 9; break;
    }

    u32 ofxmask, ofymask;
    if (bgcnt & 0x2000)
    {
        ofxmask = 0;
        ofymask = 0;
    }
    else
    {
        ofxmask = ~xmask;
        ofymask = ~ymask;
    }

    s16 rotA = BGRotA[0];
    s16 rotB = BGRotB[0];
    s16 rotC = BGRotC[0];
    s16 rotD = BGRotD[0];

    s32 rotX = BGXRefInternal[0];
    s32 rotY = BGYRefInternal[0];

    if (bgcnt & 0x0040)
    {
        // vertical mosaic
        rotX -= (BGMosaicY * rotB);
        rotY -= (BGMosaicY * rotD);
    }

    u8* bgvram;
    u32 bgvrammask;
    GetBGVRAM(Num, bgvram, bgvrammask);

    // 256-color bitmap

    if (Num) pal = (u16*)&GPU::Palette[0x400];
    else     pal = (u16*)&GPU::Palette[0];

    u8 color;

    for (int i = 0; i < 256; i++)
    {
        if (WindowMask[i] & (1<<2))
        {
            s32 finalX, finalY;
            if (mosaic)
            {
                int im = CurBGXMosaicTable[i];
                finalX = rotX - (im * rotA);
                finalY = rotY - (im * rotC);
            }
            else
            {
                finalX = rotX;
                finalY = rotY;
            }

            if (!(finalX & ofxmask) && !(finalY & ofymask))
            {
                color = bgvram[(tilemapaddr + (((finalY & ymask) >> 8) << yshift) + ((finalX & xmask) >> 8)) & bgvrammask];

                if (color)
                    drawPixel(&BGOBJLine[i], pal[color], 0x01000000<<2);
            }
        }

        rotX += rotA;
        rotY += rotC;
    }

    BGXRefInternal[0] += rotB;
    BGYRefInternal[0] += rotD;
}

// OBJ line buffer:
// * bit0-15: color (bit15=1: direct color, bit15=0: palette index, bit12=0 to indicate extpal)
// * bit16-17: BG-relative priority
// * bit18: non-transparent sprite pixel exists here
// * bit19: X mosaic should be applied here
// * bit24-31: compositor flags

void GPU2D::ApplySpriteMosaicX()
{
    // apply X mosaic if needed
    // X mosaic for sprites is applied after all sprites are rendered

    if (OBJMosaicSize[0] == 0) return;

    u32 lastcolor = OBJLine[0];

    for (u32 i = 1; i < 256; i++)
    {
        if (!(OBJLine[i] & 0x100000))
        {
            // not a mosaic'd sprite pixel
            continue;
        }

        if ((OBJIndex[i] != OBJIndex[i-1]) || (CurOBJXMosaicTable[i] == 0))
            lastcolor = OBJLine[i];
        else
            OBJLine[i] = lastcolor;
    }
}

template <GPU2D::DrawPixel drawPixel>
void GPU2D::InterleaveSprites(u32 prio)
{
    u16* pal = (u16*)&GPU::Palette[Num ? 0x600 : 0x200];

    if (DispCnt & 0x80000000)
    {
        u16* extpal = GetOBJExtPal();

        for (u32 i = 0; i < 256; i++)
        {
            if ((OBJLine[i] & 0x70000) != prio) continue;
            if (!(WindowMask[i] & 0x10))        continue;

            u16 color;
            u32 pixel = OBJLine[i];

            if (pixel & 0x8000)
                color = pixel & 0x7FFF;
            else if (pixel & 0x1000)
                color = pal[pixel & 0xFF];
            else
                color = extpal[pixel & 0xFFF];

            drawPixel(&BGOBJLine[i], color, pixel & 0xFF000000);
        }
    }
    else
    {
        // optimized no-extpal version

        for (u32 i = 0; i < 256; i++)
        {
            if ((OBJLine[i] & 0x70000) != prio) continue;
            if (!(WindowMask[i] & 0x10))        continue;

            u16 color;
            u32 pixel = OBJLine[i];

            if (pixel & 0x8000)
                color = pixel & 0x7FFF;
            else
                color = pal[pixel & 0xFF];

            drawPixel(&BGOBJLine[i], color, pixel & 0xFF000000);
        }
    }
}

void GetOBJVRAM(u32 num, u8*& data, u32& mask)
{
    if (num == 0)
    {
        data = GPU::VRAMFlat_AOBJ;
        mask = 0x3FFFF;
    }
    else
    {
        data = GPU::VRAMFlat_BOBJ;
        mask = 0x1FFFF;
    }
}

#define DoDrawSprite(type, ...) \
    if (iswin) \
    { \
        DrawSprite_##type<true>(__VA_ARGS__); \
    } \
    else \
    { \
        DrawSprite_##type<false>(__VA_ARGS__); \
    }

void GPU2D::DrawSprites(u32 line)
{
    if (line == 0)
    {
        // reset those counters here
        // TODO: find out when those are supposed to be reset
        // it would make sense to reset them at the end of VBlank
        // however, sprites are rendered one scanline in advance
        // so they need to be reset a bit earlier

        OBJMosaicY = 0;
        OBJMosaicYCount = 0;
    }

    if (Num == 0)
    {
        auto objDirty = GPU::VRAMDirty_AOBJ.DeriveState(GPU::VRAMMap_AOBJ);
        GPU::MakeVRAMFlat_AOBJCoherent(objDirty);
    }
    else
    {
        auto objDirty = GPU::VRAMDirty_BOBJ.DeriveState(GPU::VRAMMap_BOBJ);
        GPU::MakeVRAMFlat_BOBJCoherent(objDirty);
    }

    NumSprites = 0;
    memset(OBJLine, 0, 256*4);
    memset(OBJWindow, 0, 256);
    if (!(DispCnt & 0x1000)) return;

    memset(OBJIndex, 0xFF, 256);

    u16* oam = (u16*)&GPU::OAM[Num ? 0x400 : 0];

    const s32 spritewidth[16] =
    {
        8, 16, 8, 8,
        16, 32, 8, 8,
        32, 32, 16, 8,
        64, 64, 32, 8
    };
    const s32 spriteheight[16] =
    {
        8, 8, 16, 8,
        16, 8, 32, 8,
        32, 16, 32, 8,
        64, 32, 64, 8
    };

    for (int bgnum = 0x0C00; bgnum >= 0x0000; bgnum -= 0x0400)
    {
        for (int sprnum = 127; sprnum >= 0; sprnum--)
        {
            u16* attrib = &oam[sprnum*4];

            if ((attrib[2] & 0x0C00) != bgnum)
                continue;

            bool iswin = (((attrib[0] >> 10) & 0x3) == 2);

            u32 sprline;
            if ((attrib[0] & 0x1000) && !iswin)
            {
                // apply Y mosaic
                sprline = OBJMosaicY;
            }
            else
                sprline = line;

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
                ypos = (sprline - ypos) & 0xFF;
                if (ypos >= (u32)boundheight)
                    continue;

                s32 xpos = (s32)(attrib[1] << 23) >> 23;
                if (xpos <= -boundwidth)
                    continue;

                u32 rotparamgroup = (attrib[1] >> 9) & 0x1F;

                DoDrawSprite(Rotscale, sprnum, boundwidth, boundheight, width, height, xpos, ypos);

                NumSprites++;
            }
            else
            {
                if (attrib[0] & 0x0200)
                    continue;

                u32 sizeparam = (attrib[0] >> 14) | ((attrib[1] & 0xC000) >> 12);
                s32 width = spritewidth[sizeparam];
                s32 height = spriteheight[sizeparam];

                u32 ypos = attrib[0] & 0xFF;
                ypos = (sprline - ypos) & 0xFF;
                if (ypos >= (u32)height)
                    continue;

                s32 xpos = (s32)(attrib[1] << 23) >> 23;
                if (xpos <= -width)
                    continue;

                DoDrawSprite(Normal, sprnum, width, height, xpos, ypos);

                NumSprites++;
            }
        }
    }
}

template<bool window>
void GPU2D::DrawSprite_Rotscale(u32 num, u32 boundwidth, u32 boundheight, u32 width, u32 height, s32 xpos, s32 ypos)
{
    u16* oam = (u16*)&GPU::OAM[Num ? 0x400 : 0];
    u16* attrib = &oam[num * 4];
    u16* rotparams = &oam[(((attrib[1] >> 9) & 0x1F) * 16) + 3];

    u32 pixelattr = ((attrib[2] & 0x0C00) << 6) | 0xC0000;
    u32 tilenum = attrib[2] & 0x03FF;
    u32 spritemode = window ? 0 : ((attrib[0] >> 10) & 0x3);

    u32 ytilefactor;

    u8* objvram;
    u32 objvrammask;
    GetOBJVRAM(Num, objvram, objvrammask);

    s32 centerX = boundwidth >> 1;
    s32 centerY = boundheight >> 1;

    if ((attrib[0] & 0x1000) && !window)
    {
        // apply Y mosaic
        pixelattr |= 0x100000;
    }

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

    u16 color = 0; // transparent in all cases

    if (spritemode == 3)
    {
        u32 alpha = attrib[2] >> 12;
        if (!alpha) return;
        alpha++;

        pixelattr |= (0xC0000000 | (alpha << 24));

        u32 pixelsaddr;
        if (DispCnt & 0x40)
        {
            if (DispCnt & 0x20)
            {
                // 'reserved'
                // draws nothing

                return;
            }
            else
            {
                pixelsaddr = tilenum << (7 + ((DispCnt >> 22) & 0x1));
                ytilefactor = ((width >> 8) * 2);
            }
        }
        else
        {
            if (DispCnt & 0x20)
            {
                pixelsaddr = ((tilenum & 0x01F) << 4) + ((tilenum & 0x3E0) << 7);
                ytilefactor = (256 * 2);
            }
            else
            {
                pixelsaddr = ((tilenum & 0x00F) << 4) + ((tilenum & 0x3F0) << 7);
                ytilefactor = (128 * 2);
            }
        }

        for (; xoff < boundwidth;)
        {
            if ((u32)rotX < width && (u32)rotY < height)
            {
                color = *(u16*)&objvram[(pixelsaddr + ((rotY >> 8) * ytilefactor) + ((rotX >> 8) << 1)) & objvrammask];

                if (color & 0x8000)
                {
                    if (window) OBJWindow[xpos] = 1;
                    else      { OBJLine[xpos] = color | pixelattr; OBJIndex[xpos] = num; }
                }
                else if (!window)
                {
                    if (OBJLine[xpos] == 0)
                    {
                        OBJLine[xpos] = pixelattr & 0x180000;
                        OBJIndex[xpos] = num;
                    }
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
        u32 pixelsaddr = tilenum;
        if (DispCnt & 0x10)
        {
            pixelsaddr <<= ((DispCnt >> 20) & 0x3);
            ytilefactor = (width >> 11) << ((attrib[0] & 0x2000) ? 1:0);
        }
        else
        {
            ytilefactor = 0x20;
        }

        if (spritemode == 1) pixelattr |= 0x80000000;
        else                 pixelattr |= 0x10000000;

        if (attrib[0] & 0x2000)
        {
            // 256-color
            ytilefactor <<= 5;
            pixelsaddr <<= 5;

            if (!window)
            {
                if (!(DispCnt & 0x80000000))
                    pixelattr |= 0x1000;
                else
                    pixelattr |= ((attrib[2] & 0xF000) >> 4);
            }

            for (; xoff < boundwidth;)
            {
                if ((u32)rotX < width && (u32)rotY < height)
                {
                    color = objvram[(pixelsaddr + ((rotY>>11)*ytilefactor) + ((rotY&0x700)>>5) + ((rotX>>11)*64) + ((rotX&0x700)>>8)) & objvrammask];

                    if (color)
                    {
                        if (window) OBJWindow[xpos] = 1;
                        else      { OBJLine[xpos] = color | pixelattr; OBJIndex[xpos] = num; }
                    }
                    else if (!window)
                    {
                        if (OBJLine[xpos] == 0)
                        {
                            OBJLine[xpos] = pixelattr & 0x180000;
                            OBJIndex[xpos] = num;
                        }
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

            if (!window)
            {
                pixelattr |= 0x1000;
                pixelattr |= ((attrib[2] & 0xF000) >> 8);
            }

            for (; xoff < boundwidth;)
            {
                if ((u32)rotX < width && (u32)rotY < height)
                {
                    color = objvram[(pixelsaddr + ((rotY>>11)*ytilefactor) + ((rotY&0x700)>>6) + ((rotX>>11)*32) + ((rotX&0x700)>>9)) & objvrammask];
                    if (rotX & 0x100)
                        color >>= 4;
                    else
                        color &= 0x0F;

                    if (color)
                    {
                        if (window) OBJWindow[xpos] = 1;
                        else      { OBJLine[xpos] = color | pixelattr; OBJIndex[xpos] = num; }
                    }
                    else if (!window)
                    {
                        if (OBJLine[xpos] == 0)
                        {
                            OBJLine[xpos] = pixelattr & 0x180000;
                            OBJIndex[xpos] = num;
                        }
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
void GPU2D::DrawSprite_Normal(u32 num, u32 width, u32 height, s32 xpos, s32 ypos)
{
    u16* oam = (u16*)&GPU::OAM[Num ? 0x400 : 0];
    u16* attrib = &oam[num * 4];

    u32 pixelattr = ((attrib[2] & 0x0C00) << 6) | 0xC0000;
    u32 tilenum = attrib[2] & 0x03FF;
    u32 spritemode = window ? 0 : ((attrib[0] >> 10) & 0x3);

    u32 wmask = width - 8; // really ((width - 1) & ~0x7)

    if ((attrib[0] & 0x1000) && !window)
    {
        // apply Y mosaic
        pixelattr |= 0x100000;
    }

    u8* objvram;
    u32 objvrammask;
    GetOBJVRAM(Num, objvram, objvrammask);

    // yflip
    if (attrib[1] & 0x2000)
        ypos = height-1 - ypos;

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

    u16 color = 0; // transparent in all cases

    if (spritemode == 3)
    {
        // bitmap sprite

        u32 alpha = attrib[2] >> 12;
        if (!alpha) return;
        alpha++;

        pixelattr |= (0xC0000000 | (alpha << 24));

        u32 pixelsaddr = tilenum;
        if (DispCnt & 0x40)
        {
            if (DispCnt & 0x20)
            {
                // 'reserved'
                // draws nothing

                return;
            }
            else
            {
                pixelsaddr <<= (7 + ((DispCnt >> 22) & 0x1));
                pixelsaddr += (ypos * width * 2);
            }
        }
        else
        {
            if (DispCnt & 0x20)
            {
                pixelsaddr = ((tilenum & 0x01F) << 4) + ((tilenum & 0x3E0) << 7);
                pixelsaddr += (ypos * 256 * 2);
            }
            else
            {
                pixelsaddr = ((tilenum & 0x00F) << 4) + ((tilenum & 0x3F0) << 7);
                pixelsaddr += (ypos * 128 * 2);
            }
        }

        s32 pixelstride;

        if (attrib[1] & 0x1000) // xflip
        {
            pixelsaddr += (width-1 << 1);
            pixelsaddr -= (xoff << 1);
            pixelstride = -2;
        }
        else
        {
            pixelsaddr += (xoff << 1);
            pixelstride = 2;
        }

        for (; xoff < xend;)
        {
            color = *(u16*)&objvram[pixelsaddr & objvrammask];

            pixelsaddr += pixelstride;

            if (color & 0x8000)
            {
                if (window) OBJWindow[xpos] = 1;
                else      { OBJLine[xpos] = color | pixelattr; OBJIndex[xpos] = num; }
            }
            else if (!window)
            {
                if (OBJLine[xpos] == 0)
                {
                    OBJLine[xpos] = pixelattr & 0x180000;
                    OBJIndex[xpos] = num;
                }
            }

            xoff++;
            xpos++;
        }
    }
    else
    {
        u32 pixelsaddr = tilenum;
        if (DispCnt & 0x10)
        {
            pixelsaddr <<= ((DispCnt >> 20) & 0x3);
            pixelsaddr += ((ypos >> 3) * (width >> 3)) << ((attrib[0] & 0x2000) ? 1:0);
        }
        else
        {
            pixelsaddr += ((ypos >> 3) * 0x20);
        }

        if (spritemode == 1) pixelattr |= 0x80000000;
        else                 pixelattr |= 0x10000000;

        if (attrib[0] & 0x2000)
        {
            // 256-color
            pixelsaddr <<= 5;
            pixelsaddr += ((ypos & 0x7) << 3);
            s32 pixelstride;

            if (!window)
            {
                if (!(DispCnt & 0x80000000))
                    pixelattr |= 0x1000;
                else
                    pixelattr |= ((attrib[2] & 0xF000) >> 4);
            }

            if (attrib[1] & 0x1000) // xflip
            {
                pixelsaddr += (((width-1) & wmask) << 3);
                pixelsaddr += ((width-1) & 0x7);
                pixelsaddr -= ((xoff & wmask) << 3);
                pixelsaddr -= (xoff & 0x7);
                pixelstride = -1;
            }
            else
            {
                pixelsaddr += ((xoff & wmask) << 3);
                pixelsaddr += (xoff & 0x7);
                pixelstride = 1;
            }

            for (; xoff < xend;)
            {
                color = objvram[pixelsaddr];

                pixelsaddr += pixelstride;

                if (color)
                {
                    if (window) OBJWindow[xpos] = 1;
                    else      { OBJLine[xpos] = color | pixelattr; OBJIndex[xpos] = num; }
                }
                else if (!window)
                {
                    if (OBJLine[xpos] == 0)
                    {
                        OBJLine[xpos] = pixelattr & 0x180000;
                        OBJIndex[xpos] = num;
                    }
                }

                xoff++;
                xpos++;
                if (!(xoff & 0x7)) pixelsaddr += (56 * pixelstride);
            }
        }
        else
        {
            // 16-color
            pixelsaddr <<= 5;
            pixelsaddr += ((ypos & 0x7) << 2);
            s32 pixelstride;

            if (!window)
            {
                pixelattr |= 0x1000;
                pixelattr |= ((attrib[2] & 0xF000) >> 8);
            }

            // TODO: optimize VRAM access!!
            // TODO: do xflip better? the 'two pixels per byte' thing makes it a bit shitty

            if (attrib[1] & 0x1000) // xflip
            {
                pixelsaddr += (((width-1) & wmask) << 2);
                pixelsaddr += (((width-1) & 0x7) >> 1);
                pixelsaddr -= ((xoff & wmask) << 2);
                pixelsaddr -= ((xoff & 0x7) >> 1);
                pixelstride = -1;
            }
            else
            {
                pixelsaddr += ((xoff & wmask) << 2);
                pixelsaddr += ((xoff & 0x7) >> 1);
                pixelstride = 1;
            }

            for (; xoff < xend;)
            {
                if (attrib[1] & 0x1000)
                {
                    if (xoff & 0x1) { color = objvram[pixelsaddr & objvrammask] & 0x0F; pixelsaddr--; }
                    else              color = objvram[pixelsaddr & objvrammask] >> 4;
                }
                else
                {
                    if (xoff & 0x1) { color = objvram[pixelsaddr & objvrammask] >> 4; pixelsaddr++; }
                    else              color = objvram[pixelsaddr & objvrammask] & 0x0F;
                }

                if (color)
                {
                    if (window) OBJWindow[xpos] = 1;
                    else      { OBJLine[xpos] = color | pixelattr; OBJIndex[xpos] = num; }
                }
                else if (!window)
                {
                    if (OBJLine[xpos] == 0)
                    {
                        OBJLine[xpos] = pixelattr & 0x180000;
                        OBJIndex[xpos] = num;
                    }
                }

                xoff++;
                xpos++;
                if (!(xoff & 0x7)) pixelsaddr += ((attrib[1] & 0x1000) ? -28 : 28);
            }
        }
    }
}
