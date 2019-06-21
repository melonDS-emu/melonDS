#ifndef ARMJIT_H
#define ARMJIT_H

#include "types.h"

#include <string.h>

#include "ARM.h"
#include "ARM_InstrInfo.h"

namespace ARMJIT
{

typedef u32 (*CompiledBlock)();

class RegCache
{

static const int NativeRegAllocOrder[];
static const int NativeRegsCount;

};

struct FetchedInstr
{
    u32 A_Reg(int pos) const
    {
        return (Instr >> pos) & 0xF;
    }

    u32 T_Reg(int pos) const
    {
        return (Instr >> pos) & 0x7;
    }

    u32 Cond() const
    {
        return Instr >> 28;
    }

    u32 Instr;
    u32 NextInstr[2];

    u8 CodeCycles;

    ARMInstrInfo::Info Info;
};

/* 
	Copied from DeSmuME
	Some names where changed to match the nomenclature of melonDS

	Since it's nowhere explained and atleast I needed some time to get behind it,
	here's a summary on how it works:
		more or less all memory locations from which code can be executed are
		represented by an array of function pointers, which point to null or
		a function which executes a block instructions starting from there.

		The most significant 4 bits of each address is ignored. This 28 bit space is
		divided into 0x4000 16 KB blocks, each of which a pointer to the relevant
		place inside the before mentioned arrays. Only half of the bytes need to be
		addressed (ARM address are aligned to 4, Thumb addresses to a 2 byte boundary).

		In case a memory write hits mapped memory, the function block at this
		address is set to null, so it's recompiled the next time it's executed.

		This method has disadvantages, namely that only writing to the
		first instruction of a block marks it as invalid and that memory remapping
        (SWRAM and VRAM) isn't taken into account.
*/

struct BlockCache
{
    CompiledBlock* AddrMapping[2][0x4000] = {0};

    CompiledBlock MainRAM[16*1024*1024/2];
	CompiledBlock SWRAM[0x8000/2]; // Shared working RAM
	CompiledBlock ARM9_ITCM[0x8000/2];
	CompiledBlock ARM9_LCDC[0xA4000/2];
	CompiledBlock ARM9_BIOS[0x8000/2];
	CompiledBlock ARM7_BIOS[0x4000/2];
	CompiledBlock ARM7_WRAM[0x10000/2]; // dedicated ARM7 WRAM
	CompiledBlock ARM7_WIRAM[0x10000/2]; // Wifi
	CompiledBlock ARM7_WVRAM[0x40000/2]; // VRAM allocated as Working RAM
};

extern BlockCache cache;

inline bool IsMapped(u32 num, u32 addr)
{
	return cache.AddrMapping[num][(addr & 0xFFFFFFF) >> 14];
}

inline CompiledBlock LookUpBlock(u32 num, u32 addr)
{
	return cache.AddrMapping[num][(addr & 0xFFFFFFF) >> 14][(addr & 0x3FFF) >> 1];
}

inline void Invalidate16(u32 num, u32 addr)
{
	if (IsMapped(num, addr))
		cache.AddrMapping[num][(addr & 0xFFFFFFF) >> 14][(addr & 0x3FFF) >> 1] = NULL;
}

inline void Invalidate32(u32 num, u32 addr)
{
	if (IsMapped(num, addr))
	{
		CompiledBlock* page = cache.AddrMapping[num][(addr & 0xFFFFFFF) >> 14];
		page[(addr & 0x3FFF) >> 1] = NULL;
		page[((addr + 2) & 0x3FFF) >> 1] = NULL;
	}
}

inline void InsertBlock(u32 num, u32 addr, CompiledBlock func)
{
	cache.AddrMapping[num][(addr & 0xFFFFFFF) >> 14][(addr & 0x3FFF) >> 1] = func;
}

inline void ResetBlocks()
{
	memset(cache.MainRAM, 0, sizeof(cache.MainRAM));
	memset(cache.SWRAM, 0, sizeof(cache.SWRAM));
	memset(cache.ARM9_BIOS, 0, sizeof(cache.ARM9_BIOS));
	memset(cache.ARM9_ITCM, 0, sizeof(cache.ARM9_ITCM));
	memset(cache.ARM9_LCDC, 0, sizeof(cache.ARM9_LCDC));
	memset(cache.ARM7_BIOS, 0, sizeof(cache.ARM7_BIOS));
	memset(cache.ARM7_WIRAM, 0, sizeof(cache.ARM7_WIRAM));
	memset(cache.ARM7_WRAM, 0, sizeof(cache.ARM7_WRAM));
	memset(cache.ARM7_WVRAM, 0, sizeof(cache.ARM7_WVRAM));
}

void Init();
void DeInit();

CompiledBlock CompileBlock(ARM* cpu);

}

#endif