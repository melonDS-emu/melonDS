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

#ifndef DSI_I2S_H
#define DSI_I2S_H

#include "FIFO.h"
#include "types.h"
#include "Savestate.h"

namespace melonDS
{
class DSi;
class DSi_I2S
{
public:
    DSi_I2S(melonDS::DSi& dsi);
    ~DSi_I2S();
    void Reset();
    void DoSavestate(Savestate* file);

    void SampleClock(s16 output[2]);

    u16 ReadMicCnt();
    void WriteMicCnt(u16 val, u16 mask);

    u32 ReadMicData();

    u16 ReadSndExCnt();
    void WriteSndExCnt(u16 val, u16 mask);

private:
    melonDS::DSi& DSi;

    u16 MicCnt;
    u16 SndExCnt;
    u8 MicClockDivider;
    FIFO<u32, 16> MicFifo;
    s16 MicTempSample;
    u8 MicTempCount;

    void WriteMicData(s16 sample);
};

}
#endif // DSI_I2S_H
