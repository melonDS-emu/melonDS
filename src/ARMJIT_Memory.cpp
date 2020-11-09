#if defined(__SWITCH__)
#include "switch/compat_switch.h"
#elif defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
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

#include <malloc.h>

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

#if defined(__SWITCH__)
// with LTO the symbols seem to be not properly overriden
// if they're somewhere else

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

    if (ARMJIT_Memory::FaultHandler(desc, offset))
    {
        integerRegisters[32] = (u64)desc.FaultPC;

        ARM_RestoreContext(integerRegisters);	
    }

    if (ctx->pc.x >= (u64)&__start__ && ctx->pc.x < (u64)&__rodata_start)
    {
        printf("unintentional fault in .text at 0x%x (type %d) (trying to access 0x%x?)\n", 
            ctx->pc.x - (u64)&__start__, ctx->error_desc, ctx->far.x);
    }
    else
    {
        printf("unintentional fault somewhere in deep (address) space at %x (type %d)\n", ctx->pc.x, ctx->error_desc);
    }
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
    desc.FaultPC = (u8*)exceptionInfo->ContextRecord->Rip;

    if (ARMJIT_Memory::FaultHandler(desc))
    {
        exceptionInfo->ContextRecord->Rip = (u64)desc.FaultPC;
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
#ifdef __x86_64__
    desc.EmulatedFaultAddr = (u8*)info->si_addr - curArea;
    desc.FaultPC = (u8*)context->uc_mcontext.gregs[REG_RIP];
#else
    desc.EmulatedFaultAddr = (u8*)context->uc_mcontext.fault_address - curArea;
    desc.FaultPC = (u8*)context->uc_mcontext.pc;
#endif

    if (ARMJIT_Memory::FaultHandler(desc))
    {
#ifdef __x86_64__
        context->uc_mcontext.gregs[REG_RIP] = (u64)desc.FaultPC;
#else
        context->uc_mcontext.pc = (u64)desc.FaultPC;
#endif
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
u8* MemoryBase;
u8* MemoryBaseCodeMem;
#elif defined(_WIN32)
u8* MemoryBase;
HANDLE MemoryFile;
LPVOID ExceptionHandlerHandle;
#else
u8* MemoryBase;
int MemoryFile;
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
        bool skipDTCM = Num == 0 && region != memregion_DTCM;
        u8* statuses = Num == 0 ? MappingStatus9 : MappingStatus7;
        u32 offset = 0;
        while (offset < Size)
        {
            if (skipDTCM && Addr + offset == NDS::ARM9->DTCMBase)
            {
                offset += NDS::ARM9->DTCMSize;
            }
            else
            {
                u32 segmentOffset = offset;
                u8 status = statuses[(Addr + offset) >> 12];
                while (statuses[(Addr + offset) >> 12] == status
                    && offset < Size
                    && (!skipDTCM || Addr + offset != NDS::ARM9->DTCMBase))
                {
                    assert(statuses[(Addr + offset) >> 12] != memstate_Unmapped);
                    statuses[(Addr + offset) >> 12] = memstate_Unmapped;
                    offset += 0x1000;
                }

#ifdef __SWITCH__
                if (status == memstate_MappedRW)
                {
                    u32 segmentSize = offset - segmentOffset;
                    printf("unmapping %x %x %x %x\n", Addr + segmentOffset, Num, segmentOffset + LocalOffset + OffsetsPerRegion[region], segmentSize);
                    bool success = UnmapFromRange(Addr + segmentOffset, Num, segmentOffset + LocalOffset + OffsetsPerRegion[region], segmentSize);
                    assert(success);
                }
#endif
            }
        }
#ifndef __SWITCH__
        bool succeded = UnmapFromRange(Addr, Num, OffsetsPerRegion[region] + LocalOffset, Size);
        assert(succeded);
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
            && effectiveAddr >= NDS::ARM9->DTCMBase
            && effectiveAddr < (NDS::ARM9->DTCMBase + NDS::ARM9->DTCMSize))
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
    u32 oldDTCBEnd = oldDTCMBase + NDS::ARM9->DTCMSize;

    u32 newEnd = newBase + newSize;

    printf("remapping DTCM %x %x %x %x\n", newBase, newEnd, oldDTCMBase, oldDTCBEnd);
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

            printf("unmapping %d %x %x %x %x\n", region, mapping.Addr, mapping.Size, mapping.Num, mapping.LocalOffset);

            bool oldOverlap = NDS::ARM9->DTCMSize > 0 && !(oldDTCMBase >= end || oldDTCBEnd <= start);
            bool newOverlap = newSize > 0 && !(newBase >= end || newEnd <= start);

            if (mapping.Num == 0 && (oldOverlap || newOverlap))
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
        if (!(DSi::NWRAMStart[mapping.Num][num] >= mapping.Addr + mapping.Size
            || DSi::NWRAMEnd[mapping.Num][num] < mapping.Addr))
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
    printf("remapping SWRAM\n");
    for (int i = 0; i < Mappings[memregion_WRAM7].Length;)
    {
        Mapping& mapping = Mappings[memregion_WRAM7][i];
        if (mapping.Addr + mapping.Size < 0x03800000)
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
    printf("trying to create mapping %x, %x %x %d %d\n", mirrorStart, mirrorSize, memoryOffset, region, num);
    bool isExecutable = ARMJIT::CodeMemRegions[region];

#ifndef __SWITCH__
    if (num == 0)
    {
        // if a DTCM mapping is mapped before the mapping below it
        // we unmap it, so it won't just be overriden
        for (int i = 0; i < Mappings[memregion_DTCM].Length; i++)
        {
            Mapping& mapping = Mappings[memregion_DTCM][i];
            if (mirrorStart < mapping.Addr + mapping.Size && mirrorStart + mirrorSize >= mapping.Addr)
            {
                mapping.Unmap(memregion_DTCM);
            }
        }
        Mappings[memregion_DTCM].Clear();
    }

    bool succeded = MapIntoRange(mirrorStart, num, OffsetsPerRegion[region] + memoryOffset, mirrorSize);
    assert(succeded);
#endif

    ARMJIT::AddressRange* range = ARMJIT::CodeMemRegions[region] + memoryOffset / 512;

    // this overcomplicated piece of code basically just finds whole pieces of code memory
    // which can be mapped
    u32 offset = 0;	
    bool skipDTCM = num == 0 && region != memregion_DTCM;
    while (offset < mirrorSize)
    {
        if (skipDTCM && mirrorStart + offset == NDS::ARM9->DTCMBase)
        {
            SetCodeProtectionRange(NDS::ARM9->DTCMBase, NDS::ARM9->DTCMSize, 0, 0);
            offset += NDS::ARM9->DTCMSize;
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
                printf("trying to map %x (size: %x) from %x\n", mirrorStart + sectionOffset, sectionSize, sectionOffset + memoryOffset + OffsetsPerRegion[region]);
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

    printf("mapped mirror at %08x-%08x\n", mirrorStart, mirrorStart + mirrorSize - 1);

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

void Init()
{
    const u64 AddrSpaceSize = 0x100000000;

#if defined(__SWITCH__)
    MemoryBase = (u8*)memalign(0x1000, MemoryTotalSize);
    MemoryBaseCodeMem = (u8*)virtmemReserve(MemoryTotalSize);

    bool succeded = R_SUCCEEDED(svcMapProcessCodeMemory(envGetOwnProcessHandle(), (u64)MemoryBaseCodeMem, 
        (u64)MemoryBase, MemoryTotalSize));
    assert(succeded);
    succeded = R_SUCCEEDED(svcSetProcessMemoryPermission(envGetOwnProcessHandle(), (u64)MemoryBaseCodeMem, 
        MemoryTotalSize, Perm_Rw));
    assert(succeded);

    // 8 GB of address space, just don't ask...
    FastMem9Start = virtmemReserve(AddrSpaceSize);
    assert(FastMem9Start);
    FastMem7Start = virtmemReserve(AddrSpaceSize);
    assert(FastMem7Start);

    u8* basePtr = MemoryBaseCodeMem;
#elif defined(_WIN32)
    ExceptionHandlerHandle = AddVectoredExceptionHandler(1, ExceptionHandler);

    MemoryFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MemoryTotalSize, NULL);

    MemoryBase = (u8*)VirtualAlloc(NULL, MemoryTotalSize, MEM_RESERVE, PAGE_READWRITE);

    FastMem9Start = VirtualAlloc(NULL, AddrSpaceSize, MEM_RESERVE, PAGE_READWRITE);
    FastMem7Start = VirtualAlloc(NULL, AddrSpaceSize, MEM_RESERVE, PAGE_READWRITE);

    // only free them after they have all been reserved
    // so they can't overlap
    VirtualFree(MemoryBase, 0, MEM_RELEASE);
    VirtualFree(FastMem9Start, 0, MEM_RELEASE);
    VirtualFree(FastMem7Start, 0, MEM_RELEASE);

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

    MemoryFile = memfd_create("melondsfastmem", 0);
    ftruncate(MemoryFile, MemoryTotalSize);

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
    virtmemFree(FastMem9Start, AddrSpaceSize);
    virtmemFree(FastMem7Start, AddrSpaceSize);

    svcUnmapProcessCodeMemory(envGetOwnProcessHandle(), (u64)MemoryBaseCodeMem, (u64)MemoryBase, MemoryTotalSize);
    virtmemFree(MemoryBaseCodeMem, MemoryTotalSize);
    free(MemoryBase);
#elif defined(_WIN32)
    assert(UnmapViewOfFile(MemoryBase));
    CloseHandle(MemoryFile);

    RemoveVectoredExceptionHandler(ExceptionHandlerHandle);
#else
    sigaction(SIGSEGV, &OldSaSegv, nullptr);
#ifdef __APPLE__
    sigaction(SIGBUS, &OldSaBus, nullptr);
#endif

    munmap(MemoryBase, MemoryTotalSize);
    close(MemoryFile);
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

    for (int i = 0; i < sizeof(MappingStatus9); i++)
    {
        assert(MappingStatus9[i] == memstate_Unmapped);
        assert(MappingStatus7[i] == memstate_Unmapped);
    }

    printf("done resetting jit mem\n");
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
    if (region == memregion_DTCM
        || region == memregion_MainRAM
        || region == memregion_NewSharedWRAM_A
        || region == memregion_NewSharedWRAM_B
        || region == memregion_NewSharedWRAM_C
        || region == memregion_SharedWRAM)
        return false;
    //return OffsetsPerRegion[region] != UINT32_MAX;
    return false;
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
    else if (addr >= NDS::ARM9->DTCMBase && addr < (NDS::ARM9->DTCMBase + NDS::ARM9->DTCMSize))
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