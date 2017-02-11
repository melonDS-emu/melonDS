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
}


void RenderPolygon(Polygon* polygon)
{
    int nverts = polygon->NumVertices;

    int vtop, vbot;
    s32 ytop = 191, ybot = 0;
    s32 scrcoords[10][3];

    // find the topmost and bottommost vertices of the polygon

    for (int i = 0; i < nverts; i++)
    {
        Vertex* vtx = polygon->Vertices[i];

        s32 scrX = (((vtx->Position[0] + 0x1000) * Viewport[2]) >> 13) + Viewport[0];
        s32 scrY = (((vtx->Position[1] + 0x1000) * Viewport[3]) >> 13) + Viewport[1];
        if (scrX > 255) scrX = 255;
        if (scrY > 191) scrY = 191;

        scrcoords[i][0] = scrX;
        scrcoords[i][1] = 191 - scrY;
        scrcoords[i][2] = 0; // TODO: Z

        if (scrcoords[i][1] < ytop)
        {
            ytop = scrcoords[i][1];
            vtop = i;
        }
        if (scrcoords[i][1] > ybot)
        {
            ybot = scrcoords[i][1];
            vbot = i;
        }
    }

    // draw, line per line

    int lcur = vtop, rcur = vtop;
    int lnext, rnext;
    s32 lstep, rstep;
    //s32 xmin, xmax;

    lnext = lcur + 1;
    if (lnext >= nverts) lnext = 0;
    rnext = rcur - 1;
    if (rnext < 0) rnext = nverts - 1;

    /*if ((scrcoords[lnext][1] - scrcoords[lcur][1]) == 0) lstep = 0; else
    lstep = ((scrcoords[lnext][0] - scrcoords[lcur][0]) << 12) / (scrcoords[lnext][1] - scrcoords[lcur][1]);
    if ((scrcoords[rnext][1] - scrcoords[rcur][1]) == 0) rstep = 0; else
    rstep = ((scrcoords[rnext][0] - scrcoords[rcur][0]) << 12) / (scrcoords[rnext][1] - scrcoords[rcur][1]);*/

    //xmin = scrcoords[lcur][0] << 12;
    //xmax = scrcoords[rcur][0] << 12;

    for (s32 y = ytop; y <= ybot; y++)
    {
        if (y == scrcoords[lnext][1] && y < ybot)
        {
            lcur++;
            if (lcur >= nverts) lcur = 0;

            lnext = lcur + 1;
            if (lnext >= nverts) lnext = 0;

            //lstep = ((scrcoords[lnext][0] - scrcoords[lcur][0]) << 12) / (scrcoords[lnext][1] - scrcoords[lcur][1]);
            //xmin = scrcoords[lcur][0] << 12;
        }

        if (y == scrcoords[rnext][1] && y < ybot)
        {
            rcur--;
            if (rcur < 0) rcur = nverts - 1;

            rnext = rcur - 1;
            if (rnext < 0) rnext = nverts - 1;

            //rstep = ((scrcoords[rnext][0] - scrcoords[rcur][0]) << 12) / (scrcoords[rnext][1] - scrcoords[rcur][1]);
            //xmax = scrcoords[rcur][0] << 12;
        }

        s32 lfactor, rfactor;

        if (scrcoords[lnext][1] == scrcoords[lcur][1])
            lfactor = 0;
        else
            lfactor = ((y - scrcoords[lcur][1]) << 12) / (scrcoords[lnext][1] - scrcoords[lcur][1]);

        if (scrcoords[rnext][1] == scrcoords[rcur][1])
            rfactor = 0;
        else
            rfactor = ((y - scrcoords[rcur][1]) << 12) / (scrcoords[rnext][1] - scrcoords[rcur][1]);

        s32 xmin = scrcoords[lcur][0] + (((scrcoords[lnext][0] - scrcoords[lcur][0]) * lfactor) >> 12);
        s32 xmax = scrcoords[rcur][0] + (((scrcoords[rnext][0] - scrcoords[rcur][0]) * rfactor) >> 12);

        for (s32 x = xmin; x <= xmax; x++)
        {
            u8* pixel = &ColorBuffer[((256*y) + x) * 4];
            pixel[0] = 0;
            pixel[1] = 63;
            pixel[2] = 0;
            pixel[3] = 31;
        }
    }

    // test
    /*for (int i = 0; i < nverts; i++)
    {
        Vertex* vtx = polygon->Vertices[i];

        s32 scrX = (((vtx->Position[0] + 0x1000) * Viewport[2]) >> 13) + Viewport[0];
        s32 scrY = (((vtx->Position[1] + 0x1000) * Viewport[3]) >> 13) + Viewport[1];
        if (scrX > 255) scrX = 255;
        if (scrY > 191) scrY = 191;

        u8* pixel = &ColorBuffer[((256*scrY) + scrX) * 4];
        pixel[0] = 0;
        pixel[1] = 63;
        pixel[2] = 0;
        pixel[3] = 31;
    }*/
}

void RenderFrame(Vertex* vertices, Polygon* polygons, int npolys)
{
    // TODO: render translucent polygons last

    for (int i = 0; i < 256*192; i++)
    {
        ((u32*)ColorBuffer)[i] = 0x1F000000;
    }

    for (int i = 0; i < npolys; i++)
    {
        RenderPolygon(&polygons[i]);
    }
}

u8* GetLine(int line)
{
    return &ColorBuffer[line * 256 * 4];
}

}
}
