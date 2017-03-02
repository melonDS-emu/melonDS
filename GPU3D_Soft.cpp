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


void TextureLookup(u32 texparam, u32 texpal, s16 s, s16 t, u8* r, u8* g, u8* b)
{
    u32 vramaddr = (texparam & 0xFFFF) << 3;

    u32 width = 8 << ((texparam >> 20) & 0x7);
    u32 height = 8 << ((texparam >> 23) & 0x7);

    s >>= 4;
    t >>= 4;

    // TODO: wraparound modes
    s &= width-1;
    t &= height-1;

    switch ((texparam >> 26) & 0x7)
    {
    case 3: // 16-color
        {
            vramaddr += (((t * width) + s) >> 1);
            u8 pixel = GPU::ReadVRAM_Texture<u8>(vramaddr);
            if (s & 0x1) pixel >>= 4;
            else         pixel &= 0xF;

            texpal <<= 4;
            u16 color = GPU::ReadVRAM_TexPal<u16>(texpal + (pixel<<1));

            *r = (color << 1) & 0x3E; if (*r) *r++;
            *g = (color >> 4) & 0x3E; if (*g) *g++;
            *b = (color >> 9) & 0x3E; if (*b) *b++;
        }
        break;

    case 4: // 256-color
        {
            vramaddr += ((t * width) + s);
            u8 pixel = GPU::ReadVRAM_Texture<u8>(vramaddr);

            texpal <<= 4;
            u16 color = GPU::ReadVRAM_TexPal<u16>(texpal + (pixel<<1));

            *r = (color << 1) & 0x3E; if (*r) *r++;
            *g = (color >> 4) & 0x3E; if (*g) *g++;
            *b = (color >> 9) & 0x3E; if (*b) *b++;
        }
        break;

    case 5: // compressed
        {
            vramaddr += ((t & 0x3FC) * (width>>2)) + (s & 0x3FC);
            vramaddr += (t & 0x3);

            u32 slot1addr = 0x20000 + ((vramaddr & 0x1FFFC) >> 1);
            if (vramaddr >= 0x40000)
                slot1addr += 0x10000;

            u8 val = GPU::ReadVRAM_Texture<u8>(vramaddr);
            val >>= (2 * (s & 0x3));

            u16 palinfo = GPU::ReadVRAM_Texture<u16>(slot1addr);
            u32 paloffset = (palinfo & 0x3FFF) << 2;
            texpal <<= 4;

            u16 color;
            switch (val & 0x3)
            {
            case 0:
                color = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset);
                break;

            case 1:
                color = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset + 2);
                break;

            case 2:
                if ((palinfo >> 14) == 1)
                {
                    u16 color0 = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset);
                    u16 color1 = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset + 2);

                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    u32 r = (r0 + r1) >> 1;
                    u32 g = ((g0 + g1) >> 1) & 0x03E0;
                    u32 b = ((b0 + b1) >> 1) & 0x7C00;

                    color = r | g | b;
                }
                else if ((palinfo >> 14) == 3)
                {
                    u16 color0 = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset);
                    u16 color1 = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset + 2);

                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    u32 r = (r0*5 + r1*3) >> 3;
                    u32 g = ((g0*5 + g1*3) >> 3) & 0x03E0;
                    u32 b = ((b0*5 + b1*3) >> 3) & 0x7C00;

                    color = r | g | b;
                }
                else
                    color = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset + 4);
                break;

            case 3:
                if ((palinfo >> 14) == 2)
                    color = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset + 6);
                else if ((palinfo >> 14) == 3)
                {
                    u16 color0 = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset);
                    u16 color1 = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset + 2);

                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    u32 r = (r0*3 + r1*5) >> 3;
                    u32 g = ((g0*3 + g1*5) >> 3) & 0x03E0;
                    u32 b = ((b0*3 + b1*5) >> 3) & 0x7C00;

                    color = r | g | b;
                }
                else
                    color = 0; // TODO transparent!
                break;
            }

            *r = (color << 1) & 0x3E; if (*r) *r++;
            *g = (color >> 4) & 0x3E; if (*g) *g++;
            *b = (color >> 9) & 0x3E; if (*b) *b++;
        }
        break;

    default:
        *r = (s)&0x3F;
        *g = 0;
        *b = (t)&0x3F;
        break;
    }
}

void RenderPixel(Polygon* polygon, s32 x, s32 y, s32 z, u8 vr, u8 vg, u8 vb, s16 s, s16 t)
{
    u32 attr = polygon->Attr;

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

    u8 r, g, b;

    if (((polygon->TexParam >> 26) & 0x7) != 0)
    {
        // TODO: also take DISP3DCNT into account

        u8 tr, tg, tb;
        TextureLookup(polygon->TexParam, polygon->TexPalette, s, t, &tr, &tg, &tb);

        // TODO: other blending modes
        /*r = ((tr+1) * (vr+1) - 1) >> 6;
        g = ((tg+1) * (vg+1) - 1) >> 6;
        b = ((tb+1) * (vb+1) - 1) >> 6;*/
        r = tr;
        g = tg;
        b = tb;
    }
    else
    {
        r = vr;
        g = vg;
        b = vb;
    }

    u8* pixel = &ColorBuffer[((256*y) + x) * 4];
    pixel[0] = r;
    pixel[1] = g;
    pixel[2] = b;
    pixel[3] = 31; // TODO: alpha

    // TODO: optional update for translucent pixels
    if (z > 0xFFFFFF) z = 0xFFFFFF;
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

            vtx->FinalColor[0] = vtx->Color[0] >> 12;
            if (vtx->FinalColor[0]) vtx->FinalColor[0] = ((vtx->FinalColor[0] << 4) + 0xF);
            vtx->FinalColor[1] = vtx->Color[1] >> 12;
            if (vtx->FinalColor[1]) vtx->FinalColor[1] = ((vtx->FinalColor[1] << 4) + 0xF);
            vtx->FinalColor[2] = vtx->Color[2] >> 12;
            if (vtx->FinalColor[2]) vtx->FinalColor[2] = ((vtx->FinalColor[2] << 4) + 0xF);

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

        //printf("v%d: %d,%d\n", i, vtx->FinalPosition[0], vtx->FinalPosition[1]);
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

        // TODO: work out the actual division bias there. 0x400 was found to make things look good.
        // but actually, it isn't right. so what's going on there?
        // seems vertical slopes are interpolated starting from the bottom and not the top. maybe.
        // also seems lfactor/rfactor are rounded

        if (vlnext->FinalPosition[1] == vlcur->FinalPosition[1])
            lfactor = 0;
        else
            lfactor = (((y+1 - vlcur->FinalPosition[1]) << 12) + 0x00) / (vlnext->FinalPosition[1] - vlcur->FinalPosition[1]);

        if (vrnext->FinalPosition[1] == vrcur->FinalPosition[1])
            rfactor = 0;
        else
            rfactor = (((y+1 - vrcur->FinalPosition[1]) << 12) + 0x00) / (vrnext->FinalPosition[1] - vrcur->FinalPosition[1]);

        s32 xl = vlcur->FinalPosition[0] + ((((vlnext->FinalPosition[0] - vlcur->FinalPosition[0]) * lfactor) + 0x800) >> 12);
        s32 xr = vrcur->FinalPosition[0] + ((((vrnext->FinalPosition[0] - vrcur->FinalPosition[0]) * rfactor) + 0x800) >> 12);
//printf("y:%d xl:%d xr:%d    %08X\n", y, xl, xr, rfactor); // y: 48 143
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

        s32 zl = vlcur->FinalPosition[2] + (((s64)(vlnext->FinalPosition[2] - vlcur->FinalPosition[2]) * lfactor) >> 12);
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

        s32 sl = ((perspfactorl1 * vlcur->TexCoords[0]) + (perspfactorl2 * vlnext->TexCoords[0])) / (perspfactorl1 + perspfactorl2);
        s32 tl = ((perspfactorl1 * vlcur->TexCoords[1]) + (perspfactorl2 * vlnext->TexCoords[1])) / (perspfactorl1 + perspfactorl2);

        s32 rr = ((perspfactorr1 * vrcur->FinalColor[0]) + (perspfactorr2 * vrnext->FinalColor[0])) / (perspfactorr1 + perspfactorr2);
        s32 gr = ((perspfactorr1 * vrcur->FinalColor[1]) + (perspfactorr2 * vrnext->FinalColor[1])) / (perspfactorr1 + perspfactorr2);
        s32 br = ((perspfactorr1 * vrcur->FinalColor[2]) + (perspfactorr2 * vrnext->FinalColor[2])) / (perspfactorr1 + perspfactorr2);

        s32 sr = ((perspfactorr1 * vrcur->TexCoords[0]) + (perspfactorr2 * vrnext->TexCoords[0])) / (perspfactorr1 + perspfactorr2);
        s32 tr = ((perspfactorr1 * vrcur->TexCoords[1]) + (perspfactorr2 * vrnext->TexCoords[1])) / (perspfactorr1 + perspfactorr2);

        if (xr == xl) xr++;
        s32 xdiv = 0x1000 / (xr - xl);

        //printf("y%d: %d->%d   %08X %08X\n", y, xl, xr, lfactor, rfactor);

        for (s32 x = xl; x < xr; x++)
        {
            //s32 xfactor = ((x - xl) << 12) / (xr - xl);
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

            //z = 0x1000000 / (perspfactor1 + perspfactor2);

            // possible optimization: only do color interpolation if the depth test passes
            u32 vr = ((perspfactor1 * rl) + (perspfactor2 * rr)) / (perspfactor1 + perspfactor2);
            u32 vg = ((perspfactor1 * gl) + (perspfactor2 * gr)) / (perspfactor1 + perspfactor2);
            u32 vb = ((perspfactor1 * bl) + (perspfactor2 * br)) / (perspfactor1 + perspfactor2);

            s16 s = ((perspfactor1 * sl) + (perspfactor2 * sr)) / (perspfactor1 + perspfactor2);
            s16 t = ((perspfactor1 * tl) + (perspfactor2 * tr)) / (perspfactor1 + perspfactor2);

            RenderPixel(polygon, x, y, z, vr>>3, vg>>3, vb>>3, s, t);
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
