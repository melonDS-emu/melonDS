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

// TODO move faad stuff to Platform
#include <neaacdec.h>

#include "../DSi.h"
#include "AACUcode.h"
#include "../Platform.h"


namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace DSP_HLE
{

NeAACDecHandle AACDec;


AACUcode::AACUcode(melonDS::DSi& dsi, int version) : UcodeBase(dsi)
{
    DSi.RegisterEventFuncs(Event_DSi_DSPHLE, this, {MakeEventThunk(AACUcode, FinishCmd)});

    AACDec = NeAACDecOpen();

    NeAACDecConfiguration* cfg = NeAACDecGetCurrentConfiguration(AACDec);
    cfg->defObjectType = LC;
    cfg->defSampleRate = 48000;
    cfg->outputFormat = FAAD_FMT_16BIT;
    NeAACDecSetConfiguration(AACDec, cfg);

    /*unsigned long freq = 48000;
    unsigned char chan = 2;
    int res = NeAACDecInit(AACDec, nullptr, 234, &freq, &chan);
    printf("init = %d\n", res);*/

    if (version == -1)
        Log(LogLevel::Info, "DSP_HLE: initializing AAC decoder ucode (DSi sound app)\n");
    else
        Log(LogLevel::Info, "DSP_HLE: initializing AAC SDK ucode version %02X\n", version);
}

AACUcode::~AACUcode()
{
    NeAACDecClose(AACDec);

    DSi.UnregisterEventFuncs(Event_DSi_DSPHLE);
}

void AACUcode::Reset()
{
    UcodeBase::Reset();

    CmdState = 0;
    CmdIndex = 0;
    CmdParamCount = 0;
    memset(CmdParams, 0, sizeof(CmdParams));

    memset(FrameBuf, 0, sizeof(FrameBuf));
    memset(LeftOutput, 0, sizeof(LeftOutput));
    memset(RightOutput, 0, sizeof(RightOutput));
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
int pett = 0;
void AACUcode::CmdDecodeFrame()
{
    u16 framelen = CmdParams[0];
    u32 freq = (CmdParams[1] << 16) | CmdParams[2];
    u16 chan = CmdParams[3];
    u32 frameaddr = (CmdParams[4] << 16) | CmdParams[5];
    u32 leftaddr = (CmdParams[6] << 16) | CmdParams[7];
    u32 rightaddr = (CmdParams[8] << 16) | CmdParams[9];

    printf("AAC: len=%d freq=%d chan=%d in=%08X out=%08X/%08X\n",
           framelen, freq, chan, frameaddr, leftaddr, rightaddr);

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

    // check input frequency
    // this isn't entirely accurate
    // in the ucode, any frequency not within the list below causes an init failure
    // but the return value from the init function is ignored
    u32 freqlist[9] = {48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};
    u8 freqnum = 0xF;
    for (int i = 0; i < 9; i++)
    {
        if (freq == freqlist[i])
        {
            freqnum = 3 + i;
            break;
        }
    }
    if (freqnum == 0xF)
        fail = true;

    if (fail)
    {
        // end the command with return code 1 (invalid parameters)
        DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 256, 0, 1);
        return;
    }

    printf("AAC: good, freq=%d\n", freqnum);

    // make an ADTS header
    /*
     * u32 adts[2];
        int freq = 4;
        int ch = 2;
        int framelen = fs & 0x1FFF;
        int resv = 0x7FF;
        databotte[0] = 0xFF;
        databotte[1] = 0xF1;
        databotte[2] = 0x40 | (freq << 2) | (ch >> 2); // freq
        databotte[3] = (ch << 6) | (fs >> 11);
        databotte[4] = (framelen >> 3);
        databotte[5] = (framelen << 5) | (resv >> 6);
        databotte[6] = (resv << 2);*/
    u32 totallen = framelen + 7;
    u32 rsv = 0x7FF;
    FrameBuf[0] = 0xFF;
    FrameBuf[1] = 0xF1;
    FrameBuf[2] = 0x40 | (freqnum << 2) | (chan >> 2);
    FrameBuf[3] = (chan << 6) | (totallen >> 11);
    FrameBuf[4] = (totallen >> 3);
    FrameBuf[5] = (totallen << 5) | (rsv >> 6);
    FrameBuf[6] = (rsv << 2);

#define databotte FrameBuf
    printf("%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
           databotte[0],databotte[1],databotte[2],databotte[3],
           databotte[4],databotte[5],databotte[6]);

    // read frame data
    //ReadARM9Mem((u16*)&FrameBuf[7], frameaddr, framelen);
    for (int i = 0; i < framelen; i++)
    {
        FrameBuf[7+i] = DSi.ARM9Read8(frameaddr + i);
    }

    // init
    // TODO only do if config changed!
    if (pett < 2)
    {
        if (pett == 1)
        {
            unsigned long _freq = 0;
            unsigned char _chan = 0;
            int res = NeAACDecInit(AACDec, FrameBuf, totallen, &_freq, &_chan);
            printf("init = %d, %ld, %d\n", res, _freq, _chan);
        }
        pett++;
    }

    // decode
    NeAACDecFrameInfo finfo;
    /*void* samplebuf[2] = {LeftOutput, RightOutput};
    NeAACDecDecode2(AACDec, &finfo, FrameBuf, totallen, samplebuf, 1024*4);
    printf("decode res = %d %d %d\n", finfo.error, finfo.bytesconsumed, finfo.samples);

    WriteARM9Mem((u16*)LeftOutput, leftaddr, 1024*2);
    WriteARM9Mem((u16*)RightOutput, rightaddr, 1024*2); // checkme*/
    s16* dataout = (s16*)NeAACDecDecode(AACDec, &finfo, FrameBuf, totallen);
    printf("decode res = %p %d %d/%d %d\n", dataout, finfo.error, (int)finfo.bytesconsumed, totallen, (int)finfo.samples);
    if (dataout)
    {
        for (int i = 0; i < 1024; i++)
        {
            DSi.ARM9Write16(leftaddr, *dataout++);
            DSi.ARM9Write16(rightaddr, *dataout++);
            leftaddr += 2;
            rightaddr += 2;
        }
    }

    DSi.ScheduleEvent(Event_DSi_DSPHLE, false, 115000, 0, 0);
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
