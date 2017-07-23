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

#ifndef GPU2D_H
#define GPU2D_H

class GPU2D
{
public:
    GPU2D(u32 num);
    ~GPU2D();

    void Reset();

    void SetFramebuffer(u32* buf);

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
    void VBlank();
    void VBlankEnd();

    void CheckWindows(u32 line);

    void BGExtPalDirty(u32 base);
    void OBJExtPalDirty();

    u16* GetBGExtPal(u32 slot, u32 pal);
    u16* GetOBJExtPal(u32 pal);

private:
    u32 Num;
    u32* Framebuffer;

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
    bool Win0Active;
    bool Win1Active;

    u8 BGMosaicSize[2];
    u8 OBJMosaicSize[2];
    u8 BGMosaicY, BGMosaicYMax;
    u8 OBJMosaicY, OBJMosaicYMax;

    u16 BlendCnt;
    u16 BlendAlpha;
    u8 EVA, EVB;
    u8 EVY;

    u32 CaptureCnt;

    u16 MasterBrightness;

    u16 BGExtPalCache[4][16*256];
    u16 OBJExtPalCache[16*256];
    u32 BGExtPalStatus[4];
    u32 OBJExtPalStatus;

    template<u32 bgmode> void DrawScanlineBGMode(u32 line, u32* spritebuf, u32* dst);
    void DrawScanlineBGMode6(u32 line, u32* spritebuf, u32* dst);
    void DrawScanline_Mode1(u32 line, u32* dst);

    void DrawPixel(u32* dst, u16 color, u32 flag);

    void DrawBG_3D(u32 line, u32* dst);
    void DrawBG_Text(u32 line, u32* dst, u32 bgnum);
    void DrawBG_Affine(u32 line, u32* dst, u32 bgnum);
    void DrawBG_Extended(u32 line, u32* dst, u32 bgnum);
    void DrawBG_Large(u32 line, u32* dst);

    void InterleaveSprites(u32* buf, u32 prio, u32* dst);
    void DrawSprites(u32 line, u32* dst);
    void DrawSpritesWindow(u32 line, u8* dst);
    template<bool window> void DrawSprite_Rotscale(u16* attrib, u16* rotparams, u32 boundwidth, u32 boundheight, u32 width, u32 height, s32 xpos, s32 ypos, u32* dst);
    template<bool window> void DrawSprite_Normal(u16* attrib, u32 width, s32 xpos, s32 ypos, u32* dst);

    void DoCapture(u32 line, u32 width, u32* src);

    void CalculateWindowMask(u32 line, u8* mask);
};

#endif
