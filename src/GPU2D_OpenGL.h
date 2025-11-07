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

#include "GPU2D_Soft.h"

namespace melonDS
{
class GPU;

namespace GPU2D
{

class GLRenderer : public SoftRenderer
{
public:
    GLRenderer(melonDS::GPU& gpu);
    ~GLRenderer() override {}

    void DrawScanline(u32 line, Unit* unit) override;
    /*void DrawSprites(u32 line, Unit* unit) override;
    void VBlankEnd(Unit* unitA, Unit* unitB) override;*/

private:
    static void DrawPixel_Test(u32* dst, u16 color, u32 flag);
};

}

}