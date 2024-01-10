/*
    Copyright 2016-2024 melonDS team

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

#ifndef FRAMEDUMP_H
#define FRAMEDUMP_H

#include <vector>
#include "GPU.h"

namespace melonDS
{

class FrameDump
{
public:
    FrameDump(melonDS::GPU& gpu);
    
    void FDWrite(u16 cmd, u32* param); 
    void StartFrameDump();
    void FinFrameDump();

private:
    melonDS::GPU& GPU;

public:    
    // save before first command is sent
    bool ZDotDisp_Track = false;
    u16 ZDotDisp = 0;
    u32 PolyAttr = 0;
    u32 PolyAttrUnset = 0;
    u32 VtxColor = 0;
    u32 Viewport = 0;
    u32 ProjStack[16] {};
    u32 PosStack[32*16] {};
    u32 VecStack[32*16] {};
    u32 TexStack[16] {};
    u32 ProjMtx[16] {};
    u32 PosMtx[16] {};
    u32 VecMtx[16] {};
    u32 TexMtx[16] {};
    u32 MatrixMode = 0;
    u32 Polygon = 0;
    u32 VtxXY = 0;
    u16 VtxZ = 0;
    u32 TexCoord = 0;
    u32 TexParam = 0;
    u32 TexPalette = 0;
    u32 DiffAmbi = 0;
    u32 SpecEmis = 0;
    u32 Shininess[32] {};
    u32 LightVec[4] {};
    u32 LightColor[4] {};
    u32 SwapBuffer = 0;

    // store relevant gpu writes
    std::vector<u16> Cmds;
    std::vector<u32> Params;
};
}
#endif