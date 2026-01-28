/*
    Copyright 2016-2025 melonDS team

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
#include "GPU3D.h"

namespace melonDS
{
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
// * [Gericom] bit15 is used as bottom green bit for palettes.
//   applies to any BG/OBJ graphics except direct color
//   does not apply to VRAM display or mainmem FIFO
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


GPU2D::GPU2D(u32 num, melonDS::GPU& gpu) : Num(num), GPU(gpu)
{
}

void GPU2D::Reset()
{
    Enabled = false;

    DispCnt = 0;
    memset(DispCntLatch, 0, sizeof(DispCntLatch));
    LayerEnable = 0;
    OBJEnable = 0;
    ForcedBlank = 0;
    memset(BGCnt, 0, 4*2);
    memset(BGXPos, 0, 4*2);
    memset(BGYPos, 0, 4*2);
    memset(BGXRef, 0, 2*4);
    memset(BGYRef, 0, 2*4);
    memset(BGXRefInternal, 0, 2*4);
    memset(BGYRefInternal, 0, 2*4);
    memset(BGXRefReload, 0, 2*4);
    memset(BGYRefReload, 0, 2*4);
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
    BGMosaicLatch = true;
    OBJMosaicLatch = true;
    BGMosaicLine = 0;
    OBJMosaicLine = 0;

    BlendCnt = 0;
    EVA = 16;
    EVB = 0;
    EVY = 0;
}

void GPU2D::DoSavestate(Savestate* file)
{
    file->Section((char*)(Num ? "GP2B" : "GP2A"));

    file->Var32(&DispCnt);
    file->VarArray(DispCntLatch, sizeof(DispCntLatch));
    file->Var8(&LayerEnable);
    file->Var8(&OBJEnable);
    file->Var8(&ForcedBlank);
    file->VarArray(BGCnt, 4*2);
    file->VarArray(BGXPos, 4*2);
    file->VarArray(BGYPos, 4*2);
    file->VarArray(BGXRef, 2*4);
    file->VarArray(BGYRef, 2*4);
    file->VarArray(BGXRefInternal, 2*4);
    file->VarArray(BGYRefInternal, 2*4);
    file->VarArray(BGXRefReload, 2*4);
    file->VarArray(BGYRefReload, 2*4);
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
    file->VarBool(&BGMosaicLatch);
    file->VarBool(&OBJMosaicLatch);
    file->Var32(&BGMosaicLine);
    file->Var32(&OBJMosaicLine);

    file->Var16(&BlendCnt);
    file->Var16(&BlendAlpha);
    file->Var8(&EVA);
    file->Var8(&EVB);
    file->Var8(&EVY);

    file->Var8(&Win0Active);
    file->Var8(&Win1Active);
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

    case 0x050: return BlendCnt & 0xFF;
    case 0x051: return BlendCnt >> 8;
    case 0x052: return BlendAlpha & 0xFF;
    case 0x053: return BlendAlpha >> 8;

    // there are games accidentally trying to read those
    // those are write-only
    case 0x04C:
    case 0x04D: return 0;
    }

    Log(LogLevel::Debug, "unknown GPU2D read8 %08X\n", addr);
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
    }

    Log(LogLevel::Debug, "unknown GPU2D read16 %08X\n", addr);
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

    case 0x010:
        if (!Num) GPU.GPU3D.SetRenderXPos(val, 0x00FF);
        break;
    case 0x011:
        if (!Num) GPU.GPU3D.SetRenderXPos(val << 8, 0xFF00);
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

    Log(LogLevel::Debug, "unknown GPU2D write8 %08X %02X\n", addr, val);
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

    case 0x010:
        if (!Num) GPU.GPU3D.SetRenderXPos(val, 0xFFFF);
        break;
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
        BGXRefReload[0] = BGXRef[0];
        return;
    case 0x02A:
        if (val & 0x0800) val |= 0xF000;
        BGXRef[0] = (BGXRef[0] & 0xFFFF) | (val << 16);
        BGXRefReload[0] = BGXRef[0];
        return;
    case 0x02C:
        BGYRef[0] = (BGYRef[0] & 0xFFFF0000) | val;
        BGYRefReload[0] = BGYRef[0];
        return;
    case 0x02E:
        if (val & 0x0800) val |= 0xF000;
        BGYRef[0] = (BGYRef[0] & 0xFFFF) | (val << 16);
        BGYRefReload[0] = BGYRef[0];
        return;

    case 0x030: BGRotA[1] = val; return;
    case 0x032: BGRotB[1] = val; return;
    case 0x034: BGRotC[1] = val; return;
    case 0x036: BGRotD[1] = val; return;
    case 0x038:
        BGXRef[1] = (BGXRef[1] & 0xFFFF0000) | val;
        BGXRefReload[1] = BGXRef[1];
        return;
    case 0x03A:
        if (val & 0x0800) val |= 0xF000;
        BGXRef[1] = (BGXRef[1] & 0xFFFF) | (val << 16);
        BGXRefReload[1] = BGXRef[1];
        return;
    case 0x03C:
        BGYRef[1] = (BGYRef[1] & 0xFFFF0000) | val;
        BGYRefReload[1] = BGYRef[1];
        return;
    case 0x03E:
        if (val & 0x0800) val |= 0xF000;
        BGYRef[1] = (BGYRef[1] & 0xFFFF) | (val << 16);
        BGYRefReload[1] = BGYRef[1];
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

    //printf("unknown GPU2D write16 %08X %04X\n", addr, val);
}

void GPU2D::Write32(u32 addr, u32 val)
{
    switch (addr & 0x00000FFF)
    {
    case 0x000:
        DispCnt = val;
        if (Num) DispCnt &= 0xC0B1FFF7;
        return;
    }

    if (!Enabled)
    {
        Write16(addr, val&0xFFFF);
        Write16(addr+2, val>>16);
        return;
    }

    switch (addr & 0x00000FFF)
    {
    case 0x028:
        if (val & 0x08000000) val |= 0xF0000000;
        BGXRef[0] = val;
        BGXRefReload[0] = BGXRef[0];
        return;
    case 0x02C:
        if (val & 0x08000000) val |= 0xF0000000;
        BGYRef[0] = val;
        BGYRefReload[0] = BGYRef[0];
        return;

    case 0x038:
        if (val & 0x08000000) val |= 0xF0000000;
        BGXRef[1] = val;
        BGXRefReload[1] = BGXRef[1];
        return;
    case 0x03C:
        if (val & 0x08000000) val |= 0xF0000000;
        BGYRef[1] = val;
        BGYRefReload[1] = BGYRef[1];
        return;
    }

    Write16(addr, val&0xFFFF);
    Write16(addr+2, val>>16);
}


u16* GPU2D::GetBGExtPal(u32 slot, u32 pal)
{
    const u32 PaletteSize = 256 * 2;
    const u32 SlotSize = PaletteSize * 16;
    return (u16*)&(Num == 0
         ? GPU.VRAMFlat_ABGExtPal
         : GPU.VRAMFlat_BBGExtPal)[slot * SlotSize + pal * PaletteSize];
}

u16* GPU2D::GetOBJExtPal()
{
    return Num == 0
         ? (u16*)GPU.VRAMFlat_AOBJExtPal
         : (u16*)GPU.VRAMFlat_BOBJExtPal;
}


void GPU2D::UpdateRegistersPreDraw(bool reset)
{
    if (!Enabled) return;

    // enabling BG/OBJ layers or disabling forced blank takes two scanlines to apply
    // however, disabling layers or enabling forced blank applies immediately
    DispCntLatch[2] = DispCntLatch[1];
    DispCntLatch[1] = DispCntLatch[0];
    DispCntLatch[0] = DispCnt;
    LayerEnable = ((DispCntLatch[2] & DispCnt) >> 8) & 0x1F;
    OBJEnable = ((DispCntLatch[1] & DispCnt) >> 12) & 0x1;
    ForcedBlank = ((DispCntLatch[2] | DispCnt) >> 7) & 0x1;

    if (BGMosaicLatch)
        BGMosaicLine = GPU.VCount;

    for (int i = 0; i < 2; i++)
    {
        if (!(BGCnt[2+i] & (1<<6)) || BGMosaicLatch)
        {
            BGXRefInternal[i] = BGXRef[i];
            BGYRefInternal[i] = BGYRef[i];
        }
    }

    if (DispCnt & (1<<12))
    {
        // update OBJ mosaic counter

        if (reset || (OBJMosaicY == OBJMosaicSize[1]))
        {
            OBJMosaicY = 0;
            OBJMosaicLatch = true;
        }
        else
        {
            OBJMosaicY++;
            OBJMosaicY &= 0xF;
            OBJMosaicLatch = false;
        }
    }

    if (OBJMosaicLatch)
        OBJMosaicLine = reset ? 0 : (GPU.VCount+1);
}

void GPU2D::UpdateRegistersPostDraw(bool reset)
{
    if (!Enabled) return;

    if (reset)
    {
        BGMosaicYMax = BGMosaicSize[1];
        BGMosaicY = 0;
        BGMosaicLatch = true;
    }
    else
    {
        // for BG mosaic, the size in MOSAIC is copied to an internal register
        // on the other hand, OBJ mosaic directly checks against the size in MOSAIC
        // this makes the OBJ mosaic counter prone to overflowing if MOSAIC is modified midframe

        if (BGMosaicY == BGMosaicYMax)
        {
            BGMosaicYMax = BGMosaicSize[1];
            BGMosaicY = 0;
            BGMosaicLatch = true;
        }
        else
        {
            BGMosaicY++;
            BGMosaicY &= 0xF;
            BGMosaicLatch = false;
        }
    }

    for (int i = 0; i < 2; i++)
    {
        // reference points for rotscale layers are only updated if the layer is enabled
        // TODO do they get updated if the layer isn't a rotscale layer?
        if (!(LayerEnable & (4<<i)))
            continue;

        if (reset)
        {
            BGXRef[i] = BGXRefReload[i];
            BGYRef[i] = BGYRefReload[i];
        }
        else
        {
            BGXRef[i] += BGRotB[i];
            BGYRef[i] += BGRotD[i];
        }
    }
}

void GPU2D::UpdateWindows(u32 line)
{
    if (!Enabled) return;

    // this seems to be done at the very beginning of each scanline
    // this also seems to be done always, even if windows are disabled
    line &= 0xFF;
    if (line == Win0Coords[3])      Win0Active &= ~0x1;
    else if (line == Win0Coords[2]) Win0Active |=  0x1;
    if (line == Win1Coords[3])      Win1Active &= ~0x1;
    else if (line == Win1Coords[2]) Win1Active |=  0x1;
}

void GPU2D::CalculateWindowMask(u8* windowMask, const u8* objWindow)
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

void GPU2D::GetBGVRAM(u8*& data, u32& mask) const
{
    if (Num == 0)
    {
        data = GPU.VRAMFlat_ABG;
        mask = 0x7FFFF;
    }
    else
    {
        data = GPU.VRAMFlat_BBG;
        mask = 0x1FFFF;
    }
}

void GPU2D::GetOBJVRAM(u8*& data, u32& mask) const
{
    if (Num == 0)
    {
        data = GPU.VRAMFlat_AOBJ;
        mask = 0x3FFFF;
    }
    else
    {
        data = GPU.VRAMFlat_BOBJ;
        mask = 0x1FFFF;
    }
}

void GPU2D::GetCaptureInfo_BG(int* info) const
{
    if (Num == 0)
        return GPU.GetCaptureInfo_ABG(info);
    else
        return GPU.GetCaptureInfo_BBG(info);
}

void GPU2D::GetCaptureInfo_OBJ(int* info) const
{
    if (Num == 0)
        return GPU.GetCaptureInfo_AOBJ(info);
    else
        return GPU.GetCaptureInfo_BOBJ(info);
}

}
