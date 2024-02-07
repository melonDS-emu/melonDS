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

#ifndef ARMJIT_MEMORY
#define ARMJIT_MEMORY

#include "types.h"
#include "MemConstants.h"

#ifdef JIT_ENABLED
#  include "TinyVector.h"
#  include "ARM.h"
#  if defined(__SWITCH__)
#    include <switch.h>
#  elif defined(_WIN32)
#include <windows.h>
#  else
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <fcntl.h>
#    include <unistd.h>
#    include <signal.h>
#  endif
#else
# include <array>
#endif

namespace melonDS
{
#ifdef JIT_ENABLED
namespace Platform { struct DynamicLibrary; }
class Compiler;
class ARMJIT;
#endif

constexpr u32 RoundUp(u32 size) noexcept
{
#ifdef _WIN32
    return (size + 0xFFFF) & ~0xFFFF;
#else
    return size;
#endif
}

const u32 MemBlockMainRAMOffset = 0;
const u32 MemBlockSWRAMOffset = RoundUp(MainRAMMaxSize);
const u32 MemBlockARM7WRAMOffset = MemBlockSWRAMOffset + RoundUp(SharedWRAMSize);
const u32 MemBlockDTCMOffset = MemBlockARM7WRAMOffset + RoundUp(ARM7WRAMSize);
const u32 MemBlockNWRAM_AOffset = MemBlockDTCMOffset + RoundUp(DTCMPhysicalSize);
const u32 MemBlockNWRAM_BOffset = MemBlockNWRAM_AOffset + RoundUp(NWRAMSize);
const u32 MemBlockNWRAM_COffset = MemBlockNWRAM_BOffset + RoundUp(NWRAMSize);
const u32 MemoryTotalSize = MemBlockNWRAM_COffset + RoundUp(NWRAMSize);

class ARMJIT_Memory
{
public:
    enum
    {
        memregion_Other = 0,
        memregion_ITCM,
        memregion_DTCM,
        memregion_BIOS9,
        memregion_MainRAM,
        memregion_SharedWRAM,
        memregion_IO9,
        memregion_VRAM,
        memregion_BIOS7,
        memregion_WRAM7,
        memregion_IO7,
        memregion_Wifi,
        memregion_VWRAM,

        // DSi
        memregion_BIOS9DSi,
        memregion_BIOS7DSi,
        memregion_NewSharedWRAM_A,
        memregion_NewSharedWRAM_B,
        memregion_NewSharedWRAM_C,

        memregions_Count
    };

#ifdef JIT_ENABLED
public:
    explicit ARMJIT_Memory(melonDS::NDS& nds);
    ~ARMJIT_Memory() noexcept;
    ARMJIT_Memory(const ARMJIT_Memory&) = delete;
    ARMJIT_Memory(ARMJIT_Memory&&) = delete;
    ARMJIT_Memory& operator=(const ARMJIT_Memory&) = delete;
    ARMJIT_Memory& operator=(ARMJIT_Memory&&) = delete;
    void Reset() noexcept;
    void RemapDTCM(u32 newBase, u32 newSize) noexcept;
    void RemapSWRAM() noexcept;
    void RemapNWRAM(int num) noexcept;
    void SetCodeProtection(int region, u32 offset, bool protect) noexcept;

    [[nodiscard]] u8* GetMainRAM() noexcept { return MemoryBase + MemBlockMainRAMOffset; }
    [[nodiscard]] const u8* GetMainRAM() const noexcept { return MemoryBase + MemBlockMainRAMOffset; }

    [[nodiscard]] u8* GetSharedWRAM() noexcept { return MemoryBase + MemBlockSWRAMOffset; }
    [[nodiscard]] const u8* GetSharedWRAM() const noexcept { return MemoryBase + MemBlockSWRAMOffset; }

    [[nodiscard]] u8* GetARM7WRAM() noexcept { return MemoryBase + MemBlockARM7WRAMOffset; }
    [[nodiscard]] const u8* GetARM7WRAM() const noexcept { return MemoryBase + MemBlockARM7WRAMOffset; }

    [[nodiscard]] u8* GetARM9DTCM() noexcept { return MemoryBase + MemBlockDTCMOffset; }
    [[nodiscard]] const u8* GetARM9DTCM() const noexcept { return MemoryBase + MemBlockDTCMOffset; }

    [[nodiscard]] u8* GetNWRAM_A() noexcept { return MemoryBase + MemBlockNWRAM_AOffset; }
    [[nodiscard]] const u8* GetNWRAM_A() const noexcept { return MemoryBase + MemBlockNWRAM_AOffset; }

    [[nodiscard]] u8* GetNWRAM_B() noexcept { return MemoryBase + MemBlockNWRAM_BOffset; }
    [[nodiscard]] const u8* GetNWRAM_B() const noexcept { return MemoryBase + MemBlockNWRAM_BOffset; }

    [[nodiscard]] u8* GetNWRAM_C() noexcept { return MemoryBase + MemBlockNWRAM_COffset; }
    [[nodiscard]] const u8* GetNWRAM_C() const noexcept { return MemoryBase + MemBlockNWRAM_COffset; }

    int ClassifyAddress9(u32 addr) const noexcept;
    int ClassifyAddress7(u32 addr) const noexcept;
    bool GetMirrorLocation(int region, u32 num, u32 addr, u32& memoryOffset, u32& mirrorStart, u32& mirrorSize) const noexcept;
    u32 LocaliseAddress(int region, u32 num, u32 addr) const noexcept;
    bool IsFastmemCompatible(int region) const noexcept;
    void* GetFuncForAddr(ARM* cpu, u32 addr, bool store, int size) const noexcept;
    bool MapAtAddress(u32 addr) noexcept;
private:
    friend class Compiler;
    struct Mapping
    {
        u32 Addr;
        u32 Size, LocalOffset;
        u32 Num;

        void Unmap(int region, NDS& nds) noexcept;
    };

    struct FaultDescription
    {
        u32 EmulatedFaultAddr;
        u8* FaultPC;
    };
    static bool FaultHandler(FaultDescription& faultDesc, melonDS::NDS& nds);
    bool MapIntoRange(u32 addr, u32 num, u32 offset, u32 size) noexcept;
    bool UnmapFromRange(u32 addr, u32 num, u32 offset, u32 size) noexcept;
    void SetCodeProtectionRange(u32 addr, u32 size, u32 num, int protection) noexcept;

    melonDS::NDS& NDS;
    void* FastMem9Start;
    void* FastMem7Start;
    u8* MemoryBase = nullptr;
#if defined(__SWITCH__)
    VirtmemReservation* FastMem9Reservation, *FastMem7Reservation;
    u8* MemoryBaseCodeMem;
#elif defined(_WIN32)
    static LONG ExceptionHandler(EXCEPTION_POINTERS* exceptionInfo);
    HANDLE MemoryFile = INVALID_HANDLE_VALUE;
    LPVOID ExceptionHandlerHandle = nullptr;
#else
    static void SigsegvHandler(int sig, siginfo_t* info, void* rawContext);
    int MemoryFile = -1;
#endif
#ifdef ANDROID
    Platform::DynamicLibrary* Libandroid = nullptr;
#endif
    u8 MappingStatus9[1 << (32-12)] {};
    u8 MappingStatus7[1 << (32-12)] {};
    TinyVector<Mapping> Mappings[memregions_Count] {};
#else
public:
    explicit ARMJIT_Memory(melonDS::NDS&) {};
    ~ARMJIT_Memory() = default;
    ARMJIT_Memory(const ARMJIT_Memory&) = delete;
    ARMJIT_Memory(ARMJIT_Memory&&) = delete;
    ARMJIT_Memory& operator=(const ARMJIT_Memory&) = delete;
    ARMJIT_Memory& operator=(ARMJIT_Memory&&) = delete;

    void Reset() noexcept {}
    void RemapDTCM(u32 newBase, u32 newSize) noexcept {}
    void RemapSWRAM() noexcept {}
    void RemapNWRAM(int num) noexcept {}
    void SetCodeProtection(int region, u32 offset, bool protect) noexcept {}

    [[nodiscard]] u8* GetMainRAM() noexcept { return MainRAM.data(); }
    [[nodiscard]] const u8* GetMainRAM() const noexcept { return MainRAM.data(); }

    [[nodiscard]] u8* GetSharedWRAM() noexcept { return SharedWRAM.data(); }
    [[nodiscard]] const u8* GetSharedWRAM() const noexcept { return SharedWRAM.data(); }

    [[nodiscard]] u8* GetARM7WRAM() noexcept { return ARM7WRAM.data(); }
    [[nodiscard]] const u8* GetARM7WRAM() const noexcept { return ARM7WRAM.data(); }

    [[nodiscard]] u8* GetARM9DTCM() noexcept { return DTCM.data(); }
    [[nodiscard]] const u8* GetARM9DTCM() const noexcept { return DTCM.data(); }

    [[nodiscard]] u8* GetNWRAM_A() noexcept { return NWRAM_A.data(); }
    [[nodiscard]] const u8* GetNWRAM_A() const noexcept { return NWRAM_A.data(); }

    [[nodiscard]] u8* GetNWRAM_B() noexcept { return NWRAM_B.data(); }
    [[nodiscard]] const u8* GetNWRAM_B() const noexcept { return NWRAM_B.data(); }

    [[nodiscard]] u8* GetNWRAM_C() noexcept { return NWRAM_C.data(); }
    [[nodiscard]] const u8* GetNWRAM_C() const noexcept { return NWRAM_C.data(); }
private:
    std::array<u8, MainRAMMaxSize> MainRAM {};
    std::array<u8, ARM7WRAMSize> ARM7WRAM {};
    std::array<u8, SharedWRAMSize> SharedWRAM {};
    std::array<u8, DTCMPhysicalSize> DTCM {};
    std::array<u8, NWRAMSize> NWRAM_A {};
    std::array<u8, NWRAMSize> NWRAM_B {};
    std::array<u8, NWRAMSize> NWRAM_C {};
#endif
};
}
#endif
