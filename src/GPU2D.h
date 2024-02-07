/*
    Copyright 2016-2023 melonDS team

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

namespace GPU2D
{

class Unit
{
public:
    // take a reference to the GPU so we can access its state
    // and ensure that it's not null
    Unit(u32 num, melonDS::GPU& gpu);
    virtual ~Unit() = default;
    Unit(const Unit&) = delete;
    Unit& operator=(const Unit&) = delete;

    void Reset();

    void DoSavestate(Savestate* file);

    void SetEnabled(bool enable) { Enabled = enable; }

    u8 Read8(u32 addr);
    u16 Read16(u32 addr);
    u32 Read32(u32 addr);
    void Write8(u32 addr, u8 val);
    void Write16(u32 addr, u16 val);
    void Write32(u32 addr, u32 val);

    bool UsesFIFO() const
    {
        if (((DispCnt >> 16) & 0x3) == 3)
            return true;
        if ((CaptureCnt & (1<<25)) && ((CaptureCnt >> 29) & 0x3) != 0)
            return true;

        return false;
    }

    void SampleFIFO(u32 offset, u32 num);

    void VBlank();
    virtual void VBlankEnd();

    void CheckWindows(u32 line);

    u16* GetBGExtPal(u32 slot, u32 pal);
    u16* GetOBJExtPal();

    void GetBGVRAM(u8*& data, u32& mask) const;
    void GetOBJVRAM(u8*& data, u32& mask) const;

    void UpdateMosaicCounters(u32 line);
    void CalculateWindowMask(u32 line, u8* windowMask, const u8* objWindow);

    u32 Num;
    bool Enabled;

    u16 DispFIFO[16];
    u32 DispFIFOReadPtr;
    u32 DispFIFOWritePtr;

    u16 DispFIFOBuffer[256];

    u32 DispCnt;
    u16 BGCnt[4];

    u16 BGXPos[4];
    u16 BGYPos[4];

    s32 BGXRef[2];
    s32 BGYRef[2];
    s32 BGXRefInternal[2];
    s32 BGYRefInternal[2];
    s16 BGRotA[2];
    s16 BGRotB[2];
    s16 BGRotC[2];
    s16 BGRotD[2];

    u8 Win0Coords[4];
    u8 Win1Coords[4];
    u8 WinCnt[4];
    u32 Win0Active;
    u32 Win1Active;

    u8 BGMosaicSize[2];
    u8 OBJMosaicSize[2];
    u8 BGMosaicY, BGMosaicYMax;
    u8 OBJMosaicYCount, OBJMosaicY, OBJMosaicYMax;

    u16 BlendCnt;
    u16 BlendAlpha;
    u8 EVA, EVB;
    u8 EVY;

    bool CaptureLatch;
    u32 CaptureCnt;

    u16 MasterBrightness;
private:
    melonDS::GPU& GPU;
};

class Renderer2D
{
public:
    virtual ~Renderer2D() {}

    virtual void DrawScanline(u32 line, Unit* unit) = 0;
    virtual void DrawSprites(u32 line, Unit* unit) = 0;

    virtual void VBlankEnd(Unit* unitA, Unit* unitB) = 0;

    void SetFramebuffer(u32* unitA, u32* unitB)
    {
        Framebuffer[0] = unitA;
        Framebuffer[1] = unitB;
    }
protected:
    u32* Framebuffer[2];

    Unit* CurUnit;
};

}

}
#endif
