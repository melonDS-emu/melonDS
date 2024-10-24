/*
    Copyright 2016-2024 melonDS team

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

#include "GPU3D_Soft.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "GPU.h"

namespace melonDS
{

void RenderThreadFunc();


void SoftRenderer::StopRenderThread()
{
    if (RenderThreadRunning.load(std::memory_order_relaxed))
    {
        // Tell the render thread to stop drawing new frames, and finish up the current one.
        RenderThreadRunning = false;

        Platform::Semaphore_Post(Sema_RenderStart);

        Platform::Thread_Wait(RenderThread);
        Platform::Thread_Free(RenderThread);
        RenderThread = nullptr;
    }
}

void SoftRenderer::SetupRenderThread(GPU& gpu)
{
    if (Threaded)
    {
        if (!RenderThreadRunning.load(std::memory_order_relaxed))
        { // If the render thread isn't already running...
            RenderThreadRunning = true; // "Time for work, render thread!"
            RenderThread = Platform::Thread_Create([this, &gpu]() {
                RenderThreadFunc(gpu);
            });
        }

        // "Be on standby, but don't start rendering until I tell you to!"
        Platform::Semaphore_Reset(Sema_RenderStart);

        // "Oh, sorry, were you already in the middle of a frame from the last iteration?"
        if (RenderThreadRendering)
            // "Tell me when you're done, I'll wait here."
            Platform::Semaphore_Wait(Sema_RenderDone);

        // "All good? Okay, let me give you your training."
        // "(Maybe you're still the same thread, but I have to tell you this stuff anyway.)"

        // "This is the signal you'll send when you're done with a frame."
        // "I'll listen for it when I need to show something to the frontend."
        Platform::Semaphore_Reset(Sema_RenderDone);

        // "This is the signal I'll send when I want you to start rendering."
        // "Don't do anything until you get the message."
        Platform::Semaphore_Reset(Sema_RenderStart);

        // "This is the signal you'll send every time you finish drawing a line."
        // "I might need some of your scanlines before you finish the whole buffer,"
        // "so let me know as soon as you're done with each one."
        Platform::Semaphore_Reset(Sema_ScanlineCount);
    }
    else
    {
        StopRenderThread();
    }
}

void SoftRenderer::EnableRenderThread()
{
    if (Threaded && Sema_RenderStart)
    {
        Platform::Semaphore_Post(Sema_RenderStart);
    }
}

SoftRenderer::SoftRenderer() noexcept
    : Renderer3D(false)
{
    Sema_RenderStart = Platform::Semaphore_Create();
    Sema_RenderDone = Platform::Semaphore_Create();
    Sema_ScanlineCount = Platform::Semaphore_Create();

    RenderThreadRunning = false;
    RenderThreadRendering = false;
    RenderThread = nullptr;
}

SoftRenderer::~SoftRenderer()
{
    StopRenderThread();

    Platform::Semaphore_Free(Sema_RenderStart);
    Platform::Semaphore_Free(Sema_RenderDone);
    Platform::Semaphore_Free(Sema_ScanlineCount);
}

void SoftRenderer::Reset(GPU& gpu)
{
    memset(ColorBuffer, 0, BufferSize * 2 * 4);
    memset(DepthBuffer, 0, BufferSize * 2 * 4);
    memset(AttrBuffer, 0, BufferSize * 2 * 4);
    memset(StencilBuffer, 0, 256*2);

    ShadowRendered[0] = false;
    ShadowRendered[1] = false;
    ShadowRenderedi[0] = false;
    ShadowRenderedi[1] = false;
    SetupRenderThread(gpu);
    EnableRenderThread();
}

void SoftRenderer::DoSavestate(Savestate* file)
{
    bool secfound = file->Section("SW3D", true);

    if (secfound)
    {
        file->VarArray(StencilBuffer, sizeof(StencilBuffer));
        file->VarArray(ShadowRendered, sizeof(ShadowRendered));
        file->VarArray(ShadowRenderedi, sizeof(ShadowRenderedi));
    }
    else
    {
        memset(StencilBuffer, 0, sizeof(StencilBuffer));
        memset(ShadowRendered, 0, sizeof(ShadowRendered));
        memset(ShadowRenderedi, 0, sizeof(ShadowRenderedi));
    }
}

void SoftRenderer::SetThreaded(bool threaded, GPU& gpu) noexcept
{
    if (Threaded != threaded)
    {
        Threaded = threaded;
        SetupRenderThread(gpu);
        EnableRenderThread();
    }
}

template <bool colorcorrect>
inline void SoftRenderer::ColorConv(const u16 color, u8* r, u8* g, u8* b) const
{
    *r = (color << 1) & 0x3E; if (colorcorrect && *r) ++*r;
    *g = (color >> 4) & 0x3E; if (colorcorrect && *g) ++*g;
    *b = (color >> 9) & 0x3E; if (colorcorrect && *b) ++*b;
}

void SoftRenderer::TextureLookup(const GPU& gpu, u32 texparam, u32 texpal, s16 s, s16 t, u8* tr, u8* tg, u8* tb, u8* alpha) const
{
    u32 vramaddr = (texparam & 0xFFFF) << 3;

    s32 width = 8 << ((texparam >> 20) & 0x7);
    s32 height = 8 << ((texparam >> 23) & 0x7);

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
            u8 pixel = ReadVRAM_Texture<u8>(vramaddr, gpu);

            texpal <<= 4;
            u16 color = ReadVRAM_TexPal<u16>(texpal + ((pixel&0x1F)<<1), gpu);
            *alpha = ((pixel >> 3) & 0x1C) + (pixel >> 6);

            ColorConv<true>(color, tr, tg, tb);
        }
        break;

    case 2: // 4-color
        {
            vramaddr += (((t * width) + s) >> 2);
            u8 pixel = ReadVRAM_Texture<u8>(vramaddr, gpu);
            pixel >>= ((s & 0x3) << 1);
            pixel &= 0x3;

            texpal <<= 3;
            u16 color = ReadVRAM_TexPal<u16>(texpal + (pixel<<1), gpu);
            *alpha = (pixel==0) ? alpha0 : 31;

            ColorConv<true>(color, tr, tg, tb);
        }
        break;

    case 3: // 16-color
        {
            vramaddr += (((t * width) + s) >> 1);
            u8 pixel = ReadVRAM_Texture<u8>(vramaddr, gpu);
            if (s & 0x1) pixel >>= 4;
            else         pixel &= 0xF;

            texpal <<= 4;
            u16 color = ReadVRAM_TexPal<u16>(texpal + (pixel<<1), gpu);
            *alpha = (pixel==0) ? alpha0 : 31;

            ColorConv<true>(color, tr, tg, tb);
        }
        break;

    case 4: // 256-color
        {
            vramaddr += ((t * width) + s);
            u8 pixel = ReadVRAM_Texture<u8>(vramaddr, gpu);

            texpal <<= 4;
            u16 color = ReadVRAM_TexPal<u16>(texpal + (pixel<<1), gpu);
            *alpha = (pixel==0) ? alpha0 : 31;

            ColorConv<true>(color, tr, tg, tb);
        }
        break;

    case 5: // compressed
        {
            // NOTE: compressed textures have a bug where the wont add +1 to their texture color when increasing bit depth
            // This only happens in modes 1 and 3, but this *is* fixed by the revised rasterizer circuit
            // ...except they forgot to fix it for the interpolated colors so uh... partial credit
            // Interpolated colors are unique in being 6 bit colors; they dont discard their least significant bit.
            vramaddr += ((t & 0x3FC) * (width>>2)) + (s & 0x3FC);
            vramaddr += (t & 0x3);
            vramaddr &= 0x7FFFF; // address used for all calcs wraps around after slot 3

            u32 slot1addr = 0x20000 + ((vramaddr & 0x1FFFC) >> 1);
            if (vramaddr >= 0x40000)
                slot1addr += 0x10000;

            u8 val;
            if (vramaddr >= 0x20000 && vramaddr < 0x40000) // reading slot 1 for texels should always read 0
                val = 0;
            else
            {
                val = ReadVRAM_Texture<u8>(vramaddr, gpu);
                val >>= (2 * (s & 0x3));
            }

            u16 palinfo = ReadVRAM_Texture<u16>(slot1addr, gpu);
            u32 paloffset = (palinfo & 0x3FFF) << 2;
            texpal <<= 4;

            switch (val & 0x3)
            {
            case 0:
            {
                u16 color = ReadVRAM_TexPal<u16>(texpal + paloffset, gpu);
                *alpha = 31;
                if (!((palinfo >> 14) & 0x1) || gpu.GPU3D.RenderRasterRev)
                    ColorConv<true>(color, tr, tg, tb);
                else
                    ColorConv<false>(color, tr, tg, tb);
            }
            break;

            case 1:
            {
                u16 color = ReadVRAM_TexPal<u16>(texpal + paloffset + 2, gpu);
                *alpha = 31;
                if (!((palinfo >> 14) & 0x1) || gpu.GPU3D.RenderRasterRev)
                    ColorConv<true>(color, tr, tg, tb);
                else
                    ColorConv<false>(color, tr, tg, tb);
            }
            break;

            case 2:
                if ((palinfo >> 14) == 1)
                {
                    u16 color0 = ReadVRAM_TexPal<u16>(texpal + paloffset, gpu);
                    u16 color1 = ReadVRAM_TexPal<u16>(texpal + paloffset + 2, gpu);

                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    *tr = (r0 + r1);
                    *tg = ((g0 + g1) >> 5);
                    *tb = ((b0 + b1) >> 10);
                }
                else if ((palinfo >> 14) == 3)
                {
                    u16 color0 = ReadVRAM_TexPal<u16>(texpal + paloffset, gpu);
                    u16 color1 = ReadVRAM_TexPal<u16>(texpal + paloffset + 2, gpu);

                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    *tr = ((r0*5 + r1*3) >> 2);
                    *tg = ((g0*5 + g1*3) >> 7);
                    *tb = ((b0*5 + b1*3) >> 12);
                }
                else
                {
                    u16 color = ReadVRAM_TexPal<u16>(texpal + paloffset + 4, gpu);
                    ColorConv<true>(color, tr, tg, tb);
                }
                *alpha = 31;
                break;

            case 3:
                if ((palinfo >> 14) == 2)
                {
                    u16 color = ReadVRAM_TexPal<u16>(texpal + paloffset + 6, gpu);
                    ColorConv<true>(color, tr, tg, tb);
                    *alpha = 31;
                }
                else if ((palinfo >> 14) == 3)
                {
                    u16 color0 = ReadVRAM_TexPal<u16>(texpal + paloffset, gpu);
                    u16 color1 = ReadVRAM_TexPal<u16>(texpal + paloffset + 2, gpu);

                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    *tr = ((r0*3 + r1*5) >> 2);
                    *tg = ((g0*3 + g1*5) >> 7);
                    *tb = ((b0*3 + b1*5) >> 12);

                    *alpha = 31;
                }
                else
                {
                    *tr = 0;
                    *tg = 0;
                    *tb = 0;
                    *alpha = 0;
                }
                break;
            }
        }
        break;

    case 6: // A5I3
        {
            vramaddr += ((t * width) + s);
            u8 pixel = ReadVRAM_Texture<u8>(vramaddr, gpu);

            texpal <<= 4;
            u16 color = ReadVRAM_TexPal<u16>(texpal + ((pixel&0x7)<<1), gpu);
            ColorConv<true>(color, tr, tg, tb);
            *alpha = (pixel >> 3);
        }
        break;

    case 7: // direct color
        {
            vramaddr += (((t * width) + s) << 1);
            u16 color = ReadVRAM_Texture<u16>(vramaddr, gpu);
            ColorConv<true>(color, tr, tg, tb);
            *alpha = (color & 0x8000) ? 31 : 0;
        }
        break;
    }
}

// depth test is 'less or equal' instead of 'less than' under the following conditions:
// * when drawing a front-facing (non-shadow/shadow mask) pixel over a back-facing pixel
//   note: back-facing flag is only set in the attr buffer by opaque, non-shadow polygons
// * when drawing a top xmajor edge over a bottom xmajor edge or a left ymajor edge over a right ymajor edge
//   while their front/back-facing flag is identical
//   note: translucent/shadow/shadow mask polygons dont have edge flags
//
// range is different based on depth-buffering mode
// Z-buffering: +-0x200
// W-buffering: +-0xFF

bool DepthTest_Equal_Z(s32 dstz, s32 z, u32 attr, u32 dstattr, u8 flags)
{
    s32 diff = dstz - z;
    if ((u32)(diff + 0x200) <= 0x400)
        return true;

    return false;
}

bool DepthTest_Equal_W(s32 dstz, s32 z, u32 attr, u32 dstattr, u8 flags)
{
    s32 diff = dstz - z;
    if ((u32)(diff + 0xFF) <= 0x1FE)
        return true;

    return false;
}

bool DepthTest_LessThan(s32 dstz, s32 z, u32 attr, u32 dstattr, u8 flags)
{
    if (z < dstz)
        return true;

    return false;
}

bool DepthTest_LessThan_FrontFacing(s32 dstz, s32 z, u32 attr, u32 dstattr, u8 flags)
{
    if ((dstattr & (1<<4))) // back facing
    {
        if (z <= dstz)
            return true;
    }
    else
    {
        if (z < dstz)
            return true;
    }

    return false;
}

bool DepthTest_LessThan_EdgeQuirk(s32 dstz, s32 z, u32 attr, u32 dstattr, u8 flags)
{
    if (!(attr & (1<<4)) && (dstattr & (1<<4)))
    {
        if (z <= dstz)
            return true;
    }
    else if (((attr & (1<<4)) == (dstattr & (1<<4))) &&
            (((flags == EF_TopXMajor) && (dstattr & EF_BotXMajor)) ||
            ((flags == EF_LYMajor) && (dstattr & EF_RYMajor))))
    {
        if (z <= dstz)
            return true;
    }
    else
    {
        if (z < dstz)
            return true;
    }

    return false;
}

u32 SoftRenderer::AlphaBlend(const GPU3D& gpu3d, u32 srccolor, u32 dstcolor, u32 alpha) const noexcept
{
    u32 dstalpha = dstcolor >> 24;

    if (dstalpha == 0)
        return srccolor;

    u32 srcR = srccolor & 0x3F;
    u32 srcG = (srccolor >> 8) & 0x3F;
    u32 srcB = (srccolor >> 16) & 0x3F;

    if (gpu3d.RenderDispCnt & (1<<3))
    {
        u32 dstR = dstcolor & 0x3F;
        u32 dstG = (dstcolor >> 8) & 0x3F;
        u32 dstB = (dstcolor >> 16) & 0x3F;

        alpha++;
        srcR = ((srcR * alpha) + (dstR * (32-alpha))) >> 5;
        srcG = ((srcG * alpha) + (dstG * (32-alpha))) >> 5;
        srcB = ((srcB * alpha) + (dstB * (32-alpha))) >> 5;
        alpha--;
    }

    if (alpha > dstalpha)
        dstalpha = alpha;

    return srcR | (srcG << 8) | (srcB << 16) | (dstalpha << 24);
}

u32 SoftRenderer::RenderPixel(const GPU& gpu, const Polygon* polygon, u8 vr, u8 vg, u8 vb, s16 s, s16 t) const
{
    u8 r, g, b, a;

    u32 blendmode = (polygon->Attr >> 4) & 0x3;
    u32 polyalpha = (polygon->Attr >> 16) & 0x1F;
    bool wireframe = (polyalpha == 0);

    if (blendmode == 2)
    {
        if (gpu.GPU3D.RenderDispCnt & (1<<1))
        {
            // highlight mode: color is calculated normally
            // except all vertex color components are set
            // to the red component
            // the toon color is added to the final color

            vg = vr;
            vb = vr;
        }
        else
        {
            // toon mode: vertex color is replaced by toon color

            u16 tooncolor = gpu.GPU3D.RenderToonTable[vr >> 1];

            vr = (tooncolor << 1) & 0x3E; if (vr) vr++;
            vg = (tooncolor >> 4) & 0x3E; if (vg) vg++;
            vb = (tooncolor >> 9) & 0x3E; if (vb) vb++;
        }
    }

    if ((gpu.GPU3D.RenderDispCnt & (1<<0)) && (((polygon->TexParam >> 26) & 0x7) != 0))
    {
        u8 tr, tg, tb;

        u8 talpha;
        TextureLookup(gpu, polygon->TexParam, polygon->TexPalette, s, t, &tr, &tg, &tb, &talpha);

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

    if ((blendmode == 2) && (gpu.GPU3D.RenderDispCnt & (1<<1)))
    {
        u16 tooncolor = gpu.GPU3D.RenderToonTable[vr >> 1];

        vr = (tooncolor << 1) & 0x3E; if (vr) vr++;
        vg = (tooncolor >> 4) & 0x3E; if (vg) vg++;
        vb = (tooncolor >> 9) & 0x3E; if (vb) vb++;

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

void SoftRenderer::PlotTranslucentPixel(const GPU3D& gpu3d, u32 pixeladdr, u32 color, u32 z, u32 polyattr, u32 shadow)
{
    u32 dstattr = AttrBuffer[pixeladdr];
    u32 attr = (polyattr & (1<<15)) | ((polyattr >> 8) & 0xFF0000) | (1<<22) | (dstattr & 0xFF001F1F);

    if (shadow)
    {
        // for shadows, opaque pixels are also checked
        if (dstattr & (1<<22))
        {
            if ((dstattr & 0x007F0000) == (attr & 0x007F0000))
                return;
        }
        else
        {
            if ((dstattr & 0x3F000000) == (polyattr & 0x3F000000))
                return;
        }
    }
    else
    {
        // skip if translucent polygon IDs are equal
        if ((dstattr & 0x007F0000) == (attr & 0x007F0000))
            return;
    }

    // fog flag
    if (!(dstattr & (1<<15)))
        attr &= ~(1<<15);

    color = AlphaBlend(gpu3d, color, ColorBuffer[pixeladdr], color>>24);

    if (z != -1)
        DepthBuffer[pixeladdr] = z;

    ColorBuffer[pixeladdr] = color;
    AttrBuffer[pixeladdr] = attr;
}

void SoftRenderer::SetupPolygonLeftEdge(SoftRenderer::RendererPolygon* rp, s32 y) const
{
    Polygon* polygon = rp->PolyData;

    while (y >= polygon->Vertices[rp->NextVL]->FinalPosition[1] && rp->CurVL != polygon->VBottom)
    {
        rp->CurVL = rp->NextVL;

        if (polygon->FacingView)
        {
            rp->NextVL = rp->CurVL + 1;
            if (rp->NextVL >= polygon->NumVertices)
                rp->NextVL = 0;
        }
        else
        {
            rp->NextVL = rp->CurVL - 1;
            if ((s32)rp->NextVL < 0)
                rp->NextVL = polygon->NumVertices - 1;
        }
    }

    rp->XL = rp->SlopeL.Setup(polygon->Vertices[rp->CurVL]->FinalPosition[0], polygon->Vertices[rp->NextVL]->FinalPosition[0],
                              polygon->Vertices[rp->CurVL]->FinalPosition[1], polygon->Vertices[rp->NextVL]->FinalPosition[1],
                              polygon->FinalW[rp->CurVL], polygon->FinalW[rp->NextVL], y);
}

void SoftRenderer::SetupPolygonRightEdge(SoftRenderer::RendererPolygon* rp, s32 y) const
{
    Polygon* polygon = rp->PolyData;

    while (y >= polygon->Vertices[rp->NextVR]->FinalPosition[1] && rp->CurVR != polygon->VBottom)
    {
        rp->CurVR = rp->NextVR;

        if (polygon->FacingView)
        {
            rp->NextVR = rp->CurVR - 1;
            if ((s32)rp->NextVR < 0)
                rp->NextVR = polygon->NumVertices - 1;
        }
        else
        {
            rp->NextVR = rp->CurVR + 1;
            if (rp->NextVR >= polygon->NumVertices)
                rp->NextVR = 0;
        }
    }

    rp->XR = rp->SlopeR.Setup(polygon->Vertices[rp->CurVR]->FinalPosition[0], polygon->Vertices[rp->NextVR]->FinalPosition[0],
                              polygon->Vertices[rp->CurVR]->FinalPosition[1], polygon->Vertices[rp->NextVR]->FinalPosition[1],
                              polygon->FinalW[rp->CurVR], polygon->FinalW[rp->NextVR], y);
}

void SoftRenderer::SetupPolygon(SoftRenderer::RendererPolygon* rp, Polygon* polygon) const
{
    u32 nverts = polygon->NumVertices;

    u32 vtop = polygon->VTop, vbot = polygon->VBottom;
    s32 ytop = polygon->YTop, ybot = polygon->YBottom;

    rp->PolyData = polygon;

    rp->CurVL = vtop;
    rp->CurVR = vtop;

    if (polygon->FacingView)
    {
        rp->NextVL = rp->CurVL + 1;
        if (rp->NextVL >= nverts) rp->NextVL = 0;
        rp->NextVR = rp->CurVR - 1;
        if ((s32)rp->NextVR < 0) rp->NextVR = nverts - 1;
    }
    else
    {
        rp->NextVL = rp->CurVL - 1;
        if ((s32)rp->NextVL < 0) rp->NextVL = nverts - 1;
        rp->NextVR = rp->CurVR + 1;
        if (rp->NextVR >= nverts) rp->NextVR = 0;
    }

    if (ybot == ytop)
    {
        vtop = 0; vbot = 0;
        int i;

        i = 1;
        if (polygon->Vertices[i]->FinalPosition[0] < polygon->Vertices[vtop]->FinalPosition[0]) vtop = i;
        if (polygon->Vertices[i]->FinalPosition[0] > polygon->Vertices[vbot]->FinalPosition[0]) vbot = i;

        i = nverts - 1;
        if (polygon->Vertices[i]->FinalPosition[0] < polygon->Vertices[vtop]->FinalPosition[0]) vtop = i;
        if (polygon->Vertices[i]->FinalPosition[0] > polygon->Vertices[vbot]->FinalPosition[0]) vbot = i;

        rp->CurVL = vtop; rp->NextVL = vtop;
        rp->CurVR = vbot; rp->NextVR = vbot;

        rp->XL = rp->SlopeL.SetupDummy(polygon->Vertices[rp->CurVL]->FinalPosition[0]);
        rp->XR = rp->SlopeR.SetupDummy(polygon->Vertices[rp->CurVR]->FinalPosition[0]);
    }
    else
    {
        SetupPolygonLeftEdge(rp, ytop);
        SetupPolygonRightEdge(rp, ytop);
    }
}

void SoftRenderer::RenderShadowMaskScanline(const GPU3D& gpu3d, RendererPolygon* rp, s32 y)
{
    Polygon* polygon = rp->PolyData;

    bool (*fnDepthTest)(s32 dstz, s32 z, u32 attr, u32 dstattr, u8 flags);

    // stencil buffer is only cleared when beginning a shadow mask after a shadow polygon is rendered
    // the state of whether a polygon was or wasn't rendered can persist between frames
    // the Revised Rasterizer Circuit handles clearing the stencil buffer elsewhere
    if (ShadowRendered[y&0x1] && (!gpu3d.RenderRasterRev || polygon->ClearStencil))
    {
        StencilCleared[y&0x1] = true;
        memset(&StencilBuffer[256 * (y&0x1)], 0, 256);
        ShadowRendered[y&0x1] = false;
    }

    if (polygon->YTop != polygon->YBottom)
    {
        if (y >= polygon->Vertices[rp->NextVL]->FinalPosition[1] && rp->CurVL != polygon->VBottom)
        {
            SetupPolygonLeftEdge(rp, y);
        }

        if (y >= polygon->Vertices[rp->NextVR]->FinalPosition[1] && rp->CurVR != polygon->VBottom)
        {
            SetupPolygonRightEdge(rp, y);
        }
    }

    Vertex *vlnext, *vrnext;
    s32 xstart, xend;
    bool l_filledge, r_filledge;
    s32 l_edgelen, r_edgelen;
    s32 l_edgecov, r_edgecov;

    xstart = rp->XL;
    xend = rp->XR;

    s32 wl = rp->SlopeL.Interp.Interpolate(polygon->FinalW[rp->CurVL], polygon->FinalW[rp->NextVL]);
    s32 wr = rp->SlopeR.Interp.Interpolate(polygon->FinalW[rp->CurVR], polygon->FinalW[rp->NextVR]);

    s32 zl = rp->SlopeL.Interp.InterpolateZ(polygon->FinalZ[rp->CurVL], polygon->FinalZ[rp->NextVL], polygon->WBuffer);
    s32 zr = rp->SlopeR.Interp.InterpolateZ(polygon->FinalZ[rp->CurVR], polygon->FinalZ[rp->NextVR], polygon->WBuffer);

    // right vertical edges are pushed 1px to the left as long as either:
    // the left edge slope is not 0, or the span is not 0 pixels wide, and it is not at the leftmost pixel of the screen
    if (rp->SlopeR.Increment==0 && (rp->SlopeL.Increment!=0 || xstart != xend) && (xend != 0))
        xend--;

    // if the left and right edges are swapped, render backwards.
    if (xstart > xend)
    {
        vlnext = polygon->Vertices[rp->NextVR];
        vrnext = polygon->Vertices[rp->NextVL];

        rp->SlopeR.EdgeParams<true>(&l_edgelen, &l_edgecov);
        rp->SlopeL.EdgeParams<true>(&r_edgelen, &r_edgecov);

        std::swap(xstart, xend);
        std::swap(wl, wr);
        std::swap(zl, zr);
        
        if (polygon->Attr & (1<<14))
            fnDepthTest = polygon->WBuffer ? DepthTest_Equal_W : DepthTest_Equal_Z;
        else
            fnDepthTest = DepthTest_LessThan;

        // shadow masks follow the same fill rules as regular polygons
        if ((gpu3d.RenderDispCnt & ((1<<4)|(1<<5))) || ((polygon->Attr & (0x1F << 16)) == 0) || (((polygon->Attr & (0x1F << 16)) != (31<<16)) && (gpu3d.RenderDispCnt & (1<<3))))
        {
            l_filledge = true;
            r_filledge = true;
        }
        else
        {
            l_filledge = (rp->SlopeR.Negative || !rp->SlopeR.XMajor)
                || (y == polygon->YBottom-1) && rp->SlopeR.XMajor && (vlnext->FinalPosition[0] != vrnext->FinalPosition[0]);
            r_filledge = (!rp->SlopeL.Negative && rp->SlopeL.XMajor)
                || (!(rp->SlopeL.Negative && rp->SlopeL.XMajor) && rp->SlopeR.Increment==0)
                || (y == polygon->YBottom-1) && rp->SlopeL.XMajor && (vlnext->FinalPosition[0] != vrnext->FinalPosition[0]);
        }
    }
    else
    {
        vlnext = polygon->Vertices[rp->NextVL];
        vrnext = polygon->Vertices[rp->NextVR];

        rp->SlopeL.EdgeParams<false>(&l_edgelen, &l_edgecov);
        rp->SlopeR.EdgeParams<false>(&r_edgelen, &r_edgecov);
        
        if (polygon->Attr & (1<<14))
            fnDepthTest = polygon->WBuffer ? DepthTest_Equal_W : DepthTest_Equal_Z;
        else
            fnDepthTest = DepthTest_LessThan;

        // shadow masks follow the same fill rules as regular polygons
        if ((gpu3d.RenderDispCnt & ((1<<4)|(1<<5))) || ((polygon->Attr & (0x1F << 16)) == 0) || (((polygon->Attr & (0x1F << 16)) != (31<<16)) && (gpu3d.RenderDispCnt & (1<<3))))
        {
            l_filledge = true;
            r_filledge = true;
        }
        else
        {
            l_filledge = ((rp->SlopeL.Negative || !rp->SlopeL.XMajor)
                || (y == polygon->YBottom-1) && rp->SlopeL.XMajor && (vlnext->FinalPosition[0] != vrnext->FinalPosition[0]))
                || (rp->SlopeL.Increment == rp->SlopeR.Increment) && (xstart+l_edgelen == xend+1);
            r_filledge = (!rp->SlopeR.Negative && rp->SlopeR.XMajor) || (rp->SlopeR.Increment==0)
                || (y == polygon->YBottom-1) && rp->SlopeR.XMajor && (vlnext->FinalPosition[0] != vrnext->FinalPosition[0]);
        }
    }

    s32 x = xstart;
    Interpolator<0> interpX(xstart, xend+1, wl, wr);

    if (x < 0) x = 0;
    s32 xlimit;

    // for shadow masks: set stencil bits where the depth test fails.
    // draw nothing.

    // part 1: left edge
    xlimit = xstart+l_edgelen;
    if (xlimit > xend+1) xlimit = xend+1;
    if (xlimit > 256) xlimit = 256;

    if (!l_filledge) x = xlimit;
    else for (; x < xlimit; x++)
    {
        u32 pixeladdr = FirstPixelOffset + (y*ScanlineWidth) + x;

        interpX.SetX(x);

        s32 z = interpX.InterpolateZ(zl, zr, polygon->WBuffer);

        if (!fnDepthTest(DepthBuffer[pixeladdr], z, 0, 0, 0))
        {
            StencilBuffer[256*(y&0x1) + x] = 1;

            pixeladdr += BufferSize;
            if (!fnDepthTest(DepthBuffer[pixeladdr], z, 0, 0, 0))
                StencilBuffer[256*(y&0x1) + x] |= 0x2;
        }
    }

    // part 2: polygon inside
    xlimit = xend-r_edgelen+1;
    if (xlimit > xend+1) xlimit = xend+1;
    if (xlimit > 256) xlimit = 256;

    for (; x < xlimit; x++)
    {
        u32 pixeladdr = FirstPixelOffset + (y*ScanlineWidth) + x;

        interpX.SetX(x);

        s32 z = interpX.InterpolateZ(zl, zr, polygon->WBuffer);

        if (!fnDepthTest(DepthBuffer[pixeladdr], z, 0, 0, 0))
        {
            StencilBuffer[256*(y&0x1) + x] = 1;

            pixeladdr += BufferSize;
            if (!fnDepthTest(DepthBuffer[pixeladdr], z, 0, 0, 0))
                StencilBuffer[256*(y&0x1) + x] |= 0x2;
        }
    }

    // part 3: right edge
    xlimit = xend+1;
    if (xlimit > 256) xlimit = 256;

    if (r_filledge)
    for (; x < xlimit; x++)
    {
        u32 pixeladdr = FirstPixelOffset + (y*ScanlineWidth) + x;

        interpX.SetX(x);

        s32 z = interpX.InterpolateZ(zl, zr, polygon->WBuffer);

        if (!fnDepthTest(DepthBuffer[pixeladdr], z, 0, 0, 0))
        {
            StencilBuffer[256*(y&0x1) + x] = 1;

            pixeladdr += BufferSize;
            if (!fnDepthTest(DepthBuffer[pixeladdr], z, 0, 0, 0))
                StencilBuffer[256*(y&0x1) + x] |= 0x2;
        }
    }

    rp->XL = rp->SlopeL.Step();
    rp->XR = rp->SlopeR.Step();
}

void SoftRenderer::RenderPolygonScanline(GPU& gpu, RendererPolygon* rp, s32 y)
{
    Polygon* polygon = rp->PolyData;

    u32 polyattr = (polygon->Attr & 0x3F008000);

    u32 polyalpha = (polygon->Attr >> 16) & 0x1F;
    bool wireframe = (polyalpha == 0);

    if (polygon->IsShadow)
    {
        if (!gpu.GPU3D.RenderRasterRev || !polygon->Translucent) ShadowRendered[y&0x1] = true;
        if (wireframe) return; // TODO: this probably still counts towards timings.
        if (!StencilCleared[y&0x1])
        {
            gpu.GPU3D.ForceRerender = true;
            // set both stencil cleared flags since we've already determined that the frame needs to be rendered twice
            StencilCleared[0] = StencilCleared[1] = true;
        }
    }

    if (polygon->YTop != polygon->YBottom)
    {
        if (y >= polygon->Vertices[rp->NextVL]->FinalPosition[1] && rp->CurVL != polygon->VBottom)
        {
            SetupPolygonLeftEdge(rp, y);
        }

        if (y >= polygon->Vertices[rp->NextVR]->FinalPosition[1] && rp->CurVR != polygon->VBottom)
        {
            SetupPolygonRightEdge(rp, y);
        }
    }
    
    bool (*fnDepthTestL)(s32 dstz, s32 z, u32 attr, u32 dstattr, u8 flags);
    bool (*fnDepthTestC)(s32 dstz, s32 z, u32 attr, u32 dstattr, u8 flags);
    bool (*fnDepthTestR)(s32 dstz, s32 z, u32 attr, u32 dstattr, u8 flags);

    Vertex *vlcur, *vlnext, *vrcur, *vrnext;
    s32 xstart, xend;
    bool l_filledge, r_filledge;
    u8 l_edgeflag, c_edgeflag, r_edgeflag;
    s32 l_edgelen, r_edgelen;
    s32 l_edgecov, r_edgecov;
    Interpolator<1>* interp_start;
    Interpolator<1>* interp_end;

    xstart = rp->XL;
    xend = rp->XR;

    s32 wl = rp->SlopeL.Interp.Interpolate(polygon->FinalW[rp->CurVL], polygon->FinalW[rp->NextVL]);
    s32 wr = rp->SlopeR.Interp.Interpolate(polygon->FinalW[rp->CurVR], polygon->FinalW[rp->NextVR]);

    s32 zl = rp->SlopeL.Interp.InterpolateZ(polygon->FinalZ[rp->CurVL], polygon->FinalZ[rp->NextVL], polygon->WBuffer);
    s32 zr = rp->SlopeR.Interp.InterpolateZ(polygon->FinalZ[rp->CurVR], polygon->FinalZ[rp->NextVR], polygon->WBuffer);

    // right vertical edges are pushed 1px to the left as long as either:
    // the left edge slope is not 0, or the span is not 0 pixels wide, and it is not at the leftmost pixel of the screen
    if (rp->SlopeR.Increment==0 && (rp->SlopeL.Increment!=0 || xstart != xend) && (xend != 0))
        xend--;

    // if the left and right edges are swapped, render backwards.
    // on hardware, swapped edges seem to break edge length calculation,
    // causing X-major edges to be rendered wrong when filled,
    // and resulting in buggy looking anti-aliasing on X-major edges

    if (xstart > xend)
    {
        vlcur = polygon->Vertices[rp->CurVR];
        vlnext = polygon->Vertices[rp->NextVR];
        vrcur = polygon->Vertices[rp->CurVL];
        vrnext = polygon->Vertices[rp->NextVL];

        interp_start = &rp->SlopeR.Interp;
        interp_end = &rp->SlopeL.Interp;

        rp->SlopeR.EdgeParams<true>(&l_edgelen, &l_edgecov);
        rp->SlopeL.EdgeParams<true>(&r_edgelen, &r_edgecov);

        std::swap(xstart, xend);
        std::swap(wl, wr);
        std::swap(zl, zr);

        if (polygon->FacingView) polyattr |= (1<<4);
        bool equalsdt = false;

        if (polygon->Attr & (1<<14))
        {
            fnDepthTestR = fnDepthTestL = fnDepthTestC = polygon->WBuffer ? DepthTest_Equal_W : DepthTest_Equal_Z;
            equalsdt = true;
        }
        else if (!polygon->FacingView && !polygon->IsShadow)
            fnDepthTestR = fnDepthTestL = fnDepthTestC = DepthTest_LessThan_FrontFacing;
        else
            fnDepthTestR = fnDepthTestL = fnDepthTestC = DepthTest_LessThan;

        // edge fill rules for swapped opaque edges:
        // * right edge is filled if slope > 1, or if the left edge = 0, but is never filled if it is < -1
        // * left edge is filled if slope <= 1
        // * the bottom-most pixel of negative x-major slopes are filled if they are next to a flat bottom edge
        // edges are always filled if antialiasing/edgemarking are enabled,
        // if the pixels are translucent and alpha blending is enabled, or if the polygon is wireframe
        // checkme: do swapped line polygons exist?
        if ((gpu.GPU3D.RenderDispCnt & ((1<<4)|(1<<5))) || ((polyalpha < 31) && (gpu.GPU3D.RenderDispCnt & (1<<3))) || wireframe)
        {
            l_filledge = true;
            r_filledge = true;
        }
        else
        {
            l_filledge = (rp->SlopeR.Negative || !rp->SlopeR.XMajor)
                || (y == polygon->YBottom-1) && rp->SlopeR.XMajor && (vlnext->FinalPosition[0] != vrnext->FinalPosition[0]);
            r_filledge = (!rp->SlopeL.Negative && rp->SlopeL.XMajor)
                || (!(rp->SlopeL.Negative && rp->SlopeL.XMajor) && rp->SlopeR.Increment==0)
                || (y == polygon->YBottom-1) && rp->SlopeL.XMajor && (vlnext->FinalPosition[0] != vrnext->FinalPosition[0]);
        }

        // there is a quirk with the less than equals depth test where the back facing flag's role is inverted
        // edge marking also has a similar quirk where the top pixel is overridden by the bottom pixel when aa is disabled
        // both quirks are based on certain "edge flags" derrived from the slope's characteristics.
        // * bottom xmajor/horizontal edges are overridden by top xmajor/horizontal edges
        // * right ymajor/vertical/diagonal edges are overridden by left ymajor/vertical/diagonal edges
        // * transparent, shadow, and shadow mask polygons don't have edge flags
        if (polyalpha == 31 || wireframe)
        {
            if (rp->SlopeR.XMajor)
            {
                if (rp->SlopeR.Negative)
                {
                    l_edgeflag = EF_TopXMajor;
                    if (!equalsdt) fnDepthTestL = DepthTest_LessThan_EdgeQuirk;
                }
                else
                    l_edgeflag = EF_BotXMajor;
            }
            else
            {
                l_edgeflag = EF_LYMajor;
                if (!equalsdt) fnDepthTestL = DepthTest_LessThan_EdgeQuirk;
            }

            if (rp->SlopeL.XMajor)
            {
                if (!rp->SlopeL.Negative)
                {
                    r_edgeflag = EF_TopXMajor;
                    if (!equalsdt) fnDepthTestR = DepthTest_LessThan_EdgeQuirk;
                }
                else
                    r_edgeflag = EF_BotXMajor;
            }
            else r_edgeflag = EF_RYMajor;

            // non-slope edge flags
            //CHECKME: What happens when both flags should be applied?
            if (y == polygon->YTop)
            {
                c_edgeflag = EF_TopXMajor;
                if (!equalsdt) fnDepthTestC = DepthTest_LessThan_EdgeQuirk;
            }
            else if (y == polygon->YBottom-1)
                c_edgeflag = EF_BotXMajor;
            else
                c_edgeflag = EF_None;
        }
    }
    else
    {
        vlcur = polygon->Vertices[rp->CurVL];
        vlnext = polygon->Vertices[rp->NextVL];
        vrcur = polygon->Vertices[rp->CurVR];
        vrnext = polygon->Vertices[rp->NextVR];

        interp_start = &rp->SlopeL.Interp;
        interp_end = &rp->SlopeR.Interp;

        rp->SlopeL.EdgeParams<false>(&l_edgelen, &l_edgecov);
        rp->SlopeR.EdgeParams<false>(&r_edgelen, &r_edgecov);
        
        if (!polygon->FacingView) polyattr |= (1<<4);
        bool equalsdt = false;
        
        if (polygon->Attr & (1<<14))
        {
            fnDepthTestR = fnDepthTestL = fnDepthTestC = polygon->WBuffer ? DepthTest_Equal_W : DepthTest_Equal_Z;
            equalsdt = true;
        }
        else if (polygon->FacingView && !polygon->IsShadow)
            fnDepthTestR = fnDepthTestL = fnDepthTestC = DepthTest_LessThan_FrontFacing;
        else
            fnDepthTestR = fnDepthTestL = fnDepthTestC = DepthTest_LessThan;

        // edge fill rules for unswapped opaque edges:
        // * right edge is filled if slope > 1
        // * left edge is filled if slope <= 1
        // * edges with slope = 0 are always filled
        // * the bottom-most pixel of negative x-major slopes are filled if they are next to a flat bottom edge
        // * edges are filled if both sides are identical and fully overlapping
        // edges are always filled if antialiasing/edgemarking are enabled,
        // if the pixels are translucent and alpha blending is enabled, or if the polygon is wireframe
        if ((gpu.GPU3D.RenderDispCnt & ((1<<4)|(1<<5))) || wireframe)
        {
            l_filledge = true;
            r_filledge = true;
        }
        else
        {
            l_filledge = ((rp->SlopeL.Negative || !rp->SlopeL.XMajor)
                || (y == polygon->YBottom-1) && rp->SlopeL.XMajor && (vlnext->FinalPosition[0] != vrnext->FinalPosition[0]))
                || (rp->SlopeL.Increment == rp->SlopeR.Increment) && (xstart+l_edgelen == xend+1);
            r_filledge = (!rp->SlopeR.Negative && rp->SlopeR.XMajor) || (rp->SlopeR.Increment==0)
                || (y == polygon->YBottom-1) && rp->SlopeR.XMajor && (vlnext->FinalPosition[0] != vrnext->FinalPosition[0]);
        }

        // there is a quirk with the less than equals depth test where the back facing flag's role is inverted
        // edge marking also has a similar quirk where the top pixel is overridden by the bottom pixel when aa is disabled
        // both quirks are based on certain "edge flags" derrived from the slope's characteristics.
        // * bottom xmajor/horizontal edges are overridden by top xmajor/horizontal edges
        // * right ymajor/vertical/diagonal edges are overridden by left ymajor/vertical/diagonal edges
        // * transparent, shadow, and shadow mask polygons don't have edge flags
        if (polyalpha == 31 || wireframe)
        {
            if (rp->SlopeL.XMajor)
            {
                if (rp->SlopeL.Negative)
                {
                    l_edgeflag = EF_TopXMajor;
                    if (!equalsdt) fnDepthTestL = DepthTest_LessThan_EdgeQuirk;
                }
                else
                    l_edgeflag = EF_BotXMajor;
            }
            else
            {
                l_edgeflag = EF_LYMajor;
                if (!equalsdt) fnDepthTestL = DepthTest_LessThan_EdgeQuirk;
            }

            if (rp->SlopeR.XMajor)
            {
                if (!rp->SlopeR.Negative)
                {
                    r_edgeflag = EF_TopXMajor;
                    if (!equalsdt) fnDepthTestR = DepthTest_LessThan_EdgeQuirk;
                }
                else
                    r_edgeflag = EF_BotXMajor;
            }
            else r_edgeflag = EF_RYMajor;

            // non-slope edge flags
            if (y == polygon->YTop)
            {
                c_edgeflag = EF_TopXMajor;
                if (!equalsdt) fnDepthTestC = DepthTest_LessThan_EdgeQuirk;
            }
            else if (y == polygon->YBottom-1)
                c_edgeflag = EF_BotXMajor;
            else
                c_edgeflag = EF_None;
        }
    }
    

    // interpolate attributes along Y

    s32 rl = interp_start->Interpolate(vlcur->FinalColor[0], vlnext->FinalColor[0]);
    s32 gl = interp_start->Interpolate(vlcur->FinalColor[1], vlnext->FinalColor[1]);
    s32 bl = interp_start->Interpolate(vlcur->FinalColor[2], vlnext->FinalColor[2]);

    s32 sl = interp_start->Interpolate(vlcur->TexCoords[0], vlnext->TexCoords[0]);
    s32 tl = interp_start->Interpolate(vlcur->TexCoords[1], vlnext->TexCoords[1]);

    s32 rr = interp_end->Interpolate(vrcur->FinalColor[0], vrnext->FinalColor[0]);
    s32 gr = interp_end->Interpolate(vrcur->FinalColor[1], vrnext->FinalColor[1]);
    s32 br = interp_end->Interpolate(vrcur->FinalColor[2], vrnext->FinalColor[2]);

    s32 sr = interp_end->Interpolate(vrcur->TexCoords[0], vrnext->TexCoords[0]);
    s32 tr = interp_end->Interpolate(vrcur->TexCoords[1], vrnext->TexCoords[1]);

    s32 x = xstart;
    Interpolator<0> interpX(xstart, xend+1, wl, wr);

    if (x < 0) x = 0;
    s32 xlimit;

    s32 xcov = 0;

    // part 1: left edge
    xlimit = xstart+l_edgelen;
    if (xlimit > xend+1) xlimit = xend+1;
    if (xlimit > 256) xlimit = 256;
    if (l_edgecov & (1<<31))
    {
        xcov = (l_edgecov >> 12) & 0x3FF;
        if (xcov == 0x3FF) xcov = 0;
    }

    // allow potentially translucent pixels to be checked for translucency if blending is enabled, even if the edge isn't filled
    if (!l_filledge && !(polygon->Translucent && (gpu.GPU3D.RenderDispCnt & (1<<3)))) x = xlimit;
    else for (; x < xlimit; x++)
    {
        u32 pixeladdr = FirstPixelOffset + (y*ScanlineWidth) + x;
        u32 dstattr = AttrBuffer[pixeladdr];

        // check stencil buffer for shadows
        bool blendbot = true;
        if (polygon->IsShadow)
        {
            u8 stencil = StencilBuffer[256*(y&0x1) + x];
            if (!stencil) // if the top bit isnt set then the bottom cant be either
                continue;
            if (!(stencil & 0x2)) // check bottom pixel bit
                blendbot = false;
        }

        interpX.SetX(x);

        s32 z = interpX.InterpolateZ(zl, zr, polygon->WBuffer);

        // if depth test against the topmost pixel fails, test
        // against the pixel underneath
        if (!fnDepthTestL(DepthBuffer[pixeladdr], z, polyattr, dstattr, l_edgeflag))
        {
            if (!(dstattr & EF_AnyEdge) || !blendbot) continue;

            pixeladdr += BufferSize;
            dstattr = AttrBuffer[pixeladdr];
            if (!fnDepthTestL(DepthBuffer[pixeladdr], z, polyattr, dstattr, l_edgeflag))
                continue;
        }

        u32 vr = interpX.Interpolate(rl, rr);
        u32 vg = interpX.Interpolate(gl, gr);
        u32 vb = interpX.Interpolate(bl, br);

        s16 s = interpX.Interpolate(sl, sr);
        s16 t = interpX.Interpolate(tl, tr);

        u32 color = RenderPixel(gpu, polygon, vr>>3, vg>>3, vb>>3, s, t);
        u8 alpha = color >> 24;

        // alpha test
        if (alpha <= gpu.GPU3D.RenderAlphaRef) continue;

        if (alpha == 31)
        {
            if (!l_filledge) continue; // dont render opaque pixels unless the edge was filled
            if (!polygon->IsShadow)
            {
                u32 attr = polyattr | l_edgeflag;

                if (gpu.GPU3D.RenderDispCnt & (1<<4))
                {
                    // anti-aliasing: all edges are rendered

                    // calculate coverage
                    s32 cov = l_edgecov;
                    if (cov & (1<<31))
                    {
                        cov = xcov >> 5;
                        if (cov > 31) cov = 31;
                        xcov += (l_edgecov & 0x3FF);
                    }
                    attr |= (cov << 8);

                    // push old pixel down if needed
                    if (pixeladdr < BufferSize)
                    {
                        ColorBuffer[pixeladdr+BufferSize] = ColorBuffer[pixeladdr];
                        DepthBuffer[pixeladdr+BufferSize] = DepthBuffer[pixeladdr];
                        AttrBuffer[pixeladdr+BufferSize] = AttrBuffer[pixeladdr];
                    }
                }
                else
                {
                    // technically the bottom pixel is always updated on hardware
                    // but it doesn't matter for our purposes, aside from the depth buffer
                    // and only for a *very* niche quirk of shadow masks
                    if (pixeladdr < BufferSize)
                        DepthBuffer[pixeladdr+BufferSize] = DepthBuffer[pixeladdr];
                }

                ColorBuffer[pixeladdr] = color;
                DepthBuffer[pixeladdr] = z;
                AttrBuffer[pixeladdr] = attr;
            }
            // opaque shadows need a different opaque poly id to render (CHECKME: does translucent id matter?)
            else if ((dstattr & (0x3F << 24)) != (polyattr & (0x3F << 24)))
            {
                // opaque shadows only update the color buffer
                if ((gpu.GPU3D.RenderDispCnt & (1<<4)) && (pixeladdr < BufferSize))
                    ColorBuffer[pixeladdr+BufferSize] = ColorBuffer[pixeladdr];

                ColorBuffer[pixeladdr] = color;
            }
        }
        else
        {
            if (!(polygon->Attr & (1<<11))) z = -1;
            PlotTranslucentPixel(gpu.GPU3D, pixeladdr, color, z, polyattr, polygon->IsShadow);

            // blend with bottom pixel too, if needed
            if (blendbot && (pixeladdr < BufferSize))
                PlotTranslucentPixel(gpu.GPU3D, pixeladdr+BufferSize, color, z, polyattr, polygon->IsShadow);
        }
    }

    // part 2: polygon inside
    xlimit = xend-r_edgelen+1;
    if (xlimit > xend+1) xlimit = xend+1;
    if (xlimit > 256) xlimit = 256;

    if (wireframe && !c_edgeflag) x = std::max(x, xlimit);
    else for (; x < xlimit; x++)
    {
        u32 pixeladdr = FirstPixelOffset + (y*ScanlineWidth) + x;
        u32 dstattr = AttrBuffer[pixeladdr];

        // check stencil buffer for shadows
        bool blendbot = true;
        if (polygon->IsShadow)
        {
            u8 stencil = StencilBuffer[256*(y&0x1) + x];
            if (!stencil) // if the top bit isnt set then the bottom cant be either
                continue;
            if (!(stencil & 0x2)) // check bottom pixel bit
                blendbot = false;
        }

        interpX.SetX(x);

        s32 z = interpX.InterpolateZ(zl, zr, polygon->WBuffer);

        // if depth test against the topmost pixel fails, test
        // against the pixel underneath
        if (!fnDepthTestC(DepthBuffer[pixeladdr], z, polyattr, dstattr, c_edgeflag))
        {
            if (!(dstattr & EF_AnyEdge) || !blendbot) continue;

            pixeladdr += BufferSize;
            dstattr = AttrBuffer[pixeladdr];
            if (!fnDepthTestC(DepthBuffer[pixeladdr], z, polyattr, dstattr, c_edgeflag))
                continue;
        }

        u32 vr = interpX.Interpolate(rl, rr);
        u32 vg = interpX.Interpolate(gl, gr);
        u32 vb = interpX.Interpolate(bl, br);

        s16 s = interpX.Interpolate(sl, sr);
        s16 t = interpX.Interpolate(tl, tr);

        u32 color = RenderPixel(gpu, polygon, vr>>3, vg>>3, vb>>3, s, t);
        u8 alpha = color >> 24;

        // alpha test
        if (alpha <= gpu.GPU3D.RenderAlphaRef) continue;

        if (alpha == 31)
        {
            if (!polygon->IsShadow)
            {
                u32 attr = polyattr | c_edgeflag;

                if ((gpu.GPU3D.RenderDispCnt & (1<<4)) && c_edgeflag)
                {
                    // anti-aliasing: all edges are rendered

                    // set coverage to avoid black lines from anti-aliasing
                    attr |= (0x1F << 8);

                    // push old pixel down if needed
                    if (pixeladdr < BufferSize)
                    {
                        ColorBuffer[pixeladdr+BufferSize] = ColorBuffer[pixeladdr];
                        DepthBuffer[pixeladdr+BufferSize] = DepthBuffer[pixeladdr];
                        AttrBuffer[pixeladdr+BufferSize] = AttrBuffer[pixeladdr];
                    }
                }
                else
                {
                    // technically the bottom pixel is always updated on hardware
                    // but it doesn't matter for our purposes, aside from the depth buffer
                    // and only for a *very* niche quirk of shadow masks
                    if (pixeladdr < BufferSize)
                        DepthBuffer[pixeladdr+BufferSize] = DepthBuffer[pixeladdr];
                }

                ColorBuffer[pixeladdr] = color;
                DepthBuffer[pixeladdr] = z;
                AttrBuffer[pixeladdr] = attr;
            }
            // opaque shadows need a different opaque poly id to render (CHECKME: does translucent id matter?)
            else if ((dstattr & (0x3F << 24)) != (polyattr & (0x3F << 24)))
            {
                // opaque shadows only update the color buffer
                if ((gpu.GPU3D.RenderDispCnt & (1<<4)) && (pixeladdr < BufferSize))
                    ColorBuffer[pixeladdr+BufferSize] = ColorBuffer[pixeladdr];

                ColorBuffer[pixeladdr] = color;
            }
        }
        else
        {
            if (!(polygon->Attr & (1<<11))) z = -1;
            PlotTranslucentPixel(gpu.GPU3D, pixeladdr, color, z, polyattr, polygon->IsShadow);

            // blend with bottom pixel too, if needed
            if (blendbot && (pixeladdr < BufferSize))
                PlotTranslucentPixel(gpu.GPU3D, pixeladdr+BufferSize, color, z, polyattr, polygon->IsShadow);
        }
    }

    // part 3: right edge
    xlimit = xend+1;
    if (xlimit > 256) xlimit = 256;
    if (r_edgecov & (1<<31))
    {
        xcov = (r_edgecov >> 12) & 0x3FF;
        if (xcov == 0x3FF) xcov = 0;
    }

    // allow potentially translucent pixels to be checked for translucency if blending is enabled, even if the edge isn't filled
    if (r_filledge || (polygon->Translucent && (gpu.GPU3D.RenderDispCnt & (1<<3))))
    for (; x < xlimit; x++)
    {
        u32 pixeladdr = FirstPixelOffset + (y*ScanlineWidth) + x;
        u32 dstattr = AttrBuffer[pixeladdr];

        // check stencil buffer for shadows
        bool blendbot = true;
        if (polygon->IsShadow)
        {
            u8 stencil = StencilBuffer[256*(y&0x1) + x];
            if (!stencil) // if the top bit isnt set then the bottom cant be either
                continue;
            if (!(stencil & 0x2)) // check bottom pixel bit
                blendbot = false;
        }

        interpX.SetX(x);

        s32 z = interpX.InterpolateZ(zl, zr, polygon->WBuffer);

        // if depth test against the topmost pixel fails, test
        // against the pixel underneath
        if (!fnDepthTestR(DepthBuffer[pixeladdr], z, polyattr, dstattr, r_edgeflag))
        {
            if (!(dstattr & EF_AnyEdge) || !blendbot) continue;

            pixeladdr += BufferSize;
            dstattr = AttrBuffer[pixeladdr];
            if (!fnDepthTestR(DepthBuffer[pixeladdr], z, polyattr, dstattr, r_edgeflag))
                continue;
        }

        u32 vr = interpX.Interpolate(rl, rr);
        u32 vg = interpX.Interpolate(gl, gr);
        u32 vb = interpX.Interpolate(bl, br);

        s16 s = interpX.Interpolate(sl, sr);
        s16 t = interpX.Interpolate(tl, tr);

        u32 color = RenderPixel(gpu, polygon, vr>>3, vg>>3, vb>>3, s, t);
        u8 alpha = color >> 24;

        // alpha test
        if (alpha <= gpu.GPU3D.RenderAlphaRef) continue;

        if (alpha == 31)
        {
            if (!r_filledge) continue; // dont render opaque pixels unless the edge was filled
            if (!polygon->IsShadow)
            {
                u32 attr = polyattr | r_edgeflag;

                if (gpu.GPU3D.RenderDispCnt & (1<<4))
                {
                    // anti-aliasing: all edges are rendered

                    // calculate coverage
                    s32 cov = r_edgecov;
                    if (cov & (1<<31))
                    {
                        cov = 0x1F - (xcov >> 5);
                        if (cov < 0) cov = 0;
                        xcov += (r_edgecov & 0x3FF);
                    }
                    attr |= (cov << 8);

                    // push old pixel down if needed
                    if (pixeladdr < BufferSize)
                    {
                        ColorBuffer[pixeladdr+BufferSize] = ColorBuffer[pixeladdr];
                        DepthBuffer[pixeladdr+BufferSize] = DepthBuffer[pixeladdr];
                        AttrBuffer[pixeladdr+BufferSize] = AttrBuffer[pixeladdr];
                    }
                }
                else
                {
                    // technically the bottom pixel is always updated on hardware
                    // but it doesn't matter for our purposes, aside from the depth buffer
                    // and only for a *very* niche quirk of shadow masks
                    if (pixeladdr < BufferSize)
                        DepthBuffer[pixeladdr+BufferSize] = DepthBuffer[pixeladdr];
                }

                ColorBuffer[pixeladdr] = color;
                DepthBuffer[pixeladdr] = z;
                AttrBuffer[pixeladdr] = attr;
            }
            // opaque shadows need a different opaque poly id to render (CHECKME: does translucent id matter?)
            else if ((dstattr & (0x3F << 24)) != (polyattr & (0x3F << 24)))
            {
                // opaque shadows only update the color buffer
                if ((gpu.GPU3D.RenderDispCnt & (1<<4)) && (pixeladdr < BufferSize))
                    ColorBuffer[pixeladdr+BufferSize] = ColorBuffer[pixeladdr];

                ColorBuffer[pixeladdr] = color;
            }
        }
        else
        {
            if (!(polygon->Attr & (1<<11))) z = -1;
            PlotTranslucentPixel(gpu.GPU3D, pixeladdr, color, z, polyattr, polygon->IsShadow);

            // blend with bottom pixel too, if needed
            if (blendbot && (pixeladdr < BufferSize))
                PlotTranslucentPixel(gpu.GPU3D, pixeladdr+BufferSize, color, z, polyattr, polygon->IsShadow);
        }
    }

    rp->XL = rp->SlopeL.Step();
    rp->XR = rp->SlopeR.Step();
}

void SoftRenderer::RenderScanline(GPU& gpu, s32 y, int npolys)
{
    for (int i = 0; i < npolys; i++)
    {
        RendererPolygon* rp = &PolygonList[i];
        Polygon* polygon = rp->PolyData;
        
        //we actually handle clearing the stencil buffer here when the revision bit is set, this allows for a polygon to clear it on every scanline, even ones it isn't part of.
        if (gpu.GPU3D.RenderRasterRev)
        {
            if (polygon->ClearStencil && ShadowRenderedi[y&0x1])
            {
                StencilCleared[y&0x1] = true;
                memset(&StencilBuffer[256 * (y&0x1)], 0, 256);
                ShadowRenderedi[y&0x1] = false;
            }
            else if (polygon->IsShadow && polygon->Translucent)
            {
                ShadowRenderedi[y&0x1] = true;
            }
        }

        if (y >= polygon->YTop && (y < polygon->YBottom || (y == polygon->YTop && polygon->YBottom == polygon->YTop)))
        {
            if (polygon->IsShadowMask)
                RenderShadowMaskScanline(gpu.GPU3D, rp, y);
            else
                RenderPolygonScanline(gpu, rp, y);
        }
    }
}

u32 SoftRenderer::CalculateFogDensity(const GPU3D& gpu3d, u32 pixeladdr) const
{
    u32 z = DepthBuffer[pixeladdr];
    u32 densityid, densityfrac;

    if (z < gpu3d.RenderFogOffset)
    {
        densityid = 0;
        densityfrac = 0;
    }
    else
    {
        // technically: Z difference is shifted right by two, then shifted left by fog shift
        // then bit 0-16 are the fractional part and bit 17-31 are the density index
        // on hardware, the final value can overflow the 32-bit range with a shift big enough,
        // causing fog to 'wrap around' and accidentally apply to larger Z ranges

        z -= gpu3d.RenderFogOffset;
        z = (z >> 2) << gpu3d.RenderFogShift;

        densityid = z >> 17;
        if (densityid >= 32)
        {
            densityid = 32;
            densityfrac = 0;
        }
        else
            densityfrac = z & 0x1FFFF;
    }

    // checkme (may be too precise?)
    u32 density =
        ((gpu3d.RenderFogDensityTable[densityid] * (0x20000-densityfrac)) +
         (gpu3d.RenderFogDensityTable[densityid+1] * densityfrac)) >> 17;
    if (density >= 127) density = 128;

    return density;
}

void SoftRenderer::ScanlineFinalPass(const GPU3D& gpu3d, s32 y)
{
    // to consider:
    // clearing all polygon fog flags if the master flag isn't set?
    // merging all final pass loops into one?

    if (gpu3d.RenderDispCnt & (1<<5))
    {
        // edge marking
        // only applied to topmost pixels

        for (int x = 0; x < 256; x++)
        {
            u32 pixeladdr = FirstPixelOffset + (y*ScanlineWidth) + x;

            u32 attr = AttrBuffer[pixeladdr];
            if (!(attr & EF_AnyEdge)) continue;

            u32 polyid = attr >> 24; // opaque polygon IDs are used for edgemarking
            u32 z = DepthBuffer[pixeladdr];

            if (((polyid != (AttrBuffer[pixeladdr-1] >> 24)) && (z < DepthBuffer[pixeladdr-1])) ||
                ((polyid != (AttrBuffer[pixeladdr+1] >> 24)) && (z < DepthBuffer[pixeladdr+1])) ||
                ((polyid != (AttrBuffer[pixeladdr-ScanlineWidth] >> 24)) && (z < DepthBuffer[pixeladdr-ScanlineWidth])) ||
                ((polyid != (AttrBuffer[pixeladdr+ScanlineWidth] >> 24)) && (z < DepthBuffer[pixeladdr+ScanlineWidth])))
            {
                u16 edgecolor = gpu3d.RenderEdgeTable[polyid >> 3];
                u32 edgeR = (edgecolor << 1) & 0x3E; if (edgeR) edgeR++;
                u32 edgeG = (edgecolor >> 4) & 0x3E; if (edgeG) edgeG++;
                u32 edgeB = (edgecolor >> 9) & 0x3E; if (edgeB) edgeB++;

                ColorBuffer[pixeladdr] = edgeR | (edgeG << 8) | (edgeB << 16) | (ColorBuffer[pixeladdr] & 0xFF000000);

                // break antialiasing coverage (checkme)
                AttrBuffer[pixeladdr] = (AttrBuffer[pixeladdr] & 0xFFFFE0FF) | 0x00001000;
            }
            else if (!(gpu3d.RenderDispCnt & (1<<4)))
            {
                // if aa is disabled and the pixel failed the edge marking pass
                // do a check for top (bot flag) / left (right flag) edge flags on the bottom pixel
                // and for the absence of a top flag on the pixel down one (bot flag) / left flag one to the right (right flag)
                // if these checks pass set the colorbuffer to the bottom one
                u32 dstattr = AttrBuffer[pixeladdr+BufferSize];
                if (((attr & EF_BotXMajor) && (dstattr & EF_TopXMajor) && !(AttrBuffer[pixeladdr+ScanlineWidth] & EF_TopXMajor)) ||
                    ((attr & EF_RYMajor) && (dstattr & EF_LYMajor) && !(AttrBuffer[pixeladdr+1] & EF_LYMajor)))
                {
                    // depth and attr buffers do not get updated
                    ColorBuffer[pixeladdr] = ColorBuffer[pixeladdr+BufferSize];
                }
            }
        }
    }

    if (gpu3d.RenderDispCnt & (1<<7))
    {
        // fog

        // hardware testing shows that the fog step is 0x80000>>SHIFT
        // basically, the depth values used in GBAtek need to be
        // multiplied by 0x200 to match Z-buffer values

        // fog is applied to the topmost two pixels, which is required for
        // proper antialiasing

        // TODO: check the 'fog alpha glitch with small Z' GBAtek talks about

        bool fogcolor = !(gpu3d.RenderDispCnt & (1<<6));

        u32 fogR = (gpu3d.RenderFogColor << 1) & 0x3E; if (fogR) fogR++;
        u32 fogG = (gpu3d.RenderFogColor >> 4) & 0x3E; if (fogG) fogG++;
        u32 fogB = (gpu3d.RenderFogColor >> 9) & 0x3E; if (fogB) fogB++;
        u32 fogA = (gpu3d.RenderFogColor >> 16) & 0x1F;

        for (int x = 0; x < 256; x++)
        {
            u32 pixeladdr = FirstPixelOffset + (y*ScanlineWidth) + x;
            u32 density, srccolor, srcR, srcG, srcB, srcA;

            u32 attr = AttrBuffer[pixeladdr];
            if (attr & (1<<15))
            {
                density = CalculateFogDensity(gpu3d, pixeladdr);

                srccolor = ColorBuffer[pixeladdr];
                srcR = srccolor & 0x3F;
                srcG = (srccolor >> 8) & 0x3F;
                srcB = (srccolor >> 16) & 0x3F;
                srcA = (srccolor >> 24) & 0x1F;

                if (fogcolor)
                {
                    srcR = ((fogR * density) + (srcR * (128-density))) >> 7;
                    srcG = ((fogG * density) + (srcG * (128-density))) >> 7;
                    srcB = ((fogB * density) + (srcB * (128-density))) >> 7;
                }

                srcA = ((fogA * density) + (srcA * (128-density))) >> 7;

                ColorBuffer[pixeladdr] = srcR | (srcG << 8) | (srcB << 16) | (srcA << 24);
            }

            // fog for lower pixel
            // TODO: make this code nicer, but avoid using a loop

            if (!(attr & EF_AnyEdge)) continue;
            pixeladdr += BufferSize;

            attr = AttrBuffer[pixeladdr];
            if (!(attr & (1<<15))) continue;

            density = CalculateFogDensity(gpu3d, pixeladdr);

            srccolor = ColorBuffer[pixeladdr];
            srcR = srccolor & 0x3F;
            srcG = (srccolor >> 8) & 0x3F;
            srcB = (srccolor >> 16) & 0x3F;
            srcA = (srccolor >> 24) & 0x1F;

            if (fogcolor)
            {
                srcR = ((fogR * density) + (srcR * (128-density))) >> 7;
                srcG = ((fogG * density) + (srcG * (128-density))) >> 7;
                srcB = ((fogB * density) + (srcB * (128-density))) >> 7;
            }

            srcA = ((fogA * density) + (srcA * (128-density))) >> 7;

            ColorBuffer[pixeladdr] = srcR | (srcG << 8) | (srcB << 16) | (srcA << 24);
        }
    }

    if (gpu3d.RenderDispCnt & (1<<4))
    {
        // anti-aliasing

        // edges were flagged and their coverages calculated during rendering
        // this is where such edge pixels are blended with the pixels underneath

        for (int x = 0; x < 256; x++)
        {
            u32 pixeladdr = FirstPixelOffset + (y*ScanlineWidth) + x;

            u32 attr = AttrBuffer[pixeladdr];
            if (!(attr & EF_AnyEdge)) continue;

            u32 coverage = (attr >> 8) & 0x1F;
            if (coverage == 0x1F) continue;

            if (coverage == 0)
            {
                ColorBuffer[pixeladdr] = ColorBuffer[pixeladdr+BufferSize];
                continue;
            }

            u32 topcolor = ColorBuffer[pixeladdr];
            u32 topR = topcolor & 0x3F;
            u32 topG = (topcolor >> 8) & 0x3F;
            u32 topB = (topcolor >> 16) & 0x3F;
            u32 topA = (topcolor >> 24) & 0x1F;

            u32 botcolor = ColorBuffer[pixeladdr+BufferSize];
            u32 botR = botcolor & 0x3F;
            u32 botG = (botcolor >> 8) & 0x3F;
            u32 botB = (botcolor >> 16) & 0x3F;
            u32 botA = (botcolor >> 24) & 0x1F;

            coverage++;

            // only blend color if the bottom pixel isn't fully transparent
            if (botA > 0)
            {
                topR = ((topR * coverage) + (botR * (32-coverage))) >> 5;
                topG = ((topG * coverage) + (botG * (32-coverage))) >> 5;
                topB = ((topB * coverage) + (botB * (32-coverage))) >> 5;
            }

            // alpha is always blended
            topA = ((topA * coverage) + (botA * (32-coverage))) >> 5;

            ColorBuffer[pixeladdr] = topR | (topG << 8) | (topB << 16) | (topA << 24);
        }
    }
}

void SoftRenderer::ClearBuffers(const GPU& gpu)
{
    u32 clearz = ((gpu.GPU3D.RenderClearAttr2 & 0x7FFF) * 0x200) + 0x1FF;
    u32 polyid = gpu.GPU3D.RenderClearAttr1 & 0x3F000000; // this sets the opaque polygonID

    // clear attr buffer for the bottom pixel
    memset(&AttrBuffer[BufferSize], 0, 4*BufferSize);
    
    // fill screen borders for edge marking

    for (int x = 0; x < ScanlineWidth; x++)
    {
        ColorBuffer[x] = 0;
        DepthBuffer[x] = clearz;
        AttrBuffer[x] = polyid;
    }

    for (int x = ScanlineWidth; x < ScanlineWidth*193; x+=ScanlineWidth)
    {
        ColorBuffer[x] = 0;
        DepthBuffer[x] = clearz;
        AttrBuffer[x] = polyid;
        ColorBuffer[x+257] = 0;
        DepthBuffer[x+257] = clearz;
        AttrBuffer[x+257] = polyid;
    }

    for (int x = ScanlineWidth*193; x < ScanlineWidth*194; x++)
    {
        ColorBuffer[x] = 0;
        DepthBuffer[x] = clearz;
        AttrBuffer[x] = polyid;
    }

    // clear the screen

    if (gpu.GPU3D.RenderDispCnt & (1<<14))
    {
        u8 xoff = (gpu.GPU3D.RenderClearAttr2 >> 16) & 0xFF;
        u8 yoff = (gpu.GPU3D.RenderClearAttr2 >> 24) & 0xFF;

        for (int y = 0; y < ScanlineWidth*192; y+=ScanlineWidth)
        {
            for (int x = 0; x < 256; x++)
            {
                u16 val2 = ReadVRAM_Texture<u16>(0x40000 + (yoff << 9) + (xoff << 1), gpu);
                u16 val3 = ReadVRAM_Texture<u16>(0x60000 + (yoff << 9) + (xoff << 1), gpu);

                // TODO: confirm color conversion
                u32 r = (val2 << 1) & 0x3E; if (r) r++;
                u32 g = (val2 >> 4) & 0x3E; if (g) g++;
                u32 b = (val2 >> 9) & 0x3E; if (b) b++;
                u32 a = (val2 & 0x8000) ? 0x1F000000 : 0;
                u32 color = r | (g << 8) | (b << 16) | a;

                u32 z = ((val3 & 0x7FFF) * 0x200) + 0x1FF;

                u32 pixeladdr = FirstPixelOffset + y + x;
                ColorBuffer[pixeladdr] = color;
                DepthBuffer[pixeladdr] = z;
                AttrBuffer[pixeladdr] = polyid | (val3 & 0x8000);

                xoff++;
            }

            yoff++;
        }
    }
    else
    {
        // TODO: confirm color conversion
        u32 r = (gpu.GPU3D.RenderClearAttr1 << 1) & 0x3E; if (r) r++;
        u32 g = (gpu.GPU3D.RenderClearAttr1 >> 4) & 0x3E; if (g) g++;
        u32 b = (gpu.GPU3D.RenderClearAttr1 >> 9) & 0x3E; if (b) b++;
        u32 a = (gpu.GPU3D.RenderClearAttr1 >> 16) & 0x1F;
        u32 color = r | (g << 8) | (b << 16) | (a << 24);

        polyid |= (gpu.GPU3D.RenderClearAttr1 & 0x8000);

        for (int y = 0; y < ScanlineWidth*192; y+=ScanlineWidth)
        {
            for (int x = 0; x < 256; x++)
            {
                u32 pixeladdr = FirstPixelOffset + y + x;
                ColorBuffer[pixeladdr] = color;
                DepthBuffer[pixeladdr] = clearz;
                AttrBuffer[pixeladdr] = polyid;
            }
        }
    }
}

void SoftRenderer::RenderPolygons(GPU& gpu, bool threaded, Polygon** polygons, int npolys)
{
    if (gpu.GPU3D.DontRerenderLoop)
        gpu.GPU3D.DontRerenderLoop = false;
    else
        StencilCleared[0] = StencilCleared[1] = false;

    int j = 0;
    for (int i = 0; i < npolys; i++)
    {
        if (polygons[i]->Degenerate) continue;
        SetupPolygon(&PolygonList[j++], polygons[i]);
    }

    RenderScanline(gpu, 0, j);

    for (s32 y = 1; y < 192; y++)
    {
        RenderScanline(gpu, y, j);
        ScanlineFinalPass(gpu.GPU3D, y-1);

        if (threaded)
            // Notify the main thread that we're done with a scanline.
            Platform::Semaphore_Post(Sema_ScanlineCount);
    }

    ScanlineFinalPass(gpu.GPU3D, 191);

    if (threaded)
        // If this renderer is threaded, notify the main thread that we're done with the frame.
        Platform::Semaphore_Post(Sema_ScanlineCount);
}

void SoftRenderer::VCount144(GPU& gpu)
{
    if (RenderThreadRunning.load(std::memory_order_relaxed) && !gpu.GPU3D.AbortFrame)
        Platform::Semaphore_Wait(Sema_RenderDone);
}

void SoftRenderer::RenderFrame(GPU& gpu)
{
    auto textureDirty = gpu.VRAMDirty_Texture.DeriveState(gpu.VRAMMap_Texture, gpu);
    auto texPalDirty = gpu.VRAMDirty_TexPal.DeriveState(gpu.VRAMMap_TexPal, gpu);

    bool textureChanged = gpu.MakeVRAMFlat_TextureCoherent(textureDirty);
    bool texPalChanged = gpu.MakeVRAMFlat_TexPalCoherent(texPalDirty);

    FrameIdentical = !(textureChanged || texPalChanged) && gpu.GPU3D.RenderFrameIdentical;

    if (RenderThreadRunning.load(std::memory_order_relaxed))
    {
        // "Render thread, you're up! Get moving."
        Platform::Semaphore_Post(Sema_RenderStart);
    }
    else if (!FrameIdentical)
    {
        ClearBuffers(gpu);
        RenderPolygons(gpu, false, &gpu.GPU3D.RenderPolygonRAM[0], gpu.GPU3D.RenderNumPolygons);
    }
}

void SoftRenderer::RestartFrame(GPU& gpu)
{
    SetupRenderThread(gpu);
    EnableRenderThread();
}

void SoftRenderer::RenderThreadFunc(GPU& gpu)
{
    for (;;)
    {
        // Wait for a notice from the main thread to start rendering (or to stop entirely).
        Platform::Semaphore_Wait(Sema_RenderStart);
        if (!RenderThreadRunning) return;

        // Protect the GPU state from the main thread.
        // Some melonDS frontends (though not ours)
        // will repeatedly save or load states;
        // if they do so while the render thread is busy here,
        // the ensuing race conditions may cause a crash
        // (since some of the GPU state includes pointers).
        RenderThreadRendering = true;
        if (FrameIdentical)
        { // If no rendering is needed, just say we're done.
            Platform::Semaphore_Post(Sema_ScanlineCount, 192);
        }
        else
        {
            ClearBuffers(gpu);
            RenderPolygons(gpu, true, &gpu.GPU3D.RenderPolygonRAM[0], gpu.GPU3D.RenderNumPolygons);
        }

        // Tell the main thread that we're done rendering
        // and that it's safe to access the GPU state again.
        Platform::Semaphore_Post(Sema_RenderDone);

        RenderThreadRendering = false;
    }
}

u32* SoftRenderer::GetLine(int line)
{
    if (RenderThreadRunning.load(std::memory_order_relaxed))
    {
        if (line < 192)
            // We need a scanline, so let's wait for the render thread to finish it.
            // (both threads process scanlines from top-to-bottom,
            // so we don't need to wait for a specific row)
            Platform::Semaphore_Wait(Sema_ScanlineCount);
    }

    return &ColorBuffer[(line * ScanlineWidth) + FirstPixelOffset];
}

}
