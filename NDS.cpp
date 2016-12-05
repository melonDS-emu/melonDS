#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "ARM.h"
#include "CP15.h"
#include "SPI.h"


namespace NDS
{

SchedEvent SchedBuffer[SCHED_BUF_LEN];
SchedEvent* SchedQueue;

bool NeedReschedule;

ARM* ARM9;
ARM* ARM7;

s32 ARM9Cycles, ARM7Cycles;
s32 CompensatedCycles;
s32 SchedCycles;

u8 ARM9BIOS[0x1000];
u8 ARM7BIOS[0x4000];

u8 MainRAM[0x400000];

u8 SharedWRAM[0x8000];
u8 WRAMCnt;
u8* SWRAM_ARM9;
u8* SWRAM_ARM7;
u32 SWRAM_ARM9Mask;
u32 SWRAM_ARM7Mask;

u8 ARM7WRAM[0x10000];

u8 ARM9ITCM[0x8000];
u32 ARM9ITCMSize;
u8 ARM9DTCM[0x4000];
u32 ARM9DTCMBase, ARM9DTCMSize;

// IO shit
u32 IME[2];
u32 IE[2], IF[2];

Timer Timers[8];

u16 IPCSync9, IPCSync7;

u16 _soundbias; // temp

bool Running;


void Init()
{
    ARM9 = new ARM(0);
    ARM7 = new ARM(1);

    SPI::Init();

    Reset();
}

void Reset()
{
    FILE* f;

    f = fopen("bios9.bin", "rb");
    if (!f)
        printf("ARM9 BIOS not found\n");
    else
    {
        fseek(f, 0, SEEK_SET);
        fread(ARM9BIOS, 0x1000, 1, f);

        printf("ARM9 BIOS loaded: %08X\n", ARM9Read32(0xFFFF0000));
        fclose(f);
    }

    f = fopen("bios7.bin", "rb");
    if (!f)
        printf("ARM7 BIOS not found\n");
    else
    {
        fseek(f, 0, SEEK_SET);
        fread(ARM7BIOS, 0x4000, 1, f);

        printf("ARM7 BIOS loaded: %08X\n", ARM7Read32(0x00000000));
        fclose(f);
    }

    memset(MainRAM, 0, 0x400000);
    memset(SharedWRAM, 0, 0x8000);
    memset(ARM7WRAM, 0, 0x10000);
    memset(ARM9ITCM, 0, 0x8000);
    memset(ARM9DTCM, 0, 0x4000);

    WRAMCnt = 0;
    MapSharedWRAM();

    ARM9ITCMSize = 0;
    ARM9DTCMBase = 0xFFFFFFFF;
    ARM9DTCMSize = 0;

    IME[0] = 0;
    IME[1] = 0;

    IPCSync9 = 0;
    IPCSync7 = 0;

    ARM9->Reset();
    ARM7->Reset();
    CP15::Reset();

    memset(Timers, 0, 8*sizeof(Timer));

    SPI::Reset();

    memset(SchedBuffer, 0, sizeof(SchedEvent)*SCHED_BUF_LEN);
    SchedQueue = NULL;

    ARM9Cycles = 0;
    ARM7Cycles = 0;
    SchedCycles = 0;

    _soundbias = 0;

    Running = true; // hax
}


void RunFrame()
{
    s32 framecycles = 560190<<1;

    const s32 maxcycles = 16;

    while (Running && framecycles>0)
    {
        //ARM9Cycles = ARM9->Execute(32 + ARM9Cycles);
        //ARM7Cycles = ARM7->Execute(16 + ARM7Cycles);

        //framecycles -= 32;
        s32 cyclestorun = maxcycles;
        // TODO: scheduler integration here

        CompensatedCycles = ARM9Cycles;
        s32 c9 = ARM9->Execute(cyclestorun - ARM9Cycles);
        ARM9Cycles = c9 - cyclestorun;
        c9 -= CompensatedCycles;

        s32 c7 = ARM7->Execute((c9 - ARM7Cycles) >> 1) << 1;
        ARM7Cycles = c7 - c9;

        RunEvents(c9);
        framecycles -= cyclestorun;
    }
}

SchedEvent* ScheduleEvent(s32 Delay, void (*Func)(u32), u32 Param)
{
    // find a free entry
    u32 entry = -1;
    for (int i = 0; i < SCHED_BUF_LEN; i++)
    {
        if (SchedBuffer[i].Func == NULL)
        {
            entry = i;
            break;
        }
    }

    if (entry == -1)
    {
        printf("!! SCHEDULER BUFFER FULL\n");
        return NULL;
    }

    SchedEvent* evt = &SchedBuffer[entry];
    evt->Func = Func;
    evt->Param = Param;

    SchedEvent* cur = SchedQueue;
    SchedEvent* prev = NULL;
    for (;;)
    {
        if (cur == NULL) break;
        if (cur->Delay > Delay) break;

        Delay -= cur->Delay;
        prev = cur;
        cur = cur->NextEvent;
    }

    // so, we found it. we insert our event before 'cur'.
    evt->Delay = Delay;

    if (cur == NULL)
    {
        if (prev == NULL)
        {
            // list empty
            SchedQueue = evt;
            evt->PrevEvent = NULL;
            evt->NextEvent = NULL;
        }
        else
        {
            // inserting at the end of the list
            evt->PrevEvent = prev;
            evt->NextEvent = NULL;
            prev->NextEvent = evt;
        }
    }
    else
    {
        evt->NextEvent = cur;
        evt->PrevEvent = cur->PrevEvent;

        if (evt->PrevEvent)
            evt->PrevEvent->NextEvent = evt;

        cur->PrevEvent = evt;
        cur->Delay -= evt->Delay;
    }

    return evt;
}

void CancelEvent(SchedEvent* event)
{
    event->Func = NULL;

    // unlink

    if (event->PrevEvent)
        event->PrevEvent->NextEvent = event->NextEvent;
    else
        SchedQueue = event->NextEvent;

    if (event->NextEvent)
        event->NextEvent->PrevEvent = event->PrevEvent;
}

void RunEvents(s32 cycles)
{
    SchedCycles += cycles;

    SchedEvent* evt = SchedQueue;
    while (evt && evt->Delay <= SchedCycles)
    {
        evt->Func(evt->Param);
        evt->Func = NULL;
        SchedCycles -= evt->Delay;

        evt = evt->NextEvent;
    }

    SchedQueue = evt;
    if (evt) evt->PrevEvent = NULL;
}

void CompensateARM7()
{
    s32 c9 = ARM9->Cycles - CompensatedCycles;
    CompensatedCycles = ARM9->Cycles;

    s32 c7 = ARM7->Execute((c9 - ARM7Cycles) >> 1) << 1;
    ARM7Cycles = c7 - c9;

    RunEvents(c9);
}


void Halt()
{
    Running = false;
}


void MapSharedWRAM()
{
    switch (WRAMCnt & 0x3)
    {
    case 0:
        SWRAM_ARM9 = &SharedWRAM[0];
        SWRAM_ARM9Mask = 0x7FFF;
        SWRAM_ARM7 = NULL;
        SWRAM_ARM7Mask = 0;
        break;

    case 1:
        SWRAM_ARM9 = &SharedWRAM[0x4000];
        SWRAM_ARM9Mask = 0x3FFF;
        SWRAM_ARM7 = &SharedWRAM[0];
        SWRAM_ARM7Mask = 0x3FFF;
        break;

    case 2:
        SWRAM_ARM9 = &SharedWRAM[0];
        SWRAM_ARM9Mask = 0x3FFF;
        SWRAM_ARM7 = &SharedWRAM[0x4000];
        SWRAM_ARM7Mask = 0x3FFF;
        break;

    case 3:
        SWRAM_ARM9 = NULL;
        SWRAM_ARM9Mask = 0;
        SWRAM_ARM7 = &SharedWRAM[0];
        SWRAM_ARM7Mask = 0x7FFF;
        break;
    }
}


void TriggerIRQ(u32 cpu, u32 irq)
{
    if (!(IME[cpu] & 0x1)) return;

    irq = 1 << irq;
    if (!(IE[cpu] & irq)) return;

    IF[cpu] |= irq;
    (cpu?ARM7:ARM9)->TriggerIRQ();
}



const s32 TimerPrescaler[4] = {2, 128, 512, 2048};

void TimerIncrement(u32 param)
{
    Timer* timer = &Timers[param];

    u32 tid = param & 0x3;
    u32 cpu = param >> 2;

    for (;;)
    {
        timer->Counter++;
        if (param==7)printf("timer%d increment %04X %04X %04X\n", param, timer->Control, timer->Counter, timer->Reload);
        if (tid == (param&0x3))
            timer->Event = ScheduleEvent(TimerPrescaler[timer->Control&0x3], TimerIncrement, param);

        if (timer->Counter == 0)
        {
            timer->Counter = timer->Reload;

            if (timer->Control & (1<<6))
                TriggerIRQ(cpu, IRQ_Timer0 + tid);

            // cascade
            if (tid == 3)
                break;
            timer++;
            if ((timer->Control & 0x84) != 0x84)
                break;
            tid++;
            continue;
        }

        break;
    }
}

void TimerStart(u32 id, u16 cnt)
{
    Timer* timer = &Timers[id];
    u16 curstart = timer->Control & (1<<7);
    u16 newstart = cnt & (1<<7);

    printf("timer%d start: %04X %04X\n", id, timer->Control, cnt);

    timer->Control = cnt;

    if ((!curstart) && newstart)
    {
        // start the timer, if it's not a cascading timer
        if (!(cnt & (1<<2)))
        {
            timer->Counter = timer->Reload;
            timer->Event = ScheduleEvent(TimerPrescaler[cnt&0x3], TimerIncrement, id);
        }
        else
            timer->Event = NULL;
    }
    else if (curstart && !newstart)
    {
        if (timer->Event)
            CancelEvent(timer->Event);
    }
}



u8 ARM9Read8(u32 addr)
{
    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u8*)&ARM9BIOS[addr & 0xFFF];
    }
    if (addr < ARM9ITCMSize)
    {
        return *(u8*)&ARM9ITCM[addr & 0x7FFF];
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        return *(u8*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u8*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM9) return *(u8*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask];
        else return 0;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000247:
            return WRAMCnt;
        }
    }

    printf("unknown arm9 read8 %08X\n", addr);
    return 0;
}

u16 ARM9Read16(u32 addr)
{
    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u16*)&ARM9BIOS[addr & 0xFFF];
    }
    if (addr < ARM9ITCMSize)
    {
        return *(u16*)&ARM9ITCM[addr & 0x7FFF];
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        return *(u16*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u16*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM9) return *(u16*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask];
        else return 0;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000100: return Timers[0].Counter;
        case 0x04000102: return Timers[0].Control;
        case 0x04000104: return Timers[1].Counter;
        case 0x04000106: return Timers[1].Control;
        case 0x04000108: return Timers[2].Counter;
        case 0x0400010A: return Timers[2].Control;
        case 0x0400010C: return Timers[3].Counter;
        case 0x0400010E: return Timers[3].Control;

        case 0x04000180: return IPCSync9;
        }
    }

    printf("unknown arm9 read16 %08X\n", addr);
    return 0;
}

u32 ARM9Read32(u32 addr)
{
    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u32*)&ARM9BIOS[addr & 0xFFF];
    }
    if (addr < ARM9ITCMSize)
    {
        return *(u32*)&ARM9ITCM[addr & 0x7FFF];
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        return *(u32*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF];
    }

    if (addr >= 0xFFFF1000)
    {
        Halt();
        /*FILE* f = fopen("ram.bin", "wb");
        fwrite(MainRAM, 0x400000, 1, f);
        fclose(f);
        fopen("wram.bin", "wb");
        fwrite(ARM7WRAM, 0x10000, 1, f);
        fclose(f);
        fopen("swram.bin", "wb");
        fwrite(ARM7WRAM, 0x8000, 1, f);
        fclose(f);*/
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u32*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM9) return *(u32*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask];
        else return 0;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000100: return Timers[0].Counter | (Timers[0].Control << 16);
        case 0x04000104: return Timers[1].Counter | (Timers[1].Control << 16);
        case 0x04000108: return Timers[2].Counter | (Timers[2].Control << 16);
        case 0x0400010C: return Timers[3].Counter | (Timers[3].Control << 16);

        case 0x04000208: return IME[0];
        case 0x04000210: return IE[0];
        case 0x04000214: return IF[0];
        }
    }

    printf("unknown arm9 read32 %08X | %08X %08X %08X\n", addr, ARM9->R[15], ARM9->R[12], ARM9Read32(0x027FF820));
    return 0;
}

void ARM9Write8(u32 addr, u8 val)
{
    if (addr < ARM9ITCMSize)
    {
        *(u8*)&ARM9ITCM[addr & 0x7FFF] = val;
        return;
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        *(u8*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF] = val;
        return;
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        *(u8*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM9) *(u8*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask] = val;
        return;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000247:
            WRAMCnt = val;
            MapSharedWRAM();
            return;
        }
    }

    printf("unknown arm9 write8 %08X %02X\n", addr, val);
}

void ARM9Write16(u32 addr, u16 val)
{
    if (addr < ARM9ITCMSize)
    {
        *(u16*)&ARM9ITCM[addr & 0x7FFF] = val;
        return;
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        *(u16*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF] = val;
        return;
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        *(u16*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM9) *(u16*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask] = val;
        return;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000100: Timers[0].Reload = val; return;
        case 0x04000102: TimerStart(0, val); return;
        case 0x04000104: Timers[1].Reload = val; return;
        case 0x04000106: TimerStart(1, val); return;
        case 0x04000108: Timers[2].Reload = val; return;
        case 0x0400010A: TimerStart(2, val); return;
        case 0x0400010C: Timers[3].Reload = val; return;
        case 0x0400010E: TimerStart(3, val); return;

        case 0x04000180:
            IPCSync7 &= 0xFFF0;
            IPCSync7 |= ((val & 0x0F00) >> 8);
            IPCSync9 &= 0xB0FF;
            IPCSync9 |= (val & 0x4F00);
            if ((val & 0x2000) && (IPCSync7 & 0x4000))
            {
                TriggerIRQ(1, IRQ_IPCSync);
            }
            CompensateARM7();
            return;
        }
    }

    printf("unknown arm9 write16 %08X %04X\n", addr, val);
}

void ARM9Write32(u32 addr, u32 val)
{
    if (addr < ARM9ITCMSize)
    {
        *(u32*)&ARM9ITCM[addr & 0x7FFF] = val;
        return;
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        *(u32*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF] = val;
        return;
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        *(u32*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM9) *(u32*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask] = val;
        return;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000100:
            Timers[0].Reload = val & 0xFFFF;
            TimerStart(0, val>>16);
            return;
        case 0x04000104:
            Timers[1].Reload = val & 0xFFFF;
            TimerStart(1, val>>16);
            return;
        case 0x04000108:
            Timers[2].Reload = val & 0xFFFF;
            TimerStart(2, val>>16);
            return;
        case 0x0400010C:
            Timers[3].Reload = val & 0xFFFF;
            TimerStart(3, val>>16);
            return;

        case 0x04000208: IME[0] = val; return;
        case 0x04000210: IE[0] = val; return;
        case 0x04000214: IF[0] &= ~val; return;
        }
    }

    printf("unknown arm9 write32 %08X %08X | %08X\n", addr, val, ARM9->R[15]);
}



u8 ARM7Read8(u32 addr)
{
    if (addr < 0x00004000)
    {
        return *(u8*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        return *(u8*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM7) return *(u8*)&SWRAM_ARM7[addr & SWRAM_ARM7Mask];
        else return *(u8*)&ARM7WRAM[addr & 0xFFFF];

    case 0x03800000:
        return *(u8*)&ARM7WRAM[addr & 0xFFFF];

    case 0x04000000:
        switch (addr)
        {
        case 0x04000138: return 0; // RTC shit

        case 0x040001C2: return SPI::ReadData();

        case 0x04000241:
            return WRAMCnt;
        }
    }

    printf("unknown arm7 read8 %08X\n", addr);
    return 0;
}

u16 ARM7Read16(u32 addr)
{
    if (addr < 0x00004000)
    {
        return *(u16*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        return *(u16*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM7) return *(u16*)&SWRAM_ARM7[addr & SWRAM_ARM7Mask];
        else return *(u16*)&ARM7WRAM[addr & 0xFFFF];

    case 0x03800000:
        return *(u16*)&ARM7WRAM[addr & 0xFFFF];

    case 0x04000000:
        switch (addr)
        {
        case 0x04000100: return Timers[4].Counter;
        case 0x04000102: return Timers[4].Control;
        case 0x04000104: return Timers[5].Counter;
        case 0x04000106: return Timers[5].Control;
        case 0x04000108: return Timers[6].Counter;
        case 0x0400010A: return Timers[6].Control;
        case 0x0400010C: return Timers[7].Counter;
        case 0x0400010E: return Timers[7].Control;

        case 0x04000180: return IPCSync7;

        case 0x040001C0: return SPI::ReadCnt();
        case 0x040001C2: return SPI::ReadData();

        case 0x04000504: return _soundbias;
        }
    }

    printf("unknown arm7 read16 %08X %08X\n", addr, ARM7->R[15]);
    return 0;
}

u32 ARM7Read32(u32 addr)
{
    if (addr < 0x00004000)
    {
        return *(u32*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        return *(u32*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM7) return *(u32*)&SWRAM_ARM7[addr & SWRAM_ARM7Mask];
        else return *(u32*)&ARM7WRAM[addr & 0xFFFF];

    case 0x03800000:
        return *(u32*)&ARM7WRAM[addr & 0xFFFF];

    case 0x04000000:
        switch (addr)
        {
        case 0x04000100: return Timers[4].Counter | (Timers[4].Control << 16);
        case 0x04000104: return Timers[5].Counter | (Timers[5].Control << 16);
        case 0x04000108: return Timers[6].Counter | (Timers[6].Control << 16);
        case 0x0400010C: return Timers[7].Counter | (Timers[7].Control << 16);

        case 0x040001A4:
            return 0x00800000; // hax

        case 0x040001C0:
            return SPI::ReadCnt() | (SPI::ReadData() << 16);

        case 0x04000208: return IME[1];
        case 0x04000210: return IE[1];
        case 0x04000214: return IF[1];
        }
    }
if ((addr&0xFF000000) == 0xEA000000) Halt();

    printf("unknown arm7 read32 %08X | %08X\n", addr, ARM7->R[15]);
    return 0;
}

void ARM7Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        *(u8*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM7) *(u8*)&SWRAM_ARM7[addr & SWRAM_ARM7Mask] = val;
        else *(u8*)&ARM7WRAM[addr & 0xFFFF] = val;
        return;

    case 0x03800000:
        *(u8*)&ARM7WRAM[addr & 0xFFFF] = val;
        return;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000138:
            return;

        case 0x04000301:
            if (val != 0x80) return;
            TriggerIRQ(1, IRQ_CartSendDone); // HAAAAXX!!
            return;

        case 0x040001C2:
            SPI::WriteData(val);
            return;
        }
    }

    if (addr==0xA20)
    {
        /*FILE* f = fopen("ram.bin", "wb");
        fwrite(MainRAM, 0x400000, 1, f);
        fclose(f);
        fopen("wram.bin", "wb");
        fwrite(ARM7WRAM, 0x10000, 1, f);
        fclose(f);
        fopen("swram.bin", "wb");
        fwrite(ARM7WRAM, 0x8000, 1, f);
        fclose(f);*/
    }

    printf("unknown arm7 write8 %08X %02X | %08X | %08X %08X %08X %08X\n", addr, val, ARM7->R[15], IME[1], IE[1], ARM7->R[0], ARM7->R[1]);
}

void ARM7Write16(u32 addr, u16 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        *(u16*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM7) *(u16*)&SWRAM_ARM7[addr & SWRAM_ARM7Mask] = val;
        else *(u16*)&ARM7WRAM[addr & 0xFFFF] = val;
        return;

    case 0x03800000:
        *(u16*)&ARM7WRAM[addr & 0xFFFF] = val;
        return;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000100: Timers[4].Reload = val; return;
        case 0x04000102: TimerStart(4, val); return;
        case 0x04000104: Timers[5].Reload = val; return;
        case 0x04000106: TimerStart(5, val); return;
        case 0x04000108: Timers[6].Reload = val; return;
        case 0x0400010A: TimerStart(6, val); return;
        case 0x0400010C: Timers[7].Reload = val; return;
        case 0x0400010E: TimerStart(7, val); return;

        case 0x04000180:
            IPCSync9 &= 0xFFF0;
            IPCSync9 |= ((val & 0x0F00) >> 8);
            IPCSync7 &= 0xB0FF;
            IPCSync7 |= (val & 0x4F00);
            if ((val & 0x2000) && (IPCSync9 & 0x4000))
            {
                TriggerIRQ(0, IRQ_IPCSync);
            }
            return;

        case 0x040001C0:
            SPI::WriteCnt(val);
            return;

        case 0x040001C2:
            SPI::WriteData(val & 0xFF);
            return;

        case 0x04000504:
            _soundbias = val & 0x3FF;
            return;
        }
    }

    printf("unknown arm7 write16 %08X %04X | %08X\n", addr, val, ARM7->R[15]);
}

void ARM7Write32(u32 addr, u32 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        *(u32*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM7) *(u32*)&SWRAM_ARM7[addr & SWRAM_ARM7Mask] = val;
        else *(u32*)&ARM7WRAM[addr & 0xFFFF] = val;
        return;

    case 0x03800000:
        *(u32*)&ARM7WRAM[addr & 0xFFFF] = val;
        return;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000100:
            Timers[4].Reload = val & 0xFFFF;
            TimerStart(4, val>>16);
            return;
        case 0x04000104:
            Timers[5].Reload = val & 0xFFFF;
            TimerStart(5, val>>16);
            return;
        case 0x04000108:
            Timers[6].Reload = val & 0xFFFF;
            TimerStart(6, val>>16);
            return;
        case 0x0400010C:
            Timers[7].Reload = val & 0xFFFF;
            TimerStart(7, val>>16);
            return;

        case 0x04000208: IME[1] = val; return;
        case 0x04000210: IE[1] = val; return;
        case 0x04000214: IF[1] &= ~val; printf("IRQ ack %08X\n", val);return;
        }
    }

    printf("unknown arm7 write32 %08X %08X | %08X %08X\n", addr, val, ARM7->R[15], ARM7->CurInstr);
}

}
