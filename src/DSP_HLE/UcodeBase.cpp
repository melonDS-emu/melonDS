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
#include "UcodeBase.h"
#include "../Platform.h"


namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace DSP_HLE
{


UcodeBase::UcodeBase(melonDS::DSi& dsi) : DSi(dsi)
{
    DSi.RegisterEventFuncs(Event_DSi_DSPHLE, this, {MakeEventThunk(UcodeBase, OnUcodeCmdFinish)});
}

UcodeBase::~UcodeBase()
{
    //
}

void UcodeBase::Reset()
{
    DataMemory = nullptr;

    memset(CmdReg, 0, sizeof(CmdReg));
    memset(CmdWritten, 0, sizeof(CmdWritten));
    memset(ReplyReg, 0, sizeof(ReplyReg));
    memset(ReplyWritten, 0, sizeof(ReplyWritten));
    ReplyReadCb[0] = nullptr;
    ReplyReadCb[1] = nullptr;
    ReplyReadCb[2] = nullptr;

    SemaphoreIn = 0;
    SemaphoreOut = 0;
    SemaphoreMask = 0;

    UcodeCmd = 0;
}

void UcodeBase::DoSavestate(Savestate *file)
{
    // TODO
}


bool UcodeBase::RecvDataIsReady(u8 index) const
{
    return ReplyWritten[index];
}

bool UcodeBase::SendDataIsEmpty(u8 index) const
{
    return !CmdWritten[index];
}

u16 UcodeBase::RecvData(u8 index)
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

void UcodeBase::SendData(u8 index, u16 val)
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

    // extra shit shall be implemented in subclasses
}


void UcodeBase::SendReply(u8 index, u16 val)
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

void UcodeBase::SetReplyReadCallback(u8 index, fnReplyReadCb callback)
{
    ReplyReadCb[index] = callback;
}


u16 UcodeBase::DMAChan0GetSrcHigh()
{
    // TODO?
    return 0;
}

u16 UcodeBase::DMAChan0GetDstHigh()
{
    // TODO?
    return 0;
}

u16 UcodeBase::AHBMGetDmaChannel(u16 index) const
{
    //
    return 0;
}

u16 UcodeBase::AHBMGetDirection(u16 index) const
{
    //
    return 0;
}

u16 UcodeBase::AHBMGetUnitSize(u16 index) const
{
    //
    return 0;
}

u16 UcodeBase::DataReadA32(u32 addr) const
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

void UcodeBase::DataWriteA32(u32 addr, u16 val)
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

u16 UcodeBase::MMIORead(u16 addr)
{
    //
    return 0;
}

void UcodeBase::MMIOWrite(u16 addr, u16 val)
{
    //
}

u16 UcodeBase::ProgramRead(u32 addr) const
{
    //
    return 0;
}

void UcodeBase::ProgramWrite(u32 addr, u16 val)
{
    //
}

u16 UcodeBase::AHBMRead16(u32 addr)
{
    //
    return 0;
}

u16 UcodeBase::AHBMRead32(u32 addr)
{
    //
    return 0;
}

void UcodeBase::AHBMWrite16(u32 addr, u16 val)
{
    //
}

void UcodeBase::AHBMWrite32(u32 addr, u32 val)
{
    //
}

u16 UcodeBase::GetSemaphore() const
{
    return SemaphoreOut;
}

void UcodeBase::SetSemaphore(u16 val)
{
    SemaphoreIn |= val;
}

void UcodeBase::ClearSemaphore(u16 val)
{
    SemaphoreOut &= ~val;
}

void UcodeBase::MaskSemaphore(u16 val)
{
    SemaphoreMask = val;
}

void UcodeBase::SetSemaphoreOut(u16 val)
{
    SemaphoreOut |= val;
    if (SemaphoreOut & (~SemaphoreMask))
        DSi.DSP.IrqSem();
}


void UcodeBase::Start()
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


u16* UcodeBase::LoadPipe(u8 index)
{
    const u16 pipeaddr = 0x0800;
    u16* mem = (u16*)DSi.NWRAMMap_C[2][0];

    u16* pipe = &mem[pipeaddr + (index * 5)];
    return pipe;
}

u32 UcodeBase::GetPipeLength(u16* pipe)
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

u32 UcodeBase::ReadPipe(u16* pipe, u16* data, u32 len)
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

void UcodeBase::RunUcodeCmd()
{
    u16* pipe = LoadPipe(7);
    u32 len = GetPipeLength(pipe);
printf("try to run ucode cmd: cmd=%d, len=%d\n", UcodeCmd, len);
    //

    //UcodeCmd = 0;
}

void UcodeBase::OnUcodeCmdFinish(u32 param)
{
    printf("finish cmd %d, param=%d, %d/%d\n", UcodeCmd, param, CmdWritten[2], ReplyWritten[2]);
    UcodeCmd = 0;
    SendReply(1, (u16)param);
}


}
}
