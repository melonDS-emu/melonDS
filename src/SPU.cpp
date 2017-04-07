/*
    Copyright 2016-2017 StapleButter

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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "SPU.h"


namespace SPU
{

const s8 ADPCMIndexTable[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

const u16 ADPCMTable[89] =
{
    0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E,
    0x0010, 0x0011, 0x0013, 0x0015, 0x0017, 0x0019, 0x001C, 0x001F,
    0x0022, 0x0025, 0x0029, 0x002D, 0x0032, 0x0037, 0x003C, 0x0042,
    0x0049, 0x0050, 0x0058, 0x0061, 0x006B, 0x0076, 0x0082, 0x008F,
    0x009D, 0x00AD, 0x00BE, 0x00D1, 0x00E6, 0x00FD, 0x0117, 0x0133,
    0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE, 0x0220, 0x0256, 0x0292,
    0x02D4, 0x031C, 0x036C, 0x03C3, 0x0424, 0x048E, 0x0502, 0x0583,
    0x0610, 0x06AB, 0x0756, 0x0812, 0x08E0, 0x09C3, 0x0ABD, 0x0BD0,
    0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE, 0x1706, 0x1954,
    0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B,
    0x3BB9, 0x41B2, 0x4844, 0x4F7E, 0x5771, 0x602F, 0x69CE, 0x7462,
    0x7FFF
};

const s16 PSGTable[8][8] =
{
    {-0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF,  0x7FFF},
    {-0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF,  0x7FFF,  0x7FFF},
    {-0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF},
    {-0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF},
    {-0x7FFF, -0x7FFF, -0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF},
    {-0x7FFF, -0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF},
    {-0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF},
    {-0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF}
};

const u32 OutputBufferSize = 2*1024;
s16 OutputBuffer[2 * OutputBufferSize];
u32 OutputReadOffset;
u32 OutputWriteOffset;


u16 Cnt;
u8 MasterVolume;
u16 Bias;

Channel* Channels[16];


bool Init()
{
    for (int i = 0; i < 16; i++)
        Channels[i] = new Channel(i);

    return true;
}

void DeInit()
{
    for (int i = 0; i < 16; i++)
        delete Channels[i];
}

void Reset()
{
    memset(OutputBuffer, 0, 2*OutputBufferSize*2);
    OutputReadOffset = 0;
    OutputWriteOffset = 0;

    Cnt = 0;
    MasterVolume = 0;
    Bias = 0;

    for (int i = 0; i < 16; i++)
        Channels[i]->Reset();

    NDS::ScheduleEvent(NDS::Event_SPU, true, 1024*16, Mix, 16);
}


void SetBias(u16 bias)
{
    Bias = bias;
}


Channel::Channel(u32 num)
{
    Num = num;
}

Channel::~Channel()
{
}

void Channel::Reset()
{
    SetCnt(0);
    SrcAddr = 0;
    TimerReload = 0;
    LoopPos = 0;
    Length = 0;
}

void Channel::Start()
{
    Timer = TimerReload;

    if (((Cnt >> 29) & 0x3) == 1)
        Pos = -1;
    else
        Pos = -3;

    NoiseVal = 0x7FFF;
    CurSample = 0;
}

void Channel::NextSample_PCM8()
{
    Pos++;
    if (Pos < 0) return;
    if (Pos >= (LoopPos + Length))
    {
        // TODO: what happens when mode 3 is used?
        u32 repeat = (Cnt >> 27) & 0x3;
        if (repeat == 2)
        {
            CurSample = 0;
            Cnt &= ~(1<<31);
            return;
        }
        else if (repeat == 1)
        {
            Pos = LoopPos;
        }
    }

    s8 val = (s8)NDS::ARM7Read8(SrcAddr + Pos);
    CurSample = val << 8;
}

void Channel::NextSample_PCM16()
{
    Pos++;
    if (Pos < 0) return;
    if ((Pos<<1) >= (LoopPos + Length))
    {
        // TODO: what happens when mode 3 is used?
        u32 repeat = (Cnt >> 27) & 0x3;
        if (repeat == 2)
        {
            CurSample = 0;
            Cnt &= ~(1<<31);
            return;
        }
        else if (repeat == 1)
        {
            Pos = LoopPos>>1;
        }
    }

    s16 val = (s16)NDS::ARM7Read16(SrcAddr + (Pos<<1));
    CurSample = val;
}

void Channel::NextSample_ADPCM()
{
    Pos++;
    if (Pos < 8)
    {
        if (Pos == 0)
        {
            // setup ADPCM
            u32 header = NDS::ARM7Read32(SrcAddr);
            ADPCMVal = header & 0xFFFF;
            ADPCMIndex = (header >> 16) & 0x7F;
            if (ADPCMIndex > 88) ADPCMIndex = 88;

            ADPCMValLoop = ADPCMVal;
            ADPCMIndexLoop = ADPCMIndex;
        }

        return;
    }

    if ((Pos>>1) >= (LoopPos + Length))
    {
        // TODO: what happens when mode 3 is used?
        u32 repeat = (Cnt >> 27) & 0x3;
        if (repeat == 2)
        {
            CurSample = 0;
            Cnt &= ~(1<<31);
            return;
        }
        else if (repeat == 1)
        {
            Pos = LoopPos<<1;
            ADPCMVal = ADPCMValLoop;
            ADPCMIndex = ADPCMIndexLoop;
        }
    }
    else
    {
        if (!(Pos & 0x1))
            ADPCMCurByte = NDS::ARM7Read8(SrcAddr + (Pos>>1));
        else
            ADPCMCurByte >>= 4;

        u16 val = ADPCMTable[ADPCMIndex];
        u16 diff = val >> 3;
        if (ADPCMCurByte & 0x1) diff += (val >> 2);
        if (ADPCMCurByte & 0x2) diff += (val >> 1);
        if (ADPCMCurByte & 0x4) diff += val;

        if (ADPCMCurByte & 0x8)
        {
            ADPCMVal -= diff;
            if (ADPCMVal < -0x7FFF) ADPCMVal = -0x7FFF;
        }
        else
        {
            ADPCMVal += diff;
            if (ADPCMVal > 0x7FFF) ADPCMVal = 0x7FFF;
        }

        ADPCMIndex += ADPCMIndexTable[ADPCMCurByte & 0x7];
        if      (ADPCMIndex < 0)  ADPCMIndex = 0;
        else if (ADPCMIndex > 88) ADPCMIndex = 88;

        if (Pos == (LoopPos<<1))
        {
            ADPCMValLoop = ADPCMVal;
            ADPCMIndexLoop = ADPCMIndex;
        }
    }

    CurSample = ADPCMVal;
}

void Channel::NextSample_PSG()
{
    Pos++;
    CurSample = PSGTable[(Cnt >> 24) & 0x7][Pos & 0x7];
}

void Channel::NextSample_Noise()
{
    if (NoiseVal & 0x1)
    {
        NoiseVal = (NoiseVal >> 1) ^ 0x6000;
        CurSample = -0x7FFF;
    }
    else
    {
        NoiseVal >>= 1;
        CurSample = 0x7FFF;
    }
}

template<u32 type>
void Channel::Run(s32* buf, u32 samples)
{
    for (u32 s = 0; s < samples; s++)
        buf[s] = 0;

    if (!(Cnt & 0x7F)) return;

    for (u32 s = 0; s < samples; s++)
    {
        Timer += 512; // 1 sample = 512 cycles at 16MHz

        while (Timer >> 16)
        {
            Timer = TimerReload + (Timer - 0x10000);

            switch (type)
            {
            case 0: NextSample_PCM8(); break;
            case 1: NextSample_PCM16(); break;
            case 2: NextSample_ADPCM(); break;
            case 3: NextSample_PSG(); break;
            case 4: NextSample_Noise(); break;
            }
        }

        buf[s] = (s32)CurSample;
    }
}


void Mix(u32 samples)
{
    s32 channelbuf[32];
    s32 leftbuf[32];
    s32 rightbuf[32];

    for (u32 s = 0; s < samples; s++)
    {
        leftbuf[s] = 0;
        rightbuf[s] = 0;
    }

    for (int i = 0; i < 16; i++)
    {
        Channel* chan = Channels[i];
        if (!(chan->Cnt & (1<<31))) continue;

        // TODO: what happens if we use type 3 on channels 0-7??
        switch ((chan->Cnt >> 29) & 0x3)
        {
        case 0: chan->Run<0>(channelbuf, samples); break;
        case 1: chan->Run<1>(channelbuf, samples); break;
        case 2: chan->Run<2>(channelbuf, samples); break;
        case 3:
            if      (i >= 14) chan->Run<4>(channelbuf, samples);
            else if (i >= 8)  chan->Run<3>(channelbuf, samples);
            break;
        }

        for (u32 s = 0; s < samples; s++)
        {
            s32 val = (s32)channelbuf[s];

            val <<= chan->VolumeShift;
            val *= chan->Volume;

            s32 l = ((s64)val * (128-chan->Pan)) >> 10;
            s32 r = ((s64)val * chan->Pan) >> 10;

            leftbuf[s] += l;
            rightbuf[s] += r;
        }
    }

    //

    for (u32 s = 0; s < samples; s++)
    {
        s32 l = (s32)leftbuf[s];
        s32 r = (s32)rightbuf[s];

        l = ((s64)l * MasterVolume) >> 7;
        r = ((s64)r * MasterVolume) >> 7;

        l >>= 12;
        if      (l < -0x8000) l = -0x8000;
        else if (l > 0x7FFF)  l = 0x7FFF;
        r >>= 12;
        if      (r < -0x8000) r = -0x8000;
        else if (r > 0x7FFF)  r = 0x7FFF;

        OutputBuffer[OutputWriteOffset    ] = l << 3;
        OutputBuffer[OutputWriteOffset + 1] = r << 3;
        OutputWriteOffset += 2;
        OutputWriteOffset &= ((2*OutputBufferSize)-1);
    }


    NDS::ScheduleEvent(NDS::Event_SPU, true, 1024*16, Mix, 16);
}


void ReadOutput(s16* data, int samples)
{
    for (int i = 0; i < samples; i++)
    {
        *data++ = OutputBuffer[OutputReadOffset];
        *data++ = OutputBuffer[OutputReadOffset + 1];

        if (OutputReadOffset != OutputWriteOffset)
        {
            OutputReadOffset += 2;
            OutputReadOffset &= ((2*OutputBufferSize)-1);
        }
    }
}


u8 Read8(u32 addr)
{
    if (addr < 0x04000500)
    {
        Channel* chan = Channels[(addr >> 4) & 0xF];

        switch (addr & 0xF)
        {
        case 0x0: return chan->Cnt & 0xFF;
        case 0x1: return (chan->Cnt >> 8) & 0xFF;
        case 0x2: return (chan->Cnt >> 16) & 0xFF;
        case 0x3: return chan->Cnt >> 24;
        }
    }
    else
    {
        switch (addr)
        {
        case 0x04000500: return Cnt & 0x7F;
        case 0x04000501: return Cnt >> 8;
        }
    }

    //printf("unknown SPU read8 %08X\n", addr);
    return 0;
}

u16 Read16(u32 addr)
{
    if (addr < 0x04000500)
    {
        Channel* chan = Channels[(addr >> 4) & 0xF];

        switch (addr & 0xF)
        {
        case 0x0: return chan->Cnt & 0xFFFF;
        case 0x2: return chan->Cnt >> 16;
        }
    }
    else
    {
        switch (addr)
        {
        case 0x04000500: return Cnt;
        case 0x04000504: return Bias;
        }
    }

    printf("unknown SPU read16 %08X\n", addr);
    return 0;
}

u32 Read32(u32 addr)
{
    if (addr < 0x04000500)
    {
        Channel* chan = Channels[(addr >> 4) & 0xF];

        switch (addr & 0xF)
        {
        case 0x0: return chan->Cnt;
        }
    }
    else
    {
        switch (addr)
        {
        case 0x04000500: return Cnt;
        case 0x04000504: return Bias;
        }
    }

    printf("unknown SPU read32 %08X\n", addr);
    return 0;
}

void Write8(u32 addr, u8 val)
{
    if (addr < 0x04000500)
    {
        Channel* chan = Channels[(addr >> 4) & 0xF];

        switch (addr & 0xF)
        {
        case 0x0: chan->SetCnt((chan->Cnt & 0xFFFFFF00) | val); return;
        case 0x1: chan->SetCnt((chan->Cnt & 0xFFFF00FF) | (val << 8)); return;
        case 0x2: chan->SetCnt((chan->Cnt & 0xFF00FFFF) | (val << 16)); return;
        case 0x3: chan->SetCnt((chan->Cnt & 0x00FFFFFF) | (val << 24)); return;
        }
    }
    else
    {
        switch (addr)
        {
        case 0x04000500:
            Cnt = (Cnt & 0xBF00) | (val & 0x7F);
            MasterVolume = Cnt & 0x7F;
            if (MasterVolume == 127) MasterVolume++;
            return;
        case 0x04000501:
            Cnt = (Cnt & 0x007F) | ((val & 0xBF) << 8);
            return;
        }
    }

    printf("unknown SPU write8 %08X %02X\n", addr, val);
}

void Write16(u32 addr, u16 val)
{
    if (addr < 0x04000500)
    {
        Channel* chan = Channels[(addr >> 4) & 0xF];

        switch (addr & 0xF)
        {
        case 0x0: chan->SetCnt((chan->Cnt & 0xFFFF0000) | val); return;
        case 0x2: chan->SetCnt((chan->Cnt & 0x0000FFFF) | (val << 16)); return;
        case 0x8: chan->SetTimerReload(val); return;
        case 0xA: chan->SetLoopPos(val); return;
        }
    }
    else
    {
        switch (addr)
        {
        case 0x04000500:
            Cnt = val & 0xBF7F;
            MasterVolume = Cnt & 0x7F;
            if (MasterVolume == 127) MasterVolume++;
            return;

        case 0x04000504:
            Bias = val & 0x3FF;
            return;
        }
    }

    printf("unknown SPU write16 %08X %04X\n", addr, val);
}

void Write32(u32 addr, u32 val)
{
    if (addr < 0x04000500)
    {
        Channel* chan = Channels[(addr >> 4) & 0xF];

        switch (addr & 0xF)
        {
        case 0x0: chan->SetCnt(val); return;
        case 0x4: chan->SetSrcAddr(val); return;
        case 0x8:
            chan->SetTimerReload(val & 0xFFFF);
            chan->SetLoopPos(val >> 16);
            return;
        case 0xC: chan->SetLength(val); return;
        }
    }
    else
    {
        switch (addr)
        {
        case 0x04000500:
            Cnt = val & 0xBF7F;
            MasterVolume = Cnt & 0x7F;
            if (MasterVolume == 127) MasterVolume++;
            return;

        case 0x04000504:
            Bias = val & 0x3FF;
            return;
        }
    }

    printf("unknown SPU write32 %08X %08X\n", addr, val);
}

}
