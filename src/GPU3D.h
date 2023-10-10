/*
    Copyright 2016-2022 melonDS team

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
#include <memory>

#include "GPU.h"
#include "Savestate.h"

namespace GPU3D
{

struct Vertex
{
    s32 Position[4];
    s32 Color[3];
    s16 TexCoords[2];

    bool Clipped;

    // final vertex attributes.
    // allows them to be reused in polygon strips.

    s32 FinalPosition[2];
    s32 FinalColor[3];

    // hi-res position (4-bit fractional part)
    // TODO maybe: hi-res color? (that survives clipping)
    s32 HiresPosition[2];

};

struct Polygon
{
    Vertex* Vertices[10];
    u32 NumVertices;

    s32 FinalZ[10];
    s32 FinalW[10];
    bool WBuffer;

    u32 Attr;
    u32 TexParam;
    u32 TexPalette;

    bool Degenerate;

    bool FacingView;
    bool Translucent;

    bool IsShadowMask;
    bool IsShadow;

    int Type; // 0=regular 1=line

    u32 VTop, VBottom; // vertex indices
    s32 YTop, YBottom; // Y coords
    s32 XTop, XBottom; // associated X coords

    u32 SortKey;

};

extern u32 RenderDispCnt;
extern u8 RenderAlphaRef;

extern u16 RenderToonTable[32];
extern u16 RenderEdgeTable[8];

extern u32 RenderFogColor, RenderFogOffset, RenderFogShift;
extern u8 RenderFogDensityTable[34];

extern u32 RenderClearAttr1, RenderClearAttr2;

extern bool RenderFrameIdentical;

extern u16 RenderXPos;

extern std::array<Polygon*,2048> RenderPolygonRAM;
extern u32 RenderNumPolygons;

extern bool AbortFrame;

extern u64 Timestamp;

bool Init();
void DeInit();
void Reset();

void DoSavestate(Savestate* file);

void SetEnabled(bool geometry, bool rendering);

void ExecuteCommand();

s32 CyclesToRunFor();
void Run();
void CheckFIFOIRQ();
void CheckFIFODMA();

void VCount144();
void VBlank();
void VCount215();

void RestartFrame();

void SetRenderXPos(u16 xpos);
u32* GetLine(int line);

void WriteToGXFIFO(u32 val);

u8 Read8(u32 addr);
u16 Read16(u32 addr);
u32 Read32(u32 addr);
void Write8(u32 addr, u8 val);
void Write16(u32 addr, u16 val);
void Write32(u32 addr, u32 val);

class Renderer3D
{
public:
    virtual ~Renderer3D() = default;

    Renderer3D(const Renderer3D&) = delete;
    Renderer3D& operator=(const Renderer3D&) = delete;

    virtual void Reset() = 0;

    // This "Accelerated" flag currently communicates if the framebuffer should
    // be allocated differently and other little misc handlers. Ideally there
    // are more detailed "traits" that we can ask of the Renderer3D type 
    const bool Accelerated;

    virtual void SetRenderSettings(GPU::RenderSettings& settings) = 0;

    virtual void VCount144() {};

    virtual void RenderFrame() = 0;
    virtual void RestartFrame() {};
    virtual u32* GetLine(int line) = 0;
protected:
    Renderer3D(bool Accelerated);
};

extern int Renderer;
extern std::unique_ptr<Renderer3D> CurrentRenderer;

}

#include "GPU3D_Soft.h"

#ifdef OGLRENDERER_ENABLED
#include "GPU3D_OpenGL.h"
#endif

#endif
