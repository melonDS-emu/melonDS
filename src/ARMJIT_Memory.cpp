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

#if defined(__SWITCH__)
#include <switch.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#endif

#if defined(__ANDROID__)
#include <dlfcn.h>
#include <linux/ashmem.h>
#include <sys/ioctl.h>
#endif

#include "ARMJIT_Memory.h"

#include "ARMJIT_Internal.h"
#include "ARMJIT_Compiler.h"

#include "DSi.h"
#include "GPU.h"
#include "GPU3D.h"
#include "Wifi.h"
#include "NDSCart.h"
#include "SPU.h"

#include <stdlib.h>

using Platform::Log;
using Platform::LogLevel;

/*
    We're handling fastmem here.

    Basically we're repurposing a big piece of virtual memory
    and map the memory regions as they're structured on the DS
    in it.

    On most systems you have a single piece of main ram,
    maybe some video ram and faster cache RAM and that's about it.
    Here we have not only a lot more different memory regions,
    but also two address spaces. Not only that but they all have
    mirrors (the worst case is 16kb SWRAM which is mirrored 1024x).

    We handle this by only mapping those regions which are actually
    used and by praying the games don't go wild.

    Beware, this file is full of platform specific code and copied
    from Dolphin, so enjoy the copied comments!

*/

namespace ARMJIT_Memory
{
struct FaultDescription
{
    u32 EmulatedFaultAddr;
    u8* FaultPC;
};

bool FaultHandler(FaultDescription& faultDesc);
}

// Yes I know this looks messy, but better here than somewhere else in the code
#if defined(__x86_64__)
    #if defined(_WIN32)
        #define CONTEXT_PC Rip
    #elif defined(__linux__)
        #define CONTEXT_PC uc_mcontext.gregs[REG_RIP]
    #elif defined(__APPLE__)
        #define CONTEXT_PC uc_mcontext->__ss.__rip
    #elif defined(__FreeBSD__)
        #define CONTEXT_PC uc_mcontext.mc_rip
    #elif defined(__NetBSD__)
        #define CONTEXT_PC uc_mcontext.__gregs[_REG_RIP]
    #endif
#elif defined(__aarch64__)
    #if defined(_WIN32)
        #define CONTEXT_PC Pc
    #elif defined(__linux__)
        #define CONTEXT_PC uc_mcontext.pc
    #elif defined(__APPLE__)
        #define CONTEXT_PC uc_mcontext->__ss.__pc
    #elif defined(__FreeBSD__)
        #define CONTEXT_PC uc_mcontext.mc_gpregs.gp_elr
    #elif defined(__NetBSD__)
        #define CONTEXT_PC uc_mcontext.__gregs[_REG_PC]
    #endif
#endif

#if defined(__ANDROID__)
#define ASHMEM_DEVICE "/dev/ashmem"
Platform::DynamicLibrary* Libandroid = nullptr;
#endif

#if defined(__SWITCH__)
// with LTO the symbols seem to be not properly overriden
// if they're somewhere else

void HandleFault(u64 pc, u64 lr, u64 fp, u64 faultAddr, u32 desc);

extern "C"
{

void ARM_RestoreContext(u64* registers) __attribute__((noreturn));

extern char __start__;
extern char __rodata_start;

alignas(16) u8 __nx_exception_stack[0x8000];
u64 __nx_exception_stack_size = 0x8000;

void __libnx_exception_handler(ThreadExceptionDump* ctx)
{
    ARMJIT_Memory::FaultDescription desc;
    u8* curArea = (u8*)(NDS::CurCPU == 0 ? ARMJIT_Memory::FastMem9Start : ARMJIT_Memory::FastMem7Start);
    desc.EmulatedFaultAddr = (u8*)ctx->far.x - curArea;
    desc.FaultPC = (u8*)ctx->pc.x;

    u64 integerRegisters[33];
    memcpy(integerRegisters, &ctx->cpu_gprs[0].x, 8*29);
    integerRegisters[29] = ctx->fp.x;
    integerRegisters[30] = ctx->lr.x;
    integerRegisters[31] = ctx->sp.x;
    integerRegisters[32] = ctx->pc.x;

    if (ARMJIT_Memory::FaultHandler(desc))
    {
        integerRegisters[32] = (u64)desc.FaultPC;

        ARM_RestoreContext(integerRegisters);
    }

    HandleFault(ctx->pc.x, ctx->lr.x, ctx->fp.x, ctx->far.x, ctx->error_desc);
}

}

#elif defined(_WIN32)

static LONG ExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
{
    if (exceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    ARMJIT_Memory::FaultDescription desc;
    u8* curArea = (u8*)(NDS::CurCPU == 0 ? ARMJIT_Memory::FastMem9Start : ARMJIT_Memory::FastMem7Start);
    desc.EmulatedFaultAddr = (u8*)exceptionInfo->ExceptionRecord->ExceptionInformation[1] - curArea;
    desc.FaultPC = (u8*)exceptionInfo->ContextRecord->CONTEXT_PC;

    if (ARMJIT_Memory::FaultHandler(desc))
    {
        exceptionInfo->ContextRecord->CONTEXT_PC = (u64)desc.FaultPC;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

#else

static struct sigaction OldSaSegv;
static struct sigaction OldSaBus;

static void SigsegvHandler(int sig, siginfo_t* info, void* rawContext)
{
    if (sig != SIGSEGV && sig != SIGBUS)
    {
        // We are not interested in other signals - handle it as usual.
        return;
    }
    if (info->si_code != SEGV_MAPERR && info->si_code != SEGV_ACCERR)
    {
        // Huh? Return.
        return;
    }

    ucontext_t* context = (ucontext_t*)rawContext;

    ARMJIT_Memory::FaultDescription desc;
    u8* curArea = (u8*)(NDS::CurCPU == 0 ? ARMJIT_Memory::FastMem9Start : ARMJIT_Memory::FastMem7Start);

    desc.EmulatedFaultAddr = (u8*)info->si_addr - curArea;
    desc.FaultPC = (u8*)context->CONTEXT_PC;

    if (ARMJIT_Memory::FaultHandler(desc))
    {
        context->CONTEXT_PC = (u64)desc.FaultPC;
        return;
    }

    struct sigaction* oldSa;
    if (sig == SIGSEGV)
      oldSa = &OldSaSegv;
    else
      oldSa = &OldSaBus;

    if (oldSa->sa_flags & SA_SIGINFO)
    {
      oldSa->sa_sigaction(sig, info, rawContext);
      return;
    }
    if (oldSa->sa_handler == SIG_DFL)
    {
      signal(sig, SIG_DFL);
      return;
    }
    if (oldSa->sa_handler == SIG_IGN)
    {
      // Ignore signal
      return;
    }
    oldSa->sa_handler(sig);
}

#endif

namespace ARMJIT_Memory
{

void* FastMem9Start, *FastMem7Start;

#ifdef _WIN32
inline u32 RoundUp(u32 size)
{
    return (size + 0xFFFF) & ~0xFFFF;
}
#else
inline u32 RoundUp(u32 size)
{
    return size;
}
#endif

const u32 MemBlockMainRAMOffset = 0;
const u32 MemBlockSWRAMOffset = RoundUp(NDS::MainRAMMaxSize);
const u32 MemBlockARM7WRAMOffset = MemBlockSWRAMOffset + RoundUp(NDS::SharedWRAMSize);
const u32 MemBlockDTCMOffset = MemBlockARM7WRAMOffset + RoundUp(NDS::ARM7WRAMSize);
const u32 MemBlockNWRAM_AOffset = MemBlockDTCMOffset + RoundUp(DTCMPhysicalSize);
const u32 MemBlockNWRAM_BOffset = MemBlockNWRAM_AOffset + RoundUp(DSi::NWRAMSize);
const u32 MemBlockNWRAM_COffset = MemBlockNWRAM_BOffset + RoundUp(DSi::NWRAMSize);
const u32 MemoryTotalSize = MemBlockNWRAM_COffset + RoundUp(DSi::NWRAMSize);

const u32 OffsetsPerRegion[memregions_Count] =
{
    UINT32_MAX,
    UINT32_MAX,
    MemBlockDTCMOffset,
    UINT32_MAX,
    MemBlockMainRAMOffset,
    MemBlockSWRAMOffset,
    UINT32_MAX,
    UINT32_MAX,
    UINT32_MAX,
    MemBlockARM7WRAMOffset,
    UINT32_MAX,
    UINT32_MAX,
    UINT32_MAX,
    UINT32_MAX,
    UINT32_MAX,
    MemBlockNWRAM_AOffset,
    MemBlockNWRAM_BOffset,
    MemBlockNWRAM_COffset
};

enum
{
    memstate_Unmapped,
    memstate_MappedRW,
    // on Switch this is unmapped as well
    memstate_MappedProtected,
};

u8 MappingStatus9[1 << (32-12)];
u8 MappingStatus7[1 << (32-12)];

#if defined(__SWITCH__)
VirtmemReservation* FastMem9Reservation, *FastMem7Reservation;
u8* MemoryBase;
u8* MemoryBaseCodeMem;
#elif defined(_WIN32)
u8* MemoryBase;
HANDLE MemoryFile;
LPVOID ExceptionHandlerHandle;
#else
u8* MemoryBase;
int MemoryFile = -1;
#endif

bool MapIntoRange(u32 addr, u32 num, u32 offset, u32 size)
{
    u8* dst = (u8*)(num == 0 ? FastMem9Start : FastMem7Start) + addr;
#ifdef __SWITCH__
    Result r = (svcMapProcessMemory(dst, envGetOwnProcessHandle(),
        (u64)(MemoryBaseCodeMem + offset), size));
    return R_SUCCEEDED(r);
#elif defined(_WIN32)
    bool r = MapViewOfFileEx(MemoryFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, offset, size, dst) == dst;
    return r;
#else
    return mmap(dst, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, MemoryFile, offset) != MAP_FAILED;
#endif
}

bool UnmapFromRange(u32 addr, u32 num, u32 offset, u32 size)
{
    u8* dst = (u8*)(num == 0 ? FastMem9Start : FastMem7Start) + addr;
#ifdef __SWITCH__
    Result r = svcUnmapProcessMemory(dst, envGetOwnProcessHandle(),
        (u64)(MemoryBaseCodeMem + offset), size);
    return R_SUCCEEDED(r);
#elif defined(_WIN32)
    return UnmapViewOfFile(dst);
#else
    return munmap(dst, size) == 0;
#endif
}

#ifndef __SWITCH__
void SetCodeProtectionRange(u32 addr, u32 size, u32 num, int protection)
{
    u8* dst = (u8*)(num == 0 ? FastMem9Start : FastMem7Start) + addr;
#if defined(_WIN32)
    DWORD winProtection, oldProtection;
    if (protection == 0)
        winProtection = PAGE_NOACCESS;
    else if (protection == 1)
        winProtection = PAGE_READONLY;
    else
        winProtection = PAGE_READWRITE;
    bool success = VirtualProtect(dst, size, winProtection, &oldProtection);
    assert(success);
#else
    int posixProt;
    if (protection == 0)
        posixProt = PROT_NONE;
    else if (protection == 1)
        posixProt = PROT_READ;
    else
        posixProt = PROT_READ | PROT_WRITE;
    mprotect(dst, size, posixProt);
#endif
}
#endif

struct Mapping
{
    u32 Addr;
    u32 Size, LocalOffset;
    u32 Num;

    void Unmap(int region)
    {
        u32 dtcmStart = NDS::ARM9->DTCMBase;
        u32 dtcmSize = ~NDS::ARM9->DTCMMask + 1;
        bool skipDTCM = Num == 0 && region != memregion_DTCM;
        u8* statuses = Num == 0 ? MappingStatus9 : MappingStatus7;
        u32 offset = 0;
        while (offset < Size)
        {
            if (skipDTCM && Addr + offset == dtcmStart)
            {
                offset += dtcmSize;
            }
            else
            {
                u32 segmentOffset = offset;
                u8 status = statuses[(Addr + offset) >> 12];
                while (statuses[(Addr + offset) >> 12] == status
                    && offset < Size
                    && (!skipDTCM || Addr + offset != dtcmStart))
                {
                    assert(statuses[(Addr + offset) >> 12] != memstate_Unmapped);
                    statuses[(Addr + offset) >> 12] = memstate_Unmapped;
                    offset += 0x1000;
                }

#ifdef __SWITCH__
                if (status == memstate_MappedRW)
                {
                    u32 segmentSize = offset - segmentOffset;
                    Log(LogLevel::Debug, "unmapping %x %x %x %x\n", Addr + segmentOffset, Num, segmentOffset + LocalOffset + OffsetsPerRegion[region], segmentSize);
                    bool success = UnmapFromRange(Addr + segmentOffset, Num, segmentOffset + LocalOffset + OffsetsPerRegion[region], segmentSize);
                    assert(success);
                }
#endif
            }
        }

#ifndef __SWITCH__
#ifndef _WIN32
        u32 dtcmEnd = dtcmStart + dtcmSize;
        if (Num == 0
            && dtcmEnd >= Addr
            && dtcmStart < Addr + Size)
        {
            bool success;
            if (dtcmStart > Addr)
            {
                success = UnmapFromRange(Addr, 0, OffsetsPerRegion[region] + LocalOffset, dtcmStart - Addr);
                assert(success);
            }
            if (dtcmEnd < Addr + Size)
            {
                u32 offset = dtcmStart - Addr + dtcmSize;
                success = UnmapFromRange(dtcmEnd, 0, OffsetsPerRegion[region] + LocalOffset + offset, Size - offset);
                assert(success);
            }
        }
        else
#endif
        {
            bool succeded = UnmapFromRange(Addr, Num, OffsetsPerRegion[region] + LocalOffset, Size);
            assert(succeded);
        }
#endif
    }
};
ARMJIT::TinyVector<Mapping> Mappings[memregions_Count];

void SetCodeProtection(int region, u32 offset, bool protect)
{
    offset &= ~0xFFF;
    //printf("set code protection %d %x %d\n", region, offset, protect);

    for (int i = 0; i < Mappings[region].Length; i++)
    {
        Mapping& mapping = Mappings[region][i];

        if (offset < mapping.LocalOffset || offset >= mapping.LocalOffset + mapping.Size)
            continue;

        u32 effectiveAddr = mapping.Addr + (offset - mapping.LocalOffset);
        if (mapping.Num == 0
            && region != memregion_DTCM
            && (effectiveAddr & NDS::ARM9->DTCMMask) == NDS::ARM9->DTCMBase)
            continue;

        u8* states = (u8*)(mapping.Num == 0 ? MappingStatus9 : MappingStatus7);

        //printf("%x %d %x %x %x %d\n", effectiveAddr, mapping.Num, mapping.Addr, mapping.LocalOffset, mapping.Size, states[effectiveAddr >> 12]);
        assert(states[effectiveAddr >> 12] == (protect ? memstate_MappedRW : memstate_MappedProtected));
        states[effectiveAddr >> 12] = protect ? memstate_MappedProtected : memstate_MappedRW;

#if defined(__SWITCH__)
        bool success;
        if (protect)
            success = UnmapFromRange(effectiveAddr, mapping.Num, OffsetsPerRegion[region] + offset, 0x1000);
        else
            success = MapIntoRange(effectiveAddr, mapping.Num, OffsetsPerRegion[region] + offset, 0x1000);
        assert(success);
#else
        SetCodeProtectionRange(effectiveAddr, 0x1000, mapping.Num, protect ? 1 : 2);
#endif
    }
}

void RemapDTCM(u32 newBase, u32 newSize)
{
    // this first part could be made more efficient
    // by unmapping DTCM first and then map the holes
    u32 oldDTCMBase = NDS::ARM9->DTCMBase;
    u32 oldDTCMSize = ~NDS::ARM9->DTCMMask + 1;
    u32 oldDTCMEnd = oldDTCMBase + NDS::ARM9->DTCMMask;

    u32 newEnd = newBase + newSize;

    Log(LogLevel::Debug, "remapping DTCM %x %x %x %x\n", newBase, newEnd, oldDTCMBase, oldDTCMEnd);
    // unmap all regions containing the old or the current DTCM mapping
    for (int region = 0; region < memregions_Count; region++)
    {
        if (region == memregion_DTCM)
            continue;

        for (int i = 0; i < Mappings[region].Length;)
        {
            Mapping& mapping = Mappings[region][i];

            u32 start = mapping.Addr;
            u32 end = mapping.Addr + mapping.Size;

            Log(LogLevel::Debug, "unmapping %d %x %x %x %x\n", region, mapping.Addr, mapping.Size, mapping.Num, mapping.LocalOffset);

            bool overlap = (oldDTCMSize > 0 && oldDTCMBase < end && oldDTCMEnd > start)
                || (newSize > 0 && newBase < end && newEnd > start);

            if (mapping.Num == 0 && overlap)
            {
                mapping.Unmap(region);
                Mappings[region].Remove(i);
            }
            else
            {
                i++;
            }
        }
    }

    for (int i = 0; i < Mappings[memregion_DTCM].Length; i++)
    {
        Mappings[memregion_DTCM][i].Unmap(memregion_DTCM);
    }
    Mappings[memregion_DTCM].Clear();
}

void RemapNWRAM(int num)
{
    for (int i = 0; i < Mappings[memregion_SharedWRAM].Length;)
    {
        Mapping& mapping = Mappings[memregion_SharedWRAM][i];
        if (DSi::NWRAMStart[mapping.Num][num] < mapping.Addr + mapping.Size
            && DSi::NWRAMEnd[mapping.Num][num] > mapping.Addr)
        {
            mapping.Unmap(memregion_SharedWRAM);
            Mappings[memregion_SharedWRAM].Remove(i);
        }
        else
        {
            i++;
        }
    }
    for (int i = 0; i < Mappings[memregion_NewSharedWRAM_A + num].Length; i++)
    {
        Mappings[memregion_NewSharedWRAM_A + num][i].Unmap(memregion_NewSharedWRAM_A + num);
    }
    Mappings[memregion_NewSharedWRAM_A + num].Clear();
}

void RemapSWRAM()
{
    Log(LogLevel::Debug, "remapping SWRAM\n");
    for (int i = 0; i < Mappings[memregion_WRAM7].Length;)
    {
        Mapping& mapping = Mappings[memregion_WRAM7][i];
        if (mapping.Addr + mapping.Size <= 0x03800000)
        {
            mapping.Unmap(memregion_WRAM7);
            Mappings[memregion_WRAM7].Remove(i);
        }
        else
            i++;
    }
    for (int i = 0; i < Mappings[memregion_SharedWRAM].Length; i++)
    {
        Mappings[memregion_SharedWRAM][i].Unmap(memregion_SharedWRAM);
    }
    Mappings[memregion_SharedWRAM].Clear();
}

bool MapAtAddress(u32 addr)
{
    u32 num = NDS::CurCPU;

    int region = num == 0
        ? ClassifyAddress9(addr)
        : ClassifyAddress7(addr);

    if (!IsFastmemCompatible(region))
        return false;

    u32 mirrorStart, mirrorSize, memoryOffset;
    bool isMapped = GetMirrorLocation(region, num, addr, memoryOffset, mirrorStart, mirrorSize);
    if (!isMapped)
        return false;

    u8* states = num == 0 ? MappingStatus9 : MappingStatus7;
    //printf("mapping mirror %x, %x %x %d %d\n", mirrorStart, mirrorSize, memoryOffset, region, num);
    bool isExecutable = ARMJIT::CodeMemRegions[region];

    u32 dtcmStart = NDS::ARM9->DTCMBase;
    u32 dtcmSize = ~NDS::ARM9->DTCMMask + 1;
    u32 dtcmEnd = dtcmStart + dtcmSize;
#ifndef __SWITCH__
#ifndef _WIN32
    if (num == 0
        && dtcmEnd >= mirrorStart
        && dtcmStart < mirrorStart + mirrorSize)
    {
        bool success;
        if (dtcmStart > mirrorStart)
        {
            success = MapIntoRange(mirrorStart, 0, OffsetsPerRegion[region] + memoryOffset, dtcmStart - mirrorStart);
            assert(success);
        }
        if (dtcmEnd < mirrorStart + mirrorSize)
        {
            u32 offset = dtcmStart - mirrorStart + dtcmSize;
            success = MapIntoRange(dtcmEnd, 0, OffsetsPerRegion[region] + memoryOffset + offset, mirrorSize - offset);
            assert(success);
        }
    }
    else
#endif
    {
        bool succeded = MapIntoRange(mirrorStart, num, OffsetsPerRegion[region] + memoryOffset, mirrorSize);
        assert(succeded);
    }
#endif

    ARMJIT::AddressRange* range = ARMJIT::CodeMemRegions[region] + memoryOffset / 512;

    // this overcomplicated piece of code basically just finds whole pieces of code memory
    // which can be mapped/protected
    u32 offset = 0;
    bool skipDTCM = num == 0 && region != memregion_DTCM;
    while (offset < mirrorSize)
    {
        if (skipDTCM && mirrorStart + offset == dtcmStart)
        {
#ifdef _WIN32
            SetCodeProtectionRange(dtcmStart, dtcmSize, 0, 0);
#endif
            offset += dtcmSize;
        }
        else
        {
            u32 sectionOffset = offset;
            bool hasCode = isExecutable && ARMJIT::PageContainsCode(&range[offset / 512]);
            while (offset < mirrorSize
                && (!isExecutable || ARMJIT::PageContainsCode(&range[offset / 512]) == hasCode)
                && (!skipDTCM || mirrorStart + offset != NDS::ARM9->DTCMBase))
            {
                assert(states[(mirrorStart + offset) >> 12] == memstate_Unmapped);
                states[(mirrorStart + offset) >> 12] = hasCode ? memstate_MappedProtected : memstate_MappedRW;
                offset += 0x1000;
            }

            u32 sectionSize = offset - sectionOffset;

#if defined(__SWITCH__)
            if (!hasCode)
            {
                //printf("trying to map %x (size: %x) from %x\n", mirrorStart + sectionOffset, sectionSize, sectionOffset + memoryOffset + OffsetsPerRegion[region]);
                bool succeded = MapIntoRange(mirrorStart + sectionOffset, num, sectionOffset + memoryOffset + OffsetsPerRegion[region], sectionSize);
                assert(succeded);
            }
#else
            if (hasCode)
            {
                SetCodeProtectionRange(mirrorStart + sectionOffset, sectionSize, num, 1);
            }
#endif
        }
    }

    assert(num == 0 || num == 1);
    Mapping mapping{mirrorStart, mirrorSize, memoryOffset, num};
    Mappings[region].Add(mapping);

    //printf("mapped mirror at %08x-%08x\n", mirrorStart, mirrorStart + mirrorSize - 1);

    return true;
}

bool FaultHandler(FaultDescription& faultDesc)
{
    if (ARMJIT::JITCompiler->IsJITFault(faultDesc.FaultPC))
    {
        bool rewriteToSlowPath = true;

        u8* memStatus = NDS::CurCPU == 0 ? MappingStatus9 : MappingStatus7;

        if (memStatus[faultDesc.EmulatedFaultAddr >> 12] == memstate_Unmapped)
            rewriteToSlowPath = !MapAtAddress(faultDesc.EmulatedFaultAddr);

        if (rewriteToSlowPath)
            faultDesc.FaultPC = ARMJIT::JITCompiler->RewriteMemAccess(faultDesc.FaultPC);

        return true;
    }
    return false;
}

const u64 AddrSpaceSize = 0x100000000;

void Init()
{
#if defined(__SWITCH__)
    MemoryBase = (u8*)aligned_alloc(0x1000, MemoryTotalSize);
    virtmemLock();
    MemoryBaseCodeMem = (u8*)virtmemFindCodeMemory(MemoryTotalSize, 0x1000);

    bool succeded = R_SUCCEEDED(svcMapProcessCodeMemory(envGetOwnProcessHandle(), (u64)MemoryBaseCodeMem,
        (u64)MemoryBase, MemoryTotalSize));
    assert(succeded);
    succeded = R_SUCCEEDED(svcSetProcessMemoryPermission(envGetOwnProcessHandle(), (u64)MemoryBaseCodeMem,
        MemoryTotalSize, Perm_Rw));
    assert(succeded);

    // 8 GB of address space, just don't ask...
    FastMem9Start = virtmemFindAslr(AddrSpaceSize, 0x1000);
    assert(FastMem9Start);
    FastMem7Start = virtmemFindAslr(AddrSpaceSize, 0x1000);
    assert(FastMem7Start);

    FastMem9Reservation = virtmemAddReservation(FastMem9Start, AddrSpaceSize);
    FastMem7Reservation = virtmemAddReservation(FastMem7Start, AddrSpaceSize);
    virtmemUnlock();

    u8* basePtr = MemoryBaseCodeMem;
#elif defined(_WIN32)
    ExceptionHandlerHandle = AddVectoredExceptionHandler(1, ExceptionHandler);

    MemoryFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MemoryTotalSize, NULL);

    MemoryBase = (u8*)VirtualAlloc(NULL, AddrSpaceSize*4, MEM_RESERVE, PAGE_READWRITE);
    VirtualFree(MemoryBase, 0, MEM_RELEASE);
    // this is incredible hacky
    // but someone else is trying to go into our address space!
    // Windows will very likely give them virtual memory starting at the same address
    // as it is giving us now.
    // That's why we don't use this address, but instead 4gb inwards
    // I know this is terrible
    FastMem9Start = MemoryBase + AddrSpaceSize;
    FastMem7Start = MemoryBase + AddrSpaceSize*2;
    MemoryBase = MemoryBase + AddrSpaceSize*3;

    MapViewOfFileEx(MemoryFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, MemoryTotalSize, MemoryBase);

    u8* basePtr = MemoryBase;
#else
    // this used to be allocated with three different mmaps
    // The idea was to give the OS more freedom where to position the buffers,
    // but something was bad about this so instead we take this vmem eating monster
    // which seems to work better.
    MemoryBase = (u8*)mmap(NULL, AddrSpaceSize*4, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    munmap(MemoryBase, AddrSpaceSize*4);
    FastMem9Start = MemoryBase;
    FastMem7Start = MemoryBase + AddrSpaceSize;
    MemoryBase = MemoryBase + AddrSpaceSize*2;

#if defined(__ANDROID__)
    Libandroid = Platform::DynamicLibrary_Load("libandroid.so");
    using type_ASharedMemory_create = int(*)(const char* name, size_t size);
    auto ASharedMemory_create = reinterpret_cast<type_ASharedMemory_create>(Platform::DynamicLibrary_LoadFunction(Libandroid, "ASharedMemory_create"));

    if (ASharedMemory_create)
    {
        MemoryFile = ASharedMemory_create("melondsfastmem", MemoryTotalSize);
    }
    else
    {
        int fd = open(ASHMEM_DEVICE, O_RDWR);
        ioctl(fd, ASHMEM_SET_NAME, "melondsfastmem");
        ioctl(fd, ASHMEM_SET_SIZE, MemoryTotalSize);
        MemoryFile = fd;
    }
#else
    char fastmemPidName[snprintf(NULL, 0, "/melondsfastmem%d", getpid()) + 1];
    sprintf(fastmemPidName, "/melondsfastmem%d", getpid());
    MemoryFile = shm_open(fastmemPidName, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (MemoryFile == -1)
    {
        Log(LogLevel::Error, "Failed to open memory using shm_open! (%s)", strerror(errno));
    }
    shm_unlink(fastmemPidName);
#endif
    if (ftruncate(MemoryFile, MemoryTotalSize) < 0)
    {
        Log(LogLevel::Error, "Failed to allocate memory using ftruncate! (%s)", strerror(errno));
    }

    struct sigaction sa;
    sa.sa_handler = nullptr;
    sa.sa_sigaction = &SigsegvHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &OldSaSegv);
#ifdef __APPLE__
    sigaction(SIGBUS, &sa, &OldSaBus);
#endif

    mmap(MemoryBase, MemoryTotalSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, MemoryFile, 0);

    u8* basePtr = MemoryBase;
#endif
    NDS::MainRAM = basePtr + MemBlockMainRAMOffset;
    NDS::SharedWRAM = basePtr + MemBlockSWRAMOffset;
    NDS::ARM7WRAM = basePtr + MemBlockARM7WRAMOffset;
    NDS::ARM9->DTCM = basePtr + MemBlockDTCMOffset;
    DSi::NWRAM_A = basePtr + MemBlockNWRAM_AOffset;
    DSi::NWRAM_B = basePtr + MemBlockNWRAM_BOffset;
    DSi::NWRAM_C = basePtr + MemBlockNWRAM_COffset;
}

void DeInit()
{
#if defined(__SWITCH__)
    virtmemLock();
    if (FastMem9Reservation)
        virtmemRemoveReservation(FastMem9Reservation);

    if (FastMem7Reservation)
        virtmemRemoveReservation(FastMem7Reservation);

    FastMem9Reservation = nullptr;
    FastMem7Reservation = nullptr;
    virtmemUnlock();

    svcUnmapProcessCodeMemory(envGetOwnProcessHandle(), (u64)MemoryBaseCodeMem, (u64)MemoryBase, MemoryTotalSize);
    free(MemoryBase);
    MemoryBase = nullptr;
#elif defined(_WIN32)
    if (MemoryBase)
    {
        bool viewUnmapped = UnmapViewOfFile(MemoryBase);
        assert(viewUnmapped);
        MemoryBase = nullptr;
        FastMem9Start = nullptr;
        FastMem7Start = nullptr;
    }

    if (MemoryFile)
    {
        CloseHandle(MemoryFile);
        MemoryFile = INVALID_HANDLE_VALUE;
    }

    if (ExceptionHandlerHandle)
    {
        RemoveVectoredExceptionHandler(ExceptionHandlerHandle);
        ExceptionHandlerHandle = nullptr;
    }
#else
    sigaction(SIGSEGV, &OldSaSegv, nullptr);
#ifdef __APPLE__
    sigaction(SIGBUS, &OldSaBus, nullptr);
#endif
    if (MemoryBase)
    {
        munmap(MemoryBase, MemoryTotalSize);
        MemoryBase = nullptr;
        FastMem9Start = nullptr;
        FastMem7Start = nullptr;
    }

    if (MemoryFile >= 0)
    {
        close(MemoryFile);
        MemoryFile = -1;
    }

#if defined(__ANDROID__)
    if (Libandroid)
    {
        Platform::DynamicLibrary_Unload(Libandroid);
        Libandroid = nullptr;
    }
#endif

#endif
}

void Reset()
{
    for (int region = 0; region < memregions_Count; region++)
    {
        for (int i = 0; i < Mappings[region].Length; i++)
            Mappings[region][i].Unmap(region);
        Mappings[region].Clear();
    }

    for (size_t i = 0; i < sizeof(MappingStatus9); i++)
    {
        assert(MappingStatus9[i] == memstate_Unmapped);
        assert(MappingStatus7[i] == memstate_Unmapped);
    }

    Log(LogLevel::Debug, "done resetting jit mem\n");
}

bool IsFastmemCompatible(int region)
{
#ifdef _WIN32
    /*
        TODO: with some hacks, the smaller shared WRAM regions
        could be mapped in some occaisons as well
    */
    if (region == memregion_DTCM
        || region == memregion_SharedWRAM
        || region == memregion_NewSharedWRAM_B
        || region == memregion_NewSharedWRAM_C)
        return false;
#endif
    return OffsetsPerRegion[region] != UINT32_MAX;
}

bool GetMirrorLocation(int region, u32 num, u32 addr, u32& memoryOffset, u32& mirrorStart, u32& mirrorSize)
{
    memoryOffset = 0;
    switch (region)
    {
    case memregion_ITCM:
        if (num == 0)
        {
            mirrorStart = addr & ~(ITCMPhysicalSize - 1);
            mirrorSize = ITCMPhysicalSize;
            return true;
        }
        return false;
    case memregion_DTCM:
        if (num == 0)
        {
            mirrorStart = addr & ~(DTCMPhysicalSize - 1);
            mirrorSize = DTCMPhysicalSize;
            return true;
        }
        return false;
    case memregion_MainRAM:
        mirrorStart = addr & ~NDS::MainRAMMask;
        mirrorSize = NDS::MainRAMMask + 1;
        return true;
    case memregion_BIOS9:
        if (num == 0)
        {
            mirrorStart = addr & ~0xFFF;
            mirrorSize = 0x1000;
            return true;
        }
        return false;
    case memregion_BIOS7:
        if (num == 1)
        {
            mirrorStart = 0;
            mirrorSize = 0x4000;
            return true;
        }
        return false;
    case memregion_SharedWRAM:
        if (num == 0 && NDS::SWRAM_ARM9.Mem)
        {
            mirrorStart = addr & ~NDS::SWRAM_ARM9.Mask;
            mirrorSize = NDS::SWRAM_ARM9.Mask + 1;
            memoryOffset = NDS::SWRAM_ARM9.Mem - NDS::SharedWRAM;
            return true;
        }
        else if (num == 1 && NDS::SWRAM_ARM7.Mem)
        {
            mirrorStart = addr & ~NDS::SWRAM_ARM7.Mask;
            mirrorSize = NDS::SWRAM_ARM7.Mask + 1;
            memoryOffset = NDS::SWRAM_ARM7.Mem - NDS::SharedWRAM;
            return true;
        }
        return false;
    case memregion_WRAM7:
        if (num == 1)
        {
            mirrorStart = addr & ~(NDS::ARM7WRAMSize - 1);
            mirrorSize = NDS::ARM7WRAMSize;
            return true;
        }
        return false;
    case memregion_VRAM:
        if (num == 0)
        {
            mirrorStart = addr & ~0xFFFFF;
            mirrorSize = 0x100000;
            return true;
        }
        return false;
    case memregion_VWRAM:
        if (num == 1)
        {
            mirrorStart = addr & ~0x3FFFF;
            mirrorSize = 0x40000;
            return true;
        }
        return false;
    case memregion_NewSharedWRAM_A:
        {
            u8* ptr = DSi::NWRAMMap_A[num][(addr >> 16) & DSi::NWRAMMask[num][0]];
            if (ptr)
            {
                memoryOffset = ptr - DSi::NWRAM_A;
                mirrorStart = addr & ~0xFFFF;
                mirrorSize = 0x10000;
                return true;
            }
            return false; // zero filled memory
        }
    case memregion_NewSharedWRAM_B:
        {
            u8* ptr = DSi::NWRAMMap_B[num][(addr >> 15) & DSi::NWRAMMask[num][1]];
            if (ptr)
            {
                memoryOffset = ptr - DSi::NWRAM_B;
                mirrorStart = addr & ~0x7FFF;
                mirrorSize = 0x8000;
                return true;
            }
            return false; // zero filled memory
        }
    case memregion_NewSharedWRAM_C:
        {
            u8* ptr = DSi::NWRAMMap_C[num][(addr >> 15) & DSi::NWRAMMask[num][2]];
            if (ptr)
            {
                memoryOffset = ptr - DSi::NWRAM_C;
                mirrorStart = addr & ~0x7FFF;
                mirrorSize = 0x8000;
                return true;
            }
            return false; // zero filled memory
        }
    case memregion_BIOS9DSi:
        if (num == 0)
        {
            mirrorStart = addr & ~0xFFFF;
            mirrorSize = DSi::SCFG_BIOS & (1<<0) ? 0x8000 : 0x10000;
            return true;
        }
        return false;
    case memregion_BIOS7DSi:
        if (num == 1)
        {
            mirrorStart = addr & ~0xFFFF;
            mirrorSize = DSi::SCFG_BIOS & (1<<8) ? 0x8000 : 0x10000;
            return true;
        }
        return false;
    default:
        assert(false && "For the time being this should only be used for code");
        return false;
    }
}

u32 LocaliseAddress(int region, u32 num, u32 addr)
{
    switch (region)
    {
    case memregion_ITCM:
        return (addr & (ITCMPhysicalSize - 1)) | (memregion_ITCM << 27);
    case memregion_MainRAM:
        return (addr & NDS::MainRAMMask) | (memregion_MainRAM << 27);
    case memregion_BIOS9:
        return (addr & 0xFFF) | (memregion_BIOS9 << 27);
    case memregion_BIOS7:
        return (addr & 0x3FFF) | (memregion_BIOS7 << 27);
    case memregion_SharedWRAM:
        if (num == 0)
            return ((addr & NDS::SWRAM_ARM9.Mask) + (NDS::SWRAM_ARM9.Mem - NDS::SharedWRAM)) | (memregion_SharedWRAM << 27);
        else
            return ((addr & NDS::SWRAM_ARM7.Mask) + (NDS::SWRAM_ARM7.Mem - NDS::SharedWRAM)) | (memregion_SharedWRAM << 27);
    case memregion_WRAM7:
        return (addr & (NDS::ARM7WRAMSize - 1)) | (memregion_WRAM7 << 27);
    case memregion_VRAM:
        // TODO: take mapping properly into account
        return (addr & 0xFFFFF) | (memregion_VRAM << 27);
    case memregion_VWRAM:
        // same here
        return (addr & 0x3FFFF) | (memregion_VWRAM << 27);
    case memregion_NewSharedWRAM_A:
        {
            u8* ptr = DSi::NWRAMMap_A[num][(addr >> 16) & DSi::NWRAMMask[num][0]];
            if (ptr)
                return (ptr - DSi::NWRAM_A + (addr & 0xFFFF)) | (memregion_NewSharedWRAM_A << 27);
            else
                return memregion_Other << 27; // zero filled memory
        }
    case memregion_NewSharedWRAM_B:
        {
            u8* ptr = DSi::NWRAMMap_B[num][(addr >> 15) & DSi::NWRAMMask[num][1]];
            if (ptr)
                return (ptr - DSi::NWRAM_B + (addr & 0x7FFF)) | (memregion_NewSharedWRAM_B << 27);
            else
                return memregion_Other << 27;
        }
    case memregion_NewSharedWRAM_C:
        {
            u8* ptr = DSi::NWRAMMap_C[num][(addr >> 15) & DSi::NWRAMMask[num][2]];
            if (ptr)
                return (ptr - DSi::NWRAM_C + (addr & 0x7FFF)) | (memregion_NewSharedWRAM_C << 27);
            else
                return memregion_Other << 27;
        }
    case memregion_BIOS9DSi:
    case memregion_BIOS7DSi:
        return (addr & 0xFFFF) | (region << 27);
    default:
        assert(false && "This should only be needed for regions which can contain code");
        return memregion_Other << 27;
    }
}

int ClassifyAddress9(u32 addr)
{
    if (addr < NDS::ARM9->ITCMSize)
    {
        return memregion_ITCM;
    }
    else if ((addr & NDS::ARM9->DTCMMask) == NDS::ARM9->DTCMBase)
    {
        return memregion_DTCM;
    }
    else
    {
        if (NDS::ConsoleType == 1 && addr >= 0xFFFF0000 && !(DSi::SCFG_BIOS & (1<<1)))
        {
            if ((addr >= 0xFFFF8000) && (DSi::SCFG_BIOS & (1<<0)))
                return memregion_Other;

            return memregion_BIOS9DSi;
        }
        else if ((addr & 0xFFFFF000) == 0xFFFF0000)
        {
            return memregion_BIOS9;
        }

        switch (addr & 0xFF000000)
        {
        case 0x02000000:
            return memregion_MainRAM;
        case 0x03000000:
            if (NDS::ConsoleType == 1)
            {
                if (addr >= DSi::NWRAMStart[0][0] && addr < DSi::NWRAMEnd[0][0])
                    return memregion_NewSharedWRAM_A;
                if (addr >= DSi::NWRAMStart[0][1] && addr < DSi::NWRAMEnd[0][1])
                    return memregion_NewSharedWRAM_B;
                if (addr >= DSi::NWRAMStart[0][2] && addr < DSi::NWRAMEnd[0][2])
                    return memregion_NewSharedWRAM_C;
            }

            if (NDS::SWRAM_ARM9.Mem)
                return memregion_SharedWRAM;
            return memregion_Other;
        case 0x04000000:
            return memregion_IO9;
        case 0x06000000:
            return memregion_VRAM;
        case 0x0C000000:
            return (NDS::ConsoleType==1) ? memregion_MainRAM : memregion_Other;
        default:
            return memregion_Other;
        }
    }
}

int ClassifyAddress7(u32 addr)
{
    if (NDS::ConsoleType == 1 && addr < 0x00010000 && !(DSi::SCFG_BIOS & (1<<9)))
    {
        if (addr >= 0x00008000 && DSi::SCFG_BIOS & (1<<8))
            return memregion_Other;

        return memregion_BIOS7DSi;
    }
    else if (addr < 0x00004000)
    {
        return memregion_BIOS7;
    }
    else
    {
        switch (addr & 0xFF800000)
        {
        case 0x02000000:
        case 0x02800000:
            return memregion_MainRAM;
        case 0x03000000:
            if (NDS::ConsoleType == 1)
            {
                if (addr >= DSi::NWRAMStart[1][0] && addr < DSi::NWRAMEnd[1][0])
                    return memregion_NewSharedWRAM_A;
                if (addr >= DSi::NWRAMStart[1][1] && addr < DSi::NWRAMEnd[1][1])
                    return memregion_NewSharedWRAM_B;
                if (addr >= DSi::NWRAMStart[1][2] && addr < DSi::NWRAMEnd[1][2])
                    return memregion_NewSharedWRAM_C;
            }

            if (NDS::SWRAM_ARM7.Mem)
                return memregion_SharedWRAM;
            return memregion_WRAM7;
        case 0x03800000:
            return memregion_WRAM7;
        case 0x04000000:
            return memregion_IO7;
        case 0x04800000:
            return memregion_Wifi;
        case 0x06000000:
        case 0x06800000:
            return memregion_VWRAM;
        case 0x0C000000:
        case 0x0C800000:
            return (NDS::ConsoleType==1) ? memregion_MainRAM : memregion_Other;
        default:
            return memregion_Other;
        }
    }
}

void WifiWrite32(u32 addr, u32 val)
{
    Wifi::Write(addr, val & 0xFFFF);
    Wifi::Write(addr + 2, val >> 16);
}

u32 WifiRead32(u32 addr)
{
    return (u32)Wifi::Read(addr) | ((u32)Wifi::Read(addr + 2) << 16);
}

template <typename T>
void VRAMWrite(u32 addr, T val)
{
    switch (addr & 0x00E00000)
    {
    case 0x00000000: GPU::WriteVRAM_ABG<T>(addr, val); return;
    case 0x00200000: GPU::WriteVRAM_BBG<T>(addr, val); return;
    case 0x00400000: GPU::WriteVRAM_AOBJ<T>(addr, val); return;
    case 0x00600000: GPU::WriteVRAM_BOBJ<T>(addr, val); return;
    default: GPU::WriteVRAM_LCDC<T>(addr, val); return;
    }
}
template <typename T>
T VRAMRead(u32 addr)
{
    switch (addr & 0x00E00000)
    {
    case 0x00000000: return GPU::ReadVRAM_ABG<T>(addr);
    case 0x00200000: return GPU::ReadVRAM_BBG<T>(addr);
    case 0x00400000: return GPU::ReadVRAM_AOBJ<T>(addr);
    case 0x00600000: return GPU::ReadVRAM_BOBJ<T>(addr);
    default: return GPU::ReadVRAM_LCDC<T>(addr);
    }
}

void* GetFuncForAddr(ARM* cpu, u32 addr, bool store, int size)
{
    if (cpu->Num == 0)
    {
        switch (addr & 0xFF000000)
        {
        case 0x04000000:
            if (!store && size == 32 && addr == 0x04100010 && NDS::ExMemCnt[0] & (1<<11))
                return (void*)NDSCart::ReadROMData;

            /*
                unfortunately we can't map GPU2D this way
                since it's hidden inside an object

                though GPU3D registers are accessed much more intensive
            */
            if (addr >= 0x04000320 && addr < 0x040006A4)
            {
                switch (size | store)
                {
                case 8: return (void*)GPU3D::Read8;
                case 9: return (void*)GPU3D::Write8;
                case 16: return (void*)GPU3D::Read16;
                case 17: return (void*)GPU3D::Write16;
                case 32: return (void*)GPU3D::Read32;
                case 33: return (void*)GPU3D::Write32;
                }
            }

            if (NDS::ConsoleType == 0)
            {
                switch (size | store)
                {
                case 8: return (void*)NDS::ARM9IORead8;
                case 9: return (void*)NDS::ARM9IOWrite8;
                case 16: return (void*)NDS::ARM9IORead16;
                case 17: return (void*)NDS::ARM9IOWrite16;
                case 32: return (void*)NDS::ARM9IORead32;
                case 33: return (void*)NDS::ARM9IOWrite32;
                }
            }
            else
            {
                switch (size | store)
                {
                case 8: return (void*)DSi::ARM9IORead8;
                case 9: return (void*)DSi::ARM9IOWrite8;
                case 16: return (void*)DSi::ARM9IORead16;
                case 17: return (void*)DSi::ARM9IOWrite16;
                case 32: return (void*)DSi::ARM9IORead32;
                case 33: return (void*)DSi::ARM9IOWrite32;
                }
            }
            break;
        case 0x06000000:
            switch (size | store)
            {
            case 8: return (void*)VRAMRead<u8>;
            case 9: return NULL;
            case 16: return (void*)VRAMRead<u16>;
            case 17: return (void*)VRAMWrite<u16>;
            case 32: return (void*)VRAMRead<u32>;
            case 33: return (void*)VRAMWrite<u32>;
            }
            break;
        }
    }
    else
    {
        switch (addr & 0xFF800000)
        {
        case 0x04000000:
            if (addr >= 0x04000400 && addr < 0x04000520)
            {
                switch (size | store)
                {
                case 8: return (void*)SPU::Read8;
                case 9: return (void*)SPU::Write8;
                case 16: return (void*)SPU::Read16;
                case 17: return (void*)SPU::Write16;
                case 32: return (void*)SPU::Read32;
                case 33: return (void*)SPU::Write32;
                }
            }

            if (NDS::ConsoleType == 0)
            {
                switch (size | store)
                {
                case 8: return (void*)NDS::ARM7IORead8;
                case 9: return (void*)NDS::ARM7IOWrite8;
                case 16: return (void*)NDS::ARM7IORead16;
                case 17: return (void*)NDS::ARM7IOWrite16;
                case 32: return (void*)NDS::ARM7IORead32;
                case 33: return (void*)NDS::ARM7IOWrite32;
                }
            }
            else
            {
                switch (size | store)
                {
                case 8: return (void*)DSi::ARM7IORead8;
                case 9: return (void*)DSi::ARM7IOWrite8;
                case 16: return (void*)DSi::ARM7IORead16;
                case 17: return (void*)DSi::ARM7IOWrite16;
                case 32: return (void*)DSi::ARM7IORead32;
                case 33: return (void*)DSi::ARM7IOWrite32;
                }
            }
            break;
        case 0x04800000:
            if (addr < 0x04810000 && size >= 16)
            {
                switch (size | store)
                {
                case 16: return (void*)Wifi::Read;
                case 17: return (void*)Wifi::Write;
                case 32: return (void*)WifiRead32;
                case 33: return (void*)WifiWrite32;
                }
            }
            break;
        case 0x06000000:
        case 0x06800000:
            switch (size | store)
            {
            case 8: return (void*)GPU::ReadVRAM_ARM7<u8>;
            case 9: return (void*)GPU::WriteVRAM_ARM7<u8>;
            case 16: return (void*)GPU::ReadVRAM_ARM7<u16>;
            case 17: return (void*)GPU::WriteVRAM_ARM7<u16>;
            case 32: return (void*)GPU::ReadVRAM_ARM7<u32>;
            case 33: return (void*)GPU::WriteVRAM_ARM7<u32>;
            }
        }
    }
    return NULL;
}

}
