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


DSPHLE_UcodeBase::DSPHLE_UcodeBase()
{
    //
}

DSPHLE_UcodeBase::~DSPHLE_UcodeBase()
{
    //
}

void DSPHLE_UcodeBase::Reset()
{
    memset(CmdReg, 0, sizeof(CmdReg));
    memset(CmdWritten, 0, sizeof(CmdWritten));
    memset(ReplyReg, 0, sizeof(ReplyReg));
    memset(ReplyWritten, 0, sizeof(ReplyWritten));
    memset(ReplyReadCb, 0, sizeof(ReplyReadCb));
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
    if (!ReplyWritten[index]) return 0; // CHECKME

    u16 ret = ReplyReg[index];
    ReplyWritten[index] = false;

    if (ReplyReadCb[index])
    {
        ReplyReadCb[index]();
        ReplyReadCb[index] = nullptr;
    }

    return ret;
}

void DSPHLE_UcodeBase::SendData(u8 index, u16 val)
{
    if (CmdWritten[index])
    {
        printf("??? trying to write cmd but there's already one\n");
        return; // CHECKME
    }

    CmdReg[index] = val;
    CmdWritten[index] = true;
    printf("DSP: send cmd%d %04X\n", index, val);
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

    // TODO add callback for when it is successfully written
}

void DSPHLE_UcodeBase::SetReplyReadCallback(u8 index, fnReplyReadCb callback)
{
    ReplyReadCb[index] = callback;
}


u16 DSPHLE_UcodeBase::DMAChan0GetDstHigh()
{
    //
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
    //
    return 0;
}

void DSPHLE_UcodeBase::DataWriteA32(u32 addr, u16 val)
{
    //
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
    //
    return 0;
}

void DSPHLE_UcodeBase::SetSemaphore(u16 val)
{
    //
}

void DSPHLE_UcodeBase::ClearSemaphore(u16 val)
{
    //
}

void DSPHLE_UcodeBase::MaskSemaphore(u16 val)
{
    //
}


void DSPHLE_UcodeBase::Start()
{
    printf("DSP HLE: start\n");
    // TODO later: detect which ucode it is and create the right class!
    // (and fall back to Teakra if not a known ucode)

    SendReply(0, 1);
    SendReply(1, 1);
    SendReply(2, 1);
    SetReplyReadCallback(2, [=]()
    {
        printf("reply 2 was read\n");

        SendReply(2, 0x0800);
    });

    // TODO more shit
}


void DSPHLE_UcodeBase::Run(u32 cycles)
{
    //
}


}