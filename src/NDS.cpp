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
#include "ARM.h"
#include "CP15.h"
#include "NDSCart.h"
#include "DMA.h"
#include "FIFO.h"
#include "GPU.h"
#include "SPI.h"
#include "RTC.h"
#include "Wifi.h"

#ifdef __LIBRETRO__
extern char retro_base_directory[4096];
extern char retro_game_path[4096];
#endif

namespace NDS
{

// TODO LIST
// * stick all the variables in a big structure?
//   would make it easier to deal with savestates

/*SchedEvent SchedBuffer[SCHED_BUF_LEN];
SchedEvent* SchedQueue;

bool NeedReschedule;*/

ARM* ARM9;
ARM* ARM7;

/*s32 ARM9Cycles, ARM7Cycles;
s32 CompensatedCycles;
s32 SchedCycles;*/
s32 CurIterationCycles;
s32 ARM7Offset;

SchedEvent SchedList[Event_MAX];
u32 SchedListMask;

u32 CPUStop;

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

u16 ExMemCnt[2];

u8 ROMSeed0[2*8];
u8 ROMSeed1[2*8];

// IO shit
u32 IME[2];
u32 IE[2], IF[2];

u8 PostFlag9;
u8 PostFlag7;
u16 PowerControl9;
u16 PowerControl7;

u16 ARM7BIOSProt;

Timer Timers[8];

DMA* DMAs[8];
u32 DMA9Fill[4];

u16 IPCSync9, IPCSync7;
u16 IPCFIFOCnt9, IPCFIFOCnt7;
FIFO<u32>* IPCFIFO9; // FIFO in which the ARM9 writes
FIFO<u32>* IPCFIFO7;

u16 DivCnt;
u32 DivNumerator[2];
u32 DivDenominator[2];
u32 DivQuotient[2];
u32 DivRemainder[2];

u16 SqrtCnt;
u32 SqrtVal[2];
u32 SqrtRes;

u32 KeyInput;

u16 _soundbias; // temp

bool Running;


bool Init()
{
    ARM9 = new ARM(0);
    ARM7 = new ARM(1);

    DMAs[0] = new DMA(0, 0);
    DMAs[1] = new DMA(0, 1);
    DMAs[2] = new DMA(0, 2);
    DMAs[3] = new DMA(0, 3);
    DMAs[4] = new DMA(1, 0);
    DMAs[5] = new DMA(1, 1);
    DMAs[6] = new DMA(1, 2);
    DMAs[7] = new DMA(1, 3);

    IPCFIFO9 = new FIFO<u32>(16);
    IPCFIFO7 = new FIFO<u32>(16);

    if (!NDSCart::Init()) return false;
    if (!GPU::Init()) return false;
    if (!SPI::Init()) return false;
    if (!RTC::Init()) return false;

    return true;
}

void DeInit()
{
    delete ARM9;
    delete ARM7;

    for (int i = 0; i < 8; i++)
        delete DMAs[i];

    delete IPCFIFO9;
    delete IPCFIFO7;

    NDSCart::DeInit();
    GPU::DeInit();
    SPI::DeInit();
    RTC::DeInit();
}


void SetupDirectBoot()
{
    u32 bootparams[8];
    memcpy(bootparams, &NDSCart::CartROM[0x20], 8*4);

    printf("ARM9: offset=%08X entry=%08X RAM=%08X size=%08X\n",
           bootparams[0], bootparams[1], bootparams[2], bootparams[3]);
    printf("ARM7: offset=%08X entry=%08X RAM=%08X size=%08X\n",
           bootparams[4], bootparams[5], bootparams[6], bootparams[7]);

    MapSharedWRAM(3);

    for (u32 i = 0; i < bootparams[3]; i+=4)
    {
        u32 tmp = *(u32*)&NDSCart::CartROM[bootparams[0]+i];
        ARM9Write32(bootparams[2]+i, tmp);
    }

    for (u32 i = 0; i < bootparams[7]; i+=4)
    {
        u32 tmp = *(u32*)&NDSCart::CartROM[bootparams[4]+i];
        ARM7Write32(bootparams[6]+i, tmp);
    }

    for (u32 i = 0; i < 0x170; i+=4)
    {
        u32 tmp = *(u32*)&NDSCart::CartROM[i];
        ARM9Write32(0x027FFE00+i, tmp);
    }

    ARM9Write32(0x027FF800, 0x00001FC2);
    ARM9Write32(0x027FF804, 0x00001FC2);
    ARM9Write16(0x027FF808, *(u16*)&NDSCart::CartROM[0x15E]);
    ARM9Write16(0x027FF80A, *(u16*)&NDSCart::CartROM[0x6C]);

    ARM9Write16(0x027FF850, 0x5835);

    ARM9Write32(0x027FFC00, 0x00001FC2);
    ARM9Write32(0x027FFC04, 0x00001FC2);
    ARM9Write16(0x027FFC08, *(u16*)&NDSCart::CartROM[0x15E]);
    ARM9Write16(0x027FFC0A, *(u16*)&NDSCart::CartROM[0x6C]);

    ARM9Write16(0x027FFC10, 0x5835);
    ARM9Write16(0x027FFC30, 0xFFFF);
    ARM9Write16(0x027FFC40, 0x0001);

    CP15::Write(0x910, 0x0300000A);
    CP15::Write(0x911, 0x00000020);
    CP15::Write(0x100, 0x00050000);

    ARM9->JumpTo(bootparams[1]);
    ARM7->JumpTo(bootparams[5]);

    PowerControl9 = 0x820F;
    GPU::DisplaySwap(PowerControl9);

    ARM7BIOSProt = 0x1204;
}

void Reset()
{
    FILE* f;
    u32 i;

#ifdef __LIBRETRO__
    char path[2048];
    snprintf(path, sizeof(path), "%s/bios9.bin", retro_base_directory);
    f = fopen(path, "rb");
#else
    f = fopen("bios9.bin", "rb");
#endif
    if (!f)
        printf("ARM9 BIOS not found\n");
    else
    {
        fseek(f, 0, SEEK_SET);
        fread(ARM9BIOS, 0x1000, 1, f);

        printf("ARM9 BIOS loaded\n");
        fclose(f);
    }

#ifdef __LIBRETRO__
    snprintf(path, sizeof(path), "%s/bios7.bin", retro_base_directory);
    f = fopen(path, "rb");
#else
    f = fopen("bios7.bin", "rb");
#endif
    if (!f)
        printf("ARM7 BIOS not found\n");
    else
    {
        fseek(f, 0, SEEK_SET);
        fread(ARM7BIOS, 0x4000, 1, f);

        printf("ARM7 BIOS loaded\n");
        fclose(f);
    }

    memset(MainRAM, 0, 0x400000);
    memset(SharedWRAM, 0, 0x8000);
    memset(ARM7WRAM, 0, 0x10000);

    MapSharedWRAM(0);

    ExMemCnt[0] = 0;
    ExMemCnt[1] = 0;
    memset(ROMSeed0, 0, 2*8);
    memset(ROMSeed1, 0, 2*8);

    IME[0] = 0;
    IME[1] = 0;

    PostFlag9 = 0x00;
    PostFlag7 = 0x00;
    PowerControl9 = 0x0001;
    PowerControl7 = 0x0001;

    ARM7BIOSProt = 0;

    IPCSync9 = 0;
    IPCSync7 = 0;
    IPCFIFOCnt9 = 0;
    IPCFIFOCnt7 = 0;
    IPCFIFO9->Clear();
    IPCFIFO7->Clear();

    DivCnt = 0;
    SqrtCnt = 0;

    ARM9->Reset();
    ARM7->Reset();
    CP15::Reset();

    CPUStop = 0;

    memset(Timers, 0, 8*sizeof(Timer));

    for (i = 0; i < 8; i++) DMAs[i]->Reset();
    memset(DMA9Fill, 0, 4*4);

    NDSCart::Reset();
    GPU::Reset();
    SPI::Reset();
    RTC::Reset();
    Wifi::Reset();

   // memset(SchedBuffer, 0, sizeof(SchedEvent)*SCHED_BUF_LEN);
   // SchedQueue = NULL;
    memset(SchedList, 0, sizeof(SchedList));
    SchedListMask = 0;

    /*ARM9Cycles = 0;
    ARM7Cycles = 0;
    SchedCycles = 0;*/
    CurIterationCycles = 0;
    ARM7Offset = 0;

    KeyInput = 0x007F03FF;

    _soundbias = 0;
}

void LoadROM(const char* path, bool direct)
{
    Reset();

    if (NDSCart::LoadROM(path, direct))
        Running = true;
}


void CalcIterationCycles()
{
    CurIterationCycles = 16;

    for (int i = 0; i < Event_MAX; i++)
    {
        if (!(SchedListMask & (1<<i)))
            continue;

        if (SchedList[i].WaitCycles < CurIterationCycles)
            CurIterationCycles = SchedList[i].WaitCycles;
    }
}

void RunSystem(s32 cycles)
{
    for (int i = 0; i < 8; i++)
    {
        if ((Timers[i].Cnt & 0x84) == 0x80)
            Timers[i].Counter += (ARM9->Cycles >> 1) << Timers[i].CycleShift;
    }
    for (int i = 4; i < 8; i++)
    {
        if ((Timers[i].Cnt & 0x84) == 0x80)
            Timers[i].Counter += ARM7->Cycles << Timers[i].CycleShift;
    }

    for (int i = 0; i < Event_MAX; i++)
    {
        if (!(SchedListMask & (1<<i)))
            continue;

        SchedList[i].WaitCycles -= cycles;
        if (SchedList[i].WaitCycles < 1)
        {
            SchedListMask &= ~(1<<i);
            SchedList[i].Func(SchedList[i].Param);
        }
    }
}

void RunFrame()
{
    s32 framecycles = 560190;

    if (!Running) return; // dorp


    GPU::StartFrame();

    while (Running && framecycles>0)
    {
        s32 ndscyclestorun;
        s32 ndscycles = 0;

        CalcIterationCycles();

        if (CPUStop & 0xFFFF)
        {
            s32 cycles = CurIterationCycles;
            cycles = DMAs[0]->Run(cycles);
            if (cycles > 0) cycles = DMAs[1]->Run(cycles);
            if (cycles > 0) cycles = DMAs[2]->Run(cycles);
            if (cycles > 0) cycles = DMAs[3]->Run(cycles);
            ndscyclestorun = CurIterationCycles - cycles;

            // TODO: run other timing critical shit, like timers
            GPU3D::Run(ndscyclestorun);
        }
        else
        {
            ARM9->CyclesToRun = CurIterationCycles << 1;
            ARM9->Execute();
            ndscyclestorun = ARM9->Cycles >> 1;
        }

        if (CPUStop & 0xFFFF0000)
        {
            s32 cycles = ndscyclestorun - ARM7Offset;
            cycles = DMAs[4]->Run(cycles);
            if (cycles > 0) cycles = DMAs[5]->Run(cycles);
            if (cycles > 0) cycles = DMAs[6]->Run(cycles);
            if (cycles > 0) cycles = DMAs[7]->Run(cycles);
            ARM7Offset = cycles;
        }
        else
        {
            ARM7->CyclesToRun = ndscyclestorun - ARM7Offset;
            ARM7->Execute();
            ARM7Offset = ARM7->Cycles - ARM7->CyclesToRun;
        }

        RunSystem(ndscyclestorun);
        //GPU3D::Run(ndscyclestorun);

        /*while (ndscycles < ndscyclestorun)
        {
            ARM7->CyclesToRun = ndscyclestorun - ndscycles - ARM7Offset;
            ARM7->Execute();
            ARM7Offset = 0;

            RunEvents(ARM7->Cycles);
            ndscycles += ARM7->Cycles;
        }

        ARM7Offset = ndscycles - ndscyclestorun;*/

        framecycles -= ndscyclestorun;
    }
}

void Reschedule()
{
    CalcIterationCycles();

    ARM9->CyclesToRun = CurIterationCycles << 1;
    //ARM7->CyclesToRun = CurIterationCycles - ARM7Offset;
    //ARM7->CyclesToRun = (ARM9->Cycles >> 1) - ARM7->Cycles - ARM7Offset;
}

void ScheduleEvent(u32 id, bool periodic, s32 delay, void (*func)(u32), u32 param)
{
    if (SchedListMask & (1<<id))
    {
        printf("!! EVENT %d ALREADY SCHEDULED\n", id);
        return;
    }

    SchedEvent* evt = &SchedList[id];

    if (periodic) evt->WaitCycles += delay;
    else          evt->WaitCycles  = delay + (ARM9->Cycles >> 1);

    evt->Func = func;
    evt->Param = param;

    SchedListMask |= (1<<id);

    Reschedule();
}

void CancelEvent(u32 id)
{
    SchedListMask &= ~(1<<id);
}


void PressKey(u32 key)
{
    KeyInput &= ~(1 << key);
}

void ReleaseKey(u32 key)
{
    KeyInput |= (1 << key);
}

void TouchScreen(u16 x, u16 y)
{
    SPI_TSC::SetTouchCoords(x, y);
}

void ReleaseScreen()
{
    SPI_TSC::SetTouchCoords(0x000, 0xFFF);
}


void Halt()
{
    printf("Halt()\n");
    Running = false;
}


void MapSharedWRAM(u8 val)
{
    WRAMCnt = val;

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


void SetIRQ(u32 cpu, u32 irq)
{
    IF[cpu] |= (1 << irq);
}

void ClearIRQ(u32 cpu, u32 irq)
{
    IF[cpu] &= ~(1 << irq);
}

bool HaltInterrupted(u32 cpu)
{
    if (cpu == 0)
    {
        if (!(IME[0] & 0x1))
            return false;
    }

    if (IF[cpu] & IE[cpu])
        return true;

    return false;
}

void StopCPU(u32 cpu, u32 mask)
{
    if (cpu) mask <<= 16;
    CPUStop |= mask;
}

void ResumeCPU(u32 cpu, u32 mask)
{
    if (cpu) mask <<= 16;
    CPUStop &= ~mask;
}



void CheckDMAs(u32 cpu, u32 mode)
{
    cpu <<= 2;
    DMAs[cpu+0]->StartIfNeeded(mode);
    DMAs[cpu+1]->StartIfNeeded(mode);
    DMAs[cpu+2]->StartIfNeeded(mode);
    DMAs[cpu+3]->StartIfNeeded(mode);
}



//const s32 TimerPrescaler[4] = {1, 64, 256, 1024};
const s32 TimerPrescaler[4] = {0, 6, 8, 10};

u16 TimerGetCounter(u32 timer)
{
    u32 ret = Timers[timer].Counter;

    if ((Timers[timer].Cnt & 0x84) == 0x80)
    {
        u32 c = (timer & 0x4) ? ARM7->Cycles : (ARM9->Cycles>>1);
        ret += (c << Timers[timer].CycleShift);
    }

    return ret >> 16;
}

void TimerOverflow(u32 param)
{
    Timer* timer = &Timers[param];
    timer->Counter = 0;

    u32 tid = param & 0x3;
    u32 cpu = param >> 2;

    for (;;)
    {
        if (tid == (param&0x3))
            ScheduleEvent(Event_Timer9_0 + param, true, (0x10000 - timer->Reload) << TimerPrescaler[timer->Cnt & 0x03], TimerOverflow, param);
            //timer->Event = ScheduleEvent(TimerPrescaler[timer->Control&0x3], TimerIncrement, param);

        if (timer->Counter == 0)
        {
            timer->Counter = timer->Reload << 16;

            if (timer->Cnt & (1<<6))
                SetIRQ(cpu, IRQ_Timer0 + tid);

            // cascade
            if (tid == 3)
                break;
            timer++;
            if ((timer->Cnt & 0x84) != 0x84)
                break;
            timer->Counter += 0x10000;
            tid++;
            continue;
        }

        break;
    }
}

void TimerStart(u32 id, u16 cnt)
{
    Timer* timer = &Timers[id];
    u16 curstart = timer->Cnt & (1<<7);
    u16 newstart = cnt & (1<<7);

    timer->Cnt = cnt;

    if ((!curstart) && newstart)
    {
        timer->Counter = timer->Reload << 16;
        timer->CycleShift = 16 - TimerPrescaler[cnt & 0x03];

        // start the timer, if it's not a cascading timer
        if (!(cnt & (1<<2)))
            ScheduleEvent(Event_Timer9_0 + id, false, (0x10000 - timer->Reload) << TimerPrescaler[cnt & 0x03], TimerOverflow, id);
        else
            CancelEvent(Event_Timer9_0 + id);
    }
    else if (curstart && (!newstart))
    {
        CancelEvent(Event_Timer9_0 + id);
    }
}



void StartDiv()
{
    // TODO: division isn't instant!

    DivCnt &= ~0x2000;

    switch (DivCnt & 0x0003)
    {
    case 0x0000:
        {
            s32 num = (s32)DivNumerator[0];
            s32 den = (s32)DivDenominator[0];
            if (den == 0)
            {
                DivQuotient[0] = (num<0) ? 1:-1;
                DivQuotient[1] = (num<0) ? -1:1;
                *(s64*)&DivRemainder[0] = num;
            }
            else if (num == -0x80000000 && den == -1)
            {
                *(s64*)&DivQuotient[0] = 0x80000000;
            }
            else
            {
                *(s64*)&DivQuotient[0] = (s64)(num / den);
                *(s64*)&DivRemainder[0] = (s64)(num % den);
            }
        }
        break;

    case 0x0001:
    case 0x0003:
        {
            s64 num = *(s64*)&DivNumerator[0];
            s32 den = (s32)DivDenominator[0];
            if (den == 0)
            {
                *(s64*)&DivQuotient[0] = (num<0) ? 1:-1;
                *(s64*)&DivRemainder[0] = num;
            }
            else if (num == -0x8000000000000000 && den == -1)
            {
                *(s64*)&DivQuotient[0] = 0x8000000000000000;
            }
            else
            {
                *(s64*)&DivQuotient[0] = (s64)(num / den);
                *(s64*)&DivRemainder[0] = (s64)(num % den);
            }
        }
        break;

    case 0x0002:
        {
            s64 num = *(s64*)&DivNumerator[0];
            s64 den = *(s64*)&DivDenominator[0];
            if (den == 0)
            {
                *(s64*)&DivQuotient[0] = (num<0) ? 1:-1;
                *(s64*)&DivRemainder[0] = num;
            }
            else if (num == -0x8000000000000000 && den == -1)
            {
                *(s64*)&DivQuotient[0] = 0x8000000000000000;
            }
            else
            {
                *(s64*)&DivQuotient[0] = (s64)(num / den);
                *(s64*)&DivRemainder[0] = (s64)(num % den);
            }
        }
        break;
    }

    if ((DivDenominator[0] | DivDenominator[1]) == 0)
        DivCnt |= 0x2000;
}

// http://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
void StartSqrt()
{
    // TODO: sqrt isn't instant either. oh well

    u64 val;
    u32 res = 0;
    u64 rem = 0;
    u32 prod = 0;
    u32 nbits, topshift;

    if (SqrtCnt & 0x0001)
    {
        val = *(u64*)&SqrtVal[0];
        nbits = 32;
        topshift = 62;
    }
    else
    {
        val = (u64)SqrtVal[0]; // 32bit
        nbits = 16;
        topshift = 30;
    }

    for (u32 i = 0; i < nbits; i++)
    {
        rem = (rem << 2) + ((val >> topshift) & 0x3);
        val <<= 2;
        res <<= 1;

        prod = (res << 1) + 1;
        if (rem >= prod)
        {
            rem -= prod;
            res++;
        }
    }

    SqrtRes = res;
}



void debug(u32 param)
{
    printf("ARM9 PC=%08X LR=%08X %08X\n", ARM9->R[15], ARM9->R[14], ARM9->R_IRQ[1]);
    printf("ARM7 PC=%08X LR=%08X %08X\n", ARM7->R[15], ARM7->R[14], ARM7->R_IRQ[1]);

    for (int i = 0; i < 9; i++)
        printf("VRAM %c: %02X\n", 'A'+i, GPU::VRAMCNT[i]);
}



u8 ARM9Read8(u32 addr)
{
    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u8*)&ARM9BIOS[addr & 0xFFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u8*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM9) return *(u8*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask];
        else return 0;

    case 0x04000000:
        return ARM9IORead8(addr);

    case 0x05000000:
        return *(u8*)&GPU::Palette[addr & 0x7FF];

    case 0x06000000:
        {
            switch (addr & 0x00E00000)
            {
            case 0x00000000: return GPU::ReadVRAM_ABG<u8>(addr);
            case 0x00200000: return GPU::ReadVRAM_BBG<u8>(addr);
            case 0x00400000: return GPU::ReadVRAM_AOBJ<u8>(addr);
            case 0x00600000: return GPU::ReadVRAM_BOBJ<u8>(addr);
            default:         return GPU::ReadVRAM_LCDC<u8>(addr);
            }
        }
        return 0;

    case 0x07000000:
        return *(u8*)&GPU::OAM[addr & 0x7FF];

    case 0x08000000:
    case 0x09000000:
        return 0xFF;
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

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u16*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM9) return *(u16*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask];
        else return 0;

    case 0x04000000:
        return ARM9IORead16(addr);

    case 0x05000000:
        return *(u16*)&GPU::Palette[addr & 0x7FF];

    case 0x06000000:
        {
            switch (addr & 0x00E00000)
            {
            case 0x00000000: return GPU::ReadVRAM_ABG<u16>(addr);
            case 0x00200000: return GPU::ReadVRAM_BBG<u16>(addr);
            case 0x00400000: return GPU::ReadVRAM_AOBJ<u16>(addr);
            case 0x00600000: return GPU::ReadVRAM_BOBJ<u16>(addr);
            default:         return GPU::ReadVRAM_LCDC<u16>(addr);
            }
        }
        return 0;

    case 0x07000000:
        return *(u16*)&GPU::OAM[addr & 0x7FF];

    case 0x08000000:
    case 0x09000000:
        return 0xFFFF;
    }

    //printf("unknown arm9 read16 %08X %08X %08X %08X\n", addr, ARM9->R[15], ARM9->R[1], ARM9->R[2]);
    return 0;
}

u32 ARM9Read32(u32 addr)
{
    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u32*)&ARM9BIOS[addr & 0xFFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u32*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM9) return *(u32*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask];
        else return 0;

    case 0x04000000:
        return ARM9IORead32(addr);

    case 0x05000000:
        return *(u32*)&GPU::Palette[addr & 0x7FF];

    case 0x06000000:
        {
            switch (addr & 0x00E00000)
            {
            case 0x00000000: return GPU::ReadVRAM_ABG<u32>(addr);
            case 0x00200000: return GPU::ReadVRAM_BBG<u32>(addr);
            case 0x00400000: return GPU::ReadVRAM_AOBJ<u32>(addr);
            case 0x00600000: return GPU::ReadVRAM_BOBJ<u32>(addr);
            default:         return GPU::ReadVRAM_LCDC<u32>(addr);
            }
        }
        return 0;

    case 0x07000000:
        return *(u32*)&GPU::OAM[addr & 0x7FF];

    case 0x08000000:
    case 0x09000000:
        return 0xFFFFFFFF;
    }

    printf("unknown arm9 read32 %08X | %08X %08X %08X\n", addr, ARM9->R[15], ARM9->R[12], ARM9Read32(0x027FF820));
    return 0;
}

void ARM9Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        *(u8*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM9) *(u8*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask] = val;
        return;

    case 0x04000000:
        ARM9IOWrite8(addr, val);
        return;

    case 0x05000000:
    case 0x06000000:
    case 0x07000000:
        return;
    }

    printf("unknown arm9 write8 %08X %02X\n", addr, val);
}

void ARM9Write16(u32 addr, u16 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        *(u16*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM9) *(u16*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask] = val;
        return;

    case 0x04000000:
        ARM9IOWrite16(addr, val);
        return;

    case 0x05000000:
        *(u16*)&GPU::Palette[addr & 0x7FF] = val;
        return;

    case 0x06000000:
        switch (addr & 0x00E00000)
        {
        case 0x00000000: GPU::WriteVRAM_ABG<u16>(addr, val); break;
        case 0x00200000: GPU::WriteVRAM_BBG<u16>(addr, val); break;
        case 0x00400000: GPU::WriteVRAM_AOBJ<u16>(addr, val); break;
        case 0x00600000: GPU::WriteVRAM_BOBJ<u16>(addr, val); break;
        default:         GPU::WriteVRAM_LCDC<u16>(addr, val); break;
        }
        return;

    case 0x07000000:
        *(u16*)&GPU::OAM[addr & 0x7FF] = val;
        return;
    }

    //printf("unknown arm9 write16 %08X %04X\n", addr, val);
}

void ARM9Write32(u32 addr, u32 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        *(u32*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM9) *(u32*)&SWRAM_ARM9[addr & SWRAM_ARM9Mask] = val;
        return;

    case 0x04000000:
        ARM9IOWrite32(addr, val);
        return;

    case 0x05000000:
        *(u32*)&GPU::Palette[addr & 0x7FF] = val;
        return;

    case 0x06000000:
        switch (addr & 0x00E00000)
        {
        case 0x00000000: GPU::WriteVRAM_ABG<u32>(addr, val); break;
        case 0x00200000: GPU::WriteVRAM_BBG<u32>(addr, val); break;
        case 0x00400000: GPU::WriteVRAM_AOBJ<u32>(addr, val); break;
        case 0x00600000: GPU::WriteVRAM_BOBJ<u32>(addr, val); break;
        default:         GPU::WriteVRAM_LCDC<u32>(addr, val); break;
        }
        return;

    case 0x07000000:
        *(u32*)&GPU::OAM[addr & 0x7FF] = val;
        return;
    }

    printf("unknown arm9 write32 %08X %08X | %08X\n", addr, val, ARM9->R[15]);
}



u8 ARM7Read8(u32 addr)
{
    if (addr < 0x00004000)
    {
        if (ARM7->R[15] >= 0x4000)
            return 0xFF;
        if (addr < ARM7BIOSProt && ARM7->R[15] >= ARM7BIOSProt)
            return 0xFF;

        return *(u8*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        return *(u8*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM7) return *(u8*)&SWRAM_ARM7[addr & SWRAM_ARM7Mask];
        else return *(u8*)&ARM7WRAM[addr & 0xFFFF];

    case 0x03800000:
        return *(u8*)&ARM7WRAM[addr & 0xFFFF];

    case 0x04000000:
        return ARM7IORead8(addr);

    case 0x06000000:
    case 0x06800000:
        return GPU::ReadVRAM_ARM7<u8>(addr);
    }

    printf("unknown arm7 read8 %08X %08X %08X/%08X\n", addr, ARM7->R[15], ARM7->R[0], ARM7->R[1]);
    return 0;
}

u16 ARM7Read16(u32 addr)
{
    if (addr < 0x00004000)
    {
        if (ARM7->R[15] >= 0x4000)
            return 0xFFFF;
        if (addr < ARM7BIOSProt && ARM7->R[15] >= ARM7BIOSProt)
            return 0xFFFF;

        return *(u16*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        return *(u16*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM7) return *(u16*)&SWRAM_ARM7[addr & SWRAM_ARM7Mask];
        else return *(u16*)&ARM7WRAM[addr & 0xFFFF];

    case 0x03800000:
        return *(u16*)&ARM7WRAM[addr & 0xFFFF];

    case 0x04000000:
        return ARM7IORead16(addr);

    case 0x04800000:
        return Wifi::Read(addr);

    case 0x06000000:
    case 0x06800000:
        return GPU::ReadVRAM_ARM7<u16>(addr);
    }

    printf("unknown arm7 read16 %08X %08X\n", addr, ARM7->R[15]);
    return 0;
}

u32 ARM7Read32(u32 addr)
{
    if (addr < 0x00004000)
    {
        if (ARM7->R[15] >= 0x4000)
            return 0xFFFFFFFF;
        if (addr < ARM7BIOSProt && ARM7->R[15] >= ARM7BIOSProt)
            return 0xFFFFFFFF;

        return *(u32*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        return *(u32*)&MainRAM[addr & 0x3FFFFF];

    case 0x03000000:
        if (SWRAM_ARM7) return *(u32*)&SWRAM_ARM7[addr & SWRAM_ARM7Mask];
        else return *(u32*)&ARM7WRAM[addr & 0xFFFF];

    case 0x03800000:
        return *(u32*)&ARM7WRAM[addr & 0xFFFF];

    case 0x04000000:
        return ARM7IORead32(addr);

    case 0x06000000:
    case 0x06800000:
        return GPU::ReadVRAM_ARM7<u32>(addr);
    }

    printf("unknown arm7 read32 %08X | %08X\n", addr, ARM7->R[15]);
    return 0;
}

void ARM7Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
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
        ARM7IOWrite8(addr, val);
        return;

    case 0x06000000:
    case 0x06800000:
        GPU::WriteVRAM_ARM7<u8>(addr, val);
        return;
    }

    printf("unknown arm7 write8 %08X %02X | %08X | %08X %08X %08X %08X\n", addr, val, ARM7->R[15], IME[1], IE[1], ARM7->R[0], ARM7->R[1]);
}

void ARM7Write16(u32 addr, u16 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
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
        ARM7IOWrite16(addr, val);
        return;

    case 0x04800000:
        Wifi::Write(addr, val);
        return;

    case 0x06000000:
    case 0x06800000:
        GPU::WriteVRAM_ARM7<u16>(addr, val);
        return;
    }

    printf("unknown arm7 write16 %08X %04X | %08X\n", addr, val, ARM7->R[15]);
}

void ARM7Write32(u32 addr, u32 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
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
        ARM7IOWrite32(addr, val);
        return;

    case 0x06000000:
    case 0x06800000:
        GPU::WriteVRAM_ARM7<u32>(addr, val);
        return;
    }

    printf("unknown arm7 write32 %08X %08X | %08X %08X\n", addr, val, ARM7->R[15], ARM7->CurInstr);
}




u8 ARM9IORead8(u32 addr)
{
    switch (addr)
    {
    case 0x040001A2: return NDSCart::ReadSPIData();

    case 0x04000208: return IME[0];

    case 0x04000240: return GPU::VRAMCNT[0];
    case 0x04000241: return GPU::VRAMCNT[1];
    case 0x04000242: return GPU::VRAMCNT[2];
    case 0x04000243: return GPU::VRAMCNT[3];
    case 0x04000244: return GPU::VRAMCNT[4];
    case 0x04000245: return GPU::VRAMCNT[5];
    case 0x04000246: return GPU::VRAMCNT[6];
    case 0x04000247: return WRAMCnt;
    case 0x04000248: return GPU::VRAMCNT[7];
    case 0x04000249: return GPU::VRAMCNT[8];

    case 0x04000300: return PostFlag9;
    }

    if (addr >= 0x04000000 && addr < 0x04000060)
    {
        return GPU::GPU2D_A->Read8(addr);
    }
    if (addr >= 0x04001000 && addr < 0x04001060)
    {
        return GPU::GPU2D_B->Read8(addr);
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        return GPU3D::Read8(addr);
    }

    printf("unknown ARM9 IO read8 %08X\n", addr);
    return 0;
}

u16 ARM9IORead16(u32 addr)
{
    switch (addr)
    {
    case 0x04000004: return GPU::DispStat[0];
    case 0x04000006: return GPU::VCount;

    case 0x04000060: return GPU3D::Read16(addr);
    case 0x04000064:
    case 0x04000066: return GPU::GPU2D_A->Read16(addr);

    case 0x040000B8: return DMAs[0]->Cnt & 0xFFFF;
    case 0x040000BA: return DMAs[0]->Cnt >> 16;
    case 0x040000C4: return DMAs[1]->Cnt & 0xFFFF;
    case 0x040000C6: return DMAs[1]->Cnt >> 16;
    case 0x040000D0: return DMAs[2]->Cnt & 0xFFFF;
    case 0x040000D2: return DMAs[2]->Cnt >> 16;
    case 0x040000DC: return DMAs[3]->Cnt & 0xFFFF;
    case 0x040000DE: return DMAs[3]->Cnt >> 16;

    case 0x040000E0: return ((u16*)DMA9Fill)[0];
    case 0x040000E2: return ((u16*)DMA9Fill)[1];
    case 0x040000E4: return ((u16*)DMA9Fill)[2];
    case 0x040000E6: return ((u16*)DMA9Fill)[3];
    case 0x040000E8: return ((u16*)DMA9Fill)[4];
    case 0x040000EA: return ((u16*)DMA9Fill)[5];
    case 0x040000EC: return ((u16*)DMA9Fill)[6];
    case 0x040000EE: return ((u16*)DMA9Fill)[7];

    case 0x04000100: return TimerGetCounter(0);
    case 0x04000102: return Timers[0].Cnt;
    case 0x04000104: return TimerGetCounter(1);
    case 0x04000106: return Timers[1].Cnt;
    case 0x04000108: return TimerGetCounter(2);
    case 0x0400010A: return Timers[2].Cnt;
    case 0x0400010C: return TimerGetCounter(3);
    case 0x0400010E: return Timers[3].Cnt;

    case 0x04000130: return KeyInput & 0xFFFF;

    case 0x04000180: return IPCSync9;
    case 0x04000184:
        {
            u16 val = IPCFIFOCnt9;
            if (IPCFIFO9->IsEmpty())     val |= 0x0001;
            else if (IPCFIFO9->IsFull()) val |= 0x0002;
            if (IPCFIFO7->IsEmpty())     val |= 0x0100;
            else if (IPCFIFO7->IsFull()) val |= 0x0200;
            return val;
        }

    case 0x040001A0: return NDSCart::SPICnt;
    case 0x040001A2: return NDSCart::ReadSPIData();

    case 0x04000204: return ExMemCnt[0];
    case 0x04000208: return IME[0];

    case 0x04000240: return GPU::VRAMCNT[0] | (GPU::VRAMCNT[1] << 8);
    case 0x04000242: return GPU::VRAMCNT[2] | (GPU::VRAMCNT[3] << 8);
    case 0x04000244: return GPU::VRAMCNT[4] | (GPU::VRAMCNT[5] << 8);
    case 0x04000246: return GPU::VRAMCNT[6] | (WRAMCnt << 8);
    case 0x04000248: return GPU::VRAMCNT[7] | (GPU::VRAMCNT[8] << 8);

    case 0x04000280: return DivCnt;

    case 0x040002B0: return SqrtCnt;

    case 0x04000300: return PostFlag9;
    case 0x04000304: return PowerControl9;
    }

    if (addr >= 0x04000000 && addr < 0x04000060)
    {
        return GPU::GPU2D_A->Read16(addr);
    }
    if (addr >= 0x04001000 && addr < 0x04001060)
    {
        return GPU::GPU2D_B->Read16(addr);
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        return GPU3D::Read16(addr);
    }

    printf("unknown ARM9 IO read16 %08X %08X\n", addr, ARM9->R[15]);
    return 0;
}

u32 ARM9IORead32(u32 addr)
{
    switch (addr)
    {
    case 0x04000004: return GPU::DispStat[0] | (GPU::VCount << 16);

    case 0x04000060: return GPU3D::Read32(addr);
    case 0x04000064: return GPU::GPU2D_A->Read32(addr);

    case 0x040000B0: return DMAs[0]->SrcAddr;
    case 0x040000B4: return DMAs[0]->DstAddr;
    case 0x040000B8: return DMAs[0]->Cnt;
    case 0x040000BC: return DMAs[1]->SrcAddr;
    case 0x040000C0: return DMAs[1]->DstAddr;
    case 0x040000C4: return DMAs[1]->Cnt;
    case 0x040000C8: return DMAs[2]->SrcAddr;
    case 0x040000CC: return DMAs[2]->DstAddr;
    case 0x040000D0: return DMAs[2]->Cnt;
    case 0x040000D4: return DMAs[3]->SrcAddr;
    case 0x040000D8: return DMAs[3]->DstAddr;
    case 0x040000DC: return DMAs[3]->Cnt;

    case 0x040000E0: return DMA9Fill[0];
    case 0x040000E4: return DMA9Fill[1];
    case 0x040000E8: return DMA9Fill[2];
    case 0x040000EC: return DMA9Fill[3];

    case 0x04000100: return TimerGetCounter(0) | (Timers[0].Cnt << 16);
    case 0x04000104: return TimerGetCounter(1) | (Timers[1].Cnt << 16);
    case 0x04000108: return TimerGetCounter(2) | (Timers[2].Cnt << 16);
    case 0x0400010C: return TimerGetCounter(3) | (Timers[3].Cnt << 16);

    case 0x040001A0: return NDSCart::SPICnt | (NDSCart::ReadSPIData() << 16);
    case 0x040001A4: return NDSCart::ROMCnt;

    case 0x04000208: return IME[0];
    case 0x04000210: return IE[0];
    case 0x04000214: return IF[0];

    case 0x04000240: return GPU::VRAMCNT[0] | (GPU::VRAMCNT[1] << 8) | (GPU::VRAMCNT[2] << 16) | (GPU::VRAMCNT[3] << 24);
    case 0x04000244: return GPU::VRAMCNT[4] | (GPU::VRAMCNT[5] << 8) | (GPU::VRAMCNT[6] << 16) | (WRAMCnt << 24);
    case 0x04000248: return GPU::VRAMCNT[7] | (GPU::VRAMCNT[8] << 8);

    case 0x04000290: return DivNumerator[0];
    case 0x04000294: return DivNumerator[1];
    case 0x04000298: return DivDenominator[0];
    case 0x0400029C: return DivDenominator[1];
    case 0x040002A0: return DivQuotient[0];
    case 0x040002A4: return DivQuotient[1];
    case 0x040002A8: return DivRemainder[0];
    case 0x040002AC: return DivRemainder[1];

    case 0x040002B4: return SqrtRes;
    case 0x040002B8: return SqrtVal[0];
    case 0x040002BC: return SqrtVal[1];

    case 0x04100000:
        if (IPCFIFOCnt9 & 0x8000)
        {
            u32 ret;
            if (IPCFIFO7->IsEmpty())
            {
                IPCFIFOCnt9 |= 0x4000;
                ret = IPCFIFO7->Peek();
            }
            else
            {
                ret = IPCFIFO7->Read();

                if (IPCFIFO7->IsEmpty() && (IPCFIFOCnt7 & 0x0004))
                    SetIRQ(1, IRQ_IPCSendDone);
            }
            return ret;
        }
        else
            return IPCFIFO7->Peek();

    case 0x04100010:
        if (!(ExMemCnt[0] & (1<<11))) return NDSCart::ReadROMData();
        return 0;
    }

    if (addr >= 0x04000000 && addr < 0x04000060)
    {
        return GPU::GPU2D_A->Read32(addr);
    }
    if (addr >= 0x04001000 && addr < 0x04001060)
    {
        return GPU::GPU2D_B->Read32(addr);
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        return GPU3D::Read32(addr);
    }

    printf("unknown ARM9 IO read32 %08X\n", addr);
    return 0;
}

void ARM9IOWrite8(u32 addr, u8 val)
{
    switch (addr)
    {
    case 0x040001A0:
        if (!(ExMemCnt[0] & (1<<11)))
        {
            NDSCart::WriteSPICnt((NDSCart::SPICnt & 0xFF00) | val);
        }
        return;
    case 0x040001A1:
        if (!(ExMemCnt[0] & (1<<11)))
        {
            NDSCart::WriteSPICnt((NDSCart::SPICnt & 0x00FF) | (val << 8));
        }
        return;
    case 0x040001A2:
        NDSCart::WriteSPIData(val);
        return;

    case 0x040001A8: NDSCart::ROMCommand[0] = val; return;
    case 0x040001A9: NDSCart::ROMCommand[1] = val; return;
    case 0x040001AA: NDSCart::ROMCommand[2] = val; return;
    case 0x040001AB: NDSCart::ROMCommand[3] = val; return;
    case 0x040001AC: NDSCart::ROMCommand[4] = val; return;
    case 0x040001AD: NDSCart::ROMCommand[5] = val; return;
    case 0x040001AE: NDSCart::ROMCommand[6] = val; return;
    case 0x040001AF: NDSCart::ROMCommand[7] = val; return;

    case 0x04000208: IME[0] = val & 0x1; return;

    case 0x04000240: GPU::MapVRAM_AB(0, val); return;
    case 0x04000241: GPU::MapVRAM_AB(1, val); return;
    case 0x04000242: GPU::MapVRAM_CD(2, val); return;
    case 0x04000243: GPU::MapVRAM_CD(3, val); return;
    case 0x04000244: GPU::MapVRAM_E(4, val); return;
    case 0x04000245: GPU::MapVRAM_FG(5, val); return;
    case 0x04000246: GPU::MapVRAM_FG(6, val); return;
    case 0x04000247: MapSharedWRAM(val); return;
    case 0x04000248: GPU::MapVRAM_H(7, val); return;
    case 0x04000249: GPU::MapVRAM_I(8, val); return;

    case 0x04000300:
        if (PostFlag9 & 0x01) val |= 0x01;
        PostFlag9 = val & 0x03;
        return;
    }

    if (addr >= 0x04000000 && addr < 0x04000060)
    {
        GPU::GPU2D_A->Write8(addr, val);
        return;
    }
    if (addr >= 0x04001000 && addr < 0x04001060)
    {
        GPU::GPU2D_B->Write8(addr, val);
        return;
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        GPU3D::Write8(addr, val);
        return;
    }

    printf("unknown ARM9 IO write8 %08X %02X\n", addr, val);
}

void ARM9IOWrite16(u32 addr, u16 val)
{
    switch (addr)
    {
    case 0x04000004: GPU::SetDispStat(0, val); return;

    case 0x04000060: GPU3D::Write16(addr, val); return;

    case 0x040000B8: DMAs[0]->WriteCnt((DMAs[0]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000BA: DMAs[0]->WriteCnt((DMAs[0]->Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000C4: DMAs[1]->WriteCnt((DMAs[1]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000C6: DMAs[1]->WriteCnt((DMAs[1]->Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000D0: DMAs[2]->WriteCnt((DMAs[2]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000D2: DMAs[2]->WriteCnt((DMAs[2]->Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000DC: DMAs[3]->WriteCnt((DMAs[3]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000DE: DMAs[3]->WriteCnt((DMAs[3]->Cnt & 0x0000FFFF) | (val << 16)); return;

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
            SetIRQ(1, IRQ_IPCSync);
        }
        //CompensateARM7();
        return;

    case 0x04000184:
        if (val & 0x0008)
            IPCFIFO9->Clear();
        if ((val & 0x0004) && (!(IPCFIFOCnt9 & 0x0004)) && IPCFIFO9->IsEmpty())
            SetIRQ(0, IRQ_IPCSendDone);
        if ((val & 0x0400) && (!(IPCFIFOCnt9 & 0x0400)) && (!IPCFIFO7->IsEmpty()))
            SetIRQ(0, IRQ_IPCRecv);
        if (val & 0x4000)
            IPCFIFOCnt9 &= ~0x4000;
        IPCFIFOCnt9 = val & 0x8404;
        return;

    case 0x040001A0:
        if (!(ExMemCnt[0] & (1<<11))) NDSCart::WriteSPICnt(val);
        return;
    case 0x040001A2:
        NDSCart::WriteSPIData(val & 0xFF);
        return;

    case 0x040001B8: ROMSeed0[4] = val & 0x7F; return;
    case 0x040001BA: ROMSeed1[4] = val & 0x7F; return;

    case 0x04000204:
        ExMemCnt[0] = val;
        ExMemCnt[1] = (ExMemCnt[1] & 0x007F) | (val & 0xFF80);
        return;

    case 0x04000208: IME[0] = val & 0x1; return;

    case 0x04000240:
        GPU::MapVRAM_AB(0, val & 0xFF);
        GPU::MapVRAM_AB(1, val >> 8);
        return;
    case 0x04000242:
        GPU::MapVRAM_CD(2, val & 0xFF);
        GPU::MapVRAM_CD(3, val >> 8);
        return;
    case 0x04000244:
        GPU::MapVRAM_E(4, val & 0xFF);
        GPU::MapVRAM_FG(5, val >> 8);
        return;
    case 0x04000246:
        GPU::MapVRAM_FG(6, val & 0xFF);
        MapSharedWRAM(val >> 8);
        return;
    case 0x04000248:
        GPU::MapVRAM_H(7, val & 0xFF);
        GPU::MapVRAM_I(8, val >> 8);
        return;

    case 0x04000280: DivCnt = val; StartDiv(); return;

    case 0x040002B0: SqrtCnt = val; StartSqrt(); return;

    case 0x04000300:
        if (PostFlag9 & 0x01) val |= 0x01;
        PostFlag9 = val & 0x03;
        return;

    case 0x04000304:
        PowerControl9 = val;
        GPU::DisplaySwap(PowerControl9>>15);
        return;
    }

    if ((addr >= 0x04000000 && addr < 0x04000060) || (addr == 0x0400006C))
    {
        GPU::GPU2D_A->Write16(addr, val);
        return;
    }
    if ((addr >= 0x04001000 && addr < 0x04001060) || (addr == 0x0400106C))
    {
        GPU::GPU2D_B->Write16(addr, val);
        return;
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        GPU3D::Write16(addr, val);
        return;
    }

    printf("unknown ARM9 IO write16 %08X %04X %08X\n", addr, val, ARM9->R[14]);
}

void ARM9IOWrite32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x04000060: GPU3D::Write32(addr, val); return;
    case 0x04000064: GPU::GPU2D_A->Write32(addr, val); return;

    case 0x040000B0: DMAs[0]->SrcAddr = val; return;
    case 0x040000B4: DMAs[0]->DstAddr = val; return;
    case 0x040000B8: DMAs[0]->WriteCnt(val); return;
    case 0x040000BC: DMAs[1]->SrcAddr = val; return;
    case 0x040000C0: DMAs[1]->DstAddr = val; return;
    case 0x040000C4: DMAs[1]->WriteCnt(val); return;
    case 0x040000C8: DMAs[2]->SrcAddr = val; return;
    case 0x040000CC: DMAs[2]->DstAddr = val; return;
    case 0x040000D0: DMAs[2]->WriteCnt(val); return;
    case 0x040000D4: DMAs[3]->SrcAddr = val; return;
    case 0x040000D8: DMAs[3]->DstAddr = val; return;
    case 0x040000DC: DMAs[3]->WriteCnt(val); return;

    case 0x040000E0: DMA9Fill[0] = val; return;
    case 0x040000E4: DMA9Fill[1] = val; return;
    case 0x040000E8: DMA9Fill[2] = val; return;
    case 0x040000EC: DMA9Fill[3] = val; return;

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

    case 0x04000188:
        if (IPCFIFOCnt9 & 0x8000)
        {
            if (IPCFIFO9->IsFull())
                IPCFIFOCnt9 |= 0x4000;
            else
            {
                bool wasempty = IPCFIFO9->IsEmpty();
                IPCFIFO9->Write(val);
                if ((IPCFIFOCnt7 & 0x0400) && wasempty)
                    SetIRQ(1, IRQ_IPCRecv);
            }
        }
        return;

    case 0x040001A0:
        if (!(ExMemCnt[0] & (1<<11)))
        {
            NDSCart::WriteSPICnt(val & 0xFFFF);
            NDSCart::WriteSPIData((val >> 16) & 0xFF);
        }
        return;
    case 0x040001A4:
        if (!(ExMemCnt[0] & (1<<11))) NDSCart::WriteROMCnt(val);
        return;

    case 0x040001B0: *(u32*)&ROMSeed0[0] = val; return;
    case 0x040001B4: *(u32*)&ROMSeed1[0] = val; return;

    case 0x04000208: IME[0] = val & 0x1; return;
    case 0x04000210: IE[0] = val; return;
    case 0x04000214: IF[0] &= ~val; GPU3D::CheckFIFOIRQ(); return;

    case 0x04000240:
        GPU::MapVRAM_AB(0, val & 0xFF);
        GPU::MapVRAM_AB(1, (val >> 8) & 0xFF);
        GPU::MapVRAM_CD(2, (val >> 16) & 0xFF);
        GPU::MapVRAM_CD(3, val >> 24);
        return;
    case 0x04000244:
        GPU::MapVRAM_E(4, val & 0xFF);
        GPU::MapVRAM_FG(5, (val >> 8) & 0xFF);
        GPU::MapVRAM_FG(6, (val >> 16) & 0xFF);
        MapSharedWRAM(val >> 24);
        return;
    case 0x04000248:
        GPU::MapVRAM_H(7, val & 0xFF);
        GPU::MapVRAM_I(8, (val >> 8) & 0xFF);
        return;

    case 0x04000290: DivNumerator[0] = val; StartDiv(); return;
    case 0x04000294: DivNumerator[1] = val; StartDiv(); return;
    case 0x04000298: DivDenominator[0] = val; StartDiv(); return;
    case 0x0400029C: DivDenominator[1] = val; StartDiv(); return;

    case 0x040002B8: SqrtVal[0] = val; StartSqrt(); return;
    case 0x040002BC: SqrtVal[1] = val; StartSqrt(); return;
    }

    if (addr >= 0x04000000 && addr < 0x04000060)
    {
        GPU::GPU2D_A->Write32(addr, val);
        return;
    }
    if (addr >= 0x04001000 && addr < 0x04001060)
    {
        GPU::GPU2D_B->Write32(addr, val);
        return;
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        GPU3D::Write32(addr, val);
        return;
    }

    printf("unknown ARM9 IO write32 %08X %08X\n", addr, val);
}


u8 ARM7IORead8(u32 addr)
{
    switch (addr)
    {
    case 0x04000138: return RTC::Read() & 0xFF;

    case 0x040001A2: return NDSCart::ReadSPIData();

    case 0x040001C2: return SPI::ReadData();

    case 0x04000208: return IME[1];

    case 0x04000240: return GPU::VRAMSTAT;
    case 0x04000241: return WRAMCnt;

    case 0x04000300: return PostFlag7;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        // sound I/O
        return 0;
    }

    printf("unknown ARM7 IO read8 %08X\n", addr);
    return 0;
}

u16 ARM7IORead16(u32 addr)
{
    switch (addr)
    {
    case 0x04000004: return GPU::DispStat[1];
    case 0x04000006: return GPU::VCount;

    case 0x040000B8: return DMAs[4]->Cnt & 0xFFFF;
    case 0x040000BA: return DMAs[4]->Cnt >> 16;
    case 0x040000C4: return DMAs[5]->Cnt & 0xFFFF;
    case 0x040000C6: return DMAs[5]->Cnt >> 16;
    case 0x040000D0: return DMAs[6]->Cnt & 0xFFFF;
    case 0x040000D2: return DMAs[6]->Cnt >> 16;
    case 0x040000DC: return DMAs[7]->Cnt & 0xFFFF;
    case 0x040000DE: return DMAs[7]->Cnt >> 16;

    case 0x04000100: return TimerGetCounter(4);
    case 0x04000102: return Timers[4].Cnt;
    case 0x04000104: return TimerGetCounter(5);
    case 0x04000106: return Timers[5].Cnt;
    case 0x04000108: return TimerGetCounter(6);
    case 0x0400010A: return Timers[6].Cnt;
    case 0x0400010C: return TimerGetCounter(7);
    case 0x0400010E: return Timers[7].Cnt;

    case 0x04000130: return KeyInput & 0xFFFF;
    case 0x04000136: return KeyInput >> 16;

    case 0x04000134: return 0x8000;
    case 0x04000138: return RTC::Read();

    case 0x04000180: return IPCSync7;
    case 0x04000184:
        {
            u16 val = IPCFIFOCnt7;
            if (IPCFIFO7->IsEmpty())     val |= 0x0001;
            else if (IPCFIFO7->IsFull()) val |= 0x0002;
            if (IPCFIFO9->IsEmpty())     val |= 0x0100;
            else if (IPCFIFO9->IsFull()) val |= 0x0200;
            return val;
        }

    case 0x040001A0: return NDSCart::SPICnt;
    case 0x040001A2: return NDSCart::ReadSPIData();

    case 0x040001C0: return SPI::Cnt;
    case 0x040001C2: return SPI::ReadData();

    case 0x04000204: return ExMemCnt[1];
    case 0x04000208: return IME[1];

    case 0x04000300: return PostFlag7;
    case 0x04000304: return PowerControl7;
    case 0x04000308: return ARM7BIOSProt;

    case 0x04000504: return _soundbias;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        // sound I/O
        return 0;
    }

    printf("unknown ARM7 IO read16 %08X %08X\n", addr, ARM9->R[15]);
    return 0;
}

u32 ARM7IORead32(u32 addr)
{
    switch (addr)
    {
    case 0x04000004: return GPU::DispStat[1] | (GPU::VCount << 16);

    case 0x040000B0: return DMAs[4]->SrcAddr;
    case 0x040000B4: return DMAs[4]->DstAddr;
    case 0x040000B8: return DMAs[4]->Cnt;
    case 0x040000BC: return DMAs[5]->SrcAddr;
    case 0x040000C0: return DMAs[5]->DstAddr;
    case 0x040000C4: return DMAs[5]->Cnt;
    case 0x040000C8: return DMAs[6]->SrcAddr;
    case 0x040000CC: return DMAs[6]->DstAddr;
    case 0x040000D0: return DMAs[6]->Cnt;
    case 0x040000D4: return DMAs[7]->SrcAddr;
    case 0x040000D8: return DMAs[7]->DstAddr;
    case 0x040000DC: return DMAs[7]->Cnt;

    case 0x04000100: return TimerGetCounter(4) | (Timers[4].Cnt << 16);
    case 0x04000104: return TimerGetCounter(5) | (Timers[5].Cnt << 16);
    case 0x04000108: return TimerGetCounter(6) | (Timers[6].Cnt << 16);
    case 0x0400010C: return TimerGetCounter(7) | (Timers[7].Cnt << 16);

    case 0x040001A0: return NDSCart::SPICnt | (NDSCart::ReadSPIData() << 16);
    case 0x040001A4: return NDSCart::ROMCnt;

    case 0x040001C0:
        return SPI::Cnt | (SPI::ReadData() << 16);

    case 0x04000208: return IME[1];
    case 0x04000210: return IE[1];
    case 0x04000214: return IF[1];

    case 0x04100000:
        if (IPCFIFOCnt7 & 0x8000)
        {
            u32 ret;
            if (IPCFIFO9->IsEmpty())
            {
                IPCFIFOCnt7 |= 0x4000;
                ret = IPCFIFO9->Peek();
            }
            else
            {
                ret = IPCFIFO9->Read();

                if (IPCFIFO9->IsEmpty() && (IPCFIFOCnt9 & 0x0004))
                    SetIRQ(0, IRQ_IPCSendDone);
            }
            return ret;
        }
        else
            return IPCFIFO9->Peek();

    case 0x04100010:
        if (ExMemCnt[0] & (1<<11)) return NDSCart::ReadROMData();
        return 0;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        // sound I/O
        return 0;
    }

    printf("unknown ARM7 IO read32 %08X\n", addr);
    return 0;
}

void ARM7IOWrite8(u32 addr, u8 val)
{
    switch (addr)
    {
    case 0x04000138: RTC::Write(val, true); return;

    case 0x040001A0:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCart::WriteSPICnt((NDSCart::SPICnt & 0xFF00) | val);
        }
        return;
    case 0x040001A1:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCart::WriteSPICnt((NDSCart::SPICnt & 0x00FF) | (val << 8));
        }
        return;
    case 0x040001A2:
        NDSCart::WriteSPIData(val);
        return;

    case 0x040001A8: NDSCart::ROMCommand[0] = val; return;
    case 0x040001A9: NDSCart::ROMCommand[1] = val; return;
    case 0x040001AA: NDSCart::ROMCommand[2] = val; return;
    case 0x040001AB: NDSCart::ROMCommand[3] = val; return;
    case 0x040001AC: NDSCart::ROMCommand[4] = val; return;
    case 0x040001AD: NDSCart::ROMCommand[5] = val; return;
    case 0x040001AE: NDSCart::ROMCommand[6] = val; return;
    case 0x040001AF: NDSCart::ROMCommand[7] = val; return;

    case 0x040001C2:
        SPI::WriteData(val);
        return;

    case 0x04000208: IME[1] = val & 0x1; return;

    case 0x04000300:
        if (ARM7->R[15] >= 0x4000)
            return;
        if (!(PostFlag7 & 0x01))
            PostFlag7 = val & 0x01;
        return;

    case 0x04000301:
        if (val == 0x80) ARM7->Halt(1);
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        // sound I/O
        return;
    }

    printf("unknown ARM7 IO write8 %08X %02X\n", addr, val);
}

void ARM7IOWrite16(u32 addr, u16 val)
{
    switch (addr)
    {
    case 0x04000004: GPU::SetDispStat(1, val); return;

    case 0x040000B8: DMAs[4]->WriteCnt((DMAs[4]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000BA: DMAs[4]->WriteCnt((DMAs[4]->Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000C4: DMAs[5]->WriteCnt((DMAs[5]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000C6: DMAs[5]->WriteCnt((DMAs[5]->Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000D0: DMAs[6]->WriteCnt((DMAs[6]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000D2: DMAs[6]->WriteCnt((DMAs[6]->Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000DC: DMAs[7]->WriteCnt((DMAs[7]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000DE: DMAs[7]->WriteCnt((DMAs[7]->Cnt & 0x0000FFFF) | (val << 16)); return;

    case 0x04000100: Timers[4].Reload = val; return;
    case 0x04000102: TimerStart(4, val); return;
    case 0x04000104: Timers[5].Reload = val; return;
    case 0x04000106: TimerStart(5, val); return;
    case 0x04000108: Timers[6].Reload = val; return;
    case 0x0400010A: TimerStart(6, val); return;
    case 0x0400010C: Timers[7].Reload = val; return;
    case 0x0400010E: TimerStart(7, val); return;

    case 0x04000134: return;printf("set debug port %04X %08X\n", val, ARM7Read32(ARM7->R[13]+4)); return;

    case 0x04000138: RTC::Write(val, false); return;

    case 0x04000180:
        IPCSync9 &= 0xFFF0;
        IPCSync9 |= ((val & 0x0F00) >> 8);
        IPCSync7 &= 0xB0FF;
        IPCSync7 |= (val & 0x4F00);
        if ((val & 0x2000) && (IPCSync9 & 0x4000))
        {
            SetIRQ(0, IRQ_IPCSync);
        }
        return;

    case 0x04000184:
        if (val & 0x0008)
            IPCFIFO7->Clear();
        if ((val & 0x0004) && (!(IPCFIFOCnt7 & 0x0004)) && IPCFIFO7->IsEmpty())
            SetIRQ(1, IRQ_IPCSendDone);
        if ((val & 0x0400) && (!(IPCFIFOCnt7 & 0x0400)) && (!IPCFIFO9->IsEmpty()))
            SetIRQ(1, IRQ_IPCRecv);
        if (val & 0x4000)
            IPCFIFOCnt7 &= ~0x4000;
        IPCFIFOCnt7 = val & 0x8404;
        return;

    case 0x040001A0:
        if (ExMemCnt[0] & (1<<11))
            NDSCart::WriteSPICnt(val);
        return;
    case 0x040001A2:
        NDSCart::WriteSPIData(val & 0xFF);
        return;

    case 0x040001B8: ROMSeed0[12] = val & 0x7F; return;
    case 0x040001BA: ROMSeed1[12] = val & 0x7F; return;

    case 0x040001C0:
        SPI::WriteCnt(val);
        return;
    case 0x040001C2:
        SPI::WriteData(val & 0xFF);
        return;

    case 0x04000204:
        ExMemCnt[1] = (ExMemCnt[1] & 0xFF80) | (val & 0x007F);
        return;

    case 0x04000208: IME[1] = val & 0x1; return;

    case 0x04000300:
        if (ARM7->R[15] >= 0x4000)
            return;
        if (!(PostFlag7 & 0x01))
            PostFlag7 = val & 0x01;
        return;

    case 0x04000304: PowerControl7 = val; return;

    case 0x04000308:
        if (ARM7BIOSProt == 0)
            ARM7BIOSProt = val;
        return;

    case 0x04000504: // removeme
        _soundbias = val & 0x3FF;
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        // sound I/O
        return;
    }

    printf("unknown ARM7 IO write16 %08X %04X\n", addr, val);
}

void ARM7IOWrite32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x040000B0: DMAs[4]->SrcAddr = val; return;
    case 0x040000B4: DMAs[4]->DstAddr = val; return;
    case 0x040000B8: DMAs[4]->WriteCnt(val); return;
    case 0x040000BC: DMAs[5]->SrcAddr = val; return;
    case 0x040000C0: DMAs[5]->DstAddr = val; return;
    case 0x040000C4: DMAs[5]->WriteCnt(val); return;
    case 0x040000C8: DMAs[6]->SrcAddr = val; return;
    case 0x040000CC: DMAs[6]->DstAddr = val; return;
    case 0x040000D0: DMAs[6]->WriteCnt(val); return;
    case 0x040000D4: DMAs[7]->SrcAddr = val; return;
    case 0x040000D8: DMAs[7]->DstAddr = val; return;
    case 0x040000DC: DMAs[7]->WriteCnt(val); return;

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

    case 0x04000188:
        if (IPCFIFOCnt7 & 0x8000)
        {
            if (IPCFIFO7->IsFull())
                IPCFIFOCnt7 |= 0x4000;
            else
            {
                bool wasempty = IPCFIFO7->IsEmpty();
                IPCFIFO7->Write(val);
                if ((IPCFIFOCnt9 & 0x0400) && wasempty)
                    SetIRQ(0, IRQ_IPCRecv);
            }
        }
        return;

    case 0x040001A0:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCart::WriteSPICnt(val & 0xFFFF);
            NDSCart::WriteSPIData((val >> 16) & 0xFF);
        }
        return;
    case 0x040001A4:
        if (ExMemCnt[0] & (1<<11)) NDSCart::WriteROMCnt(val);
        return;

    case 0x040001B0: *(u32*)&ROMSeed0[8] = val; return;
    case 0x040001B4: *(u32*)&ROMSeed1[8] = val; return;

    case 0x04000208: IME[1] = val & 0x1; return;
    case 0x04000210: IE[1] = val; return;
    case 0x04000214: IF[1] &= ~val; return;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        // sound I/O
        return;
    }

    printf("unknown ARM7 IO write32 %08X %08X\n", addr, val);
}

}
