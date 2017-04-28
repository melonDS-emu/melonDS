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
// bit15: fog enable
// bit24-29: polygon ID
// bit30: translucent flag

u8 StencilBuffer[256*192];

// note: the stencil buffer isn't emulated properly.
// emulating it properly would require rendering polygons per-scanline
// the stencil buffer is normally limited to 2 scanlines


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


// Notes on the interpolator:
//
// This is a theory on how the DS hardware interpolates values. It matches hardware output
// in the tests I did, but the hardware may be doing it differently. You never know.
//
// Assuming you want to perspective-correctly interpolate a variable named A across two points
// in a typical rasterizer, you would calculate A/W and 1/W at each point, interpolate linearly,
// then divide A/W by 1/W to recover the correct A value.
//
// The DS GPU approximates interpolation by calculating a perspective-correct interpolation
// between 0 and 1, then using the result as a factor to linearly interpolate the actual
// vertex attributes. The factor has 9 bits of precision when interpolating along Y and
// 8 bits along X.
//
// There's a special path for when the two W values are equal: it directly does linear
// interpolation, avoiding precision loss from the aforementioned approximation.
// Which is desirable when using the GPU to draw 2D graphics.

class Interpolator
{
public:
    Interpolator() {}
    Interpolator(s32 x0, s32 x1, s32 w0, s32 w1, int shift)
    {
        Setup(x0, x1, w0, w1, shift);
    }

    void Setup(s32 x0, s32 x1, s32 w0, s32 w1, int shift)
    {
        this->x0 = x0;
        this->x1 = x1;
        this->xdiff = x1 - x0;
        this->shift = shift;

        this->w0factor = (s64)w0 * xdiff;
        this->w1factor = (s64)w1 * xdiff;
        this->wdiff = w1 - w0;
    }

    void SetX(s32 x)
    {
        x -= x0;
        this->x = x;
        if (xdiff != 0 && wdiff != 0)
        {
            s64 num = ((s64)x << (shift + 40)) / w1factor;
            s64 denw0 = ((s64)(xdiff-x) << 40) / w0factor;
            s64 denw1 = num >> shift;

            s64 denom = denw0 + denw1;
            if (denom == 0)
                yfactor = 0;
            else
            {
                yfactor = (s32)(num / denom);
            }
        }
    }

    s32 Interpolate(s32 y0, s32 y1)
    {
        if (xdiff == 0) return y0;

        if (wdiff != 0)
            return y0 + (((y1 - y0) * yfactor) >> shift);
        else
            return y0 + (((y1 - y0) * x) / xdiff);
    }

    s32 InterpolateZ(s32 z0, s32 z1, bool wbuffer)
    {
        if (xdiff == 0) return z0;

        if ((wdiff != 0) && wbuffer)
            return z0 + (((s64)(z1 - z0) * yfactor) >> shift);
        else
            return z0 + (((s64)(z1 - z0) * x) / xdiff);
    }

private:
    s32 x0, x1, xdiff, x;
    s64 w0factor, w1factor;
    s32 wdiff;
    int shift;

    s32 yfactor;
};


class Slope
{
public:
    Slope() {}

    s32 SetupDummy(s32 x0, int side)
    {
        if (side)
        {
            dx = -0x10000;
            x0--;
        }
        else
        {
            dx = 0;
        }

        this->x0 = x0;
        this->xmin = x0;
        this->xmax = x0;

        Increment = 0;
        XMajor = false;

        Interp.Setup(0, 0, 0, 0, 9);
        Interp.SetX(0);

        return x0;
    }

    s32 Setup(s32 x0, s32 x1, s32 y0, s32 y1, s32 w0, s32 w1, int side)
    {
        this->x0 = x0;
        this->y = y0;

        if (x1 > x0)
        {
            this->xmin = x0;
            this->xmax = x1-1;
        }
        else if (x1 < x0)
        {
            this->xmin = x1;
            this->xmax = x0-1;
        }
        else
        {
            this->xmin = x0;
            if (side) this->xmin--;
            this->xmax = this->xmin;
        }

        if (y0 == y1)
            Increment = 0;
        else
            Increment = ((x1 - x0) << 16) / (y1 - y0);

        if (Increment < 0)
        {
            Increment = -Increment;
            Negative = true;
        }
        else
            Negative = false;

        XMajor = (Increment > 0x10000);

        if (side)
        {
            // right

            if (XMajor)              dx = Negative ? (0x8000 + 0x10000) : (Increment - 0x8000);
            else if (Increment != 0) dx = Negative ? 0x10000 : 0;
            else                     dx = -0x10000;
        }
        else
        {
            // left

            if (XMajor)              dx = Negative ? ((Increment - 0x8000) + 0x10000) : 0x8000;
            else if (Increment != 0) dx = Negative ? 0x10000 : 0;
            else                     dx = 0;
        }

        if (XMajor)
        {
            if (side) Interp.Setup(x0-1, x1-1, w0, w1, 9); // checkme
            else      Interp.Setup(x0, x1, w0, w1, 9);
        }
        else        Interp.Setup(y0, y1, w0, w1, 9);

        s32 x = XVal();
        if (XMajor) Interp.SetX(x);
        else        Interp.SetX(y);
        return x;
    }

    s32 Step()
    {
        dx += Increment;
        y++;

        s32 x = XVal();
        if (XMajor) Interp.SetX(x);
        else        Interp.SetX(y);
        return x;
    }

    s32 XVal()
    {
        s32 ret;
        if (Negative) ret = x0 - (dx >> 16);
        else          ret = x0 + (dx >> 16);

        if (ret < xmin) ret = xmin;
        else if (ret > xmax) ret = xmax;
        return ret;
    }

    s32 EdgeLimit(int side)
    {
        s32 ret;
        if (side)
        {
            if (Negative) ret = x0 - ((dx+Increment) >> 16);
            else          ret = x0 + ((dx-Increment) >> 16);
        }
        else
        {
            if (Negative) ret = x0 - ((dx-Increment) >> 16);
            else          ret = x0 + ((dx+Increment) >> 16);
        }

        return ret;
    }

    s32 Increment;
    bool Negative;
    bool XMajor;
    Interpolator Interp;

private:
    s32 x0, xmin, xmax;
    s32 dx;
    s32 y;
};


void TextureLookup(u32 texparam, u32 texpal, s16 s, s16 t, u16* color, u8* alpha)
{
    u32 vramaddr = (texparam & 0xFFFF) << 3;

    u32 width = 8 << ((texparam >> 20) & 0x7);
    u32 height = 8 << ((texparam >> 23) & 0x7);

    s >>= 4;
    t >>= 4;

    // texture wrapping
    // TODO: optimize this somehow
    // testing shows that it's hardly worth optimizing, actually

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

template<bool func_equal>
bool DepthTest(s32 oldz, s32 z)
{
    if (func_equal)
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

u32 RenderPixel(Polygon* polygon, u8 vr, u8 vg, u8 vb, s16 s, s16 t)
{
    u8 r, g, b, a;

    u32 blendmode = (polygon->Attr >> 4) & 0x3;
    u32 polyalpha = (polygon->Attr >> 16) & 0x1F;
    bool wireframe = (polyalpha == 0);

    if (blendmode == 2)
    {
        u16 tooncolor = ToonTable[vr >> 1];

        vr = (tooncolor << 1) & 0x3E; if (vr) vr++;
        vg = (tooncolor >> 4) & 0x3E; if (vg) vg++;
        vb = (tooncolor >> 9) & 0x3E; if (vb) vb++;
    }

    if ((DispCnt & (1<<0)) && (((polygon->TexParam >> 26) & 0x7) != 0))
    {
        u8 tr, tg, tb;

        u16 tcolor; u8 talpha;
        TextureLookup(polygon->TexParam, polygon->TexPalette, s, t, &tcolor, &talpha);

        tr = (tcolor << 1) & 0x3E; if (tr) tr++;
        tg = (tcolor >> 4) & 0x3E; if (tg) tg++;
        tb = (tcolor >> 9) & 0x3E; if (tb) tb++;

        if (blendmode & 0x1)
        {
            // decal

            if (talpha == 0)
            {
                r = vr;
                g = vg;
                b = vb;
            }
            else if (talpha == 31)
            {
                r = tr;
                g = tg;
                b = tb;
            }
            else
            {
                r = ((tr * talpha) + (vr * (31-talpha))) >> 5;
                g = ((tg * talpha) + (vg * (31-talpha))) >> 5;
                b = ((tb * talpha) + (vb * (31-talpha))) >> 5;
            }
            a = polyalpha;
        }
        else
        {
            // modulate

            r = ((tr+1) * (vr+1) - 1) >> 6;
            g = ((tg+1) * (vg+1) - 1) >> 6;
            b = ((tb+1) * (vb+1) - 1) >> 6;
            a = ((talpha+1) * (polyalpha+1) - 1) >> 5;
        }
    }
    else
    {
        r = vr;
        g = vg;
        b = vb;
        a = polyalpha;
    }

    if ((blendmode == 2) && (DispCnt & (1<<1)))
    {
        r += vr;
        g += vg;
        b += vb;

        if (r > 63) r = 63;
        if (g > 63) g = 63;
        if (b > 63) b = 63;
    }

    // checkme: can wireframe polygons use texture alpha?
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

    bool (*fnDepthTest)(s32 oldz, s32 z);
    if (polygon->Attr & (1<<14))
        fnDepthTest = DepthTest<true>;
    else
        fnDepthTest = DepthTest<false>;


    int lcur = vtop, rcur = vtop;
    int lnext, rnext;

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

    Slope slopeL, slopeR;
    s32 xL, xR;
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

        xL = slopeL.SetupDummy(polygon->Vertices[lcur]->FinalPosition[0], 0);
        xR = slopeR.SetupDummy(polygon->Vertices[rcur]->FinalPosition[0], 1);
    }
    else
    {
        while (ytop >= polygon->Vertices[lnext]->FinalPosition[1] && lcur != vbot)
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

        xL = slopeL.Setup(polygon->Vertices[lcur]->FinalPosition[0], polygon->Vertices[lnext]->FinalPosition[0],
                          polygon->Vertices[lcur]->FinalPosition[1], polygon->Vertices[lnext]->FinalPosition[1],
                          polygon->FinalW[lcur], polygon->FinalW[lnext], 0);

        while (ytop >= polygon->Vertices[rnext]->FinalPosition[1] && rcur != vbot)
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

        xR = slopeR.Setup(polygon->Vertices[rcur]->FinalPosition[0], polygon->Vertices[rnext]->FinalPosition[0],
                          polygon->Vertices[rcur]->FinalPosition[1], polygon->Vertices[rnext]->FinalPosition[1],
                          polygon->FinalW[rcur], polygon->FinalW[rnext], 1);
    }

    if (ybot > 192) ybot = 192;

    if (polygon->ClearStencil)
    {
        memset(StencilBuffer, 0, 192*256);
    }

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

                xL = slopeL.Setup(polygon->Vertices[lcur]->FinalPosition[0], polygon->Vertices[lnext]->FinalPosition[0],
                                  polygon->Vertices[lcur]->FinalPosition[1], polygon->Vertices[lnext]->FinalPosition[1],
                                  polygon->FinalW[lcur], polygon->FinalW[lnext], 0);
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

                xR = slopeR.Setup(polygon->Vertices[rcur]->FinalPosition[0], polygon->Vertices[rnext]->FinalPosition[0],
                                  polygon->Vertices[rcur]->FinalPosition[1], polygon->Vertices[rnext]->FinalPosition[1],
                                  polygon->FinalW[rcur], polygon->FinalW[rnext], 1);
            }
        }

        Vertex *vlcur, *vlnext, *vrcur, *vrnext;
        s32 xstart, xend;
        Slope* slope_start;
        Slope* slope_end;

        xstart = xL;
        xend = xR;

        s32 wl = slopeL.Interp.Interpolate(polygon->FinalW[lcur], polygon->FinalW[lnext]);
        s32 wr = slopeR.Interp.Interpolate(polygon->FinalW[rcur], polygon->FinalW[rnext]);

        s32 zl = slopeL.Interp.InterpolateZ(polygon->FinalZ[lcur], polygon->FinalZ[lnext], polygon->WBuffer);
        s32 zr = slopeR.Interp.InterpolateZ(polygon->FinalZ[rcur], polygon->FinalZ[rnext], polygon->WBuffer);

        // if the left and right edges are swapped, render backwards.
        // note: we 'forget' to swap the xmajor flags, on purpose
        // the hardware has the same bug
        if (xstart > xend)
        {
            vlcur = polygon->Vertices[rcur];
            vlnext = polygon->Vertices[rnext];
            vrcur = polygon->Vertices[lcur];
            vrnext = polygon->Vertices[lnext];

            slope_start = &slopeR;
            slope_end = &slopeL;

            s32 tmp;
            tmp = xstart; xstart = xend; xend = tmp;
            tmp = wl; wl = wr; wr = tmp;
            tmp = zl; zl = zr; zr = tmp;
        }
        else
        {
            vlcur = polygon->Vertices[lcur];
            vlnext = polygon->Vertices[lnext];
            vrcur = polygon->Vertices[rcur];
            vrnext = polygon->Vertices[rnext];

            slope_start = &slopeL;
            slope_end = &slopeR;
        }

        // interpolate attributes along Y

        s32 rl = slope_start->Interp.Interpolate(vlcur->FinalColor[0], vlnext->FinalColor[0]);
        s32 gl = slope_start->Interp.Interpolate(vlcur->FinalColor[1], vlnext->FinalColor[1]);
        s32 bl = slope_start->Interp.Interpolate(vlcur->FinalColor[2], vlnext->FinalColor[2]);

        s32 sl = slope_start->Interp.Interpolate(vlcur->TexCoords[0], vlnext->TexCoords[0]);
        s32 tl = slope_start->Interp.Interpolate(vlcur->TexCoords[1], vlnext->TexCoords[1]);

        s32 rr = slope_end->Interp.Interpolate(vrcur->FinalColor[0], vrnext->FinalColor[0]);
        s32 gr = slope_end->Interp.Interpolate(vrcur->FinalColor[1], vrnext->FinalColor[1]);
        s32 br = slope_end->Interp.Interpolate(vrcur->FinalColor[2], vrnext->FinalColor[2]);

        s32 sr = slope_end->Interp.Interpolate(vrcur->TexCoords[0], vrnext->TexCoords[0]);
        s32 tr = slope_end->Interp.Interpolate(vrcur->TexCoords[1], vrnext->TexCoords[1]);

        // calculate edges
        //
        // edge fill rules for opaque pixels:
        // * right edge is filled if slope > 1
        // * left edge is filled if slope <= 1
        // * edges with slope = 0 are always filled
        // edges are always filled if the pixels are translucent
        // in wireframe mode, there are special rules for equal Z (TODO)

        s32 l_edgeend, r_edgestart;
        bool l_filledge, r_filledge;

        if (slopeL.XMajor)
        {
            l_edgeend = slope_start->EdgeLimit(0);
            if (l_edgeend == xstart) l_edgeend++;

            l_filledge = slope_start->Negative;
        }
        else
        {
            l_edgeend = xstart + 1;

            l_filledge = true;
        }

        if (slopeR.XMajor)
        {
            r_edgestart = slope_end->EdgeLimit(1);
            if (r_edgestart == xend) r_edgestart--;

            r_filledge = !slope_end->Negative;
        }
        else
        {
            r_edgestart = xend - 1;

            r_filledge = slope_end->Increment==0;
        }

        int yedge = 0;
        if (y == ytop)        yedge = 0x4;
        else if (y == ybot-1) yedge = 0x8;

        Interpolator interpX(xstart, xend+1, wl, wr, 8);

        for (s32 x = xstart; x <= xend; x++)
        {
            if (x < 0) continue;
            if (x > 255) break;

            int edge = yedge;
            if (x < l_edgeend)        edge |= 0x1;
            else if (x > r_edgestart) edge |= 0x2;

            // wireframe polygons. really ugly, but works
            if (wireframe && edge==0)
            {
                x = r_edgestart + 1;
                continue;
            }

            u32 pixeladdr = (y*256) + x;
            u32 attr = polygon->Attr & 0x3F008000;

            // check stencil buffer for shadows
            if (polygon->IsShadow)
            {
                if (StencilBuffer[pixeladdr] == 0)
                    continue;
            }

            interpX.SetX(x);

            s32 z = interpX.InterpolateZ(zl, zr, polygon->WBuffer);

            if (polygon->IsShadowMask)
            {
                // for shadow masks: set stencil bits where the depth test fails.
                // draw nothing.

                // checkme
                if (polyalpha == 31)
                {
                    if (!wireframe)
                    {
                        if ((edge & 0x1) && !l_filledge)
                            continue;
                        if ((edge & 0x2) && !r_filledge)
                            continue;
                    }
                }

                if (!fnDepthTest(DepthBuffer[pixeladdr], z))
                    StencilBuffer[pixeladdr] = 1;

                continue;
            }

            if (!fnDepthTest(DepthBuffer[pixeladdr], z))
                continue;

            u32 vr = interpX.Interpolate(rl, rr);
            u32 vg = interpX.Interpolate(gl, gr);
            u32 vb = interpX.Interpolate(bl, br);

            s16 s = interpX.Interpolate(sl, sr);
            s16 t = interpX.Interpolate(tl, tr);

            u32 color = RenderPixel(polygon, vr>>3, vg>>3, vb>>3, s, t);
            u8 alpha = color >> 24;

            // alpha test
            // TODO: check alpha test when blending is disabled
            if (alpha <= AlphaRef) continue;

            if (alpha == 31)
            {
                // edge fill rules for opaque pixels
                // TODO, eventually: antialiasing
                if (!wireframe)
                {
                    if ((edge & 0x1) && !l_filledge)
                        continue;
                    if ((edge & 0x2) && !r_filledge)
                        continue;
                }

                DepthBuffer[pixeladdr] = z;
            }
            else
            {
                u32 dstattr = AttrBuffer[pixeladdr];
                attr |= (1<<30);
                if (polygon->IsShadow) dstattr |= (1<<30);

                // skip if polygon IDs are equal
                // note: this only happens if the destination pixel was translucent
                // or always when drawing a shadow
                // (the GPU keeps track of which pixels are translucent, regardless of
                // the destination alpha)
                if ((dstattr & 0x7F000000) == (attr & 0x7F000000))
                    continue;

                u32 dstcolor = ColorBuffer[pixeladdr];
                u32 dstalpha = dstcolor >> 24;

                if ((dstalpha > 0) && (DispCnt & (1<<3)))
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
                }

                if (polygon->Attr & (1<<11))
                    DepthBuffer[pixeladdr] = z;
            }

            ColorBuffer[pixeladdr] = color;
            AttrBuffer[pixeladdr] = attr;
        }

        xL = slopeL.Step();
        xR = slopeR.Step();
    }
}

void RenderFrame(Vertex* vertices, Polygon* polygons, int npolys)
{
    u32 polyid = RenderClearAttr1 & 0x3F000000;

    if (DispCnt & (1<<14))
    {
        u8 xoff = (RenderClearAttr2 >> 16) & 0xFF;
        u8 yoff = (RenderClearAttr2 >> 24) & 0xFF;

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

                ColorBuffer[y+x] = color;
                DepthBuffer[y+x] = z;
                AttrBuffer[y+x] = polyid | (val3 & 0x8000);

                xoff++;
            }

            yoff++;
        }
    }
    else
    {
        // TODO: confirm color conversion
        u32 r = (RenderClearAttr1 << 1) & 0x3E; if (r) r++;
        u32 g = (RenderClearAttr1 >> 4) & 0x3E; if (g) g++;
        u32 b = (RenderClearAttr1 >> 9) & 0x3E; if (b) b++;
        u32 a = (RenderClearAttr1 >> 16) & 0x1F;
        u32 color = r | (g << 8) | (b << 16) | (a << 24);

        u32 z = ((RenderClearAttr2 & 0x7FFF) * 0x200) + 0x1FF;

		polyid |= (RenderClearAttr1 & 0x8000);

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
