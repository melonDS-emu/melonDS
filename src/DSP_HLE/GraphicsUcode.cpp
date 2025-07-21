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
    Log(LogLevel::Info, "DSP_HLE: initializing Graphics SDK ucode version %d\n", version);
}

GraphicsUcode::~GraphicsUcode()
{
    //
}

void GraphicsUcode::Reset()
{
    UcodeBase::Reset();
}

void GraphicsUcode::DoSavestate(Savestate *file)
{
    //
}


void GraphicsUcode::SendData(u8 index, u16 val)
{
    UcodeBase::SendData(index, val);

    if (index == 0)
    {
        if (UcodeCmd)
        {
            printf("???? there is already a command pending\n");
            return;
        }

        // writing to CMD0 initiates a ucode-specific command
        // parameters are then written to pipe 7
        UcodeCmd = val;
        CmdWritten[index] = false;

        RunUcodeCmd();
    }
    else if (index == 2)
    {
        // CMD2 serves to notify that a pipe was written to
        // value is the pipe index

        CmdWritten[index] = false;

        if (UcodeCmd)
            RunUcodeCmd();
    }
}


void GraphicsUcode::RunUcodeCmd()
{
    u16* pipe = LoadPipe(7);
    u32 len = GetPipeLength(pipe);
printf("try to run ucode cmd: cmd=%d, len=%d\n", UcodeCmd, len);
    switch (UcodeCmd)
    {
    case 1: // scaling
        if (len < 14) return;
        UcodeCmd_Scaling(pipe);
        break;
    }

    //UcodeCmd = 0;
}

void GraphicsUcode::OnUcodeCmdFinish(u32 param)
{
    printf("finish cmd %d, param=%d, %d/%d\n", UcodeCmd, param, CmdWritten[2], ReplyWritten[2]);
    UcodeCmd = 0;
    SendReply(1, (u16)param);
}

void GraphicsUcode::UcodeCmd_Scaling(u16* pipe)
{
    u16 params[14];
    ReadPipe(pipe, params, 14);

    u32 src_addr = (params[1] << 16) | params[0];
    u32 dst_addr = (params[3] << 16) | params[2];
    u16 filter = params[4];
    u16 src_width = params[5];
    u16 src_height = params[6];
    u16 width_scale = params[7];
    u16 height_scale = params[8];
    u16 rect_xoffset = params[9];
    u16 rect_yoffset = params[10];
    u16 rect_width = params[11];
    u16 rect_height = params[12];

    u32 dst_width = (src_width * width_scale) / 1000;
    u32 dst_height = (src_height * height_scale) / 1000;

    // TODO those are slightly different for bicubic
    u32 x_factor = ((rect_width - 2) << 10) / (dst_width - 1);
    u32 y_factor = ((rect_height - 2) << 10) / (dst_height - 1);

    // bound check
    // CHECKME
    //if (dst_width > rect_width) dst_width = rect_width;
    //if (dst_height > rect_height) dst_height = rect_height;
    // at 1700 it starts going out of bounds

    src_addr += (((rect_yoffset * src_width) + rect_xoffset) << 1);

    if (filter == 2)
    {
        // bilinear

        for (u32 y = 0; y < dst_height; y++)
        {
            u32 sy = (y * y_factor) + 0x200;// + 0x3FF;
            u32 syf = sy & 0x3FF;
            u32 src_line1 = src_addr + (((sy >> 10) * src_width) << 1);
            u32 src_line2 = src_line1 + (src_width << 1);

            for (u32 x = 0; x < dst_width; x++)
            {
                u32 sx = (x * x_factor) + 0x200;// + 0x3FF;
                u32 sxf = sx & 0x3FF;

                // TODO caching? see what the ucode does
                // ucode loads enough lines to fill 32K buffer (16K dsp words)
                // keeps last scanline from previous buffer
                // uses 32bit DMA
                // also starting pos is 0x200 (0.5), 0x600 for bicubic
                u16 v[4];
                v[0] = DSi.ARM9Read16(src_line1 + ((sx >> 10) << 1));
                v[1] = DSi.ARM9Read16(src_line1 + ((sx >> 10) << 1) + 2);
                v[2] = DSi.ARM9Read16(src_line2 + ((sx >> 10) << 1));
                v[3] = DSi.ARM9Read16(src_line2 + ((sx >> 10) << 1) + 2);

                u16 r[4], g[4], b[4];
                for (int i = 0; i < 4; i++)
                {
                    r[i] = v[i] & 0x1F;
                    g[i] = (v[i] >> 5) & 0x1F;
                    b[i] = (v[i] >> 10) & 0x1F;
                }

                u32 f_r, f_g, f_b;
                u32 t1, t2;

                t1 = (r[0] * (0x400-sxf)) + (r[1] * sxf);
                t2 = (r[2] * (0x400-sxf)) + (r[3] * sxf);
                f_r = (t1 * (0x400-syf)) + (t2 * syf);
                f_r = (f_r >> 20) & 0x1F;

                t1 = (g[0] * (0x400-sxf)) + (g[1] * sxf);
                t2 = (g[2] * (0x400-sxf)) + (g[3] * sxf);
                f_g = (t1 * (0x400-syf)) + (t2 * syf);
                f_g = (f_g >> 15) & 0x3E0;

                t1 = (b[0] * (0x400-sxf)) + (b[1] * sxf);
                t2 = (b[2] * (0x400-sxf)) + (b[3] * sxf);
                f_b = (t1 * (0x400-syf)) + (t2 * syf);
                f_b = (f_b >> 10) & 0x7C00;

                DSi.ARM9Write16(dst_addr, f_r | f_g | f_b | 0x8000);

                dst_addr += 2;
            }
        }
    }
    else if (filter == 3)
    {
        // bicubic
    }
    else
    {
        // nearest neighbor

        for (u32 y = 0; y < dst_height; y++)
        {
            u32 sy = ((y * y_factor) + 0x3FF) >> 10;
            u32 src_line = src_addr + ((sy * src_width) << 1);

            for (u32 x = 0; x < dst_width; x++)
            {
                u32 sx = ((x * x_factor) + 0x3FF) >> 10;

                u16 v = DSi.ARM9Read16(src_line + (sx << 1));
                DSi.ARM9Write16(dst_addr, v);

                dst_addr += 2;
            }
        }
    }

    // TODO the rest of the shit!!

    // TODO add a delay to this
    // TODO make the delay realistic
    //SendReply(1, 1);
    DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 600000, 0, 1);
}


}
}
