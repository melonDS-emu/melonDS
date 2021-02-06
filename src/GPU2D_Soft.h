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

#pragma once

#include "GPU2D.h"

class GPU2D_Soft : public GPU2D
{
public:
    GPU2D_Soft(u32 num);
    ~GPU2D_Soft() override {}

    void DrawScanline(u32 line) override;
    void DrawSprites(u32 line) override;
    void VBlankEnd() override;

protected:
    void MosaicXSizeChanged() override;

private:

    alignas(8) u32 BGOBJLine[256*3];
    u32* _3DLine;

    alignas(8) u32 OBJLine[256];
    alignas(8) u8 OBJIndex[256];

    u32 NumSprites;

    u8 MosaicTable[16][256];
    u8* CurBGXMosaicTable;
    u8* CurOBJXMosaicTable;

    u32 ColorBlend4(u32 val1, u32 val2, u32 eva, u32 evb);
    u32 ColorBlend5(u32 val1, u32 val2);
    u32 ColorBrightnessUp(u32 val, u32 factor);
    u32 ColorBrightnessDown(u32 val, u32 factor);
    u32 ColorComposite(int i, u32 val1, u32 val2);

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
};