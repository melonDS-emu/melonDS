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

#include <string.h>
#include "DSi.h"
#include "DSi_DSP.h"
#include "DSi_I2S.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;


DSi_I2S::DSi_I2S(melonDS::DSi& dsi) : DSi(dsi)
{
    DSi.RegisterEventFuncs(Event_DSi_I2S, this, {MakeEventThunk(DSi_I2S, Clock)});

    MicCnt = 0;
    SndExCnt = 0;
    MicClockDivider = 0;

    MicBufferLen = 0;
}

DSi_I2S::~DSi_I2S()
{
    DSi.UnregisterEventFuncs(Event_DSi_I2S);
}

void DSi_I2S::Reset()
{
    MicCnt = 0;
    SndExCnt = 0;
    MicClockDivider = 0;

    MicFifo.Clear();

    MicBufferLen = 0;

    DSi.ScheduleEvent(Event_DSi_I2S, false, 1024, 0, I2S_Freq_32728Hz);
}

void DSi_I2S::DoSavestate(Savestate* file)
{
    file->Section("I2Si");

    file->Var16(&MicCnt);
    file->Var16(&SndExCnt);
    file->Var8(&MicClockDivider);

    MicFifo.DoSavestate(file);
}

void DSi_I2S::MicInputFrame(const s16* data, int samples)
{
    if (!data)
    {
        MicBufferLen = 0;
        return;
    }

    if (samples > 1024) samples = 1024;
    memcpy(MicBuffer, data, samples * sizeof(s16));
    MicBufferLen = samples;
}

u16 DSi_I2S::ReadMicCnt()
{
    u16 ret = MicCnt;
    if (MicFifo.Level() ==  0) ret |= 1 << 8;
    if (MicFifo.Level() >= 16) ret |= 1 << 9;
    if (MicFifo.Level() >= 32) ret |= 1 << 10;
    return ret;
}

void DSi_I2S::WriteMicCnt(u16 val, u16 mask)
{
    val = (val & mask) | (MicCnt & ~mask);

    // FIFO clear can only happen if the mic was disabled before the write
    if (!(MicCnt & (1 << 15)) && (val & (1 << 12)))
    {
        MicCnt &= ~(1 << 11);
        MicFifo.Clear();
    }

    MicCnt = (val & 0xE00F) | (MicCnt & (1 << 11));
}

u32 DSi_I2S::ReadMicData()
{
    // CHECKME: This is a complete guess on how mic data reads work
    // gbatek states the FIFO is 16 words large, with 1 word having 2 samples
    u32 ret = MicFifo.IsEmpty() ? 0 : (u16)MicFifo.Read();
    ret |= (MicFifo.IsEmpty() ? 0 : (u16)MicFifo.Read()) << 16;
    return ret;
}

u16 DSi_I2S::ReadSndExCnt()
{
    return SndExCnt;
}

void DSi_I2S::WriteSndExCnt(u16 val, u16 mask)
{
    val = (val & mask) | (SndExCnt & ~mask);

    // Note: SNDEXCNT can be accessed in "NDS mode"
    // This is due to the corresponding SCFG_EXT flag not being disabled
    // If it is disabled (with homebrew), SNDEXCNT cannot be accessed
    // This is more purely a software mistake on the DSi menu's part

    // I2S frequency can only be changed if it was disabled before the write
    if (SndExCnt & (1 << 15))
    {
        val &= ~(1 << 13);
        val |= SndExCnt & (1 << 13);
    }

    if ((SndExCnt ^ val) & (1 << 13))
    {
        Log(LogLevel::Debug, "Changed I2S frequency to %dHz\n", (SndExCnt & (1 << 13)) ? 47605 : 32728);
    }

    SndExCnt = val & 0xE00F;
}

void DSi_I2S::SampleClock(s16 output[2])
{
    // HAX
    s16 output_nitro[2] = {output[0], output[1]};
    s16 output_dsp[2];

    DSi.DSP.SampleClock(output_dsp, 0);

    // NITRO/DSP ratio
    // any value greater than 8 acts the same as 8
    u8 fn = SndExCnt & 0xF;
    if (fn > 8) fn = 8;
    u8 fd = 8 - fn;

    output[0] = ((output_nitro[0] * fn) + (output_dsp[0] * fd)) >> 3;
    output[1] = ((output_nitro[1] * fn) + (output_dsp[1] * fd)) >> 3;
}

void DSi_I2S::Clock(u32 freq)
{
    if (SndExCnt & (1 << 15))
    {
        // CHECKME (from gbatek)
        // "The Sampling Rate becomes zero (no data arriving) when SNDEXCNT.Bit15=0, or when MIC_CNT.bit0-1=3, or when MIC_CNT.bit15=0, or when Overrun has occurred."
        // This likely means on any of these conditions the mic completely ignores any I2S clocks
        // Although perhaps it might still acknowledge the clocks in some capacity (maybe affecting below clock division)
        if ((MicCnt & (1 << 15)) && !(MicCnt & (1 << 11)) && (MicCnt & 3) != 3)
        {
            // CHECKME (from gbatek)
            // "2-3   Sampling Rate  (0..3=F/1, F/2, F/3, F/4)"
            // This likely works with an internal counter compared with the sampling rate
            // This counter is then likely reset on mic sample
            // But this is completely untested

            MicClockDivider++;
            u8 micRate = (MicCnt >> 2) & 3;
            if (MicClockDivider > micRate)
            {
                MicClockDivider = 0;

                s16 sample = 0;
                if (MicBufferLen > 0)
                {
                    // 560190 cycles per frame
                    u32 cyclepos = (u32)DSi.GetSysClockCycles(2);
                    u32 samplepos = (cyclepos * MicBufferLen) / 560190;
                    if (samplepos >= MicBufferLen) samplepos = MicBufferLen - 1;
                    sample = MicBuffer[samplepos];
                }

                u32 oldLevel = MicFifo.Level();
                if ((MicCnt & 3) == 0)
                {
                    // stereo (this just duplicates the sample, as the mic itself is mono)
                    if (MicFifo.IsFull()) MicCnt |= 1 << 11;
                    MicFifo.Write(sample);
                    if (MicFifo.IsFull()) MicCnt |= 1 << 11;
                    MicFifo.Write(sample);
                }
                else
                {
                    // mono
                    if (MicFifo.IsFull()) MicCnt |= 1 << 11;
                    MicFifo.Write(sample);
                }

                // if bit 13 is set, an IRQ is generated when the mic FIFO is half full
                if (MicCnt & (1 << 13))
                {
                    if (oldLevel < 16 && MicFifo.Level() >= 16) DSi.SetIRQ2(IRQ2_DSi_MicExt);
                }
                // if bit 13 is not set and bit 14 is set, an IRQ is generated when the mic FIFO is full
                else if (MicCnt & (1 << 14))
                {
                    if (oldLevel < 32 && MicFifo.Level() >= 32) DSi.SetIRQ2(IRQ2_DSi_MicExt);
                }
            }
        }

        // TODO: SPU and DSP sampling should happen here
        // use passed freq to know how much to advance SPU by?
    }

    if (SndExCnt & (1 << 13))
    {
        DSi.ScheduleEvent(Event_DSi_I2S, false, 704, 0, I2S_Freq_47605Hz);
    }
    else
    {
        DSi.ScheduleEvent(Event_DSi_I2S, false, 1024, 0, I2S_Freq_32728Hz);
    }
}

}
