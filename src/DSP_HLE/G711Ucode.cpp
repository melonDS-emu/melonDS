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
    DSi.RegisterEventFuncs(Event_DSi_DSPHLE, this, {MakeEventThunk(G711Ucode, FinishCmd)});

    Log(LogLevel::Info, "DSP_HLE: initializing G711 SDK ucode version %02X\n", version);
}

G711Ucode::~G711Ucode()
{
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
    //
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
    u32 len = (CmdParams[6] << 16) | CmdParams[7];
    printf("G711 CMD LEN %08X\n", len);

    // gross estimation
    // TODO not going to be right for all command types
    u32 cmdtime = 14 * len;

    CmdState = 1;
    DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 200 + cmdtime, 0, 0);
}

void G711Ucode::FinishCmd(u32 param)
{
    if (CmdState != 1)
        return;

    // TODO: execute command here

    // response = processed length
    u16* pipe = LoadPipe(6);
    u16 resp[2] = {CmdParams[6], CmdParams[7]};
    WritePipe(pipe, resp, 2);

    CmdState = 0;
    TryStartCmd();
}


//


}
}
