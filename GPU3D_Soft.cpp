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

    int vtop = 0, vbot = 0;
    s32 ytop = 191, ybot = 0;
    s32 scrcoords[10][4];

    // find the topmost and bottommost vertices of the polygon

    for (int i = 0; i < nverts; i++)
    {
        Vertex* vtx = polygon->Vertices[i];

        s32 w_inv;
        if (vtx->Position[3] == 0)
        {
            w_inv = 0x1000; // checkme
            printf("!! W=0\n");
        }
        else
            w_inv = 0x1000000 / vtx->Position[3];

        if (vtx->Position[3] < 0) printf("!!! W=%d\n", vtx->Position[3]);

        s32 posX = (vtx->Position[0] * w_inv) >> 12;
        s32 posY = (vtx->Position[1] * w_inv) >> 12;
        s32 posZ = (vtx->Position[2] * w_inv) >> 12;
        //s32 posX = vtx->Position[0];
        //s32 posY = vtx->Position[1];

        s32 scrX = (((posX + 0x1000) * Viewport[2]) >> 13) + Viewport[0];
        s32 scrY = (((posY + 0x1000) * Viewport[3]) >> 13) + Viewport[1];
        s32 scrZ = (vtx->Position[2] + 0x1000) >> 1;
        if (scrX > 255) scrX = 255;
        if (scrY > 191) scrY = 191;
        if (scrZ > 0xFFF) scrZ = 0xFFF;
        if (scrX < 0) { printf("!! bad X %d\n", scrX); scrX = 0;}
        if (scrY < 0) { printf("!! bad Y %d\n", scrY); scrY = 0;}

        scrcoords[i][0] = scrX;
        scrcoords[i][1] = 191 - scrY;
        scrcoords[i][2] = scrZ;
        scrcoords[i][3] = vtx->Position[3];

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
        //if (vtx->Color[0]==63 && vtx->Color[1]==0 && vtx->Color[2]==0)
        //printf("v%d: %d,%d  Z=%f  W=%f\n", i, scrX, 191-scrY, vtx->Position[2]/4096.0f, vtx->Position[3]/4096.0f);
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
        if (y < ybot)
        {
            while (y == scrcoords[lnext][1])
            {
                lcur++;
                if (lcur >= nverts) lcur = 0;

                lnext = lcur + 1;
                if (lnext >= nverts) lnext = 0;

                //lstep = ((scrcoords[lnext][0] - scrcoords[lcur][0]) << 12) / (scrcoords[lnext][1] - scrcoords[lcur][1]);
                //xmin = scrcoords[lcur][0] << 12;
                if (lcur == vbot) break;
            }

            while (y == scrcoords[rnext][1])
            {
                rcur--;
                if (rcur < 0) rcur = nverts - 1;

                rnext = rcur - 1;
                if (rnext < 0) rnext = nverts - 1;

                //rstep = ((scrcoords[rnext][0] - scrcoords[rcur][0]) << 12) / (scrcoords[rnext][1] - scrcoords[rcur][1]);
                //xmax = scrcoords[rcur][0] << 12;
                if (rcur == vbot) break;
            }
        }

        Vertex* vlcur = polygon->Vertices[lcur];
        Vertex* vlnext = polygon->Vertices[lnext];
        Vertex* vrcur = polygon->Vertices[rcur];
        Vertex* vrnext = polygon->Vertices[rnext];

        s32 lfactor, rfactor;

        if (scrcoords[lnext][1] == scrcoords[lcur][1])
            lfactor = 0;
        else
            lfactor = ((y - scrcoords[lcur][1]) << 12) / (scrcoords[lnext][1] - scrcoords[lcur][1]);

        if (scrcoords[rnext][1] == scrcoords[rcur][1])
            rfactor = 0;
        else
            rfactor = ((y - scrcoords[rcur][1]) << 12) / (scrcoords[rnext][1] - scrcoords[rcur][1]);

        s32 xl = scrcoords[lcur][0] + (((scrcoords[lnext][0] - scrcoords[lcur][0]) * lfactor) >> 12);
        s32 xr = scrcoords[rcur][0] + (((scrcoords[rnext][0] - scrcoords[rcur][0]) * rfactor) >> 12);

        if (xl<0 || xr>255) continue; // hax

        //if (vlcur->Color[0]==0 && vlcur->Color[1]==63 && vlcur->Color[2]==0)
            /*printf("y:%d  xleft:%d  xright:%d   %d,%d  %d,%d   |   left: %d to %d   right: %d to %d\n",
                   y, xl, xr, lcur, rcur, vtop, vbot,
                   scrcoords[lcur][0], scrcoords[lnext][0],
                   scrcoords[rcur][0], scrcoords[rnext][0]);*/

        //s32 zl = scrcoords[lcur][2] + (((scrcoords[lnext][2] - scrcoords[lcur][2]) * lfactor) >> 12);
        //s32 zr = scrcoords[rcur][2] + (((scrcoords[rnext][2] - scrcoords[rcur][2]) * rfactor) >> 12);

        u8 rl = vlcur->Color[0] + (((vlnext->Color[0] - vlcur->Color[0]) * lfactor) >> 12);
        u8 gl = vlcur->Color[1] + (((vlnext->Color[1] - vlcur->Color[1]) * lfactor) >> 12);
        u8 bl = vlcur->Color[2] + (((vlnext->Color[2] - vlcur->Color[2]) * lfactor) >> 12);

        u8 rr = vrcur->Color[0] + (((vrnext->Color[0] - vrcur->Color[0]) * rfactor) >> 12);
        u8 gr = vrcur->Color[1] + (((vrnext->Color[1] - vrcur->Color[1]) * rfactor) >> 12);
        u8 br = vrcur->Color[2] + (((vrnext->Color[2] - vrcur->Color[2]) * rfactor) >> 12);

        s32 xdiv;
        if (xr == xl)
            xdiv = 0;
        else
            xdiv = 0x1000 / (xr - xl);

        for (s32 x = xl; x <= xr; x++)
        {
            s32 xfactor = (x - xl) * xdiv;

            //s32 z = (((zr - zl) * xfactor) >> 12);
            //if (zr!=zl) z = (z << 12) / (zr - zl);

            //s32 z_inv = ((z>>12)==0) ? 0x1000 : 0x1000000 / (z >> 12);
            //xfactor = (xfactor * z_inv) >> 12;
            //if (z) xfactor = (xfactor << 12) / z;

            // TODO: get rid of this shit
            if (x<0 || x>255 || y<0 || y>191)
            {
                //printf("BAD COORDS!! %d %d\n", x, y);
                x = 0; y = 0;
            }

            u8* pixel = &ColorBuffer[((256*y) + x) * 4];
            pixel[0] = rl + (((rr - rl) * xfactor) >> 12);
            pixel[1] = gl + (((gr - gl) * xfactor) >> 12);
            pixel[2] = bl + (((br - bl) * xfactor) >> 12);
            pixel[3] = 31; // TODO: alpha

            // Z debug
            /*u8 zerp = (z * 63) / 0xFFFFFF;
            pixel[0] = zerp;
            pixel[1] = zerp;
            pixel[2] = zerp;*/
        }
    }
}

void RenderFrame(Vertex* vertices, Polygon* polygons, int npolys)
{
    // TODO: render translucent polygons last

    for (int i = 0; i < 256*192; i++)
    {
        ((u32*)ColorBuffer)[i] = 0x00000000;
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
        RenderPolygon(&polygons[i]);
    }
}

u8* GetLine(int line)
{
    return &ColorBuffer[line * 256 * 4];
}

}
}
