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

#include "GPU2D_OpenGL.h"
#include "GPU.h"
#include "GPU3D.h"

namespace melonDS::GPU2D
{

GLRenderer::GLRenderer(melonDS::GPU& gpu)
    : SoftRenderer(gpu)
{
    // lala test
}


void GLRenderer::DrawScanline(u32 line, Unit* unit)
{
    // schm
    CurUnit = unit;

    printf("ass\n");
    SoftRenderer::DrawScanline(line, unit);
    //DrawBG_Text<true, DrawPixel_Test>(line, 0);
}

/*void GLRenderer::VBlankEnd(Unit* unitA, Unit* unitB)
{
#ifdef OGLRENDERER_ENABLED
    if (Renderer3D& renderer3d = GPU.GPU3D.GetCurrentRenderer(); renderer3d.Accelerated)
    {
        if ((unitA->CaptureCnt & (1<<31)) && (((unitA->CaptureCnt >> 29) & 0x3) != 1))
        {
            renderer3d.PrepareCaptureFrame();
        }
    }
#endif
}*/




void GLRenderer::DrawPixel_Test(u32* dst, u16 color, u32 flag)
{
    u8 r = (color & 0x001F) << 1;
    u8 g = (color & 0x03E0) >> 4;
    u8 b = (color & 0x7C00) >> 9;
    //g |= ((color & 0x8000) >> 15);

    *(dst+256) = *dst;
    *dst = r | (g << 8) | (b << 16) | flag;
}



}
