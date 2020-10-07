/*
    Copyright 2016-2020 Arisotura

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
#include "Config.h"
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

#ifdef JIT_ENABLED
#include "ARMJIT.h"
#include "ARMJIT_Memory.h"
#endif

#include "DSi.h"
#include "DSi_SPI_TSC.h"


namespace NDS
{

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

int ConsoleType;

u8 ARM9MemTimings[0x40000][4];
u8 ARM7MemTimings[0x20000][4];

ARMv5* ARM9;
ARMv4* ARM7;

u32 NumFrames;
u64 LastSysClockCycles;
u64 FrameStartTimestamp;

int CurCPU;

const s32 kMaxIterationCycles = 64;

u32 ARM9ClockShift;

// no need to worry about those overflowing, they can keep going for atleast 4350 years
u64 ARM9Timestamp, ARM9Target;
u64 ARM7Timestamp, ARM7Target;
u64 SysTimestamp;

SchedEvent SchedList[Event_MAX];
u32 SchedListMask;

u32 CPUStop;

u8 ARM9BIOS[0x1000];
u8 ARM7BIOS[0x4000];

u8* MainRAM;
u32 MainRAMMask;

u8* SharedWRAM;
u8 WRAMCnt;

// putting them together so they're always next to each other
MemRegion SWRAM_ARM9;
MemRegion SWRAM_ARM7;

u8* ARM7WRAM;

u16 ExMemCnt[2];

// TODO: these belong in NDSCart!
u8 ROMSeed0[2*8];
u8 ROMSeed1[2*8];

// IO shit
u32 IME[2];
u32 IE[2], IF[2];
u32 IE2, IF2;

u8 PostFlag9;
u8 PostFlag7;
u16 PowerControl9;
u16 PowerControl7;

u16 WifiWaitCnt;

u16 ARM7BIOSProt;

Timer Timers[8];
u8 TimerCheckMask[2];
u64 TimerTimestamp[2];

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
u16 KeyCnt;
u16 RCnt;

bool Running;

bool RunningGame;


void DivDone(u32 param);
void SqrtDone(u32 param);
void RunTimer(u32 tid, s32 cycles);
void SetWifiWaitCnt(u16 val);
void SetGBASlotTimings();


bool Init()
{
    ARM9 = new ARMv5();
    ARM7 = new ARMv4();

#ifdef JIT_ENABLED
    ARMJIT::Init();
#else
    MainRAM = new u8[0x1000000];
    ARM7WRAM = new u8[ARM7WRAMSize];
    SharedWRAM = new u8[SharedWRAMSize];
#endif

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
    if (!GBACart::Init()) return false;
    if (!GPU::Init()) return false;
    if (!SPU::Init()) return false;
    if (!SPI::Init()) return false;
    if (!RTC::Init()) return false;
    if (!Wifi::Init()) return false;

    if (!DSi::Init()) return false;

    if (!AREngine::Init()) return false;

    return true;
}

void DeInit()
{
    delete ARM9;
    delete ARM7;

#ifdef JIT_ENABLED
    ARMJIT::DeInit();
#endif

    for (int i = 0; i < 8; i++)
        delete DMAs[i];

    delete IPCFIFO9;
    delete IPCFIFO7;

    NDSCart::DeInit();
    GBACart::DeInit();
    GPU::DeInit();
    SPU::DeInit();
    SPI::DeInit();
    RTC::DeInit();
    Wifi::DeInit();

    DSi::DeInit();

    AREngine::DeInit();
}


void SetARM9RegionTimings(u32 addrstart, u32 addrend, int buswidth, int nonseq, int seq)
{
    addrstart >>= 14;
    addrend   >>= 14;

    if (addrend == 0x3FFFF) addrend++;

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
        ARM9MemTimings[i][0] = N16;
        ARM9MemTimings[i][1] = S16;
        ARM9MemTimings[i][2] = N32;
        ARM9MemTimings[i][3] = S32;
    }

    ARM9->UpdateRegionTimings(addrstart<<14, addrend == 0x40000
        ? 0xFFFFFFFF
        : (addrend<<14));
}

void SetARM7RegionTimings(u32 addrstart, u32 addrend, int buswidth, int nonseq, int seq)
{
    addrstart >>= 15;
    addrend   >>= 15;

    if (addrend == 0x1FFFF) addrend++;

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
        ARM7MemTimings[i][0] = N16;
        ARM7MemTimings[i][1] = S16;
        ARM7MemTimings[i][2] = N32;
        ARM7MemTimings[i][3] = S32;
    }
}

void InitTimings()
{
    // TODO, eventually:
    // VRAM is initially unmapped. The timings should be those of void regions.
    // Similarly for any unmapped VRAM area.
    // Need to check whether supporting these timing characteristics would impact performance
    // (especially wrt VRAM mirroring and overlapping and whatnot).

    // ARM9
    // TODO: +3c nonseq waitstate doesn't apply to DMA!
    // but of course mainRAM always gets 8c nonseq waitstate

    // TODO: DSi-specific timings!!

    SetARM9RegionTimings(0x00000000, 0xFFFFFFFF, 32, 1 + 3, 1); // void

    SetARM9RegionTimings(0xFFFF0000, 0xFFFFFFFF, 32, 1 + 3, 1); // BIOS
    SetARM9RegionTimings(0x02000000, 0x03000000, 16, 8, 1);     // main RAM
    SetARM9RegionTimings(0x03000000, 0x04000000, 32, 1 + 3, 1); // ARM9/shared WRAM
    SetARM9RegionTimings(0x04000000, 0x05000000, 32, 1 + 3, 1); // IO
    SetARM9RegionTimings(0x05000000, 0x06000000, 16, 1 + 3, 1); // palette
    SetARM9RegionTimings(0x06000000, 0x07000000, 16, 1 + 3, 1); // VRAM
    SetARM9RegionTimings(0x07000000, 0x08000000, 32, 1 + 3, 1); // OAM

    // ARM7

    SetARM7RegionTimings(0x00000000, 0xFFFFFFFF, 32, 1, 1); // void

    SetARM7RegionTimings(0x00000000, 0x00010000, 32, 1, 1); // BIOS
    SetARM7RegionTimings(0x02000000, 0x03000000, 16, 8, 1); // main RAM
    SetARM7RegionTimings(0x03000000, 0x04000000, 32, 1, 1); // ARM7/shared WRAM
    SetARM7RegionTimings(0x04000000, 0x04800000, 32, 1, 1); // IO
    SetARM7RegionTimings(0x06000000, 0x07000000, 16, 1, 1); // ARM7 VRAM

    // handled later: GBA slot, wifi
}

void SetupDirectBoot()
{
    if (ConsoleType == 1)
    {
        printf("!! DIRECT BOOT NOT SUPPORTED IN DSI MODE\n");
        return;
    }

    u32 bootparams[8];
    memcpy(bootparams, &NDSCart::CartROM[0x20], 8*4);

    printf("ARM9: offset=%08X entry=%08X RAM=%08X size=%08X\n",
           bootparams[0], bootparams[1], bootparams[2], bootparams[3]);
    printf("ARM7: offset=%08X entry=%08X RAM=%08X size=%08X\n",
           bootparams[4], bootparams[5], bootparams[6], bootparams[7]);

    MapSharedWRAM(3);

    u32 arm9start = 0;

    // load the ARM9 secure area
    if (bootparams[0] >= 0x4000 && bootparams[0] < 0x8000)
    {
        u8 securearea[0x800];
        NDSCart::DecryptSecureArea(securearea);

        for (u32 i = 0; i < 0x800; i+=4)
        {
            ARM9Write32(bootparams[2]+i, *(u32*)&securearea[i]);
            arm9start += 4;
        }
    }

    // CHECKME: firmware seems to load this in 0x200 byte chunks

    for (u32 i = arm9start; i < bootparams[3]; i+=4)
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

    ARM9Write32(0x027FF800, NDSCart::CartID);
    ARM9Write32(0x027FF804, NDSCart::CartID);
    ARM9Write16(0x027FF808, *(u16*)&NDSCart::CartROM[0x15E]);
    ARM9Write16(0x027FF80A, *(u16*)&NDSCart::CartROM[0x6C]);

    ARM9Write16(0x027FF850, 0x5835);

    ARM9Write32(0x027FFC00, NDSCart::CartID);
    ARM9Write32(0x027FFC04, NDSCart::CartID);
    ARM9Write16(0x027FFC08, *(u16*)&NDSCart::CartROM[0x15E]);
    ARM9Write16(0x027FFC0A, *(u16*)&NDSCart::CartROM[0x6C]);

    ARM9Write16(0x027FFC10, 0x5835);
    ARM9Write16(0x027FFC30, 0xFFFF);
    ARM9Write16(0x027FFC40, 0x0001);

    ARM9->CP15Write(0x910, 0x0300000A);
    ARM9->CP15Write(0x911, 0x00000020);
    ARM9->CP15Write(0x100, ARM9->CP15Read(0x100) | 0x00050000);

    ARM9->R[12] = bootparams[1];
    ARM9->R[13] = 0x03002F7C;
    ARM9->R[14] = bootparams[1];
    ARM9->R_IRQ[0] = 0x03003F80;
    ARM9->R_SVC[0] = 0x03003FC0;

    ARM7->R[12] = bootparams[5];
    ARM7->R[13] = 0x0380FD80;
    ARM7->R[14] = bootparams[5];
    ARM7->R_IRQ[0] = 0x0380FF80;
    ARM7->R_SVC[0] = 0x0380FFC0;

    ARM9->JumpTo(bootparams[1]);
    ARM7->JumpTo(bootparams[5]);

    PostFlag9 = 0x01;
    PostFlag7 = 0x01;

    PowerControl9 = 0x820F;
    GPU::SetPowerCnt(PowerControl9);

    // checkme
    RCnt = 0x8000;

    NDSCart::SPICnt = 0x8000;

    SPU::SetBias(0x200);

    SetWifiWaitCnt(0x0030);

    ARM7BIOSProt = 0x1204;

    SPI_Firmware::SetupDirectBoot();
}

void Reset()
{
    FILE* f;
    u32 i;

    RunningGame = false;
    LastSysClockCycles = 0;

    memset(ARM9BIOS, 0, 0x1000);
    memset(ARM7BIOS, 0, 0x4000);

    // DS BIOSes are always loaded, even in DSi mode
    // we need them for DS-compatible mode

    f = Platform::OpenLocalFile(Config::BIOS9Path, "rb");
    if (!f)
    {
        printf("ARM9 BIOS not found. Loading FreeBIOS.\n");
        memcpy(ARM9BIOS, bios_arm9_bin, bios_arm9_bin_len);
    }
    else
    {
        fseek(f, 0, SEEK_SET);
        fread(ARM9BIOS, 0x1000, 1, f);

        printf("ARM9 BIOS loaded\n");
        fclose(f);
    }

    f = Platform::OpenLocalFile(Config::BIOS7Path, "rb");
    if (!f)
    {
        printf("ARM7 BIOS not found. Loading FreeBIOS.\n");
        memcpy(ARM7BIOS, bios_arm7_bin, bios_arm7_bin_len);
    }
    else
    {
        fseek(f, 0, SEEK_SET);
        fread(ARM7BIOS, 0x4000, 1, f);

        printf("ARM7 BIOS loaded\n");
        fclose(f);
    }

#ifdef JIT_ENABLED
    ARMJIT::Reset();
#endif

    if (ConsoleType == 1)
    {
        DSi::LoadBIOS();
        DSi::LoadNAND();

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
    ARM9->CP15Reset();

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
    IPCFIFO9->Clear();
    IPCFIFO7->Clear();

    DivCnt = 0;
    SqrtCnt = 0;

    ARM9->Reset();
    ARM7->Reset();

    CPUStop = 0;

    memset(Timers, 0, 8*sizeof(Timer));
    TimerCheckMask[0] = 0;
    TimerCheckMask[1] = 0;
    TimerTimestamp[0] = 0;
    TimerTimestamp[1] = 0;

    for (i = 0; i < 8; i++) DMAs[i]->Reset();
    memset(DMA9Fill, 0, 4*4);

    memset(SchedList, 0, sizeof(SchedList));
    SchedListMask = 0;

    KeyInput = 0x007F03FF;
    KeyCnt = 0;
    RCnt = 0;

    NDSCart::Reset();
    GBACart::Reset();
    GPU::Reset();
    SPU::Reset();
    SPI::Reset();
    RTC::Reset();
    Wifi::Reset();

    if (ConsoleType == 1)
    {
        DSi::Reset();
        KeyInput &= ~(1 << (16+6));
    }

    AREngine::Reset();
}

void Stop()
{
    printf("Stopping: shutdown\n");
    Running = false;
    Platform::StopEmu();
    GPU::Stop();
    SPU::Stop();
}

bool DoSavestate_Scheduler(Savestate* file)
{
    // this is a bit of a hack
    // but uh, your local coder realized that the scheduler list contains function pointers
    // and that storing those as-is is not a very good idea
    // unless you want it to crash and burn

    // this is the solution your local coder came up with.
    // it's gross but I think it's the best solution for this problem.
    // just remember to add here if you add more event callbacks, kay?
    // atleast until we come up with something more elegant.

    void (*eventfuncs[])(u32) =
    {
        GPU::StartScanline, GPU::StartHBlank, GPU::FinishFrame,
        SPU::Mix,
        Wifi::USTimer,

        GPU::DisplayFIFO,
        NDSCart::ROMPrepareData, NDSCart::ROMEndTransfer,
        NDSCart::SPITransferDone,
        SPI::TransferDone,
        DivDone,
        SqrtDone,

        NULL
    };

    int len = Event_MAX;
    if (file->Saving)
    {
        for (int i = 0; i < len; i++)
        {
            SchedEvent* evt = &SchedList[i];

            u32 funcid = -1;
            if (evt->Func)
            {
                for (int j = 0; eventfuncs[j]; j++)
                {
                    if (evt->Func == eventfuncs[j])
                    {
                        funcid = j;
                        break;
                    }
                }
                if (funcid == -1)
                {
                    printf("savestate: VERY BAD!!!!! FUNCTION POINTER FOR EVENT %d NOT IN HACKY LIST. CANNOT SAVE. SMACK ARISOTURA.\n", i);
                    return false;
                }
            }

            file->Var32(&funcid);
            file->Var64(&evt->Timestamp);
            file->Var32(&evt->Param);
        }
    }
    else
    {
        for (int i = 0; i < len; i++)
        {
            SchedEvent* evt = &SchedList[i];

            u32 funcid;
            file->Var32(&funcid);

            if (funcid != -1)
            {
                for (int j = 0; ; j++)
                {
                    if (!eventfuncs[j])
                    {
                        printf("savestate: VERY BAD!!!!!! EVENT FUNCTION POINTER ID %d IS OUT OF RANGE. HAX?????\n", j);
                        return false;
                    }
                    if (j == funcid) break;
                }

                evt->Func = eventfuncs[funcid];
            }
            else
                evt->Func = NULL;

            file->Var64(&evt->Timestamp);
            file->Var32(&evt->Param);
        }
    }

    return true;
}

bool DoSavestate(Savestate* file)
{
    file->Section("NDSG");

    // TODO:
    // * do something for bool's (sizeof=1)
    // * do something for 'loading DSi-mode savestate in DS mode' and vice-versa
    // * add IE2/IF2 there

    file->VarArray(MainRAM, 0x400000);
    file->VarArray(SharedWRAM, 0x8000);
    file->VarArray(ARM7WRAM, ARM7WRAMSize);

    file->VarArray(ExMemCnt, 2*sizeof(u16));
    file->VarArray(ROMSeed0, 2*8);
    file->VarArray(ROMSeed1, 2*8);

    file->Var16(&WifiWaitCnt);

    file->VarArray(IME, 2*sizeof(u32));
    file->VarArray(IE, 2*sizeof(u32));
    file->VarArray(IF, 2*sizeof(u32));

    file->Var8(&PostFlag9);
    file->Var8(&PostFlag7);
    file->Var16(&PowerControl9);
    file->Var16(&PowerControl7);

    file->Var16(&ARM7BIOSProt);

    file->Var16(&IPCSync9);
    file->Var16(&IPCSync7);
    file->Var16(&IPCFIFOCnt9);
    file->Var16(&IPCFIFOCnt7);
    IPCFIFO9->DoSavestate(file);
    IPCFIFO7->DoSavestate(file);

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

    if (!DoSavestate_Scheduler(file)) return false;
    file->Var32(&SchedListMask);
    file->Var64(&ARM9Timestamp);
    file->Var64(&ARM9Target);
    file->Var64(&ARM7Timestamp);
    file->Var64(&ARM7Target);
    file->Var64(&SysTimestamp);
    file->Var64(&LastSysClockCycles);
    file->Var64(&FrameStartTimestamp);
    file->Var32(&NumFrames);

    // TODO: save KeyInput????
    file->Var16(&KeyCnt);
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

        u16 tmp = WifiWaitCnt;
        WifiWaitCnt = 0xFFFF;
        SetWifiWaitCnt(tmp); // force timing table update
    }

    for (int i = 0; i < 8; i++)
        DMAs[i]->DoSavestate(file);

    ARM9->DoSavestate(file);
    ARM7->DoSavestate(file);

    NDSCart::DoSavestate(file);
    GBACart::DoSavestate(file);
    GPU::DoSavestate(file);
    SPU::DoSavestate(file);
    SPI::DoSavestate(file);
    RTC::DoSavestate(file);
    Wifi::DoSavestate(file);

    if (!file->Saving)
    {
        GPU::SetPowerCnt(PowerControl9);
    }

#ifdef JIT_ENABLED
    if (!file->Saving)
    {
        ARMJIT::ResetBlockCache();
        ARMJIT_Memory::Reset();
    }
#endif

    return true;
}

void SetConsoleType(int type)
{
    ConsoleType = type;
}

bool LoadROM(const char* path, const char* sram, bool direct)
{
    if (NDSCart::LoadROM(path, sram, direct))
    {
        Running = true;
        return true;
    }
    else
    {
        printf("Failed to load ROM %s\n", path);
        return false;
    }
}

bool LoadGBAROM(const char* path, const char* sram)
{
    if (GBACart::LoadROM(path, sram))
    {
        return true;
    }
    else
    {
        printf("Failed to load ROM %s\n", path);
        return false;
    }
}

void LoadBIOS()
{
    Reset();
    Running = true;
}

void RelocateSave(const char* path, bool write)
{
    printf("SRAM: relocating to %s (write=%s)\n", path, write?"true":"false");
    NDSCart::RelocateSave(path, write);
}



u64 NextTarget()
{
    u64 ret = SysTimestamp + kMaxIterationCycles;

    u32 mask = SchedListMask;
    for (int i = 0; i < Event_MAX; i++)
    {
        if (!mask) break;
        if (mask & 0x1)
        {
            if (SchedList[i].Timestamp < ret)
                ret = SchedList[i].Timestamp;
        }

        mask >>= 1;
    }

    return ret;
}

void RunSystem(u64 timestamp)
{
    SysTimestamp = timestamp;

    u32 mask = SchedListMask;
    for (int i = 0; i < Event_MAX; i++)
    {
        if (!mask) break;
        if (mask & 0x1)
        {
            if (SchedList[i].Timestamp <= SysTimestamp)
            {
                SchedListMask &= ~(1<<i);
                SchedList[i].Func(SchedList[i].Param);
            }
        }

        mask >>= 1;
    }
}

template <bool EnableJIT>
u32 RunFrame()
{
    FrameStartTimestamp = SysTimestamp;

    if (!Running) return 263; // dorp
    if (CPUStop & 0x40000000) return 263;

    GPU::StartFrame();

    while (Running && GPU::TotalScanlines==0)
    {
        // TODO: give it some margin, so it can directly do 17 cycles instead of 16 then 1
        u64 target = NextTarget();
        ARM9Target = target << ARM9ClockShift;
        CurCPU = 0;

        if (CPUStop & 0x80000000)
        {
            // GXFIFO stall
            s32 cycles = GPU3D::CyclesToRunFor();

            ARM9Timestamp = std::min(ARM9Target, ARM9Timestamp+(cycles<<ARM9ClockShift));
        }
        else if (CPUStop & 0x0FFF)
        {
            DMAs[0]->Run();
            if (!(CPUStop & 0x80000000)) DMAs[1]->Run();
            if (!(CPUStop & 0x80000000)) DMAs[2]->Run();
            if (!(CPUStop & 0x80000000)) DMAs[3]->Run();
            if (ConsoleType == 1) DSi::RunNDMAs(0);
        }
        else
        {
#ifdef JIT_ENABLED
            if (EnableJIT)
                ARM9->ExecuteJIT();
            else
#endif
                ARM9->Execute();
        }

        RunTimers(0);
        GPU3D::Run();

        target = ARM9Timestamp >> ARM9ClockShift;
        CurCPU = 1;

        while (ARM7Timestamp < target)
        {
            ARM7Target = target; // might be changed by a reschedule

            if (CPUStop & 0x0FFF0000)
            {
                DMAs[4]->Run();
                DMAs[5]->Run();
                DMAs[6]->Run();
                DMAs[7]->Run();
                if (ConsoleType == 1) DSi::RunNDMAs(1);
            }
            else
            {
#ifdef JIT_ENABLED
                if (EnableJIT)
                    ARM7->ExecuteJIT();
                else
#endif
                    ARM7->Execute();
            }

            RunTimers(1);
        }

        RunSystem(target);

        if (CPUStop & 0x40000000)
        {
            // checkme: when is sleep mode effective?
            //CancelEvent(Event_LCD);
            //GPU::TotalScanlines = 263;
            break;
        }
    }

#ifdef DEBUG_CHECK_DESYNC
    printf("[%08X%08X] ARM9=%ld, ARM7=%ld, GPU=%ld\n",
           (u32)(SysTimestamp>>32), (u32)SysTimestamp,
           (ARM9Timestamp>>1)-SysTimestamp,
           ARM7Timestamp-SysTimestamp,
           GPU3D::Timestamp-SysTimestamp);
#endif

    NumFrames++;

    return GPU::TotalScanlines;
}

u32 RunFrame()
{
#ifdef JIT_ENABLED
    if (Config::JIT_Enable)
        return RunFrame<true>();
    else
#endif
        return RunFrame<false>();
}

void Reschedule(u64 target)
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

void ScheduleEvent(u32 id, bool periodic, s32 delay, void (*func)(u32), u32 param)
{
    if (SchedListMask & (1<<id))
    {
        printf("!! EVENT %d ALREADY SCHEDULED\n", id);
        return;
    }

    SchedEvent* evt = &SchedList[id];

    if (periodic)
        evt->Timestamp += delay;
    else
    {
        if (CurCPU == 0)
            evt->Timestamp = (ARM9Timestamp >> ARM9ClockShift) + delay;
        else
            evt->Timestamp = ARM7Timestamp + delay;
    }

    evt->Func = func;
    evt->Param = param;

    SchedListMask |= (1<<id);

    Reschedule(evt->Timestamp);
}

void CancelEvent(u32 id)
{
    SchedListMask &= ~(1<<id);
}


void TouchScreen(u16 x, u16 y)
{
    if (ConsoleType == 1)
    {
        DSi_SPI_TSC::SetTouchCoords(x, y);
    }
    else
    {
        SPI_TSC::SetTouchCoords(x, y);
        KeyInput &= ~(1 << (16+6));
    }
}

void ReleaseScreen()
{
    if (ConsoleType == 1)
    {
        DSi_SPI_TSC::SetTouchCoords(0x000, 0xFFF);
    }
    else
    {
        SPI_TSC::SetTouchCoords(0x000, 0xFFF);
        KeyInput |= (1 << (16+6));
    }
}


void SetKeyMask(u32 mask)
{
    u32 key_lo = mask & 0x3FF;
    u32 key_hi = (mask >> 10) & 0x3;

    KeyInput &= 0xFFFCFC00;
    KeyInput |= key_lo | (key_hi << 16);
}

bool IsLidClosed()
{
    if (KeyInput & (1<<23)) return true;
    return false;
}

void SetLidClosed(bool closed)
{
    if (closed)
    {
        KeyInput |= (1<<23);
    }
    else
    {
        KeyInput &= ~(1<<23);
        SetIRQ(1, IRQ_LidOpen);
        CPUStop &= ~0x40000000;
    }
}

void MicInputFrame(s16* data, int samples)
{
    return SPI_TSC::MicInputFrame(data, samples);
}

int ImportSRAM(u8* data, u32 length)
{
    return NDSCart::ImportSRAM(data, length);
}


void Halt()
{
    printf("Halt()\n");
    Running = false;
}


void MapSharedWRAM(u8 val)
{
    if (val == WRAMCnt)
        return;

#ifdef JIT_ENABLED
    ARMJIT_Memory::RemapSWRAM();
#endif

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


void SetWifiWaitCnt(u16 val)
{
    if (WifiWaitCnt == val) return;

    WifiWaitCnt = val;

    const int ntimings[4] = {10, 8, 6, 18};
    SetARM7RegionTimings(0x04800000, 0x04808000, 16, ntimings[val & 0x3], (val & 0x4) ? 4 : 6);
    SetARM7RegionTimings(0x04808000, 0x04810000, 16, ntimings[(val>>3) & 0x3], (val & 0x20) ? 4 : 10);
}

void SetGBASlotTimings()
{
    int curcpu = (ExMemCnt[0] >> 7) & 0x1;

    const int ntimings[4] = {10, 8, 6, 18};

    u16 curcnt = ExMemCnt[curcpu];
    int ramN = ntimings[curcnt & 0x3];
    int romN = ntimings[(curcnt>>2) & 0x3];
    int romS = (curcnt & 0x10) ? 4 : 6;

    // TODO: PHI pin thing?

    if (curcpu == 0)
    {
        SetARM9RegionTimings(0x08000000, 0x0A000000, 16, romN + 3, romS);
        SetARM9RegionTimings(0x0A000000, 0x0B000000, 8, ramN + 3, ramN);

        SetARM7RegionTimings(0x08000000, 0x0A000000, 32, 1, 1);
        SetARM7RegionTimings(0x0A000000, 0x0B000000, 32, 1, 1);
    }
    else
    {
        SetARM9RegionTimings(0x08000000, 0x0A000000, 32, 1, 1);
        SetARM9RegionTimings(0x0A000000, 0x0B000000, 32, 1, 1);

        SetARM7RegionTimings(0x08000000, 0x0A000000, 16, romN, romS);
        SetARM7RegionTimings(0x0A000000, 0x0B000000, 8, ramN, ramN);
    }
}


void UpdateIRQ(u32 cpu)
{
    ARM* arm = cpu ? (ARM*)ARM7 : (ARM*)ARM9;

    if (IME[cpu] & 0x1)
    {
        arm->IRQ = !!(IE[cpu] & IF[cpu]);
        if ((ConsoleType == 1) && cpu)
            arm->IRQ |= !!(IE2 & IF2);
    }
    else
    {
        arm->IRQ = 0;
    }
}

void SetIRQ(u32 cpu, u32 irq)
{
    IF[cpu] |= (1 << irq);
    UpdateIRQ(cpu);
}

void ClearIRQ(u32 cpu, u32 irq)
{
    IF[cpu] &= ~(1 << irq);
    UpdateIRQ(cpu);
}

void SetIRQ2(u32 irq)
{
    IF2 |= (1 << irq);
    UpdateIRQ(1);
}

void ClearIRQ2(u32 irq)
{
    IF2 &= ~(1 << irq);
    UpdateIRQ(1);
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

    if ((ConsoleType == 1) && cpu && (IF2 & IE2))
        return true;

    return false;
}

void StopCPU(u32 cpu, u32 mask)
{
    if (cpu)
    {
        CPUStop |= (mask << 16);
        ARM7->Halt(2);
    }
    else
    {
        CPUStop |= mask;
        ARM9->Halt(2);
    }
}

void ResumeCPU(u32 cpu, u32 mask)
{
    if (cpu) mask <<= 16;
    CPUStop &= ~mask;
}

void GXFIFOStall()
{
    if (CPUStop & 0x80000000) return;

    CPUStop |= 0x80000000;

    if (CurCPU == 1) ARM9->Halt(2);
    else
    {
        DMAs[0]->StallIfRunning();
        DMAs[1]->StallIfRunning();
        DMAs[2]->StallIfRunning();
        DMAs[3]->StallIfRunning();
        if (ConsoleType == 1) DSi::StallNDMAs();
    }
}

void GXFIFOUnstall()
{
    CPUStop &= ~0x80000000;
}

void EnterSleepMode()
{
    if (CPUStop & 0x40000000) return;

    CPUStop |= 0x40000000;
    ARM7->Halt(2);
}

u32 GetPC(u32 cpu)
{
    return cpu ? ARM7->R[15] : ARM9->R[15];
}

u64 GetSysClockCycles(int num)
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

void NocashPrint(u32 ncpu, u32 addr)
{
    // addr: u16 flags (TODO: research? libnds doesn't use those)
    // addr+2: debug string

    addr += 2;

    ARM* cpu = ncpu ? (ARM*)ARM7 : (ARM*)ARM9;
    u8 (*readfn)(u32) = ncpu ? NDS::ARM7Read8 : NDS::ARM9Read8;

    char output[1024];
    int ptr = 0;

    for (int i = 0; i < 120 && ptr < 1023; )
    {
        char ch = readfn(addr++);
        i++;

        if (ch == '%')
        {
            char cmd[16]; int j;
            for (j = 0; j < 15; )
            {
                char ch2 = readfn(addr++);
                i++;
                if (i >= 120) break;
                if (ch2 == '%') break;
                cmd[j++] = ch2;
            }
            cmd[j] = '\0';

            char subs[64];

            if (cmd[0] == 'r')
            {
                if      (!strcmp(cmd, "r0")) sprintf(subs, "%08X", cpu->R[0]);
                else if (!strcmp(cmd, "r1")) sprintf(subs, "%08X", cpu->R[1]);
                else if (!strcmp(cmd, "r2")) sprintf(subs, "%08X", cpu->R[2]);
                else if (!strcmp(cmd, "r3")) sprintf(subs, "%08X", cpu->R[3]);
                else if (!strcmp(cmd, "r4")) sprintf(subs, "%08X", cpu->R[4]);
                else if (!strcmp(cmd, "r5")) sprintf(subs, "%08X", cpu->R[5]);
                else if (!strcmp(cmd, "r6")) sprintf(subs, "%08X", cpu->R[6]);
                else if (!strcmp(cmd, "r7")) sprintf(subs, "%08X", cpu->R[7]);
                else if (!strcmp(cmd, "r8")) sprintf(subs, "%08X", cpu->R[8]);
                else if (!strcmp(cmd, "r9")) sprintf(subs, "%08X", cpu->R[9]);
                else if (!strcmp(cmd, "r10")) sprintf(subs, "%08X", cpu->R[10]);
                else if (!strcmp(cmd, "r11")) sprintf(subs, "%08X", cpu->R[11]);
                else if (!strcmp(cmd, "r12")) sprintf(subs, "%08X", cpu->R[12]);
                else if (!strcmp(cmd, "r13")) sprintf(subs, "%08X", cpu->R[13]);
                else if (!strcmp(cmd, "r14")) sprintf(subs, "%08X", cpu->R[14]);
                else if (!strcmp(cmd, "r15")) sprintf(subs, "%08X", cpu->R[15]);
            }
            else
            {
                if      (!strcmp(cmd, "sp")) sprintf(subs, "%08X", cpu->R[13]);
                else if (!strcmp(cmd, "lr")) sprintf(subs, "%08X", cpu->R[14]);
                else if (!strcmp(cmd, "pc")) sprintf(subs, "%08X", cpu->R[15]);
                else if (!strcmp(cmd, "frame")) sprintf(subs, "%u", NumFrames);
                else if (!strcmp(cmd, "scanline")) sprintf(subs, "%u", GPU::VCount);
                else if (!strcmp(cmd, "totalclks")) sprintf(subs, "%lu", GetSysClockCycles(0));
                else if (!strcmp(cmd, "lastclks")) sprintf(subs, "%lu", GetSysClockCycles(1));
                else if (!strcmp(cmd, "zeroclks"))
                {
                    sprintf(subs, "");
                    GetSysClockCycles(1);
                }
            }

            int slen = strlen(subs);
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
    printf("%s", output);
}



void MonitorARM9Jump(u32 addr)
{
    // checkme: can the entrypoint addr be THUMB?
    // also TODO: make it work in DSi mode

    if ((!RunningGame) && NDSCart::CartROM)
    {
        if (addr == *(u32*)&NDSCart::CartROM[0x24])
        {
            printf("Game is now booting\n");
            RunningGame = true;
        }
    }
}



void HandleTimerOverflow(u32 tid)
{
    Timer* timer = &Timers[tid];

    timer->Counter += timer->Reload << 16;
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

        timer->Counter += 0x10000;
        if (timer->Counter >> 16)
            break;

        timer->Counter = timer->Reload << 16;
        if (timer->Cnt & (1<<6))
            SetIRQ(tid >> 2, IRQ_Timer0 + (tid & 0x3));

        if ((tid & 0x3) == 3)
            break;
    }
}

void RunTimer(u32 tid, s32 cycles)
{
    Timer* timer = &Timers[tid];

    u32 oldcount = timer->Counter;
    timer->Counter += (cycles << timer->CycleShift);
    if (timer->Counter < oldcount)
        HandleTimerOverflow(tid);
}

void RunTimers(u32 cpu)
{
    register u32 timermask = TimerCheckMask[cpu];
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



// matching NDMA modes for DSi
const u32 NDMAModes[] =
{
    // ARM9

    0x10, // immediate
    0x06, // VBlank
    0x07, // HBlank
    0x08, // scanline start
    0x09, // mainmem FIFO
    0x04, // DS cart slot
    0xFF, // GBA cart slot
    0x0A, // GX FIFO
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

    // ARM7

    0x30, // immediate
    0x26, // VBlank
    0x24, // DS cart slot
    0xFF, // wifi / GBA cart slot (TODO)
};

bool DMAsInMode(u32 cpu, u32 mode)
{
    cpu <<= 2;
    if (DMAs[cpu+0]->IsInMode(mode)) return true;
    if (DMAs[cpu+1]->IsInMode(mode)) return true;
    if (DMAs[cpu+2]->IsInMode(mode)) return true;
    if (DMAs[cpu+3]->IsInMode(mode)) return true;

    if (ConsoleType == 1)
    {
        cpu >>= 2;
        return DSi::NDMAsInMode(cpu, NDMAModes[mode]);
    }

    return false;
}

bool DMAsRunning(u32 cpu)
{
    cpu <<= 2;
    if (DMAs[cpu+0]->IsRunning()) return true;
    if (DMAs[cpu+1]->IsRunning()) return true;
    if (DMAs[cpu+2]->IsRunning()) return true;
    if (DMAs[cpu+3]->IsRunning()) return true;
    if (ConsoleType == 1)
    {
        if (DSi::NDMAsRunning(cpu>>2)) return true;
    }
    return false;
}

void CheckDMAs(u32 cpu, u32 mode)
{
    cpu <<= 2;
    DMAs[cpu+0]->StartIfNeeded(mode);
    DMAs[cpu+1]->StartIfNeeded(mode);
    DMAs[cpu+2]->StartIfNeeded(mode);
    DMAs[cpu+3]->StartIfNeeded(mode);

    if (ConsoleType == 1)
    {
        cpu >>= 2;
        DSi::CheckNDMAs(cpu, NDMAModes[mode]);
    }
}

void StopDMAs(u32 cpu, u32 mode)
{
    cpu <<= 2;
    DMAs[cpu+0]->StopIfNeeded(mode);
    DMAs[cpu+1]->StopIfNeeded(mode);
    DMAs[cpu+2]->StopIfNeeded(mode);
    DMAs[cpu+3]->StopIfNeeded(mode);

    if (ConsoleType == 1)
    {
        cpu >>= 2;
        DSi::StopNDMAs(cpu, NDMAModes[mode]);
    }
}




const s32 TimerPrescaler[4] = {0, 6, 8, 10};

u16 TimerGetCounter(u32 timer)
{
    RunTimers(timer>>2);
    u32 ret = Timers[timer].Counter;

    return ret >> 16;
}

void TimerStart(u32 id, u16 cnt)
{
    Timer* timer = &Timers[id];
    u16 curstart = timer->Cnt & (1<<7);
    u16 newstart = cnt & (1<<7);

    timer->Cnt = cnt;
    timer->CycleShift = 16 - TimerPrescaler[cnt & 0x03];

    if ((!curstart) && newstart)
    {
        timer->Counter = timer->Reload << 16;

        /*if ((cnt & 0x84) == 0x80)
        {
            u32 delay = (0x10000 - timer->Reload) << TimerPrescaler[cnt & 0x03];
            printf("timer%d IRQ: start   %d, reload=%04X cnt=%08X\n", id, delay, timer->Reload, timer->Counter);
            CancelEvent(Event_TimerIRQ_0 + id);
            ScheduleEvent(Event_TimerIRQ_0 + id, false, delay, HandleTimerOverflow, id);
        }*/
    }

    if ((cnt & 0x84) == 0x80)
    {
        u32 tmask;
        //if ((cnt & 0x03) == 0)
            tmask = 0x01 << (id&0x3);
        //else
        //    tmask = 0x10 << (id&0x3);

        TimerCheckMask[id>>2] |= tmask;
    }
    else
        TimerCheckMask[id>>2] &= ~(0x11 << (id&0x3));
}



void DivDone(u32 param)
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
        DivCnt |= 0x4000;
}

void StartDiv()
{
    NDS::CancelEvent(NDS::Event_Div);
    DivCnt |= 0x8000;
    NDS::ScheduleEvent(NDS::Event_Div, false, ((DivCnt&0x3)==0) ? 18:34, DivDone, 0);
}

// http://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
void SqrtDone(u32 param)
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

void StartSqrt()
{
    NDS::CancelEvent(NDS::Event_Sqrt);
    SqrtCnt |= 0x8000;
    NDS::ScheduleEvent(NDS::Event_Sqrt, false, 13, SqrtDone, 0);
}



void debug(u32 param)
{
    printf("ARM9 PC=%08X LR=%08X %08X\n", ARM9->R[15], ARM9->R[14], ARM9->R_IRQ[1]);
    printf("ARM7 PC=%08X LR=%08X %08X\n", ARM7->R[15], ARM7->R[14], ARM7->R_IRQ[1]);

    printf("ARM9 IME=%08X IE=%08X IF=%08X\n", IME[0], IE[0], IF[0]);
    printf("ARM7 IME=%08X IE=%08X IF=%08X IE2=%04X IF2=%04X\n", IME[1], IE[1], IF[1], IE2, IF2);

    //for (int i = 0; i < 9; i++)
    //    printf("VRAM %c: %02X\n", 'A'+i, GPU::VRAMCNT[i]);

    /*FILE*
    shit = fopen("debug/party.bin", "wb");
    fwrite(ARM9->ITCM, 0x8000, 1, shit);
    for (u32 i = 0x02000000; i < 0x02400000; i+=4)
    {
        u32 val = ARM7Read32(i);
        fwrite(&val, 4, 1, shit);
    }
    for (u32 i = 0x037F0000; i < 0x03810000; i+=4)
    {
        u32 val = ARM7Read32(i);
        fwrite(&val, 4, 1, shit);
    }
    fclose(shit);*/

    FILE*
    shit = fopen("debug/picto9.bin", "wb");
    for (u32 i = 0x02000000; i < 0x04000000; i+=4)
    {
        u32 val = DSi::ARM9Read32(i);
        fwrite(&val, 4, 1, shit);
    }
    fclose(shit);
    shit = fopen("debug/picto7.bin", "wb");
    for (u32 i = 0x02000000; i < 0x04000000; i+=4)
    {
        u32 val = DSi::ARM7Read32(i);
        fwrite(&val, 4, 1, shit);
    }
    fclose(shit);
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
        return ARM9IORead8(addr);

    case 0x05000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return *(u8*)&GPU::Palette[addr & 0x7FF];

    case 0x06000000:
        switch (addr & 0x00E00000)
        {
        case 0x00000000: return GPU::ReadVRAM_ABG<u8>(addr);
        case 0x00200000: return GPU::ReadVRAM_BBG<u8>(addr);
        case 0x00400000: return GPU::ReadVRAM_AOBJ<u8>(addr);
        case 0x00600000: return GPU::ReadVRAM_BOBJ<u8>(addr);
        default:         return GPU::ReadVRAM_LCDC<u8>(addr);
        }

    case 0x07000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return *(u8*)&GPU::OAM[addr & 0x7FF];

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return 0x00; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return *(u8*)&GBACart::CartROM[addr & (GBACart::CartROMSize-1)];
        }
        return 0xFF; // TODO: proper open bus

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return 0x00; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return GBACart_SRAM::Read8(addr & (GBACart_SRAM::SRAMLength-1));
        }
        return 0xFF; // TODO: proper open bus
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
        return ARM9IORead16(addr);

    case 0x05000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return *(u16*)&GPU::Palette[addr & 0x7FF];

    case 0x06000000:
        switch (addr & 0x00E00000)
        {
        case 0x00000000: return GPU::ReadVRAM_ABG<u16>(addr);
        case 0x00200000: return GPU::ReadVRAM_BBG<u16>(addr);
        case 0x00400000: return GPU::ReadVRAM_AOBJ<u16>(addr);
        case 0x00600000: return GPU::ReadVRAM_BOBJ<u16>(addr);
        default:         return GPU::ReadVRAM_LCDC<u16>(addr);
        }

    case 0x07000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return *(u16*)&GPU::OAM[addr & 0x7FF];

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return 0x0000; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return *(u16*)&GBACart::CartROM[addr & (GBACart::CartROMSize-1)];
        }
        return 0xFFFF; // TODO: proper open bus

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return 0x0000; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return GBACart_SRAM::Read16(addr & (GBACart_SRAM::SRAMLength-1));
        }
        return 0xFFFF; // TODO: proper open bus
    }

    //printf("unknown arm9 read16 %08X %08X\n", addr, ARM9->R[15]);
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
        return ARM9IORead32(addr);

    case 0x05000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return *(u32*)&GPU::Palette[addr & 0x7FF];

    case 0x06000000:
        switch (addr & 0x00E00000)
        {
        case 0x00000000: return GPU::ReadVRAM_ABG<u32>(addr);
        case 0x00200000: return GPU::ReadVRAM_BBG<u32>(addr);
        case 0x00400000: return GPU::ReadVRAM_AOBJ<u32>(addr);
        case 0x00600000: return GPU::ReadVRAM_BOBJ<u32>(addr);
        default:         return GPU::ReadVRAM_LCDC<u32>(addr);
        }

    case 0x07000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return 0;
        return *(u32*)&GPU::OAM[addr & 0x7FF];

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return 0x00000000; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return *(u32*)&GBACart::CartROM[addr & (GBACart::CartROMSize-1)];
        }
        return 0xFFFFFFFF; // TODO: proper open bus

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return 0x00000000; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return GBACart_SRAM::Read32(addr & (GBACart_SRAM::SRAMLength-1));
        }
        return 0xFFFFFFFF; // TODO: proper open bus
    }

    printf("unknown arm9 read32 %08X | %08X %08X\n", addr, ARM9->R[15], ARM9->R[12]);
    return 0;
}

void ARM9Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x02000000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u8*)&MainRAM[addr & MainRAMMask] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM9.Mem)
        {
#ifdef JIT_ENABLED
            ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_SharedWRAM>(addr);
#endif
            *(u8*)&SWRAM_ARM9.Mem[addr & SWRAM_ARM9.Mask] = val;
        }
        return;

    case 0x04000000:
        ARM9IOWrite8(addr, val);
        return;

    case 0x05000000:
    case 0x06000000:
    case 0x07000000:
        // checkme
        return;

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            if ((addr & 0x00FFFFFF) >= 0xC4 && (addr & 0x00FFFFFF) <= 0xC9)
            {
                GBACart::WriteGPIO(addr & (GBACart::CartROMSize-1), val);
                return;
            }
        }
        break;

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            GBACart_SRAM::Write8(addr & (GBACart_SRAM::SRAMLength-1), val);
        }
        return;
    }

    printf("unknown arm9 write8 %08X %02X\n", addr, val);
}

void ARM9Write16(u32 addr, u16 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x02000000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u16*)&MainRAM[addr & MainRAMMask] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM9.Mem)
        {
#ifdef JIT_ENABLED
            ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_SharedWRAM>(addr);
#endif
            *(u16*)&SWRAM_ARM9.Mem[addr & SWRAM_ARM9.Mask] = val;
        }
        return;

    case 0x04000000:
        ARM9IOWrite16(addr, val);
        return;

    case 0x05000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return;
        *(u16*)&GPU::Palette[addr & 0x7FF] = val;
        return;

    case 0x06000000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_VRAM>(addr);
#endif
        switch (addr & 0x00E00000)
        {
        case 0x00000000: GPU::WriteVRAM_ABG<u16>(addr, val); return;
        case 0x00200000: GPU::WriteVRAM_BBG<u16>(addr, val); return;
        case 0x00400000: GPU::WriteVRAM_AOBJ<u16>(addr, val); return;
        case 0x00600000: GPU::WriteVRAM_BOBJ<u16>(addr, val); return;
        default: GPU::WriteVRAM_LCDC<u16>(addr, val); return;
        }

    case 0x07000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return;
        *(u16*)&GPU::OAM[addr & 0x7FF] = val;
        return;

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            // Note: the lower bound is adjusted such that a write starting
            // there will hit the first byte of the GPIO region.
            if ((addr & 0x00FFFFFF) >= 0xC3 && (addr & 0x00FFFFFF) <= 0xC9)
            {
                GBACart::WriteGPIO(addr & (GBACart::CartROMSize-1), val);
                return;
            }
        }
        break;

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            GBACart_SRAM::Write16(addr & (GBACart_SRAM::SRAMLength-1), val);
        }
        return;
    }

    //printf("unknown arm9 write16 %08X %04X\n", addr, val);
}

void ARM9Write32(u32 addr, u32 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x02000000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u32*)&MainRAM[addr & MainRAMMask] = val;
        return ;

    case 0x03000000:
        if (SWRAM_ARM9.Mem)
        {
#ifdef JIT_ENABLED
            ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_SharedWRAM>(addr);
#endif
            *(u32*)&SWRAM_ARM9.Mem[addr & SWRAM_ARM9.Mask] = val;
        }
        return;

    case 0x04000000:
        ARM9IOWrite32(addr, val);
        return;

    case 0x05000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return;
        *(u32*)&GPU::Palette[addr & 0x7FF] = val;
        return;

    case 0x06000000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_VRAM>(addr);
#endif
        switch (addr & 0x00E00000)
        {
        case 0x00000000: GPU::WriteVRAM_ABG<u32>(addr, val); return;
        case 0x00200000: GPU::WriteVRAM_BBG<u32>(addr, val); return;
        case 0x00400000: GPU::WriteVRAM_AOBJ<u32>(addr, val); return;
        case 0x00600000: GPU::WriteVRAM_BOBJ<u32>(addr, val); return;
        default: GPU::WriteVRAM_LCDC<u32>(addr, val); return;
        }

    case 0x07000000:
        if (!(PowerControl9 & ((addr & 0x400) ? (1<<9) : (1<<1)))) return;
        *(u32*)&GPU::OAM[addr & 0x7FF] = val;
        return;

    case 0x08000000:
    case 0x09000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            // Note: the lower bound is adjusted such that a write starting
            // there will hit the first byte of the GPIO region.
            if ((addr & 0x00FFFFFF) >= 0xC1 && (addr & 0x00FFFFFF) <= 0xC9)
            {
                GBACart::WriteGPIO(addr & (GBACart::CartROMSize-1), val & 0xFF);
                GBACart::WriteGPIO((addr + 2) & (GBACart::CartROMSize-1), (val >> 16) & 0xFF);
                return;
            }
        }
        break;

    case 0x0A000000:
        if (ExMemCnt[0] & (1<<7)) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            GBACart_SRAM::Write32(addr & (GBACart_SRAM::SRAMLength-1), val);
        }
        return;
    }

    //printf("unknown arm9 write32 %08X %08X | %08X\n", addr, val, ARM9->R[15]);
}

bool ARM9GetMemRegion(u32 addr, bool write, MemRegion* region)
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
        region->Mem = ARM9BIOS;
        region->Mask = 0xFFF;
        return true;
    }

    region->Mem = NULL;
    return false;
}



u8 ARM7Read8(u32 addr)
{
    if (addr < 0x00004000)
    {
        // TODO: check the boundary? is it 4000 or higher on regular DS?
        if (ARM7->R[15] >= 0x00004000)
            return 0xFF;
        if (addr < ARM7BIOSProt && ARM7->R[15] >= ARM7BIOSProt)
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
        return ARM7IORead8(addr);

    case 0x06000000:
    case 0x06800000:
        return GPU::ReadVRAM_ARM7<u8>(addr);

    case 0x08000000:
    case 0x09000000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x00; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return *(u8*)&GBACart::CartROM[addr & (GBACart::CartROMSize-1)];
        }
        return 0xFF; // TODO: proper open bus

    case 0x0A000000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x00; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return GBACart_SRAM::Read8(addr & (GBACart_SRAM::SRAMLength-1));
        }
        return 0xFF; // TODO: proper open bus
    }

    printf("unknown arm7 read8 %08X %08X %08X/%08X\n", addr, ARM7->R[15], ARM7->R[0], ARM7->R[1]);
    return 0;
}

u16 ARM7Read16(u32 addr)
{
    if (addr < 0x00004000)
    {
        if (ARM7->R[15] >= 0x00004000)
            return 0xFFFF;
        if (addr < ARM7BIOSProt && ARM7->R[15] >= ARM7BIOSProt)
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
        return ARM7IORead16(addr);

    case 0x04800000:
        if (addr < 0x04810000)
        {
            return Wifi::Read(addr);
        }
        break;

    case 0x06000000:
    case 0x06800000:
        return GPU::ReadVRAM_ARM7<u16>(addr);

    case 0x08000000:
    case 0x09000000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x0000; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return *(u16*)&GBACart::CartROM[addr & (GBACart::CartROMSize-1)];
        }
        return 0xFFFF; // TODO: proper open bus

    case 0x0A000000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x0000; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return GBACart_SRAM::Read16(addr & (GBACart_SRAM::SRAMLength-1));
        }
        return 0xFFFF; // TODO: proper open bus
    }

    printf("unknown arm7 read16 %08X %08X\n", addr, ARM7->R[15]);
    return 0;
}

u32 ARM7Read32(u32 addr)
{
    if (addr < 0x00004000)
    {
        if (ARM7->R[15] >= 0x00004000)
            return 0xFFFFFFFF;
        if (addr < ARM7BIOSProt && ARM7->R[15] >= ARM7BIOSProt)
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
        return ARM7IORead32(addr);

    case 0x04800000:
        if (addr < 0x04810000)
        {
            return Wifi::Read(addr) | (Wifi::Read(addr+2) << 16);
        }
        break;

    case 0x06000000:
    case 0x06800000:
        return GPU::ReadVRAM_ARM7<u32>(addr);

    case 0x08000000:
    case 0x09000000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x00000000; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return *(u32*)&GBACart::CartROM[addr & (GBACart::CartROMSize-1)];
        }
        return 0xFFFFFFFF; // TODO: proper open bus

    case 0x0A000000:
        if (!(ExMemCnt[0] & (1<<7))) return 0x00000000; // deselected CPU is 00h-filled
        if (GBACart::CartInserted)
        {
            return GBACart_SRAM::Read32(addr & (GBACart_SRAM::SRAMLength-1));
        }
        return 0xFFFFFFFF; // TODO: proper open bus
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
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u8*)&MainRAM[addr & MainRAMMask] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM7.Mem)
        {
#ifdef JIT_ENABLED
            ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_SharedWRAM>(addr);
#endif
            *(u8*)&SWRAM_ARM7.Mem[addr & SWRAM_ARM7.Mask] = val;
            return;
        }
        else
        {
#ifdef JIT_ENABLED
            ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
#endif
            *(u8*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
            return;
        }

    case 0x03800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
#endif
        *(u8*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
        return;

    case 0x04000000:
        ARM7IOWrite8(addr, val);
        return;

    case 0x06000000:
    case 0x06800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_VWRAM>(addr);
#endif
        GPU::WriteVRAM_ARM7<u8>(addr, val);
        return;

    case 0x08000000:
    case 0x09000000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            if ((addr & 0x00FFFFFF) >= 0xC4 && (addr & 0x00FFFFFF) <= 0xC9)
            {
                GBACart::WriteGPIO(addr & (GBACart::CartROMSize-1), val);
                return;
            }
        }
        break;

    case 0x0A000000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            GBACart_SRAM::Write8(addr & (GBACart_SRAM::SRAMLength-1), val);
        }
        return;
    }

    if (ARM7->R[15] > 0x00002F30) // ARM7 BIOS bug
        printf("unknown arm7 write8 %08X %02X @ %08X\n", addr, val, ARM7->R[15]);
}

void ARM7Write16(u32 addr, u16 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u16*)&MainRAM[addr & MainRAMMask] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM7.Mem)
        {
#ifdef JIT_ENABLED
            ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_SharedWRAM>(addr);
#endif
            *(u16*)&SWRAM_ARM7.Mem[addr & SWRAM_ARM7.Mask] = val;
            return;
        }
        else
        {
#ifdef JIT_ENABLED
            ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
#endif
            *(u16*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
            return;
        }

    case 0x03800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
#endif
        *(u16*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
        return;

    case 0x04000000:
        ARM7IOWrite16(addr, val);
        return;

    case 0x04800000:
        if (addr < 0x04810000)
        {
            Wifi::Write(addr, val);
            return;
        }
        break;

    case 0x06000000:
    case 0x06800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_VWRAM>(addr);
#endif
        GPU::WriteVRAM_ARM7<u16>(addr, val);
        return;

    case 0x08000000:
    case 0x09000000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            // Note: the lower bound is adjusted such that a write starting
            // there will hit the first byte of the GPIO region.
            if ((addr & 0x00FFFFFF) >= 0xC3 && (addr & 0x00FFFFFF) <= 0xC9)
            {
                GBACart::WriteGPIO(addr & (GBACart::CartROMSize-1), val);
                return;
            }
        }
        break;

    case 0x0A000000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            GBACart_SRAM::Write16(addr & (GBACart_SRAM::SRAMLength-1), val);
        }
        return;
    }

    //printf("unknown arm7 write16 %08X %04X @ %08X\n", addr, val, ARM7->R[15]);
}

void ARM7Write32(u32 addr, u32 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u32*)&MainRAM[addr & MainRAMMask] = val;
        return;

    case 0x03000000:
        if (SWRAM_ARM7.Mem)
        {
#ifdef JIT_ENABLED
            ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_SharedWRAM>(addr);
#endif
            *(u32*)&SWRAM_ARM7.Mem[addr & SWRAM_ARM7.Mask] = val;
            return;
        }
        else
        {
#ifdef JIT_ENABLED
            ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
#endif
            *(u32*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
            return;
        }

    case 0x03800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_WRAM7>(addr);
#endif
        *(u32*)&ARM7WRAM[addr & (ARM7WRAMSize - 1)] = val;
        return;

    case 0x04000000:
        ARM7IOWrite32(addr, val);
        return;

    case 0x04800000:
        if (addr < 0x04810000)
        {
            Wifi::Write(addr, val & 0xFFFF);
            Wifi::Write(addr+2, val >> 16);
            return;
        }
        break;

    case 0x06000000:
    case 0x06800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_VWRAM>(addr);
#endif
        GPU::WriteVRAM_ARM7<u32>(addr, val);
        return;

    case 0x08000000:
    case 0x09000000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            // Note: the lower bound is adjusted such that a write starting
            // there will hit the first byte of the GPIO region.
            if ((addr & 0x00FFFFFF) >= 0xC1 && (addr & 0x00FFFFFF) <= 0xC9)
            {
                GBACart::WriteGPIO(addr & (GBACart::CartROMSize-1), val & 0xFF);
                GBACart::WriteGPIO((addr + 2) & (GBACart::CartROMSize-1), (val >> 16) & 0xFF);
                return;
            }
        }
        break;

    case 0x0A000000:
        if (!(ExMemCnt[0] & (1<<7))) return; // deselected CPU, skip the write
        if (GBACart::CartInserted)
        {
            GBACart_SRAM::Write32(addr & (GBACart_SRAM::SRAMLength-1), val);
        }
        return;
    }

    //printf("unknown arm7 write32 %08X %08X @ %08X\n", addr, val, ARM7->R[15]);
}

bool ARM7GetMemRegion(u32 addr, bool write, MemRegion* region)
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
        if (ARM7->R[15] < 0x4000 && (addr >= ARM7BIOSProt || ARM7->R[15] < ARM7BIOSProt))
        {
            region->Mem = ARM7BIOS;
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

u8 ARM9IORead8(u32 addr)
{
    switch (addr)
    {
    case 0x04000130: return KeyInput & 0xFF;
    case 0x04000131: return (KeyInput >> 8) & 0xFF;
    case 0x04000132: return KeyCnt & 0xFF;
    case 0x04000133: return KeyCnt >> 8;

    case 0x040001A2: return NDSCart::ReadSPIData();

    case 0x040001A8: return NDSCart::ROMCommand[0];
    case 0x040001A9: return NDSCart::ROMCommand[1];
    case 0x040001AA: return NDSCart::ROMCommand[2];
    case 0x040001AB: return NDSCart::ROMCommand[3];
    case 0x040001AC: return NDSCart::ROMCommand[4];
    case 0x040001AD: return NDSCart::ROMCommand[5];
    case 0x040001AE: return NDSCart::ROMCommand[6];
    case 0x040001AF: return NDSCart::ROMCommand[7];

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

    printf("unknown ARM9 IO read8 %08X %08X\n", addr, ARM9->R[15]);
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
    case 0x04000132: return KeyCnt;

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

    case 0x040001A8: return NDSCart::ROMCommand[0] |
                           (NDSCart::ROMCommand[1] << 8);
    case 0x040001AA: return NDSCart::ROMCommand[2] |
                           (NDSCart::ROMCommand[3] << 8);
    case 0x040001AC: return NDSCart::ROMCommand[4] |
                           (NDSCart::ROMCommand[5] << 8);
    case 0x040001AE: return NDSCart::ROMCommand[6] |
                           (NDSCart::ROMCommand[7] << 8);

    case 0x04000204: return ExMemCnt[0];
    case 0x04000208: return IME[0];
    case 0x04000210: return IE[0] & 0xFFFF;
    case 0x04000212: return IE[0] >> 16;

    case 0x04000240: return GPU::VRAMCNT[0] | (GPU::VRAMCNT[1] << 8);
    case 0x04000242: return GPU::VRAMCNT[2] | (GPU::VRAMCNT[3] << 8);
    case 0x04000244: return GPU::VRAMCNT[4] | (GPU::VRAMCNT[5] << 8);
    case 0x04000246: return GPU::VRAMCNT[6] | (WRAMCnt << 8);
    case 0x04000248: return GPU::VRAMCNT[7] | (GPU::VRAMCNT[8] << 8);

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
    }

    if ((addr >= 0x04000000 && addr < 0x04000060) || (addr == 0x0400006C))
    {
        return GPU::GPU2D_A->Read16(addr);
    }
    if ((addr >= 0x04001000 && addr < 0x04001060) || (addr == 0x0400106C))
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

    case 0x040000F4: return 0; // ???? Golden Sun Dark Dawn keeps reading this

    case 0x04000100: return TimerGetCounter(0) | (Timers[0].Cnt << 16);
    case 0x04000104: return TimerGetCounter(1) | (Timers[1].Cnt << 16);
    case 0x04000108: return TimerGetCounter(2) | (Timers[2].Cnt << 16);
    case 0x0400010C: return TimerGetCounter(3) | (Timers[3].Cnt << 16);

    case 0x04000130: return (KeyInput & 0xFFFF) | (KeyCnt << 16);

    case 0x04000180: return IPCSync9;

    case 0x040001A0: return NDSCart::SPICnt | (NDSCart::ReadSPIData() << 16);
    case 0x040001A4: return NDSCart::ROMCnt;

    case 0x040001A8: return NDSCart::ROMCommand[0] |
                           (NDSCart::ROMCommand[1] << 8) |
                           (NDSCart::ROMCommand[2] << 16) |
                           (NDSCart::ROMCommand[3] << 24);
    case 0x040001AC: return NDSCart::ROMCommand[4] |
                           (NDSCart::ROMCommand[5] << 8) |
                           (NDSCart::ROMCommand[6] << 16) |
                           (NDSCart::ROMCommand[7] << 24);

    case 0x04000208: return IME[0];
    case 0x04000210: return IE[0];
    case 0x04000214: return IF[0];

    case 0x04000240: return GPU::VRAMCNT[0] | (GPU::VRAMCNT[1] << 8) | (GPU::VRAMCNT[2] << 16) | (GPU::VRAMCNT[3] << 24);
    case 0x04000244: return GPU::VRAMCNT[4] | (GPU::VRAMCNT[5] << 8) | (GPU::VRAMCNT[6] << 16) | (WRAMCnt << 24);
    case 0x04000248: return GPU::VRAMCNT[7] | (GPU::VRAMCNT[8] << 8);

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

    if ((addr >= 0x04000000 && addr < 0x04000060) || (addr == 0x0400006C))
    {
        return GPU::GPU2D_A->Read32(addr);
    }
    if ((addr >= 0x04001000 && addr < 0x04001060) || (addr == 0x0400106C))
    {
        return GPU::GPU2D_B->Read32(addr);
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        return GPU3D::Read32(addr);
    }

    printf("unknown ARM9 IO read32 %08X %08X\n", addr, ARM9->R[15]);
    return 0;
}

void ARM9IOWrite8(u32 addr, u8 val)
{
    switch (addr)
    {
    case 0x0400006C:
    case 0x0400006D: GPU::GPU2D_A->Write8(addr, val); return;
    case 0x0400106C:
    case 0x0400106D: GPU::GPU2D_B->Write8(addr, val); return;

    case 0x04000132:
        KeyCnt = (KeyCnt & 0xFF00) | val;
        return;
    case 0x04000133:
        KeyCnt = (KeyCnt & 0x00FF) | (val << 8);
        return;

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

    case 0x04000208: IME[0] = val & 0x1; UpdateIRQ(0); return;

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

    printf("unknown ARM9 IO write8 %08X %02X %08X\n", addr, val, ARM9->R[15]);
}

void ARM9IOWrite16(u32 addr, u16 val)
{
    switch (addr)
    {
    case 0x04000004: GPU::SetDispStat(0, val); return;
    case 0x04000006: GPU::SetVCount(val); return;

    case 0x04000060: GPU3D::Write16(addr, val); return;

    case 0x04000068:
    case 0x0400006A: GPU::GPU2D_A->Write16(addr, val); return;

    case 0x0400006C: GPU::GPU2D_A->Write16(addr, val); return;
    case 0x0400106C: GPU::GPU2D_B->Write16(addr, val); return;

    case 0x040000B8: DMAs[0]->WriteCnt((DMAs[0]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000BA: DMAs[0]->WriteCnt((DMAs[0]->Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000C4: DMAs[1]->WriteCnt((DMAs[1]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000C6: DMAs[1]->WriteCnt((DMAs[1]->Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000D0: DMAs[2]->WriteCnt((DMAs[2]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000D2: DMAs[2]->WriteCnt((DMAs[2]->Cnt & 0x0000FFFF) | (val << 16)); return;
    case 0x040000DC: DMAs[3]->WriteCnt((DMAs[3]->Cnt & 0xFFFF0000) | val); return;
    case 0x040000DE: DMAs[3]->WriteCnt((DMAs[3]->Cnt & 0x0000FFFF) | (val << 16)); return;

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
        KeyCnt = val;
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

    case 0x040001A8:
        NDSCart::ROMCommand[0] = val & 0xFF;
        NDSCart::ROMCommand[1] = val >> 8;
        return;
    case 0x040001AA:
        NDSCart::ROMCommand[2] = val & 0xFF;
        NDSCart::ROMCommand[3] = val >> 8;
        return;
    case 0x040001AC:
        NDSCart::ROMCommand[4] = val & 0xFF;
        NDSCart::ROMCommand[5] = val >> 8;
        return;
    case 0x040001AE:
        NDSCart::ROMCommand[6] = val & 0xFF;
        NDSCart::ROMCommand[7] = val >> 8;
        return;

    case 0x040001B8: ROMSeed0[4] = val & 0x7F; return;
    case 0x040001BA: ROMSeed1[4] = val & 0x7F; return;

    case 0x04000204:
        ExMemCnt[0] = val;
        ExMemCnt[1] = (ExMemCnt[1] & 0x007F) | (val & 0xFF80);
        SetGBASlotTimings();
        return;

    case 0x04000208: IME[0] = val & 0x1; UpdateIRQ(0); return;
    case 0x04000210: IE[0] = (IE[0] & 0xFFFF0000) | val; UpdateIRQ(0); return;
    case 0x04000212: IE[0] = (IE[0] & 0x0000FFFF) | (val << 16); UpdateIRQ(0); return;
    // TODO: what happens when writing to IF this way??

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
        PowerControl9 = val & 0x820F;
        GPU::SetPowerCnt(PowerControl9);
        return;
    }

    if (addr >= 0x04000000 && addr < 0x04000060)
    {
        GPU::GPU2D_A->Write16(addr, val);
        return;
    }
    if (addr >= 0x04001000 && addr < 0x04001060)
    {
        GPU::GPU2D_B->Write16(addr, val);
        return;
    }
    if (addr >= 0x04000320 && addr < 0x040006A4)
    {
        GPU3D::Write16(addr, val);
        return;
    }

    printf("unknown ARM9 IO write16 %08X %04X %08X\n", addr, val, ARM9->R[15]);
}

void ARM9IOWrite32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x04000004:
        GPU::SetDispStat(0, val & 0xFFFF);
        GPU::SetVCount(val >> 16);
        return;

    case 0x04000060: GPU3D::Write32(addr, val); return;
    case 0x04000064:
    case 0x04000068: GPU::GPU2D_A->Write32(addr, val); return;

    case 0x0400006C: GPU::GPU2D_A->Write16(addr, val&0xFFFF); return;
    case 0x0400106C: GPU::GPU2D_B->Write16(addr, val&0xFFFF); return;

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

    case 0x04000130:
        KeyCnt = val >> 16;
        return;
    case 0x04000180:
        ARM9IOWrite16(addr, val);
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

    case 0x040001A8:
        NDSCart::ROMCommand[0] = val & 0xFF;
        NDSCart::ROMCommand[1] = (val >> 8) & 0xFF;
        NDSCart::ROMCommand[2] = (val >> 16) & 0xFF;
        NDSCart::ROMCommand[3] = val >> 24;
        return;
    case 0x040001AC:
        NDSCart::ROMCommand[4] = val & 0xFF;
        NDSCart::ROMCommand[5] = (val >> 8) & 0xFF;
        NDSCart::ROMCommand[6] = (val >> 16) & 0xFF;
        NDSCart::ROMCommand[7] = val >> 24;
        return;

    case 0x040001B0: *(u32*)&ROMSeed0[0] = val; return;
    case 0x040001B4: *(u32*)&ROMSeed1[0] = val; return;

    case 0x04000208: IME[0] = val & 0x1; UpdateIRQ(0); return;
    case 0x04000210: IE[0] = val; UpdateIRQ(0); return;
    case 0x04000214: IF[0] &= ~val; GPU3D::CheckFIFOIRQ(); UpdateIRQ(0); return;

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
        GPU::SetPowerCnt(PowerControl9);
        return;

    case 0x04100010:
        NDSCart::WriteROMData(val);
        return;
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

    printf("unknown ARM9 IO write32 %08X %08X %08X\n", addr, val, ARM9->R[15]);
}


u8 ARM7IORead8(u32 addr)
{
    switch (addr)
    {
    case 0x04000130: return KeyInput & 0xFF;
    case 0x04000131: return (KeyInput >> 8) & 0xFF;
    case 0x04000132: return KeyCnt & 0xFF;
    case 0x04000133: return KeyCnt >> 8;
    case 0x04000134: return RCnt & 0xFF;
    case 0x04000135: return RCnt >> 8;
    case 0x04000136: return (KeyInput >> 16) & 0xFF;
    case 0x04000137: return KeyInput >> 24;

    case 0x04000138: return RTC::Read() & 0xFF;

    case 0x040001A2: return NDSCart::ReadSPIData();

    case 0x040001A8: return NDSCart::ROMCommand[0];
    case 0x040001A9: return NDSCart::ROMCommand[1];
    case 0x040001AA: return NDSCart::ROMCommand[2];
    case 0x040001AB: return NDSCart::ROMCommand[3];
    case 0x040001AC: return NDSCart::ROMCommand[4];
    case 0x040001AD: return NDSCart::ROMCommand[5];
    case 0x040001AE: return NDSCart::ROMCommand[6];
    case 0x040001AF: return NDSCart::ROMCommand[7];

    case 0x040001C2: return SPI::ReadData();

    case 0x04000208: return IME[1];

    case 0x04000240: return GPU::VRAMSTAT;
    case 0x04000241: return WRAMCnt;

    case 0x04000300: return PostFlag7;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        return SPU::Read8(addr);
    }

    printf("unknown ARM7 IO read8 %08X %08X\n", addr, ARM7->R[15]);
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
    case 0x04000132: return KeyCnt;
    case 0x04000134: return RCnt;
    case 0x04000136: return KeyInput >> 16;

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

    case 0x040001A8: return NDSCart::ROMCommand[0] |
                           (NDSCart::ROMCommand[1] << 8);
    case 0x040001AA: return NDSCart::ROMCommand[2] |
                           (NDSCart::ROMCommand[3] << 8);
    case 0x040001AC: return NDSCart::ROMCommand[4] |
                           (NDSCart::ROMCommand[5] << 8);
    case 0x040001AE: return NDSCart::ROMCommand[6] |
                           (NDSCart::ROMCommand[7] << 8);

    case 0x040001C0: return SPI::Cnt;
    case 0x040001C2: return SPI::ReadData();

    case 0x04000204: return ExMemCnt[1];
    case 0x04000206: return WifiWaitCnt;

    case 0x04000208: return IME[1];
    case 0x04000210: return IE[1] & 0xFFFF;
    case 0x04000212: return IE[1] >> 16;

    case 0x04000300: return PostFlag7;
    case 0x04000304: return PowerControl7;
    case 0x04000308: return ARM7BIOSProt;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        return SPU::Read16(addr);
    }

    printf("unknown ARM7 IO read16 %08X %08X\n", addr, ARM7->R[15]);
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

    case 0x04000130: return (KeyInput & 0xFFFF) | (KeyCnt << 16);
    case 0x04000134: return RCnt | (KeyCnt & 0xFFFF0000);
    case 0x04000138: return RTC::Read();

    case 0x04000180: return IPCSync7;

    case 0x040001A0: return NDSCart::SPICnt | (NDSCart::ReadSPIData() << 16);
    case 0x040001A4: return NDSCart::ROMCnt;

    case 0x040001A8: return NDSCart::ROMCommand[0] |
                           (NDSCart::ROMCommand[1] << 8) |
                           (NDSCart::ROMCommand[2] << 16) |
                           (NDSCart::ROMCommand[3] << 24);
    case 0x040001AC: return NDSCart::ROMCommand[4] |
                           (NDSCart::ROMCommand[5] << 8) |
                           (NDSCart::ROMCommand[6] << 16) |
                           (NDSCart::ROMCommand[7] << 24);

    case 0x040001C0:
        return SPI::Cnt | (SPI::ReadData() << 16);

    case 0x04000208: return IME[1];
    case 0x04000210: return IE[1];
    case 0x04000214: return IF[1];

    case 0x04000308: return ARM7BIOSProt;

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
        return SPU::Read32(addr);
    }

    printf("unknown ARM7 IO read32 %08X %08X\n", addr, ARM7->R[15]);
    return 0;
}

void ARM7IOWrite8(u32 addr, u8 val)
{
    switch (addr)
    {
    case 0x04000132:
        KeyCnt = (KeyCnt & 0xFF00) | val;
        return;
    case 0x04000133:
        KeyCnt = (KeyCnt & 0x00FF) | (val << 8);
        return;
    case 0x04000134:
        RCnt = (RCnt & 0xFF00) | val;
        return;
    case 0x04000135:
        RCnt = (RCnt & 0x00FF) | (val << 8);
        return;

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

    case 0x04000208: IME[1] = val & 0x1; UpdateIRQ(1); return;

    case 0x04000300:
        if (ARM7->R[15] >= 0x4000)
            return;
        if (!(PostFlag7 & 0x01))
            PostFlag7 = val & 0x01;
        return;

    case 0x04000301:
        val &= 0xC0;
        if      (val == 0x40) printf("!! GBA MODE NOT SUPPORTED\n");
        else if (val == 0x80) ARM7->Halt(1);
        else if (val == 0xC0) EnterSleepMode();
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        SPU::Write8(addr, val);
        return;
    }

    printf("unknown ARM7 IO write8 %08X %02X %08X\n", addr, val, ARM7->R[15]);
}

void ARM7IOWrite16(u32 addr, u16 val)
{
    switch (addr)
    {
    case 0x04000004: GPU::SetDispStat(1, val); return;
    case 0x04000006: GPU::SetVCount(val); return;

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

    case 0x04000132: KeyCnt = val; return;
    case 0x04000134: RCnt = val; return;

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

    case 0x040001A8:
        NDSCart::ROMCommand[0] = val & 0xFF;
        NDSCart::ROMCommand[1] = val >> 8;
        return;
    case 0x040001AA:
        NDSCart::ROMCommand[2] = val & 0xFF;
        NDSCart::ROMCommand[3] = val >> 8;
        return;
    case 0x040001AC:
        NDSCart::ROMCommand[4] = val & 0xFF;
        NDSCart::ROMCommand[5] = val >> 8;
        return;
    case 0x040001AE:
        NDSCart::ROMCommand[6] = val & 0xFF;
        NDSCart::ROMCommand[7] = val >> 8;
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
        SetGBASlotTimings();
        return;
    case 0x04000206:
        SetWifiWaitCnt(val);
        return;

    case 0x04000208: IME[1] = val & 0x1; UpdateIRQ(1); return;
    case 0x04000210: IE[1] = (IE[1] & 0xFFFF0000) | val; UpdateIRQ(1); return;
    case 0x04000212: IE[1] = (IE[1] & 0x0000FFFF) | (val << 16); UpdateIRQ(1); return;
    // TODO: what happens when writing to IF this way??

    case 0x04000300:
        if (ARM7->R[15] >= 0x4000)
            return;
        if (!(PostFlag7 & 0x01))
            PostFlag7 = val & 0x01;
        return;

    case 0x04000304: PowerControl7 = val; return;

    case 0x04000308:
        if (ARM7BIOSProt == 0)
            ARM7BIOSProt = val & 0xFFFE;
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        SPU::Write16(addr, val);
        return;
    }

    printf("unknown ARM7 IO write16 %08X %04X %08X\n", addr, val, ARM7->R[15]);
}

void ARM7IOWrite32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x04000004:
        GPU::SetDispStat(1, val & 0xFFFF);
        GPU::SetVCount(val >> 16);
        return;

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

    case 0x04000130: KeyCnt = val >> 16; return;
    case 0x04000134: RCnt = val & 0xFFFF; return;
    case 0x04000138: RTC::Write(val & 0xFFFF, false); return;

    case 0x04000180:
        ARM7IOWrite16(addr, val);
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

    case 0x040001A8:
        NDSCart::ROMCommand[0] = val & 0xFF;
        NDSCart::ROMCommand[1] = (val >> 8) & 0xFF;
        NDSCart::ROMCommand[2] = (val >> 16) & 0xFF;
        NDSCart::ROMCommand[3] = val >> 24;
        return;
    case 0x040001AC:
        NDSCart::ROMCommand[4] = val & 0xFF;
        NDSCart::ROMCommand[5] = (val >> 8) & 0xFF;
        NDSCart::ROMCommand[6] = (val >> 16) & 0xFF;
        NDSCart::ROMCommand[7] = val >> 24;
        return;

    case 0x040001B0: *(u32*)&ROMSeed0[8] = val; return;
    case 0x040001B4: *(u32*)&ROMSeed1[8] = val; return;

    case 0x04000208: IME[1] = val & 0x1; UpdateIRQ(1); return;
    case 0x04000210: IE[1] = val; UpdateIRQ(1); return;
    case 0x04000214: IF[1] &= ~val; UpdateIRQ(1); return;

    case 0x04000304: PowerControl7 = val & 0xFFFF; return;

    case 0x04000308:
        if (ARM7BIOSProt == 0)
            ARM7BIOSProt = val & 0xFFFE;
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000520)
    {
        SPU::Write32(addr, val);
        return;
    }

    printf("unknown ARM7 IO write32 %08X %08X %08X\n", addr, val, ARM7->R[15]);
}

}
