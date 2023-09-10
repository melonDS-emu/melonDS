/*
    Copyright 2016-2022 melonDS team

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

#pragma once

#include "GPU3D.h"
#include "Platform.h"
#include <thread>
#include <atomic>

namespace GPU3D
{
class SoftRenderer : public Renderer3D
{
public:
    SoftRenderer() noexcept;
    virtual ~SoftRenderer() override;
    virtual void Reset() override;

    virtual void SetRenderSettings(GPU::RenderSettings& settings) override;

    virtual void VCount144() override;
    virtual void RenderFrame() override;
    virtual void RestartFrame() override;
    virtual u32* GetLine(int line) override;

    void SetupRenderThread();
    void StopRenderThread();
private:
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

    template<int dir>
    class Interpolator
    {
    public:
        Interpolator() {}
        Interpolator(s32 x0, s32 x1, s32 w0, s32 w1)
        {
            Setup(x0, x1, w0, w1);
        }

        void Setup(s32 x0, s32 x1, s32 w0, s32 w1)
        {
            this->x0 = x0;
            this->x1 = x1;
            this->xdiff = x1 - x0;

            // calculate reciprocal for Z interpolation
            // TODO eventually: use a faster reciprocal function?
            if (this->xdiff != 0)
                this->xrecip_z = (1<<22) / this->xdiff;
            else
                this->xrecip_z = 0;

            // linear mode is used if both W values are equal and have
            // low-order bits cleared (0-6 along X, 1-6 along Y)
            u32 mask = dir ? 0x7E : 0x7F;
            if ((w0 == w1) && !(w0 & mask) && !(w1 & mask))
                this->linear = true;
            else
                this->linear = false;

            if (dir)
            {
                // along Y

                if ((w0 & 0x1) && !(w1 & 0x1))
                {
                    this->w0n = w0 - 1;
                    this->w0d = w0 + 1;
                    this->w1d = w1;
                }
                else
                {
                    this->w0n = w0 & 0xFFFE;
                    this->w0d = w0 & 0xFFFE;
                    this->w1d = w1 & 0xFFFE;
                }

                this->shift = 9;
            }
            else
            {
                // along X

                this->w0n = w0;
                this->w0d = w0;
                this->w1d = w1;

                this->shift = 8;
            }
        }

        void SetX(s32 x)
        {
            x -= x0;
            this->x = x;
            if (xdiff != 0 && !linear)
            {
                s64 num = ((s64)x * w0n) << shift;
                s32 den = (x * w0d) + ((xdiff-x) * w1d);

                // this seems to be a proper division on hardware :/
                // I haven't been able to find cases that produce imperfect output
                if (den == 0) yfactor = 0;
                else          yfactor = (s32)(num / den);
            }
        }

        s32 Interpolate(s32 y0, s32 y1)
        {
            if (xdiff == 0 || y0 == y1) return y0;

            if (!linear)
            {
                // perspective-correct approx. interpolation
                if (y0 < y1)
                    return y0 + (((y1-y0) * yfactor) >> shift);
                else
                    return y1 + (((y0-y1) * ((1<<shift)-yfactor)) >> shift);
            }
            else
            {
                // linear interpolation
                if (y0 < y1)
                    return y0 + (s64)(y1-y0) * x / xdiff;
                else
                    return y1 + (s64)(y0-y1) * (xdiff - x) / xdiff;
            }
        }

        s32 InterpolateZ(s32 z0, s32 z1, bool wbuffer)
        {
            if (xdiff == 0 || z0 == z1) return z0;

            if (wbuffer)
            {
                // W-buffering: perspective-correct approx. interpolation
                if (z0 < z1)
                    return z0 + (((s64)(z1-z0) * yfactor) >> shift);
                else
                    return z1 + (((s64)(z0-z1) * ((1<<shift)-yfactor)) >> shift);
            }
            else
            {
                // Z-buffering: linear interpolation
                // still doesn't quite match hardware...
                s32 base, disp, factor;

                if (z0 < z1)
                {
                    base = z0;
                    disp = z1 - z0;
                    factor = x;
                }
                else
                {
                    base = z1;
                    disp = z0 - z1,
                    factor = xdiff - x;
                }

                if (dir)
                {
                    int shift = 0;
                    while (disp > 0x3FF)
                    {
                        disp >>= 1;
                        shift++;
                    }

                    return base + ((((s64)disp * factor * xrecip_z) >> 22) << shift);
                }
                else
                {
                    disp >>= 9;
                    return base + (((s64)disp * factor * xrecip_z) >> 13);
                }
            }
        }

    private:
        s32 x0, x1, xdiff, x;

        int shift;
        bool linear;

        s32 xrecip_z;
        s32 w0n, w0d, w1d;

        u32 yfactor;
    };


    template<int side>
    class Slope
    {
    public:
        Slope() {}

        s32 SetupDummy(s32 x0)
        {
            dx = 0;
            
            this->x0 = x0;
            this->xmin = x0;
            this->xmax = x0;

            Increment = 0;
            XMajor = false;

            Interp.Setup(0, 0, 0, 0);
            Interp.SetX(0);

            xcov_incr = 0;

            return x0;
        }

        s32 Setup(s32 x0, s32 x1, s32 y0, s32 y1, s32 w0, s32 w1, s32 y)
        {
            this->x0 = x0;
            this->y = y;

            if (x1 > x0)
            {
                this->xmin = x0;
                this->xmax = x1-1;
                this->Negative = false;
            }
            else if (x1 < x0)
            {
                this->xmin = x1;
                this->xmax = x0-1;
                this->Negative = true;
            }
            else
            {
                this->xmin = x0;
                this->xmax = this->xmin;
                this->Negative = false;
            }

            xlen = xmax+1 - xmin;
            ylen = y1 - y0;

            // slope increment has a 18-bit fractional part
            // note: for some reason, x/y isn't calculated directly,
            // instead, 1/y is calculated and then multiplied by x
            // TODO: this is still not perfect (see for example x=169 y=33)
            if (ylen == 0)
                Increment = 0;
            else if (ylen == xlen && xlen != 1)
                Increment = 0x40000;
            else
            {
                s32 yrecip = (1<<18) / ylen;
                Increment = (x1-x0) * yrecip;
                if (Increment < 0) Increment = -Increment;
            }

            XMajor = (Increment > 0x40000);

            if (side)
            {
                // right

                if (XMajor)              dx = Negative ? (0x20000 + 0x40000) : (Increment - 0x20000);
                else if (Increment != 0) dx = Negative ? 0x40000 : 0;
                else                     dx = 0;
            }
            else
            {
                // left

                if (XMajor)              dx = Negative ? ((Increment - 0x20000) + 0x40000) : 0x20000;
                else if (Increment != 0) dx = Negative ? 0x40000 : 0;
                else                     dx = 0;
            }

            dx += (y - y0) * Increment;

            s32 x = XVal();

            int interpoffset = (Increment >= 0x40000) && (side ^ Negative);
            Interp.Setup(y0-interpoffset, y1-interpoffset, w0, w1);
            Interp.SetX(y);

            // used for calculating AA coverage
            if (XMajor) xcov_incr = (ylen << 10) / xlen;

            return x;
        }

        s32 Step()
        {
            dx += Increment;
            y++;

            s32 x = XVal();
            Interp.SetX(y);
            return x;
        }

        s32 XVal()
        {
            s32 ret;
            if (Negative) ret = x0 - (dx >> 18);
            else          ret = x0 + (dx >> 18);

            if (ret < xmin) ret = xmin;
            else if (ret > xmax) ret = xmax;
            return ret;
        }
        
        template<bool swapped>
        void EdgeParams_XMajor(s32* length, s32* coverage)
        {
            // only do length calc for right side when swapped as it's
            // only needed for aa calcs, as actual line spans are broken
            if constexpr (!swapped || side)
            {
                if (side ^ Negative)
                    *length = (dx >> 18) - ((dx-Increment) >> 18);
                else
                    *length = ((dx+Increment) >> 18) - (dx >> 18);
            }

            // for X-major edges, we return the coverage
            // for the first pixel, and the increment for
            // further pixels on the same scanline
            s32 startx = dx >> 18;
            if (Negative) startx = xlen - startx;
            if (side)     startx = startx - *length + 1;

            s32 startcov = (((startx << 10) + 0x1FF) * ylen) / xlen;
            *coverage = (1<<31) | ((startcov & 0x3FF) << 12) | (xcov_incr & 0x3FF);

            if constexpr (swapped) *length = 1;
        }

        template<bool swapped>
        void EdgeParams_YMajor(s32* length, s32* coverage)
        {
            *length = 1;

            if (Increment == 0)
            {
                // for some reason vertical edges' aa values
                // are inverted too when the edges are swapped
                if constexpr (swapped)
                    *coverage = 0;
                else
                    *coverage = 31;
            }
            else
            {
                s32 cov = ((dx >> 9) + (Increment >> 10)) >> 4;
                if ((cov >> 5) != (dx >> 18)) cov = 31;
                cov &= 0x1F;
                if constexpr (swapped)
                {
                    if (side ^ Negative) cov = 0x1F - cov;
                }
                else
                {
                    if (!(side ^ Negative)) cov = 0x1F - cov;
                }

                *coverage = cov;
            }
        }
        
        template<bool swapped>
        void EdgeParams(s32* length, s32* coverage)
        {
            if (XMajor)
                return EdgeParams_XMajor<swapped>(length, coverage);
            else
                return EdgeParams_YMajor<swapped>(length, coverage);
        }

        s32 Increment;
        bool Negative;
        bool XMajor;
        Interpolator<1> Interp;

    private:
        s32 x0, xmin, xmax;
        s32 xlen, ylen;
        s32 dx;
        s32 y;

        s32 xcov_incr;
        s32 ycoverage, ycov_incr;
    };

    template <typename T>
    inline T ReadVRAM_Texture(u32 addr)
    {
        return *(T*)&GPU::VRAMFlat_Texture[addr & 0x7FFFF];
    }
    template <typename T>
    inline T ReadVRAM_TexPal(u32 addr)
    {
        return *(T*)&GPU::VRAMFlat_TexPal[addr & 0x1FFFF];
    }

    struct RendererPolygon
    {
        Polygon* PolyData;

        Slope<0> SlopeL;
        Slope<1> SlopeR;
        s32 XL, XR;
        u32 CurVL, CurVR;
        u32 NextVL, NextVR;

    };

    RendererPolygon PolygonList[2048];
    void TextureLookup(u32 texparam, u32 texpal, s16 s, s16 t, u16* color, u8* alpha);
    u32 RenderPixel(Polygon* polygon, u8 vr, u8 vg, u8 vb, s16 s, s16 t);
    void PlotTranslucentPixel(u32 pixeladdr, u32 color, u32 z, u32 polyattr, u32 shadow);
    void SetupPolygonLeftEdge(RendererPolygon* rp, s32 y);
    void SetupPolygonRightEdge(RendererPolygon* rp, s32 y);
    void SetupPolygon(RendererPolygon* rp, Polygon* polygon);
    void RenderShadowMaskScanline(RendererPolygon* rp, s32 y);
    void RenderPolygonScanline(RendererPolygon* rp, s32 y);
    void RenderScanline(s32 y, int npolys);
    u32 CalculateFogDensity(u32 pixeladdr);
    void ScanlineFinalPass(s32 y);
    void ClearBuffers();
    void RenderPolygons(bool threaded, Polygon** polygons, int npolys);

    void RenderThreadFunc();

    // buffer dimensions are 258x194 to add a offscreen 1px border
    // which simplifies edge marking tests
    // buffer is duplicated to keep track of the two topmost pixels
    // TODO: check if the hardware can accidentally plot pixels
    // offscreen in that border

    static constexpr int ScanlineWidth = 258;
    static constexpr int NumScanlines = 194;
    static constexpr int BufferSize = ScanlineWidth * NumScanlines;
    static constexpr int FirstPixelOffset = ScanlineWidth + 1;

    u32 ColorBuffer[BufferSize * 2];
    u32 DepthBuffer[BufferSize * 2];
    u32 AttrBuffer[BufferSize * 2];

    // attribute buffer:
    // bit0-3: edge flags (left/right/top/bottom)
    // bit4: backfacing flag
    // bit8-12: antialiasing alpha
    // bit15: fog enable
    // bit16-21: polygon ID for translucent pixels
    // bit22: translucent flag
    // bit24-29: polygon ID for opaque pixels

    u8 StencilBuffer[256*2];
    bool PrevIsShadowMask;

    bool Enabled;

    bool FrameIdentical;

    // threading

    bool Threaded;
    Platform::Thread* RenderThread;
    std::atomic_bool RenderThreadRunning;
    std::atomic_bool RenderThreadRendering;
    Platform::Semaphore* Sema_RenderStart;
    Platform::Semaphore* Sema_RenderDone;
    Platform::Semaphore* Sema_ScanlineCount;
};
}