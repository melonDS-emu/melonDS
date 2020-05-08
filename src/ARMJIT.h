#ifndef ARMJIT_H
#define ARMJIT_H

#include "types.h"

#include "ARM.h"
#include "ARM_InstrInfo.h"

namespace ARMJIT
{

enum ExeMemKind
{
	exeMem_Unmapped = 0,
	exeMem_ITCM,
	exeMem_MainRAM,
	exeMem_SWRAM,
	exeMem_LCDC,
	exeMem_ARM9_BIOS,
	exeMem_ARM7_BIOS,
	exeMem_ARM7_WRAM,
	exeMem_ARM7_WVRAM,
	exeMem_Count
};

extern const u32 ExeMemRegionOffsets[];
extern const u32 ExeMemRegionSizes[];

typedef u32 (*JitBlockEntry)();

const u32 ExeMemSpaceSize = 0x518000; // I hate you C++, sometimes I really hate you...

u32 TranslateAddr9(u32 addr);
u32 TranslateAddr7(u32 addr);

template <u32 Num>
JitBlockEntry LookUpBlockEntry(u32 addr);

void Init();
void DeInit();

void Reset();

void InvalidateByAddr(u32 pseudoPhysical);

void InvalidateRegionIfNecessary(u32 addr);

inline void InvalidateMainRAMIfNecessary(u32 addr)
{
	InvalidateRegionIfNecessary(ExeMemRegionOffsets[exeMem_MainRAM] + (addr & (MAIN_RAM_SIZE - 1)));
}
inline void InvalidateITCMIfNecessary(u32 addr)
{
	InvalidateRegionIfNecessary(ExeMemRegionOffsets[exeMem_ITCM] + (addr & 0x7FFF));
}
inline void InvalidateLCDCIfNecessary(u32 addr)
{
	if (addr < 0x68A3FFF)
		InvalidateRegionIfNecessary(ExeMemRegionOffsets[exeMem_LCDC] + (addr - 0x6800000));
}
inline void InvalidateSWRAM7IfNecessary(u32 addr)
{
	InvalidateRegionIfNecessary(ExeMemRegionOffsets[exeMem_SWRAM] + (NDS::SWRAM_ARM7 - NDS::SharedWRAM) + (addr & NDS::SWRAM_ARM7Mask));
}
inline void InvalidateSWRAM9IfNecessary(u32 addr)
{
	InvalidateRegionIfNecessary(ExeMemRegionOffsets[exeMem_SWRAM] + (NDS::SWRAM_ARM9 - NDS::SharedWRAM) + (addr & NDS::SWRAM_ARM9Mask));
}
inline void InvalidateARM7WRAMIfNecessary(u32 addr)
{
	InvalidateRegionIfNecessary(ExeMemRegionOffsets[exeMem_ARM7_WRAM] + (addr & 0xFFFF));
}
inline void InvalidateARM7WVRAMIfNecessary(u32 addr)
{
	InvalidateRegionIfNecessary(ExeMemRegionOffsets[exeMem_ARM7_WVRAM] + (addr & 0x1FFFF));
}

void CompileBlock(ARM* cpu);

void ResetBlockCache();

void UpdateMemoryStatus9(u32 start, u32 end);
void UpdateMemoryStatus7(u32 start, u32 end);

}

extern "C" void ARM_Dispatch(ARM* cpu, ARMJIT::JitBlockEntry entry);

#endif