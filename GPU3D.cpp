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


namespace GPU3D
{

#define COPYVERTEX(a, b)  { *(u64*)&a[0] = *(u64*)&b[0]; *(u64*)&a[2] = *(u64*)&b[2]; }

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

u32 GXStat;

u32 ExecParams[32];
u32 ExecParamCount;
s32 CycleCount;


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
s32 ProjMatrixStackPointer;
s32 PosMatrixStackPointer;

void MatrixLoadIdentity(s32* m);
void UpdateClipMatrix();


u32 PolygonMode;
s16 CurVertex[3];

Vertex TempVertexBuffer[4];
u32 VertexNum;
u32 VertexNumInPoly;

Vertex VertexRAM[6144 * 2];
Polygon PolygonRAM[2048 * 2];

Vertex* CurVertexRAM;
Polygon* CurPolygonRAM;
u32 NumVertices, NumPolygons;
u32 CurRAMBank;



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
    ProjMatrixStackPointer = 0;
    PosMatrixStackPointer = 0;

    VertexNum = 0;
    VertexNumInPoly = 0;

    CurRAMBank = 0;
    CurVertexRAM = &VertexRAM[0];
    CurPolygonRAM = &PolygonRAM[0];
    NumVertices = 0;
    NumPolygons = 0;

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
    m[0] = (s[0]*tmp[0] + s[1]*tmp[4] + s[2]*tmp[8] + s[3]*tmp[12]) >> 12;
    m[1] = (s[0]*tmp[1] + s[1]*tmp[5] + s[2]*tmp[9] + s[3]*tmp[13]) >> 12;
    m[2] = (s[0]*tmp[2] + s[1]*tmp[6] + s[2]*tmp[10] + s[3]*tmp[14]) >> 12;
    m[3] = (s[0]*tmp[3] + s[1]*tmp[7] + s[2]*tmp[11] + s[3]*tmp[15]) >> 12;

    m[4] = (s[4]*tmp[0] + s[5]*tmp[4] + s[6]*tmp[8] + s[7]*tmp[12]) >> 12;
    m[5] = (s[4]*tmp[1] + s[5]*tmp[5] + s[6]*tmp[9] + s[7]*tmp[13]) >> 12;
    m[6] = (s[4]*tmp[2] + s[5]*tmp[6] + s[6]*tmp[10] + s[7]*tmp[14]) >> 12;
    m[7] = (s[4]*tmp[3] + s[5]*tmp[7] + s[6]*tmp[11] + s[7]*tmp[15]) >> 12;

    m[8] = (s[8]*tmp[0] + s[9]*tmp[4] + s[10]*tmp[8] + s[11]*tmp[12]) >> 12;
    m[9] = (s[8]*tmp[1] + s[9]*tmp[5] + s[10]*tmp[9] + s[11]*tmp[13]) >> 12;
    m[10] = (s[8]*tmp[2] + s[9]*tmp[6] + s[10]*tmp[10] + s[11]*tmp[14]) >> 12;
    m[11] = (s[8]*tmp[3] + s[9]*tmp[7] + s[10]*tmp[11] + s[11]*tmp[15]) >> 12;

    m[12] = (s[12]*tmp[0] + s[13]*tmp[4] + s[14]*tmp[8] + s[15]*tmp[12]) >> 12;
    m[13] = (s[12]*tmp[1] + s[13]*tmp[5] + s[14]*tmp[9] + s[15]*tmp[13]) >> 12;
    m[14] = (s[12]*tmp[2] + s[13]*tmp[6] + s[14]*tmp[10] + s[15]*tmp[14]) >> 12;
    m[15] = (s[12]*tmp[3] + s[13]*tmp[7] + s[14]*tmp[11] + s[15]*tmp[15]) >> 12;
}

void MatrixMult4x3(s32* m, s32* s)
{
    s32 tmp[16];
    memcpy(tmp, m, 16*4);

    // m = s*m
    m[0] = (s[0]*tmp[0] + s[1]*tmp[4] + s[2]*tmp[8]) >> 12;
    m[1] = (s[0]*tmp[1] + s[1]*tmp[5] + s[2]*tmp[9]) >> 12;
    m[2] = (s[0]*tmp[2] + s[1]*tmp[6] + s[2]*tmp[10]) >> 12;
    m[3] = (s[0]*tmp[3] + s[1]*tmp[7] + s[2]*tmp[11]) >> 12;

    m[4] = (s[3]*tmp[0] + s[4]*tmp[4] + s[5]*tmp[8]) >> 12;
    m[5] = (s[3]*tmp[1] + s[4]*tmp[5] + s[5]*tmp[9]) >> 12;
    m[6] = (s[3]*tmp[2] + s[4]*tmp[6] + s[5]*tmp[10]) >> 12;
    m[7] = (s[3]*tmp[3] + s[4]*tmp[7] + s[5]*tmp[11]) >> 12;

    m[8] = (s[6]*tmp[0] + s[7]*tmp[4] + s[8]*tmp[8]) >> 12;
    m[9] = (s[6]*tmp[1] + s[7]*tmp[5] + s[8]*tmp[9]) >> 12;
    m[10] = (s[6]*tmp[2] + s[7]*tmp[6] + s[8]*tmp[10]) >> 12;
    m[11] = (s[6]*tmp[3] + s[7]*tmp[7] + s[8]*tmp[11]) >> 12;

    m[12] = (s[9]*tmp[0] + s[10]*tmp[4] + s[11]*tmp[8] + 0x1000*tmp[12]) >> 12;
    m[13] = (s[9]*tmp[1] + s[10]*tmp[5] + s[11]*tmp[9] + 0x1000*tmp[13]) >> 12;
    m[14] = (s[9]*tmp[2] + s[10]*tmp[6] + s[11]*tmp[10] + 0x1000*tmp[14]) >> 12;
    m[15] = (s[9]*tmp[3] + s[10]*tmp[7] + s[11]*tmp[11] + 0x1000*tmp[15]) >> 12;
}

void MatrixMult3x3(s32* m, s32* s)
{
    s32 tmp[12];
    memcpy(tmp, m, 12*4);

    // m = s*m
    m[0] = (s[0]*tmp[0] + s[1]*tmp[4] + s[2]*tmp[8]) >> 12;
    m[1] = (s[0]*tmp[1] + s[1]*tmp[5] + s[2]*tmp[9]) >> 12;
    m[2] = (s[0]*tmp[2] + s[1]*tmp[6] + s[2]*tmp[10]) >> 12;
    m[3] = (s[0]*tmp[3] + s[1]*tmp[7] + s[2]*tmp[11]) >> 12;

    m[4] = (s[3]*tmp[0] + s[4]*tmp[4] + s[5]*tmp[8]) >> 12;
    m[5] = (s[3]*tmp[1] + s[4]*tmp[5] + s[5]*tmp[9]) >> 12;
    m[6] = (s[3]*tmp[2] + s[4]*tmp[6] + s[5]*tmp[10]) >> 12;
    m[7] = (s[3]*tmp[3] + s[4]*tmp[7] + s[5]*tmp[11]) >> 12;

    m[8] = (s[6]*tmp[0] + s[7]*tmp[4] + s[8]*tmp[8]) >> 12;
    m[9] = (s[6]*tmp[1] + s[7]*tmp[5] + s[8]*tmp[9]) >> 12;
    m[10] = (s[6]*tmp[2] + s[7]*tmp[6] + s[8]*tmp[10]) >> 12;
    m[11] = (s[6]*tmp[3] + s[7]*tmp[7] + s[8]*tmp[11]) >> 12;
}

void MatrixScale(s32* m, s32* s)
{
    m[0] = (s[0]*m[0]) >> 12;
    m[1] = (s[0]*m[1]) >> 12;
    m[2] = (s[0]*m[2]) >> 12;
    m[3] = (s[0]*m[3]) >> 12;

    m[4] = (s[1]*m[4]) >> 12;
    m[5] = (s[1]*m[5]) >> 12;
    m[6] = (s[1]*m[6]) >> 12;
    m[7] = (s[1]*m[7]) >> 12;

    m[8] = (s[2]*m[8]) >> 12;
    m[9] = (s[2]*m[9]) >> 12;
    m[10] = (s[2]*m[10]) >> 12;
    m[11] = (s[2]*m[11]) >> 12;
}

void MatrixTranslate(s32* m, s32* s)
{
    m[12] += (s[0]*m[0] + s[1]*m[4] + s[2]*m[8]) >> 12;
    m[13] += (s[0]*m[1] + s[1]*m[5] + s[2]*m[9]) >> 12;
    m[14] += (s[0]*m[2] + s[1]*m[6] + s[2]*m[10]) >> 12;
}

void UpdateClipMatrix()
{
    if (!ClipMatrixDirty) return;
    ClipMatrixDirty = false;

    memcpy(ClipMatrix, ProjMatrix, 16*4);
    MatrixMult4x4(ClipMatrix, PosMatrix);
}



void SubmitPolygon()
{
    // clip.
    // for each vertex:
    // if it's outside, check if the previous and next vertices are inside, if so, fixor

    Vertex clippedvertices[20];
    u32 numclipped;

    int nverts = PolygonMode & 0x1 ? 4:3;
    int nvisible = 0;

    /*for (int i = 0; i < nverts; i++)
    {
        s32* v = TempVertexBuffer[i].Position;

        if ((u32)(v[0]+0x1000) <= 0x2000 &&
            (u32)(v[1]+0x1000) <= 0x2000 &&
            (u32)(v[2]+0x1000) <= 0x2000)
        {
            nvisible++;
        }
    }

    if (!nvisible) return;*/

    int prev, next;
    int c;

    // X clipping

    prev = nverts-1; next = 1; c = 0;
    for (int i = 0; i < nverts; i++)
    {
        Vertex vtx = TempVertexBuffer[i];
        if (vtx.Position[0] > 0x1000)
        {
            Vertex* vprev = &TempVertexBuffer[prev];
            if (vprev->Position[0] <= 0x1000)
            {
                s32 factor = ((0x1000 - vprev->Position[0]) << 12) / (vtx.Position[0] - vprev->Position[0]);

                Vertex mid;
                mid.Position[0] = 0x1000;
                mid.Position[1] = vprev->Position[1] + (((vtx.Position[1] - vprev->Position[1]) * factor) >> 12);
                mid.Position[2] = vprev->Position[2] + (((vtx.Position[2] - vprev->Position[2]) * factor) >> 12);
                mid.Color[0] = vprev->Color[0] + (((vtx.Color[0] - vprev->Color[0]) * factor) >> 12);
                mid.Color[1] = vprev->Color[1] + (((vtx.Color[1] - vprev->Color[1]) * factor) >> 12);
                mid.Color[2] = vprev->Color[2] + (((vtx.Color[2] - vprev->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }

            Vertex* vnext = &TempVertexBuffer[next];
            if (vnext->Position[0] <= 0x1000)
            {
                s32 factor = ((0x1000 - vnext->Position[0]) << 12) / (vtx.Position[0] - vnext->Position[0]);

                Vertex mid;
                mid.Position[0] = 0x1000;
                mid.Position[1] = vnext->Position[1] + (((vtx.Position[1] - vnext->Position[1]) * factor) >> 12);
                mid.Position[2] = vnext->Position[2] + (((vtx.Position[2] - vnext->Position[2]) * factor) >> 12);
                mid.Color[0] = vnext->Color[0] + (((vtx.Color[0] - vnext->Color[0]) * factor) >> 12);
                mid.Color[1] = vnext->Color[1] + (((vtx.Color[1] - vnext->Color[1]) * factor) >> 12);
                mid.Color[2] = vnext->Color[2] + (((vtx.Color[2] - vnext->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }
        }
        else if (vtx.Position[0] < -0x1000)
        {
            Vertex* vprev = &TempVertexBuffer[prev];
            if (vprev->Position[0] >= -0x1000)
            {
                s32 factor = ((-0x1000 - vprev->Position[0]) << 12) / (vtx.Position[0] - vprev->Position[0]);

                Vertex mid;
                mid.Position[0] = -0x1000;
                mid.Position[1] = vprev->Position[1] + (((vtx.Position[1] - vprev->Position[1]) * factor) >> 12);
                mid.Position[2] = vprev->Position[2] + (((vtx.Position[2] - vprev->Position[2]) * factor) >> 12);
                mid.Color[0] = vprev->Color[0] + (((vtx.Color[0] - vprev->Color[0]) * factor) >> 12);
                mid.Color[1] = vprev->Color[1] + (((vtx.Color[1] - vprev->Color[1]) * factor) >> 12);
                mid.Color[2] = vprev->Color[2] + (((vtx.Color[2] - vprev->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }

            Vertex* vnext = &TempVertexBuffer[next];
            if (vnext->Position[0] >= -0x1000)
            {
                s32 factor = ((-0x1000 - vnext->Position[0]) << 12) / (vtx.Position[0] - vnext->Position[0]);

                Vertex mid;
                mid.Position[0] = -0x1000;
                mid.Position[1] = vnext->Position[1] + (((vtx.Position[1] - vnext->Position[1]) * factor) >> 12);
                mid.Position[2] = vnext->Position[2] + (((vtx.Position[2] - vnext->Position[2]) * factor) >> 12);
                mid.Color[0] = vnext->Color[0] + (((vtx.Color[0] - vnext->Color[0]) * factor) >> 12);
                mid.Color[1] = vnext->Color[1] + (((vtx.Color[1] - vnext->Color[1]) * factor) >> 12);
                mid.Color[2] = vnext->Color[2] + (((vtx.Color[2] - vnext->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }
        }
        else
            clippedvertices[c++] = vtx;

        prev++; if (prev >= nverts) prev = 0;
        next++; if (next >= nverts) next = 0;
    }

    // Y clipping

    nverts = c;
    prev = nverts-1; next = 1; c = 10;
    for (int i = 0; i < nverts; i++)
    {
        Vertex vtx = clippedvertices[i];
        if (vtx.Position[1] > 0x1000)
        {
            Vertex* vprev = &clippedvertices[prev];
            if (vprev->Position[1] <= 0x1000)
            {
                s32 factor = ((0x1000 - vprev->Position[1]) << 12) / (vtx.Position[1] - vprev->Position[1]);

                Vertex mid;
                mid.Position[0] = vprev->Position[0] + (((vtx.Position[0] - vprev->Position[0]) * factor) >> 12);
                mid.Position[1] = 0x1000;
                mid.Position[2] = vprev->Position[2] + (((vtx.Position[2] - vprev->Position[2]) * factor) >> 12);
                mid.Color[0] = vprev->Color[0] + (((vtx.Color[0] - vprev->Color[0]) * factor) >> 12);
                mid.Color[1] = vprev->Color[1] + (((vtx.Color[1] - vprev->Color[1]) * factor) >> 12);
                mid.Color[2] = vprev->Color[2] + (((vtx.Color[2] - vprev->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }

            Vertex* vnext = &clippedvertices[next];
            if (vnext->Position[1] <= 0x1000)
            {
                s32 factor = ((0x1000 - vnext->Position[1]) << 12) / (vtx.Position[1] - vnext->Position[1]);

                Vertex mid;
                mid.Position[0] = vnext->Position[0] + (((vtx.Position[0] - vnext->Position[0]) * factor) >> 12);
                mid.Position[1] = 0x1000;
                mid.Position[2] = vnext->Position[2] + (((vtx.Position[2] - vnext->Position[2]) * factor) >> 12);
                mid.Color[0] = vnext->Color[0] + (((vtx.Color[0] - vnext->Color[0]) * factor) >> 12);
                mid.Color[1] = vnext->Color[1] + (((vtx.Color[1] - vnext->Color[1]) * factor) >> 12);
                mid.Color[2] = vnext->Color[2] + (((vtx.Color[2] - vnext->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }
        }
        else if (vtx.Position[1] < -0x1000)
        {
            Vertex* vprev = &clippedvertices[prev];
            if (vprev->Position[1] >= -0x1000)
            {
                s32 factor = ((-0x1000 - vprev->Position[1]) << 12) / (vtx.Position[1] - vprev->Position[1]);

                Vertex mid;
                mid.Position[0] = vprev->Position[0] + (((vtx.Position[0] - vprev->Position[0]) * factor) >> 12);
                mid.Position[1] = -0x1000;
                mid.Position[2] = vprev->Position[2] + (((vtx.Position[2] - vprev->Position[2]) * factor) >> 12);
                mid.Color[0] = vprev->Color[0] + (((vtx.Color[0] - vprev->Color[0]) * factor) >> 12);
                mid.Color[1] = vprev->Color[1] + (((vtx.Color[1] - vprev->Color[1]) * factor) >> 12);
                mid.Color[2] = vprev->Color[2] + (((vtx.Color[2] - vprev->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }

            Vertex* vnext = &clippedvertices[next];
            if (vnext->Position[1] >= -0x1000)
            {
                s32 factor = ((-0x1000 - vnext->Position[1]) << 12) / (vtx.Position[1] - vnext->Position[1]);

                Vertex mid;
                mid.Position[0] = vnext->Position[0] + (((vtx.Position[0] - vnext->Position[0]) * factor) >> 12);
                mid.Position[1] = -0x1000;
                mid.Position[2] = vnext->Position[2] + (((vtx.Position[2] - vnext->Position[2]) * factor) >> 12);
                mid.Color[0] = vnext->Color[0] + (((vtx.Color[0] - vnext->Color[0]) * factor) >> 12);
                mid.Color[1] = vnext->Color[1] + (((vtx.Color[1] - vnext->Color[1]) * factor) >> 12);
                mid.Color[2] = vnext->Color[2] + (((vtx.Color[2] - vnext->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }
        }
        else
            clippedvertices[c++] = vtx;

        prev++; if (prev >= nverts) prev = 0;
        next++; if (next >= nverts) next = 0;
    }

    // Z clipping

    nverts = c-10;
    prev = nverts-1; next = 1; c = 0;
    for (int i = 0; i < nverts; i++)
    {
        Vertex vtx = clippedvertices[10+i];
        if (vtx.Position[2] > 0x1000)
        {
            Vertex* vprev = &clippedvertices[10+prev];
            if (vprev->Position[2] <= 0x1000)
            {
                s32 factor = ((0x1000 - vprev->Position[2]) << 12) / (vtx.Position[2] - vprev->Position[2]);

                Vertex mid;
                mid.Position[0] = vprev->Position[0] + (((vtx.Position[0] - vprev->Position[0]) * factor) >> 12);
                mid.Position[1] = vprev->Position[1] + (((vtx.Position[1] - vprev->Position[1]) * factor) >> 12);
                mid.Position[2] = 0x1000;
                mid.Color[0] = vprev->Color[0] + (((vtx.Color[0] - vprev->Color[0]) * factor) >> 12);
                mid.Color[1] = vprev->Color[1] + (((vtx.Color[1] - vprev->Color[1]) * factor) >> 12);
                mid.Color[2] = vprev->Color[2] + (((vtx.Color[2] - vprev->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }

            Vertex* vnext = &clippedvertices[10+next];
            if (vnext->Position[2] <= 0x1000)
            {
                s32 factor = ((0x1000 - vnext->Position[2]) << 12) / (vtx.Position[2] - vnext->Position[2]);

                Vertex mid;
                mid.Position[0] = vnext->Position[0] + (((vtx.Position[0] - vnext->Position[0]) * factor) >> 12);
                mid.Position[1] = vnext->Position[1] + (((vtx.Position[1] - vnext->Position[1]) * factor) >> 12);
                mid.Position[2] = 0x1000;
                mid.Color[0] = vnext->Color[0] + (((vtx.Color[0] - vnext->Color[0]) * factor) >> 12);
                mid.Color[1] = vnext->Color[1] + (((vtx.Color[1] - vnext->Color[1]) * factor) >> 12);
                mid.Color[2] = vnext->Color[2] + (((vtx.Color[2] - vnext->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }
        }
        else if (vtx.Position[2] < -0x1000)
        {
            Vertex* vprev = &clippedvertices[10+prev];
            if (vprev->Position[2] >= -0x1000)
            {
                s32 factor = ((-0x1000 - vprev->Position[2]) << 12) / (vtx.Position[2] - vprev->Position[2]);

                Vertex mid;
                mid.Position[0] = vprev->Position[0] + (((vtx.Position[0] - vprev->Position[0]) * factor) >> 12);
                mid.Position[1] = vprev->Position[1] + (((vtx.Position[1] - vprev->Position[1]) * factor) >> 12);
                mid.Position[2] = -0x1000;
                mid.Color[0] = vprev->Color[0] + (((vtx.Color[0] - vprev->Color[0]) * factor) >> 12);
                mid.Color[1] = vprev->Color[1] + (((vtx.Color[1] - vprev->Color[1]) * factor) >> 12);
                mid.Color[2] = vprev->Color[2] + (((vtx.Color[2] - vprev->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }

            Vertex* vnext = &clippedvertices[10+next];
            if (vnext->Position[2] >= -0x1000)
            {
                s32 factor = ((-0x1000 - vnext->Position[2]) << 12) / (vtx.Position[2] - vnext->Position[2]);

                Vertex mid;
                mid.Position[0] = vnext->Position[0] + (((vtx.Position[0] - vnext->Position[0]) * factor) >> 12);
                mid.Position[1] = vnext->Position[1] + (((vtx.Position[1] - vnext->Position[1]) * factor) >> 12);
                mid.Position[2] = -0x1000;
                mid.Color[0] = vnext->Color[0] + (((vtx.Color[0] - vnext->Color[0]) * factor) >> 12);
                mid.Color[1] = vnext->Color[1] + (((vtx.Color[1] - vnext->Color[1]) * factor) >> 12);
                mid.Color[2] = vnext->Color[2] + (((vtx.Color[2] - vnext->Color[2]) * factor) >> 12);

                clippedvertices[c++] = mid;
            }
        }
        else
            clippedvertices[c++] = vtx;

        prev++; if (prev >= nverts) prev = 0;
        next++; if (next >= nverts) next = 0;
    }

    if (c == 0) return;

    // build the actual polygon
    // TODO: tri/quad strips

    if (NumPolygons >= 2048) return;
    if (NumVertices+c > 6144) return;

    Polygon* poly = &CurPolygonRAM[NumPolygons++];
    poly->NumVertices = 0;

    for (int i = 0; i < c; i++)
    {
        CurVertexRAM[NumVertices] = clippedvertices[i];
        poly->Vertices[i] = &CurVertexRAM[NumVertices];

        NumVertices++;
        poly->NumVertices++;
    }
}

void SubmitVertex()
{
    s32 vertex[4] = {(s32)CurVertex[0], (s32)CurVertex[1], (s32)CurVertex[2], 0x1000};
    //s32 vertextrans[4];
    Vertex* vertextrans = &TempVertexBuffer[VertexNumInPoly];

    //printf("vertex: %f %f %f\n", vertex[0]/4096.0f, vertex[1]/4096.0f, vertex[2]/4096.0f);

    UpdateClipMatrix();
    vertextrans->Position[0] = (vertex[0]*ClipMatrix[0] + vertex[1]*ClipMatrix[4] + vertex[2]*ClipMatrix[8] + vertex[3]*ClipMatrix[12]) >> 12;
    vertextrans->Position[1] = (vertex[0]*ClipMatrix[1] + vertex[1]*ClipMatrix[5] + vertex[2]*ClipMatrix[9] + vertex[3]*ClipMatrix[13]) >> 12;
    vertextrans->Position[2] = (vertex[0]*ClipMatrix[2] + vertex[1]*ClipMatrix[6] + vertex[2]*ClipMatrix[10] + vertex[3]*ClipMatrix[14]) >> 12;
    vertextrans->Position[3] = (vertex[0]*ClipMatrix[3] + vertex[1]*ClipMatrix[7] + vertex[2]*ClipMatrix[11] + vertex[3]*ClipMatrix[15]) >> 12;

    s32 w_inv;
    if (vertextrans->Position[3] == 0)
        w_inv = 0x1000; // checkme
    else
        w_inv = 0x1000000 / vertextrans->Position[3];

    vertextrans->Position[0] = (vertextrans->Position[0] * w_inv) >> 12;
    vertextrans->Position[1] = (vertextrans->Position[1] * w_inv) >> 12;
    vertextrans->Position[2] = (vertextrans->Position[2] * w_inv) >> 12;

    // boo
    vertextrans->Color[0] = 63;
    vertextrans->Color[1] = 0;
    vertextrans->Color[2] = 63;

    /*printf("vertex trans: %f %f %f %f\n",
           vertextrans->Position[0]/4096.0f,
           vertextrans->Position[1]/4096.0f,
           vertextrans->Position[2]/4096.0f,
           vertextrans->Position[3]/4096.0f);*/

    /*if (vertextrans[3] == 0)
    {
        //printf("!!!! VERTEX W IS ZERO\n");
        //return;
        vertextrans[3] = 0x1000; // checkme
    }

    s32 screenX = (((vertextrans[0]+vertextrans[3]) * Viewport[2]) / (vertextrans[3]<<1)) + Viewport[0];
    s32 screenY = (((vertextrans[1]+vertextrans[3]) * Viewport[3]) / (vertextrans[3]<<1)) + Viewport[1];

    printf("screen: %d, %d\n", screenX, screenY);

    s32* finalvertex = TempVertexBuffer[VertexNumInPoly];
    finalvertex[0] = screenX;
    finalvertex[1] = screenY;
    finalvertex[2] = vertextrans[2];
    finalvertex[3] = vertextrans[3];*/

    // triangle strip:  0,1,2  1,2,3  2,3,4  3,4,5  ...
    // quad strip:  0,1,3,2  2,3,5,4  4,5,7,6  6,7,9,8  ...

    VertexNum++;
    VertexNumInPoly++;
    if (VertexNumInPoly >= (PolygonMode & 0x1 ? 4:3))
    {
        VertexNumInPoly = (PolygonMode & 0x2 ? 2:0);
        SubmitPolygon();
    }
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
            printf("!!! GX FIFO FULL\n");
            return;
        }

        CmdFIFO->Write(entry);
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

    ExecParams[ExecParamCount] = entry.Param;
    ExecParamCount++;

    if (ExecParamCount >= CmdNumParams[entry.Command])
    {
        CycleCount += CmdNumCycles[entry.Command];
        ExecParamCount = 0;

        GXStat &= ~(1<<14);

        //printf("3D CMD %02X\n", entry.Command);

        switch (entry.Command)
        {
        case 0x10: // matrix mode
            MatrixMode = ExecParams[0] & 0x3;
            break;

        case 0x11: // push matrix
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
                GXStat |= (1<<14);
            }
            else if (MatrixMode == 3)
            {
                printf("!! CAN'T PUSH TEXTURE MATRIX\n");
                GXStat |= (1<<15); // CHECKME
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
                PosMatrixStackPointer++;
                GXStat |= (1<<14);
            }
            break;

        case 0x12: // pop matrix
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
                GXStat |= (1<<14);
                ClipMatrixDirty = true;
            }
            else if (MatrixMode == 3)
            {
                printf("!! CAN'T POP TEXTURE MATRIX\n");
                GXStat |= (1<<15); // CHECKME
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
                GXStat |= (1<<14);
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
                printf("!! CAN'T STORE TEXTURE MATRIX\n");
                GXStat |= (1<<15); // CHECKME
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
                printf("!! CAN'T RESTORE TEXTURE MATRIX\n");
                GXStat |= (1<<15); // CHECKME
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

        case 0x21:
            // TODO: more cycles if lights are enabled
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

        case 0x40:
            PolygonMode = ExecParams[0] & 0x3;
            VertexNum = 0;
            VertexNumInPoly = 0;
            break;

        case 0x50:
            // TODO: make it happen upon VBlank, not right now
            break;

        case 0x60: // viewport x1,y1,x2,y2
            Viewport[0] = ExecParams[0] & 0xFF;
            Viewport[1] = (ExecParams[0] >> 8) & 0xFF;
            Viewport[2] = ((ExecParams[0] >> 16) & 0xFF) - Viewport[0] + 1;
            Viewport[3] = (ExecParams[0] >> 24) - Viewport[1] + 1;
            break;
        }
    }
}

void Run(s32 cycles)
{
    if (CycleCount <= 0)
    {
        while (CycleCount <= 0 && !CmdPIPE->IsEmpty())
            ExecuteCommand();
    }

    CycleCount -= cycles;

    if (CycleCount <= 0 && CmdPIPE->IsEmpty())
        CycleCount = 0;
}


void CheckFIFOIRQ()
{
    bool irq = false;
    switch (GXStat >> 30)
    {
    case 1: irq = (CmdFIFO->Level() < 128); break;
    case 2: irq = CmdFIFO->IsEmpty(); break;
    }

    if (irq) NDS::TriggerIRQ(0, NDS::IRQ_GXFIFO);
}

void CheckFIFODMA()
{
    if (CmdFIFO->Level() < 128)
        NDS::CheckDMAs(0, 0x07);
}


void VBlank()
{
    // TODO: only do this if a SwapBuffers command was issued
    SoftRenderer::RenderFrame(CurVertexRAM, CurPolygonRAM, NumPolygons);

    CurRAMBank = CurRAMBank?0:1;
    CurVertexRAM = &VertexRAM[CurRAMBank ? 6144 : 0];
    CurPolygonRAM = &PolygonRAM[CurRAMBank ? 2048 : 0];

    NumVertices = 0;
    NumPolygons = 0;
}


u8 Read8(u32 addr)
{
    return 0;
}

u16 Read16(u32 addr)
{
    return 0;
}

u32 Read32(u32 addr)
{
    switch (addr)
    {
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
                   (fifolevel == 0  ? (1<<26) : 0) |
                   (CycleCount > 0  ? (1<<27) : 0);
        }
    }

    if (addr >= 0x04000640 && addr < 0x04000680)
    {
        UpdateClipMatrix();
        return ClipMatrix[(addr & 0x3C) >> 2];
    }
    if (addr >= 0x04000680 && addr < 0x040006A4)
    {
        printf("!! VECMTX READ\n");
        return 0;
    }

    return 0;
}

void Write8(u32 addr, u8 val)
{
    //
}

void Write16(u32 addr, u16 val)
{
    //
}

void Write32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x04000600:
        if (val & 0x8000) GXStat &= ~0x8000;
        val &= 0xC0000000;
        GXStat &= 0x3FFFFFFF;
        GXStat |= val;
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000440)
    {
        if (NumCommands == 0)
        {
            NumCommands = 4;
            CurCommand = val;
            ParamCount = 0;
            TotalParams = CmdNumParams[CurCommand & 0xFF];
        }
        else
            ParamCount++;

        while (ParamCount == TotalParams)
        {
            CmdFIFOEntry entry;
            entry.Command = CurCommand & 0xFF;
            entry.Param = val;
            CmdFIFOWrite(entry);

            CurCommand >>= 8;
            NumCommands--;
            if (NumCommands == 0) break;

            ParamCount = 0;
            TotalParams = CmdNumParams[CurCommand & 0xFF];
        }

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
}

}

