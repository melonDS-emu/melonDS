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

#ifndef NDS_H
#define NDS_H

#include "types.h"

namespace NDS
{

/*#define SCHED_BUF_LEN 64

typedef struct _SchedEvent
{
    u32 Delay;
    void (*Func)(u32);
    u32 Param;
    struct _SchedEvent* PrevEvent;
    struct _SchedEvent*  NextEvent;

} SchedEvent;*/

enum
{
    Event_LCD = 0,

    Event_Timer9_0,
    Event_Timer9_1,
    Event_Timer9_2,
    Event_Timer9_3,
    Event_Timer7_0,
    Event_Timer7_1,
    Event_Timer7_2,
    Event_Timer7_3,

    Event_MAX
};

typedef struct
{
    void (*Func)(u32 param);
    s32 WaitCycles;
    u32 Param;

} SchedEvent;

enum
{
    IRQ_VBlank = 0,
    IRQ_HBlank,
    IRQ_VCount,
    IRQ_Timer0,
    IRQ_Timer1,
    IRQ_Timer2,
    IRQ_Timer3,
    IRQ_RTC,
    IRQ_DMA0,
    IRQ_DMA1,
    IRQ_DMA2,
    IRQ_DMA3,
    IRQ_Keypad,
    IRQ_GBASlot,
    IRQ_Unused14,
    IRQ_Unused15,
    IRQ_IPCSync,
    IRQ_IPCSendDone,
    IRQ_IPCRecv,
    IRQ_CartSendDone,
    IRQ_CartIREQMC,
    IRQ_GXFIFO,
    IRQ_LidOpen,
    IRQ_SPI,
    IRQ_Wifi
};

typedef struct
{
    u16 Reload;
    u16 Cnt;
    u32 Counter;
    u32 CycleShift;
    //SchedEvent* Event;

} Timer;

// hax
extern u32 IME[2];
extern u32 IE[2];
extern u32 IF[2];
extern Timer Timers[8];

extern u16 ExMemCnt[2];
extern u8 ROMSeed0[2*8];
extern u8 ROMSeed1[2*8];

extern u8 ARM9BIOS[0x1000];
extern u8 ARM7BIOS[0x4000];

bool Init();
void DeInit();
void Reset();

void LoadROM(const char* path, bool direct);
void SetupDirectBoot();

void RunFrame();

void PressKey(u32 key);
void ReleaseKey(u32 key);
void TouchScreen(u16 x, u16 y);
void ReleaseScreen();

/*SchedEvent* ScheduleEvent(s32 Delay, void (*Func)(u32), u32 Param);
void CancelEvent(SchedEvent* event);
void RunEvents(s32 cycles);*/
void ScheduleEvent(u32 id, bool periodic, s32 delay, void (*func)(u32), u32 param);
void CancelEvent(u32 id);

// DO NOT CALL FROM ARM7!!
void CompensateARM7();

void debug(u32 p);

void Halt();

void MapSharedWRAM(u8 val);

void SetIRQ(u32 cpu, u32 irq);
void ClearIRQ(u32 cpu, u32 irq);
bool HaltInterrupted(u32 cpu);
void StopCPU(u32 cpu, u32 mask);
void ResumeCPU(u32 cpu, u32 mask);

void CheckDMAs(u32 cpu, u32 mode);

u8 ARM9Read8(u32 addr);
u16 ARM9Read16(u32 addr);
u32 ARM9Read32(u32 addr);
void ARM9Write8(u32 addr, u8 val);
void ARM9Write16(u32 addr, u16 val);
void ARM9Write32(u32 addr, u32 val);

u8 ARM7Read8(u32 addr);
u16 ARM7Read16(u32 addr);
u32 ARM7Read32(u32 addr);
void ARM7Write8(u32 addr, u8 val);
void ARM7Write16(u32 addr, u16 val);
void ARM7Write32(u32 addr, u32 val);

u8 ARM9IORead8(u32 addr);
u16 ARM9IORead16(u32 addr);
u32 ARM9IORead32(u32 addr);
void ARM9IOWrite8(u32 addr, u8 val);
void ARM9IOWrite16(u32 addr, u16 val);
void ARM9IOWrite32(u32 addr, u32 val);

u8 ARM7IORead8(u32 addr);
u16 ARM7IORead16(u32 addr);
u32 ARM7IORead32(u32 addr);
void ARM7IOWrite8(u32 addr, u8 val);
void ARM7IOWrite16(u32 addr, u16 val);
void ARM7IOWrite32(u32 addr, u32 val);

}

#endif // NDS_H
