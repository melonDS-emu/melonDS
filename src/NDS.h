/*
    Copyright 2016-2023 melonDS team

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

#include <memory>
#include <string>
#include <memory>
#include <functional>

#include "Platform.h"
#include "Savestate.h"
#include "types.h"
#include "NDSCart.h"
#include "GBACart.h"
#include "MemRegion.h"
#include "SPU.h"
#include "SPI.h"
#include "RTC.h"
#include "Wifi.h"
#include "AREngine.h"
#include "GPU.h"
#include "ARMJIT.h"
#include "DMA.h"

// when touching the main loop/timing code, pls test a lot of shit
// with this enabled, to make sure it doesn't desync
//#define DEBUG_CHECK_DESYNC

namespace melonDS
{

class NDS
{

public:
    enum
    {
        Event_LCD = 0,
        Event_SPU,
        Event_Wifi,
        Event_RTC,

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

    typedef std::function<void(u32)> EventFunc;
    #define MemberEventFunc(cls,func) std::bind(&cls::func,this,std::placeholders::_1)
    struct SchedEvent
    {
        std::map<u32, EventFunc> Funcs;
        u64 Timestamp;
        u32 FuncID;
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

    enum
    {
        CPUStop_DMA9_0 = (1<<0),
        CPUStop_DMA9_1 = (1<<1),
        CPUStop_DMA9_2 = (1<<2),
        CPUStop_DMA9_3 = (1<<3),
        CPUStop_NDMA9_0 = (1<<4),
        CPUStop_NDMA9_1 = (1<<5),
        CPUStop_NDMA9_2 = (1<<6),
        CPUStop_NDMA9_3 = (1<<7),
        CPUStop_DMA9 = 0xFFF,

        CPUStop_DMA7_0 = (1<<16),
        CPUStop_DMA7_1 = (1<<17),
        CPUStop_DMA7_2 = (1<<18),
        CPUStop_DMA7_3 = (1<<19),
        CPUStop_NDMA7_0 = (1<<20),
        CPUStop_NDMA7_1 = (1<<21),
        CPUStop_NDMA7_2 = (1<<22),
        CPUStop_NDMA7_3 = (1<<23),
        CPUStop_DMA7 = (0xFFF<<16),

        CPUStop_Wakeup = (1<<29),
        CPUStop_Sleep = (1<<30),
        CPUStop_GXStall = (1<<31),
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

    // supported GBA slot addon types
    enum
    {
        GBAAddon_RAMExpansion = 1,
    };

public:
    // The frontend should set and unset this manually after creating and destroying the NDS object.
    [[deprecated("Temporary workaround until JIT code generation is revised to accommodate multiple NDS objects.")]] static NDS* Current;

    NDS() noexcept : NDS(0) {}
    virtual ~NDS() noexcept;
    NDS(const NDS&) = delete;
    NDS& operator=(const NDS&) = delete;
    NDS(NDS&&) = delete;
    NDS& operator=(NDS&&) = delete;
    // Keep these components in the same order (relative to each other),
    // as all constructors are run in the order of member declaration.
    // (regardless of the order that they're listed in the constructor's initializer)
    melonDS::ARMJIT JIT;
    melonDS::SPU SPU;
    melonDS::GPU GPU;
    melonDS::SPIHost SPI;
    melonDS::RTC RTC;
    melonDS::Wifi Wifi;
    melonDS::NDSCart::NDSCartSlot NDSCartSlot;
    melonDS::GBACart::GBACartSlot GBACartSlot;
    melonDS::AREngine AREngine;
    ARMv5 ARM9;
    ARMv4 ARM7;
    std::array<DMA, 8> DMAs;

protected:
    explicit NDS(int type) noexcept;
    virtual void DoSavestateExtra(Savestate* file) noexcept {}
public:
#ifdef JIT_ENABLED
    bool EnableJIT;
#endif
    const int ConsoleType;
    int CurCPU;

    u8 ARM9MemTimings[0x40000][8];
    std::array<u32, 0x40000> ARM9Regions;
    u8 ARM7MemTimings[0x20000][4];
    std::array<u32, 0x20000> ARM7Regions;

    u32 NumFrames;
    u32 NumLagFrames;
    bool LagFrameFlag;
    u64 LastSysClockCycles;
    u64 FrameStartTimestamp;

    // no need to worry about those overflowing, they can keep going for atleast 4350 years
    u64 ARM9Timestamp, ARM9Target;
    u64 ARM7Timestamp, ARM7Target;
    u32 ARM9ClockShift;
    u64 SysTimestamp;

    u32 IME[2];
    u32 IE[2];
    u32 IF[2];
    u32 IE2;
    u32 IF2;
    Timer Timers[8];

    std::array<SchedEvent, Event_MAX> SchedList;
    u32 SchedListMask;
    u32 CPUStop;

    u16 PowerControl9;

    u16 ExMemCnt[2];
    u8 ROMSeed0[2*8];
    u8 ROMSeed1[2*8];

    u8 ARM9BIOS[ARM9BIOSLength];
    u8 ARM7BIOS[ARM7BIOSLength];
    u16 ARM7BIOSProt;

    u8* MainRAM;
    u32 MainRAMMask;
    u8* SharedWRAM;
    u8 WRAMCnt;

    MemRegion SWRAM_ARM9;
    MemRegion SWRAM_ARM7;

    u32 KeyInput;
    u16 RCnt;

    u8* ARM7WRAM;

    u8 PostFlag9;
    u8 PostFlag7;
    u16 PowerControl7;

    u16 WifiWaitCnt;
    u8 TimerCheckMask[2];
    u64 TimerTimestamp[2];

    u32 DMA9Fill[4];

    u16 IPCSync9, IPCSync7;
    u16 IPCFIFOCnt9, IPCFIFOCnt7;
    FIFO<u32, 16> IPCFIFO9; // FIFO in which the ARM9 writes
    FIFO<u32, 16> IPCFIFO7;

    u16 DivCnt;
    u32 DivNumerator[2];
    u32 DivDenominator[2];
    u32 DivQuotient[2];
    u32 DivRemainder[2];

    u16 SqrtCnt;
    u32 SqrtVal[2];
    u32 SqrtRes;

    u16 KeyCnt[2];

    bool Running;

    bool RunningGame;
public:
    virtual void Reset() noexcept;
    void Start() noexcept;
    u32 RunFrame() noexcept;

    /// Stop the emulator.
    virtual void Stop(Platform::StopReason reason) noexcept;
    void Stop() noexcept { Stop(Platform::External); }

    bool DoSavestate(Savestate* file) noexcept;

    void InitTimings() noexcept;
    void SetARM9RegionTimings(u32 addrstart, u32 addrend, u32 region, int buswidth, int nonseq, int seq) noexcept;
    void SetARM7RegionTimings(u32 addrstart, u32 addrend, u32 region, int buswidth, int nonseq, int seq) noexcept;

    void LoadBIOS() noexcept;
    bool IsLoadedARM9BIOSBuiltIn() const noexcept;
    bool IsLoadedARM7BIOSBuiltIn() const noexcept;

    virtual bool LoadCart(const u8* romdata, u32 romlen, const u8* savedata, u32 savelen) noexcept;
    void LoadSave(const u8* savedata, u32 savelen) noexcept;
    virtual void EjectCart() noexcept;
    [[nodiscard]] bool CartInserted() const noexcept;

    [[nodiscard]] virtual bool NeedsDirectBoot() const noexcept;
    void SetupDirectBoot(const std::string& romname) noexcept;
    virtual void SetupDirectBoot() noexcept;

    bool LoadGBACart(const u8* romdata, u32 romlen, const u8* savedata, u32 savelen) noexcept;
    void LoadGBAAddon(int type) noexcept;
    void EjectGBACart() noexcept;

    void TouchScreen(u16 x, u16 y) noexcept;
    void ReleaseScreen() noexcept;

    void SetKeyMask(u32 mask) noexcept;

    bool IsLidClosed() const noexcept;
    void SetLidClosed(bool closed) noexcept;

    // TODO: support things like the GBA-slot camera addon whenever these are emulated
    virtual void CamInputFrame(int cam, const u32* data, int width, int height, bool rgb) noexcept {}
    void MicInputFrame(s16* data, int samples) noexcept;

    void RegisterEventFunc(u32 id, u32 funcid, const EventFunc& func) noexcept;
    void UnregisterEventFunc(u32 id, u32 funcid) noexcept;
    void ScheduleEvent(u32 id, bool periodic, s32 delay, u32 funcid, u32 param) noexcept;
    void CancelEvent(u32 id) noexcept;

    void debug(u32 p) noexcept;

    void Halt() noexcept;

    void MapSharedWRAM(u8 val) noexcept;

    void UpdateIRQ(u32 cpu) noexcept;
    void SetIRQ(u32 cpu, u32 irq) noexcept;
    void ClearIRQ(u32 cpu, u32 irq) noexcept;
    void SetIRQ2(u32 irq) noexcept;
    void ClearIRQ2(u32 irq) noexcept;
    bool HaltInterrupted(u32 cpu) const noexcept;
    void StopCPU(u32 cpu, u32 mask) noexcept;
    void ResumeCPU(u32 cpu, u32 mask) noexcept;
    void GXFIFOStall() noexcept;
    void GXFIFOUnstall() noexcept;

    u32 GetPC(u32 cpu) const noexcept;
    u64 GetSysClockCycles(int num) noexcept;
    void NocashPrint(u32 cpu, u32 addr) noexcept;

    void MonitorARM9Jump(u32 addr) noexcept;

    virtual bool DMAsInMode(u32 cpu, u32 mode) const noexcept;
    virtual bool DMAsRunning(u32 cpu) const noexcept;
    virtual void CheckDMAs(u32 cpu, u32 mode) noexcept;
    virtual void StopDMAs(u32 cpu, u32 mode) noexcept;

    void RunTimers(u32 cpu) noexcept;

    virtual u8 ARM9Read8(u32 addr) noexcept;
    virtual u16 ARM9Read16(u32 addr) noexcept;
    virtual u32 ARM9Read32(u32 addr) noexcept;
    virtual void ARM9Write8(u32 addr, u8 val) noexcept;
    virtual void ARM9Write16(u32 addr, u16 val) noexcept;
    virtual void ARM9Write32(u32 addr, u32 val) noexcept;

    virtual bool ARM9GetMemRegion(u32 addr, bool write, MemRegion* region) noexcept;

    virtual u8 ARM7Read8(u32 addr) noexcept;
    virtual u16 ARM7Read16(u32 addr) noexcept;
    virtual u32 ARM7Read32(u32 addr) noexcept;
    virtual void ARM7Write8(u32 addr, u8 val) noexcept;
    virtual void ARM7Write16(u32 addr, u16 val) noexcept;
    virtual void ARM7Write32(u32 addr, u32 val) noexcept;

    virtual bool ARM7GetMemRegion(u32 addr, bool write, MemRegion* region) noexcept;

    virtual u8 ARM9IORead8(u32 addr) noexcept;
    virtual u16 ARM9IORead16(u32 addr) noexcept;
    virtual u32 ARM9IORead32(u32 addr) noexcept;
    virtual void ARM9IOWrite8(u32 addr, u8 val) noexcept;
    virtual void ARM9IOWrite16(u32 addr, u16 val) noexcept;
    virtual void ARM9IOWrite32(u32 addr, u32 val) noexcept;

    virtual u8 ARM7IORead8(u32 addr) noexcept;
    virtual u16 ARM7IORead16(u32 addr) noexcept;
    virtual u32 ARM7IORead32(u32 addr) noexcept;
    virtual void ARM7IOWrite8(u32 addr, u8 val) noexcept;
    virtual void ARM7IOWrite16(u32 addr, u16 val) noexcept;
    virtual void ARM7IOWrite32(u32 addr, u32 val) noexcept;
private:
    template <bool EnableJIT, int ConsoleType>
    u32 RunFrame() noexcept;

    u64 NextTarget() noexcept;
    u64 NextTargetSleep() const noexcept;
    void CheckKeyIRQ(u32 cpu, u32 oldkey, u32 newkey) noexcept;
    void Reschedule(u64 target) noexcept;
    void RunSystemSleep(u64 timestamp) noexcept;
    void RunSystem(u64 timestamp) noexcept;
    void HandleTimerOverflow(u32 tid) noexcept;
    u16 TimerGetCounter(u32 timer) noexcept;
    void TimerStart(u32 id, u16 cnt) noexcept;
    void StartDiv() noexcept;
    void DivDone(u32 param) noexcept;
    void SqrtDone(u32 param) noexcept;
    void StartSqrt() noexcept;
    void RunTimer(u32 tid, s32 cycles) noexcept;
    void UpdateWifiTimings() noexcept;
    void SetWifiWaitCnt(u16 val) noexcept;
    void SetGBASlotTimings() noexcept;
    void EnterSleepMode() noexcept;
};

}
#endif // NDS_H
