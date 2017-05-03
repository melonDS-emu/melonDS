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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "GPU.h"
#include "FIFO.h"


// 3D engine notes
//
// vertex/polygon RAM is filled when a complete polygon is defined, after it's been culled and clipped
// 04000604 reads from bank used by renderer
// bank used by renderer is emptied at scanline ~192
// banks are swapped at scanline ~194
// TODO: needs more investigation. it's weird.
//
// clipping rules:
// * if a shared vertex in a strip is clipped, affected polygons are converted into single polygons
//   strip is resumed at the first eligible polygon
//
// clipping exhibits oddities on the real thing. bad precision? fancy algorithm? TODO: investigate.
//
// vertex color precision:
// * vertex colors are kept at 5-bit during clipping. makes for shitty results.
// * vertex colors are converted to 9-bit before drawing, as such:
//   if (x > 0) x = (x << 4) + 0xF
//   the added bias affects interpolation.
//
// depth buffer:
// Z-buffering mode: val = ((Z * 0x800 * 0x1000) / W) + 0x7FFEFF
// W-buffering mode: val = W
//
// formula for clear depth: (GBAtek is wrong there)
// clearZ = (val * 0x200) + 0x1FF;
//
// alpha is 5-bit
//
// matrix push/pop on the position matrix are always applied to the vector matrix too, even in position-only mode
// store/restore too, probably (TODO: confirm)
// (the idea is that each position matrix has an associated vector matrix)
//
// TODO: check if translate works on the vector matrix? seems pointless
//
// viewport Y coordinates are upside-down
//
// clear color/depth/bitmap/etc registers (04000350/04000354) are double-buffered


namespace GPU3D
{

const u32 CmdNumParams[256] =
{
    // 0x00
    0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x10
    1, 0, 1, 1, 1, 0, 16, 12, 16, 12, 9, 3, 3,
    0, 0, 0,
    // 0x20
    1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0,
    // 0x30
    1, 1, 1, 1, 32,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x40
    1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x50
    1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x60
    1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x70
    3, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x80+
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const s32 CmdNumCycles[256] =
{
    // 0x00
    0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x10
    1, 17, 36, 17, 36, 19, 34, 30, 35, 31, 28, 22, 22,
    0, 0, 0,
    // 0x20
    1, 9, 1, 9, 8, 8, 8, 8, 8, 1, 1, 1,
    0, 0, 0, 0,
    // 0x30
    4, 4, 6, 1, 32,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x40
    1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x50
    392,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x60
    1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x70
    103, 9, 5,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x80+
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

typedef struct
{
    u8 Command;
    u32 Param;

} CmdFIFOEntry;

FIFO<CmdFIFOEntry>* CmdFIFO;
FIFO<CmdFIFOEntry>* CmdPIPE;

u32 NumCommands, CurCommand, ParamCount, TotalParams;

u32 DispCnt;
u8 AlphaRefVal;
u8 AlphaRef;

u16 ToonTable[32];
u16 EdgeTable[8];

u32 FogColor;
u32 FogOffset;
u8 FogDensityTable[32];

u32 GXStat;

u32 ExecParams[32];
u32 ExecParamCount;
s32 CycleCount;

u32 NumPushPopCommands;
u32 NumTestCommands;


u32 MatrixMode;

s32 ProjMatrix[16];
s32 PosMatrix[16];
s32 VecMatrix[16];
s32 TexMatrix[16];

s32 ClipMatrix[16];
bool ClipMatrixDirty;

s32 Viewport[4];

s32 ProjMatrixStack[16];
s32 PosMatrixStack[31][16];
s32 VecMatrixStack[31][16];
s32 TexMatrixStack[16];
s32 ProjMatrixStackPointer;
s32 PosMatrixStackPointer;
s32 TexMatrixStackPointer;

void MatrixLoadIdentity(s32* m);
void UpdateClipMatrix();


u32 PolygonMode;
s16 CurVertex[3];
u8 VertexColor[3];
s16 TexCoords[2];
s16 RawTexCoords[2];
s16 Normal[3];

s16 LightDirection[4][3];
u8 LightColor[4][3];
u8 MatDiffuse[3];
u8 MatAmbient[3];
u8 MatSpecular[3];
u8 MatEmission[3];

bool UseShininessTable;
u8 ShininessTable[128];

u32 PolygonAttr;
u32 CurPolygonAttr;

u32 TexParam;
u32 TexPalette;

s32 PosTestResult[4];
s16 VecTestResult[3];

Vertex TempVertexBuffer[4];
u32 VertexNum;
u32 VertexNumInPoly;
u32 NumConsecutivePolygons;
Polygon* LastStripPolygon;

Vertex VertexRAM[6144 * 2];
Polygon PolygonRAM[2048 * 2];

Vertex* CurVertexRAM;
Polygon* CurPolygonRAM;
u32 NumVertices, NumPolygons;
u32 CurRAMBank;

Vertex* RenderVertexRAM;
Polygon* RenderPolygonRAM;
u32 RenderNumPolygons;

u32 ClearAttr1, ClearAttr2;
u32 RenderClearAttr1, RenderClearAttr2;

u32 FlushRequest;
u32 FlushAttributes;



bool Init()
{
    CmdFIFO = new FIFO<CmdFIFOEntry>(256);
    CmdPIPE = new FIFO<CmdFIFOEntry>(4);

    if (!SoftRenderer::Init()) return false;

    return true;
}

void DeInit()
{
    SoftRenderer::DeInit();

    delete CmdFIFO;
    delete CmdPIPE;
}

void Reset()
{
    CmdFIFO->Clear();
    CmdPIPE->Clear();

    NumCommands = 0;
    CurCommand = 0;
    ParamCount = 0;
    TotalParams = 0;

    NumPushPopCommands = 0;
    NumTestCommands = 0;

    DispCnt = 0;
    AlphaRef = 0;

    GXStat = 0;

    memset(ExecParams, 0, 32*4);
    ExecParamCount = 0;
    CycleCount = 0;


    MatrixMode = 0;

    MatrixLoadIdentity(ProjMatrix);
    MatrixLoadIdentity(PosMatrix);
    MatrixLoadIdentity(VecMatrix);
    MatrixLoadIdentity(TexMatrix);

    ClipMatrixDirty = true;
    UpdateClipMatrix();

    memset(Viewport, 0, sizeof(Viewport));

    memset(ProjMatrixStack, 0, 16*4);
    memset(PosMatrixStack, 0, 31 * 16*4);
    memset(VecMatrixStack, 0, 31 * 16*4);
    memset(TexMatrixStack, 0, 16*4);
    ProjMatrixStackPointer = 0;
    PosMatrixStackPointer = 0;
    TexMatrixStackPointer = 0;

    memset(PosTestResult, 0, 4*4);
    memset(VecTestResult, 0, 2*3);

    VertexNum = 0;
    VertexNumInPoly = 0;

    CurRAMBank = 0;
    CurVertexRAM = &VertexRAM[0];
    CurPolygonRAM = &PolygonRAM[0];
    NumVertices = 0;
    NumPolygons = 0;

    ClearAttr1 = 0;
    ClearAttr2 = 0;

    FlushRequest = 0;
    FlushAttributes = 0;

    SoftRenderer::Reset();
}



void MatrixLoadIdentity(s32* m)
{
    m[0] = 0x1000; m[1] = 0;      m[2] = 0;       m[3] = 0;
    m[4] = 0;      m[5] = 0x1000; m[6] = 0;       m[7] = 0;
    m[8] = 0;      m[9] = 0;      m[10] = 0x1000; m[11] = 0;
    m[12] = 0;     m[13] = 0;     m[14] = 0;      m[15] = 0x1000;
}

void MatrixLoad4x4(s32* m, s32* s)
{
    memcpy(m, s, 16*4);
}

void MatrixLoad4x3(s32* m, s32* s)
{
    m[0] = s[0];  m[1] = s[1];  m[2] = s[2];    m[3] = 0;
    m[4] = s[3];  m[5] = s[4];  m[6] = s[5];    m[7] = 0;
    m[8] = s[6];  m[9] = s[7];  m[10] = s[8];   m[11] = 0;
    m[12] = s[9]; m[13] = s[10]; m[14] = s[11]; m[15] = 0x1000;
}

void MatrixMult4x4(s32* m, s32* s)
{
    s32 tmp[16];
    memcpy(tmp, m, 16*4);

    // m = s*m
    m[0] = ((s64)s[0]*tmp[0] + (s64)s[1]*tmp[4] + (s64)s[2]*tmp[8] + (s64)s[3]*tmp[12]) >> 12;
    m[1] = ((s64)s[0]*tmp[1] + (s64)s[1]*tmp[5] + (s64)s[2]*tmp[9] + (s64)s[3]*tmp[13]) >> 12;
    m[2] = ((s64)s[0]*tmp[2] + (s64)s[1]*tmp[6] + (s64)s[2]*tmp[10] + (s64)s[3]*tmp[14]) >> 12;
    m[3] = ((s64)s[0]*tmp[3] + (s64)s[1]*tmp[7] + (s64)s[2]*tmp[11] + (s64)s[3]*tmp[15]) >> 12;

    m[4] = ((s64)s[4]*tmp[0] + (s64)s[5]*tmp[4] + (s64)s[6]*tmp[8] + (s64)s[7]*tmp[12]) >> 12;
    m[5] = ((s64)s[4]*tmp[1] + (s64)s[5]*tmp[5] + (s64)s[6]*tmp[9] + (s64)s[7]*tmp[13]) >> 12;
    m[6] = ((s64)s[4]*tmp[2] + (s64)s[5]*tmp[6] + (s64)s[6]*tmp[10] + (s64)s[7]*tmp[14]) >> 12;
    m[7] = ((s64)s[4]*tmp[3] + (s64)s[5]*tmp[7] + (s64)s[6]*tmp[11] + (s64)s[7]*tmp[15]) >> 12;

    m[8] = ((s64)s[8]*tmp[0] + (s64)s[9]*tmp[4] + (s64)s[10]*tmp[8] + (s64)s[11]*tmp[12]) >> 12;
    m[9] = ((s64)s[8]*tmp[1] + (s64)s[9]*tmp[5] + (s64)s[10]*tmp[9] + (s64)s[11]*tmp[13]) >> 12;
    m[10] = ((s64)s[8]*tmp[2] + (s64)s[9]*tmp[6] + (s64)s[10]*tmp[10] + (s64)s[11]*tmp[14]) >> 12;
    m[11] = ((s64)s[8]*tmp[3] + (s64)s[9]*tmp[7] + (s64)s[10]*tmp[11] + (s64)s[11]*tmp[15]) >> 12;

    m[12] = ((s64)s[12]*tmp[0] + (s64)s[13]*tmp[4] + (s64)s[14]*tmp[8] + (s64)s[15]*tmp[12]) >> 12;
    m[13] = ((s64)s[12]*tmp[1] + (s64)s[13]*tmp[5] + (s64)s[14]*tmp[9] + (s64)s[15]*tmp[13]) >> 12;
    m[14] = ((s64)s[12]*tmp[2] + (s64)s[13]*tmp[6] + (s64)s[14]*tmp[10] + (s64)s[15]*tmp[14]) >> 12;
    m[15] = ((s64)s[12]*tmp[3] + (s64)s[13]*tmp[7] + (s64)s[14]*tmp[11] + (s64)s[15]*tmp[15]) >> 12;
}

void MatrixMult4x3(s32* m, s32* s)
{
    s32 tmp[16];
    memcpy(tmp, m, 16*4);

    // m = s*m
    m[0] = ((s64)s[0]*tmp[0] + (s64)s[1]*tmp[4] + (s64)s[2]*tmp[8]) >> 12;
    m[1] = ((s64)s[0]*tmp[1] + (s64)s[1]*tmp[5] + (s64)s[2]*tmp[9]) >> 12;
    m[2] = ((s64)s[0]*tmp[2] + (s64)s[1]*tmp[6] + (s64)s[2]*tmp[10]) >> 12;
    m[3] = ((s64)s[0]*tmp[3] + (s64)s[1]*tmp[7] + (s64)s[2]*tmp[11]) >> 12;

    m[4] = ((s64)s[3]*tmp[0] + (s64)s[4]*tmp[4] + (s64)s[5]*tmp[8]) >> 12;
    m[5] = ((s64)s[3]*tmp[1] + (s64)s[4]*tmp[5] + (s64)s[5]*tmp[9]) >> 12;
    m[6] = ((s64)s[3]*tmp[2] + (s64)s[4]*tmp[6] + (s64)s[5]*tmp[10]) >> 12;
    m[7] = ((s64)s[3]*tmp[3] + (s64)s[4]*tmp[7] + (s64)s[5]*tmp[11]) >> 12;

    m[8] = ((s64)s[6]*tmp[0] + (s64)s[7]*tmp[4] + (s64)s[8]*tmp[8]) >> 12;
    m[9] = ((s64)s[6]*tmp[1] + (s64)s[7]*tmp[5] + (s64)s[8]*tmp[9]) >> 12;
    m[10] = ((s64)s[6]*tmp[2] + (s64)s[7]*tmp[6] + (s64)s[8]*tmp[10]) >> 12;
    m[11] = ((s64)s[6]*tmp[3] + (s64)s[7]*tmp[7] + (s64)s[8]*tmp[11]) >> 12;

    m[12] = ((s64)s[9]*tmp[0] + (s64)s[10]*tmp[4] + (s64)s[11]*tmp[8] + (s64)0x1000*tmp[12]) >> 12;
    m[13] = ((s64)s[9]*tmp[1] + (s64)s[10]*tmp[5] + (s64)s[11]*tmp[9] + (s64)0x1000*tmp[13]) >> 12;
    m[14] = ((s64)s[9]*tmp[2] + (s64)s[10]*tmp[6] + (s64)s[11]*tmp[10] + (s64)0x1000*tmp[14]) >> 12;
    m[15] = ((s64)s[9]*tmp[3] + (s64)s[10]*tmp[7] + (s64)s[11]*tmp[11] + (s64)0x1000*tmp[15]) >> 12;
}

void MatrixMult3x3(s32* m, s32* s)
{
    s32 tmp[12];
    memcpy(tmp, m, 12*4);

    // m = s*m
    m[0] = ((s64)s[0]*tmp[0] + (s64)s[1]*tmp[4] + (s64)s[2]*tmp[8]) >> 12;
    m[1] = ((s64)s[0]*tmp[1] + (s64)s[1]*tmp[5] + (s64)s[2]*tmp[9]) >> 12;
    m[2] = ((s64)s[0]*tmp[2] + (s64)s[1]*tmp[6] + (s64)s[2]*tmp[10]) >> 12;
    m[3] = ((s64)s[0]*tmp[3] + (s64)s[1]*tmp[7] + (s64)s[2]*tmp[11]) >> 12;

    m[4] = ((s64)s[3]*tmp[0] + (s64)s[4]*tmp[4] + (s64)s[5]*tmp[8]) >> 12;
    m[5] = ((s64)s[3]*tmp[1] + (s64)s[4]*tmp[5] + (s64)s[5]*tmp[9]) >> 12;
    m[6] = ((s64)s[3]*tmp[2] + (s64)s[4]*tmp[6] + (s64)s[5]*tmp[10]) >> 12;
    m[7] = ((s64)s[3]*tmp[3] + (s64)s[4]*tmp[7] + (s64)s[5]*tmp[11]) >> 12;

    m[8] = ((s64)s[6]*tmp[0] + (s64)s[7]*tmp[4] + (s64)s[8]*tmp[8]) >> 12;
    m[9] = ((s64)s[6]*tmp[1] + (s64)s[7]*tmp[5] + (s64)s[8]*tmp[9]) >> 12;
    m[10] = ((s64)s[6]*tmp[2] + (s64)s[7]*tmp[6] + (s64)s[8]*tmp[10]) >> 12;
    m[11] = ((s64)s[6]*tmp[3] + (s64)s[7]*tmp[7] + (s64)s[8]*tmp[11]) >> 12;
}

void MatrixScale(s32* m, s32* s)
{
    m[0] = ((s64)s[0]*m[0]) >> 12;
    m[1] = ((s64)s[0]*m[1]) >> 12;
    m[2] = ((s64)s[0]*m[2]) >> 12;
    m[3] = ((s64)s[0]*m[3]) >> 12;

    m[4] = ((s64)s[1]*m[4]) >> 12;
    m[5] = ((s64)s[1]*m[5]) >> 12;
    m[6] = ((s64)s[1]*m[6]) >> 12;
    m[7] = ((s64)s[1]*m[7]) >> 12;

    m[8] = ((s64)s[2]*m[8]) >> 12;
    m[9] = ((s64)s[2]*m[9]) >> 12;
    m[10] = ((s64)s[2]*m[10]) >> 12;
    m[11] = ((s64)s[2]*m[11]) >> 12;
}

void MatrixTranslate(s32* m, s32* s)
{
    m[12] += ((s64)s[0]*m[0] + (s64)s[1]*m[4] + (s64)s[2]*m[8]) >> 12;
    m[13] += ((s64)s[0]*m[1] + (s64)s[1]*m[5] + (s64)s[2]*m[9]) >> 12;
    m[14] += ((s64)s[0]*m[2] + (s64)s[1]*m[6] + (s64)s[2]*m[10]) >> 12;
}

void UpdateClipMatrix()
{
    if (!ClipMatrixDirty) return;
    ClipMatrixDirty = false;

    memcpy(ClipMatrix, ProjMatrix, 16*4);
    MatrixMult4x4(ClipMatrix, PosMatrix);
}



template<int comp, s32 plane, bool attribs>
void ClipSegment(Vertex* outbuf, Vertex* vout, Vertex* vin)
{
    s64 factor_num = vin->Position[3] - (plane*vin->Position[comp]);
    s32 factor_den = factor_num - (vout->Position[3] - (plane*vout->Position[comp]));

#define INTERPOLATE(var)  { outbuf->var = (vin->var + ((vout->var - vin->var) * factor_num) / factor_den); }

    if (comp != 0) INTERPOLATE(Position[0]);
    if (comp != 1) INTERPOLATE(Position[1]);
    if (comp != 2) INTERPOLATE(Position[2]);
    INTERPOLATE(Position[3]);
    outbuf->Position[comp] = plane*outbuf->Position[3];

    if (attribs)
    {
        INTERPOLATE(Color[0]);
        INTERPOLATE(Color[1]);
        INTERPOLATE(Color[2]);

        INTERPOLATE(TexCoords[0]);
        INTERPOLATE(TexCoords[1]);
    }

    outbuf->Clipped = true;

#undef INTERPOLATE
}

template<int comp, bool attribs>
int ClipAgainstPlane(Vertex* vertices, int nverts, int clipstart)
{
    Vertex temp[10];
    int prev, next;
    int c = clipstart;

    if (clipstart == 2)
    {
        temp[0] = vertices[0];
        temp[1] = vertices[1];
    }

    for (int i = clipstart; i < nverts; i++)
    {
        prev = i-1; if (prev < 0) prev = nverts-1;
        next = i+1; if (next >= nverts) next = 0;

        Vertex vtx = vertices[i];
        if (vtx.Position[comp] > vtx.Position[3])
        {
            if ((comp == 2) && (!(CurPolygonAttr & (1<<12)))) return 0;

            Vertex* vprev = &vertices[prev];
            if (vprev->Position[comp] <= vprev->Position[3])
            {
                ClipSegment<comp, 1, attribs>(&temp[c], &vtx, vprev);
                c++;
            }

            Vertex* vnext = &vertices[next];
            if (vnext->Position[comp] <= vnext->Position[3])
            {
                ClipSegment<comp, 1, attribs>(&temp[c], &vtx, vnext);
                c++;
            }
        }
        else
            temp[c++] = vtx;
    }

    nverts = c; c = clipstart;
    for (int i = clipstart; i < nverts; i++)
    {
        prev = i-1; if (prev < 0) prev = nverts-1;
        next = i+1; if (next >= nverts) next = 0;

        Vertex vtx = temp[i];
        if (vtx.Position[comp] < -vtx.Position[3])
        {
            Vertex* vprev = &temp[prev];
            if (vprev->Position[comp] >= -vprev->Position[3])
            {
                ClipSegment<comp, -1, attribs>(&vertices[c], &vtx, vprev);
                c++;
            }

            Vertex* vnext = &temp[next];
            if (vnext->Position[comp] >= -vnext->Position[3])
            {
                ClipSegment<comp, -1, attribs>(&vertices[c], &vtx, vnext);
                c++;
            }
        }
        else
            vertices[c++] = vtx;
    }

    // checkme
    for (int i = 0; i < c; i++)
    {
        Vertex* vtx = &vertices[i];

        vtx->Color[0] &= ~0xFFF; vtx->Color[0] += 0xFFF;
        vtx->Color[1] &= ~0xFFF; vtx->Color[1] += 0xFFF;
        vtx->Color[2] &= ~0xFFF; vtx->Color[2] += 0xFFF;
    }

    return c;
}

template<bool attribs>
int ClipPolygon(Vertex* vertices, int nverts, int clipstart)
{
    // clip.
    // for each vertex:
    // if it's outside, check if the previous and next vertices are inside
    // if so, place a new vertex at the edge of the view volume

    // TODO: check for 1-dot polygons
    // TODO: the hardware seems to use a different algorithm. it reacts differently to vertices with W=0

    // X clipping
    nverts = ClipAgainstPlane<0, attribs>(vertices, nverts, clipstart);

    // Y clipping
    nverts = ClipAgainstPlane<1, attribs>(vertices, nverts, clipstart);

    // Z clipping
    nverts = ClipAgainstPlane<2, attribs>(vertices, nverts, clipstart);

    return nverts;
}

void SubmitPolygon()
{
    Vertex clippedvertices[10];
    Vertex* reusedvertices[2];
    int clipstart = 0;
    int lastpolyverts = 0;

    int nverts = PolygonMode & 0x1 ? 4:3;
    int prev, next;

    // culling
    // TODO: work out how it works on the real thing
    // the normalization part is a wild guess

    Vertex *v0, *v1, *v2;
    s64 normalX, normalY, normalZ;
    s64 dot;

    v0 = &TempVertexBuffer[0];
    v1 = &TempVertexBuffer[1];
    v2 = &TempVertexBuffer[2];
    normalX = ((s64)v0->Position[1] * v2->Position[3]) - ((s64)v0->Position[3] * v2->Position[1]);
    normalY = ((s64)v0->Position[3] * v2->Position[0]) - ((s64)v0->Position[0] * v2->Position[3]);
    normalZ = ((s64)v0->Position[0] * v2->Position[1]) - ((s64)v0->Position[1] * v2->Position[0]);

    while ((((normalX>>31) ^ (normalX>>63)) != 0) ||
           (((normalY>>31) ^ (normalY>>63)) != 0) ||
           (((normalZ>>31) ^ (normalZ>>63)) != 0))
    {
        normalX >>= 4;
        normalY >>= 4;
        normalZ >>= 4;
    }

    dot = ((s64)v1->Position[0] * normalX) + ((s64)v1->Position[1] * normalY) + ((s64)v1->Position[3] * normalZ);

    bool facingview = (dot < 0);

    if (facingview)
    {
        if (!(CurPolygonAttr & (1<<7)))
        {
            LastStripPolygon = NULL;
            return;
        }
    }
    else if (dot > 0)
    {
        if (!(CurPolygonAttr & (1<<6)))
        {
            LastStripPolygon = NULL;
            return;
        }
    }

    // for strips, check whether we can attach to the previous polygon
    // this requires two vertices shared with the previous polygon, and that
    // the two polygons be of the same type

    if (PolygonMode >= 2 && LastStripPolygon)
    {
        int id0, id1;
        if (PolygonMode == 2)
        {
            if (NumConsecutivePolygons & 1)
            {
                id0 = 2;
                id1 = 1;
            }
            else
            {
                id0 = 0;
                id1 = 2;
            }

            lastpolyverts = 3;
        }
        else
        {
            id0 = 3;
            id1 = 2;

            lastpolyverts = 4;
        }

        if (LastStripPolygon->NumVertices == lastpolyverts &&
            !LastStripPolygon->Vertices[id0]->Clipped &&
            !LastStripPolygon->Vertices[id1]->Clipped)
        {
            reusedvertices[0] = LastStripPolygon->Vertices[id0];
            reusedvertices[1] = LastStripPolygon->Vertices[id1];

            clippedvertices[0] = *reusedvertices[0];
            clippedvertices[1] = *reusedvertices[1];

            clipstart = 2;
        }
    }

    for (int i = clipstart; i < nverts; i++)
        clippedvertices[i] = TempVertexBuffer[i];

    // clipping

    nverts = ClipPolygon<true>(clippedvertices, nverts, clipstart);

    if (nverts == 0)
    {
        LastStripPolygon = NULL;
        return;
    }

    // build the actual polygon

    if (NumPolygons >= 2048 || NumVertices+nverts > 6144)
    {
        LastStripPolygon = NULL;
        DispCnt |= (1<<13);
        return;
    }

    Polygon* poly = &CurPolygonRAM[NumPolygons++];
    poly->NumVertices = 0;

    poly->Attr = CurPolygonAttr;
    poly->TexParam = TexParam;
    poly->TexPalette = TexPalette;

    poly->FacingView = facingview;

    u32 texfmt = (TexParam >> 26) & 0x7;
    u32 polyalpha = (CurPolygonAttr >> 16) & 0x1F;
    poly->Translucent = ((texfmt == 1 || texfmt == 6) && !(CurPolygonAttr & 0x10)) || (polyalpha > 0 && polyalpha < 31);

    poly->IsShadowMask = ((CurPolygonAttr & 0x3F000030) == 0x00000030);
    if ((NumPolygons == 1) || (!CurPolygonRAM[NumPolygons-2].IsShadowMask))
        poly->ClearStencil = poly->IsShadowMask;
    else
        poly->ClearStencil = false;

    poly->IsShadow = ((CurPolygonAttr & 0x30) == 0x30) && !poly->IsShadowMask;

    if (LastStripPolygon && clipstart > 0)
    {
        if (nverts == lastpolyverts)
        {
            poly->Vertices[0] = reusedvertices[0];
            poly->Vertices[1] = reusedvertices[1];
        }
        else
        {
            Vertex v0 = *reusedvertices[0];
            Vertex v1 = *reusedvertices[1];

            CurVertexRAM[NumVertices] = v0;
            poly->Vertices[0] = &CurVertexRAM[NumVertices];
            CurVertexRAM[NumVertices+1] = v1;
            poly->Vertices[1] = &CurVertexRAM[NumVertices+1];
            NumVertices += 2;
        }

        poly->NumVertices += 2;
    }

    for (int i = clipstart; i < nverts; i++)
    {
        Vertex* vtx = &CurVertexRAM[NumVertices];
        *vtx = clippedvertices[i];
        poly->Vertices[i] = vtx;

        NumVertices++;
        poly->NumVertices++;

        // viewport transform
        s32 posX, posY, posZ;
        s32 w = vtx->Position[3];
        if (w == 0)
        {
            posX = 0;
            posY = 0;
            posZ = 0;
            w = 0x1000;
        }
        else
        {
            // W is normalized, such that all the polygon's W values fit within 16 bits
            // the viewport transform for X and Y uses the original W values, but
            // the transform for Z uses the normalized W values
            // W normalization is applied to separate polygons, even within strips

            posX = (((s64)(vtx->Position[0] + w) * Viewport[2]) / (((s64)w) << 1)) + Viewport[0];
            posY = (((s64)(-vtx->Position[1] + w) * Viewport[3]) / (((s64)w) << 1)) + Viewport[1];

            //if (FlushAttributes & 0x2) posZ = w;
            //else                       posZ = (((s64)vtx->Position[2] * 0x800000) / w) + 0x7FFEFF;
        }

        if      (posX < 0)        posX = 0;
        else if (posX > 256)      posX = 256;
        if      (posY < 0)        posY = 0;
        else if (posY > 192)      posY = 192;
        //if      (posZ < 0)        posZ = 0;
        //else if (posZ > 0xFFFFFF) posZ = 0xFFFFFF;

        vtx->FinalPosition[0] = posX;
        vtx->FinalPosition[1] = posY;
        //vtx->FinalPosition[2] = posZ;
        //vtx->FinalPosition[3] = w;

        vtx->FinalColor[0] = vtx->Color[0] >> 12;
        if (vtx->FinalColor[0]) vtx->FinalColor[0] = ((vtx->FinalColor[0] << 4) + 0xF);
        vtx->FinalColor[1] = vtx->Color[1] >> 12;
        if (vtx->FinalColor[1]) vtx->FinalColor[1] = ((vtx->FinalColor[1] << 4) + 0xF);
        vtx->FinalColor[2] = vtx->Color[2] >> 12;
        if (vtx->FinalColor[2]) vtx->FinalColor[2] = ((vtx->FinalColor[2] << 4) + 0xF);
    }

    // determine bounds of the polygon
    // also determine the W shift and normalize W
    // TODO: normalization works both ways
    // (ie two W's that span 12 bits or less will be brought to 16 bits)

    u32 vtop = 0, vbot = 0;
    s32 ytop = 192, ybot = 0;
    s32 xtop = 256, xbot = 0;
    u32 wshift = 0;

    for (int i = 0; i < nverts; i++)
    {
        Vertex* vtx = poly->Vertices[i];

        if (vtx->FinalPosition[1] < ytop || (vtx->FinalPosition[1] == ytop && vtx->FinalPosition[0] < xtop))
        {
            xtop = vtx->FinalPosition[0];
            ytop = vtx->FinalPosition[1];
            vtop = i;
        }
        if (vtx->FinalPosition[1] > ybot || (vtx->FinalPosition[1] == ybot && vtx->FinalPosition[0] > xbot))
        {
            xbot = vtx->FinalPosition[0];
            ybot = vtx->FinalPosition[1];
            vbot = i;
        }

        u32 w = (u32)vtx->Position[3];
        while ((w >> wshift) & 0xFFFF0000)
            wshift += 4;
    }

    poly->VTop = vtop; poly->VBottom = vbot;
    poly->YTop = ytop; poly->YBottom = ybot;
    poly->XTop = xtop; poly->XBottom = xbot;
    poly->WShift = wshift;
    poly->WBuffer = (FlushAttributes & 0x2);

    for (int i = 0; i < nverts; i++)
    {
        Vertex* vtx = poly->Vertices[i];
        s32 w = vtx->Position[3] >> wshift;

        s32 z;
        if (FlushAttributes & 0x2)
            z = w << wshift;
        else
            z = (((s64)vtx->Position[2] * 0x800000) / (w << wshift)) + 0x7FFEFF;

        // checkme
        if (z < 0) z = 0;
        else if (z > 0xFFFFFF) z = 0xFFFFFF;

        poly->FinalZ[i] = z;
        poly->FinalW[i] = w;
    }

    if (PolygonMode >= 2)
        LastStripPolygon = poly;
    else
        LastStripPolygon = NULL;
}

void SubmitVertex()
{
    s64 vertex[4] = {(s64)CurVertex[0], (s64)CurVertex[1], (s64)CurVertex[2], 0x1000};
    Vertex* vertextrans = &TempVertexBuffer[VertexNumInPoly];

    //printf("vertex: %08X %08X %08X, %d %d %d\n", CurVertex[0], CurVertex[1], CurVertex[2], VertexColor[0], VertexColor[1], VertexColor[2]);

    UpdateClipMatrix();
    vertextrans->Position[0] = (vertex[0]*ClipMatrix[0] + vertex[1]*ClipMatrix[4] + vertex[2]*ClipMatrix[8] + vertex[3]*ClipMatrix[12]) >> 12;
    vertextrans->Position[1] = (vertex[0]*ClipMatrix[1] + vertex[1]*ClipMatrix[5] + vertex[2]*ClipMatrix[9] + vertex[3]*ClipMatrix[13]) >> 12;
    vertextrans->Position[2] = (vertex[0]*ClipMatrix[2] + vertex[1]*ClipMatrix[6] + vertex[2]*ClipMatrix[10] + vertex[3]*ClipMatrix[14]) >> 12;
    vertextrans->Position[3] = (vertex[0]*ClipMatrix[3] + vertex[1]*ClipMatrix[7] + vertex[2]*ClipMatrix[11] + vertex[3]*ClipMatrix[15]) >> 12;

    vertextrans->Color[0] = (VertexColor[0] << 12) + 0xFFF;
    vertextrans->Color[1] = (VertexColor[1] << 12) + 0xFFF;
    vertextrans->Color[2] = (VertexColor[2] << 12) + 0xFFF;

    if ((TexParam >> 30) == 3)
    {
        vertextrans->TexCoords[0] = ((vertex[0]*TexMatrix[0] + vertex[1]*TexMatrix[4] + vertex[2]*TexMatrix[8]) >> 24) + RawTexCoords[0];
        vertextrans->TexCoords[1] = ((vertex[0]*TexMatrix[1] + vertex[1]*TexMatrix[5] + vertex[2]*TexMatrix[9]) >> 24) + RawTexCoords[1];
    }
    else
    {
        vertextrans->TexCoords[0] = TexCoords[0];
        vertextrans->TexCoords[1] = TexCoords[1];
    }

    vertextrans->Clipped = false;

    VertexNum++;
    VertexNumInPoly++;

    switch (PolygonMode)
    {
    case 0: // triangle
        if (VertexNumInPoly == 3)
        {
            VertexNumInPoly = 0;
            SubmitPolygon();
            NumConsecutivePolygons++;
        }
        break;

    case 1: // quad
        if (VertexNumInPoly == 4)
        {
            VertexNumInPoly = 0;
            SubmitPolygon();
            NumConsecutivePolygons++;
        }
        break;

    case 2: // triangle strip
        if (NumConsecutivePolygons & 1)
        {
            Vertex tmp = TempVertexBuffer[1];
            TempVertexBuffer[1] = TempVertexBuffer[0];
            TempVertexBuffer[0] = tmp;

            VertexNumInPoly = 2;
            SubmitPolygon();
            NumConsecutivePolygons++;

            TempVertexBuffer[1] = TempVertexBuffer[2];
        }
        else if (VertexNumInPoly == 3)
        {
            VertexNumInPoly = 2;
            SubmitPolygon();
            NumConsecutivePolygons++;

            TempVertexBuffer[0] = TempVertexBuffer[1];
            TempVertexBuffer[1] = TempVertexBuffer[2];
        }
        break;

    case 3: // quad strip
        if (VertexNumInPoly == 4)
        {
            Vertex tmp = TempVertexBuffer[3];
            TempVertexBuffer[3] = TempVertexBuffer[2];
            TempVertexBuffer[2] = tmp;

            VertexNumInPoly = 2;
            SubmitPolygon();
            NumConsecutivePolygons++;

            TempVertexBuffer[0] = TempVertexBuffer[3];
            TempVertexBuffer[1] = TempVertexBuffer[2];
        }
        break;
    }
}

s32 CalculateLighting()
{
    if ((TexParam >> 30) == 2)
    {
        TexCoords[0] = RawTexCoords[0] + (((s64)Normal[0]*TexMatrix[0] + (s64)Normal[1]*TexMatrix[4] + (s64)Normal[2]*TexMatrix[8]) >> 21);
        TexCoords[1] = RawTexCoords[1] + (((s64)Normal[0]*TexMatrix[1] + (s64)Normal[1]*TexMatrix[5] + (s64)Normal[2]*TexMatrix[9]) >> 21);
    }

    s32 normaltrans[3];
    normaltrans[0] = (Normal[0]*VecMatrix[0] + Normal[1]*VecMatrix[4] + Normal[2]*VecMatrix[8]) >> 12;
    normaltrans[1] = (Normal[0]*VecMatrix[1] + Normal[1]*VecMatrix[5] + Normal[2]*VecMatrix[9]) >> 12;
    normaltrans[2] = (Normal[0]*VecMatrix[2] + Normal[1]*VecMatrix[6] + Normal[2]*VecMatrix[10]) >> 12;

    VertexColor[0] = MatEmission[0];
    VertexColor[1] = MatEmission[1];
    VertexColor[2] = MatEmission[2];

    s32 c = 0;
    for (int i = 0; i < 4; i++)
    {
        if (!(CurPolygonAttr & (1<<i)))
            continue;

        // overflow handling (for example, if the normal length is >1)
        // according to some hardware tests
        // * diffuse level is saturated to 255
        // * shininess level mirrors back to 0 and is ANDed with 0xFF, that before being squared

        s32 difflevel = (-(LightDirection[i][0]*normaltrans[0] +
                         LightDirection[i][1]*normaltrans[1] +
                         LightDirection[i][2]*normaltrans[2])) >> 10;
        if (difflevel < 0) difflevel = 0;
        else if (difflevel > 255) difflevel = 255;

        s32 shinelevel = -(((LightDirection[i][0]>>1)*normaltrans[0] +
                          (LightDirection[i][1]>>1)*normaltrans[1] +
                          ((LightDirection[i][2]-0x200)>>1)*normaltrans[2]) >> 10);
        if (shinelevel < 0) shinelevel = 0;
        else if (shinelevel > 255) shinelevel = (0x100 - shinelevel) & 0xFF;
        shinelevel = ((shinelevel * shinelevel) >> 7) - 0x100; // really (2*shinelevel*shinelevel)-1
        if (shinelevel < 0) shinelevel = 0;

        if (UseShininessTable)
        {
            // checkme
            shinelevel >>= 1;
            shinelevel = ShininessTable[shinelevel];
        }

        VertexColor[0] += ((MatSpecular[0] * LightColor[i][0] * shinelevel) >> 13);
        VertexColor[0] += ((MatDiffuse[0] * LightColor[i][0] * difflevel) >> 13);
        VertexColor[0] += ((MatAmbient[0] * LightColor[i][0]) >> 5);

        VertexColor[1] += ((MatSpecular[1] * LightColor[i][1] * shinelevel) >> 13);
        VertexColor[1] += ((MatDiffuse[1] * LightColor[i][1] * difflevel) >> 13);
        VertexColor[1] += ((MatAmbient[1] * LightColor[i][1]) >> 5);

        VertexColor[2] += ((MatSpecular[2] * LightColor[i][2] * shinelevel) >> 13);
        VertexColor[2] += ((MatDiffuse[2] * LightColor[i][2] * difflevel) >> 13);
        VertexColor[2] += ((MatAmbient[2] * LightColor[i][2]) >> 5);

        if (VertexColor[0] > 31) VertexColor[0] = 31;
        if (VertexColor[1] > 31) VertexColor[1] = 31;
        if (VertexColor[2] > 31) VertexColor[2] = 31;

        c++;
    }

    // checkme: cycle count
    return c;
}


void BoxTest(u32* params)
{
    Vertex cube[8];
    Vertex face[10];
    int res;

    GXStat &= ~(1<<1);

    s32 x0 = (s32)(s16)(params[0] & 0xFFFF);
    s32 y0 = ((s32)params[0]) >> 16;
    s32 z0 = (s32)(s16)(params[1] & 0xFFFF);
    s32 x1 = ((s32)params[1]) >> 16;
    s32 y1 = (s32)(s16)(params[2] & 0xFFFF);
    s32 z1 = ((s32)params[2]) >> 16;

    x1 += x0;
    y1 += y0;
    z1 += z0;

    cube[0].Position[0] = x0; cube[0].Position[1] = y0; cube[0].Position[2] = z0;
    cube[1].Position[0] = x1; cube[1].Position[1] = y0; cube[1].Position[2] = z0;
    cube[2].Position[0] = x1; cube[2].Position[1] = y1; cube[2].Position[2] = z0;
    cube[3].Position[0] = x0; cube[3].Position[1] = y1; cube[3].Position[2] = z0;
    cube[4].Position[0] = x0; cube[4].Position[1] = y1; cube[4].Position[2] = z1;
    cube[5].Position[0] = x0; cube[5].Position[1] = y0; cube[5].Position[2] = z1;
    cube[6].Position[0] = x1; cube[6].Position[1] = y0; cube[6].Position[2] = z1;
    cube[7].Position[0] = x1; cube[7].Position[1] = y1; cube[7].Position[2] = z1;

    UpdateClipMatrix();
    for (int i = 0; i < 8; i++)
    {
        s32 x = cube[i].Position[0];
        s32 y = cube[i].Position[1];
        s32 z = cube[i].Position[2];

        cube[i].Position[0] = ((s64)x*ClipMatrix[0] + (s64)y*ClipMatrix[4] + (s64)z*ClipMatrix[8] + 0x1000*ClipMatrix[12]) >> 12;
        cube[i].Position[1] = ((s64)x*ClipMatrix[1] + (s64)y*ClipMatrix[5] + (s64)z*ClipMatrix[9] + 0x1000*ClipMatrix[13]) >> 12;
        cube[i].Position[2] = ((s64)x*ClipMatrix[2] + (s64)y*ClipMatrix[6] + (s64)z*ClipMatrix[10] + 0x1000*ClipMatrix[14]) >> 12;
        cube[i].Position[3] = ((s64)x*ClipMatrix[3] + (s64)y*ClipMatrix[7] + (s64)z*ClipMatrix[11] + 0x1000*ClipMatrix[15]) >> 12;
    }

    // front face (-Z)
    face[0] = cube[0]; face[1] = cube[1]; face[2] = cube[2]; face[3] = cube[3];
    res = ClipPolygon<false>(face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }

    // back face (+Z)
    face[0] = cube[4]; face[1] = cube[5]; face[2] = cube[6]; face[3] = cube[7];
    res = ClipPolygon<false>(face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }

    // left face (-X)
    face[0] = cube[0]; face[1] = cube[3]; face[2] = cube[4]; face[3] = cube[5];
    res = ClipPolygon<false>(face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }

    // right face (+X)
    face[0] = cube[1]; face[1] = cube[2]; face[2] = cube[7]; face[3] = cube[6];
    res = ClipPolygon<false>(face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }

    // bottom face (-Y)
    face[0] = cube[0]; face[1] = cube[1]; face[2] = cube[6]; face[3] = cube[5];
    res = ClipPolygon<false>(face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }

    // top face (+Y)
    face[0] = cube[2]; face[1] = cube[3]; face[2] = cube[4]; face[3] = cube[7];
    res = ClipPolygon<false>(face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }
}

void PosTest()
{
    s64 vertex[4] = {(s64)CurVertex[0], (s64)CurVertex[1], (s64)CurVertex[2], 0x1000};

    UpdateClipMatrix();
    PosTestResult[0] = (vertex[0]*ClipMatrix[0] + vertex[1]*ClipMatrix[4] + vertex[2]*ClipMatrix[8] + vertex[3]*ClipMatrix[12]) >> 12;
    PosTestResult[1] = (vertex[0]*ClipMatrix[1] + vertex[1]*ClipMatrix[5] + vertex[2]*ClipMatrix[9] + vertex[3]*ClipMatrix[13]) >> 12;
    PosTestResult[2] = (vertex[0]*ClipMatrix[2] + vertex[1]*ClipMatrix[6] + vertex[2]*ClipMatrix[10] + vertex[3]*ClipMatrix[14]) >> 12;
    PosTestResult[3] = (vertex[0]*ClipMatrix[3] + vertex[1]*ClipMatrix[7] + vertex[2]*ClipMatrix[11] + vertex[3]*ClipMatrix[15]) >> 12;
}

void VecTest(u32* params)
{
    // TODO: maybe it overwrites the normal registers, too

    s16 normal[3];

    normal[0] = (s16)((params[0] & 0x000003FF) << 6) >> 6;
    normal[1] = (s16)((params[0] & 0x000FFC00) >> 4) >> 6;
    normal[2] = (s16)((params[0] & 0x3FF00000) >> 14) >> 6;

    VecTestResult[0] = (normal[0]*VecMatrix[0] + normal[1]*VecMatrix[4] + normal[2]*VecMatrix[8]) >> 9;
    VecTestResult[1] = (normal[0]*VecMatrix[1] + normal[1]*VecMatrix[5] + normal[2]*VecMatrix[9]) >> 9;
    VecTestResult[2] = (normal[0]*VecMatrix[2] + normal[1]*VecMatrix[6] + normal[2]*VecMatrix[10]) >> 9;

    if (VecTestResult[0] & 0x1000) VecTestResult[0] |= 0xF000;
    if (VecTestResult[1] & 0x1000) VecTestResult[1] |= 0xF000;
    if (VecTestResult[2] & 0x1000) VecTestResult[2] |= 0xF000;
}



void CmdFIFOWrite(CmdFIFOEntry& entry)
{
    if (CmdFIFO->IsEmpty() && !CmdPIPE->IsFull())
    {
        CmdPIPE->Write(entry);
    }
    else
    {
        if (CmdFIFO->IsFull())
        {
            //printf("!!! GX FIFO FULL\n");
            //return;

            // temp. hack
            // SM64DS seems to overflow the FIFO occasionally
            // either leftover bugs in our implementation, or the game accidentally doing that
            // TODO: investigate.
            // TODO: implement this behavior properly (freezes the bus until the FIFO isn't full anymore)

            while (CmdFIFO->IsFull())
                ExecuteCommand();
        }

        CmdFIFO->Write(entry);
    }

    if (entry.Command == 0x11 || entry.Command == 0x12)
    {
        GXStat |= (1<<14); // push/pop matrix
        NumPushPopCommands++;
    }
    else if (entry.Command == 0x70 || entry.Command == 0x71 || entry.Command == 0x72)
    {
        GXStat |= (1<<0); // box/pos/vec test
        NumTestCommands++;
    }
}

CmdFIFOEntry CmdFIFORead()
{
    CmdFIFOEntry ret = CmdPIPE->Read();

    if (CmdPIPE->Level() <= 2)
    {
        if (!CmdFIFO->IsEmpty())
            CmdPIPE->Write(CmdFIFO->Read());
        if (!CmdFIFO->IsEmpty())
            CmdPIPE->Write(CmdFIFO->Read());

        CheckFIFODMA();
        CheckFIFOIRQ();
    }

    return ret;
}



void ExecuteCommand()
{
    CmdFIFOEntry entry = CmdFIFORead();

    //printf("FIFO: processing %02X %08X. Levels: FIFO=%d, PIPE=%d\n", entry.Command, entry.Param, CmdFIFO->Level(), CmdPIPE->Level());

    ExecParams[ExecParamCount] = entry.Param;
    ExecParamCount++;

    if (ExecParamCount >= CmdNumParams[entry.Command])
    {
        CycleCount += CmdNumCycles[entry.Command];
        ExecParamCount = 0;

        if (CycleCount > 0)
            GXStat |= (1<<27);

        switch (entry.Command)
        {
        case 0x10: // matrix mode
            MatrixMode = ExecParams[0] & 0x3;
            break;

        case 0x11: // push matrix
            NumPushPopCommands--;
            if (MatrixMode == 0)
            {
                if (ProjMatrixStackPointer > 0)
                {
                    printf("!! PROJ MATRIX STACK OVERFLOW\n");
                    GXStat |= (1<<15);
                    break;
                }

                memcpy(ProjMatrixStack, ProjMatrix, 16*4);
                ProjMatrixStackPointer++;
            }
            else if (MatrixMode == 3)
            {
                if (TexMatrixStackPointer > 0)
                {
                    printf("!! TEX MATRIX STACK OVERFLOW\n");
                    GXStat |= (1<<15);
                    break;
                }

                memcpy(TexMatrixStack, TexMatrix, 16*4);
                TexMatrixStackPointer++;
            }
            else
            {
                if (PosMatrixStackPointer > 30)
                {
                    printf("!! POS MATRIX STACK OVERFLOW\n");
                    GXStat |= (1<<15);
                    break;
                }

                memcpy(PosMatrixStack[PosMatrixStackPointer], PosMatrix, 16*4);
                memcpy(VecMatrixStack[PosMatrixStackPointer], VecMatrix, 16*4);
                PosMatrixStackPointer++;
            }
            break;

        case 0x12: // pop matrix
            NumPushPopCommands--;
            if (MatrixMode == 0)
            {
                if (ProjMatrixStackPointer <= 0)
                {
                    printf("!! PROJ MATRIX STACK UNDERFLOW\n");
                    GXStat |= (1<<15);
                    break;
                }

                ProjMatrixStackPointer--;
                memcpy(ProjMatrix, ProjMatrixStack, 16*4);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
            {
                if (TexMatrixStackPointer <= 0)
                {
                    printf("!! TEX MATRIX STACK UNDERFLOW\n");
                    GXStat |= (1<<15);
                    break;
                }

                TexMatrixStackPointer--;
                memcpy(TexMatrix, TexMatrixStack, 16*4);
            }
            else
            {
                s32 offset = (s32)(ExecParams[0] << 26) >> 26;
                PosMatrixStackPointer -= offset;

                if (PosMatrixStackPointer < 0 || PosMatrixStackPointer > 30)
                {
                    printf("!! POS MATRIX STACK UNDER/OVERFLOW %d\n", PosMatrixStackPointer);
                    PosMatrixStackPointer += offset;
                    GXStat |= (1<<15);
                    break;
                }

                memcpy(PosMatrix, PosMatrixStack[PosMatrixStackPointer], 16*4);
                memcpy(VecMatrix, VecMatrixStack[PosMatrixStackPointer], 16*4);
                ClipMatrixDirty = true;
            }
            break;

        case 0x13: // store matrix
            if (MatrixMode == 0)
            {
                memcpy(ProjMatrixStack, ProjMatrix, 16*4);
            }
            else if (MatrixMode == 3)
            {
                memcpy(TexMatrixStack, TexMatrix, 16*4);
            }
            else
            {
                u32 addr = ExecParams[0] & 0x1F;
                if (addr > 30)
                {
                    printf("!! POS MATRIX STORE ADDR 31\n");
                    GXStat |= (1<<15);
                    break;
                }

                memcpy(PosMatrixStack[addr], PosMatrix, 16*4);
                memcpy(VecMatrixStack[addr], VecMatrix, 16*4);
            }
            break;

        case 0x14: // restore matrix
            if (MatrixMode == 0)
            {
                memcpy(ProjMatrix, ProjMatrixStack, 16*4);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
            {
                memcpy(TexMatrix, TexMatrixStack, 16*4);
            }
            else
            {
                u32 addr = ExecParams[0] & 0x1F;
                if (addr > 30)
                {
                    printf("!! POS MATRIX STORE ADDR 31\n");
                    GXStat |= (1<<15);
                    break;
                }

                memcpy(PosMatrix, PosMatrixStack[addr], 16*4);
                memcpy(VecMatrix, VecMatrixStack[addr], 16*4);
                ClipMatrixDirty = true;
            }
            break;

        case 0x15: // identity
            if (MatrixMode == 0)
            {
                MatrixLoadIdentity(ProjMatrix);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
                MatrixLoadIdentity(TexMatrix);
            else
            {
                MatrixLoadIdentity(PosMatrix);
                if (MatrixMode == 2)
                    MatrixLoadIdentity(VecMatrix);
                ClipMatrixDirty = true;
            }
            break;

        case 0x16: // load 4x4
            if (MatrixMode == 0)
            {
                MatrixLoad4x4(ProjMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
                MatrixLoad4x4(TexMatrix, (s32*)ExecParams);
            else
            {
                MatrixLoad4x4(PosMatrix, (s32*)ExecParams);
                if (MatrixMode == 2)
                    MatrixLoad4x4(VecMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            break;

        case 0x17: // load 4x3
            if (MatrixMode == 0)
            {
                MatrixLoad4x3(ProjMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
                MatrixLoad4x3(TexMatrix, (s32*)ExecParams);
            else
            {
                MatrixLoad4x3(PosMatrix, (s32*)ExecParams);
                if (MatrixMode == 2)
                    MatrixLoad4x3(VecMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            break;

        case 0x18: // mult 4x4
            if (MatrixMode == 0)
            {
                MatrixMult4x4(ProjMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
                MatrixMult4x4(TexMatrix, (s32*)ExecParams);
            else
            {
                MatrixMult4x4(PosMatrix, (s32*)ExecParams);
                if (MatrixMode == 2)
                {
                    MatrixMult4x4(VecMatrix, (s32*)ExecParams);
                    CycleCount += 30;
                }
                ClipMatrixDirty = true;
            }
            break;

        case 0x19: // mult 4x3
            if (MatrixMode == 0)
            {
                MatrixMult4x3(ProjMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
                MatrixMult4x3(TexMatrix, (s32*)ExecParams);
            else
            {
                MatrixMult4x3(PosMatrix, (s32*)ExecParams);
                if (MatrixMode == 2)
                {
                    MatrixMult4x3(VecMatrix, (s32*)ExecParams);
                    CycleCount += 30;
                }
                ClipMatrixDirty = true;
            }
            break;

        case 0x1A: // mult 3x3
            if (MatrixMode == 0)
            {
                MatrixMult3x3(ProjMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
                MatrixMult3x3(TexMatrix, (s32*)ExecParams);
            else
            {
                MatrixMult3x3(PosMatrix, (s32*)ExecParams);
                if (MatrixMode == 2)
                {
                    MatrixMult3x3(VecMatrix, (s32*)ExecParams);
                    CycleCount += 30;
                }
                ClipMatrixDirty = true;
            }
            break;

        case 0x1B: // scale
            if (MatrixMode == 0)
            {
                MatrixScale(ProjMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
                MatrixScale(TexMatrix, (s32*)ExecParams);
            else
            {
                MatrixScale(PosMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            break;

        case 0x1C: // translate
            if (MatrixMode == 0)
            {
                MatrixTranslate(ProjMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
                MatrixTranslate(TexMatrix, (s32*)ExecParams);
            else
            {
                MatrixTranslate(PosMatrix, (s32*)ExecParams);
                if (MatrixMode == 2)
                    MatrixTranslate(VecMatrix, (s32*)ExecParams);
                ClipMatrixDirty = true;
            }
            break;

        case 0x20: // vertex color
            {
                u32 c = ExecParams[0];
                u32 r = c & 0x1F;
                u32 g = (c >> 5) & 0x1F;
                u32 b = (c >> 10) & 0x1F;
                VertexColor[0] = r;
                VertexColor[1] = g;
                VertexColor[2] = b;
            }
            break;

        case 0x21: // normal
            Normal[0] = (s16)((ExecParams[0] & 0x000003FF) << 6) >> 6;
            Normal[1] = (s16)((ExecParams[0] & 0x000FFC00) >> 4) >> 6;
            Normal[2] = (s16)((ExecParams[0] & 0x3FF00000) >> 14) >> 6;
            CycleCount += CalculateLighting();
            break;

        case 0x22: // texcoord
            RawTexCoords[0] = ExecParams[0] & 0xFFFF;
            RawTexCoords[1] = ExecParams[0] >> 16;
            if ((TexParam >> 30) == 1)
            {
                TexCoords[0] = (RawTexCoords[0]*TexMatrix[0] + RawTexCoords[1]*TexMatrix[4] + TexMatrix[8] + TexMatrix[12]) >> 12;
                TexCoords[1] = (RawTexCoords[0]*TexMatrix[1] + RawTexCoords[1]*TexMatrix[5] + TexMatrix[9] + TexMatrix[13]) >> 12;
            }
            else
            {
                TexCoords[0] = RawTexCoords[0];
                TexCoords[1] = RawTexCoords[1];
            }
            break;

        case 0x23: // full vertex
            CurVertex[0] = ExecParams[0] & 0xFFFF;
            CurVertex[1] = ExecParams[0] >> 16;
            CurVertex[2] = ExecParams[1] & 0xFFFF;
            SubmitVertex();
            break;

        case 0x24: // 10-bit vertex
            CurVertex[0] = (ExecParams[0] & 0x000003FF) << 6;
            CurVertex[1] = (ExecParams[0] & 0x000FFC00) >> 4;
            CurVertex[2] = (ExecParams[0] & 0x3FF00000) >> 14;
            SubmitVertex();
            break;

        case 0x25: // vertex XY
            CurVertex[0] = ExecParams[0] & 0xFFFF;
            CurVertex[1] = ExecParams[0] >> 16;
            SubmitVertex();
            break;

        case 0x26: // vertex XZ
            CurVertex[0] = ExecParams[0] & 0xFFFF;
            CurVertex[2] = ExecParams[0] >> 16;
            SubmitVertex();
            break;

        case 0x27: // vertex YZ
            CurVertex[1] = ExecParams[0] & 0xFFFF;
            CurVertex[2] = ExecParams[0] >> 16;
            SubmitVertex();
            break;

        case 0x28: // 10-bit delta vertex
            CurVertex[0] += (s16)((ExecParams[0] & 0x000003FF) << 6) >> 6;
            CurVertex[1] += (s16)((ExecParams[0] & 0x000FFC00) >> 4) >> 6;
            CurVertex[2] += (s16)((ExecParams[0] & 0x3FF00000) >> 14) >> 6;
            SubmitVertex();
            break;

        case 0x29: // polygon attributes
            PolygonAttr = ExecParams[0];
            break;

        case 0x2A: // texture param
            TexParam = ExecParams[0];
            break;

        case 0x2B: // texture palette
            TexPalette = ExecParams[0] & 0x1FFF;
            break;

        case 0x30: // diffuse/ambient material
            MatDiffuse[0] = ExecParams[0] & 0x1F;
            MatDiffuse[1] = (ExecParams[0] >> 5) & 0x1F;
            MatDiffuse[2] = (ExecParams[0] >> 10) & 0x1F;
            MatAmbient[0] = (ExecParams[0] >> 16) & 0x1F;
            MatAmbient[1] = (ExecParams[0] >> 21) & 0x1F;
            MatAmbient[2] = (ExecParams[0] >> 26) & 0x1F;
            if (ExecParams[0] & 0x8000)
            {
                VertexColor[0] = MatDiffuse[0];
                VertexColor[1] = MatDiffuse[1];
                VertexColor[2] = MatDiffuse[2];
            }
            break;

        case 0x31: // specular/emission material
            MatSpecular[0] = ExecParams[0] & 0x1F;
            MatSpecular[1] = (ExecParams[0] >> 5) & 0x1F;
            MatSpecular[2] = (ExecParams[0] >> 10) & 0x1F;
            MatEmission[0] = (ExecParams[0] >> 16) & 0x1F;
            MatEmission[1] = (ExecParams[0] >> 21) & 0x1F;
            MatEmission[2] = (ExecParams[0] >> 26) & 0x1F;
            UseShininessTable = (ExecParams[0] & 0x8000) != 0;
            break;

        case 0x32: // light direction
            {
                u32 l = ExecParams[0] >> 30;
                s16 dir[3];
                dir[0] = (s16)((ExecParams[0] & 0x000003FF) << 6) >> 6;
                dir[1] = (s16)((ExecParams[0] & 0x000FFC00) >> 4) >> 6;
                dir[2] = (s16)((ExecParams[0] & 0x3FF00000) >> 14) >> 6;
                LightDirection[l][0] = (dir[0]*VecMatrix[0] + dir[1]*VecMatrix[4] + dir[2]*VecMatrix[8]) >> 12;
                LightDirection[l][1] = (dir[0]*VecMatrix[1] + dir[1]*VecMatrix[5] + dir[2]*VecMatrix[9]) >> 12;
                LightDirection[l][2] = (dir[0]*VecMatrix[2] + dir[1]*VecMatrix[6] + dir[2]*VecMatrix[10]) >> 12;
            }
            break;

        case 0x33: // light color
            {
                u32 l = ExecParams[0] >> 30;
                LightColor[l][0] = ExecParams[0] & 0x1F;
                LightColor[l][1] = (ExecParams[0] >> 5) & 0x1F;
                LightColor[l][2] = (ExecParams[0] >> 10) & 0x1F;
            }
            break;

        case 0x34: // shininess table
            {
                for (int i = 0; i < 128; i += 4)
                {
                    u32 val = ExecParams[i >> 2];
                    ShininessTable[i + 0] = val & 0xFF;
                    ShininessTable[i + 1] = (val >> 8) & 0xFF;
                    ShininessTable[i + 2] = (val >> 16) & 0xFF;
                    ShininessTable[i + 3] = val >> 24;
                }
            }
            break;

        case 0x40: // begin polygons
            PolygonMode = ExecParams[0] & 0x3;
            VertexNum = 0;
            VertexNumInPoly = 0;
            NumConsecutivePolygons = 0;
            LastStripPolygon = NULL;
            CurPolygonAttr = PolygonAttr;
            break;

        case 0x50: // flush
            FlushRequest |= 0x1;
            FlushAttributes = ExecParams[0] & 0x3;
            CycleCount = 392;
            break;

        case 0x60: // viewport x1,y1,x2,y2
            // note: viewport Y coordinates are upside-down
            Viewport[0] = ExecParams[0] & 0xFF;
            Viewport[1] = 191 - (ExecParams[0] >> 24);
            Viewport[2] = ((ExecParams[0] >> 16) & 0xFF) - Viewport[0] + 1;
            Viewport[3] = (191 - ((ExecParams[0] >> 8) & 0xFF)) - Viewport[1] + 1;
            break;

        case 0x70: // box test
            NumTestCommands -= 3;
            BoxTest(ExecParams);
            break;

        case 0x71: // pos test
            NumTestCommands -= 2;
            CurVertex[0] = ExecParams[0] & 0xFFFF;
            CurVertex[1] = ExecParams[0] >> 16;
            CurVertex[2] = ExecParams[1] & 0xFFFF;
            PosTest();
            break;

        case 0x72: // vec test
            NumTestCommands--;
            VecTest(ExecParams);
            break;

        default:
            //if (entry.Command != 0x41)
                //printf("!! UNKNOWN GX COMMAND %02X %08X\n", entry.Command, entry.Param);
            break;
        }
    }
}

void Run(s32 cycles)
{
    if (FlushRequest & 0x1)
        return;
    if (CycleCount <= 0 && CmdPIPE->IsEmpty())
        return;

    CycleCount -= cycles;

    if (CycleCount <= 0)
    {
        while (CycleCount <= 0 && !CmdPIPE->IsEmpty())
        {
            if (NumPushPopCommands == 0) GXStat &= ~(1<<14);
            if (NumTestCommands == 0)    GXStat &= ~(1<<0);

            ExecuteCommand();
        }
    }

    if (CycleCount <= 0 && CmdPIPE->IsEmpty())
    {
        CycleCount = 0;
        GXStat &= ~(1<<27);

        if (NumPushPopCommands == 0) GXStat &= ~(1<<14);
        if (NumTestCommands == 0)    GXStat &= ~(1<<0);
    }
}


void CheckFIFOIRQ()
{
    bool irq = false;
    switch (GXStat >> 30)
    {
    case 1: irq = (CmdFIFO->Level() < 128); break;
    case 2: irq = CmdFIFO->IsEmpty(); break;
    }

    if (irq) NDS::SetIRQ(0, NDS::IRQ_GXFIFO);
    else     NDS::ClearIRQ(0, NDS::IRQ_GXFIFO);
}

void CheckFIFODMA()
{
    if (CmdFIFO->Level() < 128)
        NDS::CheckDMAs(0, 0x07);
}


void VBlank()
{
    if (FlushRequest & 0x1)
    {
        RenderVertexRAM = CurVertexRAM;
        RenderPolygonRAM = CurPolygonRAM;
        RenderNumPolygons = NumPolygons;

        // TODO: find out which other registers are latched for rendering
        RenderClearAttr1 = ClearAttr1;
        RenderClearAttr2 = ClearAttr2;

        CurRAMBank = CurRAMBank?0:1;
        CurVertexRAM = &VertexRAM[CurRAMBank ? 6144 : 0];
        CurPolygonRAM = &PolygonRAM[CurRAMBank ? 2048 : 0];

        NumVertices = 0;
        NumPolygons = 0;

        FlushRequest &= ~0x1;
        FlushRequest |= 0x2;
    }
}

void VCount215()
{
    // TODO: detect other conditions that could require rerendering
    // the DS is said to present new 3D frames all the time, even if no commands are sent

    if (FlushRequest & 0x2)
    {
        SoftRenderer::RenderFrame(RenderVertexRAM, RenderPolygonRAM, RenderNumPolygons);

        FlushRequest &= ~0x2;
    }
}

u32* GetLine(int line)
{
    return SoftRenderer::GetLine(line);
}


void WriteToGXFIFO(u32 val)
{
    if (NumCommands == 0)
    {
        NumCommands = 4;
        CurCommand = val;
        ParamCount = 0;
        TotalParams = CmdNumParams[CurCommand & 0xFF];

        if (TotalParams > 0) return;
    }
    else
        ParamCount++;

    for (;;)
    {
        if ((CurCommand & 0xFF) || (NumCommands == 4 && CurCommand == 0))
        {
            CmdFIFOEntry entry;
            entry.Command = CurCommand & 0xFF;
            entry.Param = val;
            CmdFIFOWrite(entry);
        }

        if (ParamCount >= TotalParams)
        {
            CurCommand >>= 8;
            NumCommands--;
            if (NumCommands == 0) break;

            ParamCount = 0;
            TotalParams = CmdNumParams[CurCommand & 0xFF];
        }
        if (ParamCount < TotalParams)
            break;
    }
}


u8 Read8(u32 addr)
{
    printf("unknown GPU3D read8 %08X\n", addr);
    return 0;
}

u16 Read16(u32 addr)
{
    switch (addr)
    {
    case 0x04000060:
        return DispCnt;

    case 0x04000320:
        return 46; // TODO, eventually

    case 0x04000604:
        return NumPolygons;
    case 0x04000606:
        return NumVertices;

    case 0x04000630: return VecTestResult[0];
    case 0x04000632: return VecTestResult[1];
    case 0x04000634: return VecTestResult[2];
    }

    printf("unknown GPU3D read16 %08X\n", addr);
    return 0;
}

u32 Read32(u32 addr)
{
    switch (addr)
    {
    case 0x04000060:
        return DispCnt;

    case 0x04000320:
        return 46; // TODO, eventually

    case 0x04000600:
        {
            u32 fifolevel = CmdFIFO->Level();

            return GXStat |
                   ((PosMatrixStackPointer & 0x1F) << 8) |
                   ((ProjMatrixStackPointer & 0x1) << 13) |
                   (fifolevel << 16) |
                   (fifolevel < 128 ? (1<<25) : 0) |
                   (fifolevel == 0  ? (1<<26) : 0);
        }

    case 0x04000604:
        return NumPolygons | (NumVertices << 16);

    case 0x04000620: return PosTestResult[0];
    case 0x04000624: return PosTestResult[1];
    case 0x04000628: return PosTestResult[2];
    case 0x0400062C: return PosTestResult[3];

    case 0x04000680: return VecMatrix[0];
    case 0x04000684: return VecMatrix[1];
    case 0x04000688: return VecMatrix[2];
    case 0x0400068C: return VecMatrix[4];
    case 0x04000690: return VecMatrix[5];
    case 0x04000694: return VecMatrix[6];
    case 0x04000698: return VecMatrix[8];
    case 0x0400069C: return VecMatrix[9];
    case 0x040006A0: return VecMatrix[10];
    }

    if (addr >= 0x04000640 && addr < 0x04000680)
    {
        UpdateClipMatrix();
        return ClipMatrix[(addr & 0x3C) >> 2];
    }

    //printf("unknown GPU3D read32 %08X\n", addr);
    return 0;
}

void Write8(u32 addr, u8 val)
{
    switch (addr)
    {
    case 0x04000340:
        AlphaRefVal = val & 0x1F;
        AlphaRef = (DispCnt & (1<<2)) ? AlphaRefVal : 0;
        return;
    }

    if (addr >= 0x04000360 && addr < 0x04000380)
    {
        FogDensityTable[addr - 0x04000360] = val;
        return;
    }

    printf("unknown GPU3D write8 %08X %02X\n", addr, val);
}

void Write16(u32 addr, u16 val)
{
    switch (addr)
    {
    case 0x04000060:
        DispCnt = (val & 0x4FFF) | (DispCnt & 0x3000);
        if (val & (1<<12)) DispCnt &= ~(1<<12);
        if (val & (1<<13)) DispCnt &= ~(1<<13);
        AlphaRef = (DispCnt & (1<<2)) ? AlphaRefVal : 0;
        return;

    case 0x04000340:
        AlphaRefVal = val & 0x1F;
        AlphaRef = (DispCnt & (1<<2)) ? AlphaRefVal : 0;
        return;

    case 0x04000350:
        ClearAttr1 = (ClearAttr1 & 0xFFFF0000) | val;
        return;
    case 0x04000352:
        ClearAttr1 = (ClearAttr1 & 0xFFFF) | (val << 16);
        return;
    case 0x04000354:
        ClearAttr2 = (ClearAttr2 & 0xFFFF0000) | val;
        return;
    case 0x04000356:
        ClearAttr2 = (ClearAttr2 & 0xFFFF) | (val << 16);
        return;

    case 0x04000358:
        FogColor = (FogColor & 0xFFFF0000) | val;
        return;
    case 0x0400035A:
        FogColor = (FogColor & 0xFFFF) | (val << 16);
        return;
    case 0x0400035C:
        FogOffset = val;
        return;
    }

    if (addr >= 0x04000330 && addr < 0x04000340)
    {
        EdgeTable[(addr - 0x04000330) >> 1] = val;
        return;
    }

    if (addr >= 0x04000360 && addr < 0x04000380)
    {
        addr -= 0x04000360;
        FogDensityTable[addr] = val & 0xFF;
        FogDensityTable[addr+1] = val >> 8;
        return;
    }

    if (addr >= 0x04000380 && addr < 0x040003C0)
    {
        ToonTable[(addr - 0x04000380) >> 1] = val;
        return;
    }

    printf("unknown GPU3D write16 %08X %04X\n", addr, val);
}

void Write32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x04000060:
        DispCnt = (val & 0x4FFF) | (DispCnt & 0x3000);
        if (val & (1<<12)) DispCnt &= ~(1<<12);
        if (val & (1<<13)) DispCnt &= ~(1<<13);
        AlphaRef = (DispCnt & (1<<2)) ? AlphaRefVal : 0;
        return;

    case 0x04000340:
        AlphaRefVal = val & 0x1F;
        AlphaRef = (DispCnt & (1<<2)) ? AlphaRefVal : 0;
        return;

    case 0x04000350:
        ClearAttr1 = val;
        return;
    case 0x04000354:
        ClearAttr2 = val;
        return;

    case 0x04000358:
        FogColor = val;
        return;
    case 0x0400035C:
        FogOffset = val;
        return;

    case 0x04000600:
        if (val & 0x8000)
        {
            GXStat &= ~0x8000;
            ProjMatrixStackPointer = 0;
            //PosMatrixStackPointer = 0;
            TexMatrixStackPointer = 0;
        }
        val &= 0xC0000000;
        GXStat &= 0x3FFFFFFF;
        GXStat |= val;
        CheckFIFOIRQ();
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000440)
    {
        WriteToGXFIFO(val);
        return;
    }

    if (addr >= 0x04000440 && addr < 0x040005CC)
    {
        CmdFIFOEntry entry;
        entry.Command = (addr & 0x1FC) >> 2;
        entry.Param = val;
        CmdFIFOWrite(entry);
        return;
    }

    if (addr >= 0x04000330 && addr < 0x04000340)
    {
        addr = (addr - 0x04000330) >> 1;
        EdgeTable[addr] = val & 0xFFFF;
        EdgeTable[addr+1] = val >> 16;
        return;
    }

    if (addr >= 0x04000360 && addr < 0x04000380)
    {
        addr -= 0x04000360;
        FogDensityTable[addr] = val & 0xFF;
        FogDensityTable[addr+1] = (val >> 8) & 0xFF;
        FogDensityTable[addr+2] = (val >> 16) & 0xFF;
        FogDensityTable[addr+3] = val >> 24;
        return;
    }

    if (addr >= 0x04000380 && addr < 0x040003C0)
    {
        addr = (addr - 0x04000380) >> 1;
        ToonTable[addr] = val & 0xFFFF;
        ToonTable[addr+1] = val >> 16;
        return;
    }

    printf("unknown GPU3D write32 %08X %08X\n", addr, val);
}

}

