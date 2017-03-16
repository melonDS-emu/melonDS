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
u32 AttrBuffer[256*192];

// attribute buffer:
// bit0-5: polygon ID
// bit8: fog enable


bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    memset(ColorBuffer, 0, 256*192 * 4);
    memset(DepthBuffer, 0, 256*192 * 4);
    memset(AttrBuffer, 0, 256*192 * 4);
}


void TextureLookup(u32 texparam, u32 texpal, s16 s, s16 t, u16* color, u8* alpha)
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

    u8 alpha0;
    if (texparam & (1<<29)) alpha0 = 0;
    else                    alpha0 = 31;

    switch ((texparam >> 26) & 0x7)
    {
    case 1: // A3I5
        {
            vramaddr += ((t * width) + s);
            u8 pixel = GPU::ReadVRAM_Texture<u8>(vramaddr);

            texpal <<= 4;
            *color = GPU::ReadVRAM_TexPal<u16>(texpal + ((pixel&0x1F)<<1));
            *alpha = ((pixel >> 3) & 0x1C) + (pixel >> 6);
        }
        break;

    case 2: // 4-color
        {
            vramaddr += (((t * width) + s) >> 2);
            u8 pixel = GPU::ReadVRAM_Texture<u8>(vramaddr);
            pixel >>= ((s & 0x3) << 1);
            pixel &= 0x3;

            texpal <<= 3;
            *color = GPU::ReadVRAM_TexPal<u16>(texpal + (pixel<<1));
            *alpha = (pixel==0) ? alpha0 : 31;
        }
        break;

    case 3: // 16-color
        {
            vramaddr += (((t * width) + s) >> 1);
            u8 pixel = GPU::ReadVRAM_Texture<u8>(vramaddr);
            if (s & 0x1) pixel >>= 4;
            else         pixel &= 0xF;

            texpal <<= 4;
            *color = GPU::ReadVRAM_TexPal<u16>(texpal + (pixel<<1));
            *alpha = (pixel==0) ? alpha0 : 31;
        }
        break;

    case 4: // 256-color
        {
            vramaddr += ((t * width) + s);
            u8 pixel = GPU::ReadVRAM_Texture<u8>(vramaddr);

            texpal <<= 4;
            *color = GPU::ReadVRAM_TexPal<u16>(texpal + (pixel<<1));
            *alpha = (pixel==0) ? alpha0 : 31;
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

            switch (val & 0x3)
            {
            case 0:
                *color = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset);
                *alpha = 31;
                break;

            case 1:
                *color = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset + 2);
                *alpha = 31;
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

                    *color = r | g | b;
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

                    *color = r | g | b;
                }
                else
                    *color = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset + 4);
                *alpha = 31;
                break;

            case 3:
                if ((palinfo >> 14) == 2)
                {
                    *color = GPU::ReadVRAM_TexPal<u16>(texpal + paloffset + 6);
                    *alpha = 31;
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

                    u32 r = (r0*3 + r1*5) >> 3;
                    u32 g = ((g0*3 + g1*5) >> 3) & 0x03E0;
                    u32 b = ((b0*3 + b1*5) >> 3) & 0x7C00;

                    *color = r | g | b;
                    *alpha = 31;
                }
                else
                {
                    *color = 0;
                    *alpha = 0;
                }
                break;
            }
        }
        break;

    case 6: // A5I3
        {
            vramaddr += ((t * width) + s);
            u8 pixel = GPU::ReadVRAM_Texture<u8>(vramaddr);

            texpal <<= 4;
            *color = GPU::ReadVRAM_TexPal<u16>(texpal + ((pixel&0x7)<<1));
            *alpha = (pixel >> 3);
        }
        break;

    case 7: // direct color
        {
            vramaddr += (((t * width) + s) << 1);
            *color = GPU::ReadVRAM_Texture<u16>(vramaddr);
            *alpha = (*color & 0x8000) ? 31 : 0;
        }
        break;
    }
}

bool DepthTest(Polygon* polygon, s32 x, s32 y, s32 z)
{
    u32 oldz = DepthBuffer[(256*y) + x];

    if (polygon->Attr & (1<<14))
    {
        s32 diff = oldz - z;
        if ((u32)(diff + 0x200) <= 0x400)
            return true;
    }
    else
    if (z < oldz)
        return true;

    return false;
}

u32 RenderPixel(Polygon* polygon, s32 x, s32 y, s32 z, u8 vr, u8 vg, u8 vb, s16 s, s16 t)
{
    u32 attr = polygon->Attr;
    u8 r, g, b, a;

    u32 polyalpha = (polygon->Attr >> 16) & 0x1F;
    bool wireframe = (polyalpha == 0);

    if ((DispCnt & (1<<0)) && (((polygon->TexParam >> 26) & 0x7) != 0))
    {
        u8 tr, tg, tb;

        u16 tcolor; u8 talpha;
        TextureLookup(polygon->TexParam, polygon->TexPalette, s, t, &tcolor, &talpha);

        tr = (tcolor << 1) & 0x3E; if (tr) tr++;
        tg = (tcolor >> 4) & 0x3E; if (tg) tg++;
        tb = (tcolor >> 9) & 0x3E; if (tb) tb++;

        // TODO: other blending modes
        r = ((tr+1) * (vr+1) - 1) >> 6;
        g = ((tg+1) * (vg+1) - 1) >> 6;
        b = ((tb+1) * (vb+1) - 1) >> 6;
        a = ((talpha+1) * (polyalpha+1) - 1) >> 5;
    }
    else
    {
        r = vr;
        g = vg;
        b = vb;
        a = polyalpha;
    }

    if (wireframe) a = 31;

    return r | (g << 8) | (b << 16) | (a << 24);
}

void RenderPolygon(Polygon* polygon)
{
    int nverts = polygon->NumVertices;
    bool isline = false;

    int vtop = polygon->VTop, vbot = polygon->VBottom;
    s32 ytop = polygon->YTop, ybot = polygon->YBottom;
    s32 xtop = polygon->XTop, xbot = polygon->XBottom;

    if (ytop > 191) return;

    // draw, line per line

    u32 polyalpha = (polygon->Attr >> 16) & 0x1F;
    bool wireframe = (polyalpha == 0);

    int lcur = vtop, rcur = vtop;
    int lnext, rnext;

    s32 dxl, dxr;
    s32 lslope, rslope;
    bool l_xmajor, r_xmajor;

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

        lslope = 0; l_xmajor = false;
        rslope = 0; r_xmajor = false;
    }
    else
    {
        //while (polygon->Vertices[lnext]->FinalPosition[1] )
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

        if (polygon->Vertices[lnext]->FinalPosition[1] == polygon->Vertices[lcur]->FinalPosition[1])
            lslope = 0;
        else
            lslope = ((polygon->Vertices[lnext]->FinalPosition[0] - polygon->Vertices[lcur]->FinalPosition[0]) << 12) /
                      (polygon->Vertices[lnext]->FinalPosition[1] - polygon->Vertices[lcur]->FinalPosition[1]);

        if (polygon->Vertices[rnext]->FinalPosition[1] == polygon->Vertices[rcur]->FinalPosition[1])
            rslope = 0;
        else
            rslope = ((polygon->Vertices[rnext]->FinalPosition[0] - polygon->Vertices[rcur]->FinalPosition[0]) << 12) /
                      (polygon->Vertices[rnext]->FinalPosition[1] - polygon->Vertices[rcur]->FinalPosition[1]);

        l_xmajor = (lslope < -0x1000) || (lslope > 0x1000);
        r_xmajor = (rslope < -0x1000) || (rslope > 0x1000);
    }

    if (l_xmajor)    dxl = (lslope > 0) ? 0x800 : (-lslope-0x800)+0x1000;
    else if (lslope) dxl = (lslope > 0) ? 0     : 0x1000;
    else             dxl = 0;

    if (r_xmajor)    dxr = (rslope > 0) ? rslope-0x800 : 0x800+0x1000;
    else if (rslope) dxr = (rslope > 0) ? 0            : 0x1000;
    else             dxr = 0x1000;

    if (ybot > 192) ybot = 192;
    for (s32 y = ytop; y < ybot; y++)
    {
        if (!isline)
        {
            if (y >= polygon->Vertices[lnext]->FinalPosition[1] && lcur != vbot)
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

                if (polygon->Vertices[lnext]->FinalPosition[1] == polygon->Vertices[lcur]->FinalPosition[1])
                    lslope = 0;
                else
                    lslope = ((polygon->Vertices[lnext]->FinalPosition[0] - polygon->Vertices[lcur]->FinalPosition[0]) << 12) /
                              (polygon->Vertices[lnext]->FinalPosition[1] - polygon->Vertices[lcur]->FinalPosition[1]);

                l_xmajor = (lslope < -0x1000) || (lslope > 0x1000);

                if (l_xmajor)    dxl = (lslope > 0) ? 0x800 : (-lslope-0x800)+0x1000;
                else if (lslope) dxl = (lslope > 0) ? 0     : 0x1000;
                else             dxl = 0;
            }

            if (y >= polygon->Vertices[rnext]->FinalPosition[1] && rcur != vbot)
            {
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

                if (polygon->Vertices[rnext]->FinalPosition[1] == polygon->Vertices[rcur]->FinalPosition[1])
                    rslope = 0;
                else
                    rslope = ((polygon->Vertices[rnext]->FinalPosition[0] - polygon->Vertices[rcur]->FinalPosition[0]) << 12) /
                              (polygon->Vertices[rnext]->FinalPosition[1] - polygon->Vertices[rcur]->FinalPosition[1]);

                r_xmajor = (rslope < -0x1000) || (rslope > 0x1000);

                if (r_xmajor)    dxr = (rslope > 0) ? rslope-0x800 : 0x800+0x1000;
                else if (rslope) dxr = (rslope > 0) ? 0            : 0x1000;
                else             dxr = 0x1000;
            }
        }

        Vertex *vlcur, *vlnext, *vrcur, *vrnext;
        s32 xstart, xend;
        s32 xstart_int, xend_int;
        s32 slope_start, slope_end;

        if (lslope == 0 && rslope == 0 &&
            polygon->Vertices[lcur]->FinalPosition[0] == polygon->Vertices[rcur]->FinalPosition[0])
        {
            xstart = polygon->Vertices[lcur]->FinalPosition[0];
            xend = xstart;
        }
        else
        {
            if (lslope > 0)
            {
                xstart = polygon->Vertices[lcur]->FinalPosition[0] + (dxl >> 12);
                if (xstart < polygon->Vertices[lcur]->FinalPosition[0])
                    xstart = polygon->Vertices[lcur]->FinalPosition[0];
                else if (xstart > polygon->Vertices[lnext]->FinalPosition[0]-1)
                    xstart = polygon->Vertices[lnext]->FinalPosition[0]-1;
            }
            else if (lslope < 0)
            {
                xstart = polygon->Vertices[lcur]->FinalPosition[0] - (dxl >> 12);
                if (xstart < polygon->Vertices[lnext]->FinalPosition[0])
                    xstart = polygon->Vertices[lnext]->FinalPosition[0];
                else if (xstart > polygon->Vertices[lcur]->FinalPosition[0]-1)
                    xstart = polygon->Vertices[lcur]->FinalPosition[0]-1;
            }
            else
                xstart = polygon->Vertices[lcur]->FinalPosition[0];

            if (rslope > 0)
            {
                xend = polygon->Vertices[rcur]->FinalPosition[0] + (dxr >> 12);
                if (xend < polygon->Vertices[rcur]->FinalPosition[0])
                    xend = polygon->Vertices[rcur]->FinalPosition[0];
                else if (xend > polygon->Vertices[rnext]->FinalPosition[0]-1)
                    xend = polygon->Vertices[rnext]->FinalPosition[0]-1;
            }
            else if (rslope < 0)
            {
                xend = polygon->Vertices[rcur]->FinalPosition[0] - (dxr >> 12);
                if (xend < polygon->Vertices[rnext]->FinalPosition[0])
                    xend = polygon->Vertices[rnext]->FinalPosition[0];
                else if (xend > polygon->Vertices[rcur]->FinalPosition[0]-1)
                    xend = polygon->Vertices[rcur]->FinalPosition[0]-1;
            }
            else
                xend = polygon->Vertices[rcur]->FinalPosition[0] - 1;
        }

        // if the left and right edges are swapped, render backwards.
        // note: we 'forget' to swap the xmajor flags, on purpose
        // the hardware has the same bug
        if (xstart > xend)
        {
            vlcur = polygon->Vertices[rcur];
            vlnext = polygon->Vertices[rnext];
            vrcur = polygon->Vertices[lcur];
            vrnext = polygon->Vertices[lnext];

            slope_start = rslope;
            slope_end = lslope;

            s32 tmp = xstart; xstart = xend; xend = tmp;
        }
        else
        {
            vlcur = polygon->Vertices[lcur];
            vlnext = polygon->Vertices[lnext];
            vrcur = polygon->Vertices[rcur];
            vrnext = polygon->Vertices[rnext];

            slope_start = lslope;
            slope_end = rslope;
        }

        // interpolate attributes along Y
        s64 lfactor1, lfactor2;
        s64 rfactor1, rfactor2;

        if (l_xmajor)
        {
            lfactor1 = (vlnext->FinalPosition[0] - xstart) * vlnext->FinalPosition[3];
            lfactor2 = (xstart - vlcur->FinalPosition[0]) * vlcur->FinalPosition[3];
        }
        else
        {
            lfactor1 = (vlnext->FinalPosition[1] - y) * vlnext->FinalPosition[3];
            lfactor2 = (y - vlcur->FinalPosition[1]) * vlcur->FinalPosition[3];
        }

        s64 ldenom = lfactor1 + lfactor2;
        if (ldenom == 0)
        {
            lfactor1 = 0x1000;
            lfactor2 = 0;
            ldenom = 0x1000;
        }

        if (r_xmajor)
        {
            rfactor1 = (vrnext->FinalPosition[0] - xend+1) * vrnext->FinalPosition[3];
            rfactor2 = (xend+1 - vrcur->FinalPosition[0]) * vrcur->FinalPosition[3];
        }
        else
        {
            rfactor1 = (vrnext->FinalPosition[1] - y) * vrnext->FinalPosition[3];
            rfactor2 = (y - vrcur->FinalPosition[1]) * vrcur->FinalPosition[3];
        }

        s64 rdenom = rfactor1 + rfactor2;
        if (rdenom == 0)
        {
            rfactor1 = 0x1000;
            rfactor2 = 0;
            rdenom = 0x1000;
        }

        s32 zl = ((lfactor1 * vlcur->FinalPosition[2]) + (lfactor2 * vlnext->FinalPosition[2])) / ldenom;
        s32 zr = ((rfactor1 * vrcur->FinalPosition[2]) + (rfactor2 * vrnext->FinalPosition[2])) / rdenom;

        s32 wl = ((lfactor1 * vlcur->FinalPosition[3]) + (lfactor2 * vlnext->FinalPosition[3])) / ldenom;
        s32 wr = ((rfactor1 * vrcur->FinalPosition[3]) + (rfactor2 * vrnext->FinalPosition[3])) / rdenom;

        s32 rl = ((lfactor1 * vlcur->FinalColor[0]) + (lfactor2 * vlnext->FinalColor[0])) / ldenom;
        s32 gl = ((lfactor1 * vlcur->FinalColor[1]) + (lfactor2 * vlnext->FinalColor[1])) / ldenom;
        s32 bl = ((lfactor1 * vlcur->FinalColor[2]) + (lfactor2 * vlnext->FinalColor[2])) / ldenom;

        s32 sl = ((lfactor1 * vlcur->TexCoords[0]) + (lfactor2 * vlnext->TexCoords[0])) / ldenom;
        s32 tl = ((lfactor1 * vlcur->TexCoords[1]) + (lfactor2 * vlnext->TexCoords[1])) / ldenom;

        s32 rr = ((rfactor1 * vrcur->FinalColor[0]) + (rfactor2 * vrnext->FinalColor[0])) / rdenom;
        s32 gr = ((rfactor1 * vrcur->FinalColor[1]) + (rfactor2 * vrnext->FinalColor[1])) / rdenom;
        s32 br = ((rfactor1 * vrcur->FinalColor[2]) + (rfactor2 * vrnext->FinalColor[2])) / rdenom;

        s32 sr = ((rfactor1 * vrcur->TexCoords[0]) + (rfactor2 * vrnext->TexCoords[0])) / rdenom;
        s32 tr = ((rfactor1 * vrcur->TexCoords[1]) + (rfactor2 * vrnext->TexCoords[1])) / rdenom;

        // calculate edges
        s32 l_edgeend, r_edgestart;

        if (l_xmajor)
        {
            if (slope_start > 0) l_edgeend = vlcur->FinalPosition[0] + ((dxl + slope_start) >> 12);
            else                 l_edgeend = vlcur->FinalPosition[0] - ((dxl - slope_start) >> 12);

            if (l_edgeend == xstart) l_edgeend++;
        }
        else
            l_edgeend = xstart + 1;

        if (r_xmajor)
        {
            if (slope_end > 0) r_edgestart = vrcur->FinalPosition[0] + ((dxr + slope_end) >> 12);
            else               r_edgestart = vrcur->FinalPosition[0] - ((dxr - slope_end) >> 12);

            if (r_edgestart == xend_int) r_edgestart--;
        }
        else
            r_edgestart = xend - 1;

        // edge fill rules for opaque pixels:
        // * right edge is filled if slope > 1
        // * left edge is filled if slope <= 1
        // * edges with slope = 0 are always filled
        // edges are always filled if the pixels are translucent
        // in wireframe mode, there are special rules for equal Z (TODO)

        for (s32 x = xstart; x <= xend; x++)
        {
            if (x < 0) continue;
            if (x > 255) break;

            int edge = 0;
            if (y == ytop)            edge |= 0x4;
            else if (y == ybot-1)     edge |= 0x8;
            if (x < l_edgeend)        edge |= 0x1;
            else if (x > r_edgestart) edge |= 0x2;

            // wireframe polygons. really ugly, but works
            if (wireframe && edge==0) continue;

            s64 factor1 = (xend+1 - x) * wr;
            s64 factor2 = (x - xstart) * wl;
            s64 denom = factor1 + factor2;
            if (denom == 0)
            {
                factor1 = 0x1000;
                factor2 = 0;
                denom = 0x1000;
            }

            s32 z = ((factor1 * zl) + (factor2 * zr)) / denom;
            if (!DepthTest(polygon, x, y, z)) continue;

            u32 vr = ((factor1 * rl) + (factor2 * rr)) / denom;
            u32 vg = ((factor1 * gl) + (factor2 * gr)) / denom;
            u32 vb = ((factor1 * bl) + (factor2 * br)) / denom;

            s16 s = ((factor1 * sl) + (factor2 * sr)) / denom;
            s16 t = ((factor1 * tl) + (factor2 * tr)) / denom;

            u32 color = RenderPixel(polygon, x, y, z, vr>>3, vg>>3, vb>>3, s, t);
            u32 attr = 0;
            u32 pixeladdr = (y*256) + x;

            u8 alpha = color >> 24;

            // alpha test
            if (DispCnt & (1<<2))
            {
                if (alpha <= AlphaRef) continue;
            }
            else
            {
                if (alpha == 0) continue;
            }

            // alpha blending disable
            // TODO: check alpha test when blending is disabled
            if (!(DispCnt & (1<<3)))
                alpha = 31;

            u32 dstcolor = ColorBuffer[pixeladdr];
            u32 dstalpha = dstcolor >> 24;

            if (alpha == 31)
            {
                // edge fill rules for opaque pixels
                // TODO, eventually: antialiasing
                if (!wireframe)
                {
                    if ((edge & 0x1) && slope_start > 0x1000)
                        continue;
                    if ((edge & 0x2) && (slope_end != 0 && slope_end <= 0x1000))
                        continue;
                }

                DepthBuffer[pixeladdr] = z;
            }
            else if (dstalpha == 0)
            {
                // TODO: conditional Z-buffer update
                DepthBuffer[pixeladdr] = z;
            }
            else
            {
                u32 srcR = color & 0x3F;
                u32 srcG = (color >> 8) & 0x3F;
                u32 srcB = (color >> 16) & 0x3F;

                u32 dstR = dstcolor & 0x3F;
                u32 dstG = (dstcolor >> 8) & 0x3F;
                u32 dstB = (dstcolor >> 16) & 0x3F;

                alpha++;
                dstR = ((srcR * alpha) + (dstR * (32-alpha))) >> 5;
                dstG = ((srcG * alpha) + (dstG * (32-alpha))) >> 5;
                dstB = ((srcB * alpha) + (dstB * (32-alpha))) >> 5;

                alpha--;
                if (alpha > dstalpha) dstalpha = alpha;

                color = dstR | (dstG << 8) | (dstB << 16) | (dstalpha << 24);

                // TODO: conditional Z-buffer update
                DepthBuffer[pixeladdr] = z;
            }

            ColorBuffer[pixeladdr] = color;
            AttrBuffer[pixeladdr] = attr;
        }

        if (lslope > 0) dxl += lslope;
        else            dxl -= lslope;
        if (rslope > 0) dxr += rslope;
        else            dxr -= rslope;
    }
}

void RenderFrame(Vertex* vertices, Polygon* polygons, int npolys)
{
    u32 polyid = (ClearAttr1 >> 24) & 0x3F;

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
                AttrBuffer[y+x] = polyid | ((val3 & 0x8000) >> 7);

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

		polyid |= ((ClearAttr1 & 0x8000) >> 7);

        for (int i = 0; i < 256*192; i++)
        {
            ColorBuffer[i] = color;
            DepthBuffer[i] = z;
            AttrBuffer[i] = polyid;
        }
    }

    // TODO: Y-sorting of translucent polygons

    for (int i = 0; i < npolys; i++)
    {
        if (polygons[i].Translucent) continue;
        RenderPolygon(&polygons[i]);
    }

    for (int i = 0; i < npolys; i++)
    {
        if (!polygons[i].Translucent) continue;
        RenderPolygon(&polygons[i]);
    }
}

u32* GetLine(int line)
{
    return &ColorBuffer[line * 256];
}

}
}
