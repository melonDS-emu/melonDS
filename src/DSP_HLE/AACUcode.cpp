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
    UcodeClass = Class_AAC;
    UcodeVersion = version;
    DSi.RegisterEventFuncs(Event_DSi_DSPHLE, this, {MakeEventThunk(AACUcode, FinishCmd)});

    Decoder = Platform::AAC_Init();
    if (!Decoder)
        Log(LogLevel::Error, "DSP_HLE: failed to initialize AAC decoder\n");

    if (version == -1)
        Log(LogLevel::Info, "DSP_HLE: initializing AAC decoder ucode (DSi sound app)\n");
    else
        Log(LogLevel::Info, "DSP_HLE: initializing AAC SDK ucode version %02X\n", version);
}

AACUcode::~AACUcode()
{
    if (Decoder)
        Platform::AAC_DeInit(Decoder);

    DSi.CancelEvent(Event_DSi_DSPHLE);
    DSi.UnregisterEventFuncs(Event_DSi_DSPHLE);
}

void AACUcode::Reset()
{
    UcodeBase::Reset();

    CmdState = 0;
    CmdIndex = 0;
    CmdParamCount = 0;
    memset(CmdParams, 0, sizeof(CmdParams));

    memset(InputBuf, 0, sizeof(InputBuf));
    memset(OutputBuf, 0, sizeof(OutputBuf));

    LastFrequency = -1;
    LastChannels = -1;
}

void AACUcode::DoSavestate(Savestate *file)
{
    UcodeBase::DoSavestate(file);

    file->Var8(&CmdState);
    file->Var16(&CmdIndex);
    file->Var8(&CmdParamCount);
    file->VarArray(CmdParams, sizeof(CmdParams));

    if (!file->Saving)
    {
        LastFrequency = -1;
        LastChannels = -1;
    }
}


void AACUcode::SendData(u8 index, u16 val)
{
    UcodeBase::SendData(index, val);

    // CMD1 is used to send commands and parameters

    if (index == 1)
    {
        //printf("-- CMD1 = %04X, state=%d cmd=%d count=%d\n", val, CmdState,CmdIndex, CmdParamCount);
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
            CmdDecodeFrame();
        }
    }
    else
        return;

    CmdWritten[1] = false;
}

void AACUcode::CmdDecodeFrame()
{
    u16 framelen = CmdParams[0];
    u32 freq = (CmdParams[1] << 16) | CmdParams[2];
    u16 chan = CmdParams[3];
    u32 frameaddr = (CmdParams[4] << 16) | CmdParams[5];
    u32 leftaddr = (CmdParams[6] << 16) | CmdParams[7];
    u32 rightaddr = (CmdParams[8] << 16) | CmdParams[9];

    //printf("AAC: len=%d freq=%d chan=%d in=%08X out=%08X/%08X\n",
    //       framelen, freq, chan, frameaddr, leftaddr, rightaddr);

    // verify the parameters
    bool fail = false;

    if ((framelen == 0) || (framelen > 1700))
        fail = true;
    if ((freq == 0) || (freq > 48000))
        fail = true;
    if ((chan != 1) && (chan != 2))
        fail = true;
    if (frameaddr == 0)
        fail = true;
    if (leftaddr == 0)
        fail = true;
    if ((chan != 1) && (rightaddr == 0))
        fail = true;

    if (fail)
    {
        // end the command with return code 1 (invalid parameters)
        DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 256, 0, 1);
        return;
    }

    // TODO more efficient read mechanism?
    // ReadARM9Mem() doesn't work well for this
    for (int i = 0; i < framelen; i++)
    {
        InputBuf[i] = DSi.ARM9Read8(frameaddr + i);
    }

    // NOTE
    // the DSi sound app will first send an all-zero frame, then send the actual first AAC frame
    // this seems to just be a bug in the sound app
    // the AAC ucode will fail to decode the zero frame, but without consequences
    // however, third-party AAC decoders do not like zero frames
    // so we need to detect this and bail out early

    if ((InputBuf[0] == 0) && (InputBuf[1] == 0) && (InputBuf[2] == 0) && (InputBuf[3] == 0))
    {
        Log(LogLevel::Warn, "DSP_HLE: skipping zero AAC frame, addr=%08X len=%d\n", frameaddr, framelen);

        DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 512, 0, 2);
        return;
    }

    // initialize the decoder if needed

    if ((freq != LastFrequency) || (chan != LastChannels))
    {
        if (!Platform::AAC_Configure(Decoder, freq, chan))
        {
            Log(LogLevel::Warn, "DSP_HLE: AAC decoder configuration failed, freq=%d chan=%d\n", freq, chan);

            LastFrequency = -1;
            LastChannels = -1;
            DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 512, 0, 2);
            return;
        }

        LastFrequency = freq;
        LastChannels = chan;
    }

    // decode the frame

    if (!Platform::AAC_DecodeFrame(Decoder, InputBuf, framelen, OutputBuf, 1024*2*sizeof(s16)))
    {
        Log(LogLevel::Warn, "DSP_HLE: AAC decoding failed, frame addr=%08X len=%d\n", frameaddr, framelen);

        LastFrequency = -1;
        LastChannels = -1;
        DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 512, 0, 2);
        return;
    }

    s16* dataout = OutputBuf;
    if (chan == 1)
    {
        for (int i = 0; i < 1024; i++)
        {
            DSi.ARM9Write16(leftaddr, *dataout++);
            dataout++;
            leftaddr += 2;
        }
    }
    else
    {
        for (int i = 0; i < 1024; i++)
        {
            DSi.ARM9Write16(leftaddr, *dataout++);
            DSi.ARM9Write16(rightaddr, *dataout++);
            leftaddr += 2;
            rightaddr += 2;
        }
    }

    DSi.ScheduleEvent(Event_DSi_DSPHLE, false, (chan==1) ? 60000 : 115000, 0, 0);
}

void AACUcode::FinishCmd(u32 param)
{
    CmdState = 0;
    CmdParamCount = 0;

    SendReply(0, param);

    if (CmdWritten[1])
        RecvCmdWord();
}


}
}
