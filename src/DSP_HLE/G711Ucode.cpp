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
#include "G711Ucode.h"
#include "../Platform.h"


namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace DSP_HLE
{


G711Ucode::G711Ucode(melonDS::DSi& dsi, int version) : UcodeBase(dsi)
{
    UcodeClass = Class_G711;
    UcodeVersion = version;
    DSi.RegisterEventFuncs(Event_DSi_DSPHLE, this, {MakeEventThunk(G711Ucode, FinishCmd)});

    Log(LogLevel::Info, "DSP_HLE: initializing G711 SDK ucode version %02X\n", version);
}

G711Ucode::~G711Ucode()
{
    DSi.CancelEvent(Event_DSi_DSPHLE);
    DSi.UnregisterEventFuncs(Event_DSi_DSPHLE);
}

void G711Ucode::Reset()
{
    UcodeBase::Reset();

    CmdState = 0;
    memset(CmdParams, 0, sizeof(CmdParams));
}

void G711Ucode::DoSavestate(Savestate *file)
{
    UcodeBase::DoSavestate(file);

    file->Var8(&CmdState);
    file->VarArray(CmdParams, sizeof(CmdParams));
}


void G711Ucode::SendData(u8 index, u16 val)
{
    UcodeBase::SendData(index, val);

    // pipe 7 is used to send commands and parameters
    // when the pipe is written, we get notified via CMD2

    if (index == 2)
    {
        if (val == 7)
            TryStartCmd();

        CmdWritten[2] = false;
    }
}


void G711Ucode::TryStartCmd()
{
    if (CmdState != 0)
        return;

    // try to start executing this command
    // we can run as soon as we have received 8 words in pipe 7

    u16* pipe = LoadPipe(7);
    u32 pipelen = GetPipeLength(pipe);
    if (pipelen < 8) return;

    ReadPipe(pipe, CmdParams, 8);
    u32 cmd = (CmdParams[0] << 16) | CmdParams[1];
    u32 len = (CmdParams[6] << 16) | CmdParams[7];

    u32 action = (cmd >> 8) & 0xF;
    u32 type = cmd & 0xFF;

    // gross estimation of the time the command would take
    u32 cmdtime;
    if ((type != 1) && (type != 2))
        cmdtime = 1000;
    else if (action == 1)
        cmdtime = 31 * len;
    else
        cmdtime = 14 * len;

    CmdState = 1;
    DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 200 + cmdtime, 0, 0);
}

void G711Ucode::FinishCmd(u32 param)
{
    if (CmdState != 1)
        return;

    u32 cmd = (CmdParams[0] << 16) | CmdParams[1];
    u32 action = (cmd >> 8) & 0xF;
    u32 type = cmd & 0xFF;

    if (action == 1)
    {
        // encode

        if (type == 1)
            CmdEncodeALaw();
        else if (type == 2)
            CmdEncodeULaw();
    }
    else
    {
        // decode

        if (type == 1)
            CmdDecodeALaw();
        else if (type == 2)
            CmdDecodeULaw();
    }

    // response = processed length (even for invalid cmd type)
    u16* pipe = LoadPipe(6);
    u16 resp[2] = {CmdParams[6], CmdParams[7]};
    WritePipe(pipe, resp, 2);

    CmdState = 0;
    TryStartCmd();
}

void G711Ucode::CmdEncodeALaw()
{
    const s16 seg_tbl[8] = {0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF};

    u32 src_addr = (CmdParams[2] << 16) | CmdParams[3];
    u32 dst_addr = (CmdParams[4] << 16) | CmdParams[5];
    u32 len = (CmdParams[6] << 16) | CmdParams[7];

    for (u32 i = 0; i < len; i++)
    {
        s16 val16 = (s16)DSi.ARM9Read16(src_addr);
        s16 val8, xor8;

        val8 = val16 >> 3;
        if (val8 > 0)
        {
            xor8 = 0xD5;
        }
        else
        {
            val8 ^= 0xFFFF;
            xor8 = 0x55;
        }

        int seg;
        for (seg = 0; seg < 8; seg++)
        {
            if (val8 <= seg_tbl[seg])
                break;
        }

        if (seg < 8)
        {
            s16 tmp = seg << 4;
            if (seg == 0) seg = 1;

            val8 = tmp | ((val8 >> seg) & 0xF);
        }
        else
            val8 = 0x7F;

        val8 ^= xor8;

        DSi.ARM9Write8(dst_addr, (s8)val8);
        src_addr += 2;
        dst_addr++;
    }
}

void G711Ucode::CmdEncodeULaw()
{
    const s16 seg_tbl[8] = {0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF};

    u32 src_addr = (CmdParams[2] << 16) | CmdParams[3];
    u32 dst_addr = (CmdParams[4] << 16) | CmdParams[5];
    u32 len = (CmdParams[6] << 16) | CmdParams[7];

    for (u32 i = 0; i < len; i++)
    {
        s16 val16 = (s16)DSi.ARM9Read16(src_addr);
        s16 val8, xor8;

        val8 = val16 >> 2;
        if (val8 > 0)
        {
            xor8 = 0xFF;
        }
        else
        {
            val8 = (val8 ^ 0xFFFF) + 1;
            xor8 = 0x7F;
        }

        if (val8 > 0x1FDF)
            val8 = 0x1FDF;

        val8 += 0x21;

        int seg;
        for (seg = 0; seg < 8; seg++)
        {
            if (val8 <= seg_tbl[seg])
                break;
        }

        if (seg < 8)
        {
            s16 tmp = seg << 4;
            seg++;

            val8 = tmp | ((val8 >> seg) & 0xF);
        }
        else
            val8 = 0x7F;

        val8 ^= xor8;

        DSi.ARM9Write8(dst_addr, (s8)val8);
        src_addr += 2;
        dst_addr++;
    }
}

void G711Ucode::CmdDecodeALaw()
{
    u32 src_addr = (CmdParams[2] << 16) | CmdParams[3];
    u32 dst_addr = (CmdParams[4] << 16) | CmdParams[5];
    u32 len = (CmdParams[6] << 16) | CmdParams[7];

    for (u32 i = 0; i < len; i++)
    {
        s8 val8 = (s8)DSi.ARM9Read8(src_addr);
        s16 val16; s8 shift, ssign;

        val8 ^= 0x55;
        ssign = shift = val8;
        val16 = ((val8 & 0xF) << 4) + 8;
        shift = (shift >> 4) & 7;
        if (shift)
            val16 = (val16 + 0x100) << (shift - 1);
        if (ssign & 0x80)
            val16 = -val16;

        DSi.ARM9Write16(dst_addr, val16);
        src_addr++;
        dst_addr += 2;
    }
}

void G711Ucode::CmdDecodeULaw()
{
    u32 src_addr = (CmdParams[2] << 16) | CmdParams[3];
    u32 dst_addr = (CmdParams[4] << 16) | CmdParams[5];
    u32 len = (CmdParams[6] << 16) | CmdParams[7];

    for (u32 i = 0; i < len; i++)
    {
        s8 val8 = (s8)DSi.ARM9Read8(src_addr);
        s16 val16; s8 shift, ssign;

        val8 = ~val8;
        ssign = shift = val8;
        val16 = ((val8 & 0xF) << 3) + 0x84;
        shift = (shift >> 4) & 7;
        val16 = 0x84 - (val16 << shift);
        if (ssign & 0x80)
            val16 = -val16;

        DSi.ARM9Write16(dst_addr, val16);
        src_addr++;
        dst_addr += 2;
    }
}


}
}
