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
#include "../DSi_DSP.h"
#include "Ucode_Base.h"
#include "../Platform.h"


namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;


DSPHLE_UcodeBase::DSPHLE_UcodeBase(melonDS::DSi& dsi) : DSi(dsi)
{
    DSi.RegisterEventFuncs(Event_DSi_DSPHLE, this, {MakeEventThunk(DSPHLE_UcodeBase, OnUcodeCmdFinish)});
}

DSPHLE_UcodeBase::~DSPHLE_UcodeBase()
{
    //
}

void DSPHLE_UcodeBase::Reset()
{
    DataMemory = nullptr;

    memset(CmdReg, 0, sizeof(CmdReg));
    memset(CmdWritten, 0, sizeof(CmdWritten));
    memset(ReplyReg, 0, sizeof(ReplyReg));
    memset(ReplyWritten, 0, sizeof(ReplyWritten));
    //memset(ReplyReadCb, 0, sizeof(ReplyReadCb));
    ReplyReadCb[0] = nullptr;
    ReplyReadCb[1] = nullptr;
    ReplyReadCb[2] = nullptr;

    SemaphoreIn = 0;
    SemaphoreOut = 0;
    SemaphoreMask = 0;

    UcodeCmd = 0;
}

void DSPHLE_UcodeBase::DoSavestate(Savestate *file)
{
    //
}


bool DSPHLE_UcodeBase::RecvDataIsReady(u8 index)
{
    return ReplyWritten[index];
}

bool DSPHLE_UcodeBase::SendDataIsEmpty(u8 index)
{
    return !CmdWritten[index];
}

u16 DSPHLE_UcodeBase::RecvData(u8 index)
{
    if (!ReplyWritten[index]) printf("DSP: receive reply%d but empty\n", index);
    if (!ReplyWritten[index]) return 0; // CHECKME

    u16 ret = ReplyReg[index];
    ReplyWritten[index] = false;
printf("DSP: receive reply%d %04X\n", index, ret);
    if (ReplyReadCb[index])
    {
        ReplyReadCb[index]();
        ReplyReadCb[index] = nullptr;
    }

    return ret;
}

void DSPHLE_UcodeBase::SendData(u8 index, u16 val)
{
    // TODO less ambiguous naming for those functions
    if (CmdWritten[index])
    {
        printf("??? trying to write cmd but there's already one\n");
        return; // CHECKME
    }

    CmdReg[index] = val;
    CmdWritten[index] = true;
    printf("DSP: send cmd%d %04X\n", index, val);

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


void DSPHLE_UcodeBase::SendReply(u8 index, u16 val)
{
    if (ReplyWritten[index])
    {
        printf("??? trying to write reply but there's already one\n");
        return;
    }

    ReplyReg[index] = val;
    ReplyWritten[index] = true;

    switch (index)
    {
    case 0: DSi.DSP.IrqRep0(); break;
    case 1: DSi.DSP.IrqRep1(); break;
    case 2: DSi.DSP.IrqRep2(); break;
    }
}

void DSPHLE_UcodeBase::SetReplyReadCallback(u8 index, fnReplyReadCb callback)
{
    ReplyReadCb[index] = callback;
}


u16 DSPHLE_UcodeBase::DMAChan0GetDstHigh()
{
    // TODO?
    return 0;
}

u16 DSPHLE_UcodeBase::AHBMGetDmaChannel(u16 index)
{
    //
    return 0;
}

u16 DSPHLE_UcodeBase::AHBMGetDirection(u16 index)
{
    //
    return 0;
}

u16 DSPHLE_UcodeBase::AHBMGetUnitSize(u16 index)
{
    //
    return 0;
}

u16 DSPHLE_UcodeBase::DataReadA32(u32 addr)
{
    printf("ucode: DataReadA32 %08X\n", addr);

    addr <<= 1;
    /*if (!(addr & 0x40000))
    {
        u8* ptr = DSi.NWRAMMap_B[2][(addr >> 15) & 0x7];
        return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
    }
    else*/
    {
        u8* ptr = DSi.NWRAMMap_C[2][(addr >> 15) & 0x7];
        return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
    }
}

void DSPHLE_UcodeBase::DataWriteA32(u32 addr, u16 val)
{
    printf("ucode: DataWriteA32 %08X %04X\n", addr, val);

    addr <<= 1;
    /*if (!(addr & 0x40000))
    {
        u8* ptr = DSi.NWRAMMap_B[2][(addr >> 15) & 0x7];
        if (ptr) *(u16*)&ptr[addr & 0x7FFF] = val;
    }
    else*/
    {
        u8* ptr = DSi.NWRAMMap_C[2][(addr >> 15) & 0x7];
        if (ptr) *(u16*)&ptr[addr & 0x7FFF] = val;
    }
}

u16 DSPHLE_UcodeBase::MMIORead(u16 addr)
{
    //
    return 0;
}

void DSPHLE_UcodeBase::MMIOWrite(u16 addr, u16 val)
{
    //
}

u16 DSPHLE_UcodeBase::ProgramRead(u32 addr)
{
    //
    return 0;
}

void DSPHLE_UcodeBase::ProgramWrite(u32 addr, u16 val)
{
    //
}

u16 DSPHLE_UcodeBase::AHBMRead16(u32 addr)
{
    //
    return 0;
}

u32 DSPHLE_UcodeBase::AHBMRead32(u32 addr)
{
    //
    return 0;
}

void DSPHLE_UcodeBase::AHBMWrite16(u32 addr, u16 val)
{
    //
}

void DSPHLE_UcodeBase::AHBMWrite32(u32 addr, u32 val)
{
    //
}

u16 DSPHLE_UcodeBase::GetSemaphore()
{
    return SemaphoreOut;
}

void DSPHLE_UcodeBase::SetSemaphore(u16 val)
{
    SemaphoreIn |= val;
}

void DSPHLE_UcodeBase::ClearSemaphore(u16 val)
{
    SemaphoreOut &= ~val;
}

void DSPHLE_UcodeBase::MaskSemaphore(u16 val)
{
    SemaphoreMask = val;
}

void DSPHLE_UcodeBase::SetSemaphoreOut(u16 val)
{
    SemaphoreOut |= val;
    if (SemaphoreOut & (~SemaphoreMask))
        DSi.DSP.IrqSem();
}


void DSPHLE_UcodeBase::Start()
{
    printf("DSP HLE: start\n");
    // TODO later: detect which ucode it is and create the right class!
    // (and fall back to Teakra if not a known ucode)

    const u16 pipeaddr = 0x0800;
    u16* mem = (u16*)DSi.NWRAMMap_C[2][0];

    // initialize pipe structure
    u16* pipe = &mem[pipeaddr];
    for (int i = 0; i < 16; i++)
    {
        *pipe++ = 0x1000 + (0x100 * i);  // buffer address
        *pipe++ = 0x200;                 // length in bytes
        *pipe++ = 0;                     // read pointer
        *pipe++ = 0;                     // write pointer
        *pipe++ = i;                     // pipe index
    }

    SendReply(0, 1);
    SendReply(1, 1);
    SendReply(2, 1);
    SetReplyReadCallback(2, [=]()
    {
        printf("reply 2 was read\n");

        SendReply(2, pipeaddr);
        SetSemaphoreOut(0x8000);
    });

    // TODO more shit
}


void DSPHLE_UcodeBase::Run(u32 cycles)
{
    //
}


u16* DSPHLE_UcodeBase::LoadPipe(u8 index)
{
    const u16 pipeaddr = 0x0800;
    u16* mem = (u16*)DSi.NWRAMMap_C[2][0];

    u16* pipe = &mem[pipeaddr + (index * 5)];
    return pipe;
}

u32 DSPHLE_UcodeBase::GetPipeLength(u16* pipe)
{
    u32 ret;
    u16 rdptr = pipe[2];
    u16 wrptr = pipe[3];
    if (rdptr > wrptr)
    {
        u16 len = pipe[1];
        ret = wrptr + len - rdptr;
    }
    else
    {
        ret = wrptr - rdptr;
    }

    if (ret & 1) printf("HUH? pipe length is odd? (%d)\n", ret);
    return ret >> 1;
}

u32 DSPHLE_UcodeBase::ReadPipe(u16* pipe, u16* data, u32 len)
{
    u16* mem = (u16*)DSi.NWRAMMap_C[2][0];
    u16* pipebuf = &mem[pipe[0]];
    u16 pipelen = pipe[1] >> 1;
    u16 rdptr = pipe[2] >> 1;
    u16 wrptr = pipe[3] >> 1;
    printf("readpipe(%d): len=%d rd=%d wr=%d\n", len, pipelen, rdptr, wrptr);
    u32 rdlen = 0;
    for (int i = 0; i < len; i++)
    {
        data[i] = pipebuf[rdptr++];
        rdlen++;
        if (rdptr >= pipelen) rdptr = 0;
        if (rdptr == wrptr) break;
    }
printf("-> rd=%d\n", rdptr);
    pipe[2] = rdptr << 1;
    SendReply(2, pipe[4]);
    SetSemaphoreOut(0x8000);

    return rdlen;
}

void DSPHLE_UcodeBase::RunUcodeCmd()
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

void DSPHLE_UcodeBase::OnUcodeCmdFinish(u32 param)
{
    printf("finish cmd %d, param=%d, %d/%d\n", UcodeCmd, param, CmdWritten[2], ReplyWritten[2]);
    UcodeCmd = 0;
    SendReply(1, (u16)param);
}

void DSPHLE_UcodeBase::UcodeCmd_Scaling(u16* pipe)
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
//printf("scale %08X -> %08X, %dx%d %dx%d %dx%d\n", src_addr, dst_addr, src_width, src_height, rect_width, rect_height, dst_width, dst_height);
    for (u32 y = 0; y < dst_height; y++)
    {
        u32 sy = ((y * y_factor) + 0x3FF) >> 10;
        u32 src_line = src_addr + ((sy * src_width) << 1);
//printf("line %d->%d %d %08X\n", y, sy, src_width, src_line);
        for (u32 x = 0; x < dst_width; x++)
        {
            u32 sx = ((x * x_factor) + 0x3FF) >> 10;

            u16 v = DSi.ARM9Read16(src_line + (sx << 1));
            DSi.ARM9Write16(dst_addr, v);
            //printf("%d,%d %08X -> %08X\n", y, x, src_line+(sx<<1),dst_addr);
            dst_addr += 2;
        }

        //src_addr += (src_width << 1);
    }

    // TODO the rest of the shit!!

    // TODO add a delay to this
    // TODO make the delay realistic
    //SendReply(1, 1);
    DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 600000, 0, 1);
}


}