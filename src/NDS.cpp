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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "NDS.h"
#include "ARM.h"
#include "NDSCart.h"
#include "GBACart.h"
#include "DMA.h"
#include "FIFO.h"
#include "GPU.h"
#include "SPU.h"
#include "SPI.h"
#include "RTC.h"
#include "Wifi.h"
#include "AREngine.h"
#include "Platform.h"
#include "FreeBIOS.h"
#include "Args.h"
#include "version.h"

#include "DSi.h"
#include "DSi_SPI_TSC.h"
#include "DSi_NWifi.h"
#include "DSi_Camera.h"
#include "DSi_DSP.h"
#include "ARMJIT.h"
#include "ARMJIT_Memory.h"

namespace melonDS
{
using namespace Platform;

const s32 kMaxIterationCycles = 64;
const s32 kIterationCycleMargin = 8;

// timing notes
//
// * this implementation is technically wrong for VRAM
//   each bank is considered a separate region
//   but this would only matter in specific VRAM->VRAM DMA transfers or
//   when running code in VRAM, which is way unlikely
//
// bus/basedelay/nspenalty
//
// bus types:
// * 0 / 32-bit: nothing special
// * 1 / 16-bit: 32-bit accesses split into two 16-bit accesses, second is always sequential
// * 2 / 8-bit/GBARAM: (presumably) split into multiple 8-bit accesses?
// * 3 / ARM9 internal: cache/TCM
//
// ARM9 always gets 3c nonseq penalty when using the bus (except for mainRAM where the penalty is 7c)
// /!\ 3c penalty doesn't apply to DMA!
//
// ARM7 only gets nonseq penalty when accessing mainRAM (7c as for ARM9)
//
// timings for GBA slot and wifi are set up at runtime

NDS* NDS::Current = nullptr;

NDS::NDS() noexcept :
    NDS(
        NDSArgs {
            nullptr,
            nullptr,
            std::make_unique<ARM9BIOSImage>(bios_arm9_bin),
            std::make_unique<ARM7BIOSImage>(bios_arm7_bin),
            Firmware(0),
        }
    )
{
}

NDS::NDS(NDSArgs&& args, int type) noexcept :
    ConsoleType(type),
    ARM7BIOS(*args.ARM7BIOS),
    ARM9BIOS(*args.ARM9BIOS),
    ARM7BIOSNative(CRC32(ARM7BIOS.data(), ARM7BIOS.size()) == ARM7BIOSCRC32),
    ARM9BIOSNative(CRC32(ARM9BIOS.data(), ARM9BIOS.size()) == ARM9BIOSCRC32),
    JIT(*this, args.JIT),
    SPU(*this, args.BitDepth, args.Interpolation),
    GPU(*this, std::move(args.Renderer3D)),
    SPI(*this, std::move(args.Firmware)),
    RTC(*this),
    Wifi(*this),
    NDSCartSlot(*this, std::move(args.NDSROM)),
    GBACartSlot(type == 1 ? nullptr : std::move(args.GBAROM)),
    AREngine(*this),
    ARM9(*this, args.GDB, args.JIT.has_value()),
    ARM7(*this, args.GDB, args.JIT.has_value()),
#ifdef JIT_ENABLED
    EnableJIT(args.JIT.has_value()),
#endif
    DMAs {
        DMA(0, 0, *this),
        DMA(0, 1, *this),
        DMA(0, 2, *this),
        DMA(0, 3, *this),
        DMA(1, 0, *this),
        DMA(1, 1, *this),
        DMA(1, 2, *this),
        DMA(1, 3, *this),
    }
{
    RegisterEventFunc(Event_Div, 0, MemberEventFunc(NDS, DivDone));
    RegisterEventFunc(Event_Sqrt, 0, MemberEventFunc(NDS, SqrtDone));

    MainRAM = JIT.Memory.GetMainRAM();
    SharedWRAM = JIT.Memory.GetSharedWRAM();
    ARM7WRAM = JIT.Memory.GetARM7WRAM();
}

NDS::~NDS() noexcept
{
    UnregisterEventFunc(Event_Div, 0);
    UnregisterEventFunc(Event_Sqrt, 0);
    // The destructor for each component is automatically called by the compiler
}


void NDS::SetARM9RegionTimings(u32 addrstart, u32 addrend, u32 region, int buswidth, int nonseq, int seq)
{
    addrstart >>= 2;
    addrend   >>= 2;

    int N16, S16, N32, S32, cpuN;
    N16 = nonseq;
    S16 = seq;
    if (buswidth == 16)
    {
        N32 = N16 + S16;
        S32 = S16 + S16;
    }
    else
    {
        N32 = N16;
        S32 = S16;
    }

    // nonseq accesses on the CPU get a 3-cycle penalty for all regions except main RAM
    cpuN = (region == Mem9_MainRAM) ? 0 : 3;

    for (u32 i = addrstart; i < addrend; i++)
    {
        // CPU timings
        ARM9MemTimings[i][0] = N16 + cpuN;
        ARM9MemTimings[i][1] = S16;
        ARM9MemTimings[i][2] = N32 + cpuN;
        ARM9MemTimings[i][3] = S32;

        // DMA timings
        ARM9MemTimings[i][4] = N16;
        ARM9MemTimings[i][5] = S16;
        ARM9MemTimings[i][6] = N32;
        ARM9MemTimings[i][7] = S32;

        ARM9Regions[i] = region;
    }

    ARM9.UpdateRegionTimings(addrstart<<2, addrend<<2);
}

void NDS::SetARM7RegionTimings(u32 addrstart, u32 addrend, u32 region, int buswidth, int nonseq, int seq)
{
    addrstart >>= 3;
    addrend   >>= 3;

    int N16, S16, N32, S32;
    N16 = nonseq;
    S16 = seq;
    if (buswidth == 16)
    {
        N32 = N16 + S16;
        S32 = S16 + S16;
    }
    else
    {
        N32 = N16;
        S32 = S16;
    }

    for (u32 i = addrstart; i < addrend; i++)
    {
        // CPU and DMA timings are the same
        ARM7MemTimings[i][0] = N16;
        ARM7MemTimings[i][1] = S16;
        ARM7MemTimings[i][2] = N32;
        ARM7MemTimings[i][3] = S32;

        ARM7Regions[i] = region;
    }
}

#ifdef JIT_ENABLED
void NDS::SetJITArgs(std::optional<JITArgs> args) noexcept
{
    if (args)
    { // If we want to turn the JIT on...
        JIT.SetJITArgs(*args);
    }
    else if (args.has_value() != EnableJIT)
    { // Else if we want to turn the JIT off, and it wasn't already off...
        JIT.ResetBlockCache();
    }

    EnableJIT = args.has_value();
}
#endif

void NDS::InitTimings()
{
    // TODO, eventually:
    // VRAM is initially unmapped. The timings should be those of void regions.
    // Similarly for any unmapped VRAM area.
    // Need to check whether supporting these timing characteristics would impact performance
    // (especially wrt VRAM mirroring and overlapping and whatnot).
    // Also, each VRAM bank is its own memory region. This would matter when DMAing from a VRAM
    // bank to another (if this is a thing) for example.

    // TODO: check in detail how WRAM works, although it seems to be one region.

    // TODO: DSi-specific timings!!

    SetARM9RegionTimings(0x00000, 0x100000, 0, 32, 1, 1); // void

    SetARM9RegionTimings(0xFFFF0, 0x100000, Mem9_BIOS,    32, 1, 1); // BIOS
    SetARM9RegionTimings(0x02000, 0x03000,  Mem9_MainRAM, 16, 8, 1);     // main RAM
    SetARM9RegionTimings(0x03000, 0x04000,  Mem9_WRAM,    32, 1, 1); // ARM9/shared WRAM
    SetARM9RegionTimings(0x04000, 0x05000,  Mem9_IO,      32, 1, 1); // IO
    SetARM9RegionTimings(0x05000, 0x06000,  Mem9_Pal,     16, 1, 1); // palette
    SetARM9RegionTimings(0x06000, 0x07000,  Mem9_VRAM,    16, 1, 1); // VRAM
    SetARM9RegionTimings(0x07000, 0x08000,  Mem9_OAM,     32, 1, 1); // OAM

    // ARM7

    SetARM7RegionTimings(0x00000, 0x100000, 0, 32, 1, 1); // void

    SetARM7RegionTimings(0x00000, 0x00010, Mem7_BIOS,    32, 1, 1); // BIOS
    SetARM7RegionTimings(0x02000, 0x03000, Mem7_MainRAM, 16, 8, 1); // main RAM
    SetARM7RegionTimings(0x03000, 0x04000, Mem7_WRAM,    32, 1, 1); // ARM7/shared WRAM
    SetARM7RegionTimings(0x04000, 0x04800, Mem7_IO,      32, 1, 1); // IO
    SetARM7RegionTimings(0x06000, 0x07000, Mem7_VRAM,    16, 1, 1); // ARM7 VRAM

    // handled later: GBA slot, wifi
}

bool NDS::NeedsDirectBoot() const
{
    if (ConsoleType == 1)
    {
        // for now, DSi mode requires original BIOS/NAND
        return false;
    }
    else
    {
        // DSi/3DS firmwares aren't bootable, neither is the generated firmware
        if (!SPI.GetFirmware().IsBootable())
            return true;

        // FreeBIOS requires direct boot (it can't boot firmware)
        if (!IsLoadedARM9BIOSKnownNative() || !IsLoadedARM7BIOSKnownNative())
            return true;

        return false;
    }
}

void NDS::SetupDirectBoot()
{
    const NDSHeader& header = NDSCartSlot.GetCart()->GetHeader();
    u32 cartid = NDSCartSlot.GetCart()->ID();
    const u8* cartrom = NDSCartSlot.GetCart()->GetROM();
    MapSharedWRAM(3);

    // Copy the Nintendo logo from the NDS ROM header to the ARM9 BIOS if using FreeBIOS
    // Games need this for DS<->GBA comm to work
    if (!IsLoadedARM9BIOSKnownNative())
    {
        memcpy(ARM9BIOS.data() + 0x20, header.NintendoLogo, 0x9C);
    }

    // setup main RAM data

    for (u32 i = 0; i < 0x170; i+=4)
    {
        u32 tmp = *(u32*)&cartrom[i];
        NDS::ARM9Write32(0x027FFE00+i, tmp);
    }

    NDS::ARM9Write32(0x027FF800, cartid);
    NDS::ARM9Write32(0x027FF804, cartid);
    NDS::ARM9Write16(0x027FF808, header.HeaderCRC16);
    NDS::ARM9Write16(0x027FF80A, header.SecureAreaCRC16);

    NDS::ARM9Write16(0x027FF850, 0x5835);

    NDS::ARM9Write32(0x027FFC00, cartid);
    NDS::ARM9Write32(0x027FFC04, cartid);
    NDS::ARM9Write16(0x027FFC08, header.HeaderCRC16);
    NDS::ARM9Write16(0x027FFC0A, header.SecureAreaCRC16);

    NDS::ARM9Write16(0x027FFC10, 0x5835);
    NDS::ARM9Write16(0x027FFC30, 0xFFFF);
    NDS::ARM9Write16(0x027FFC40, 0x0001);

    u32 arm9start = 0;

    // load the ARM9 secure area
    if (header.ARM9ROMOffset >= 0x4000 && header.ARM9ROMOffset < 0x8000)
    {
        u8 securearea[0x800];
        NDSCartSlot.DecryptSecureArea(securearea);

        for (u32 i = 0; i < 0x800; i+=4)
        {
            NDS::ARM9Write32(header.ARM9RAMAddress+i, *(u32*)&securearea[i]);
            arm9start += 4;
        }
    }

    // CHECKME: firmware seems to load this in 0x200 byte chunks

    for (u32 i = arm9start; i < header.ARM9Size; i+=4)
    {
        u32 tmp = *(u32*)&cartrom[header.ARM9ROMOffset+i];
        NDS::ARM9Write32(header.ARM9RAMAddress+i, tmp);
    }

    for (u32 i = 0; i < header.ARM7Size; i+=4)
    {
        u32 tmp = *(u32*)&cartrom[header.ARM7ROMOffset+i];
        NDS::ARM7Write32(header.ARM7RAMAddress+i, tmp);
    }

    ARM7BIOSProt = 0x1204;

    SPI.GetFirmwareMem()->SetupDirectBoot();

    ARM9.CP15Write(0x100, 0x00012078);
    ARM9.CP15Write(0x200, 0x00000042);
    ARM9.CP15Write(0x201, 0x00000042);
    ARM9.CP15Write(0x300, 0x00000002);
    ARM9.CP15Write(0x502, 0x15111011);
    ARM9.CP15Write(0x503, 0x05100011);
    ARM9.CP15Write(0x600, 0x04000033);
    ARM9.CP15Write(0x601, 0x04000033);
    ARM9.CP15Write(0x610, 0x0200002B);
    ARM9.CP15Write(0x611, 0x0200002B);
    ARM9.CP15Write(0x620, 0x00000000);
    ARM9.CP15Write(0x621, 0x00000000);
    ARM9.CP15Write(0x630, 0x08000035);
    ARM9.CP15Write(0x631, 0x08000035);
    ARM9.CP15Write(0x640, 0x0300001B);
    ARM9.CP15Write(0x641, 0x0300001B);
    ARM9.CP15Write(0x650, 0x00000000);
    ARM9.CP15Write(0x651, 0x00000000);
    ARM9.CP15Write(0x660, 0xFFFF001D);
    ARM9.CP15Write(0x661, 0xFFFF001D);
    ARM9.CP15Write(0x670, 0x027FF017);
    ARM9.CP15Write(0x671, 0x027FF017);
    ARM9.CP15Write(0x910, 0x0300000A);
    ARM9.CP15Write(0x911, 0x00000020);
}

void NDS::SetupDirectBoot(const std::string& romname)
{
    const NDSHeader& header = NDSCartSlot.GetCart()->GetHeader();
    SetupDirectBoot();

    NDSCartSlot.SetupDirectBoot(romname);

    ARM9.R[12] = header.ARM9EntryAddress;
    ARM9.R[13] = 0x03002F7C;
    ARM9.R[14] = header.ARM9EntryAddress;
    ARM9.R_IRQ[0] = 0x03003F80;
    ARM9.R_SVC[0] = 0x03003FC0;

    ARM7.R[12] = header.ARM7EntryAddress;
    ARM7.R[13] = 0x0380FD80;
    ARM7.R[14] = header.ARM7EntryAddress;
    ARM7.R_IRQ[0] = 0x0380FF80;
    ARM7.R_SVC[0] = 0x0380FFC0;

    ARM9.JumpTo(header.ARM9EntryAddress);
    ARM7.JumpTo(header.ARM7EntryAddress);

    PostFlag9 = 0x01;
    PostFlag7 = 0x01;

    PowerControl9 = 0x820F;
    GPU.SetPowerCnt(PowerControl9);

    // checkme
    RCnt = 0x8000;

    NDSCartSlot.SetSPICnt(0x8000);

    SPU.SetBias(0x200);

    SetWifiWaitCnt(0x0030);
}

void NDS::Reset()
{
    Platform::FileHandle* f;
    u32 i;

    RunningGame = false;
    LastSysClockCycles = 0;

    // BIOS files are now loaded by the frontend

    JIT.Reset();

    if (ConsoleType == 1)
    {
        // BIOS files are now loaded by the frontend

        ARM9ClockShift = 2;
        MainRAMMask = 0xFFFFFF;
    }
    else
    {
        ARM9ClockShift = 1;
        MainRAMMask = 0x3FFFFF;
    }
    // has to be called before InitTimings
    // otherwise some PU settings are completely
    // unitialised on the first run
    ARM9.CP15Reset();

    ARM9Timestamp = 0; ARM9Target = 0;
    ARM7Timestamp = 0; ARM7Target = 0;
    SysTimestamp = 0;

    InitTimings();

    memset(MainRAM, 0, MainRAMMask + 1);
    memset(SharedWRAM, 0, 0x8000);
    memset(ARM7WRAM, 0, 0x10000);

    MapSharedWRAM(0);

    ExMemCnt[0] = 0x4000;
    ExMemCnt[1] = 0x4000;
    memset(ROMSeed0, 0, 2*8);
    memset(ROMSeed1, 0, 2*8);
    SetGBASlotTimings();

    IME[0] = 0;
    IE[0] = 0;
    IF[0] = 0;
    IME[1] = 0;
    IE[1] = 0;
    IF[1] = 0;
    IE2 = 0;
    IF2 = 0;

    PostFlag9 = 0x00;
    PostFlag7 = 0x00;
    PowerControl9 = 0x0001;
    PowerControl7 = 0x0001;

    WifiWaitCnt = 0xFFFF; // temp
    SetWifiWaitCnt(0);

    ARM7BIOSProt = 0;

    IPCSync9 = 0;
    IPCSync7 = 0;
    IPCFIFOCnt9 = 0;
    IPCFIFOCnt7 = 0;
    IPCFIFO9.Clear();
    IPCFIFO7.Clear();

    DivCnt = 0;
    SqrtCnt = 0;

    ARM9.Reset();
    ARM7.Reset();

    CPUStop = 0;

    memset(Timers, 0, 8*sizeof(Timer));
    TimerCheckMask[0] = 0;
    TimerCheckMask[1] = 0;
    TimerTimestamp[0] = 0;
    TimerTimestamp[1] = 0;

    for (i = 0; i < 8; i++) DMAs[i].Reset();
    memset(DMA9Fill, 0, 4*4);

    for (i = 0; i < Event_MAX; i++)
    {
        SchedEvent& evt = SchedList[i];

        evt.Timestamp = 0;
        evt.FuncID = 0;
        evt.Param = 0;
    }
    SchedListMask = 0;

    KeyInput = 0x007F03FF;
    KeyCnt[0] = 0;
    KeyCnt[1] = 0;
    RCnt = 0;

    GPU.Reset();
    NDSCartSlot.Reset();
    GBACartSlot.Reset();
    SPU.Reset();
    SPI.Reset();
    RTC.Reset();
    Wifi.Reset();
}

void NDS::Start()
{
    Running = true;
}

static const char* StopReasonName(Platform::StopReason reason)
{
    switch (reason)
    {
        case Platform::StopReason::External:
            return "External";
        case Platform::StopReason::PowerOff:
            return "PowerOff";
        case Platform::StopReason::GBAModeNotSupported:
            return "GBAModeNotSupported";
        case Platform::StopReason::BadExceptionRegion:
            return "BadExceptionRegion";
        default:
            return "Unknown";
    }
}

void NDS::Stop(Platform::StopReason reason)
{
    Platform::LogLevel level;
    switch (reason)
    {
        case Platform::StopReason::External:
        case Platform::StopReason::PowerOff:
            level = LogLevel::Info;
            break;
        case Platform::StopReason::GBAModeNotSupported:
        case Platform::StopReason::BadExceptionRegion:
            level = LogLevel::Error;
            break;
        default:
            level = LogLevel::Warn;
            break;
    }

    Log(level, "Stopping emulated console (Reason: %s)\n", StopReasonName(reason));
    Running = false;
    Platform::SignalStop(reason);
    GPU.Stop();
    SPU.Stop();
}

bool NDS::DoSavestate(Savestate* file)
{
    file->Section("NDSG");

    if (file->Saving)
    {
        u32 console = ConsoleType;
        file->Var32(&console);
    }
    else
    {
        u32 console;
        file->Var32(&console);
        if (console != ConsoleType)
        {
            Log(LogLevel::Error, "savestate: Expected console type %d, got console type %d. cannot load.\n", ConsoleType, console);
            return false;
        }
    }

    file->VarArray(MainRAM, MainRAMMaxSize);
    file->VarArray(SharedWRAM, SharedWRAMSize);
    file->VarArray(ARM7WRAM, ARM7WRAMSize);

    //file->VarArray(ARM9BIOS, 0x1000);
    //file->VarArray(ARM7BIOS, 0x4000);

    file->VarArray(ExMemCnt, 2*sizeof(u16));
    file->VarArray(ROMSeed0, 2*8);
    file->VarArray(ROMSeed1, 2*8);

    file->Var16(&WifiWaitCnt);

    file->VarArray(IME, 2*sizeof(u32));
    file->VarArray(IE, 2*sizeof(u32));
    file->VarArray(IF, 2*sizeof(u32));
    file->Var32(&IE2);
    file->Var32(&IF2);

    file->Var8(&PostFlag9);
    file->Var8(&PostFlag7);
    file->Var16(&PowerControl9);
    file->Var16(&PowerControl7);

    file->Var16(&ARM7BIOSProt);

    file->Var16(&IPCSync9);
    file->Var16(&IPCSync7);
    file->Var16(&IPCFIFOCnt9);
    file->Var16(&IPCFIFOCnt7);
    IPCFIFO9.DoSavestate(file);
    IPCFIFO7.DoSavestate(file);

    file->Var16(&DivCnt);
    file->Var16(&SqrtCnt);

    file->Var32(&CPUStop);

    for (int i = 0; i < 8; i++)
    {
        Timer* timer = &Timers[i];

        file->Var16(&timer->Reload);
        file->Var16(&timer->Cnt);
        file->Var32(&timer->Counter);
        file->Var32(&timer->CycleShift);
    }
    file->VarArray(TimerCheckMask, 2*sizeof(u8));
    file->VarArray(TimerTimestamp, 2*sizeof(u64));

    file->VarArray(DMA9Fill, 4*sizeof(u32));

    for (int i = 0; i < Event_MAX; i++)
    {
        SchedEvent& evt = SchedList[i];

        file->Var64(&evt.Timestamp);
        file->Var32(&evt.FuncID);
        file->Var32(&evt.Param);
    }
    file->Var32(&SchedListMask);
    file->Var64(&ARM9Timestamp);
    file->Var64(&ARM9Target);
    file->Var64(&ARM7Timestamp);
    file->Var64(&ARM7Target);
    file->Var64(&SysTimestamp);
    file->Var64(&LastSysClockCycles);
    file->Var64(&FrameStartTimestamp);
    file->Var32(&NumFrames);
    file->Var32(&NumLagFrames);
    file->Bool32(&LagFrameFlag);

    // TODO: save KeyInput????
    file->VarArray(KeyCnt, 2*sizeof(u16));
    file->Var16(&RCnt);

    file->Var8(&WRAMCnt);

    file->Bool32(&RunningGame);

    if (!file->Saving)
    {
        // 'dept of redundancy dept'
        // but we do need to update the mappings
        MapSharedWRAM(WRAMCnt);

        InitTimings();
        SetGBASlotTimings();

        UpdateWifiTimings();
    }

    for (int i = 0; i < 8; i++)
        DMAs[i].DoSavestate(file);

    ARM9.DoSavestate(file);
    ARM7.DoSavestate(file);

    NDSCartSlot.DoSavestate(file);
    if (ConsoleType == 0)
        GBACartSlot.DoSavestate(file);
    GPU.DoSavestate(file);
    SPU.DoSavestate(file);
    SPI.DoSavestate(file);
    RTC.DoSavestate(file);
    Wifi.DoSavestate(file);

    DoSavestateExtra(file); // Handles DSi state if applicable

    if (!file->Saving)
    {
        GPU.SetPowerCnt(PowerControl9);

        SPU.SetPowerCnt(PowerControl7 & 0x0001);
        Wifi.SetPowerCnt(PowerControl7 & 0x0002);

#ifdef JIT_ENABLED
        JIT.Reset();
#endif
    }

    file->Finish();

    return true;
}

void NDS::SetNDSCart(std::unique_ptr<NDSCart::CartCommon>&& cart)
{
    NDSCartSlot.SetCart(std::move(cart));
    // The existing cart will always be ejected;
    // if cart is null, then that's equivalent to ejecting a cart
    // without inserting a new one.
}

void NDS::SetNDSSave(const u8* savedata, u32 savelen)
{
    if (savedata && savelen)
        NDSCartSlot.SetSaveMemory(savedata, savelen);
}

void NDS::SetGBASave(const u8* savedata, u32 savelen)
{
    if (ConsoleType == 0 && savedata && savelen)
    {
        GBACartSlot.SetSaveMemory(savedata, savelen);
    }

}

void NDS::LoadGBAAddon(int type)
{
    GBACartSlot.LoadAddon(type);
}

void NDS::LoadBIOS()
{
    Reset();
}

void NDS::SetARM7BIOS(const std::array<u8, ARM7BIOSSize>& bios) noexcept
{
    ARM7BIOS = bios;
    ARM7BIOSNative = CRC32(ARM7BIOS.data(), ARM7BIOS.size()) == ARM7BIOSCRC32;
}

void NDS::SetARM9BIOS(const std::array<u8, ARM9BIOSSize>& bios) noexcept
{
    ARM9BIOS = bios;
    ARM9BIOSNative = CRC32(ARM9BIOS.data(), ARM9BIOS.size()) == ARM9BIOSCRC32;
}

u64 NDS::NextTarget()
{
    u64 minEvent = UINT64_MAX;

    u32 mask = SchedListMask;
    for (int i = 0; i < Event_MAX; i++)
    {
        if (!mask) break;
        if (mask & 0x1)
        {
            if (SchedList[i].Timestamp < minEvent)
                minEvent = SchedList[i].Timestamp;
        }

        mask >>= 1;
    }

    u64 max = SysTimestamp + kMaxIterationCycles;

    if (minEvent < max + kIterationCycleMargin)
        return minEvent;

    return max;
}

void NDS::RunSystem(u64 timestamp)
{
    SysTimestamp = timestamp;

    u32 mask = SchedListMask;
    for (int i = 0; i < Event_MAX; i++)
    {
        if (!mask) break;
        if (mask & 0x1)
        {
            SchedEvent& evt = SchedList[i];

            if (evt.Timestamp <= SysTimestamp)
            {
                SchedListMask &= ~(1<<i);

                EventFunc func = evt.Funcs[evt.FuncID];
                func(evt.Param);
            }
        }

        mask >>= 1;
    }
}

u64 NDS::NextTargetSleep()
{
    u64 minEvent = UINT64_MAX;

    u32 mask = SchedListMask;
    for (int i = 0; i < Event_MAX; i++)
    {
        if (!mask) break;
        if (i == Event_SPU || i == Event_RTC)
        {
            if (mask & 0x1)
            {
                if (SchedList[i].Timestamp < minEvent)
                    minEvent = SchedList[i].Timestamp;
            }
        }

        mask >>= 1;
    }

    return minEvent;
}

void NDS::RunSystemSleep(u64 timestamp)
{
    u64 offset = timestamp - SysTimestamp;
    SysTimestamp = timestamp;

    u32 mask = SchedListMask;
    for (int i = 0; i < Event_MAX; i++)
    {
        if (!mask) break;
        if (i == Event_SPU || i == Event_RTC)
        {
            if (mask & 0x1)
            {
                SchedEvent& evt = SchedList[i];

                if (evt.Timestamp <= SysTimestamp)
                {
                    SchedListMask &= ~(1<<i);

                    u32 param;
                    if (i == Event_SPU)
                        param = 1;
                    else
                        param = evt.Param;

                    EventFunc func = evt.Funcs[evt.FuncID];
                    func(param);
                }
            }
        }
        else if (mask & 0x1)
        {
            if (SchedList[i].Timestamp <= SysTimestamp)
            {
                SchedList[i].Timestamp += offset;
            }
        }

        mask >>= 1;
    }
}

template <bool EnableJIT>
u32 NDS::RunFrame()
{
    FrameStartTimestamp = SysTimestamp;

    GPU.TotalScanlines = 0;

    LagFrameFlag = true;
    bool runFrame = Running && !(CPUStop & CPUStop_Sleep);
    while (Running)
    {
        u64 frametarget = SysTimestamp + 560190;

        if (CPUStop & CPUStop_Sleep)
        {
            // we are running in sleep mode
            // we still need to run the RTC during this mode
            // we also keep outputting audio, so that frontends using audio sync don't skyrocket to 1000+FPS

            while (Running && (SysTimestamp < frametarget))
            {
                u64 target = NextTargetSleep();
                if (target > frametarget)
                    target = frametarget;

                ARM9Timestamp = target << ARM9ClockShift;
                ARM7Timestamp = target;
                TimerTimestamp[0] = target;
                TimerTimestamp[1] = target;
                GPU.GPU3D.Timestamp = target;
                RunSystemSleep(target);

                if (!(CPUStop & CPUStop_Sleep))
                    break;
            }

            if (SysTimestamp >= frametarget)
                GPU.BlankFrame();
        }
        else
        {
            ARM9.CheckGdbIncoming();
            ARM7.CheckGdbIncoming();

            if (!(CPUStop & CPUStop_Wakeup))
            {
                GPU.StartFrame();
            }
            CPUStop &= ~CPUStop_Wakeup;

            while (Running && GPU.TotalScanlines==0)
            {
                u64 target = NextTarget();
                ARM9Target = target << ARM9ClockShift;
                CurCPU = 0;

                if (CPUStop & CPUStop_GXStall)
                {
                    // GXFIFO stall
                    s32 cycles = GPU.GPU3D.CyclesToRunFor();

                    ARM9Timestamp = std::min(ARM9Target, ARM9Timestamp+(cycles<<ARM9ClockShift));
                }
                else if (CPUStop & CPUStop_DMA9)
                {
                    DMAs[0].Run();
                    if (!(CPUStop & CPUStop_GXStall)) DMAs[1].Run();
                    if (!(CPUStop & CPUStop_GXStall)) DMAs[2].Run();
                    if (!(CPUStop & CPUStop_GXStall)) DMAs[3].Run();
                    if (ConsoleType == 1)
                    {
                        auto& dsi = dynamic_cast<melonDS::DSi&>(*this);
                        dsi.RunNDMAs(0);
                    }
                }
                else
                {
#ifdef JIT_ENABLED
                    if (EnableJIT)
                        ARM9.ExecuteJIT();
                    else
#endif
                        ARM9.Execute();
                }

                RunTimers(0);
                GPU.GPU3D.Run();

                target = ARM9Timestamp >> ARM9ClockShift;
                CurCPU = 1;

                while (ARM7Timestamp < target)
                {
                    ARM7Target = target; // might be changed by a reschedule

                    if (CPUStop & CPUStop_DMA7)
                    {
                        DMAs[4].Run();
                        DMAs[5].Run();
                        DMAs[6].Run();
                        DMAs[7].Run();
                        if (ConsoleType == 1)
                        {
                            auto& dsi = dynamic_cast<melonDS::DSi&>(*this);
                            dsi.RunNDMAs(1);
                        }
                    }
                    else
                    {
#ifdef JIT_ENABLED
                        if (EnableJIT)
                            ARM7.ExecuteJIT();
                        else
#endif
                            ARM7.Execute();
                    }

                    RunTimers(1);
                }

                RunSystem(target);

                if (CPUStop & CPUStop_Sleep)
                {
                    break;
                }
            }
        }

        if (GPU.TotalScanlines == 0)
            continue;

#ifdef DEBUG_CHECK_DESYNC
        Log(LogLevel::Debug, "[%08X%08X] ARM9=%ld, ARM7=%ld, GPU=%ld\n",
            (u32)(SysTimestamp>>32), (u32)SysTimestamp,
            (ARM9Timestamp>>1)-SysTimestamp,
            ARM7Timestamp-SysTimestamp,
            GPU.GPU3D.Timestamp-SysTimestamp);
#endif
        SPU.TransferOutput();
        break;
    }

    // In the context of TASes, frame count is traditionally the primary measure of emulated time,
    // so it needs to be tracked even if NDS is powered off.
    NumFrames++;
    if (LagFrameFlag)
        NumLagFrames++;

    if (Running)
        return GPU.TotalScanlines;
    else
        return 263;
}

u32 NDS::RunFrame()
{
#ifdef JIT_ENABLED
    if (EnableJIT)
        return RunFrame<true>();
    else
#endif
        return RunFrame<false>();
}

void NDS::Reschedule(u64 target)
{
    if (CurCPU == 0)
    {
        if (target < (ARM9Target >> ARM9ClockShift))
            ARM9Target = (target << ARM9ClockShift);
    }
    else
    {
        if (target < ARM7Target)
            ARM7Target = target;
    }
}

void NDS::RegisterEventFunc(u32 id, u32 funcid, EventFunc func)
{
    SchedEvent& evt = SchedList[id];

    evt.Funcs[funcid] = func;
}

void NDS::UnregisterEventFunc(u32 id, u32 funcid)
{
    SchedEvent& evt = SchedList[id];

    evt.Funcs.erase(funcid);
}

void NDS::ScheduleEvent(u32 id, bool periodic, s32 delay, u32 funcid, u32 param)
{
    if (SchedListMask & (1<<id))
    {
        Log(LogLevel::Debug, "!! EVENT %d ALREADY SCHEDULED\n", id);
        return;
    }

    SchedEvent& evt = SchedList[id];

    if (periodic)
        evt.Timestamp += delay;
    else
    {
        if (CurCPU == 0)
            evt.Timestamp = (ARM9Timestamp >> ARM9ClockShift) + delay;
        else
            evt.Timestamp = ARM7Timestamp + delay;
    }

    evt.FuncID = funcid;
    evt.Param = param;

    SchedListMask |= (1<<id);

    Reschedule(evt.Timestamp);
}

void NDS::CancelEvent(u32 id)
{
    SchedListMask &= ~(1<<id);
}


void NDS::TouchScreen(u16 x, u16 y)
{
    SPI.GetTSC()->SetTouchCoords(x, y);
}

void NDS::ReleaseScreen()
{
    SPI.GetTSC()->SetTouchCoords(0x000, 0xFFF);
}


void NDS::CheckKeyIRQ(u32 cpu, u32 oldkey, u32 newkey)
{
    u16 cnt = KeyCnt[cpu];
    if (!(cnt & (1<<14))) // IRQ disabled
        return;

    u32 mask = (cnt & 0x03FF);
    oldkey &= mask;
    newkey &= mask;

    bool oldmatch, newmatch;
    if (cnt & (1<<15))
    {
        // logical AND

        oldmatch = (oldkey == 0);
        newmatch = (newkey == 0);
    }
    else
    {
        // logical OR

        oldmatch = (oldkey != mask);
        newmatch = (newkey != mask);
    }

    if ((!oldmatch) && newmatch)
        SetIRQ(cpu, IRQ_Keypad);
}

void NDS::SetKeyMask(u32 mask)
{
    u32 key_lo = mask & 0x3FF;
    u32 key_hi = (mask >> 10) & 0x3;

    u32 oldkey = KeyInput;
    KeyInput &= 0xFFFCFC00;
    KeyInput |= key_lo | (key_hi << 16);

    CheckKeyIRQ(0, oldkey, KeyInput);
    CheckKeyIRQ(1, oldkey, KeyInput);
}

bool NDS::IsLidClosed() const
{
    if (KeyInput & (1<<23)) return true;
    return false;
}

void NDS::SetLidClosed(bool closed)
{
    if (closed)
    {
        KeyInput |= (1<<23);
    }
    else
    {
        KeyInput &= ~(1<<23);
        SetIRQ(1, IRQ_LidOpen);
    }
}

void NDS::MicInputFrame(s16* data, int samples)
{
    return SPI.GetTSC()->MicInputFrame(data, samples);
}

/*int ImportSRAM(u8* data, u32 length)
{
    return NDSCart::ImportSRAM(data, length);
}*/


void NDS::Halt()
{
    Log(LogLevel::Info, "Halt()\n");
    Running = false;
}


void NDS::MapSharedWRAM(u8 val)
{
    if (val == WRAMCnt)
        return;

    JIT.Memory.RemapSWRAM();

    WRAMCnt = val;

    switch (WRAMCnt & 0x3)
    {
    case 0:
        SWRAM_ARM9.Mem = &SharedWRAM[0];
        SWRAM_ARM9.Mask = 0x7FFF;
        SWRAM_ARM7.Mem = NULL;
        SWRAM_ARM7.Mask = 0;
        break;

    case 1:
        SWRAM_ARM9.Mem = &SharedWRAM[0x4000];
        SWRAM_ARM9.Mask = 0x3FFF;
        SWRAM_ARM7.Mem = &SharedWRAM[0];
        SWRAM_ARM7.Mask = 0x3FFF;
        break;

    case 2:
        SWRAM_ARM9.Mem = &SharedWRAM[0];
        SWRAM_ARM9.Mask = 0x3FFF;
        SWRAM_ARM7.Mem = &SharedWRAM[0x4000];
        SWRAM_ARM7.Mask = 0x3FFF;
        break;

    case 3:
        SWRAM_ARM9.Mem = NULL;
        SWRAM_ARM9.Mask = 0;
        SWRAM_ARM7.Mem = &SharedWRAM[0];
        SWRAM_ARM7.Mask = 0x7FFF;
        break;
    }
}


void NDS::UpdateWifiTimings()
{
    if (PowerControl7 & 0x0002)
    {
        const int ntimings[4] = {10, 8, 6, 18};
        u16 val = WifiWaitCnt;

        SetARM7RegionTimings(0x04800, 0x04808, Mem7_Wifi0, 16, ntimings[val & 0x3], (val & 0x4) ? 4 : 6);
        SetARM7RegionTimings(0x04808, 0x04810, Mem7_Wifi1, 16, ntimings[(val>>3) & 0x3], (val & 0x20) ? 4 : 10);
    }
    else
    {
        SetARM7RegionTimings(0x04800, 0x04808, Mem7_Wifi0, 32, 1, 1);
        SetARM7RegionTimings(0x04808, 0x04810, Mem7_Wifi1, 32, 1, 1);
    }
}

void NDS::SetWifiWaitCnt(u16 val)
{
    if (WifiWaitCnt == val) return;

    WifiWaitCnt = val;
    UpdateWifiTimings();
}

void NDS::SetGBASlotTimings()
{
    const int ntimings[4] = {10, 8, 6, 18};
    const u16 openbus[4] = {0xFE08, 0x0000, 0x0000, 0xFFFF};

    u16 curcpu = (ExMemCnt[0] >> 7) & 0x1;
    u16 curcnt = ExMemCnt[curcpu];
    int ramN = ntimings[curcnt & 0x3];
    int romN = ntimings[(curcnt>>2) & 0x3];
    int romS = (curcnt & 0x10) ? 4 : 6;

    // GBA slot timings only apply on the selected side

    if (curcpu == 0)
    {
        SetARM9RegionTimings(0x08000, 0x0A000, Mem9_GBAROM, 16, romN, romS);
        SetARM9RegionTimings(0x0A000, 0x0B000, Mem9_GBARAM, 8, ramN, ramN);

        SetARM7RegionTimings(0x08000, 0x0A000, 0, 32, 1, 1);
        SetARM7RegionTimings(0x0A000, 0x0B000, 0, 32, 1, 1);
    }
    else
    {
        SetARM9RegionTimings(0x08000, 0x0A000, 0, 32, 1, 1);
        SetARM9RegionTimings(0x0A000, 0x0B000, 0, 32, 1, 1);

        SetARM7RegionTimings(0x08000, 0x0A000, Mem7_GBAROM, 16, romN, romS);
        SetARM7RegionTimings(0x0A000, 0x0B000, Mem7_GBARAM, 8, ramN, ramN);
    }

    // this open-bus implementation is a rough way of simulating the way values
    // lingering on the bus decay after a while, which is visible at higher waitstates
    // for example, the Cartridge Construction Kit relies on this to determine that
    // the GBA slot is empty

    GBACartSlot.SetOpenBusDecay(openbus[(curcnt>>2) & 0x3]);
}


void NDS::UpdateIRQ(u32 cpu)
{
    ARM& arm = cpu ? (ARM&)ARM7 : (ARM&)ARM9;

    if (IME[cpu] & 0x1)
    {
        arm.IRQ = !!(IE[cpu] & IF[cpu]);
        if ((ConsoleType == 1) && cpu)
            arm.IRQ |= !!(IE2 & IF2);
    }
    else
    {
        arm.IRQ = 0;
    }
}

void NDS::SetIRQ(u32 cpu, u32 irq)
{
    IF[cpu] |= (1 << irq);
    UpdateIRQ(cpu);

    if ((cpu == 1) && (CPUStop & CPUStop_Sleep))
    {
        if (IE[1] & (1 << irq))
        {
            CPUStop &= ~CPUStop_Sleep;
            CPUStop |= CPUStop_Wakeup;
            GPU.GPU3D.RestartFrame(GPU);
        }
    }
}

void NDS::ClearIRQ(u32 cpu, u32 irq)
{
    IF[cpu] &= ~(1 << irq);
    UpdateIRQ(cpu);
}

void NDS::SetIRQ2(u32 irq)
{
    IF2 |= (1 << irq);
    UpdateIRQ(1);
}

void NDS::ClearIRQ2(u32 irq)
{
    IF2 &= ~(1 << irq);
    UpdateIRQ(1);
}

bool NDS::HaltInterrupted(u32 cpu) const
{
    if (cpu == 0)
    {
        if (!(IME[0] & 0x1))
            return false;
    }

    if (IF[cpu] & IE[cpu])
        return true;

    if ((ConsoleType == 1) && cpu && (IF2 & IE2))
        return true;

    return false;
}

void NDS::StopCPU(u32 cpu, u32 mask)
{
    if (cpu)
    {
        CPUStop |= (mask << 16);
        ARM7.Halt(2);
    }
    else
    {
        CPUStop |= mask;
        ARM9.Halt(2);
    }
}

void NDS::ResumeCPU(u32 cpu, u32 mask)
{
    if (cpu) mask <<= 16;
    CPUStop &= ~mask;
}

void NDS::GXFIFOStall()
{
    if (CPUStop & CPUStop_GXStall) return;

    CPUStop |= CPUStop_GXStall;

    if (CurCPU == 1) ARM9.Halt(2);
    else
    {
        DMAs[0].StallIfRunning();
        DMAs[1].StallIfRunning();
        DMAs[2].StallIfRunning();
        DMAs[3].StallIfRunning();
        if (ConsoleType == 1)
        {
            auto& dsi = dynamic_cast<melonDS::DSi&>(*this);
            dsi.StallNDMAs();
        }
    }
}

void NDS::GXFIFOUnstall()
{
    CPUStop &= ~CPUStop_GXStall;
}

void NDS::EnterSleepMode()
{
    if (CPUStop & CPUStop_Sleep) return;

    CPUStop |= CPUStop_Sleep;
    ARM7.Halt(2);
}

u32 NDS::GetPC(u32 cpu) const
{
    return cpu ? ARM7.R[15] : ARM9.R[15];
}

u64 NDS::GetSysClockCycles(int num)
{
    u64 ret;

    if (num == 0 || num == 2)
    {
        if (CurCPU == 0)
            ret = ARM9Timestamp >> ARM9ClockShift;
        else
            ret = ARM7Timestamp;

        if (num == 2) ret -= FrameStartTimestamp;
    }
    else if (num == 1)
    {
        ret = LastSysClockCycles;
        LastSysClockCycles = 0;

        if (CurCPU == 0)
            LastSysClockCycles = ARM9Timestamp >> ARM9ClockShift;
        else
            LastSysClockCycles = ARM7Timestamp;
    }

    return ret;
}

void NDS::NocashPrint(u32 ncpu, u32 addr)
{
    // addr: debug string

    ARM* cpu = ncpu ? (ARM*)&ARM7 : (ARM*)&ARM9;
    u8 (NDS::*readfn)(u32) = ncpu ? &NDS::ARM7Read8 : &NDS::ARM9Read8;

    char output[1024];
    int ptr = 0;

    for (int i = 0; i < 120 && ptr < 1023; )
    {
        char ch = (this->*readfn)(addr++);
        i++;

        if (ch == '%')
        {
            char cmd[16]; int j;
            for (j = 0; j < 15; )
            {
                char ch2 = (this->*readfn)(addr++);
                i++;
                if (i >= 120) break;
                if (ch2 == '%') break;
                cmd[j++] = ch2;
            }
            cmd[j] = '\0';

            char subs[64];

            if (cmd[0] == 'r')
            {
                if      (!strcmp(cmd, "r0")) snprintf(subs, sizeof(subs), "%08X", cpu->R[0]);
                else if (!strcmp(cmd, "r1")) snprintf(subs, sizeof(subs), "%08X", cpu->R[1]);
                else if (!strcmp(cmd, "r2")) snprintf(subs, sizeof(subs), "%08X", cpu->R[2]);
                else if (!strcmp(cmd, "r3")) snprintf(subs, sizeof(subs), "%08X", cpu->R[3]);
                else if (!strcmp(cmd, "r4")) snprintf(subs, sizeof(subs), "%08X", cpu->R[4]);
                else if (!strcmp(cmd, "r5")) snprintf(subs, sizeof(subs), "%08X", cpu->R[5]);
                else if (!strcmp(cmd, "r6")) snprintf(subs, sizeof(subs), "%08X", cpu->R[6]);
                else if (!strcmp(cmd, "r7")) snprintf(subs, sizeof(subs), "%08X", cpu->R[7]);
                else if (!strcmp(cmd, "r8")) snprintf(subs, sizeof(subs), "%08X", cpu->R[8]);
                else if (!strcmp(cmd, "r9")) snprintf(subs, sizeof(subs), "%08X", cpu->R[9]);
                else if (!strcmp(cmd, "r10")) snprintf(subs, sizeof(subs), "%08X", cpu->R[10]);
                else if (!strcmp(cmd, "r11")) snprintf(subs, sizeof(subs), "%08X", cpu->R[11]);
                else if (!strcmp(cmd, "r12")) snprintf(subs, sizeof(subs), "%08X", cpu->R[12]);
                else if (!strcmp(cmd, "r13")) snprintf(subs, sizeof(subs), "%08X", cpu->R[13]);
                else if (!strcmp(cmd, "r14")) snprintf(subs, sizeof(subs), "%08X", cpu->R[14]);
                else if (!strcmp(cmd, "r15")) snprintf(subs, sizeof(subs), "%08X", cpu->R[15]);
            }
            else
            {
                if      (!strcmp(cmd, "sp")) snprintf(subs, sizeof(subs), "%08X", cpu->R[13]);
                else if (!strcmp(cmd, "lr")) snprintf(subs, sizeof(subs), "%08X", cpu->R[14]);
                else if (!strcmp(cmd, "pc")) snprintf(subs, sizeof(subs), "%08X", cpu->R[15]);
                else if (!strcmp(cmd, "frame")) snprintf(subs, sizeof(subs), "%u", NumFrames);
                else if (!strcmp(cmd, "scanline")) snprintf(subs, sizeof(subs), "%u", GPU.VCount);
                else if (!strcmp(cmd, "totalclks")) snprintf(subs, sizeof(subs), "%" PRIu64, GetSysClockCycles(0));
                else if (!strcmp(cmd, "lastclks")) snprintf(subs, sizeof(subs), "%" PRIu64, GetSysClockCycles(1));
                else if (!strcmp(cmd, "zeroclks"))
                {
                    snprintf(subs, sizeof(subs), "%s", "");
                    GetSysClockCycles(1);
                }
            }

            int slen = strnlen(subs, sizeof(subs));
            if ((ptr+slen) > 1023) slen = 1023-ptr;
            strncpy(&output[ptr], subs, slen);
            ptr += slen;
        }
        else
        {
            output[ptr++] = ch;
            if (ch == '\0') break;
        }
    }

    output[ptr] = '\0';
    Log(LogLevel::Debug, "%s", output);
}

void NDS::MonitorARM9Jump(u32 addr)
{
    // checkme: can the entrypoint addr be THUMB?
    // also TODO: make it work in DSi mode

    if ((!RunningGame) && NDSCartSlot.GetCart())
    {
        const NDSHeader& header = NDSCartSlot.GetCart()->GetHeader();
        if (addr == header.ARM9EntryAddress)
        {
            Log(LogLevel::Info, "Game is now booting\n");
            RunningGame = true;
        }
    }
}



void NDS::HandleTimerOverflow(u32 tid)
{
    Timer* timer = &Timers[tid];

    timer->Counter += (timer->Reload << 10);
    if (timer->Cnt & (1<<6))
        SetIRQ(tid >> 2, IRQ_Timer0 + (tid & 0x3));

    if ((tid & 0x3) == 3)
        return;

    for (;;)
    {
        tid++;

        timer = &Timers[tid];

        if ((timer->Cnt & 0x84) != 0x84)
            break;

        timer->Counter += (1 << 10);
        if (!(timer->Counter >> 26))
            break;

        timer->Counter = timer->Reload << 10;
        if (timer->Cnt & (1<<6))
            SetIRQ(tid >> 2, IRQ_Timer0 + (tid & 0x3));

        if ((tid & 0x3) == 3)
            break;
    }
}

void NDS::RunTimer(u32 tid, s32 cycles)
{
    Timer* timer = &Timers[tid];

    timer->Counter += (cycles << timer->CycleShift);
    while (timer->Counter >> 26)
    {
        timer->Counter -= (1 << 26);
        HandleTimerOverflow(tid);
    }
}

void NDS::RunTimers(u32 cpu)
{
    u32 timermask = TimerCheckMask[cpu];
    s32 cycles;

    if (cpu == 0)
        cycles = (ARM9Timestamp >> ARM9ClockShift) - TimerTimestamp[0];
    else
        cycles = ARM7Timestamp - TimerTimestamp[1];

    if (timermask & 0x1) RunTimer((cpu<<2)+0, cycles);
    if (timermask & 0x2) RunTimer((cpu<<2)+1, cycles);
    if (timermask & 0x4) RunTimer((cpu<<2)+2, cycles);
    if (timermask & 0x8) RunTimer((cpu<<2)+3, cycles);

    TimerTimestamp[cpu] += cycles;
}

const s32 TimerPrescaler[4] = {0, 6, 8, 10};

u16 NDS::TimerGetCounter(u32 timer)
{
    RunTimers(timer>>2);
    u32 ret = Timers[timer].Counter;

    return ret >> 10;
}

void NDS::TimerStart(u32 id, u16 cnt)
{
    Timer* timer = &Timers[id];
    u16 curstart = timer->Cnt & (1<<7);
    u16 newstart = cnt & (1<<7);

    RunTimers(id>>2);

    timer->Cnt = cnt;
    timer->CycleShift = 10 - TimerPrescaler[cnt & 0x03];

    if ((!curstart) && newstart)
    {
        timer->Counter = timer->Reload << 10;
    }

    if ((cnt & 0x84) == 0x80)
        TimerCheckMask[id>>2] |= 0x01 << (id&0x3);
    else
        TimerCheckMask[id>>2] &= ~(0x01 << (id&0x3));
}



bool NDS::DMAsInMode(u32 cpu, u32 mode) const
{
    cpu <<= 2;
    if (DMAs[cpu+0].IsInMode(mode)) return true;
    if (DMAs[cpu+1].IsInMode(mode)) return true;
    if (DMAs[cpu+2].IsInMode(mode)) return true;
    if (DMAs[cpu+3].IsInMode(mode)) return true;

    return false;
}

bool NDS::DMAsRunning(u32 cpu) const
{
    cpu <<= 2;
    if (DMAs[cpu+0].IsRunning()) return true;
    if (DMAs[cpu+1].IsRunning()) return true;
    if (DMAs[cpu+2].IsRunning()) return true;
    if (DMAs[cpu+3].IsRunning()) return true;

    return false;
}

void NDS::CheckDMAs(u32 cpu, u32 mode)
{
    cpu <<= 2;
    DMAs[cpu+0].StartIfNeeded(mode);
    DMAs[cpu+1].StartIfNeeded(mode);
    DMAs[cpu+2].StartIfNeeded(mode);
    DMAs[cpu+3].StartIfNeeded(mode);
}

void NDS::StopDMAs(u32 cpu, u32 mode)
{
    cpu <<= 2;
    DMAs[cpu+0].StopIfNeeded(mode);
    DMAs[cpu+1].StopIfNeeded(mode);
    DMAs[cpu+2].StopIfNeeded(mode);
    DMAs[cpu+3].StopIfNeeded(mode);
}



void NDS::DivDone(u32 param)
{
    DivCnt &= ~0xC000;

    switch (DivCnt & 0x0003)
    {
    case 0x0000:
        {
            s32 num = (s32)DivNumerator[0];
            s32 den = (s32)DivDenominator[0];
            if (den == 0)
            {
                DivQuotient[0] = (num<0) ? 1:-1;
                DivQuotient[1] = (num<0) ? -1:0;
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
                *(s64*)&DivRemainder[0] = 0;
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
                *(s64*)&DivRemainder[0] = 0;
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
        DivCnt |= 0x4000;
}

void NDS::StartDiv()
{
    CancelEvent(Event_Div);
    DivCnt |= 0x8000;
    ScheduleEvent(Event_Div, false, ((DivCnt&0x3)==0) ? 18:34, 0, 0);
}

// http://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
void NDS::SqrtDone(u32 param)
{
    u64 val;
    u32 res = 0;
    u64 rem = 0;
    u32 prod = 0;
    u32 nbits, topshift;

    SqrtCnt &= ~0x8000;

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

void NDS::StartSqrt()
{
    CancelEvent(Event_Sqrt);
    SqrtCnt |= 0x8000;
    ScheduleEvent(Event_Sqrt, false, 13, 0, 0);
}



void NDS::debug(u32 param)
{
    Log(LogLevel::Debug, "ARM9 PC=%08X LR=%08X %08X\n", ARM9.R[15], ARM9.R[14], ARM9.R_IRQ[1]);
    Log(LogLevel::Debug, "ARM7 PC=%08X LR=%08X %08X\n", ARM7.R[15], ARM7.R[14], ARM7.R_IRQ[1]);

    Log(LogLevel::Debug, "ARM9 IME=%08X IE=%08X IF=%08X\n", IME[0], IE[0], IF[0]);
    Log(LogLevel::Debug, "ARM7 IME=%08X IE=%08X IF=%08X IE2=%04X IF2=%04X\n", IME[1], IE[1], IF[1], IE2, IF2);

    //for (int i = 0; i < 9; i++)
    //    printf("VRAM %c: %02X\n", 'A'+i, GPU->VRAMCNT[i]);

    Platform::FileHandle* shit = Platform::OpenFile("debug/pokeplat.bin", FileMode::Write);
    Platform::FileWrite(ARM9.ITCM, 0x8000, 1, shit);
    for (u32 i = 0x02000000; i < 0x02400000; i+=4)
    {
        u32 val = NDS::ARM7Read32(i);
        Platform::FileWrite(&val, 4, 1, shit);
    }
    for (u32 i = 0x037F0000; i < 0x03810000; i+=4)
    {
        u32 val = NDS::ARM7Read32(i);
        Platform::FileWrite(&val, 4, 1, shit);
    }
    for (u32 i = 0x06000000; i < 0x06040000; i+=4)
    {
        u32 val = NDS::ARM7Read32(i);
        Platform::FileWrite(&val, 4, 1, shit);
    }
    Platform::CloseFile(shit);

    /*FILE*
    shit = fopen("debug/directboot9.bin", "wb");
    for (u32 i = 0x02000000; i < 0x04000000; i+=4)
    {
        u32 val = DSi::ARM9Read32(i);
        fwrite(&val, 4, 1, shit);
    }
    fclose(shit);
    shit = fopen("debug/camera7.bin", "wb");
    for (u32 i = 0x02000000; i < 0x04000000; i+=4)
    {
        u32 val = DSi::ARM7Read32(i);
        fwrite(&val, 4, 1, shit);
    }
    fclose(shit);*/
}



u8 NDS::ARM9Read8(u32 addr)
{
    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u8*)&ARM9BIOS[addr & 0xFFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u8*)&MainRAM[addr & MainRAMMask];

    case 0x03000000:
        if (SWRAM_ARM9.Mem)
        {
            return *(u8*)&SWRAM_ARM9.Mem[addr & SWRAM_ARM9.Mask];
        }
        else
        {
            return 0;
        }

    case 0x04000000:
        // Specifically want to call the NDS version, not a subclass
        return NDS::ARM9IORead8(addr);

    case 0x05000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return GPU.ReadPalette<u8>(addr);

    case 0x06000000:
        switch (addr & 0x00E00000)
        {
        case 0x00000000: return GPU.ReadVRAM_ABG<u8>(addr);
        case 0x00200000: return GPU.ReadVRAM_BBG<u8>(addr);
        case 0x00400000: return GPU.ReadVRAM_AOBJ<u8>(addr);
        case 0x00600000: return GPU.ReadVRAM_BOBJ<u8>(addr);
        default:         return GPU.ReadVRAM_LCDC<u8>(addr);
        }

    case 0x07000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return GPU.ReadOAM<u8>(addr);

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return 0x00; // deselected CPU is 00h-filled
        if (addr & 0x1) return GBACartSlot.ROMRead(addr-1) >> 8;
        return GBACartSlot.ROMRead(addr) & 0xFF;

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return 0x00; // deselected CPU is 00h-filled
        return GBACartSlot.SRAMRead(addr);
    }

    Log(LogLevel::Debug, "unknown arm9 read8 %08X\n", addr);
    return 0;
}

u16 NDS::ARM9Read16(u32 addr)
{
    addr &= ~0x1;

    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u16*)&ARM9BIOS[addr & 0xFFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u16*)&MainRAM[addr & MainRAMMask];

    case 0x03000000:
        if (SWRAM_ARM9.Mem)
        {
            return *(u16*)&SWRAM_ARM9.Mem[addr & SWRAM_ARM9.Mask];
        }
        else
        {
            return 0;
        }

    case 0x04000000:
        return NDS::ARM9IORead16(addr);

    case 0x05000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return GPU.ReadPalette<u16>(addr);

    case 0x06000000:
        switch (addr & 0x00E00000)
        {
        case 0x00000000: return GPU.ReadVRAM_ABG<u16>(addr);
        case 0x00200000: return GPU.ReadVRAM_BBG<u16>(addr);
        case 0x00400000: return GPU.ReadVRAM_AOBJ<u16>(addr);
        case 0x00600000: return GPU.ReadVRAM_BOBJ<u16>(addr);
        default:         return GPU.ReadVRAM_LCDC<u16>(addr);
        }

    case 0x07000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return GPU.ReadOAM<u16>(addr);

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return 0x0000; // deselected CPU is 00h-filled
        return GBACartSlot.ROMRead(addr);

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return 0x0000; // deselected CPU is 00h-filled
        return GBACartSlot.SRAMRead(addr) |
              (GBACartSlot.SRAMRead(addr+1) << 8);
    }

    //if (addr) Log(LogLevel::Warn, "unknown arm9 read16 %08X %08X\n", addr, ARM9.R[15]);
    return 0;
}

u32 NDS::ARM9Read32(u32 addr)
{
    addr &= ~0x3;

    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u32*)&ARM9BIOS[addr & 0xFFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u32*)&MainRAM[addr & MainRAMMask];

    case 0x03000000:
        if (SWRAM_ARM9.Mem)
        {
            return *(u32*)&SWRAM_ARM9.Mem[addr & SWRAM_ARM9.Mask];
        }
        else
        {
            return 0;
        }

    case 0x04000000:
        return NDS::ARM9IORead32(addr);

    case 0x05000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return GPU.ReadPalette<u32>(addr);

    case 0x06000000:
        switch (addr & 0x00E00000)
        {
        case 0x00000000: return GPU.ReadVRAM_ABG<u32>(addr);
        case 0x00200000: return GPU.ReadVRAM_BBG<u32>(addr);
        case 0x00400000: return GPU.ReadVRAM_AOBJ<u32>(addr);
        case 0x00600000: return GPU.ReadVRAM_BOBJ<u32>(addr);
        default:         return GPU.ReadVRAM_LCDC<u32>(addr);
        }

    case 0x07000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return GPU.ReadOAM<u32>(addr & 0x7FF);

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return 0x00000000; // deselected CPU is 00h-filled
        return GBACartSlot.ROMRead(addr) |
              (GBACartSlot.ROMRead(addr+2) << 16);

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return 0x00000000; // deselected CPU is 00h-filled
        return GBACartSlot.SRAMRead(addr) |
              (GBACartSlot.SRAMRead(addr+1) << 8) |
              (GBACartSlot.SRAMRead(addr+2) << 16) |
              (GBACartSlot.SRAMRead(addr+3) << 24);
    }

    //Log(LogLevel::Warn, "unknown arm9 read32 %08X | %08X %08X\n", addr, ARM9.R[15], ARM9.R[12]);
    return 0;
}

void NDS::ARM9Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_MainRAM>(addr);
        *(u8*)&MainRAM[addr & MainRAMMask] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM9.Mem)
        {
            JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_SharedWRAM>(addr);
            *(u8*)&SWRAM_ARM9.Mem[addr & SWRAM_ARM9.Mask] = val;
        }
        return;

    case 0x04000000:
        NDS::ARM9IOWrite8(addr, val);
        return;

    case 0x05000000:
    case 0x06000000:
    case 0x07000000:
        return;

    case 0x08000000:
    case 0x09000000:
        return;

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        GBACartSlot.SRAMWrite(addr, val);
        return;
    }

    Log(LogLevel::Debug, "unknown arm9 write8 %08X %02X\n", addr, val);
}

void NDS::ARM9Write16(u32 addr, u16 val)
{
    addr &= ~0x1;

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_MainRAM>(addr);
        *(u16*)&MainRAM[addr & MainRAMMask] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM9.Mem)
        {
            JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_SharedWRAM>(addr);
            *(u16*)&SWRAM_ARM9.Mem[addr & SWRAM_ARM9.Mask] = val;
        }
        return;

    case 0x04000000:
        NDS::ARM9IOWrite16(addr, val);
        return;

    case 0x05000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return;
        GPU.WritePalette<u16>(addr, val);
        return;

    case 0x06000000:
        JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_VRAM>(addr);
        switch (addr & 0x00E00000)
        {
        case 0x00000000: GPU.WriteVRAM_ABG<u16>(addr, val); return;
        case 0x00200000: GPU.WriteVRAM_BBG<u16>(addr, val); return;
        case 0x00400000: GPU.WriteVRAM_AOBJ<u16>(addr, val); return;
        case 0x00600000: GPU.WriteVRAM_BOBJ<u16>(addr, val); return;
        default: GPU.WriteVRAM_LCDC<u16>(addr, val); return;
        }

    case 0x07000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return;
        GPU.WriteOAM<u16>(addr, val);
        return;

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        GBACartSlot.ROMWrite(addr, val);
        return;

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        GBACartSlot.SRAMWrite(addr, val & 0xFF);
        GBACartSlot.SRAMWrite(addr+1, val >> 8);
        return;
    }

    //if (addr) Log(LogLevel::Warn, "unknown arm9 write16 %08X %04X\n", addr, val);
}

void NDS::ARM9Write32(u32 addr, u32 val)
{
    addr &= ~0x3;

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_MainRAM>(addr);
        *(u32*)&MainRAM[addr & MainRAMMask] = val;
        return ;

    case 0x03000000:
        if (SWRAM_ARM9.Mem)
        {
            JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_SharedWRAM>(addr);
            *(u32*)&SWRAM_ARM9.Mem[addr & SWRAM_ARM9.Mask] = val;
        }
        return;

    case 0x04000000:
        NDS::ARM9IOWrite32(addr, val);
        return;

    case 0x05000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return;
        GPU.WritePalette(addr, val);
        return;

    case 0x06000000:
        JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_VRAM>(addr);
        switch (addr & 0x00E00000)
        {
        case 0x00000000: GPU.WriteVRAM_ABG<u32>(addr, val); return;
        case 0x00200000: GPU.WriteVRAM_BBG<u32>(addr, val); return;
        case 0x00400000: GPU.WriteVRAM_AOBJ<u32>(addr, val); return;
        case 0x00600000: GPU.WriteVRAM_BOBJ<u32>(addr, val); return;
        default: GPU.WriteVRAM_LCDC<u32>(addr, val); return;
        }

    case 0x07000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return;
        GPU.WriteOAM<u32>(addr, val);
        return;

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        GBACartSlot.ROMWrite(addr, val & 0xFFFF);
        GBACartSlot.ROMWrite(addr+2, val >> 16);
        return;

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        GBACartSlot.SRAMWrite(addr, val & 0xFF);
        GBACartSlot.SRAMWrite(addr+1, (val >> 8) & 0xFF);
        GBACartSlot.SRAMWrite(addr+2, (val >> 16) & 0xFF);
        GBACartSlot.SRAMWrite(addr+3, val >> 24);
        return;
    }

    //Log(LogLevel::Warn, "unknown arm9 write32 %08X %08X | %08X\n", addr, val, ARM9.R[15]);
}

bool NDS::ARM9GetMemRegion(u32 addr, bool write, MemRegion* region)
{
    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        region->Mem = MainRAM;
        region->Mask = MainRAMMask;
        return true;

    case 0x03000000:
        if (SWRAM_ARM9.Mem)
        {
            region->Mem = SWRAM_ARM9.Mem;
            region->Mask = SWRAM_ARM9.Mask;
            return true;
        }
        break;
    }

    if ((addr & 0xFFFFF000) == 0xFFFF0000 && !write)
    {
        region->Mem = &ARM9BIOS[0];
        region->Mask = 0xFFF;
        return true;
    }

    region->Mem = NULL;
    return false;
}



u8 NDS::ARM7Read8(u32 addr)
{
    if (addr < 0x00004000)
    {
        // TODO: check the boundary? is it 4000 or higher on regular DS?
        if (ARM7.R[15] >= 0x00004000)
            return 0xFF;
        if (addr < ARM7BIOSProt && ARM7.R[15] >= ARM7BIOSProt)
            return 0xFF;

        return *(u8*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        return *(u8*)&MainRAM[addr & MainRAMMask];

    case 0x03000000:
        if (SWRAM_ARM7.Mem)
        {
            return *(u8*)&SWRAM_ARM7.Mem[addr & SWRAM_ARM7.Mask];
        }
        else
        {
            return *(u8*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)];
        }

    case 0x03800000:
        return *(u8*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)];

    case 0x04000000:
        return NDS::ARM7IORead8(addr);

    case 0x04800000:
        if (addr < 0x04810000)
        {
            if (!(PowerControl7 & (1<<1))) return 0;
            if (addr & 0x1) return Wifi.Read(addr-1) >> 8;
            return Wifi.Read(addr) & 0xFF;
        }
        break;

    case 0x06000000:
    case 0x06800000:
        return GPU.ReadVRAM_ARM7<u8>(addr);

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x00; // deselected CPU is 00h-filled
        if (addr & 0x1) return GBACartSlot.ROMRead(addr-1) >> 8;
        return GBACartSlot.ROMRead(addr) & 0xFF;

    case 0x0A000000:
    case 0x0A800000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x00; // deselected CPU is 00h-filled
        return GBACartSlot.SRAMRead(addr);
    }

    Log(LogLevel::Debug, "unknown arm7 read8 %08X %08X %08X/%08X\n", addr, ARM7.R[15], ARM7.R[0], ARM7.R[1]);
    return 0;
}

u16 NDS::ARM7Read16(u32 addr)
{
    addr &= ~0x1;

    if (addr < 0x00004000)
    {
        if (ARM7.R[15] >= 0x00004000)
            return 0xFFFF;
        if (addr < ARM7BIOSProt && ARM7.R[15] >= ARM7BIOSProt)
            return 0xFFFF;

        return *(u16*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        return *(u16*)&MainRAM[addr & MainRAMMask];

    case 0x03000000:
        if (SWRAM_ARM7.Mem)
        {
            return *(u16*)&SWRAM_ARM7.Mem[addr & SWRAM_ARM7.Mask];
        }
        else
        {
            return *(u16*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)];
        }

    case 0x03800000:
        return *(u16*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)];

    case 0x04000000:
        return NDS::ARM7IORead16(addr);

    case 0x04800000:
        if (addr < 0x04810000)
        {
            if (!(PowerControl7 & (1<<1))) return 0;
            return Wifi.Read(addr);
        }
        break;

    case 0x06000000:
    case 0x06800000:
        return GPU.ReadVRAM_ARM7<u16>(addr);

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x0000; // deselected CPU is 00h-filled
        return GBACartSlot.ROMRead(addr);

    case 0x0A000000:
    case 0x0A800000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x0000; // deselected CPU is 00h-filled
        return GBACartSlot.SRAMRead(addr) |
              (GBACartSlot.SRAMRead(addr+1) << 8);
    }

    Log(LogLevel::Debug, "unknown arm7 read16 %08X %08X\n", addr, ARM7.R[15]);
    return 0;
}

u32 NDS::ARM7Read32(u32 addr)
{
    addr &= ~0x3;

    if (addr < 0x00004000)
    {
        if (ARM7.R[15] >= 0x00004000)
            return 0xFFFFFFFF;
        if (addr < ARM7BIOSProt && ARM7.R[15] >= ARM7BIOSProt)
            return 0xFFFFFFFF;

        return *(u32*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        return *(u32*)&MainRAM[addr & MainRAMMask];

    case 0x03000000:
        if (SWRAM_ARM7.Mem)
        {
            return *(u32*)&SWRAM_ARM7.Mem[addr & SWRAM_ARM7.Mask];
        }
        else
        {
            return *(u32*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)];
        }

    case 0x03800000:
        return *(u32*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)];

    case 0x04000000:
        return NDS::ARM7IORead32(addr);

    case 0x04800000:
        if (addr < 0x04810000)
        {
            if (!(PowerControl7 & (1<<1))) return 0;
            return Wifi.Read(addr) | (Wifi.Read(addr+2) << 16);
        }
        break;

    case 0x06000000:
    case 0x06800000:
        return GPU.ReadVRAM_ARM7<u32>(addr);

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x00000000; // deselected CPU is 00h-filled
        return GBACartSlot.ROMRead(addr) |
              (GBACartSlot.ROMRead(addr+2) << 16);

    case 0x0A000000:
    case 0x0A800000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x00000000; // deselected CPU is 00h-filled
        return GBACartSlot.SRAMRead(addr) |
              (GBACartSlot.SRAMRead(addr+1) << 8) |
              (GBACartSlot.SRAMRead(addr+2) << 16) |
              (GBACartSlot.SRAMRead(addr+3) << 24);
    }

    //Log(LogLevel::Warn, "unknown arm7 read32 %08X | %08X\n", addr, ARM7.R[15]);
    return 0;
}

void NDS::ARM7Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_MainRAM>(addr);
        *(u8*)&MainRAM[addr & MainRAMMask] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM7.Mem)
        {
            JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_SharedWRAM>(addr);
            *(u8*)&SWRAM_ARM7.Mem[addr & SWRAM_ARM7.Mask] = val;
            return;
        }
        else
        {
            JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
            *(u8*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
            return;
        }

    case 0x03800000:
        JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
        *(u8*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
        return;

    case 0x04000000:
        NDS::ARM7IOWrite8(addr, val);
        return;

    case 0x06000000:
    case 0x06800000:
        JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_VWRAM>(addr);
        GPU.WriteVRAM_ARM7<u8>(addr, val);
        return;

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
        return;

    case 0x0A000000:
    case 0x0A800000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        GBACartSlot.SRAMWrite(addr, val);
        return;
    }

    //if (ARM7.R[15] > 0x00002F30) // ARM7 BIOS bug
    if (addr >= 0x01000000)
        Log(LogLevel::Debug, "unknown arm7 write8 %08X %02X @ %08X\n", addr, val, ARM7.R[15]);
}

void NDS::ARM7Write16(u32 addr, u16 val)
{
    addr &= ~0x1;

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_MainRAM>(addr);
        *(u16*)&MainRAM[addr & MainRAMMask] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM7.Mem)
        {
            JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_SharedWRAM>(addr);
            *(u16*)&SWRAM_ARM7.Mem[addr & SWRAM_ARM7.Mask] = val;
            return;
        }
        else
        {
            JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
            *(u16*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
            return;
        }

    case 0x03800000:
        JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
        *(u16*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
        return;

    case 0x04000000:
        NDS::ARM7IOWrite16(addr, val);
        return;

    case 0x04800000:
        if (addr < 0x04810000)
        {
            if (!(PowerControl7 & (1<<1))) return;
            Wifi.Write(addr, val);
            return;
        }
        break;

    case 0x06000000:
    case 0x06800000:
        JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_VWRAM>(addr);
        GPU.WriteVRAM_ARM7<u16>(addr, val);
        return;

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        GBACartSlot.ROMWrite(addr, val);
        return;

    case 0x0A000000:
    case 0x0A800000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        GBACartSlot.SRAMWrite(addr, val & 0xFF);
        GBACartSlot.SRAMWrite(addr+1, val >> 8);
        return;
    }

    if (addr >= 0x01000000)
        Log(LogLevel::Debug, "unknown arm7 write16 %08X %04X @ %08X\n", addr, val, ARM7.R[15]);
}

void NDS::ARM7Write32(u32 addr, u32 val)
{
    addr &= ~0x3;

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_MainRAM>(addr);
        *(u32*)&MainRAM[addr & MainRAMMask] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM7.Mem)
        {
            JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_SharedWRAM>(addr);
            *(u32*)&SWRAM_ARM7.Mem[addr & SWRAM_ARM7.Mask] = val;
            return;
        }
        else
        {
            JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
            *(u32*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
            return;
        }

    case 0x03800000:
        JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
        *(u32*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
        return;

    case 0x04000000:
        NDS::ARM7IOWrite32(addr, val);
        return;

    case 0x04800000:
        if (addr < 0x04810000)
        {
            if (!(PowerControl7 & (1<<1))) return;
            Wifi.Write(addr, val & 0xFFFF);
            Wifi.Write(addr+2, val >> 16);
            return;
        }
        break;

    case 0x06000000:
    case 0x06800000:
        JIT.CheckAndInvalidate<1, ARMJIT_Memory::memregion_VWRAM>(addr);
        GPU.WriteVRAM_ARM7<u32>(addr, val);
        return;

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        GBACartSlot.ROMWrite(addr, val & 0xFFFF);
        GBACartSlot.ROMWrite(addr+2, val >> 16);
        return;

    case 0x0A000000:
    case 0x0A800000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        GBACartSlot.SRAMWrite(addr, val & 0xFF);
        GBACartSlot.SRAMWrite(addr+1, (val >> 8) & 0xFF);
        GBACartSlot.SRAMWrite(addr+2, (val >> 16) & 0xFF);
        GBACartSlot.SRAMWrite(addr+3, val >> 24);
        return;
    }

    if (addr >= 0x01000000)
        Log(LogLevel::Debug, "unknown arm7 write32 %08X %08X @ %08X\n", addr, val, ARM7.R[15]);
}

bool NDS::ARM7GetMemRegion(u32 addr, bool write, MemRegion* region)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        region->Mem = MainRAM;
        region->Mask = MainRAMMask;
        return true;

    case 0x03000000:
        // note on this, and why we can only cover it in one particular case:
        // it is typical for games to map all shared WRAM to the ARM7
        // then access all the WRAM as one contiguous block starting at 0x037F8000
        // this case needs a bit of a hack to cover
        // it's not really worth bothering anyway
        if (!SWRAM_ARM7.Mem)
        {
            region->Mem = ARM7WRAM;
            region->Mask = ARM7WRAMSize-1;
            return true;
        }
        break;

    case 0x03800000:
        region->Mem = ARM7WRAM;
        region->Mask = ARM7WRAMSize-1;
        return true;
    }

    // BIOS. ARM7 PC has to be within range.
    if (addr < 0x00004000 && !write)
    {
        if (ARM7.R[15] < 0x4000 && (addr >= ARM7BIOSProt || ARM7.R[15] < ARM7BIOSProt))
        {
            region->Mem = &ARM7BIOS[0];
            region->Mask = 0x3FFF;
            return true;
        }
    }

    region->Mem = NULL;
    return false;
}




#define CASE_READ8_16BIT(addr, val) \
    case (addr): return (val) & 0xFF; \
    case (addr+1): return (val) >> 8;

#define CASE_READ8_32BIT(addr, val) \
    case (addr): return (val) & 0xFF; \
    case (addr+1): return ((val) >> 8) & 0xFF; \
    case (addr+2): return ((val) >> 16) & 0xFF; \
    case (addr+3): return (val) >> 24;

u8 NDS::ARM9IORead8(u32 addr)
{
    switch (addr)
    {
    case 0x04000130: LagFrameFlag = false; return KeyInput & 0xFF;
    case 0x04000131: LagFrameFlag = false; return (KeyInput >> 8) & 0xFF;
    case 0x04000132: return KeyCnt[0] & 0xFF;
    case 0x04000133: return KeyCnt[0] >> 8;

    case 0x040001A0:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetSPICnt() & 0xFF;
        return 0;
    case 0x040001A1:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetSPICnt() >> 8;
        return 0;

    case 0x040001A2:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.ReadSPIData();
        return 0;

    case 0x040001A4:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCnt() & 0xFF;
        return 0;
    case 0x040001A5:
        if (!(ExMemCnt[0] & (1<<11)))
            return (NDSCartSlot.GetROMCnt() >> 8) & 0xFF;
        return 0;
    case 0x040001A6:
        if (!(ExMemCnt[0] & (1<<11)))
            return (NDSCartSlot.GetROMCnt() >> 16) & 0xFF;
        return 0;
    case 0x040001A7:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCnt() >> 24;
        return 0;

    case 0x040001A8:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(0);
        return 0;
    case 0x040001A9:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(1);
        return 0;
    case 0x040001AA:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(2);
        return 0;
    case 0x040001AB:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(3);
        return 0;
    case 0x040001AC:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(4);
        return 0;
    case 0x040001AD:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(5);
        return 0;
    case 0x040001AE:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(6);
        return 0;
    case 0x040001AF:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(7);
        return 0;

    case 0x04000208: return IME[0];

    case 0x04000240: return GPU.VRAMCNT[0];
    case 0x04000241: return GPU.VRAMCNT[1];
    case 0x04000242: return GPU.VRAMCNT[2];
    case 0x04000243: return GPU.VRAMCNT[3];
    case 0x04000244: return GPU.VRAMCNT[4];
    case 0x04000245: return GPU.VRAMCNT[5];
    case 0x04000246: return GPU.VRAMCNT[6];
    case 0x04000247: return WRAMCnt;
    case 0x04000248: return GPU.VRAMCNT[7];
    case 0x04000249: return GPU.VRAMCNT[8];

    CASE_READ8_16BIT(0x04000280, DivCnt)
    CASE_READ8_32BIT(0x04000290, DivNumerator[0])
    CASE_READ8_32BIT(0x04000294, DivNumerator[1])
    CASE_READ8_32BIT(0x04000298, DivDenominator[0])
    CASE_READ8_32BIT(0x0400029C, DivDenominator[1])
    CASE_READ8_32BIT(0x040002A0, DivQuotient[0])
    CASE_READ8_32BIT(0x040002A4, DivQuotient[1])
    CASE_READ8_32BIT(0x040002A8, DivRemainder[0])
    CASE_READ8_32BIT(0x040002AC, DivRemainder[1])

    CASE_READ8_16BIT(0x040002B0, SqrtCnt)
    CASE_READ8_32BIT(0x040002B4, SqrtRes)
    CASE_READ8_32BIT(0x040002B8, SqrtVal[0])
    CASE_READ8_32BIT(0x040002BC, SqrtVal[1])

    case 0x04000300: return PostFlag9;
    }

    if (addr >= 0x04000000 && addr < 0x04000060)
    {
        return GPU.GPU2D_A.Read8(addr);
    }
    if (addr >= 0x04001000 && addr < 0x04001060)
    {
        return GPU.GPU2D_B.Read8(addr);
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        return GPU.GPU3D.Read8(addr);
    }
    // NO$GBA debug register "Emulation ID"
    if(addr >= 0x04FFFA00 && addr < 0x04FFFA10)
    {
        // FIX: GBATek says this should be padded with spaces
        static char const emuID[16] = "melonDS " MELONDS_VERSION_BASE;
        auto idx = addr - 0x04FFFA00;
        return (u8)(emuID[idx]);
    }

    if ((addr & 0xFFFFF000) != 0x04004000)
        Log(LogLevel::Debug, "unknown ARM9 IO read8 %08X %08X\n", addr, ARM9.R[15]);
    return 0;
}

u16 NDS::ARM9IORead16(u32 addr)
{
    switch (addr)
    {
    case 0x04000004: return GPU.DispStat[0];
    case 0x04000006: return GPU.VCount;

    case 0x04000060: return GPU.GPU3D.Read16(addr);
    case 0x04000064:
    case 0x04000066: return GPU.GPU2D_A.Read16(addr);

    case 0x040000B8: return DMAs[0].Cnt & 0xFFFF;
    case 0x040000BA: return DMAs[0].Cnt >> 16;
    case 0x040000C4: return DMAs[1].Cnt & 0xFFFF;
    case 0x040000C6: return DMAs[1].Cnt >> 16;
    case 0x040000D0: return DMAs[2].Cnt & 0xFFFF;
    case 0x040000D2: return DMAs[2].Cnt >> 16;
    case 0x040000DC: return DMAs[3].Cnt & 0xFFFF;
    case 0x040000DE: return DMAs[3].Cnt >> 16;

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

    case 0x04000130: LagFrameFlag = false; return KeyInput & 0xFFFF;
    case 0x04000132: return KeyCnt[0];

    case 0x04000180: return IPCSync9;
    case 0x04000184:
        {
            u16 val = IPCFIFOCnt9;
            if (IPCFIFO9.IsEmpty())     val |= 0x0001;
            else if (IPCFIFO9.IsFull()) val |= 0x0002;
            if (IPCFIFO7.IsEmpty())     val |= 0x0100;
            else if (IPCFIFO7.IsFull()) val |= 0x0200;
            return val;
        }

    case 0x040001A0:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetSPICnt();
        return 0;
    case 0x040001A2:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.ReadSPIData();
        return 0;

    case 0x040001A4:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCnt() & 0xFFFF;
        return 0;
    case 0x040001A6:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCnt() >> 16;
        return 0;

    case 0x040001A8:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(0) |
                  (NDSCartSlot.GetROMCommand(1) << 8);
        return 0;
    case 0x040001AA:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(2) |
                  (NDSCartSlot.GetROMCommand(3) << 8);
        return 0;
    case 0x040001AC:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(4) |
                  (NDSCartSlot.GetROMCommand(5) << 8);
        return 0;
    case 0x040001AE:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(6) |
                  (NDSCartSlot.GetROMCommand(7) << 8);
        return 0;

    case 0x04000204: return ExMemCnt[0];
    case 0x04000208: return IME[0];
    case 0x04000210: return IE[0] & 0xFFFF;
    case 0x04000212: return IE[0] >> 16;

    case 0x04000240: return GPU.VRAMCNT[0] | (GPU.VRAMCNT[1] << 8);
    case 0x04000242: return GPU.VRAMCNT[2] | (GPU.VRAMCNT[3] << 8);
    case 0x04000244: return GPU.VRAMCNT[4] | (GPU.VRAMCNT[5] << 8);
    case 0x04000246: return GPU.VRAMCNT[6] | (WRAMCnt << 8);
    case 0x04000248: return GPU.VRAMCNT[7] | (GPU.VRAMCNT[8] << 8);

    case 0x04000280: return DivCnt;
    case 0x04000290: return DivNumerator[0] & 0xFFFF;
    case 0x04000292: return DivNumerator[0] >> 16;
    case 0x04000294: return DivNumerator[1] & 0xFFFF;
    case 0x04000296: return DivNumerator[1] >> 16;
    case 0x04000298: return DivDenominator[0] & 0xFFFF;
    case 0x0400029A: return DivDenominator[0] >> 16;
    case 0x0400029C: return DivDenominator[1] & 0xFFFF;
    case 0x0400029E: return DivDenominator[1] >> 16;
    case 0x040002A0: return DivQuotient[0] & 0xFFFF;
    case 0x040002A2: return DivQuotient[0] >> 16;
    case 0x040002A4: return DivQuotient[1] & 0xFFFF;
    case 0x040002A6: return DivQuotient[1] >> 16;
    case 0x040002A8: return DivRemainder[0] & 0xFFFF;
    case 0x040002AA: return DivRemainder[0] >> 16;
    case 0x040002AC: return DivRemainder[1] & 0xFFFF;
    case 0x040002AE: return DivRemainder[1] >> 16;

    case 0x040002B0: return SqrtCnt;
    case 0x040002B4: return SqrtRes & 0xFFFF;
    case 0x040002B6: return SqrtRes >> 16;
    case 0x040002B8: return SqrtVal[0] & 0xFFFF;
    case 0x040002BA: return SqrtVal[0] >> 16;
    case 0x040002BC: return SqrtVal[1] & 0xFFFF;
    case 0x040002BE: return SqrtVal[1] >> 16;

    case 0x04000300: return PostFlag9;
    case 0x04000304: return PowerControl9;

    case 0x04004000:
    case 0x04004004:
    case 0x04004010:
        // shut up logging for DSi registers
        return 0;
    }

    if ((addr >= 0x04000000 && addr < 0x04000060) || (addr == 0x0400006C))
    {
        return GPU.GPU2D_A.Read16(addr);
    }
    if ((addr >= 0x04001000 && addr < 0x04001060) || (addr == 0x0400106C))
    {
        return GPU.GPU2D_B.Read16(addr);
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        return GPU.GPU3D.Read16(addr);
    }

    if ((addr & 0xFFFFF000) != 0x04004000)
        Log(LogLevel::Debug, "unknown ARM9 IO read16 %08X %08X\n", addr, ARM9.R[15]);
    return 0;
}

u32 NDS::ARM9IORead32(u32 addr)
{
    switch (addr)
    {
    case 0x04000004: return GPU.DispStat[0] | (GPU.VCount << 16);

    case 0x04000060: return GPU.GPU3D.Read32(addr);
    case 0x04000064: return GPU.GPU2D_A.Read32(addr);

    case 0x040000B0: return DMAs[0].SrcAddr;
    case 0x040000B4: return DMAs[0].DstAddr;
    case 0x040000B8: return DMAs[0].Cnt;
    case 0x040000BC: return DMAs[1].SrcAddr;
    case 0x040000C0: return DMAs[1].DstAddr;
    case 0x040000C4: return DMAs[1].Cnt;
    case 0x040000C8: return DMAs[2].SrcAddr;
    case 0x040000CC: return DMAs[2].DstAddr;
    case 0x040000D0: return DMAs[2].Cnt;
    case 0x040000D4: return DMAs[3].SrcAddr;
    case 0x040000D8: return DMAs[3].DstAddr;
    case 0x040000DC: return DMAs[3].Cnt;

    case 0x040000E0: return DMA9Fill[0];
    case 0x040000E4: return DMA9Fill[1];
    case 0x040000E8: return DMA9Fill[2];
    case 0x040000EC: return DMA9Fill[3];

    case 0x040000F4: return 0; // ???? Golden Sun Dark Dawn keeps reading this

    case 0x04000100: return TimerGetCounter(0) | (Timers[0].Cnt << 16);
    case 0x04000104: return TimerGetCounter(1) | (Timers[1].Cnt << 16);
    case 0x04000108: return TimerGetCounter(2) | (Timers[2].Cnt << 16);
    case 0x0400010C: return TimerGetCounter(3) | (Timers[3].Cnt << 16);

    case 0x04000130: LagFrameFlag = false; return (KeyInput & 0xFFFF) | (KeyCnt[0] << 16);

    case 0x04000180: return IPCSync9;
    case 0x04000184: return NDS::ARM9IORead16(addr);

    case 0x040001A0:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetSPICnt() | (NDSCartSlot.ReadSPIData() << 16);
        return 0;
    case 0x040001A4:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCnt();
        return 0;

    case 0x040001A8:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(0) |
                  (NDSCartSlot.GetROMCommand(1) << 8) |
                  (NDSCartSlot.GetROMCommand(2) << 16) |
                  (NDSCartSlot.GetROMCommand(3) << 24);
        return 0;
    case 0x040001AC:
        if (!(ExMemCnt[0] & (1<<11)))
            return NDSCartSlot.GetROMCommand(4) |
                  (NDSCartSlot.GetROMCommand(5) << 8) |
                  (NDSCartSlot.GetROMCommand(6) << 16) |
                  (NDSCartSlot.GetROMCommand(7) << 24);
        return 0;

    case 0x04000208: return IME[0];
    case 0x04000210: return IE[0];
    case 0x04000214: return IF[0];

    case 0x04000240: return GPU.VRAMCNT[0] | (GPU.VRAMCNT[1] << 8) | (GPU.VRAMCNT[2] << 16) | (GPU.VRAMCNT[3] << 24);
    case 0x04000244: return GPU.VRAMCNT[4] | (GPU.VRAMCNT[5] << 8) | (GPU.VRAMCNT[6] << 16) | (WRAMCnt << 24);
    case 0x04000248: return GPU.VRAMCNT[7] | (GPU.VRAMCNT[8] << 8);

    case 0x04000280: return DivCnt;
    case 0x04000290: return DivNumerator[0];
    case 0x04000294: return DivNumerator[1];
    case 0x04000298: return DivDenominator[0];
    case 0x0400029C: return DivDenominator[1];
    case 0x040002A0: return DivQuotient[0];
    case 0x040002A4: return DivQuotient[1];
    case 0x040002A8: return DivRemainder[0];
    case 0x040002AC: return DivRemainder[1];

    case 0x040002B0: return SqrtCnt;
    case 0x040002B4: return SqrtRes;
    case 0x040002B8: return SqrtVal[0];
    case 0x040002BC: return SqrtVal[1];

    case 0x04000300: return PostFlag9;
    case 0x04000304: return PowerControl9;

    case 0x04100000:
        if (IPCFIFOCnt9 & 0x8000)
        {
            u32 ret;
            if (IPCFIFO7.IsEmpty())
            {
                IPCFIFOCnt9 |= 0x4000;
                ret = IPCFIFO7.Peek();
            }
            else
            {
                ret = IPCFIFO7.Read();

                if (IPCFIFO7.IsEmpty() && (IPCFIFOCnt7 & 0x0004))
                    SetIRQ(1, IRQ_IPCSendDone);
            }
            return ret;
        }
        else
            return IPCFIFO7.Peek();

    case 0x04100010:
        if (!(ExMemCnt[0] & (1<<11))) return NDSCartSlot.ReadROMData();
        return 0;

    case 0x04004000:
    case 0x04004004:
    case 0x04004010:
        // shut up logging for DSi registers
        return 0;

    // NO$GBA debug register "Clock Cycles"
    // Since it's a 64 bit reg. the CPU will access it in two parts:
    case 0x04FFFA20: return (u32)(GetSysClockCycles(0) & 0xFFFFFFFF);
    case 0x04FFFA24: return (u32)(GetSysClockCycles(0) >> 32);
    }

    if ((addr >= 0x04000000 && addr < 0x04000060) || (addr == 0x0400006C))
    {
        return GPU.GPU2D_A.Read32(addr);
    }
    if ((addr >= 0x04001000 && addr < 0x04001060) || (addr == 0x0400106C))
    {
        return GPU.GPU2D_B.Read32(addr);
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        return GPU.GPU3D.Read32(addr);
    }

    if ((addr & 0xFFFFF000) != 0x04004000)
        Log(LogLevel::Debug, "unknown ARM9 IO read32 %08X %08X\n", addr, ARM9.R[15]);
    return 0;
}

void NDS::ARM9IOWrite8(u32 addr, u8 val)
{
    switch (addr)
    {
    case 0x0400006C:
    case 0x0400006D: GPU.GPU2D_A.Write8(addr, val); return;
    case 0x0400106C:
    case 0x0400106D: GPU.GPU2D_B.Write8(addr, val); return;

    case 0x04000132:
        KeyCnt[0] = (KeyCnt[0] & 0xFF00) | val;
        return;
    case 0x04000133:
        KeyCnt[0] = (KeyCnt[0] & 0x00FF) | (val << 8);
        return;

    case 0x04000188:
        NDS::ARM9IOWrite32(addr, val | (val << 8) | (val << 16) | (val << 24));
        return;

    case 0x040001A0:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteSPICnt((NDSCartSlot.GetSPICnt() & 0xFF00) | val);
        return;
    case 0x040001A1:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteSPICnt((NDSCartSlot.GetSPICnt() & 0x00FF) | (val << 8));
        return;
    case 0x040001A2:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteSPIData(val);
        return;

    case 0x040001A4:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0xFFFFFF00) | val);
        return;
    case 0x040001A5:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0xFFFF00FF) | (val << 8));
        return;
    case 0x040001A6:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0xFF00FFFF) | (val << 16));
        return;
    case 0x040001A7:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0x00FFFFFF) | (val << 24));
        return;

    case 0x040001A8: if (!(ExMemCnt[0] & (1<<11))) NDSCartSlot.SetROMCommand(0, val); return;
    case 0x040001A9: if (!(ExMemCnt[0] & (1<<11))) NDSCartSlot.SetROMCommand(1, val); return;
    case 0x040001AA: if (!(ExMemCnt[0] & (1<<11))) NDSCartSlot.SetROMCommand(2, val); return;
    case 0x040001AB: if (!(ExMemCnt[0] & (1<<11))) NDSCartSlot.SetROMCommand(3, val); return;
    case 0x040001AC: if (!(ExMemCnt[0] & (1<<11))) NDSCartSlot.SetROMCommand(4, val); return;
    case 0x040001AD: if (!(ExMemCnt[0] & (1<<11))) NDSCartSlot.SetROMCommand(5, val); return;
    case 0x040001AE: if (!(ExMemCnt[0] & (1<<11))) NDSCartSlot.SetROMCommand(6, val); return;
    case 0x040001AF: if (!(ExMemCnt[0] & (1<<11))) NDSCartSlot.SetROMCommand(7, val); return;

    case 0x04000208: IME[0] = val & 0x1; UpdateIRQ(0); return;

    case 0x04000240: GPU.MapVRAM_AB(0, val); return;
    case 0x04000241: GPU.MapVRAM_AB(1, val); return;
    case 0x04000242: GPU.MapVRAM_CD(2, val); return;
    case 0x04000243: GPU.MapVRAM_CD(3, val); return;
    case 0x04000244: GPU.MapVRAM_E(4, val); return;
    case 0x04000245: GPU.MapVRAM_FG(5, val); return;
    case 0x04000246: GPU.MapVRAM_FG(6, val); return;
    case 0x04000247: MapSharedWRAM(val); return;
    case 0x04000248: GPU.MapVRAM_H(7, val); return;
    case 0x04000249: GPU.MapVRAM_I(8, val); return;

    case 0x04000300:
        if (PostFlag9 & 0x01) val |= 0x01;
        PostFlag9 = val & 0x03;
        return;
    }

    if (addr >= 0x04000000 && addr < 0x04000060)
    {
        GPU.GPU2D_A.Write8(addr, val);
        return;
    }
    if (addr >= 0x04001000 && addr < 0x04001060)
    {
        GPU.GPU2D_B.Write8(addr, val);
        return;
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        GPU.GPU3D.Write8(addr, val);
        return;
    }

    Log(LogLevel::Debug, "unknown ARM9 IO write8 %08X %02X %08X\n", addr, val, ARM9.R[15]);
}

void NDS::ARM9IOWrite16(u32 addr, u16 val)
{
    switch (addr)
    {
    case 0x04000004: GPU.SetDispStat(0, val); return;
    case 0x04000006: GPU.SetVCount(val); return;

    case 0x04000060: GPU.GPU3D.Write16(addr, val); return;

    case 0x04000068:
    case 0x0400006A: GPU.GPU2D_A.Write16(addr, val); return;

    case 0x0400006C: GPU.GPU2D_A.Write16(addr, val); return;
    case 0x0400106C: GPU.GPU2D_B.Write16(addr, val); return;

    case 0x040000B8: DMAs[0].WriteCnt((DMAs[0].Cnt & 0xFFFF0000) | val); return;
    case 0x040000BA: DMAs[0].WriteCnt((DMAs[0].Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000C4: DMAs[1].WriteCnt((DMAs[1].Cnt & 0xFFFF0000) | val); return;
    case 0x040000C6: DMAs[1].WriteCnt((DMAs[1].Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000D0: DMAs[2].WriteCnt((DMAs[2].Cnt & 0xFFFF0000) | val); return;
    case 0x040000D2: DMAs[2].WriteCnt((DMAs[2].Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000DC: DMAs[3].WriteCnt((DMAs[3].Cnt & 0xFFFF0000) | val); return;
    case 0x040000DE: DMAs[3].WriteCnt((DMAs[3].Cnt & 0x0000FFFF) | (val << 16)); return;

    case 0x040000E0: DMA9Fill[0] = (DMA9Fill[0] & 0xFFFF0000) | val; return;
    case 0x040000E2: DMA9Fill[0] = (DMA9Fill[0] & 0x0000FFFF) | (val << 16); return;
    case 0x040000E4: DMA9Fill[1] = (DMA9Fill[1] & 0xFFFF0000) | val; return;
    case 0x040000E6: DMA9Fill[1] = (DMA9Fill[1] & 0x0000FFFF) | (val << 16); return;
    case 0x040000E8: DMA9Fill[2] = (DMA9Fill[2] & 0xFFFF0000) | val; return;
    case 0x040000EA: DMA9Fill[2] = (DMA9Fill[2] & 0x0000FFFF) | (val << 16); return;
    case 0x040000EC: DMA9Fill[3] = (DMA9Fill[3] & 0xFFFF0000) | val; return;
    case 0x040000EE: DMA9Fill[3] = (DMA9Fill[3] & 0x0000FFFF) | (val << 16); return;

    case 0x04000100: Timers[0].Reload = val; return;
    case 0x04000102: TimerStart(0, val); return;
    case 0x04000104: Timers[1].Reload = val; return;
    case 0x04000106: TimerStart(1, val); return;
    case 0x04000108: Timers[2].Reload = val; return;
    case 0x0400010A: TimerStart(2, val); return;
    case 0x0400010C: Timers[3].Reload = val; return;
    case 0x0400010E: TimerStart(3, val); return;

    case 0x04000132:
        KeyCnt[0] = val;
        return;

    case 0x04000180:
        IPCSync7 &= 0xFFF0;
        IPCSync7 |= ((val & 0x0F00) >> 8);
        IPCSync9 &= 0xB0FF;
        IPCSync9 |= (val & 0x4F00);
        if ((val & 0x2000) && (IPCSync7 & 0x4000))
        {
            SetIRQ(1, IRQ_IPCSync);
        }
        return;

    case 0x04000184:
        if (val & 0x0008)
            IPCFIFO9.Clear();
        if ((val & 0x0004) && (!(IPCFIFOCnt9 & 0x0004)) && IPCFIFO9.IsEmpty())
            SetIRQ(0, IRQ_IPCSendDone);
        if ((val & 0x0400) && (!(IPCFIFOCnt9 & 0x0400)) && (!IPCFIFO7.IsEmpty()))
            SetIRQ(0, IRQ_IPCRecv);
        if (val & 0x4000)
            IPCFIFOCnt9 &= ~0x4000;
        IPCFIFOCnt9 = (val & 0x8404) | (IPCFIFOCnt9 & 0x4000);
        return;

    case 0x04000188:
        NDS::ARM9IOWrite32(addr, val | (val << 16));
        return;

    case 0x040001A0:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteSPICnt(val);
        return;
    case 0x040001A2:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteSPIData(val & 0xFF);
        return;

    case 0x040001A4:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0xFFFF0000) | val);
        return;
    case 0x040001A6:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0x0000FFFF) | (val << 16));
        return;

    case 0x040001A8:
        if (!(ExMemCnt[0] & (1<<11)))
        {
            NDSCartSlot.SetROMCommand(0, val & 0xFF);
            NDSCartSlot.SetROMCommand(1, val >> 8);
        }
        return;
    case 0x040001AA:
        if (!(ExMemCnt[0] & (1<<11)))
        {
            NDSCartSlot.SetROMCommand(2, val & 0xFF);
            NDSCartSlot.SetROMCommand(3, val >> 8);
        }
        return;
    case 0x040001AC:
        if (!(ExMemCnt[0] & (1<<11)))
        {
            NDSCartSlot.SetROMCommand(4, val & 0xFF);
            NDSCartSlot.SetROMCommand(5, val >> 8);
        }
        return;
    case 0x040001AE:
        if (!(ExMemCnt[0] & (1<<11)))
        {
            NDSCartSlot.SetROMCommand(6, val & 0xFF);
            NDSCartSlot.SetROMCommand(7, val >> 8);
        }
        return;

    case 0x040001B8: ROMSeed0[4] = val & 0x7F; return;
    case 0x040001BA: ROMSeed1[4] = val & 0x7F; return;

    case 0x04000204:
        {
            u16 oldVal = ExMemCnt[0];
            ExMemCnt[0] = val;
            ExMemCnt[1] = (ExMemCnt[1] & 0x007F) | (val & 0xFF80);
            if ((oldVal ^ ExMemCnt[0]) & 0xFF)
                SetGBASlotTimings();
            return;
        }

    case 0x04000208: IME[0] = val & 0x1; UpdateIRQ(0); return;
    case 0x04000210: IE[0] = (IE[0] & 0xFFFF0000) | val; UpdateIRQ(0); return;
    case 0x04000212: IE[0] = (IE[0] & 0x0000FFFF) | (val << 16); UpdateIRQ(0); return;
    // TODO: what happens when writing to IF this way??

    case 0x04000240:
        GPU.MapVRAM_AB(0, val & 0xFF);
        GPU.MapVRAM_AB(1, val >> 8);
        return;
    case 0x04000242:
        GPU.MapVRAM_CD(2, val & 0xFF);
        GPU.MapVRAM_CD(3, val >> 8);
        return;
    case 0x04000244:
        GPU.MapVRAM_E(4, val & 0xFF);
        GPU.MapVRAM_FG(5, val >> 8);
        return;
    case 0x04000246:
        GPU.MapVRAM_FG(6, val & 0xFF);
        MapSharedWRAM(val >> 8);
        return;
    case 0x04000248:
        GPU.MapVRAM_H(7, val & 0xFF);
        GPU.MapVRAM_I(8, val >> 8);
        return;

    case 0x04000280: DivCnt = val; StartDiv(); return;

    case 0x040002B0: SqrtCnt = val; StartSqrt(); return;

    case 0x04000300:
        if (PostFlag9 & 0x01) val |= 0x01;
        PostFlag9 = val & 0x03;
        return;

    case 0x04000304:
        PowerControl9 = val & 0x820F;
        GPU.SetPowerCnt(PowerControl9);
        return;
    }

    if (addr >= 0x04000000 && addr < 0x04000060)
    {
        GPU.GPU2D_A.Write16(addr, val);
        return;
    }
    if (addr >= 0x04001000 && addr < 0x04001060)
    {
        GPU.GPU2D_B.Write16(addr, val);
        return;
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        GPU.GPU3D.Write16(addr, val);
        return;
    }

    Log(LogLevel::Debug, "unknown ARM9 IO write16 %08X %04X %08X\n", addr, val, ARM9.R[15]);
}

void NDS::ARM9IOWrite32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x04000004:
        GPU.SetDispStat(0, val & 0xFFFF);
        GPU.SetVCount(val >> 16);
        return;

    case 0x04000060: GPU.GPU3D.Write32(addr, val); return;
    case 0x04000064:
    case 0x04000068: GPU.GPU2D_A.Write32(addr, val); return;

    case 0x0400006C: GPU.GPU2D_A.Write16(addr, val&0xFFFF); return;
    case 0x0400106C: GPU.GPU2D_B.Write16(addr, val&0xFFFF); return;

    case 0x040000B0: DMAs[0].SrcAddr = val; return;
    case 0x040000B4: DMAs[0].DstAddr = val; return;
    case 0x040000B8: DMAs[0].WriteCnt(val); return;
    case 0x040000BC: DMAs[1].SrcAddr = val; return;
    case 0x040000C0: DMAs[1].DstAddr = val; return;
    case 0x040000C4: DMAs[1].WriteCnt(val); return;
    case 0x040000C8: DMAs[2].SrcAddr = val; return;
    case 0x040000CC: DMAs[2].DstAddr = val; return;
    case 0x040000D0: DMAs[2].WriteCnt(val); return;
    case 0x040000D4: DMAs[3].SrcAddr = val; return;
    case 0x040000D8: DMAs[3].DstAddr = val; return;
    case 0x040000DC: DMAs[3].WriteCnt(val); return;

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

    case 0x04000130:
        KeyCnt[0] = val >> 16;
        return;

    case 0x04000180:
    case 0x04000184:
        NDS::ARM9IOWrite16(addr, val);
        return;
    case 0x04000188:
        if (IPCFIFOCnt9 & 0x8000)
        {
            if (IPCFIFO9.IsFull())
                IPCFIFOCnt9 |= 0x4000;
            else
            {
                bool wasempty = IPCFIFO9.IsEmpty();
                IPCFIFO9.Write(val);
                if ((IPCFIFOCnt7 & 0x0400) && wasempty)
                    SetIRQ(1, IRQ_IPCRecv);
            }
        }
        return;

    case 0x040001A0:
        if (!(ExMemCnt[0] & (1<<11)))
        {
            NDSCartSlot.WriteSPICnt(val & 0xFFFF);
            NDSCartSlot.WriteSPIData((val >> 16) & 0xFF);
        }
        return;
    case 0x040001A4:
        if (!(ExMemCnt[0] & (1<<11)))
            NDSCartSlot.WriteROMCnt(val);
        return;

    case 0x040001A8:
        if (!(ExMemCnt[0] & (1<<11)))
        {
            NDSCartSlot.SetROMCommand(0, val & 0xFF);
            NDSCartSlot.SetROMCommand(1, (val >> 8) & 0xFF);
            NDSCartSlot.SetROMCommand(2, (val >> 16) & 0xFF);
            NDSCartSlot.SetROMCommand(3, val >> 24);
        }
        return;
    case 0x040001AC:
        if (!(ExMemCnt[0] & (1<<11)))
        {
            NDSCartSlot.SetROMCommand(4, val & 0xFF);
            NDSCartSlot.SetROMCommand(5, (val >> 8) & 0xFF);
            NDSCartSlot.SetROMCommand(6, (val >> 16) & 0xFF);
            NDSCartSlot.SetROMCommand(7, val >> 24);
        }
        return;

    case 0x040001B0: *(u32*)&ROMSeed0[0] = val; return;
    case 0x040001B4: *(u32*)&ROMSeed1[0] = val; return;

    case 0x04000208: IME[0] = val & 0x1; UpdateIRQ(0); return;
    case 0x04000210: IE[0] = val; UpdateIRQ(0); return;
    case 0x04000214: IF[0] &= ~val; GPU.GPU3D.CheckFIFOIRQ(); UpdateIRQ(0); return;

    case 0x04000240:
        GPU.MapVRAM_AB(0, val & 0xFF);
        GPU.MapVRAM_AB(1, (val >> 8) & 0xFF);
        GPU.MapVRAM_CD(2, (val >> 16) & 0xFF);
        GPU.MapVRAM_CD(3, val >> 24);
        return;
    case 0x04000244:
        GPU.MapVRAM_E(4, val & 0xFF);
        GPU.MapVRAM_FG(5, (val >> 8) & 0xFF);
        GPU.MapVRAM_FG(6, (val >> 16) & 0xFF);
        MapSharedWRAM(val >> 24);
        return;
    case 0x04000248:
        GPU.MapVRAM_H(7, val & 0xFF);
        GPU.MapVRAM_I(8, (val >> 8) & 0xFF);
        return;

    case 0x04000280: DivCnt = val; StartDiv(); return;

    case 0x040002B0: SqrtCnt = val; StartSqrt(); return;

    case 0x04000290: DivNumerator[0] = val; StartDiv(); return;
    case 0x04000294: DivNumerator[1] = val; StartDiv(); return;
    case 0x04000298: DivDenominator[0] = val; StartDiv(); return;
    case 0x0400029C: DivDenominator[1] = val; StartDiv(); return;

    case 0x040002B8: SqrtVal[0] = val; StartSqrt(); return;
    case 0x040002BC: SqrtVal[1] = val; StartSqrt(); return;

    case 0x04000304:
        PowerControl9 = val & 0x820F;
        GPU.SetPowerCnt(PowerControl9);
        return;

    case 0x04100010:
        if (!(ExMemCnt[0] & (1<<11)))  NDSCartSlot.WriteROMData(val);
        return;

    // NO$GBA debug register "String Out (raw)"
    case 0x04FFFA10:
        {
            char output[1024] = { 0 };
            char ch = '.';
            for (size_t i = 0; i < 1023 && ch != '\0'; i++)
            {
                ch = NDS::ARM9Read8(val + i);
                output[i] = ch;
            }
            Log(LogLevel::Debug, "%s", output);
            return;
        }

    // NO$GBA debug registers "String Out (with parameters)" and "String Out (with parameters, plus linefeed)"
    case 0x04FFFA14:
    case 0x04FFFA18:
        {
            bool appendLF = 0x04FFFA18 == addr;
            NocashPrint(0, val);
            if(appendLF)
                Log(LogLevel::Debug, "\n");
            return;
        }

    // NO$GBA debug register "Char Out"
        case 0x04FFFA1C: Log(LogLevel::Debug, "%c", val & 0xFF); return;
    }

    if (addr >= 0x04000000 && addr < 0x04000060)
    {
        GPU.GPU2D_A.Write32(addr, val);
        return;
    }
    if (addr >= 0x04001000 && addr < 0x04001060)
    {
        GPU.GPU2D_B.Write32(addr, val);
        return;
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        GPU.GPU3D.Write32(addr, val);
        return;
    }

    Log(LogLevel::Debug, "unknown ARM9 IO write32 %08X %08X %08X\n", addr, val, ARM9.R[15]);
}


u8 NDS::ARM7IORead8(u32 addr)
{
    switch (addr)
    {
    case 0x04000130: return KeyInput & 0xFF;
    case 0x04000131: return (KeyInput >> 8) & 0xFF;
    case 0x04000132: return KeyCnt[1] & 0xFF;
    case 0x04000133: return KeyCnt[1] >> 8;
    case 0x04000134: return RCnt & 0xFF;
    case 0x04000135: return RCnt >> 8;
    case 0x04000136: return (KeyInput >> 16) & 0xFF;
    case 0x04000137: return KeyInput >> 24;

    case 0x04000138: return RTC.Read() & 0xFF;

    case 0x040001A0:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetSPICnt() & 0xFF;
        return 0;
    case 0x040001A1:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetSPICnt() >> 8;
        return 0;

    case 0x040001A2:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.ReadSPIData();
        return 0;

    case 0x040001A4:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCnt() & 0xFF;
        return 0;
    case 0x040001A5:
        if (ExMemCnt[0] & (1<<11))
            return (NDSCartSlot.GetROMCnt() >> 8) & 0xFF;
        return 0;
    case 0x040001A6:
        if (ExMemCnt[0] & (1<<11))
            return (NDSCartSlot.GetROMCnt() >> 16) & 0xFF;
        return 0;
    case 0x040001A7:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCnt() >> 24;
        return 0;

    case 0x040001A8:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(0);
        return 0;
    case 0x040001A9:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(1);
        return 0;
    case 0x040001AA:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(2);
        return 0;
    case 0x040001AB:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(3);
        return 0;
    case 0x040001AC:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(4);
        return 0;
    case 0x040001AD:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(5);
        return 0;
    case 0x040001AE:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(6);
        return 0;
    case 0x040001AF:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(7);
        return 0;

    case 0x040001C2: return SPI.ReadData();

    case 0x04000208: return IME[1];

    case 0x04000240: return GPU.VRAMSTAT;
    case 0x04000241: return WRAMCnt;

    case 0x04000300: return PostFlag7;
    case 0x04000304: return PowerControl7;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        return SPU.Read8(addr);
    }

    if ((addr & 0xFFFFF000) != 0x04004000)
        Log(LogLevel::Debug, "unknown ARM7 IO read8 %08X %08X\n", addr, ARM7.R[15]);
    return 0;
}

u16 NDS::ARM7IORead16(u32 addr)
{
    switch (addr)
    {
    case 0x04000004: return GPU.DispStat[1];
    case 0x04000006: return GPU.VCount;

    case 0x040000B8: return DMAs[4].Cnt & 0xFFFF;
    case 0x040000BA: return DMAs[4].Cnt >> 16;
    case 0x040000C4: return DMAs[5].Cnt & 0xFFFF;
    case 0x040000C6: return DMAs[5].Cnt >> 16;
    case 0x040000D0: return DMAs[6].Cnt & 0xFFFF;
    case 0x040000D2: return DMAs[6].Cnt >> 16;
    case 0x040000DC: return DMAs[7].Cnt & 0xFFFF;
    case 0x040000DE: return DMAs[7].Cnt >> 16;

    case 0x04000100: return TimerGetCounter(4);
    case 0x04000102: return Timers[4].Cnt;
    case 0x04000104: return TimerGetCounter(5);
    case 0x04000106: return Timers[5].Cnt;
    case 0x04000108: return TimerGetCounter(6);
    case 0x0400010A: return Timers[6].Cnt;
    case 0x0400010C: return TimerGetCounter(7);
    case 0x0400010E: return Timers[7].Cnt;

    case 0x04000130: return KeyInput & 0xFFFF;
    case 0x04000132: return KeyCnt[1];
    case 0x04000134: return RCnt;
    case 0x04000136: return KeyInput >> 16;

    case 0x04000138: return RTC.Read();

    case 0x04000180: return IPCSync7;
    case 0x04000184:
        {
            u16 val = IPCFIFOCnt7;
            if (IPCFIFO7.IsEmpty())     val |= 0x0001;
            else if (IPCFIFO7.IsFull()) val |= 0x0002;
            if (IPCFIFO9.IsEmpty())     val |= 0x0100;
            else if (IPCFIFO9.IsFull()) val |= 0x0200;
            return val;
        }

    case 0x040001A0: if (ExMemCnt[0] & (1<<11)) return NDSCartSlot.GetSPICnt();   return 0;
    case 0x040001A2: if (ExMemCnt[0] & (1<<11)) return NDSCartSlot.ReadSPIData(); return 0;

    case 0x040001A4:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCnt() & 0xFFFF;
        return 0;
    case 0x040001A6:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCnt() >> 16;
        return 0;

    case 0x040001A8:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(0) |
                  (NDSCartSlot.GetROMCommand(1) << 8);
        return 0;
    case 0x040001AA:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(2) |
                  (NDSCartSlot.GetROMCommand(3) << 8);
        return 0;
    case 0x040001AC:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(4) |
                  (NDSCartSlot.GetROMCommand(5) << 8);
        return 0;
    case 0x040001AE:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(6) |
                  (NDSCartSlot.GetROMCommand(7) << 8);
        return 0;

    case 0x040001C0: return SPI.ReadCnt();
    case 0x040001C2: return SPI.ReadData();

    case 0x04000204: return ExMemCnt[1];
    case 0x04000206:
        if (!(PowerControl7 & (1<<1))) return 0;
        return WifiWaitCnt;

    case 0x04000208: return IME[1];
    case 0x04000210: return IE[1] & 0xFFFF;
    case 0x04000212: return IE[1] >> 16;

    case 0x04000300: return PostFlag7;
    case 0x04000304: return PowerControl7;
    case 0x04000308: return ARM7BIOSProt;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        return SPU.Read16(addr);
    }

    if ((addr & 0xFFFFF000) != 0x04004000)
        Log(LogLevel::Debug, "unknown ARM7 IO read16 %08X %08X\n", addr, ARM7.R[15]);
    return 0;
}

u32 NDS::ARM7IORead32(u32 addr)
{
    switch (addr)
    {
    case 0x04000004: return GPU.DispStat[1] | (GPU.VCount << 16);

    case 0x040000B0: return DMAs[4].SrcAddr;
    case 0x040000B4: return DMAs[4].DstAddr;
    case 0x040000B8: return DMAs[4].Cnt;
    case 0x040000BC: return DMAs[5].SrcAddr;
    case 0x040000C0: return DMAs[5].DstAddr;
    case 0x040000C4: return DMAs[5].Cnt;
    case 0x040000C8: return DMAs[6].SrcAddr;
    case 0x040000CC: return DMAs[6].DstAddr;
    case 0x040000D0: return DMAs[6].Cnt;
    case 0x040000D4: return DMAs[7].SrcAddr;
    case 0x040000D8: return DMAs[7].DstAddr;
    case 0x040000DC: return DMAs[7].Cnt;

    case 0x04000100: return TimerGetCounter(4) | (Timers[4].Cnt << 16);
    case 0x04000104: return TimerGetCounter(5) | (Timers[5].Cnt << 16);
    case 0x04000108: return TimerGetCounter(6) | (Timers[6].Cnt << 16);
    case 0x0400010C: return TimerGetCounter(7) | (Timers[7].Cnt << 16);

    case 0x04000130: return (KeyInput & 0xFFFF) | (KeyCnt[1] << 16);
    case 0x04000134: return RCnt | (KeyInput & 0xFFFF0000);
    case 0x04000138: return RTC.Read();

    case 0x04000180: return IPCSync7;
    case 0x04000184: return NDS::ARM7IORead16(addr);

    case 0x040001A0:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetSPICnt() | (NDSCartSlot.ReadSPIData() << 16);
        return 0;
    case 0x040001A4:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCnt();
        return 0;

    case 0x040001A8:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(0) |
                  (NDSCartSlot.GetROMCommand(1) << 8) |
                  (NDSCartSlot.GetROMCommand(2) << 16) |
                  (NDSCartSlot.GetROMCommand(3) << 24);
        return 0;
    case 0x040001AC:
        if (ExMemCnt[0] & (1<<11))
            return NDSCartSlot.GetROMCommand(4) |
                  (NDSCartSlot.GetROMCommand(5) << 8) |
                  (NDSCartSlot.GetROMCommand(6) << 16) |
                  (NDSCartSlot.GetROMCommand(7) << 24);
        return 0;

    case 0x040001C0:
        return SPI.ReadCnt() | (SPI.ReadData() << 16);

    case 0x04000208: return IME[1];
    case 0x04000210: return IE[1];
    case 0x04000214: return IF[1];

    case 0x04000304: return PowerControl7;
    case 0x04000308: return ARM7BIOSProt;

    case 0x04100000:
        if (IPCFIFOCnt7 & 0x8000)
        {
            u32 ret;
            if (IPCFIFO9.IsEmpty())
            {
                IPCFIFOCnt7 |= 0x4000;
                ret = IPCFIFO9.Peek();
            }
            else
            {
                ret = IPCFIFO9.Read();

                if (IPCFIFO9.IsEmpty() && (IPCFIFOCnt9 & 0x0004))
                    SetIRQ(0, IRQ_IPCSendDone);
            }
            return ret;
        }
        else
            return IPCFIFO9.Peek();

    case 0x04100010:
        if (ExMemCnt[0] & (1<<11)) return NDSCartSlot.ReadROMData();
        return 0;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        return SPU.Read32(addr);
    }

    if ((addr & 0xFFFFF000) != 0x04004000)
        Log(LogLevel::Debug, "unknown ARM7 IO read32 %08X %08X\n", addr, ARM7.R[15]);
    return 0;
}

void NDS::ARM7IOWrite8(u32 addr, u8 val)
{
    switch (addr)
    {
    case 0x04000132:
        KeyCnt[1] = (KeyCnt[1] & 0xFF00) | val;
        return;
    case 0x04000133:
        KeyCnt[1] = (KeyCnt[1] & 0x00FF) | (val << 8);
        return;
    case 0x04000134:
        RCnt = (RCnt & 0xFF00) | val;
        return;
    case 0x04000135:
        RCnt = (RCnt & 0x00FF) | (val << 8);
        return;

    case 0x04000138: RTC.Write(val, true); return;

    case 0x04000188:
        NDS::ARM7IOWrite32(addr, val | (val << 8) | (val << 16) | (val << 24));
        return;

    case 0x040001A0:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCartSlot.WriteSPICnt((NDSCartSlot.GetSPICnt() & 0xFF00) | val);
        }
        return;
    case 0x040001A1:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteSPICnt((NDSCartSlot.GetSPICnt() & 0x00FF) | (val << 8));
        return;
    case 0x040001A2:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteSPIData(val);
        return;

    case 0x040001A4:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0xFFFFFF00) | val);
        return;
    case 0x040001A5:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0xFFFF00FF) | (val << 8));
        return;
    case 0x040001A6:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0xFF00FFFF) | (val << 16));
        return;
    case 0x040001A7:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0x00FFFFFF) | (val << 24));
        return;

    case 0x040001A8: if (ExMemCnt[0] & (1<<11)) NDSCartSlot.SetROMCommand(0, val); return;
    case 0x040001A9: if (ExMemCnt[0] & (1<<11)) NDSCartSlot.SetROMCommand(1, val); return;
    case 0x040001AA: if (ExMemCnt[0] & (1<<11)) NDSCartSlot.SetROMCommand(2, val); return;
    case 0x040001AB: if (ExMemCnt[0] & (1<<11)) NDSCartSlot.SetROMCommand(3, val); return;
    case 0x040001AC: if (ExMemCnt[0] & (1<<11)) NDSCartSlot.SetROMCommand(4, val); return;
    case 0x040001AD: if (ExMemCnt[0] & (1<<11)) NDSCartSlot.SetROMCommand(5, val); return;
    case 0x040001AE: if (ExMemCnt[0] & (1<<11)) NDSCartSlot.SetROMCommand(6, val); return;
    case 0x040001AF: if (ExMemCnt[0] & (1<<11)) NDSCartSlot.SetROMCommand(7, val); return;

    case 0x040001C2:
        SPI.WriteData(val);
        return;

    case 0x04000208: IME[1] = val & 0x1; UpdateIRQ(1); return;

    case 0x04000300:
        if (ARM7.R[15] >= 0x4000)
            return;
        if (!(PostFlag7 & 0x01))
            PostFlag7 = val & 0x01;
        return;

    case 0x04000301:
        val &= 0xC0;
        if      (val == 0x40) Stop(StopReason::GBAModeNotSupported);
        else if (val == 0x80) ARM7.Halt(1);
        else if (val == 0xC0) EnterSleepMode();
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        SPU.Write8(addr, val);
        return;
    }

    Log(LogLevel::Debug, "unknown ARM7 IO write8 %08X %02X %08X\n", addr, val, ARM7.R[15]);
}

void NDS::ARM7IOWrite16(u32 addr, u16 val)
{
    switch (addr)
    {
    case 0x04000004: GPU.SetDispStat(1, val); return;
    case 0x04000006: GPU.SetVCount(val); return;

    case 0x040000B8: DMAs[4].WriteCnt((DMAs[4].Cnt & 0xFFFF0000) | val); return;
    case 0x040000BA: DMAs[4].WriteCnt((DMAs[4].Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000C4: DMAs[5].WriteCnt((DMAs[5].Cnt & 0xFFFF0000) | val); return;
    case 0x040000C6: DMAs[5].WriteCnt((DMAs[5].Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000D0: DMAs[6].WriteCnt((DMAs[6].Cnt & 0xFFFF0000) | val); return;
    case 0x040000D2: DMAs[6].WriteCnt((DMAs[6].Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000DC: DMAs[7].WriteCnt((DMAs[7].Cnt & 0xFFFF0000) | val); return;
    case 0x040000DE: DMAs[7].WriteCnt((DMAs[7].Cnt & 0x0000FFFF) | (val << 16)); return;

    case 0x04000100: Timers[4].Reload = val; return;
    case 0x04000102: TimerStart(4, val); return;
    case 0x04000104: Timers[5].Reload = val; return;
    case 0x04000106: TimerStart(5, val); return;
    case 0x04000108: Timers[6].Reload = val; return;
    case 0x0400010A: TimerStart(6, val); return;
    case 0x0400010C: Timers[7].Reload = val; return;
    case 0x0400010E: TimerStart(7, val); return;

    case 0x04000132: KeyCnt[1] = val; return;
    case 0x04000134: RCnt = val; return;

    case 0x04000138: RTC.Write(val, false); return;

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
            IPCFIFO7.Clear();
        if ((val & 0x0004) && (!(IPCFIFOCnt7 & 0x0004)) && IPCFIFO7.IsEmpty())
            SetIRQ(1, IRQ_IPCSendDone);
        if ((val & 0x0400) && (!(IPCFIFOCnt7 & 0x0400)) && (!IPCFIFO9.IsEmpty()))
            SetIRQ(1, IRQ_IPCRecv);
        if (val & 0x4000)
            IPCFIFOCnt7 &= ~0x4000;
        IPCFIFOCnt7 = (val & 0x8404) | (IPCFIFOCnt7 & 0x4000);
        return;

    case 0x04000188:
        NDS::ARM7IOWrite32(addr, val | (val << 16));
        return;

    case 0x040001A0:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteSPICnt(val);
        return;
    case 0x040001A2:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteSPIData(val & 0xFF);
        return;

    case 0x040001A4:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0xFFFFFF00) | val);
        return;
    case 0x040001A6:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteROMCnt((NDSCartSlot.GetROMCnt() & 0xFF00FFFF) | (val << 16));
        return;

    case 0x040001A8:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCartSlot.SetROMCommand(0, val & 0xFF);
            NDSCartSlot.SetROMCommand(1, val >> 8);
        }
        return;
    case 0x040001AA:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCartSlot.SetROMCommand(2, val & 0xFF);
            NDSCartSlot.SetROMCommand(3, val >> 8);
        }
        return;
    case 0x040001AC:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCartSlot.SetROMCommand(4, val & 0xFF);
            NDSCartSlot.SetROMCommand(5, val >> 8);
        }
        return;
    case 0x040001AE:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCartSlot.SetROMCommand(6, val & 0xFF);
            NDSCartSlot.SetROMCommand(7, val >> 8);
        }
        return;

    case 0x040001B8: ROMSeed0[12] = val & 0x7F; return;
    case 0x040001BA: ROMSeed1[12] = val & 0x7F; return;

    case 0x040001C0:
        SPI.WriteCnt(val);
        return;
    case 0x040001C2:
        SPI.WriteData(val & 0xFF);
        return;

    case 0x04000204:
        {
            u16 oldVal = ExMemCnt[1];
            ExMemCnt[1] = (ExMemCnt[1] & 0xFF80) | (val & 0x007F);
            if ((ExMemCnt[1] ^ oldVal) & 0xFF)
                SetGBASlotTimings();
            return;
        }
    case 0x04000206:
        if (!(PowerControl7 & (1<<1))) return;
        SetWifiWaitCnt(val);
        return;

    case 0x04000208: IME[1] = val & 0x1; UpdateIRQ(1); return;
    case 0x04000210: IE[1] = (IE[1] & 0xFFFF0000) | val; UpdateIRQ(1); return;
    case 0x04000212: IE[1] = (IE[1] & 0x0000FFFF) | (val << 16); UpdateIRQ(1); return;
    // TODO: what happens when writing to IF this way??

    case 0x04000300:
        if (ARM7.R[15] >= 0x4000)
            return;
        if (!(PostFlag7 & 0x01))
            PostFlag7 = val & 0x01;
        return;

    case 0x04000304:
        {
            u16 change = PowerControl7 ^ val;
            PowerControl7 = val & 0x0003;
            SPU.SetPowerCnt(val & 0x0001);
            Wifi.SetPowerCnt(val & 0x0002);
            if (change & 0x0002) UpdateWifiTimings();
        }
        return;

    case 0x04000308:
        if (ARM7BIOSProt == 0)
            ARM7BIOSProt = val & 0xFFFE;
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        SPU.Write16(addr, val);
        return;
    }

    Log(LogLevel::Debug, "unknown ARM7 IO write16 %08X %04X %08X\n", addr, val, ARM7.R[15]);
}

void NDS::ARM7IOWrite32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x04000004:
        GPU.SetDispStat(1, val & 0xFFFF);
        GPU.SetVCount(val >> 16);
        return;

    case 0x040000B0: DMAs[4].SrcAddr = val; return;
    case 0x040000B4: DMAs[4].DstAddr = val; return;
    case 0x040000B8: DMAs[4].WriteCnt(val); return;
    case 0x040000BC: DMAs[5].SrcAddr = val; return;
    case 0x040000C0: DMAs[5].DstAddr = val; return;
    case 0x040000C4: DMAs[5].WriteCnt(val); return;
    case 0x040000C8: DMAs[6].SrcAddr = val; return;
    case 0x040000CC: DMAs[6].DstAddr = val; return;
    case 0x040000D0: DMAs[6].WriteCnt(val); return;
    case 0x040000D4: DMAs[7].SrcAddr = val; return;
    case 0x040000D8: DMAs[7].DstAddr = val; return;
    case 0x040000DC: DMAs[7].WriteCnt(val); return;

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

    case 0x04000130: KeyCnt[1] = val >> 16; return;
    case 0x04000134: RCnt = val & 0xFFFF; return;
    case 0x04000138: RTC.Write(val & 0xFFFF, false); return;

    case 0x04000180:
    case 0x04000184:
        NDS::ARM7IOWrite16(addr, val);
        return;
    case 0x04000188:
        if (IPCFIFOCnt7 & 0x8000)
        {
            if (IPCFIFO7.IsFull())
                IPCFIFOCnt7 |= 0x4000;
            else
            {
                bool wasempty = IPCFIFO7.IsEmpty();
                IPCFIFO7.Write(val);
                if ((IPCFIFOCnt9 & 0x0400) && wasempty)
                    SetIRQ(0, IRQ_IPCRecv);
            }
        }
        return;

    case 0x040001A0:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCartSlot.WriteSPICnt(val & 0xFFFF);
            NDSCartSlot.WriteSPIData((val >> 16) & 0xFF);
        }
        return;
    case 0x040001A4:
        if (ExMemCnt[0] & (1<<11))
            NDSCartSlot.WriteROMCnt(val);
        return;

    case 0x040001A8:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCartSlot.SetROMCommand(0, val & 0xFF);
            NDSCartSlot.SetROMCommand(1, (val >> 8) & 0xFF);
            NDSCartSlot.SetROMCommand(2, (val >> 16) & 0xFF);
            NDSCartSlot.SetROMCommand(3, val >> 24);
        }
        return;
    case 0x040001AC:
        if (ExMemCnt[0] & (1<<11))
        {
            NDSCartSlot.SetROMCommand(4, val & 0xFF);
            NDSCartSlot.SetROMCommand(5, (val >> 8) & 0xFF);
            NDSCartSlot.SetROMCommand(6, (val >> 16) & 0xFF);
            NDSCartSlot.SetROMCommand(7, val >> 24);
        }
        return;

    case 0x040001B0: *(u32*)&ROMSeed0[8] = val; return;
    case 0x040001B4: *(u32*)&ROMSeed1[8] = val; return;

    case 0x040001C0:
        SPI.WriteCnt(val & 0xFFFF);
        SPI.WriteData((val >> 16) & 0xFF);
        return;

    case 0x04000208: IME[1] = val & 0x1; UpdateIRQ(1); return;
    case 0x04000210: IE[1] = val; UpdateIRQ(1); return;
    case 0x04000214: IF[1] &= ~val; UpdateIRQ(1); return;

    case 0x04000304:
        {
            u16 change = PowerControl7 ^ val;
            PowerControl7 = val & 0x0003;
            SPU.SetPowerCnt(val & 0x0001);
            Wifi.SetPowerCnt(val & 0x0002);
            if (change & 0x0002) UpdateWifiTimings();
        }
        return;

    case 0x04000308:
        if (ARM7BIOSProt == 0)
            ARM7BIOSProt = val & 0xFFFE;
        return;

    case 0x04100010:
        if (ExMemCnt[0] & (1<<11))  NDSCartSlot.WriteROMData(val);
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        SPU.Write32(addr, val);
        return;
    }

    Log(LogLevel::Debug, "unknown ARM7 IO write32 %08X %08X %08X\n", addr, val, ARM7.R[15]);
}

}
