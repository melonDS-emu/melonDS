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
#include "AACUcode.h"
#include "../Platform.h"


namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace DSP_HLE
{


AACUcode::AACUcode(melonDS::DSi& dsi, int version) : UcodeBase(dsi)
{
    DSi.RegisterEventFuncs(Event_DSi_DSPHLE, this, {MakeEventThunk(AACUcode, FinishCmd)});

    if (version == -1)
        Log(LogLevel::Info, "DSP_HLE: initializing AAC decoder ucode (DSi sound app)\n");
    else
        Log(LogLevel::Info, "DSP_HLE: initializing AAC SDK ucode version %02X\n", version);
}

AACUcode::~AACUcode()
{
    DSi.UnregisterEventFuncs(Event_DSi_DSPHLE);
}

void AACUcode::Reset()
{
    UcodeBase::Reset();

    CmdState = 0;
    CmdIndex = 0;
    CmdParamCount = 0;
    memset(CmdParams, 0, sizeof(CmdParams));
}

void AACUcode::DoSavestate(Savestate *file)
{
    //
}


void AACUcode::SendData(u8 index, u16 val)
{
    UcodeBase::SendData(index, val);

    // CMD1 is used to send commands and parameters

    if (index == 1)
    {
        printf("-- CMD1 = %04X, state=%d cmd=%d count=%d\n", val, CmdState,CmdIndex, CmdParamCount);
        RecvCmdWord();
    }
    else if (index == 2)
    {
        CmdWritten[2] = false;
    }
}

void AACUcode::RecvCmdWord()
{
    u16 val = CmdReg[1];

    if (CmdState == 0)
    {
        if (val == 1)
        {
            CmdState = 1;
            CmdIndex = val;
            CmdParamCount = 0;
        }
    }
    else if (CmdState == 1)
    {
        CmdParams[CmdParamCount] = val;
        CmdParamCount++;

        if (CmdParamCount == 10)
        {
            // we received all the parameter words, schedule the command
            // 115000 cycles is the average of the time it takes on hardware
            // might be different depending on sample rate etc

            CmdState = 2;

            // TODO actually decode shit

            DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 115000, 0, 0);
        }
    }
    else
        return;

    CmdWritten[1] = false;
}

void AACUcode::FinishCmd(u32 param)
{
    CmdState = 0;
    CmdParamCount = 0;

    SendReply(0, param);
}


}
}
