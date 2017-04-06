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

    u32 Timer;
    u32 Pos;
    s16 CurSample;

    void SetCnt(u32 val)
    {
        if ((val & (1<<31)) && !(Cnt & (1<<31)))
        {
            Start();
        }

        Cnt = val & 0xFF7F837F;
        //if(Num==8)printf("chan %d volume: %d\n", Num, val&0x7F);
    }

    void SetSrcAddr(u32 val) { SrcAddr = val & 0x07FFFFFF; }
    void SetTimerReload(u32 val) { TimerReload = val & 0xFFFF;if(Num==8) printf("chan8 timer %04X\n", TimerReload);}
    void SetLoopPos(u32 val) { LoopPos = (val & 0xFFFF) << 2; }
    void SetLength(u32 val) { Length = (val & 0x001FFFFF) << 2; }

    void Start();

    void NextSample_PSG();

    void Run(s32* buf, u32 samples);
};

}

#endif // SPU_H
