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

#ifndef GPU2D_H
#define GPU2D_H

class GPU2D
{
public:
    GPU2D(u32 num);
    ~GPU2D();

    void Reset();

    void DoSavestate(Savestate* file);

    void SetEnabled(bool enable) { Enabled = enable; }
    void SetFramebuffer(u32* buf);
    void SetRenderSettings(bool accel);

    u8 Read8(u32 addr);
    u16 Read16(u32 addr);
    u32 Read32(u32 addr);
    void Write8(u32 addr, u8 val);
    void Write16(u32 addr, u16 val);
    void Write32(u32 addr, u32 val);

    bool UsesFIFO()
    {
        if (((DispCnt >> 16) & 0x3) == 3)
            return true;
        if ((CaptureCnt & (1<<25)) && ((CaptureCnt >> 29) & 0x3) != 0)
            return true;

        return false;
    }

    void SampleFIFO(u32 offset, u32 num);

    void DrawScanline(u32 line);
    void DrawSprites(u32 line);
    void VBlank();
    void VBlankEnd();

    void CheckWindows(u32 line);

    u16* GetBGExtPal(u32 slot, u32 pal);
    u16* GetOBJExtPal();

private:
    u32 Num;
    bool Enabled;
    u32* Framebuffer;

    bool Accelerated;

    u32 BGOBJLine[256*3] __attribute__((aligned (8)));
    u32* _3DLine;

    u8 WindowMask[256] __attribute__((aligned (8)));
    u32 OBJLine[256] __attribute__((aligned (8)));
    u8 OBJWindow[256] __attribute__((aligned (8)));
    u8 OBJIndex[256] __attribute__((aligned (8)));

    u32 NumSprites;

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

    u8 MosaicTable[16][256];
    u8* CurBGXMosaicTable;
    u8* CurOBJXMosaicTable;

    u16 BlendCnt;
    u16 BlendAlpha;
    u8 EVA, EVB;
    u8 EVY;

    u32 CaptureCnt;

    u16 MasterBrightness;

    u16 BGExtPalCache[4][16*256];

    u32 ColorBlend4(u32 val1, u32 val2, u32 eva, u32 evb);
    u32 ColorBlend5(u32 val1, u32 val2);
    u32 ColorBrightnessUp(u32 val, u32 factor);
    u32 ColorBrightnessDown(u32 val, u32 factor);
    u32 ColorComposite(int i, u32 val1, u32 val2);

    void UpdateMosaicCounters(u32 line);

    template<u32 bgmode> void DrawScanlineBGMode(u32 line);
    void DrawScanlineBGMode6(u32 line);
    void DrawScanlineBGMode7(u32 line);
    void DrawScanline_BGOBJ(u32 line);

    static void DrawPixel_Normal(u32* dst, u16 color, u32 flag);
    static void DrawPixel_Accel(u32* dst, u16 color, u32 flag);
    
    typedef void (*DrawPixel)(u32* dst, u16 color, u32 flag);

    void DrawBG_3D();
    template<bool mosaic, DrawPixel drawPixel> void DrawBG_Text(u32 line, u32 bgnum);
    template<bool mosaic, DrawPixel drawPixel> void DrawBG_Affine(u32 line, u32 bgnum);
    template<bool mosaic, DrawPixel drawPixel> void DrawBG_Extended(u32 line, u32 bgnum);
    template<bool mosaic, DrawPixel drawPixel> void DrawBG_Large(u32 line);

    void ApplySpriteMosaicX();
    template<DrawPixel drawPixel>
    void InterleaveSprites(u32 prio);
    template<bool window> void DrawSprite_Rotscale(u32 num, u32 boundwidth, u32 boundheight, u32 width, u32 height, s32 xpos, s32 ypos);
    template<bool window> void DrawSprite_Normal(u32 num, u32 width, u32 height, s32 xpos, s32 ypos);

    void DoCapture(u32 line, u32 width);

    void CalculateWindowMask(u32 line);
};

#endif
