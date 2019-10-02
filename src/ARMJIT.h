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

extern u32 AddrTranslate9[0x2000];
extern u32 AddrTranslate7[0x4000];

const u32 ExeMemSpaceSize = 0x518000; // I hate you C++, sometimes I really hate you...
extern JitBlockEntry FastBlockAccess[ExeMemSpaceSize / 2];

template <u32 num>
inline bool IsMapped(u32 addr)
{
	if (num == 0)
		return AddrTranslate9[(addr & 0xFFFFFFF) >> 15] >= ExeMemRegionSizes[exeMem_Unmapped];
	else
		return AddrTranslate7[(addr & 0xFFFFFFF) >> 14] >= ExeMemRegionSizes[exeMem_Unmapped];
}

template <u32 num>
inline u32 TranslateAddr(u32 addr)
{
	if (num == 0)
		return AddrTranslate9[(addr & 0xFFFFFFF) >> 15] + (addr & 0x7FFF);
	else
		return AddrTranslate7[(addr & 0xFFFFFFF) >> 14] + (addr & 0x3FFF);
}

template <u32 num>
inline JitBlockEntry LookUpBlock(u32 addr)
{
	return FastBlockAccess[TranslateAddr<num>(addr) / 2];
}

void Init();
void DeInit();

void InvalidateByAddr(u32 pseudoPhysical);
void InvalidateAll();

void InvalidateITCM(u32 addr);
void InvalidateByAddr7(u32 addr);

void CompileBlock(ARM* cpu);

void ResetBlockCache();

}

#endif