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
    Pos = 0;

    // hax
    NextSample_PSG();
}

void Channel::NextSample_PSG()
{
    s16 psgtable[8][8] =
    {
        {-0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF,  0x7FFF},
        {-0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF,  0x7FFF,  0x7FFF},
        {-0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF},
        {-0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF},
        {-0x7FFF, -0x7FFF, -0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF},
        {-0x7FFF, -0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF},
        {-0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF},
        { 0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF,  0x7FFF}
    };

    CurSample = psgtable[(Cnt >> 24) & 0x7][Pos & 0x7];
}

void Channel::Run(s32* buf, u32 samples)
{
    for (u32 s = 0; s < samples; s++)
        buf[s] = 0;

    if (!(Cnt & (1<<31))) return;
    if (Num < 8) return;
    if (!(Cnt & 0x7F)) return;

    for (u32 s = 0; s < samples; s++)
    {
        Timer += 512; // 1 sample = 512 cycles at 16MHz
        // will probably shit itself for very high-pitched sounds
        while (Timer >> 16)
        {
            Timer = TimerReload + (Timer - 0x10000);
            Pos++;
            NextSample_PSG();
        }

        buf[s] = (s32)CurSample;
    }
}


void Mix(u32 samples)
{
    s32 channelbuf[32];
    s32 finalbuf[32];

    for (u32 s = 0; s < samples; s++)
        finalbuf[s] = 0;

    for (int i = 0; i < 16; i++)
    //int i = 8;
    {
        Channel* chan = Channels[i];
        chan->Run(channelbuf, samples);

        for (u32 s = 0; s < samples; s++)
        {
            finalbuf[s] += channelbuf[s];
        }
    }

    //

    for (u32 s = 0; s < samples; s++)
    {
        s16 val = (s16)(finalbuf[s] >> 4);
        // TODO panning! also volume and all

        OutputBuffer[OutputWriteOffset] = val;
        OutputBuffer[OutputWriteOffset + 1] = val;
        OutputWriteOffset += 2;
        OutputWriteOffset &= ((2*OutputBufferSize)-1);
    }


    NDS::ScheduleEvent(NDS::Event_SPU, true, 1024*16, Mix, 16);
}


void ReadOutput(s16* data, int samples)
{u32 zarp = 0;
    for (int i = 0; i < samples; i++)
    {
        *data++ = OutputBuffer[OutputReadOffset];
        *data++ = OutputBuffer[OutputReadOffset + 1];

        if (OutputReadOffset != OutputWriteOffset)
        {zarp += 2;
            OutputReadOffset += 2;
            OutputReadOffset &= ((2*OutputBufferSize)-1);
        }
    }printf("read %d samples, took %d\n", samples, zarp);
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

    printf("unknown SPU read8 %08X\n", addr);
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
