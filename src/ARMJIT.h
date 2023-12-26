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

#ifndef ARMJIT_H
#define ARMJIT_H

#include <algorithm>
#include <optional>
#include <memory>
#include "types.h"
#include "MemConstants.h"
#include "Args.h"
#include "ARMJIT_Memory.h"

#ifdef JIT_ENABLED
#include "JitBlock.h"

#if defined(__APPLE__) && defined(__aarch64__)
    #include <pthread.h>
#endif

#include "ARMJIT_Compiler.h"

namespace melonDS
{
class ARM;

class JitBlock;
class ARMJIT
{
public:
    ARMJIT(melonDS::NDS& nds, std::optional<JITArgs> jit) noexcept :
        NDS(nds),
        Memory(nds),
        JITCompiler(nds),
        MaxBlockSize(jit.has_value() ? std::clamp(jit->MaxBlockSize, 1u, 32u) : 32),
        LiteralOptimizations(jit.has_value() ? jit->LiteralOptimizations : false),
        BranchOptimizations(jit.has_value() ? jit->BranchOptimizations : false),
        FastMemory(jit.has_value() ? jit->FastMemory : false)
    {}
    ~ARMJIT() noexcept;
    void InvalidateByAddr(u32) noexcept;
    void CheckAndInvalidateWVRAM(int) noexcept;
    void CheckAndInvalidateITCM() noexcept;
    void Reset() noexcept;
    void JitEnableWrite() noexcept;
    void JitEnableExecute() noexcept;
    void CompileBlock(ARM* cpu) noexcept;
    void ResetBlockCache() noexcept;

    template <u32 num, int region>
    void CheckAndInvalidate(u32 addr) noexcept
    {
        u32 localAddr = Memory.LocaliseAddress(region, num, addr);
        if (CodeMemRegions[region][(localAddr & 0x7FFFFFF) / 512].Code & (1 << ((localAddr & 0x1FF) / 16)))
            InvalidateByAddr(localAddr);
    }
    JitBlockEntry LookUpBlock(u32 num, u64* entries, u32 offset, u32 addr) noexcept;
    bool SetupExecutableRegion(u32 num, u32 blockAddr, u64*& entry, u32& start, u32& size) noexcept;
    u32 LocaliseCodeAddress(u32 num, u32 addr) const noexcept;

    ARMJIT_Memory Memory;
private:
    int MaxBlockSize {};
    bool LiteralOptimizations = false;
    bool BranchOptimizations = false;
    bool FastMemory = false;
public:
    melonDS::NDS& NDS;
    TinyVector<u32> InvalidLiterals {};
    friend class ARMJIT_Memory;
    void blockSanityCheck(u32 num, u32 blockAddr, JitBlockEntry entry) noexcept;
    void RetireJitBlock(JitBlock* block) noexcept;

    int GetMaxBlockSize() const noexcept { return MaxBlockSize; }
    bool LiteralOptimizationsEnabled() const noexcept { return LiteralOptimizations; }
    bool BranchOptimizationsEnabled() const noexcept { return BranchOptimizations; }
    bool FastMemoryEnabled() const noexcept { return FastMemory; }

    void SetJITArgs(JITArgs args) noexcept;
    void SetMaxBlockSize(int size) noexcept;
    void SetLiteralOptimizations(bool enabled) noexcept;
    void SetBranchOptimizations(bool enabled) noexcept;
    void SetFastMemory(bool enabled) noexcept;

    Compiler JITCompiler;
    std::unordered_map<u32, JitBlock*> JitBlocks9 {};
    std::unordered_map<u32, JitBlock*> JitBlocks7 {};

    std::unordered_map<u32, JitBlock*> RestoreCandidates {};


    AddressRange CodeIndexITCM[ITCMPhysicalSize / 512] {};
    AddressRange CodeIndexMainRAM[MainRAMMaxSize / 512] {};
    AddressRange CodeIndexSWRAM[SharedWRAMSize / 512] {};
    AddressRange CodeIndexVRAM[0x100000 / 512] {};
    AddressRange CodeIndexARM9BIOS[ARM9BIOSSize / 512] {};
    AddressRange CodeIndexARM7BIOS[ARM7BIOSSize / 512] {};
    AddressRange CodeIndexARM7WRAM[ARM7WRAMSize / 512] {};
    AddressRange CodeIndexARM7WVRAM[0x40000 / 512] {};
    AddressRange CodeIndexBIOS9DSi[0x10000 / 512] {};
    AddressRange CodeIndexBIOS7DSi[0x10000 / 512] {};
    AddressRange CodeIndexNWRAM_A[NWRAMSize / 512] {};
    AddressRange CodeIndexNWRAM_B[NWRAMSize / 512] {};
    AddressRange CodeIndexNWRAM_C[NWRAMSize / 512] {};

    u64 FastBlockLookupITCM[ITCMPhysicalSize / 2] {};
    u64 FastBlockLookupMainRAM[MainRAMMaxSize / 2] {};
    u64 FastBlockLookupSWRAM[SharedWRAMSize / 2] {};
    u64 FastBlockLookupVRAM[0x100000 / 2] {};
    u64 FastBlockLookupARM9BIOS[ARM9BIOSSize / 2] {};
    u64 FastBlockLookupARM7BIOS[ARM7BIOSSize / 2] {};
    u64 FastBlockLookupARM7WRAM[ARM7WRAMSize / 2] {};
    u64 FastBlockLookupARM7WVRAM[0x40000 / 2] {};
    u64 FastBlockLookupBIOS9DSi[0x10000 / 2] {};
    u64 FastBlockLookupBIOS7DSi[0x10000 / 2] {};
    u64 FastBlockLookupNWRAM_A[NWRAMSize / 2] {};
    u64 FastBlockLookupNWRAM_B[NWRAMSize / 2] {};
    u64 FastBlockLookupNWRAM_C[NWRAMSize / 2] {};

    AddressRange* const CodeMemRegions[ARMJIT_Memory::memregions_Count] =
    {
        NULL,
        CodeIndexITCM,
        NULL,
        CodeIndexARM9BIOS,
        CodeIndexMainRAM,
        CodeIndexSWRAM,
        NULL,
        CodeIndexVRAM,
        CodeIndexARM7BIOS,
        CodeIndexARM7WRAM,
        NULL,
        NULL,
        CodeIndexARM7WVRAM,
        CodeIndexBIOS9DSi,
        CodeIndexBIOS7DSi,
        CodeIndexNWRAM_A,
        CodeIndexNWRAM_B,
        CodeIndexNWRAM_C
    };

    u64* const FastBlockLookupRegions[ARMJIT_Memory::memregions_Count] =
    {
        NULL,
        FastBlockLookupITCM,
        NULL,
        FastBlockLookupARM9BIOS,
        FastBlockLookupMainRAM,
        FastBlockLookupSWRAM,
        NULL,
        FastBlockLookupVRAM,
        FastBlockLookupARM7BIOS,
        FastBlockLookupARM7WRAM,
        NULL,
        NULL,
        FastBlockLookupARM7WVRAM,
        FastBlockLookupBIOS9DSi,
        FastBlockLookupBIOS7DSi,
        FastBlockLookupNWRAM_A,
        FastBlockLookupNWRAM_B,
        FastBlockLookupNWRAM_C
    };
};
}

// Defined in assembly
extern "C" void ARM_Dispatch(melonDS::ARM* cpu, melonDS::JitBlockEntry entry);
#else
namespace melonDS
{
class ARM;

// This version is a stub; the methods all do nothing,
// but there's still a Memory member.
class ARMJIT
{
public:
    ARMJIT(melonDS::NDS& nds, std::optional<JITArgs>) noexcept : Memory(nds) {}
    ~ARMJIT() noexcept {}
    void InvalidateByAddr(u32) noexcept {}
    void CheckAndInvalidateWVRAM(int) noexcept {}
    void CheckAndInvalidateITCM() noexcept {}
    void Reset() noexcept {}
    void JitEnableWrite() noexcept {}
    void JitEnableExecute() noexcept {}
    void CompileBlock(ARM*) noexcept {}
    void ResetBlockCache() noexcept {}
    template <u32, int>
    void CheckAndInvalidate(u32 addr) noexcept {}

    ARMJIT_Memory Memory;
};
}
#endif // JIT_ENABLED

#endif // ARMJIT_H

