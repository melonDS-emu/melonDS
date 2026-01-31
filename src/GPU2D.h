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

#ifndef GPU2D_H
#define GPU2D_H

#include "types.h"
#include "Savestate.h"

namespace melonDS
{
class GPU;

class GPU2D
{
public:
    // take a reference to the GPU so we can access its state
    // and ensure that it's not null
    GPU2D(u32 num, melonDS::GPU& gpu);
    virtual ~GPU2D() = default;
    GPU2D(const GPU2D&) = delete;
    GPU2D& operator=(const GPU2D&) = delete;

    void Reset();

    void DoSavestate(Savestate* file);

    void SetEnabled(bool enable) { Enabled = enable; }

    u8 Read8(u32 addr);
    u16 Read16(u32 addr);
    u32 Read32(u32 addr);
    void Write8(u32 addr, u8 val);
    void Write16(u32 addr, u16 val);
    void Write32(u32 addr, u32 val);

    void UpdateRegistersPreDraw(bool reset);
    void UpdateRegistersPostDraw(bool reset);
    void UpdateWindows(u32 line);

    u16* GetBGExtPal(u32 slot, u32 pal);
    u16* GetOBJExtPal();

    void GetBGVRAM(u8*& data, u32& mask) const;
    void GetOBJVRAM(u8*& data, u32& mask) const;

    void GetCaptureInfo_BG(int* info) const;
    void GetCaptureInfo_OBJ(int* info) const;

    void CalculateWindowMask(u8* windowMask, const u8* objWindow);

    u32 Num;
    bool Enabled;

    u32 DispCnt;
    u32 DispCntLatch[3];
    u8 LayerEnable;         // layer enable - enable delayed by 2 scanlines
    u8 OBJEnable;           // OBJ enable (for OBJ rendering) - enable delayed by 1 scanline
    u8 ForcedBlank;         // forced blank - disable delayed by 2 scanlines
    u16 BGCnt[4];

    u16 BGXPos[4];
    u16 BGYPos[4];

    s32 BGXRef[2];
    s32 BGYRef[2];
    s32 BGXRefInternal[2];
    s32 BGYRefInternal[2];
    s32 BGXRefReload[2];
    s32 BGYRefReload[2];
    s16 BGRotA[2];
    s16 BGRotB[2];
    s16 BGRotC[2];
    s16 BGRotD[2];

    u8 Win0Coords[4];
    u8 Win1Coords[4];
    u8 WinCnt[4];
    u8 Win0Active;
    u8 Win1Active;

    u8 BGMosaicSize[2];
    u8 OBJMosaicSize[2];
    u8 BGMosaicY, BGMosaicYMax;
    u8 OBJMosaicY;
    bool BGMosaicLatch;
    bool OBJMosaicLatch;
    u32 BGMosaicLine;
    u32 OBJMosaicLine;

    u16 BlendCnt;
    u16 BlendAlpha;
    u8 EVA, EVB;
    u8 EVY;

private:
    friend class Renderer2D;

    melonDS::GPU& GPU;
};

class Renderer2D
{
public:
    explicit Renderer2D(melonDS::GPU2D& gpu2D) : GPU(gpu2D.GPU), GPU2D(gpu2D) {}
    virtual ~Renderer2D() {}
    virtual bool Init() = 0;
    virtual void Reset() = 0;

    virtual void DrawScanline(u32 line) = 0;
    virtual void DrawSprites(u32 line) = 0;

    virtual void VBlank() = 0;
    virtual void VBlankEnd() = 0;

    virtual bool NeedsShaderCompile() { return false; }
    virtual void ShaderCompileStep(int& current, int& count) {}

protected:
    melonDS::GPU& GPU;
    melonDS::GPU2D& GPU2D;
};

}

#endif
