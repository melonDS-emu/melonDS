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
#include "Mic.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;


DSi_I2S::DSi_I2S(melonDS::DSi& dsi) : DSi(dsi)
{
    MicCnt = 0;
    SndExCnt = 0;
    MicClockDivider = 0;
}

DSi_I2S::~DSi_I2S()
{
}

void DSi_I2S::Reset()
{
    MicCnt = 0;
    SndExCnt = 0;
    MicClockDivider = 0;
    MicTempSample = 0;
    MicTempCount = 0;

    MicFifo.Clear();
}

void DSi_I2S::DoSavestate(Savestate* file)
{
    file->Section("I2Si");

    file->Var16(&MicCnt);
    file->Var16(&SndExCnt);
    file->Var8(&MicClockDivider);
    file->Var16((u16*)&MicTempSample);
    file->Var8(&MicTempCount);

    MicFifo.DoSavestate(file);
}

u16 DSi_I2S::ReadMicCnt()
{
    u16 ret = MicCnt;
    u32 fifolevel = MicFifo.Level();
    if (fifolevel == 0)  ret |= (1 << 8);
    if (fifolevel >= 8)  ret |= (1 << 9);
    if (fifolevel == 16) ret |= (1 << 10);
    return ret;
}

void DSi_I2S::WriteMicCnt(u16 val, u16 mask)
{
    if (MicCnt & (1<<15))
    {
        // data format, sampling rate and FIFO clear can only be written to
        // when bit 15 is cleared
        mask &= ~0x100F;
    }

    val = (val & mask) | (MicCnt & ~mask);

    if (val & (1 << 12))
    {
        MicCnt &= ~(1 << 11);
        MicFifo.Clear();
        MicTempSample = 0;
        MicTempCount = 0;
    }

    if ((val ^ MicCnt) & (1<<15))
    {
        // if needed, start or stop the mic
        if (val & (1<<15))
        {
            MicClockDivider = 0;
            MicTempCount = 0;

            DSi.Mic.Start(Mic_DSi);
        }
        else
            DSi.Mic.Stop(Mic_DSi);
    }

    MicCnt = (val & 0xE00F) | (MicCnt & (1 << 11));
}

u32 DSi_I2S::ReadMicData()
{
    // reads from this register, of all sizes, always pull a word from the FIFO
    // if the FIFO is empty, the last word is repeated
    return MicFifo.Read();
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
        Log(LogLevel::Debug, "Changed I2S frequency to %dHz\n", (val & (1 << 13)) ? 47605 : 32728);

        AudioSampleRate rate = (val & (1<<13)) ? AudioSampleRate::_47KHz : AudioSampleRate::_32KHz;
        DSi.SPU.SetSampleRate(rate);
    }

    SndExCnt = val & 0xE00F;
}

void DSi_I2S::SampleClock(s16 output[2])
{
    if (!(SndExCnt & (1<<15)))
    {
        // I2S interface disabled
        // no audio gets fed to the output or to the mic
        output[0] = 0;
        output[1] = 0;
        return;
    }

    s16 output_nitro[2] = {output[0], output[1]};
    s16 output_dsp[2];
    s16 mic_input = DSi.Mic.ReadSample();

    DSi.DSP.SampleClock(output_dsp, mic_input);

    WriteMicData(mic_input);

    if (SndExCnt & (1<<14))
    {
        // mute
        output[0] = 0;
        output[1] = 0;
    }
    else
    {
        // NITRO/DSP ratio
        // any value greater than 8 acts the same as 8
        u8 fn = SndExCnt & 0xF;
        if (fn > 8) fn = 8;
        u8 fd = 8 - fn;

        output[0] = ((output_nitro[0] * fn) + (output_dsp[0] * fd)) >> 3;
        output[1] = ((output_nitro[1] * fn) + (output_dsp[1] * fd)) >> 3;
    }
}

void DSi_I2S::WriteMicData(s16 sample)
{
    if (!(MicCnt & (1<<15)))    // mic disabled
        return;

    if (MicCnt & (1<<11))       // FIFO overrun
        return;

    // NOTE on mic data format (bit 0-1) and sampling rate (bit 2-3)
    // -
    // Due to the way the I2S interface works, each mic sample is duplicated.
    // Format 0 simply feeds both samples into the FIFO.
    // Formats 1 and 2 reject one of the two samples, in order to produce mono data.
    // Format 3 rejects both samples.
    // This suggests that the "data format" is actually a bitfield specifying which samples,
    // "left" or "right", to reject. However, since both are the same, it isn't clear which
    // one is "left" or "right".
    // Figuring this out would require hardware modification in order to feed custom I2S data.
    // -
    // Sampling rate uses a simple counter.
    // For example, with sampling rate value 2 (F/3), at each sample the counter goes:
    // 0 1 2 0 1 2 0 1 2 ...
    // When the data format is 0 or 1, samples are accepted when the counter is 0.
    // However, when the data format is 2, samples are only accepted when the counter is 2
    // (the maximum value for the current sampling rate setting).
    // This can be verified by starting the mic interface and measuring the time until the
    // FIFO becomes non-empty.

    u8 micMode = MicCnt & 0x3;
    u8 micRate = (MicCnt >> 2) & 0x3;

    if (micMode == 3)
        return;

    u8 chk = (micMode == 2) ? micRate : 0;
    bool capture = (MicClockDivider == chk);

    if (MicClockDivider >= micRate)
        MicClockDivider = 0;
    else
        MicClockDivider++;

    if (capture)
    {
        u32 val;

        if (micMode == 0)
        {
            // feed two samples at once

            val = (u16)sample;
            val |= (val << 16);
        }
        else
        {
            // feed one sample at once
            // we can only fill the FIFO if we already received another sample

            if (!MicTempCount)
            {
                MicTempSample = sample;
                MicTempCount = 1;
                return;
            }

            val = (u16)MicTempSample;
            val |= (((u16)sample) << 16);
        }

        if (MicFifo.IsFull())
            MicCnt |= (1<<11);
        else
            MicFifo.Write(val);

        MicTempCount = 0;
    }

    // MIC_CNT IRQ bits aren't mutually exclusive
    // also, the bit 14 IRQ signals an overrun, not a FIFO full condition

    if (MicCnt & (1<<11))
    {
        // overrun, raise IRQ if needed

        if (MicCnt & (1<<14))
            DSi.SetIRQ2(IRQ2_DSi_MicExt);

        DSi.Mic.Stop(Mic_DSi);
    }
    else if (MicFifo.Level() == 8)
    {
        // FIFO became half-full, check for DMA and raise IRQ if needed

        if (MicCnt & (1<<13))
            DSi.SetIRQ2(IRQ2_DSi_MicExt);

        DSi.CheckNDMAs(1, 0x2C);
    }
}

}
