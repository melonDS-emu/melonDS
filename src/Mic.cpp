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
}

void Mic::DoSavestate(melonDS::Savestate *file)
{
    //
}


void Mic::Start(MicSource source)
{
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

    // TODO set up something to auto stop NDS mic
}

void Mic::Stop(MicSource source)
{
    if (!(OpenMask & (1<<source)))
        return;

    OpenMask &= ~(1<<source);
    if (!OpenMask)
        Platform::Mic_Stop(NDS.UserData);
}

void Mic::StopAll()
{
    if (!OpenMask)
        return;

    OpenMask = 0;
    Platform::Mic_Stop(NDS.UserData);
}


void Mic::FeedBuffer()
{
    // try to fill half the mic buffer, but only take as much as the platform can provide

    int writelen = InputBufferSize >> 1;
    while (writelen > 0)
    {
        int thislen = writelen;
        if ((InputBufferWritePos + thislen) > InputBufferSize)
            thislen = InputBufferSize - InputBufferWritePos;

        int actuallen = Platform::Mic_ReadInput(&InputBuffer[InputBufferWritePos], thislen, NDS.UserData);
        if (!actuallen)
            break;

        InputBufferLevel += actuallen;
        InputBufferWritePos += actuallen;
        if (InputBufferWritePos >= InputBufferSize)
            InputBufferWritePos -= InputBufferSize;

        if (InputBufferLevel > InputBufferSize)
            printf("AAAAAAAAAAAAAAAAAAA %d %d\n", InputBufferLevel, InputBufferSize);

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

        if (InputBufferLevel < (InputBufferSize >> 1))
            FeedBuffer();

        if (InputBufferLevel == 0) printf("UNDERRUN\n");
        if (InputBufferLevel == 0)
            continue;

        InputBufferLevel--;
        CurSample = InputBuffer[InputBufferReadPos];
        InputBufferReadPos++;
        if (InputBufferReadPos >= InputBufferSize)
            InputBufferReadPos = 0;
    }
}

s16 Mic::ReadSample()
{
    if (!OpenMask) return 0;
    return CurSample;
}


}
