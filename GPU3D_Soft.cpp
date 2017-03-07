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

u32 ColorBuffer[256*192];
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

    // texture wrapping
    // TODO: optimize this somehow

    if (texparam & (1<<16))
    {
        if (texparam & (1<<18))
        {
            if (s & width) s = (width-1) - (s & (width-1));
            else           s = (s & (width-1));
        }
        else
            s &= width-1;
    }
    else
    {
        if (s < 0) s = 0;
        else if (s >= width) s = width-1;
    }

    if (texparam & (1<<17))
    {
        if (texparam & (1<<19))
        {
            if (t & height) t = (height-1) - (t & (height-1));
            else            t = (t & (height-1));
        }
        else
            t &= height-1;
    }
    else
    {
        if (t < 0) t = 0;
        else if (t >= height) t = height-1;
    }

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

    case 7: // direct color
        {
            vramaddr += (((t * width) + s) << 1);
            u16 color = GPU::ReadVRAM_Texture<u16>(vramaddr);

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

    u32* color = &ColorBuffer[(256*y) + x];
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

    u32 a = 31; // TODO

    *color = r | (g << 8) | (b << 16) | (a << 24);

    // TODO: optional update for translucent pixels
    if (z > 0xFFFFFF) z = 0xFFFFFF;
    *depth = z;
}

void RenderPolygon(Polygon* polygon, u32 wbuffer)
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
                posX = ((s64)vtx->Position[0] << 12) / w;
                posY = ((s64)vtx->Position[1] << 12) / w;

                if (wbuffer) posZ = w;
                else         posZ = (((s64)vtx->Position[2] * 0x800000) / w) + 0x7FFEFF;
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
            vtx->FinalPosition[3] = w;

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

        // TODO: the calculations can be simplified

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

        s64 perspfactorl1 = ((s64)(0x1000 - lfactor) * vlnext->FinalPosition[3]) >> 12;
        s64 perspfactorl2 = ((s64)lfactor            * vlcur->FinalPosition[3]) >> 12;
        s64 perspfactorr1 = ((s64)(0x1000 - rfactor) * vrnext->FinalPosition[3]) >> 12;
        s64 perspfactorr2 = ((s64)rfactor            * vrcur->FinalPosition[3]) >> 12;

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

        s32 zl = ((perspfactorl1 * vlcur->FinalPosition[2]) + (perspfactorl2 * vlnext->FinalPosition[2])) / (perspfactorl1 + perspfactorl2);
        s32 zr = ((perspfactorr1 * vrcur->FinalPosition[2]) + (perspfactorr2 * vrnext->FinalPosition[2])) / (perspfactorr1 + perspfactorr2);

        s32 wl = ((perspfactorl1 * vlcur->FinalPosition[3]) + (perspfactorl2 * vlnext->FinalPosition[3])) / (perspfactorl1 + perspfactorl2);
        s32 wr = ((perspfactorr1 * vrcur->FinalPosition[3]) + (perspfactorr2 * vrnext->FinalPosition[3])) / (perspfactorr1 + perspfactorr2);

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

        // temp.
        if (xl > 255) continue;

        //s32 xdiv = 0x1000 / (xr - xl);
        //s32 xdiv = 0x100000 / (xr - xl);

        for (s32 x = xl; x < xr; x++)
        {
            //if (x!=xl && x!=(xr-1)) continue;
            s32 xfactor = ((x - xl) << 12) / (xr - xl);
            //s32 xfactor = (x - xl) * xdiv;
            //s32 xfactor = ((x - xl) * xdiv) >> 8;


            s32 perspfactor1 = ((s64)(0x1000 - xfactor) * wr) >> 12;
            s32 perspfactor2 = ((s64)xfactor * wl) >> 12;

            if (perspfactor1 + perspfactor2 == 0)
            {
                perspfactor1 = 0x1000;
                perspfactor2 = 0;
            }

            s32 z = ((perspfactor1 * (s64)zl) + (perspfactor2 * (s64)zr)) / (perspfactor1 + perspfactor2);

            // possible optimization: only do color interpolation if the depth test passes
            u32 vr = ((perspfactor1 * rl) + (perspfactor2 * rr)) / (perspfactor1 + perspfactor2);
            u32 vg = ((perspfactor1 * gl) + (perspfactor2 * gr)) / (perspfactor1 + perspfactor2);
            u32 vb = ((perspfactor1 * bl) + (perspfactor2 * br)) / (perspfactor1 + perspfactor2);

            s16 s = ((perspfactor1 * (s64)sl) + (perspfactor2 * (s64)sr)) / (perspfactor1 + perspfactor2);
            s16 t = ((perspfactor1 * (s64)tl) + (perspfactor2 * (s64)tr)) / (perspfactor1 + perspfactor2);

            RenderPixel(polygon, x, y, z, vr>>3, vg>>3, vb>>3, s, t);
        }
    }
}

void RenderFrame(u32 attr, Vertex* vertices, Polygon* polygons, int npolys)
{
    // TODO: render translucent polygons last

    // TODO: fog, poly ID, other attributes

    if (DispCnt & (1<<14))
    {
        u8 xoff = (ClearAttr2 >> 16) & 0xFF;
        u8 yoff = (ClearAttr2 >> 24) & 0xFF;

        for (int y = 0; y < 256*192; y += 256)
        {
            for (int x = 0; x < 256; x++)
            {
                u16 val2 = GPU::ReadVRAM_Texture<u16>(0x40000 + (yoff << 9) + (xoff << 1));
                u16 val3 = GPU::ReadVRAM_Texture<u16>(0x60000 + (yoff << 9) + (xoff << 1));

                // TODO: confirm color conversion
                u32 r = (val2 << 1) & 0x3E; if (r) r++;
                u32 g = (val2 >> 4) & 0x3E; if (g) g++;
                u32 b = (val2 >> 9) & 0x3E; if (b) b++;
                u32 a = (val2 & 0x8000) ? 0x1F000000 : 0;
                u32 color = r | (g << 8) | (b << 16) | a;

                u32 z = ((val3 & 0x7FFF) * 0x200) + 0x1FF;
                if (z >= 0x10000 && z < 0xFFFFFF) z++;

                ColorBuffer[y+x] = color;
                DepthBuffer[y+x] = z;

                xoff++;
            }

            yoff++;
        }
    }
    else
    {
        // TODO: confirm color conversion
        u32 r = (ClearAttr1 << 1) & 0x3E; if (r) r++;
        u32 g = (ClearAttr1 >> 4) & 0x3E; if (g) g++;
        u32 b = (ClearAttr1 >> 9) & 0x3E; if (b) b++;
        u32 a = (ClearAttr1 >> 16) & 0x1F;
        u32 color = r | (g << 8) | (b << 16) | (a << 24);

        u32 z = ((ClearAttr2 & 0x7FFF) * 0x200) + 0x1FF;
		if (z >= 0x10000 && z < 0xFFFFFF) z++;

        for (int i = 0; i < 256*192; i++)
        {
            ColorBuffer[i] = color;
            DepthBuffer[i] = z;
        }
    }

    for (int i = 0; i < npolys; i++)
    {
        RenderPolygon(&polygons[i], attr&0x2);
    }
}

u32* GetLine(int line)
{
    return &ColorBuffer[line * 256];
}

}
}
