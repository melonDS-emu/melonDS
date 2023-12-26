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
#include <optional>
#include <functional>

#include "Platform.h"
#include "Savestate.h"
#include "types.h"
#include "NDSCart.h"
#include "GBACart.h"
#include "SPU.h"
#include "SPI.h"
#include "RTC.h"
#include "Wifi.h"
#include "AREngine.h"
#include "GPU.h"
#include "ARMJIT.h"
#include "MemRegion.h"
#include "ARMJIT_Memory.h"
#include "ARM.h"
#include "CRC32.h"
#include "DMA.h"
#include "FreeBIOS.h"

// when touching the main loop/timing code, pls test a lot of shit
// with this enabled, to make sure it doesn't desync
//#define DEBUG_CHECK_DESYNC

namespace melonDS
{
struct NDSArgs;
class Firmware;
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

class SPU;
class SPIHost;
class RTC;
class Wifi;

class AREngine;
class GPU;
class ARMJIT;

class NDS
{
private:
#ifdef JIT_ENABLED
    bool EnableJIT;
#endif

public: // TODO: Encapsulate the rest of these members
    int ConsoleType;
    int CurCPU;

    SchedEvent SchedList[Event_MAX] {};
    u8 ARM9MemTimings[0x40000][8];
    u32 ARM9Regions[0x40000];
    u8 ARM7MemTimings[0x20000][4];
    u32 ARM7Regions[0x20000];

    u32 NumFrames;
    u32 NumLagFrames;
    bool LagFrameFlag;

    // no need to worry about those overflowing, they can keep going for atleast 4350 years
    u64 ARM9Timestamp, ARM9Target;
    u64 ARM7Timestamp, ARM7Target;
    u32 ARM9ClockShift;

    u32 IME[2];
    u32 IE[2];
    u32 IF[2];
    u32 IE2;
    u32 IF2;
    Timer Timers[8];

    u32 CPUStop;

    u16 PowerControl9;

    u16 ExMemCnt[2];
    alignas(u32) u8 ROMSeed0[2*8];
    alignas(u32) u8 ROMSeed1[2*8];

protected:
    // These BIOS arrays should be declared *before* the component objects (JIT, SPI, etc.)
    // so that they're initialized before the component objects' constructors run.
    std::array<u8, ARM9BIOSSize> ARM9BIOS;
    std::array<u8, ARM7BIOSSize> ARM7BIOS;
    bool ARM9BIOSNative;
    bool ARM7BIOSNative;
public: // TODO: Encapsulate the rest of these members
    u16 ARM7BIOSProt;

    u8* MainRAM;
    u32 MainRAMMask;

    const u32 MainRAMMaxSize = 0x1000000;

    const u32 SharedWRAMSize = 0x8000;
    u8* SharedWRAM;

    MemRegion SWRAM_ARM9;
    MemRegion SWRAM_ARM7;

    u32 KeyInput;
    u16 RCnt;

    // JIT MUST be declared before all other component objects,
    // as they'll need the memory that it allocates in its constructor!
    // (Reminder: C++ fields are initialized in the order they're declared,
    // regardless of what the constructor's initializer list says.)
    melonDS::ARMJIT JIT;
    ARMv5 ARM9;
    ARMv4 ARM7;
    melonDS::SPU SPU;
    SPIHost SPI;
    melonDS::RTC RTC;
    melonDS::Wifi Wifi;
    NDSCart::NDSCartSlot NDSCartSlot;
    GBACart::GBACartSlot GBACartSlot;
    melonDS::GPU GPU;
    melonDS::AREngine AREngine;

    const u32 ARM7WRAMSize = 0x10000;
    u8* ARM7WRAM;

    virtual void Reset();
    void Start();

    /// Stop the emulator.
    virtual void Stop(Platform::StopReason reason = Platform::StopReason::External);

    bool DoSavestate(Savestate* file);

    void SetARM9RegionTimings(u32 addrstart, u32 addrend, u32 region, int buswidth, int nonseq, int seq);
    void SetARM7RegionTimings(u32 addrstart, u32 addrend, u32 region, int buswidth, int nonseq, int seq);

    void LoadBIOS();

    /// @return \c true if the loaded ARM9 BIOS image is a known dump
    /// of a native DS-compatible ARM9 BIOS.
    [[nodiscard]] bool IsLoadedARM9BIOSKnownNative() const noexcept { return ARM9BIOSNative; }
    [[nodiscard]] const std::array<u8, ARM9BIOSSize>& GetARM9BIOS() const noexcept { return ARM9BIOS; }
    void SetARM9BIOS(const std::array<u8, ARM9BIOSSize>& bios) noexcept;

    [[nodiscard]] const std::array<u8, ARM7BIOSSize>& GetARM7BIOS() const noexcept { return ARM7BIOS; }
    void SetARM7BIOS(const std::array<u8, ARM7BIOSSize>& bios) noexcept;

    /// @return \c true if the loaded ARM7 BIOS image is a known dump
    /// of a native DS-compatible ARM9 BIOS.
    [[nodiscard]] bool IsLoadedARM7BIOSKnownNative() const noexcept { return ARM7BIOSNative; }

    [[nodiscard]] NDSCart::CartCommon* GetNDSCart() { return NDSCartSlot.GetCart(); }
    [[nodiscard]] const NDSCart::CartCommon* GetNDSCart() const { return NDSCartSlot.GetCart(); }
    virtual void SetNDSCart(std::unique_ptr<NDSCart::CartCommon>&& cart);
    [[nodiscard]] bool CartInserted() const noexcept { return NDSCartSlot.GetCart() != nullptr; }
    virtual std::unique_ptr<NDSCart::CartCommon> EjectCart() { return NDSCartSlot.EjectCart(); }

    [[nodiscard]] u8* GetNDSSave() { return NDSCartSlot.GetSaveMemory(); }
    [[nodiscard]] const u8* GetNDSSave() const { return NDSCartSlot.GetSaveMemory(); }
    [[nodiscard]] u32 GetNDSSaveLength() const { return NDSCartSlot.GetSaveMemoryLength(); }
    void SetNDSSave(const u8* savedata, u32 savelen);

    const Firmware& GetFirmware() const { return SPI.GetFirmwareMem()->GetFirmware(); }
    Firmware& GetFirmware() { return SPI.GetFirmwareMem()->GetFirmware(); }
    void SetFirmware(Firmware&& firmware) { SPI.GetFirmwareMem()->SetFirmware(std::move(firmware)); }

    const Renderer3D& GetRenderer3D() const noexcept { return GPU.GetRenderer3D(); }
    Renderer3D& GetRenderer3D() noexcept { return GPU.GetRenderer3D(); }
    void SetRenderer3D(std::unique_ptr<Renderer3D>&& renderer) noexcept
    {
        if (renderer != nullptr)
            GPU.SetRenderer3D(std::move(renderer));
    }

    virtual bool NeedsDirectBoot() const;
    void SetupDirectBoot(const std::string& romname);
    virtual void SetupDirectBoot();

    [[nodiscard]] GBACart::CartCommon* GetGBACart() { return (ConsoleType == 1) ? nullptr : GBACartSlot.GetCart(); }
    [[nodiscard]] const GBACart::CartCommon* GetGBACart() const {  return (ConsoleType == 1) ? nullptr : GBACartSlot.GetCart(); }

    /// Inserts a GBA cart into the emulated console's Slot-2.
    ///
    /// @param cart The GBA cart, most likely (but not necessarily) returned from GBACart::ParseROM.
    /// To insert an accessory that doesn't use a ROM image
    /// (e.g. the Expansion Pak), create it manually and pass it here.
    /// If \c nullptr, the existing cart is ejected.
    /// If this is a DSi, this method does nothing.
    ///
    /// @post \c cart is \c nullptr and this NDS takes ownership
    /// of the cart object it held, if any.
    void SetGBACart(std::unique_ptr<GBACart::CartCommon>&& cart) { if (ConsoleType == 0) GBACartSlot.SetCart(std::move(cart)); }

    u8* GetGBASave() { return GBACartSlot.GetSaveMemory(); }
    const u8* GetGBASave() const { return GBACartSlot.GetSaveMemory(); }
    u32 GetGBASaveLength() const { return GBACartSlot.GetSaveMemoryLength(); }
    void SetGBASave(const u8* savedata, u32 savelen);

    void LoadGBAAddon(int type);
    std::unique_ptr<GBACart::CartCommon> EjectGBACart() { return GBACartSlot.EjectCart(); }

    u32 RunFrame();

    bool IsRunning() const noexcept { return Running; }

    void TouchScreen(u16 x, u16 y);
    void ReleaseScreen();

    void SetKeyMask(u32 mask);

    bool IsLidClosed() const;
    void SetLidClosed(bool closed);

    virtual void CamInputFrame(int cam, const u32* data, int width, int height, bool rgb) {}
    void MicInputFrame(s16* data, int samples);

    void RegisterEventFunc(u32 id, u32 funcid, EventFunc func);
    void UnregisterEventFunc(u32 id, u32 funcid);
    void ScheduleEvent(u32 id, bool periodic, s32 delay, u32 funcid, u32 param);
    void CancelEvent(u32 id);

    void debug(u32 p);

    void Halt();

    void MapSharedWRAM(u8 val);

    void UpdateIRQ(u32 cpu);
    void SetIRQ(u32 cpu, u32 irq);
    void ClearIRQ(u32 cpu, u32 irq);
    void SetIRQ2(u32 irq);
    void ClearIRQ2(u32 irq);
    bool HaltInterrupted(u32 cpu) const;
    void StopCPU(u32 cpu, u32 mask);
    void ResumeCPU(u32 cpu, u32 mask);
    void GXFIFOStall();
    void GXFIFOUnstall();

    u32 GetPC(u32 cpu) const;
    u64 GetSysClockCycles(int num);
    void NocashPrint(u32 cpu, u32 addr);

    void MonitorARM9Jump(u32 addr);

    virtual bool DMAsInMode(u32 cpu, u32 mode) const;
    virtual bool DMAsRunning(u32 cpu) const;
    virtual void CheckDMAs(u32 cpu, u32 mode);
    virtual void StopDMAs(u32 cpu, u32 mode);

    void RunTimers(u32 cpu);

    virtual u8 ARM9Read8(u32 addr);
    virtual u16 ARM9Read16(u32 addr);
    virtual u32 ARM9Read32(u32 addr);
    virtual void ARM9Write8(u32 addr, u8 val);
    virtual void ARM9Write16(u32 addr, u16 val);
    virtual void ARM9Write32(u32 addr, u32 val);

    virtual bool ARM9GetMemRegion(u32 addr, bool write, MemRegion* region);

    virtual u8 ARM7Read8(u32 addr);
    virtual u16 ARM7Read16(u32 addr);
    virtual u32 ARM7Read32(u32 addr);
    virtual void ARM7Write8(u32 addr, u8 val);
    virtual void ARM7Write16(u32 addr, u16 val);
    virtual void ARM7Write32(u32 addr, u32 val);

    virtual bool ARM7GetMemRegion(u32 addr, bool write, MemRegion* region);

    virtual u8 ARM9IORead8(u32 addr);
    virtual u16 ARM9IORead16(u32 addr);
    virtual u32 ARM9IORead32(u32 addr);
    virtual void ARM9IOWrite8(u32 addr, u8 val);
    virtual void ARM9IOWrite16(u32 addr, u16 val);
    virtual void ARM9IOWrite32(u32 addr, u32 val);

    virtual u8 ARM7IORead8(u32 addr);
    virtual u16 ARM7IORead16(u32 addr);
    virtual u32 ARM7IORead32(u32 addr);
    virtual void ARM7IOWrite8(u32 addr, u8 val);
    virtual void ARM7IOWrite16(u32 addr, u16 val);
    virtual void ARM7IOWrite32(u32 addr, u32 val);

#ifdef JIT_ENABLED
    [[nodiscard]] bool IsJITEnabled() const noexcept { return EnableJIT; }
    void SetJITArgs(std::optional<JITArgs> args) noexcept;
#else
    [[nodiscard]] bool IsJITEnabled() const noexcept { return false; }
    void SetJITArgs(std::optional<JITArgs> args) noexcept {}
#endif

private:
    void InitTimings();
    u32 SchedListMask;
    u64 SysTimestamp;
    u8 WRAMCnt;
    u8 PostFlag9;
    u8 PostFlag7;
    u16 PowerControl7;
    u16 WifiWaitCnt;
    u8 TimerCheckMask[2];
    u64 TimerTimestamp[2];
    DMA DMAs[8];
    u32 DMA9Fill[4];
    u16 IPCSync9, IPCSync7;
    u16 IPCFIFOCnt9, IPCFIFOCnt7;
    FIFO<u32, 16> IPCFIFO9; // FIFO in which the ARM9 writes
    FIFO<u32, 16> IPCFIFO7;
    u16 DivCnt;
    alignas(u64) u32 DivNumerator[2];
    alignas(u64) u32 DivDenominator[2];
    alignas(u64) u32 DivQuotient[2];
    alignas(u64) u32 DivRemainder[2];
    u16 SqrtCnt;
    alignas(u64) u32 SqrtVal[2];
    u32 SqrtRes;
    u16 KeyCnt[2];
    bool Running;
    bool RunningGame;
    u64 LastSysClockCycles;
    u64 FrameStartTimestamp;
    u64 NextTarget();
    u64 NextTargetSleep();
    void CheckKeyIRQ(u32 cpu, u32 oldkey, u32 newkey);
    void Reschedule(u64 target);
    void RunSystemSleep(u64 timestamp);
    void RunSystem(u64 timestamp);
    void HandleTimerOverflow(u32 tid);
    u16 TimerGetCounter(u32 timer);
    void TimerStart(u32 id, u16 cnt);
    void StartDiv();
    void DivDone(u32 param);
    void SqrtDone(u32 param);
    void StartSqrt();
    void RunTimer(u32 tid, s32 cycles);
    void UpdateWifiTimings();
    void SetWifiWaitCnt(u16 val);
    void SetGBASlotTimings();
    void EnterSleepMode();
    template <bool EnableJIT>
    u32 RunFrame();
public:
    NDS(NDSArgs&& args) noexcept : NDS(std::move(args), 0) {}
    NDS() noexcept;
    virtual ~NDS() noexcept;
    NDS(const NDS&) = delete;
    NDS& operator=(const NDS&) = delete;
    NDS(NDS&&) = delete;
    NDS& operator=(NDS&&) = delete;
    // The frontend should set and unset this manually after creating and destroying the NDS object.
    [[deprecated("Temporary workaround until JIT code generation is revised to accommodate multiple NDS objects.")]] static NDS* Current;
protected:
    explicit NDS(NDSArgs&& args, int type) noexcept;
    virtual void DoSavestateExtra(Savestate* file) {}
};

}
#endif // NDS_H
