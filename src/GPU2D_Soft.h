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

#pragma once

#include "GPU2D.h"

namespace melonDS
{
class SoftRenderer;

class SoftRenderer2D : public Renderer2D
{
public:
    SoftRenderer2D(melonDS::GPU2D& gpu2D, SoftRenderer& parent);
    ~SoftRenderer2D() override;
    bool Init() override { return true; }
    void Reset() override;

    void DrawScanline(u32 line) override;
    void DrawSprites(u32 line) override;
    void VBlank() override {}
    void VBlankEnd() override {};

private:
    SoftRenderer& Parent;

    enum
    {
        OBJ_StandardPal = (1<<12),
        OBJ_DirectColor = (1<<15),
        OBJ_BGPrioMask = (0x3<<16),
        OBJ_IsOpaque = (1<<18),
        OBJ_OpaPrioMask = (OBJ_BGPrioMask | OBJ_IsOpaque),
        OBJ_IsSprite = (1<<19),
        OBJ_Mosaic = (1<<20),
    };

    alignas(8) u32 BGOBJLine[256*2];

    alignas(8) u8 WindowMask[256];

    alignas(8) u32 OBJLine[256];
    alignas(8) u8 OBJWindow[256];

    u32 NumSprites;

    u8* CurBGXMosaicTable;
    array2d<u8, 16, 256> MosaicTable = []() constexpr
    {
        array2d<u8, 16, 256> table {};
        // initialize mosaic table
        for (int m = 0; m < 16; m++)
        {
            for (int x = 0; x < 256; x++)
            {
                int offset = x % (m+1);
                table[m][x] = offset;
            }
        }

        return table;
    }();

    u32 ColorComposite(int i, u32 val1, u32 val2) const;

    template<u32 bgmode> void DrawScanlineBGMode(u32 line);
    void DrawScanlineBGMode6(u32 line);
    void DrawScanlineBGMode7(u32 line);
    void DrawScanline_BGOBJ(u32 line, u32* dst);

    static void DrawPixel(u32* dst, u16 color, u32 flag);

    void DrawBG_3D();
    template<bool mosaic> void DrawBG_Text(u32 line, u32 bgnum);
    template<bool mosaic> void DrawBG_Affine(u32 line, u32 bgnum);
    template<bool mosaic> void DrawBG_Extended(u32 line, u32 bgnum);
    template<bool mosaic> void DrawBG_Large(u32 line);

    void ApplySpriteMosaicX();
    void InterleaveSprites(u32 prio);
    template<bool window> void DrawSpritePixel(int color, u32 pixelattr, s32 xpos);
    template<bool window> void DrawSprite_Rotscale(u32 num, u32 boundwidth, u32 boundheight, u32 width, u32 height, s32 xpos, s32 ypos);
    template<bool window> void DrawSprite_Normal(u32 num, u32 width, u32 height, s32 xpos, s32 ypos);
};

}
