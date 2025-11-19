/*
    Copyright 2016-2025 melonDS team

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

#include <algorithm>

#include "../DSi.h"
#include "GraphicsUcode.h"
#include "../Platform.h"


namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace DSP_HLE
{


GraphicsUcode::GraphicsUcode(melonDS::DSi& dsi, int version) : UcodeBase(dsi)
{
    UcodeClass = Class_Graphics;
    UcodeVersion = version;
    DSi.RegisterEventFuncs(Event_DSi_DSPHLE, this, {MakeEventThunk(GraphicsUcode, FinishCmd)});

    Log(LogLevel::Info, "DSP_HLE: initializing Graphics SDK ucode version %02X\n", version);
}

GraphicsUcode::~GraphicsUcode()
{
    DSi.CancelEvent(Event_DSi_DSPHLE);
    DSi.UnregisterEventFuncs(Event_DSi_DSPHLE);
}

void GraphicsUcode::Reset()
{
    UcodeBase::Reset();

    CmdState = 0;
    CmdIndex = 0;
    memset(CmdParams, 0, sizeof(CmdParams));
}

void GraphicsUcode::DoSavestate(Savestate *file)
{
    UcodeBase::DoSavestate(file);

    file->Var8(&CmdState);
    file->Var16(&CmdIndex);
    file->VarArray(CmdParams, sizeof(CmdParams));
}


void GraphicsUcode::SendData(u8 index, u16 val)
{
    UcodeBase::SendData(index, val);

    // CMD0 is used to send graphics commands
    // pipe 7 is used to send the command parameters
    // when the pipe is written, we get notified via CMD2

    if (index == 0)
    {
        TryStartCmd();
    }
    else if (index == 2)
    {
        if (val == 7)
            TryStartCmd();

        CmdWritten[2] = false;
    }
}


void GraphicsUcode::TryStartCmd()
{
    if (CmdState == 0)
    {
        if (!CmdWritten[0])
            return;

        CmdState = 1;
        CmdIndex = CmdReg[0];
        CmdWritten[0] = false;
    }

    if (CmdState != 1)
        return;

    // try to start executing this command
    // we can run as soon as we have received all the parameters in pipe 7
    // the command time is a gross estimation of the time it would take on hardware
    // the point is mostly to convey the idea that these operations aren't free
    // bicubic scaling is infact quite slow!

    u16* pipe = LoadPipe(7);
    u32 pipelen = GetPipeLength(pipe);

    u32 cmdtime;

    switch (CmdIndex)
    {
    case 1: // scaling
        {
            if (pipelen < 14) return;
            ReadPipe(pipe, CmdParams, 14);

            u32 srcwidth = CmdParams[11];
            u32 srcheight = CmdParams[12];

            if (CmdParams[4] == 10)
            {
                // 1/3 scaling

                // fails if source width/height aren't multiple of 3
                if ((srcwidth % 3) || (srcheight % 3))
                {
                    SendReply(1, 0);
                    CmdState = 0;
                    return;
                }

                cmdtime = 30 * srcwidth * srcheight;
            }
            else
            {
                // nearest/bilinear/bicubic scaling

                u32 dstwidth = (srcwidth * CmdParams[7]) / 1000;
                u32 dstheight = (srcheight * CmdParams[8]) / 1000;

                cmdtime = 4 * srcwidth * srcheight;

                switch (CmdParams[4]) // filtering
                {
                case 2: cmdtime += (58 * dstwidth * dstheight); break;
                case 3: cmdtime += (605 * dstwidth * dstheight); break;
                default: cmdtime += (26 * dstwidth * dstheight); break;
                }
            }
        }
        break;

    case 2: // yuv2rgb
        {
            if (pipelen < 6) return;
            ReadPipe(pipe, CmdParams, 6);

            u32 len = (CmdParams[1] << 16) | CmdParams[0];
            cmdtime = 24 * (len >> 1);
        }
        break;

    default: // unknown
        SendReply(1, 0);
        CmdState = 0;
        return;
    }

    CmdState = 2;
    DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 200 + cmdtime, 0, 0);
}

void GraphicsUcode::FinishCmd(u32 param)
{
    if (CmdState != 2)
        return;

    switch (CmdIndex)
    {
    case 1:
        switch (CmdParams[4])
        {
        case 2: CmdScalingBilinear(); break;
        case 3: CmdScalingBicubic(); break;
        case 10: CmdScalingOneThird(); break;
        default: CmdScalingNearest(); break;
        }
        break;

    case 2:
        CmdYuvToRgb();
        break;
    }

    SendReply(1, 1);
    CmdState = 0;
    TryStartCmd();
}


void GraphicsUcode::CmdScalingNearest()
{
    u32 src_addr = (CmdParams[1] << 16) | CmdParams[0];
    u32 dst_addr = (CmdParams[3] << 16) | CmdParams[2];
    u16 src_width = CmdParams[5];
    u16 src_height = CmdParams[6];
    u16 width_scale = CmdParams[7];
    u16 height_scale = CmdParams[8];
    u16 rect_xoffset = CmdParams[9];
    u16 rect_yoffset = CmdParams[10];
    u16 rect_width = CmdParams[11];
    u16 rect_height = CmdParams[12];

    u32 dst_width = (rect_width * width_scale) / 1000;
    u32 dst_height = (rect_height * height_scale) / 1000;

    // sanity checks
    if ((dst_width == 0) ||
        (dst_height == 0) ||
        (rect_width > 16384) ||
        (dst_width > 16384))
    {
        printf("DSP_HLE: incorrect parameters for nearest scaling\n");
        return;
    }

    u32 sx_incr = ((rect_width - 2) << 10) / (dst_width - 1);
    u32 sy_incr = ((rect_height - 2) << 10) / (dst_height - 1);
    u32 sx, sy;

    src_addr += (((rect_yoffset * src_width) + rect_xoffset) << 1);
    sy = 0x3FF;

    u16* src_mem = GetDataMemPointer(0x4000);
    u16* dst_mem = GetDataMemPointer(0xC000);

    // load first line
    ReadARM9Mem(src_mem, src_addr, rect_width << 1);

    for (u32 dy = 0; dy < dst_height; dy++)
    {
        sx = 0x3FF;

        for (u32 dx = 0; dx < dst_width; dx++)
        {
            u16 val = src_mem[sx >> 10];
            dst_mem[dx] = val;

            sx += sx_incr;
        }

        // store scaled line
        WriteARM9Mem(dst_mem, dst_addr, dst_width << 1);
        dst_addr += (dst_width << 1);

        u32 synext = sy + sy_incr;
        if ((synext >> 10) != (sy >> 10))
        {
            // load new line if needed
            ReadARM9Mem(src_mem, src_addr + (((synext>>10) * rect_width) << 1), rect_width << 1);
        }
        sy = synext;
    }
}

void GraphicsUcode::CmdScalingBilinear()
{
    u32 src_addr = (CmdParams[1] << 16) | CmdParams[0];
    u32 dst_addr = (CmdParams[3] << 16) | CmdParams[2];
    u16 src_width = CmdParams[5];
    u16 src_height = CmdParams[6];
    u16 width_scale = CmdParams[7];
    u16 height_scale = CmdParams[8];
    u16 rect_xoffset = CmdParams[9];
    u16 rect_yoffset = CmdParams[10];
    u16 rect_width = CmdParams[11];
    u16 rect_height = CmdParams[12];

    u32 dst_width = (rect_width * width_scale) / 1000;
    u32 dst_height = (rect_height * height_scale) / 1000;

    // sanity checks
    if ((dst_width == 0) ||
        (dst_height == 0) ||
        (rect_width > 8192) ||
        (dst_width > 8192))
    {
        printf("DSP_HLE: incorrect parameters for bilinear scaling\n");
        return;
    }

    u32 sx_incr = ((rect_width - 2) << 10) / (dst_width - 1);
    u32 sy_incr = ((rect_height - 2) << 10) / (dst_height - 1);
    u32 sx, sy;

    src_addr += (((rect_yoffset * src_width) + rect_xoffset) << 1);
    sy = 0x200;

    u16* src_mem = GetDataMemPointer(0x4000);
    u16* dst_mem = GetDataMemPointer(0xC000);

    // load first lines
    // for bilinear scaling, we keep the current line and the next one
    ReadARM9Mem(src_mem, src_addr, rect_width << 1);
    ReadARM9Mem(&src_mem[rect_width], src_addr, rect_width << 1);

    for (u32 dy = 0; dy < dst_height; dy++)
    {
        sx = 0x200;

        for (u32 dx = 0; dx < dst_width; dx++)
        {
            u16 val[4];
            val[0] = src_mem[sx >> 10];
            val[1] = src_mem[(sx >> 10) + 1];
            val[2] = src_mem[rect_width + (sx >> 10)];
            val[3] = src_mem[rect_width + (sx >> 10) + 1];

            u32 fx0 = sx & 0x3FF;
            u32 fx1 = 0x400 - fx0;
            u32 fy0 = sy & 0x3FF;
            u32 fy1 = 0x400 - fy0;

            u32 vr[4], vg[4], vb[4];
            u32 fr, fg, fb;

            for (int i = 0; i < 4; i++)
            {
                vr[i] = val[i] & 0x1F;
                vg[i] = (val[i] >> 5) & 0x1F;
                vb[i] = (val[i] >> 10) & 0x1F;
            }

            fr = ((((vr[0] * fx1) + (vr[1] * fx0)) * fy1) +
                  (((vr[2] * fx1) + (vr[3] * fx0)) * fy0)) >> 20;
            fg = ((((vg[0] * fx1) + (vg[1] * fx0)) * fy1) +
                  (((vg[2] * fx1) + (vg[3] * fx0)) * fy0)) >> 20;
            fb = ((((vb[0] * fx1) + (vb[1] * fx0)) * fy1) +
                  (((vb[2] * fx1) + (vb[3] * fx0)) * fy0)) >> 20;

            dst_mem[dx] = 0x8000 | (fr & 0x1F) | ((fg & 0x1F) << 5) | ((fb & 0x1F) << 10);

            sx += sx_incr;
        }

        // store scaled line
        WriteARM9Mem(dst_mem, dst_addr, dst_width << 1);
        dst_addr += (dst_width << 1);

        u32 synext = sy + sy_incr;
        if ((synext >> 10) != (sy >> 10))
        {
            // load new lines if needed
            ReadARM9Mem(src_mem, src_addr + (((synext>>10) * src_width) << 1), rect_width << 1);
            ReadARM9Mem(&src_mem[rect_width], src_addr + (((synext>>10) * src_width) << 1), rect_width << 1);
        }
        sy = synext;
    }
}

s32 GraphicsUcode::CalcBicubicWeight(s32 x)
{
    // this implements the bicubic convolution algorithm
    // https://en.wikipedia.org/wiki/Bicubic_interpolation#Bicubic_convolution_algorithm
    // Nintendo used a=-1.0
    // this function returns weights with 16 fractional bits

    if (x <= 0x400)
    {
        // x <= 1
        // W(x) = x^3 - 2x^2 + 1

        s32 square = (x * x) >> 2;
        s32 cube = (square * x) >> 12;
        square = 2 * (square >> 2);

        return cube - square + 0x10000;
    }
    else if (x <= 0x800)
    {
        // 1 < x <= 2
        // W(x) = -x^3 + 5x^2 - 8x + 4

        s32 cube = (s32)((u32)(((x * x) >> 2) * x) >> 12);
        s32 square = (5 * x * x) >> 4;
        s32 one = (-8 * x) << 6;

        return -cube + square + one + 0x40000;
    }
    else
        return 0;
}

void GraphicsUcode::CmdScalingBicubic()
{
    u32 src_addr = (CmdParams[1] << 16) | CmdParams[0];
    u32 dst_addr = (CmdParams[3] << 16) | CmdParams[2];
    u16 src_width = CmdParams[5];
    u16 src_height = CmdParams[6];
    u16 width_scale = CmdParams[7];
    u16 height_scale = CmdParams[8];
    u16 rect_xoffset = CmdParams[9];
    u16 rect_yoffset = CmdParams[10];
    u16 rect_width = CmdParams[11];
    u16 rect_height = CmdParams[12];

    u32 dst_width = (rect_width * width_scale) / 1000;
    u32 dst_height = (rect_height * height_scale) / 1000;

    // sanity checks
    if ((dst_width == 0) ||
        (dst_height == 0) ||
        (rect_width > 4096) ||
        (dst_width > 4096))
    {
        printf("DSP_HLE: incorrect parameters for bicubic scaling\n");
        return;
    }

    u32 sx_incr = ((rect_width - 4) << 10) / (dst_width - 1);
    u32 sy_incr = ((rect_height - 4) << 10) / (dst_height - 1);
    u32 sx, sy;

    src_addr += (((rect_yoffset * src_width) + rect_xoffset) << 1);
    sy = 0x200;

    u16* src_mem = GetDataMemPointer(0x4000);
    u16* dst_mem = GetDataMemPointer(0xC000);

    // load first lines
    // for bicubic scaling, we keep 4 lines around the current line
    for (int i = 0; i < 4; i++)
        ReadARM9Mem(&src_mem[rect_width * i], src_addr + ((src_width * i) << 1), rect_width << 1);

    for (u32 dy = 0; dy < dst_height; dy++)
    {
        sx = 0x200;

        for (u32 dx = 0; dx < dst_width; dx++)
        {
            u32 fx = sx & 0x3FF;
            u32 fy = sy & 0x3FF;

            s32 wx[4], wy[4];
            wx[0] = CalcBicubicWeight(0x400 + fx);
            wx[1] = CalcBicubicWeight(fx);
            wx[2] = CalcBicubicWeight(0x400 - fx);
            wx[3] = CalcBicubicWeight(0x800 - fx);
            wy[0] = CalcBicubicWeight(0x400 + fy);
            wy[1] = CalcBicubicWeight(fy);
            wy[2] = CalcBicubicWeight(0x400 - fy);
            wy[3] = CalcBicubicWeight(0x800 - fy);

            s64 tr = 0, tg = 0, tb = 0;

            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    u16 val = src_mem[(rect_width * i) + (sx >> 10) + j];

                    s32 vr = val & 0x1F;
                    s32 vg = (val >> 5) & 0x1F;
                    s32 vb = (val >> 10) & 0x1F;

                    s32 weight = ((wx[j] >> 1) * (wy[i] >> 1)) >> 6;

                    tr += (vr * weight);
                    tg += (vg * weight);
                    tb += (vb * weight);
                }
            }

            // round and clamp final colors
            s32 fr = (s32)((tr + 0x800000L) >> 24);
            s32 fg = (s32)((tg + 0x800000L) >> 24);
            s32 fb = (s32)((tb + 0x800000L) >> 24);

            fr = std::clamp(fr, 0, 31);
            fg = std::clamp(fg, 0, 31);
            fb = std::clamp(fb, 0, 31);

            dst_mem[dx] = 0x8000 | (fr & 0x1F) | ((fg & 0x1F) << 5) | ((fb & 0x1F) << 10);

            sx += sx_incr;
        }

        // store scaled line
        WriteARM9Mem(dst_mem, dst_addr, dst_width << 1);
        dst_addr += (dst_width << 1);

        u32 synext = sy + sy_incr;
        if ((synext >> 10) != (sy >> 10))
        {
            // load new lines if needed
            for (int i = 0; i < 4; i++)
                ReadARM9Mem(&src_mem[rect_width * i], src_addr + ((((synext>>10) + i) * src_width) << 1), rect_width << 1);
        }
        sy = synext;
    }
}

void GraphicsUcode::CmdScalingOneThird()
{
    u32 src_addr = (CmdParams[1] << 16) | CmdParams[0];
    u32 dst_addr = (CmdParams[3] << 16) | CmdParams[2];
    u16 src_width = CmdParams[5];
    u16 src_height = CmdParams[6];
    u16 rect_xoffset = CmdParams[9];
    u16 rect_yoffset = CmdParams[10];
    u16 rect_width = CmdParams[11];
    u16 rect_height = CmdParams[12];

    // these were already checked prior to running this
    // they are guaranteed to be multiples of 3
    u32 dst_width = rect_width / 3;
    u32 dst_height = rect_height / 3;

    // sanity checks
    if (rect_width > 16384)
    {
        printf("DSP_HLE: incorrect parameters for one-third scaling\n");
        return;
    }
    u32 sx, sy;

    src_addr += (((rect_yoffset * src_width) + rect_xoffset) << 1);
    sy = 0;

    u16* src_mem = GetDataMemPointer(0x4000);
    u16* dst_mem = GetDataMemPointer(0xC000);

    for (u32 dy = 0; dy < dst_height; dy++)
    {
        sx = 0;

        // load source lines
        for (int i = 0; i < 3; i++)
            ReadARM9Mem(&src_mem[rect_width * i], src_addr + (((sy + i) * src_width) << 1), rect_width << 1);

        // for this scaling method, we take a 3x3 block of source pixels
        // and average the 8 outer pixels
        for (u32 dx = 0; dx < dst_width; dx++)
        {
            u16 val[8];
            val[0] = src_mem[sx];
            val[1] = src_mem[sx + 1];
            val[2] = src_mem[sx + 2];
            val[3] = src_mem[rect_width + sx];
            val[4] = src_mem[rect_width + sx + 2];
            val[5] = src_mem[(rect_width * 2) + sx];
            val[6] = src_mem[(rect_width * 2) + sx + 1];
            val[7] = src_mem[(rect_width * 2) + sx + 2];

            u32 fr = 0, fg = 0, fb = 0;
            for (int i = 0; i < 8; i++)
            {
                fr += (val[i] & 0x1F);
                fg += ((val[i] >> 5) & 0x1F);
                fb += ((val[i] >> 10) & 0x1F);
            }

            dst_mem[dx] = 0x8000 | (fr >> 3) | ((fg << 2) & 0x3E0) | ((fb << 7) & 0x7C00);

            sx += 3;
        }

        // store scaled line
        WriteARM9Mem(dst_mem, dst_addr, dst_width << 1);
        dst_addr += (dst_width << 1);

        sy += 3;
    }
}


void GraphicsUcode::CmdYuvToRgb()
{
    u32 len = (CmdParams[1] << 16) | CmdParams[0];
    u32 src_addr = (CmdParams[3] << 16) | CmdParams[2];
    u32 dst_addr = (CmdParams[5] << 16) | CmdParams[4];

    for (u32 i = 0; i < len; i += 4)
    {
        u32 val = DSi.ARM9Read32(src_addr);
        src_addr += 4;

        s32 y1 = val & 0xFF;
        s32 u = (val >> 8) & 0xFF;
        s32 y2 = (val >> 16) & 0xFF;
        s32 v = (val >> 24) & 0xFF;

        u -= 128;
        v -= 128;

        // the ucode uses a bitshift based conversion
        // the formulas below are an equivalent

        s32 r = (v * 359) >> 8;
        s32 g = -((u * 352) + (v * 731)) >> 10;
        s32 b = (u * 1815) >> 10;

        s32 r1 = y1 + r;
        s32 g1 = y1 + g;
        s32 b1 = y1 + b;

        s32 r2 = y2 + r;
        s32 g2 = y2 + g;
        s32 b2 = y2 + b;

        r1 = std::clamp(r1, 0, 255); g1 = std::clamp(g1, 0, 255); b1 = std::clamp(b1, 0, 255);
        r2 = std::clamp(r2, 0, 255); g2 = std::clamp(g2, 0, 255); b2 = std::clamp(b2, 0, 255);

        u32 col1 = (r1 >> 3) | ((g1 >> 3) << 5) | ((b1 >> 3) << 10) | 0x8000;
        u32 col2 = (r2 >> 3) | ((g2 >> 3) << 5) | ((b2 >> 3) << 10) | 0x8000;

        DSi.ARM9Write32(dst_addr, col1 | (col2 << 16));
        dst_addr += 4;
    }
}


}
}
