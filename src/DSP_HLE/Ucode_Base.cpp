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
    //
}

void DSPHLE_UcodeBase::DoSavestate(Savestate *file)
{
    //
}

bool DSPHLE_UcodeBase::SendDataIsEmpty(u8 index)
{
    //
    return false;
}

bool DSPHLE_UcodeBase::RecvDataIsEmpty(u8 index)
{
    //
    return false;
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

u16 DSPHLE_UcodeBase::RecvData(u8 index)
{
    //
    return 0;
}

void DSPHLE_UcodeBase::SendData(u8 index, u16 val)
{
    //
}

void DSPHLE_UcodeBase::Run(u32 cycles)
{
    //
}


}