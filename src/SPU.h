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

#ifndef SPU_H
#define SPU_H

namespace SPU
{

bool Init();
void DeInit();
void Reset();

void SetBias(u16 bias);

void Mix(u32 samples);

void ReadOutput(s16* data, int samples);

u8 Read8(u32 addr);
u16 Read16(u32 addr);
u32 Read32(u32 addr);
void Write8(u32 addr, u8 val);
void Write16(u32 addr, u16 val);
void Write32(u32 addr, u32 val);

class Channel
{
public:
    Channel(u32 num);
    ~Channel();
    void Reset();

    u32 Num;

    u32 Cnt;
    u32 SrcAddr;
    u16 TimerReload;
    u32 LoopPos;
    u32 Length;

    u8 Volume;
    u8 VolumeShift;
    u8 Pan;

    u32 Timer;
    s32 Pos;
    s16 CurSample;
    u16 NoiseVal;

    s32 ADPCMVal;
    s32 ADPCMIndex;
    s32 ADPCMValLoop;
    s32 ADPCMIndexLoop;
    u8 ADPCMCurByte;

    void SetCnt(u32 val)
    {
        u32 oldcnt = Cnt;
        Cnt = val & 0xFF7F837F;

        Volume = Cnt & 0x7F;
        if (Volume == 127) Volume++;

        const u8 volshift[4] = {4, 3, 2, 0};
        VolumeShift = volshift[(Cnt >> 8) & 0x3];

        Pan = (Cnt >> 16) & 0x7F;
        if (Pan == 127) Pan++;

        if ((val & (1<<31)) && !(oldcnt & (1<<31)))
        {
            Start();
        }
    }

    void SetSrcAddr(u32 val) { SrcAddr = val & 0x07FFFFFC; }
    void SetTimerReload(u32 val) { TimerReload = val & 0xFFFF; }
    void SetLoopPos(u32 val) { LoopPos = (val & 0xFFFF) << 2; }
    void SetLength(u32 val) { Length = (val & 0x001FFFFF) << 2; }

    void Start();

    void NextSample_PCM8();
    void NextSample_PCM16();
    void NextSample_ADPCM();
    void NextSample_PSG();
    void NextSample_Noise();

    template<u32 type> void Run(s32* buf, u32 samples);

    void DoRun(s32* buf, u32 samples)
    {
        switch ((Cnt >> 29) & 0x3)
        {
        case 0: Run<0>(buf, samples); break;
        case 1: Run<1>(buf, samples); break;
        case 2: Run<2>(buf, samples); break;
        case 3:
            if      (Num >= 14) Run<4>(buf, samples);
            else if (Num >= 8)  Run<3>(buf, samples);
            break;
        }
    }
};

class CaptureUnit
{
public:
    CaptureUnit(u32 num);
    ~CaptureUnit();
    void Reset();

    u32 Num;

    u8 Cnt;
    u32 DstAddr;
    u16 TimerReload;
    u32 Length;

    u32 Timer;
    s32 Pos;

    void SetCnt(u8 val)
    {
        if ((val & 0x80) && !(Cnt & 0x80))
            Start();

        val &= 0x8F;
        if (!(val & 0x80)) val &= ~0x01;
        Cnt = val;
    }

    void SetDstAddr(u32 val) { DstAddr = val & 0x07FFFFFC; }
    void SetTimerReload(u32 val) { TimerReload = val & 0xFFFF; }
    void SetLength(u32 val) { Length = val << 2; if (Length == 0) Length = 4; }

    void Start() { Timer = TimerReload; }

    void Run(s32 sample);
};

}

#endif // SPU_H
