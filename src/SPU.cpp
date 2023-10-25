/*
    Copyright 2016-2022 melonDS team

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
#include <cmath>
#include "Platform.h"
#include "NDS.h"
#include "DSi.h"
#include "SPU.h"

using Platform::Log;
using Platform::LogLevel;


// SPU TODO
// * capture addition modes, overflow bugs
// * channel hold

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

// audio interpolation is an improvement upon the original hardware
// (which performs no interpolation)
int InterpType;
s16 InterpCos[0x100];
s16 InterpCubic[0x100][4];

const u32 OutputBufferSize = 2*2048;
s16 OutputBackbuffer[2 * OutputBufferSize];
u32 OutputBackbufferWritePosition;

s16 OutputFrontBuffer[2 * OutputBufferSize];
u32 OutputFrontBufferWritePosition;
u32 OutputFrontBufferReadPosition;

Platform::Mutex* AudioLock;

u16 Cnt;
u8 MasterVolume;
u16 Bias;
bool ApplyBias;
bool Degrade10Bit;

Channel* Channels[16];
CaptureUnit* Capture[2];


bool Init()
{
    for (int i = 0; i < 16; i++)
        Channels[i] = new Channel(i);

    Capture[0] = new CaptureUnit(0);
    Capture[1] = new CaptureUnit(1);

    AudioLock = Platform::Mutex_Create();

    InterpType = 0;
    ApplyBias = true;
    Degrade10Bit = false;

    // generate interpolation tables
    // values are 1:1:14 fixed-point

    float m_pi = std::acos(-1.0f);
    for (int i = 0; i < 0x100; i++)
    {
        float ratio = (i * m_pi) / 255.0f;
        ratio = 1.0f - std::cos(ratio);

        InterpCos[i] = (s16)(ratio * 0x2000);
    }

    for (int i = 0; i < 0x100; i++)
    {
        s32 i1 = i << 6;
        s32 i2 = (i * i) >> 2;
        s32 i3 = (i * i * i) >> 10;

        InterpCubic[i][0] = -i3 + 2*i2 - i1;
        InterpCubic[i][1] = i3 - 2*i2 + 0x4000;
        InterpCubic[i][2] = -i3 + i2 + i1;
        InterpCubic[i][3] = i3 - i2;
    }

    return true;
}

void DeInit()
{
    for (int i = 0; i < 16; i++)
    {
        delete Channels[i];
        Channels[i] = nullptr;
    }

    delete Capture[0];
    delete Capture[1];
    Capture[0] = nullptr;
    Capture[1] = nullptr;

    Platform::Mutex_Free(AudioLock);
    AudioLock = nullptr;
}

void Reset()
{
    InitOutput();

    Cnt = 0;
    MasterVolume = 0;
    Bias = 0;

    for (int i = 0; i < 16; i++)
        Channels[i]->Reset();

    Capture[0]->Reset();
    Capture[1]->Reset();

    NDS::ScheduleEvent(NDS::Event_SPU, true, 1024, Mix, 0);
}

void Stop()
{
    Platform::Mutex_Lock(AudioLock);
    memset(OutputFrontBuffer, 0, 2*OutputBufferSize*2);

    OutputBackbufferWritePosition = 0;
    OutputFrontBufferReadPosition = 0;
    OutputFrontBufferWritePosition = 0;
    Platform::Mutex_Unlock(AudioLock);
}

void DoSavestate(Savestate* file)
{
    file->Section("SPU.");

    file->Var16(&Cnt);
    file->Var8(&MasterVolume);
    file->Var16(&Bias);

    for (int i = 0; i < 16; i++)
        Channels[i]->DoSavestate(file);

    Capture[0]->DoSavestate(file);
    Capture[1]->DoSavestate(file);
}


void SetPowerCnt(u32 val)
{
    // TODO
}


void SetInterpolation(int type)
{
    InterpType = type;
}

void SetBias(u16 bias)
{
    Bias = bias;
}

void SetApplyBias(bool enable)
{
    ApplyBias = enable;
}

void SetDegrade10Bit(bool enable)
{
    Degrade10Bit = enable;
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
    if (NDS::ConsoleType == 1)
        BusRead32 = DSi::ARM7Read32;
    else
        BusRead32 = NDS::ARM7Read32;

    KeyOn = false;

    SetCnt(0);
    SrcAddr = 0;
    TimerReload = 0;
    LoopPos = 0;
    Length = 0;

    Timer = 0;

    Pos = 0;
    FIFOReadPos = 0;
    FIFOWritePos = 0;
    FIFOReadOffset = 0;
    FIFOLevel = 0;
}

void Channel::DoSavestate(Savestate* file)
{
    file->Var32(&Cnt);
    file->Var32(&SrcAddr);
    file->Var16(&TimerReload);
    file->Var32(&LoopPos);
    file->Var32(&Length);

    file->Var8(&Volume);
    file->Var8(&VolumeShift);
    file->Var8(&Pan);

    file->Var8((u8*)&KeyOn);
    file->Var32(&Timer);
    file->Var32((u32*)&Pos);
    file->VarArray(PrevSample, sizeof(PrevSample));
    file->Var16((u16*)&CurSample);
    file->Var16(&NoiseVal);

    file->Var32((u32*)&ADPCMVal);
    file->Var32((u32*)&ADPCMIndex);
    file->Var32((u32*)&ADPCMValLoop);
    file->Var32((u32*)&ADPCMIndexLoop);
    file->Var8(&ADPCMCurByte);

    file->Var32(&FIFOReadPos);
    file->Var32(&FIFOWritePos);
    file->Var32(&FIFOReadOffset);
    file->Var32(&FIFOLevel);
    file->VarArray(FIFO, sizeof(FIFO));
}

void Channel::FIFO_BufferData()
{
    u32 totallen = LoopPos + Length;

    if (FIFOReadOffset >= totallen)
    {
        u32 repeatmode = (Cnt >> 27) & 0x3;
        if      (repeatmode & 1) FIFOReadOffset = LoopPos;
        else if (repeatmode & 2) return; // one-shot sound, we're done
    }

    u32 burstlen = 16;
    if ((FIFOReadOffset + 16) > totallen)
        burstlen = totallen - FIFOReadOffset;

    // sound DMA can't read from the ARM7 BIOS
    if ((SrcAddr + FIFOReadOffset) >= 0x00004000)
    {
        for (u32 i = 0; i < burstlen; i += 4)
        {
            FIFO[FIFOWritePos] = BusRead32(SrcAddr + FIFOReadOffset);
            FIFOReadOffset += 4;
            FIFOWritePos++;
            FIFOWritePos &= 0x7;
        }
    }
    else
    {
        for (u32 i = 0; i < burstlen; i += 4)
        {
            FIFO[FIFOWritePos] = 0;
            FIFOReadOffset += 4;
            FIFOWritePos++;
            FIFOWritePos &= 0x7;
        }
    }

    FIFOLevel += burstlen;
}

template<typename T>
T Channel::FIFO_ReadData()
{
    T ret = *(T*)&((u8*)FIFO)[FIFOReadPos];

    FIFOReadPos += sizeof(T);
    FIFOReadPos &= 0x1F;
    FIFOLevel -= sizeof(T);

    if (FIFOLevel <= 16)
        FIFO_BufferData();

    return ret;
}

void Channel::Start()
{
    Timer = TimerReload;

    if (((Cnt >> 29) & 0x3) == 3)
        Pos = -1;
    else
        Pos = -3;

    NoiseVal = 0x7FFF;
    PrevSample[0] = 0;
    PrevSample[1] = 0;
    PrevSample[2] = 0;
    CurSample = 0;

    FIFOReadPos = 0;
    FIFOWritePos = 0;
    FIFOReadOffset = 0;
    FIFOLevel = 0;

    // when starting a channel, buffer data
    if (((Cnt >> 29) & 0x3) != 3)
    {
        FIFO_BufferData();
        FIFO_BufferData();
    }
}

void Channel::NextSample_PCM8()
{
    Pos++;
    if (Pos < 0) return;
    if (Pos >= (LoopPos + Length))
    {
        u32 repeat = (Cnt >> 27) & 0x3;
        if (repeat & 1)
        {
            Pos = LoopPos;
        }
        else if (repeat & 2)
        {
            CurSample = 0;
            Cnt &= ~(1<<31);
            return;
        }
    }

    s8 val = FIFO_ReadData<s8>();
    CurSample = val << 8;
}

void Channel::NextSample_PCM16()
{
    Pos++;
    if (Pos < 0) return;
    if ((Pos<<1) >= (LoopPos + Length))
    {
        u32 repeat = (Cnt >> 27) & 0x3;
        if (repeat & 1)
        {
            Pos = LoopPos>>1;
        }
        else if (repeat & 2)
        {
            CurSample = 0;
            Cnt &= ~(1<<31);
            return;
        }
    }

    s16 val = FIFO_ReadData<s16>();
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
            u32 header = FIFO_ReadData<u32>();
            ADPCMVal = (s32)(s16)(header & 0xFFFF);
            ADPCMIndex = (header >> 16) & 0x7F;
            if (ADPCMIndex > 88) ADPCMIndex = 88;

            ADPCMValLoop = ADPCMVal;
            ADPCMIndexLoop = ADPCMIndex;
        }

        return;
    }

    if ((Pos>>1) >= (LoopPos + Length))
    {
        u32 repeat = (Cnt >> 27) & 0x3;
        if (repeat & 1)
        {
            Pos = LoopPos<<1;
            ADPCMVal = ADPCMValLoop;
            ADPCMIndex = ADPCMIndexLoop;
            ADPCMCurByte = FIFO_ReadData<u8>();
        }
        else if (repeat & 2)
        {
            CurSample = 0;
            Cnt &= ~(1<<31);
            return;
        }
    }
    else
    {
        if (!(Pos & 0x1))
            ADPCMCurByte = FIFO_ReadData<u8>();
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
s32 Channel::Run()
{
    if (!(Cnt & (1<<31))) return 0;

    if ((type < 3) && ((Length+LoopPos) < 16)) return 0;

    if (KeyOn)
    {
        Start();
        KeyOn = false;
    }

    Timer += 512; // 1 sample = 512 cycles at 16MHz

    while (Timer >> 16)
    {
        Timer = TimerReload + (Timer - 0x10000);

        // for optional interpolation: save previous samples
        // the interpolated audio will be delayed by a couple samples,
        // but it's easier to deal with this way
        if ((type < 3) && (InterpType != 0))
        {
            PrevSample[2] = PrevSample[1];
            PrevSample[1] = PrevSample[0];
            PrevSample[0] = CurSample;
        }

        switch (type)
        {
        case 0: NextSample_PCM8(); break;
        case 1: NextSample_PCM16(); break;
        case 2: NextSample_ADPCM(); break;
        case 3: NextSample_PSG(); break;
        case 4: NextSample_Noise(); break;
        }
    }

    s32 val = (s32)CurSample;

    // interpolation (emulation improvement, not a hardware feature)
    if ((type < 3) && (InterpType != 0))
    {
        s32 samplepos = ((Timer - TimerReload) * 0x100) / (0x10000 - TimerReload);
        if (samplepos > 0xFF) samplepos = 0xFF;

        switch (InterpType)
        {
        case 1: // linear
            val = ((val           * samplepos) +
                   (PrevSample[0] * (0xFF-samplepos))) >> 8;
            break;

        case 2: // cosine
            val = ((val           * InterpCos[samplepos]) +
                   (PrevSample[0] * InterpCos[0xFF-samplepos])) >> 14;
            break;

        case 3: // cubic
            val = ((PrevSample[2] * InterpCubic[samplepos][0]) +
                   (PrevSample[1] * InterpCubic[samplepos][1]) +
                   (PrevSample[0] * InterpCubic[samplepos][2]) +
                   (val           * InterpCubic[samplepos][3])) >> 14;
            break;
        }
    }

    val <<= VolumeShift;
    val *= Volume;
    return val;
}

void Channel::PanOutput(s32 in, s32& left, s32& right)
{
    left += ((s64)in * (128-Pan)) >> 10;
    right += ((s64)in * Pan) >> 10;
}


CaptureUnit::CaptureUnit(u32 num)
{
    Num = num;
}

CaptureUnit::~CaptureUnit()
{
}

void CaptureUnit::Reset()
{
    if (NDS::ConsoleType == 1)
        BusWrite32 = DSi::ARM7Write32;
    else
        BusWrite32 = NDS::ARM7Write32;

    SetCnt(0);
    DstAddr = 0;
    TimerReload = 0;
    Length = 0;

    Timer = 0;

    Pos = 0;
    FIFOReadPos = 0;
    FIFOWritePos = 0;
    FIFOWriteOffset = 0;
    FIFOLevel = 0;
}

void CaptureUnit::DoSavestate(Savestate* file)
{
    file->Var8(&Cnt);
    file->Var32(&DstAddr);
    file->Var16(&TimerReload);
    file->Var32(&Length);

    file->Var32(&Timer);
    file->Var32((u32*)&Pos);

    file->Var32(&FIFOReadPos);
    file->Var32(&FIFOWritePos);
    file->Var32(&FIFOWriteOffset);
    file->Var32(&FIFOLevel);
    file->VarArray(FIFO, 4*4);
}

void CaptureUnit::FIFO_FlushData()
{
    for (u32 i = 0; i < 4; i++)
    {
        BusWrite32(DstAddr + FIFOWriteOffset, FIFO[FIFOReadPos]);

        FIFOReadPos++;
        FIFOReadPos &= 0x3;
        FIFOLevel -= 4;

        FIFOWriteOffset += 4;
        if (FIFOWriteOffset >= Length)
        {
            FIFOWriteOffset = 0;
            break;
        }
    }
}

template<typename T>
void CaptureUnit::FIFO_WriteData(T val)
{
    *(T*)&((u8*)FIFO)[FIFOWritePos] = val;

    FIFOWritePos += sizeof(T);
    FIFOWritePos &= 0xF;
    FIFOLevel += sizeof(T);

    if (FIFOLevel >= 16)
        FIFO_FlushData();
}

void CaptureUnit::Run(s32 sample)
{
    Timer += 512;

    if (Cnt & 0x08)
    {
        while (Timer >> 16)
        {
            Timer = TimerReload + (Timer - 0x10000);

            FIFO_WriteData<s8>((s8)(sample >> 8));
            Pos++;
            if (Pos >= Length)
            {
                if (FIFOLevel >= 4)
                    FIFO_FlushData();

                if (Cnt & 0x04)
                {
                    Cnt &= 0x7F;
                    return;
                }
                else
                    Pos = 0;
            }
        }
    }
    else
    {
        while (Timer >> 16)
        {
            Timer = TimerReload + (Timer - 0x10000);

            FIFO_WriteData<s16>((s16)sample);
            Pos += 2;
            if (Pos >= Length)
            {
                if (FIFOLevel >= 4)
                    FIFO_FlushData();

                if (Cnt & 0x04)
                {
                    Cnt &= 0x7F;
                    return;
                }
                else
                    Pos = 0;
            }
        }
    }
}


void Mix(u32 dummy)
{
    s32 left = 0, right = 0;
    s32 leftoutput = 0, rightoutput = 0;

    if (Cnt & (1<<15))
    {
        s32 ch0 = Channels[0]->DoRun();
        s32 ch1 = Channels[1]->DoRun();
        s32 ch2 = Channels[2]->DoRun();
        s32 ch3 = Channels[3]->DoRun();

        // TODO: addition from capture registers
        Channels[0]->PanOutput(ch0, left, right);
        Channels[2]->PanOutput(ch2, left, right);

        if (!(Cnt & (1<<12))) Channels[1]->PanOutput(ch1, left, right);
        if (!(Cnt & (1<<13))) Channels[3]->PanOutput(ch3, left, right);

        for (int i = 4; i < 16; i++)
        {
            Channel* chan = Channels[i];

            s32 channel = chan->DoRun();
            chan->PanOutput(channel, left, right);
        }

        // sound capture
        // TODO: other sound capture sources, along with their bugs

        if (Capture[0]->Cnt & (1<<7))
        {
            s32 val = left;

            val >>= 8;
            if      (val < -0x8000) val = -0x8000;
            else if (val > 0x7FFF)  val = 0x7FFF;

            Capture[0]->Run(val);
        }

        if (Capture[1]->Cnt & (1<<7))
        {
            s32 val = right;

            val >>= 8;
            if      (val < -0x8000) val = -0x8000;
            else if (val > 0x7FFF)  val = 0x7FFF;

            Capture[1]->Run(val);
        }

        // final output

        switch (Cnt & 0x0300)
        {
        case 0x0000: // left mixer
            leftoutput = left;
            break;
        case 0x0100: // channel 1
            {
                s32 pan = 128 - Channels[1]->Pan;
                leftoutput = ((s64)ch1 * pan) >> 10;
            }
            break;
        case 0x0200: // channel 3
            {
                s32 pan = 128 - Channels[3]->Pan;
                leftoutput = ((s64)ch3 * pan) >> 10;
            }
            break;
        case 0x0300: // channel 1+3
            {
                s32 pan1 = 128 - Channels[1]->Pan;
                s32 pan3 = 128 - Channels[3]->Pan;
                leftoutput = (((s64)ch1 * pan1) >> 10) + (((s64)ch3 * pan3) >> 10);
            }
            break;
        }

        switch (Cnt & 0x0C00)
        {
        case 0x0000: // right mixer
            rightoutput = right;
            break;
        case 0x0400: // channel 1
            {
                s32 pan = Channels[1]->Pan;
                rightoutput = ((s64)ch1 * pan) >> 10;
            }
            break;
        case 0x0800: // channel 3
            {
                s32 pan = Channels[3]->Pan;
                rightoutput = ((s64)ch3 * pan) >> 10;
            }
            break;
        case 0x0C00: // channel 1+3
            {
                s32 pan1 = Channels[1]->Pan;
                s32 pan3 = Channels[3]->Pan;
                rightoutput = (((s64)ch1 * pan1) >> 10) + (((s64)ch3 * pan3) >> 10);
            }
            break;
        }
    }

    leftoutput = ((s64)leftoutput * MasterVolume) >> 7;
    rightoutput = ((s64)rightoutput * MasterVolume) >> 7;

    leftoutput >>= 8;
    rightoutput >>= 8;

    // Add SOUNDBIAS value
    // The value used by all commercial games is 0x200, so we subtract that so it won't offset the final sound output.
    if (ApplyBias)
    {
        leftoutput += (Bias << 6) - 0x8000;
        rightoutput += (Bias << 6) - 0x8000;
    }

    if      (leftoutput < -0x8000) leftoutput = -0x8000;
    else if (leftoutput > 0x7FFF)  leftoutput = 0x7FFF;
    if      (rightoutput < -0x8000) rightoutput = -0x8000;
    else if (rightoutput > 0x7FFF)  rightoutput = 0x7FFF;

    // The original DS and DS lite degrade the output from 16 to 10 bit before output
    if (Degrade10Bit)
    {
        leftoutput &= 0xFFFFFFC0;
        rightoutput &= 0xFFFFFFC0;
    }

    // OutputBufferFrame can never get full because it's
    // transfered to OutputBuffer at the end of the frame
    // FIXME: apparently this does happen!!!
    if (OutputBackbufferWritePosition * 2 < OutputBufferSize - 1)
    {
        OutputBackbuffer[OutputBackbufferWritePosition    ] = leftoutput >> 1;
        OutputBackbuffer[OutputBackbufferWritePosition + 1] = rightoutput >> 1;
        OutputBackbufferWritePosition += 2;
    }

    NDS::ScheduleEvent(NDS::Event_SPU, true, 1024, Mix, 0);
}

void TransferOutput()
{
    Platform::Mutex_Lock(AudioLock);
    for (u32 i = 0; i < OutputBackbufferWritePosition; i += 2)
    {
        OutputFrontBuffer[OutputFrontBufferWritePosition    ] = OutputBackbuffer[i   ];
        OutputFrontBuffer[OutputFrontBufferWritePosition + 1] = OutputBackbuffer[i + 1];

        OutputFrontBufferWritePosition += 2;
        OutputFrontBufferWritePosition &= OutputBufferSize*2-1;
        if (OutputFrontBufferWritePosition == OutputFrontBufferReadPosition)
        {
            // advance the read position too, to avoid losing the entire FIFO
            OutputFrontBufferReadPosition += 2;
            OutputFrontBufferReadPosition &= OutputBufferSize*2-1;
        }
    }
    OutputBackbufferWritePosition = 0;
    Platform::Mutex_Unlock(AudioLock);
}

void TrimOutput()
{
    Platform::Mutex_Lock(AudioLock);
    const int halflimit = (OutputBufferSize / 2);

    int readpos = OutputFrontBufferWritePosition - (halflimit*2);
    if (readpos < 0) readpos += (OutputBufferSize*2);

    OutputFrontBufferReadPosition = readpos;
    Platform::Mutex_Unlock(AudioLock);
}

void DrainOutput()
{
    Platform::Mutex_Lock(AudioLock);
    OutputFrontBufferWritePosition = 0;
    OutputFrontBufferReadPosition = 0;
    Platform::Mutex_Unlock(AudioLock);
}

void InitOutput()
{
    Platform::Mutex_Lock(AudioLock);
    memset(OutputBackbuffer, 0, 2*OutputBufferSize*2);
    memset(OutputFrontBuffer, 0, 2*OutputBufferSize*2);
    OutputFrontBufferReadPosition = 0;
    OutputFrontBufferWritePosition = 0;
    Platform::Mutex_Unlock(AudioLock);
}

int GetOutputSize()
{
    Platform::Mutex_Lock(AudioLock);

    int ret;
    if (OutputFrontBufferWritePosition >= OutputFrontBufferReadPosition)
        ret = OutputFrontBufferWritePosition - OutputFrontBufferReadPosition;
    else
        ret = (OutputBufferSize*2) - OutputFrontBufferReadPosition + OutputFrontBufferWritePosition;

    ret >>= 1;

    Platform::Mutex_Unlock(AudioLock);
    return ret;
}

void Sync(bool wait)
{
    // this function is currently not used anywhere
    // depending on the usage context the thread safety measures could be made
    // a lot faster

    // sync to audio output in case the core is running too fast
    // * wait=true: wait until enough audio data has been played
    // * wait=false: merely skip some audio data to avoid a FIFO overflow

    const int halflimit = (OutputBufferSize / 2);

    if (wait)
    {
        // TODO: less CPU-intensive wait?
        while (GetOutputSize() > halflimit);
    }
    else if (GetOutputSize() > halflimit)
    {
        Platform::Mutex_Lock(AudioLock);

        int readpos = OutputFrontBufferWritePosition - (halflimit*2);
        if (readpos < 0) readpos += (OutputBufferSize*2);

        OutputFrontBufferReadPosition = readpos;

        Platform::Mutex_Unlock(AudioLock);
    }
}

int ReadOutput(s16* data, int samples)
{
    Platform::Mutex_Lock(AudioLock);
    if (OutputFrontBufferReadPosition == OutputFrontBufferWritePosition)
    {
        Platform::Mutex_Unlock(AudioLock);
        return 0;
    }

    for (int i = 0; i < samples; i++)
    {
        *data++ = OutputFrontBuffer[OutputFrontBufferReadPosition];
        *data++ = OutputFrontBuffer[OutputFrontBufferReadPosition + 1];

        OutputFrontBufferReadPosition += 2;
        OutputFrontBufferReadPosition &= ((2*OutputBufferSize)-1);

        if (OutputFrontBufferWritePosition == OutputFrontBufferReadPosition)
        {
            Platform::Mutex_Unlock(AudioLock);
            return i+1;
        }
    }

    Platform::Mutex_Unlock(AudioLock);
    return samples;
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

        case 0x04000508: return Capture[0]->Cnt;
        case 0x04000509: return Capture[1]->Cnt;
        }
    }

    Log(LogLevel::Warn, "unknown SPU read8 %08X\n", addr);
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

        case 0x04000508: return Capture[0]->Cnt | (Capture[1]->Cnt << 8);
        }
    }

    Log(LogLevel::Warn, "unknown SPU read16 %08X\n", addr);
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

        case 0x04000508: return Capture[0]->Cnt | (Capture[1]->Cnt << 8);

        case 0x04000510: return Capture[0]->DstAddr;
        case 0x04000518: return Capture[1]->DstAddr;
        }
    }

    Log(LogLevel::Warn, "unknown SPU read32 %08X\n", addr);
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

        case 0x04000508:
            Capture[0]->SetCnt(val);
            if (val & 0x03) Log(LogLevel::Warn, "!! UNSUPPORTED SPU CAPTURE MODE %02X\n", val);
            return;
        case 0x04000509:
            Capture[1]->SetCnt(val);
            if (val & 0x03) Log(LogLevel::Warn, "!! UNSUPPORTED SPU CAPTURE MODE %02X\n", val);
            return;
        }
    }

    Log(LogLevel::Warn, "unknown SPU write8 %08X %02X\n", addr, val);
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
        case 0x8:
            chan->SetTimerReload(val);
            if      ((addr & 0xF0) == 0x10) Capture[0]->SetTimerReload(val);
            else if ((addr & 0xF0) == 0x30) Capture[1]->SetTimerReload(val);
            return;
        case 0xA: chan->SetLoopPos(val); return;

        case 0xC: chan->SetLength(((chan->Length >> 2) & 0xFFFF0000) | val); return;
        case 0xE: chan->SetLength(((chan->Length >> 2) & 0x0000FFFF) | (val << 16)); return;
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

        case 0x04000508:
            Capture[0]->SetCnt(val & 0xFF);
            Capture[1]->SetCnt(val >> 8);
            if (val & 0x0303) Log(LogLevel::Warn, "!! UNSUPPORTED SPU CAPTURE MODE %04X\n", val);
            return;

        case 0x04000514: Capture[0]->SetLength(val); return;
        case 0x0400051C: Capture[1]->SetLength(val); return;
        }
    }

    Log(LogLevel::Warn, "unknown SPU write16 %08X %04X\n", addr, val);
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
            chan->SetLoopPos(val >> 16);
            val &= 0xFFFF;
            chan->SetTimerReload(val);
            if      ((addr & 0xF0) == 0x10) Capture[0]->SetTimerReload(val);
            else if ((addr & 0xF0) == 0x30) Capture[1]->SetTimerReload(val);
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

        case 0x04000508:
            Capture[0]->SetCnt(val & 0xFF);
            Capture[1]->SetCnt(val >> 8);
            if (val & 0x0303) Log(LogLevel::Warn, "!! UNSUPPORTED SPU CAPTURE MODE %04X\n", val);
            return;

        case 0x04000510: Capture[0]->SetDstAddr(val); return;
        case 0x04000514: Capture[0]->SetLength(val & 0xFFFF); return;
        case 0x04000518: Capture[1]->SetDstAddr(val); return;
        case 0x0400051C: Capture[1]->SetLength(val & 0xFFFF); return;
        }
    }
}

}
