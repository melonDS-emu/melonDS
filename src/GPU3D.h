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

#ifndef GPU3D_H
#define GPU3D_H

#include <array>

namespace GPU3D
{

typedef struct
{
    s32 Position[4];
    s32 Color[3];
    s16 TexCoords[2];

    bool Clipped;

    // final vertex attributes.
    // allows them to be reused in polygon strips.

    s32 FinalPosition[2];
    s32 FinalColor[3];

} Vertex;

typedef struct
{
    Vertex* Vertices[10];
    u32 NumVertices;

    s32 FinalZ[10];
    s32 FinalW[10];
    bool WBuffer;

    u32 Attr;
    u32 TexParam;
    u32 TexPalette;

    bool FacingView;
    bool Translucent;

    bool IsShadowMask;
    bool IsShadow;

    u32 VTop, VBottom; // vertex indices
    s32 YTop, YBottom; // Y coords
    s32 XTop, XBottom; // associated X coords

    u32 SortKey;

} Polygon;

extern u32 RenderDispCnt;
extern u8 RenderAlphaRef;

extern u16 RenderToonTable[32];
extern u16 RenderEdgeTable[8];

extern u32 RenderFogColor, RenderFogOffset;
extern u8 RenderFogDensityTable[34];

extern u32 RenderClearAttr1, RenderClearAttr2;

extern std::array<Polygon*,2048> RenderPolygonRAM;
extern u32 RenderNumPolygons;

bool Init();
void DeInit();
void Reset();

void ExecuteCommand();

void Run(s32 cycles);
void CheckFIFOIRQ();
void CheckFIFODMA();

void VCount144();
void VBlank();
void VCount215();
void RequestLine(int line);
u32* GetLine(int line);

void WriteToGXFIFO(u32 val);

u8 Read8(u32 addr);
u16 Read16(u32 addr);
u32 Read32(u32 addr);
void Write8(u32 addr, u8 val);
void Write16(u32 addr, u16 val);
void Write32(u32 addr, u32 val);

namespace SoftRenderer
{

bool Init();
void DeInit();
void Reset();

void SetupRenderThread();

void VCount144();
void RenderFrame();
void RequestLine(int line);
u32* GetLine(int line);

}

}

#endif
