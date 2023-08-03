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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "GPU.h"

using Platform::Log;
using Platform::LogLevel;

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

namespace GPU2D
{

Unit::Unit(u32 num)
{
    Num = num;
}

void Unit::Reset()
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
    OBJMosaicYCount = 0;

    BlendCnt = 0;
    EVA = 16;
    EVB = 0;
    EVY = 0;

    memset(DispFIFO, 0, 16*2);
    DispFIFOReadPtr = 0;
    DispFIFOWritePtr = 0;

    memset(DispFIFOBuffer, 0, 256*2);

    CaptureCnt = 0;
    CaptureLatch = false;

    MasterBrightness = 0;
}

void Unit::DoSavestate(Savestate* file)
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
}

u8 Unit::Read8(u32 addr)
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

    Log(LogLevel::Debug, "unknown GPU read8 %08X\n", addr);
    return 0;
}

u16 Unit::Read16(u32 addr)
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

    Log(LogLevel::Debug, "unknown GPU read16 %08X\n", addr);
    return 0;
}

u32 Unit::Read32(u32 addr)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000: return DispCnt;

    case 0x064: return CaptureCnt;
    }

    return Read16(addr) | (Read16(addr+2) << 16);
}

void Unit::Write8(u32 addr, u8 val)
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

    case 0x10:
        if (!Num) GPU3D::SetRenderXPos((GPU3D::RenderXPos & 0xFF00) | val);
        break;
    case 0x11:
        if (!Num) GPU3D::SetRenderXPos((GPU3D::RenderXPos & 0x00FF) | (val << 8));
        break;
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
        return;
    case 0x04D:
        OBJMosaicSize[0] = val & 0xF;
        OBJMosaicSize[1] = val >> 4;
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

    Log(LogLevel::Debug, "unknown GPU write8 %08X %02X\n", addr, val);
}

void Unit::Write16(u32 addr, u16 val)
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

    case 0x010:
        if (!Num) GPU3D::SetRenderXPos(val);
        break;

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
        OBJMosaicSize[0] = (val >> 8) & 0xF;
        OBJMosaicSize[1] = val >> 12;
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

void Unit::Write32(u32 addr, u32 val)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000:
        DispCnt = val;
        if (Num) DispCnt &= 0xC0B1FFF7;
        return;

    case 0x064:
        CaptureCnt = val & 0xEF3F1F1F;
        return;

    case 0x068:
        DispFIFO[DispFIFOWritePtr] = val & 0xFFFF;
        DispFIFO[DispFIFOWritePtr+1] = val >> 16;
        DispFIFOWritePtr += 2;
        DispFIFOWritePtr &= 0xF;
        return;
    }

    if (Enabled)
    {
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
    }

    Write16(addr, val&0xFFFF);
    Write16(addr+2, val>>16);
}

void Unit::UpdateMosaicCounters(u32 line)
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

void Unit::VBlank()
{
    if (CaptureLatch)
    {
        CaptureCnt &= ~(1<<31);
        CaptureLatch = false;
    }

    DispFIFOReadPtr = 0;
    DispFIFOWritePtr = 0;
}

void Unit::VBlankEnd()
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
}

void Unit::SampleFIFO(u32 offset, u32 num)
{
    for (u32 i = 0; i < num; i++)
    {
        u16 val = DispFIFO[DispFIFOReadPtr];
        DispFIFOReadPtr++;
        DispFIFOReadPtr &= 0xF;

        DispFIFOBuffer[offset+i] = val;
    }
}

u16* Unit::GetBGExtPal(u32 slot, u32 pal)
{
    const u32 PaletteSize = 256 * 2;
    const u32 SlotSize = PaletteSize * 16;
    return (u16*)&(Num == 0
         ? GPU::VRAMFlat_ABGExtPal
         : GPU::VRAMFlat_BBGExtPal)[slot * SlotSize + pal * PaletteSize];
}

u16* Unit::GetOBJExtPal()
{
    return Num == 0
         ? (u16*)GPU::VRAMFlat_AOBJExtPal
         : (u16*)GPU::VRAMFlat_BOBJExtPal;
}

void Unit::CheckWindows(u32 line)
{
    line &= 0xFF;
    if (line == Win0Coords[3])      Win0Active &= ~0x1;
    else if (line == Win0Coords[2]) Win0Active |=  0x1;
    if (line == Win1Coords[3])      Win1Active &= ~0x1;
    else if (line == Win1Coords[2]) Win1Active |=  0x1;
}

void Unit::CalculateWindowMask(u32 line, u8* windowMask, u8* objWindow)
{
    for (u32 i = 0; i < 256; i++)
        windowMask[i] = WinCnt[2]; // window outside

    if (DispCnt & (1<<15))
    {
        // OBJ window
        for (int i = 0; i < 256; i++)
        {
            if (objWindow[i])
                windowMask[i] = WinCnt[3];
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

            if (Win1Active == 0x3) windowMask[i] = WinCnt[1];
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

            if (Win0Active == 0x3) windowMask[i] = WinCnt[0];
        }
    }
}

void Unit::GetBGVRAM(u8*& data, u32& mask)
{
    if (Num == 0)
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

void Unit::GetOBJVRAM(u8*& data, u32& mask)
{
    if (Num == 0)
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

}
