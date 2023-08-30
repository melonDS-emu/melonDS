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

#ifndef NDS_H
#define NDS_H

#include <string>

#include "Platform.h"
#include "Savestate.h"
#include "types.h"

// when touching the main loop/timing code, pls test a lot of shit
// with this enabled, to make sure it doesn't desync
//#define DEBUG_CHECK_DESYNC

namespace NDS
{

enum
{
    Event_LCD = 0,
    Event_SPU,
    Event_Wifi,

    Event_DisplayFIFO,
    Event_ROMTransfer,
    Event_ROMSPITransfer,
    Event_SPITransfer,
    Event_Div,
    Event_Sqrt,

    // DSi
    Event_DSi_SDMMCTransfer,
    Event_DSi_SDIOTransfer,
    Event_DSi_NWifi,
    Event_DSi_CamIRQ,
    Event_DSi_CamTransfer,
    Event_DSi_DSP,

    Event_MAX
};

struct SchedEvent
{
    void (*Func)(u32 param);
    u64 Timestamp;
    u32 Param;
};

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
    IRQ_CartXferDone,
    IRQ_CartIREQMC,   // IRQ triggered by game cart (example: Pokémon Typing Adventure, BT controller)
    IRQ_GXFIFO,
    IRQ_LidOpen,
    IRQ_SPI,
    IRQ_Wifi,

    // DSi IRQs
    IRQ_DSi_DSP = 24,
    IRQ_DSi_Camera,
    IRQ_DSi_Unk26,
    IRQ_DSi_Unk27,
    IRQ_DSi_NDMA0,
    IRQ_DSi_NDMA1,
    IRQ_DSi_NDMA2,
    IRQ_DSi_NDMA3,
};

enum
{
    // DSi ARM7-side IE2/IF2
    IRQ2_DSi_GPIO18_0 = 0,
    IRQ2_DSi_GPIO18_1,
    IRQ2_DSi_GPIO18_2,
    IRQ2_DSi_Unused3,
    IRQ2_DSi_GPIO33_0,
    IRQ2_DSi_Headphone,
    IRQ2_DSi_BPTWL,
    IRQ2_DSi_GPIO33_3, // "sound enable input"
    IRQ2_DSi_SDMMC,
    IRQ2_DSi_SD_Data1,
    IRQ2_DSi_SDIO,
    IRQ2_DSi_SDIO_Data1,
    IRQ2_DSi_AES,
    IRQ2_DSi_I2C,
    IRQ2_DSi_MicExt
};

struct Timer
{
    u16 Reload;
    u16 Cnt;
    u32 Counter;
    u32 CycleShift;
};

enum
{
    Mem9_ITCM       = 0x00000001,
    Mem9_DTCM       = 0x00000002,
    Mem9_BIOS       = 0x00000004,
    Mem9_MainRAM    = 0x00000008,
    Mem9_WRAM       = 0x00000010,
    Mem9_IO         = 0x00000020,
    Mem9_Pal        = 0x00000040,
    Mem9_OAM        = 0x00000080,
    Mem9_VRAM       = 0x00000100,
    Mem9_GBAROM     = 0x00020000,
    Mem9_GBARAM     = 0x00040000,

    Mem7_BIOS       = 0x00000001,
    Mem7_MainRAM    = 0x00000002,
    Mem7_WRAM       = 0x00000004,
    Mem7_IO         = 0x00000008,
    Mem7_Wifi0      = 0x00000010,
    Mem7_Wifi1      = 0x00000020,
    Mem7_VRAM       = 0x00000040,
    Mem7_GBAROM     = 0x00000100,
    Mem7_GBARAM     = 0x00000200,

    // TODO: add DSi regions!
};

struct MemRegion
{
    u8* Mem;
    u32 Mask;
};

// supported GBA slot addon types
enum
{
    GBAAddon_RAMExpansion = 1,
};

#ifdef JIT_ENABLED
extern bool EnableJIT;
#endif
extern int ConsoleType;
extern int CurCPU;

extern u8 ARM9MemTimings[0x40000][8];
extern u32 ARM9Regions[0x40000];
extern u8 ARM7MemTimings[0x20000][4];
extern u32 ARM7Regions[0x20000];

extern u32 NumFrames;
extern u32 NumLagFrames;
extern bool LagFrameFlag;

extern u64 ARM9Timestamp, ARM9Target;
extern u64 ARM7Timestamp, ARM7Target;
extern u32 ARM9ClockShift;

extern u32 IME[2];
extern u32 IE[2];
extern u32 IF[2];
extern u32 IE2;
extern u32 IF2;
extern Timer Timers[8];

extern u32 CPUStop;

extern u16 PowerControl9;

extern u16 ExMemCnt[2];
extern u8 ROMSeed0[2*8];
extern u8 ROMSeed1[2*8];

extern u8 ARM9BIOS[0x1000];
extern u8 ARM7BIOS[0x4000];
extern u16 ARM7BIOSProt;

extern u8* MainRAM;
extern u32 MainRAMMask;

const u32 MainRAMMaxSize = 0x1000000;

const u32 SharedWRAMSize = 0x8000;
extern u8* SharedWRAM;

extern MemRegion SWRAM_ARM9;
extern MemRegion SWRAM_ARM7;

extern u32 KeyInput;

const u32 ARM7WRAMSize = 0x10000;
extern u8* ARM7WRAM;

bool Init();
void DeInit();
void Reset();
void Start();

/// Stop the emulator.
void Stop(Platform::StopReason reason = Platform::StopReason::External);

bool DoSavestate(Savestate* file);

void SetARM9RegionTimings(u32 addrstart, u32 addrend, u32 region, int buswidth, int nonseq, int seq);
void SetARM7RegionTimings(u32 addrstart, u32 addrend, u32 region, int buswidth, int nonseq, int seq);

// 0=DS  1=DSi
void SetConsoleType(int type);

void LoadBIOS();
bool IsLoadedARM9BIOSBuiltIn();
bool IsLoadedARM7BIOSBuiltIn();

bool LoadCart(const u8* romdata, u32 romlen, const u8* savedata, u32 savelen);
void LoadSave(const u8* savedata, u32 savelen);
void EjectCart();
bool CartInserted();

bool NeedsDirectBoot();
void SetupDirectBoot(const std::string& romname);

bool LoadGBACart(const u8* romdata, u32 romlen, const u8* savedata, u32 savelen);
void LoadGBAAddon(int type);
void EjectGBACart();

u32 RunFrame();

void TouchScreen(u16 x, u16 y);
void ReleaseScreen();

void SetKeyMask(u32 mask);

bool IsLidClosed();
void SetLidClosed(bool closed);

void CamInputFrame(int cam, u32* data, int width, int height, bool rgb);
void MicInputFrame(s16* data, int samples);

void ScheduleEvent(u32 id, bool periodic, s32 delay, void (*func)(u32), u32 param);
void ScheduleEvent(u32 id, u64 timestamp, void (*func)(u32), u32 param);
void CancelEvent(u32 id);

void debug(u32 p);

void Halt();

void MapSharedWRAM(u8 val);

void UpdateIRQ(u32 cpu);
void SetIRQ(u32 cpu, u32 irq);
void ClearIRQ(u32 cpu, u32 irq);
void SetIRQ2(u32 irq);
void ClearIRQ2(u32 irq);
bool HaltInterrupted(u32 cpu);
void StopCPU(u32 cpu, u32 mask);
void ResumeCPU(u32 cpu, u32 mask);
void GXFIFOStall();
void GXFIFOUnstall();

u32 GetPC(u32 cpu);
u64 GetSysClockCycles(int num);
void NocashPrint(u32 cpu, u32 addr);

void MonitorARM9Jump(u32 addr);

bool DMAsInMode(u32 cpu, u32 mode);
bool DMAsRunning(u32 cpu);
void CheckDMAs(u32 cpu, u32 mode);
void StopDMAs(u32 cpu, u32 mode);

void RunTimers(u32 cpu);

u8 ARM9Read8(u32 addr);
u16 ARM9Read16(u32 addr);
u32 ARM9Read32(u32 addr);
void ARM9Write8(u32 addr, u8 val);
void ARM9Write16(u32 addr, u16 val);
void ARM9Write32(u32 addr, u32 val);

bool ARM9GetMemRegion(u32 addr, bool write, MemRegion* region);

u8 ARM7Read8(u32 addr);
u16 ARM7Read16(u32 addr);
u32 ARM7Read32(u32 addr);
void ARM7Write8(u32 addr, u8 val);
void ARM7Write16(u32 addr, u16 val);
void ARM7Write32(u32 addr, u32 val);

bool ARM7GetMemRegion(u32 addr, bool write, MemRegion* region);

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
