
#ifndef NDS_H
#define NDS_H

#include "types.h"

namespace NDS
{

#define SCHED_BUF_LEN 64

typedef struct _SchedEvent
{
    u32 Delay;
    void (*Func)(u32);
    u32 Param;
    struct _SchedEvent* PrevEvent;
    struct _SchedEvent*  NextEvent;

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
    u16 Control;
    u16 Counter;
    SchedEvent* Event;

} Timer;

extern u32 ARM9ITCMSize;
extern u32 ARM9DTCMBase, ARM9DTCMSize;

void Init();
void Reset();

void RunFrame();

SchedEvent* ScheduleEvent(s32 Delay, void (*Func)(u32), u32 Param);
void CancelEvent(SchedEvent* event);
void RunEvents(s32 cycles);

// DO NOT CALL FROM ARM7!!
void CompensateARM7();

void Halt();

void MapSharedWRAM();

void TriggerIRQ(u32 cpu, u32 irq);

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

}

#endif // NDS_H
