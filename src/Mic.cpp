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

#include "NDS.h"
#include "DSi.h"
#include "Mic.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

// Microphone control hub
// -
// There are three different ways to access the microphone:
// * DS: through the TSC (reading the AUX input returns the current mic level)
// * DSi: through the mic interface at 0x04004600
// * DSi: through the DSP (via BTDMP 0)
//
// This module serves to centralize microphone control functions and keep the sound buffer.


Mic::Mic(melonDS::NDS& nds) : NDS(nds)
{
}

Mic::~Mic()
{
}

void Mic::Reset()
{
    StopAll();

    memset(InputBuffer, 0, sizeof(InputBuffer));
    InputBufferWritePos = 0;
    InputBufferReadPos = 0;
    InputBufferLevel = 0;

    CycleCount = 0;
    CurSample = 0;
    StopMask = 0;
    for (int i = 0; i < 3; i++)
        StopCount[i] = 0;
}

void Mic::DoSavestate(melonDS::Savestate *file)
{
    file->Section("MIC.");

    file->Var8(&OpenMask);
    file->Var32(&CycleCount);
    file->Var16((u16*)&CurSample);
    file->Var8(&StopMask);
    file->VarArray(StopCount, sizeof(StopCount));

    if (!file->Saving)
    {
        if (OpenMask)
        {
            memset(InputBuffer, 0, sizeof(InputBuffer));
            InputBufferWritePos = 0;
            InputBufferReadPos = 0;
            InputBufferLevel = 0;

            Platform::Mic_Start(NDS.UserData);
        }
        else
            Platform::Mic_Stop(NDS.UserData);
    }
}


void Mic::Start(MicSource source)
{
    if (source == Mic_NDS)
        StopMask |= (1<<source);
    else
        StopMask &= ~(1<<source);
    StopCount[source] = 0;

    if (OpenMask & (1<<source))
        return;

    if (!OpenMask)
    {
        memset(InputBuffer, 0, sizeof(InputBuffer));
        InputBufferWritePos = 0;
        InputBufferReadPos = 0;
        InputBufferLevel = 0;

        Platform::Mic_Start(NDS.UserData);
    }
    OpenMask |= (1<<source);
}

void Mic::Stop(MicSource source)
{
    if (!(OpenMask & (1<<source)))
        return;

    StopMask |= (1<<source);
    StopCount[source] = 0;
}

void Mic::DoStop(MicSource source)
{
    if (!(OpenMask & StopMask & (1<<source)))
        return;

    OpenMask &= ~(1<<source);
    StopMask &= ~(1<<source);
    StopCount[source] = 0;
    if (!OpenMask)
        Platform::Mic_Stop(NDS.UserData);
}

void Mic::StopAll()
{
    if (!OpenMask)
        return;

    OpenMask = 0;
    StopMask = 0;
    for (int i = 0; i < 3; i++)
        StopCount[i] = 0;

    Platform::Mic_Stop(NDS.UserData);
}


void Mic::FeedBuffer()
{
    // try to fill 3/4 of the mic buffer, but only take as much as the platform can provide

    int writelen = 3 * (InputBufferSize >> 2);
    while (writelen > 0)
    {
        int thislen = writelen;
        if ((InputBufferWritePos + thislen) > InputBufferSize)
            thislen = InputBufferSize - InputBufferWritePos;

        int actuallen = Platform::Mic_ReadInput(&InputBuffer[InputBufferWritePos], thislen, NDS.UserData);
        if (!actuallen)
            break;

        if (actuallen > thislen)
            actuallen = thislen;
        InputBufferLevel += actuallen;
        InputBufferWritePos += actuallen;
        if (InputBufferWritePos >= InputBufferSize)
            InputBufferWritePos -= InputBufferSize;

        if (actuallen < thislen)
            break;
        writelen -= actuallen;
    }
}


void Mic::Advance(u32 cycles)
{
    // the mic feed is 47.6 KHz, ie. one sample every 704 cycles
    // this matches the highest sample rate on DSi
    // TODO eventually: interpolation?

    if (!OpenMask)
    {
        CycleCount = 0;
        return;
    }

    CycleCount += cycles;
    while (CycleCount >= 704)
    {
        CycleCount -= 704;

        if (InputBufferLevel < (InputBufferSize >> 2))
            FeedBuffer();

        if (InputBufferLevel == 0)
            continue;

        InputBufferLevel--;
        CurSample = InputBuffer[InputBufferReadPos];
        InputBufferReadPos++;
        if (InputBufferReadPos >= InputBufferSize)
            InputBufferReadPos = 0;
    }

    // NDS mic input has no explicit start/stop control
    // so this counter will turn it off if the TSC AUX input doesn't get sampled for one video frame
    // other sources also get similar counters, in case we are dealing with stupid code that keeps
    // turning the mic on and off (hi DSi-mode libnds)
    for (int i = 0; i < 3; i++)
    {
        StopCount[i] += cycles;
        if (StopCount[i] >= 560190)
            DoStop((MicSource)i);
    }
}

s16 Mic::ReadSample()
{
    if (!OpenMask) return 0;
    return CurSample;
}


}
