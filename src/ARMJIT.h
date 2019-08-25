#ifndef ARMJIT_H
#define ARMJIT_H

#include "types.h"

#include "ARM.h"
#include "ARM_InstrInfo.h"

namespace ARMJIT
{

typedef u32 (*CompiledBlock)();

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

	u8 SetFlags;
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
		divided into 0x2000 32 KB for ARM9 and 0x4000 16 KB for ARM7, each of which 
		a pointer to the relevant place inside the afore mentioned arrays. 32 and 16 KB
		are the sizes of the smallest contigous memory region mapped to the respective CPU.
		Because ARM addresses are always aligned to 4 bytes and Thumb to a 2 byte boundary,
		we only need every second half word to be adressable.

		In case a memory write hits mapped memory, the function block at this
		address is set to null, so it's recompiled the next time it's executed.

		This method has disadvantages, namely that only writing to the
		first instruction of a block marks it as invalid and that memory remapping
        (SWRAM and VRAM) isn't taken into account.
*/

struct BlockCache
{
    CompiledBlock* AddrMapping9[0x2000] = {0};
    CompiledBlock* AddrMapping7[0x4000] = {0};

    CompiledBlock MainRAM[4*1024*1024/2];
	CompiledBlock SWRAM[0x8000/2]; // Shared working RAM
	CompiledBlock ARM9_ITCM[0x8000/2];
	CompiledBlock ARM9_LCDC[0xA4000/2];
	CompiledBlock ARM9_BIOS[0x8000/2];
	CompiledBlock ARM7_BIOS[0x4000/2];
	CompiledBlock ARM7_WRAM[0x10000/2]; // dedicated ARM7 WRAM
	CompiledBlock ARM7_WVRAM[0x40000/2]; // VRAM allocated as Working RAM
};

extern BlockCache cache;

template <u32 num>
inline bool IsMapped(u32 addr)
{
	if (num == 0)
		return cache.AddrMapping9[(addr & 0xFFFFFFF) >> 15];
	else
		return cache.AddrMapping7[(addr & 0xFFFFFFF) >> 14];
}

template <u32 num>
inline CompiledBlock LookUpBlock(u32 addr)
{
	if (num == 0)
		return cache.AddrMapping9[(addr & 0xFFFFFFF) >> 15][(addr & 0x7FFF) >> 1];
	else
		return cache.AddrMapping7[(addr & 0xFFFFFFF) >> 14][(addr & 0x3FFF) >> 1];
}

template <u32 num>
inline void Invalidate16(u32 addr)
{
	if (IsMapped<num>(addr))
	{
		if (num == 0)
			cache.AddrMapping9[(addr & 0xFFFFFFF) >> 15][(addr & 0x7FFF) >> 1] = NULL;
		else
			cache.AddrMapping7[(addr & 0xFFFFFFF) >> 14][(addr & 0x3FFF) >> 1] = NULL;
	}
}

template <u32 num>
inline void Invalidate32(u32 addr)
{
	if (IsMapped<num>(addr))
	{
		if (num == 0)
		{
			CompiledBlock* page = cache.AddrMapping9[(addr & 0xFFFFFFF) >> 15];
			page[(addr & 0x7FFF) >> 1] = NULL;
			page[((addr + 2) & 0x7FFF) >> 1] = NULL;
		}
		else
		{
			CompiledBlock* page = cache.AddrMapping7[(addr & 0xFFFFFFF) >> 14];
			page[(addr & 0x3FFF) >> 1] = NULL;
			page[((addr + 2) & 0x3FFF) >> 1] = NULL;
		}
	}
}

template <u32 num>
inline void InsertBlock(u32 addr, CompiledBlock func)
{
	if (num == 0)
		cache.AddrMapping9[(addr & 0xFFFFFFF) >> 15][(addr & 0x7FFF) >> 1] = func;
	else
		cache.AddrMapping7[(addr & 0xFFFFFFF) >> 14][(addr & 0x3FFF) >> 1] = func;
}

void Init();
void DeInit();

CompiledBlock CompileBlock(ARM* cpu);

void InvalidateBlockCache();

}

#endif