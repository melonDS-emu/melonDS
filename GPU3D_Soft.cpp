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
#include "GPU3D.h"


namespace GPU3D
{
namespace SoftRenderer
{

u8 ColorBuffer[256*192 * 4];
u32 DepthBuffer[256*192];


bool Init()
{
    return true;
}

void DeInit()
{
    //
}

void Reset()
{
    memset(ColorBuffer, 0, 256*192 * 4);
    memset(DepthBuffer, 0, 256*192 * 4);
}


void RenderPixel(u32 attr, s32 x, s32 y, s32 z, u8 vr, u8 vg, u8 vb)
{
    u32* depth = &DepthBuffer[(256*y) + x];

    bool passdepth = false;
    if (attr & (1<<14))
    {
        s32 diff = *depth - z;
        if ((u32)(diff + 0x200) <= 0x400)
            passdepth = true;
    }
    else
    if (z < *depth)
        passdepth = true;

    if (!passdepth) return;

    u8* pixel = &ColorBuffer[((256*y) + x) * 4];
    pixel[0] = vr;
    pixel[1] = vg;
    pixel[2] = vb;
    pixel[3] = 31; // TODO: alpha

    // TODO: optional update for translucent pixels
    *depth = z;
}

void RenderPolygon(Polygon* polygon)
{
    int nverts = polygon->NumVertices;
    bool isline = false;

    int vtop = 0, vbot = 0;
    s32 ytop = 192, ybot = 0;
    s32 xtop = 256, xbot = 0;

    // process the vertices, transform to screen coordinates
    // find the topmost and bottommost vertices of the polygon

    for (int i = 0; i < nverts; i++)
    {
        Vertex* vtx = polygon->Vertices[i];

        if (!vtx->ViewportTransformDone)
        {
            s32 posX, posY, posZ, posW;
            s32 w = vtx->Position[3];
            if (w == 0)
            {
                posX = 0;
                posY = 0;
                posZ = 0;
                posW = 0x1000;
            }
            else
            {
                posX = ((s64)vtx->Position[0] << 12) / w;
                posY = ((s64)vtx->Position[1] << 12) / w;

                // TODO: W-buffering
                posZ = (((s64)vtx->Position[2] * 0x800000) / w) + 0x7FFCFF;

                posW = w;
            }

            s32 scrX = (((posX + 0x1000) * Viewport[2]) >> 13) + Viewport[0];
            s32 scrY = ((0x180000 - ((posY + 0x1000) * Viewport[3])) >> 13) + Viewport[1];

            if      (scrX < 0)   scrX = 0;
            else if (scrX > 256) scrX = 256;
            if      (scrY < 0)   scrY = 0;
            else if (scrY > 192) scrY = 192;
            if      (posZ < 0)        posZ = 0;
            else if (posZ > 0xFFFFFF) posZ = 0xFFFFFF;

            vtx->FinalPosition[0] = scrX;
            vtx->FinalPosition[1] = scrY;
            vtx->FinalPosition[2] = posZ;
            vtx->FinalPosition[3] = posW;

            vtx->FinalColor[0] = vtx->Color[0] ? (((vtx->Color[0] >> 12) << 4) + 0xF) : 0;
            vtx->FinalColor[1] = vtx->Color[1] ? (((vtx->Color[1] >> 12) << 4) + 0xF) : 0;
            vtx->FinalColor[2] = vtx->Color[2] ? (((vtx->Color[2] >> 12) << 4) + 0xF) : 0;

            vtx->ViewportTransformDone = true;
        }

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
    }

    // draw, line per line

    int lcur = vtop, rcur = vtop;
    int lnext, rnext;

    if (ybot == ytop)
    {
        ybot++;
        isline = true;

        vtop = 0; vbot = 0;
        xtop = 256; xbot = 0;
        int i;

        i = 1;
        if (polygon->Vertices[i]->FinalPosition[0] < polygon->Vertices[vtop]->FinalPosition[0]) vtop = i;
        if (polygon->Vertices[i]->FinalPosition[0] > polygon->Vertices[vbot]->FinalPosition[0]) vbot = i;

        i = nverts - 1;
        if (polygon->Vertices[i]->FinalPosition[0] < polygon->Vertices[vtop]->FinalPosition[0]) vtop = i;
        if (polygon->Vertices[i]->FinalPosition[0] > polygon->Vertices[vbot]->FinalPosition[0]) vbot = i;

        lcur = vtop; lnext = vtop;
        rcur = vbot; rnext = vbot;
    }
    else
    {
        if (polygon->FacingView)
        {
            lnext = lcur + 1;
            if (lnext >= nverts) lnext = 0;
            rnext = rcur - 1;
            if (rnext < 0) rnext = nverts - 1;
        }
        else
        {
            lnext = lcur - 1;
            if (lnext < 0) lnext = nverts - 1;
            rnext = rcur + 1;
            if (rnext >= nverts) rnext = 0;
        }
    }

    for (s32 y = ytop; y < ybot; y++)
    {
        if (y > 191) break;

        if (!isline)
        {
            while (y >= polygon->Vertices[lnext]->FinalPosition[1] && lcur != vbot)
            {
                lcur = lnext;

                if (polygon->FacingView)
                {
                    lnext = lcur + 1;
                    if (lnext >= nverts) lnext = 0;
                }
                else
                {
                    lnext = lcur - 1;
                    if (lnext < 0) lnext = nverts - 1;
                }
            }

            while (y >= polygon->Vertices[rnext]->FinalPosition[1] && rcur != vbot)
            {
                rcur = rnext;

                if (polygon->FacingView)
                {
                    rnext = rcur - 1;
                    if (rnext < 0) rnext = nverts - 1;
                }
                else
                {
                    rnext = rcur + 1;
                    if (rnext >= nverts) rnext = 0;
                }
            }
        }

        Vertex* vlcur = polygon->Vertices[lcur];
        Vertex* vlnext = polygon->Vertices[lnext];
        Vertex* vrcur = polygon->Vertices[rcur];
        Vertex* vrnext = polygon->Vertices[rnext];

        s32 lfactor, rfactor;

        if (vlnext->FinalPosition[1] == vlcur->FinalPosition[1])
            lfactor = 0;
        else
            lfactor = ((y - vlcur->FinalPosition[1]) << 12) / (vlnext->FinalPosition[1] - vlcur->FinalPosition[1]);

        if (vrnext->FinalPosition[1] == vrcur->FinalPosition[1])
            rfactor = 0;
        else
            rfactor = ((y - vrcur->FinalPosition[1]) << 12) / (vrnext->FinalPosition[1] - vrcur->FinalPosition[1]);

        s32 xl = vlcur->FinalPosition[0] + (((vlnext->FinalPosition[0] - vlcur->FinalPosition[0]) * lfactor) >> 12);
        s32 xr = vrcur->FinalPosition[0] + (((vrnext->FinalPosition[0] - vrcur->FinalPosition[0]) * rfactor) >> 12);

        if (xl > xr) // TODO: handle it in a more elegant way
        {
            Vertex* vtmp;
            s32 tmp;

            vtmp = vlcur; vlcur = vrcur; vrcur = vtmp;
            vtmp = vlnext; vlnext = vrnext; vrnext = vtmp;

            tmp = lfactor; lfactor = rfactor; rfactor = tmp;

            tmp = xl; xl = xr; xr = tmp;
        }

        if (xl<0 || xr>256)
        {
            printf("!! BAD X %d %d\n", xl, xr);
            continue; // hax
        }

        s32 zl = vlcur->FinalPosition[2] + (((s64)(vlnext->FinalPosition[2] -vlcur->FinalPosition[2]) * lfactor) >> 12);
        s32 zr = vrcur->FinalPosition[2] + (((s64)(vrnext->FinalPosition[2] - vrcur->FinalPosition[2]) * rfactor) >> 12);

        s32 wl = vlcur->FinalPosition[3] + (((s64)(vlnext->FinalPosition[3] - vlcur->FinalPosition[3]) * lfactor) >> 12);
        s32 wr = vrcur->FinalPosition[3] + (((s64)(vrnext->FinalPosition[3] - vrcur->FinalPosition[3]) * rfactor) >> 12);

        s64 perspfactorl1 = ((s64)(0x1000 - lfactor) << 12) / vlcur->FinalPosition[3];
        s64 perspfactorl2 = ((s64)lfactor << 12)            / vlnext->FinalPosition[3];
        s64 perspfactorr1 = ((s64)(0x1000 - rfactor) << 12) / vrcur->FinalPosition[3];
        s64 perspfactorr2 = ((s64)rfactor << 12)            / vrnext->FinalPosition[3];

        if (perspfactorl1 + perspfactorl2 == 0)
        {
            perspfactorl1 = 0x1000;
            perspfactorl2 = 0;
        }
        if (perspfactorr1 + perspfactorr2 == 0)
        {
            perspfactorr1 = 0x1000;
            perspfactorr2 = 0;
        }

        s32 rl = ((perspfactorl1 * vlcur->FinalColor[0]) + (perspfactorl2 * vlnext->FinalColor[0])) / (perspfactorl1 + perspfactorl2);
        s32 gl = ((perspfactorl1 * vlcur->FinalColor[1]) + (perspfactorl2 * vlnext->FinalColor[1])) / (perspfactorl1 + perspfactorl2);
        s32 bl = ((perspfactorl1 * vlcur->FinalColor[2]) + (perspfactorl2 * vlnext->FinalColor[2])) / (perspfactorl1 + perspfactorl2);

        s32 rr = ((perspfactorr1 * vrcur->FinalColor[0]) + (perspfactorr2 * vrnext->FinalColor[0])) / (perspfactorr1 + perspfactorr2);
        s32 gr = ((perspfactorr1 * vrcur->FinalColor[1]) + (perspfactorr2 * vrnext->FinalColor[1])) / (perspfactorr1 + perspfactorr2);
        s32 br = ((perspfactorr1 * vrcur->FinalColor[2]) + (perspfactorr2 * vrnext->FinalColor[2])) / (perspfactorr1 + perspfactorr2);

        if (xr == xl) xr++;
        s32 xdiv = 0x1000 / (xr - xl);

        for (s32 x = xl; x < xr; x++)
        {
            s32 xfactor = (x - xl) * xdiv;

            s32 z = zl + (((s64)(zr - zl) * xfactor) >> 12);
            //z = wl + (((s64)(wr - wl) * xfactor) >> 12);
            //z -= 0x1FF;
            //if (z < 0) z = 0;

            s32 perspfactor1 = ((0x1000 - xfactor) << 12) / wl;
            s32 perspfactor2 = (xfactor << 12) / wr;

            if (perspfactor1 + perspfactor2 == 0)
            {
                perspfactor1 = 0x1000;
                perspfactor2 = 0;
            }

            // possible optimization: only do color interpolation if the depth test passes
            u32 vr = ((perspfactor1 * rl) + (perspfactor2 * rr)) / (perspfactor1 + perspfactor2);
            u32 vg = ((perspfactor1 * gl) + (perspfactor2 * gr)) / (perspfactor1 + perspfactor2);
            u32 vb = ((perspfactor1 * bl) + (perspfactor2 * br)) / (perspfactor1 + perspfactor2);

            RenderPixel(polygon->Attr, x, y, z, vr>>3, vg>>3, vb>>3);

            // Z debug
            /*u8 zerp = (w * 63) / 0xFFFFFF;
            pixel[0] = zerp;
            pixel[1] = zerp;
            pixel[2] = zerp;*/
        }
    }

    // DEBUG CODE
    /*for (int i = 0; i < nverts; i++)
    {
        s32 x = scrcoords[i][0];
        s32 y = scrcoords[i][1];

        u8* pixel = &ColorBuffer[((256*y) + x) * 4];
            pixel[0] = 63;
            pixel[1] = 63;
            pixel[2] = 63;
            pixel[3] = 31;
    }*/
}

void RenderFrame(Vertex* vertices, Polygon* polygons, int npolys)
{
    // TODO: render translucent polygons last

    // TODO proper clear color/depth support!
    for (int i = 0; i < 256*192; i++)
    {
        ((u32*)ColorBuffer)[i] = 0x00000000;
        DepthBuffer[i] = 0xFFFFFF;
    }

    for (int i = 0; i < npolys; i++)
    {
        /*printf("polygon %d: %d %d %d\n", i, polygons[i].Vertices[0]->Color[0], polygons[i].Vertices[0]->Color[1], polygons[i].Vertices[0]->Color[2]);
        for (int j = 0; j < polygons[i].NumVertices; j++)
            printf("  %d: %f %f %f\n",
                   j,
                   polygons[i].Vertices[j]->Position[0]/4096.0f,
                   polygons[i].Vertices[j]->Position[1]/4096.0f,
                   polygons[i].Vertices[j]->Position[2]/4096.0f);
*/
        //printf("polygon %d\n", i);
        //if (!polygons[i].Vertices[0]->Clipped) continue;
        //printf("polygon %d\n", i);
        RenderPolygon(&polygons[i]);
    }
}

u8* GetLine(int line)
{
    return &ColorBuffer[line * 256 * 4];
}

}
}
