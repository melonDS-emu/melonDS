#include "ARMJIT.h"

#include <string.h>
#include <assert.h>
#include <unordered_map>

#define XXH_STATIC_LINKING_ONLY
#include "xxhash/xxhash.h"

#include "Config.h"

#include "ARMJIT_Internal.h"
#if defined(__x86_64__)
#include "ARMJIT_x64/ARMJIT_Compiler.h"
#elif defined(__aarch64__)
#include "ARMJIT_A64/ARMJIT_Compiler.h"
#else
#error "The current target platform doesn't have a JIT backend"
#endif

#include "ARMInterpreter_ALU.h"
#include "ARMInterpreter_LoadStore.h"
#include "ARMInterpreter_Branch.h"
#include "ARMInterpreter.h"

#include "GPU.h"
#include "GPU3D.h"
#include "SPU.h"
#include "Wifi.h"
#include "NDSCart.h"

namespace ARMJIT
{

#define JIT_DEBUGPRINT(msg, ...)
//#define JIT_DEBUGPRINT(msg, ...) printf(msg, ## __VA_ARGS__)

Compiler* JITCompiler;

const u32 ExeMemRegionSizes[] =
{
	0x8000,			// Unmapped Region (dummy)
	0x8000, 		// ITCM
	4*1024*1024, 	// Main RAM
	0x8000, 		// SWRAM
	0xA4000, 		// LCDC
	0x8000, 		// ARM9 BIOS
	0x4000, 		// ARM7 BIOS
	0x10000,		// ARM7 WRAM
	0x40000			// ARM7 WVRAM
};

const u32 ExeMemRegionOffsets[] =
{
	0,
	0x8000,
	0x10000,
	0x410000,
	0x418000,
	0x4BC000,
	0x4C4000,
	0x4C8000,
	0x4D8000,
	0x518000,
};

/*
	translates address to pseudo physical address
		- more compact, eliminates mirroring, everything comes in a row
		- we only need one translation table
*/

u32 TranslateAddr9(u32 addr)
{
	switch (ClassifyAddress9(addr))
	{
	case memregion_MainRAM: return ExeMemRegionOffsets[exeMem_MainRAM] + (addr & (MAIN_RAM_SIZE - 1));
	case memregion_SWRAM9:
		if (NDS::SWRAM_ARM9)
			return ExeMemRegionOffsets[exeMem_SWRAM] + (NDS::SWRAM_ARM9 - NDS::SharedWRAM) + (addr & NDS::SWRAM_ARM9Mask);
		else
			return 0;
	case memregion_ITCM: return ExeMemRegionOffsets[exeMem_ITCM] + (addr & 0x7FFF);
	case memregion_VRAM: return (addr >= 0x6800000 && addr < 0x68A4000) ? ExeMemRegionOffsets[exeMem_LCDC] + (addr - 0x6800000) : 0;
	case memregion_BIOS9: return ExeMemRegionOffsets[exeMem_ARM9_BIOS] + (addr & 0xFFF);
	default: return 0;
	}
}

u32 TranslateAddr7(u32 addr)
{
	switch (ClassifyAddress7(addr))
	{
	case memregion_MainRAM: return ExeMemRegionOffsets[exeMem_MainRAM] + (addr & (MAIN_RAM_SIZE - 1));
	case memregion_SWRAM7:
		if (NDS::SWRAM_ARM7)
			return ExeMemRegionOffsets[exeMem_SWRAM] + (NDS::SWRAM_ARM7 - NDS::SharedWRAM) + (addr & NDS::SWRAM_ARM7Mask);
		else
			return 0;
	case memregion_BIOS7: return ExeMemRegionOffsets[exeMem_ARM7_BIOS] + addr;
	case memregion_WRAM7: return ExeMemRegionOffsets[exeMem_ARM7_WRAM] + (addr & 0xFFFF);
	case memregion_VWRAM: return ExeMemRegionOffsets[exeMem_ARM7_WVRAM] + (addr & 0x1FFFF);
	default: return 0;
	}
}

AddressRange CodeRanges[ExeMemSpaceSize / 512];

TinyVector<u32> InvalidLiterals;

std::unordered_map<u32, JitBlock*> JitBlocks9;
std::unordered_map<u32, JitBlock*> JitBlocks7;

u8 MemoryStatus9[0x800000];
u8 MemoryStatus7[0x800000];

int ClassifyAddress9(u32 addr)
{
	if (addr < NDS::ARM9->ITCMSize)
		return memregion_ITCM;
	else if (addr >= NDS::ARM9->DTCMBase && addr < (NDS::ARM9->DTCMBase + NDS::ARM9->DTCMSize))
		return memregion_DTCM;
	else if ((addr & 0xFFFFF000) == 0xFFFF0000)
		return memregion_BIOS9;
	else
	{
		switch (addr & 0xFF000000)
		{
		case 0x02000000:
			return memregion_MainRAM;
		case 0x03000000:
			return memregion_SWRAM9;
		case 0x04000000:
			return memregion_IO9;
		case 0x06000000:
			return memregion_VRAM;
		}
	}
	return memregion_Other;
}

int ClassifyAddress7(u32 addr)
{
	if (addr < 0x00004000)
		return memregion_BIOS7;
	else
	{
		switch (addr & 0xFF800000)
		{
		case 0x02000000:
		case 0x02800000:
			return memregion_MainRAM;
		case 0x03000000:
			if (NDS::SWRAM_ARM7)
				return memregion_SWRAM7;
			else
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
		}
	}
	return memregion_Other;
}

void UpdateMemoryStatus9(u32 start, u32 end)
{
	start >>= 12;
	end >>= 12;

	if (end == 0xFFFFF)
		end++;

	for (u32 i = start; i < end; i++)
	{
		u32 addr = i << 12;

		int region = ClassifyAddress9(addr);
		u32 pseudoPhyisical = TranslateAddr9(addr);

		for (u32 j = 0; j < 8; j++)
		{
			u8 val = region;
			if (CodeRanges[(pseudoPhyisical + (j << 12)) / 512].Blocks.Length)
				val |= 0x80;
			MemoryStatus9[i * 8 + j] = val;
		}
	}
}

void UpdateMemoryStatus7(u32 start, u32 end)
{
	start >>= 12;
	end >>= 12;

	if (end == 0xFFFFF)
		end++;

	for (u32 i = start; i < end; i++)
	{
		u32 addr = i << 12;

		int region = ClassifyAddress7(addr);
		u32 pseudoPhyisical = TranslateAddr7(addr);

		for (u32 j = 0; j < 8; j++)
		{
			u8 val = region;
			if (CodeRanges[(pseudoPhyisical + (j << 12)) / 512].Blocks.Length)
				val |= 0x80;
			MemoryStatus7[i * 8 + j] = val;
		}
	}
}

void UpdateRegionByPseudoPhyiscal(u32 addr, bool invalidate)
{
	for (u32 i = 1; i < exeMem_Count; i++)
	{
		if (addr >= ExeMemRegionOffsets[i] && addr < ExeMemRegionOffsets[i] + ExeMemRegionSizes[i])
		{
			for (u32 num = 0; num < 2; num++)
			{
				u32 physSize = ExeMemRegionSizes[i];
				u32 mapSize = 0;
				u32 mapStart = 0;
				switch (i)
				{
				case exeMem_ITCM:
					if (num == 0)
						mapStart = 0; mapSize = NDS::ARM9->ITCMSize;
					break;
				case exeMem_MainRAM: mapStart = 0x2000000; mapSize = 0x1000000; break;
				case exeMem_SWRAM:
					if (num == 0)
					{
						if (NDS::SWRAM_ARM9)
							mapStart = 0x3000000, mapSize = 0x1000000;
						else
							mapStart = mapSize = 0;
					}
					else
					{
						if (NDS::SWRAM_ARM7)
							mapStart = 0x3000000, mapSize = 0x800000;
						else
							mapStart = mapSize = 0;
					}
					break;
				case exeMem_LCDC:
					if (num == 0)
						mapStart = 0x6800000, mapSize = 0xA4000;
					break;
				case exeMem_ARM9_BIOS:
					if (num == 0)
						mapStart = 0xFFFF0000, mapSize = 0x10000;
					break;
				case exeMem_ARM7_BIOS:
					if (num == 1)
						mapStart = 0; mapSize = 0x4000;
					break;
				case exeMem_ARM7_WRAM:
					if (num == 1)
					{
						if (NDS::SWRAM_ARM7)
							mapStart = 0x3800000, mapSize = 0x800000;
						else
							mapStart = 0x3000000, mapSize = 0x1000000;
					}
					break;
				case exeMem_ARM7_WVRAM:
					if (num == 1)
						mapStart = 0x6000000, mapSize = 0x1000000;
					break;
				}

				for (u32 j = 0; j < mapSize / physSize; j++)
				{
					u32 virtAddr = mapStart + physSize * j + (addr - ExeMemRegionOffsets[i]);
					if (num == 0
						&& virtAddr >= NDS::ARM9->DTCMBase && virtAddr < (NDS::ARM9->DTCMBase + NDS::ARM9->DTCMSize))
						continue;
					if (invalidate)
					{
						if (num == 0)
							MemoryStatus9[virtAddr / 512] |= 0x80;
						else
							MemoryStatus7[virtAddr / 512] |= 0x80;
					}
					else
					{
						if (num == 0)
							MemoryStatus9[virtAddr / 512] &= ~0x80;
						else
							MemoryStatus7[virtAddr / 512] &= ~0x80;
					}
				}
				
			}
			return;
		}
	}

	assert(false);
}

template <typename T>
T SlowRead9(ARMv5* cpu, u32 addr)
{
	u32 offset = addr & 0x3;
	addr &= ~(sizeof(T) - 1);

	T val;
	if (addr < cpu->ITCMSize)
		val = *(T*)&cpu->ITCM[addr & 0x7FFF];
	else if (addr >= cpu->DTCMBase && addr < (cpu->DTCMBase + cpu->DTCMSize))
		val = *(T*)&cpu->DTCM[(addr - cpu->DTCMBase) & 0x3FFF];
	else if (std::is_same<T, u32>::value)
		val = NDS::ARM9Read32(addr);
	else if (std::is_same<T, u16>::value)
		val = NDS::ARM9Read16(addr);
	else
		val = NDS::ARM9Read8(addr);

	if (std::is_same<T, u32>::value)
		return ROR(val, offset << 3);
	else
		return val;
}

template <typename T>
void SlowWrite9(ARMv5* cpu, u32 addr, T val)
{
	addr &= ~(sizeof(T) - 1);

    if (addr < cpu->ITCMSize)
	{
		InvalidateITCMIfNecessary(addr);
		*(T*)&cpu->ITCM[addr & 0x7FFF] = val;
	}
	else if (addr >= cpu->DTCMBase && addr < (cpu->DTCMBase + cpu->DTCMSize))
	{
		*(T*)&cpu->DTCM[(addr - cpu->DTCMBase) & 0x3FFF] = val;
	}
	else if (std::is_same<T, u32>::value)
	{
		NDS::ARM9Write32(addr, val);
	}
	else if (std::is_same<T, u16>::value)
	{
		NDS::ARM9Write16(addr, val);
	}
	else
	{
		NDS::ARM9Write8(addr, val);
	}
}

template void SlowWrite9<u32>(ARMv5*, u32, u32);
template void SlowWrite9<u16>(ARMv5*, u32, u16);
template void SlowWrite9<u8>(ARMv5*, u32, u8);

template u32 SlowRead9<u32>(ARMv5*, u32);
template u16 SlowRead9<u16>(ARMv5*, u32);
template u8 SlowRead9<u8>(ARMv5*, u32);

template <typename T>
T SlowRead7(u32 addr)
{
	u32 offset = addr & 0x3;
	addr &= ~(sizeof(T) - 1);

	T val;
	if (std::is_same<T, u32>::value)
		val = NDS::ARM7Read32(addr);
	else if (std::is_same<T, u16>::value)
		val = NDS::ARM7Read16(addr);
	else
		val = NDS::ARM7Read8(addr);

	if (std::is_same<T, u32>::value)
		return ROR(val, offset << 3);
	else
		return val;
}

template <typename T>
void SlowWrite7(u32 addr, T val)
{
	addr &= ~(sizeof(T) - 1);

	if (std::is_same<T, u32>::value)
		NDS::ARM7Write32(addr, val);
	else if (std::is_same<T, u16>::value)
		NDS::ARM7Write16(addr, val);
	else
		NDS::ARM7Write8(addr, val);
}

template <bool PreInc, bool Write>
void SlowBlockTransfer9(u32 addr, u64* data, u32 num, ARMv5* cpu)
{
	addr &= ~0x3;
	for (int i = 0; i < num; i++)
	{
		addr += PreInc * 4;
		if (Write)
			SlowWrite9<u32>(cpu, addr, data[i]);
		else
			data[i] = SlowRead9<u32>(cpu, addr);
		addr += !PreInc * 4;
	}
}

template <bool PreInc, bool Write>
void SlowBlockTransfer7(u32 addr, u64* data, u32 num)
{
	addr &= ~0x3;
	for (int i = 0; i < num; i++)
	{
		addr += PreInc * 4;
		if (Write)
			SlowWrite7<u32>(addr, data[i]);
		else
			data[i] = SlowRead7<u32>(addr);
		addr += !PreInc * 4;
	}
}

template void SlowWrite7<u32>(u32, u32);
template void SlowWrite7<u16>(u32, u16);
template void SlowWrite7<u8>(u32, u8);

template u32 SlowRead7<u32>(u32);
template u16 SlowRead7<u16>(u32);
template u8 SlowRead7<u8>(u32);

template void SlowBlockTransfer9<false, false>(u32, u64*, u32, ARMv5*);
template void SlowBlockTransfer9<false, true>(u32, u64*, u32, ARMv5*);
template void SlowBlockTransfer9<true, false>(u32, u64*, u32, ARMv5*);
template void SlowBlockTransfer9<true, true>(u32, u64*, u32, ARMv5*);
template void SlowBlockTransfer7<false, false>(u32 addr, u64* data, u32 num);
template void SlowBlockTransfer7<false, true>(u32 addr, u64* data, u32 num);
template void SlowBlockTransfer7<true, false>(u32 addr, u64* data, u32 num);
template void SlowBlockTransfer7<true, true>(u32 addr, u64* data, u32 num);

template <typename K, typename V, int Size, V InvalidValue>
struct UnreliableHashTable
{
	struct Bucket
	{
		K KeyA, KeyB;
		V ValA, ValB;
	};

	Bucket Table[Size];

	void Reset()
	{
		for (int i = 0; i < Size; i++)
		{
			Table[i].ValA = Table[i].ValB = InvalidValue;
		}
	}

	UnreliableHashTable()
	{
		Reset();
	}

	V Insert(K key, V value)
	{
		u32 slot = XXH3_64bits(&key, sizeof(K)) & (Size - 1);
		Bucket* bucket = &Table[slot];

		if (bucket->ValA == value || bucket->ValB == value)
		{
			return InvalidValue;
		}
		else if (bucket->ValA == InvalidValue)
		{
			bucket->KeyA = key;
			bucket->ValA = value;
		}
		else if (bucket->ValB == InvalidValue)
		{
			bucket->KeyB = key;
			bucket->ValB = value;
		}
		else
		{
			V prevVal = bucket->ValB;
			bucket->KeyB = bucket->KeyA;
			bucket->ValB = bucket->ValA;
			bucket->KeyA = key;
			bucket->ValA = value;
			return prevVal;
		}

		return InvalidValue;
	}

	void Remove(K key)
	{
		u32 slot = XXH3_64bits(&key, sizeof(K)) & (Size - 1);
		Bucket* bucket = &Table[slot];

		if (bucket->KeyA == key && bucket->ValA != InvalidValue)
		{
			bucket->ValA = InvalidValue;
			if (bucket->ValB != InvalidValue)
			{
				bucket->KeyA = bucket->KeyB;
				bucket->ValA = bucket->ValB;
				bucket->ValB = InvalidValue;
			}
		}
		if (bucket->KeyB == key && bucket->ValB != InvalidValue)
			bucket->ValB = InvalidValue;
	}

	V LookUp(K addr)
	{
		u32 slot = XXH3_64bits(&addr, 4) & (Size - 1);
		Bucket* bucket = &Table[slot];

		if (bucket->ValA != InvalidValue && bucket->KeyA == addr)
			return bucket->ValA;
		if (bucket->ValB != InvalidValue && bucket->KeyB == addr)
			return bucket->ValB;

		return InvalidValue;
	}
};

UnreliableHashTable<u32, JitBlock*, 0x800, nullptr> RestoreCandidates;
UnreliableHashTable<u32, u32, 0x800, UINT32_MAX> FastBlockLookUp9;
UnreliableHashTable<u32, u32, 0x800, UINT32_MAX> FastBlockLookUp7;

void Init()
{
	JITCompiler = new Compiler();
}

void DeInit()
{
	delete JITCompiler;
}

void Reset()
{
	ResetBlockCache();

	UpdateMemoryStatus9(0, 0xFFFFFFFF);
	UpdateMemoryStatus7(0, 0xFFFFFFFF);
}

void FloodFillSetFlags(FetchedInstr instrs[], int start, u8 flags)
{
	for (int j = start; j >= 0; j--)
	{
		u8 match = instrs[j].Info.WriteFlags & flags;
		u8 matchMaybe = (instrs[j].Info.WriteFlags >> 4) & flags;
		if (matchMaybe) // writes flags maybe
			instrs[j].SetFlags |= matchMaybe;
		if (match)
		{
			instrs[j].SetFlags |= match;
			flags &= ~match;
			if (!flags)
				return;
		}
	}
}

bool DecodeLiteral(bool thumb, const FetchedInstr& instr, u32& addr)
{
	if (!thumb)
	{
		switch (instr.Info.Kind)
		{
		case ARMInstrInfo::ak_LDR_IMM:
		case ARMInstrInfo::ak_LDRB_IMM:
			addr = (instr.Addr + 8) + ((instr.Instr & 0xFFF) * (instr.Instr & (1 << 23) ? 1 : -1));
			return true;
		case ARMInstrInfo::ak_LDRH_IMM:
			addr = (instr.Addr + 8) + (((instr.Instr & 0xF00) >> 4 | (instr.Instr & 0xF)) * (instr.Instr & (1 << 23) ? 1 : -1));
			return true;
		default:
			break;
		}
	}
	else if (instr.Info.Kind == ARMInstrInfo::tk_LDR_PCREL)
	{
    	addr = ((instr.Addr + 4) & ~0x2) + ((instr.Instr & 0xFF) << 2);
		return true;
	}

	JIT_DEBUGPRINT("Literal %08x %x not recognised %d\n", instr.Instr, instr.Addr, instr.Info.Kind);
	return false;
}

bool DecodeBranch(bool thumb, const FetchedInstr& instr, u32& cond, bool hasLink, u32 lr, bool& link, 
	u32& linkAddr, u32& targetAddr)
{
	if (thumb)
	{
		u32 r15 = instr.Addr + 4;
		cond = 0xE;

		link = instr.Info.Kind == ARMInstrInfo::tk_BL_LONG;
		linkAddr = instr.Addr + 4;

		if (instr.Info.Kind == ARMInstrInfo::tk_BL_LONG && !(instr.Instr & (1 << 12)))
		{
			targetAddr = r15 + ((s32)((instr.Instr & 0x7FF) << 21) >> 9);
    		targetAddr += ((instr.Instr >> 16) & 0x7FF) << 1;
			return true;
		}
		else if (instr.Info.Kind == ARMInstrInfo::tk_B)
		{
			s32 offset = (s32)((instr.Instr & 0x7FF) << 21) >> 20;
			targetAddr = r15 + offset;
			return true;
		}
		else if (instr.Info.Kind == ARMInstrInfo::tk_BCOND)
		{
			cond = (instr.Instr >> 8) & 0xF;
			s32 offset = (s32)(instr.Instr << 24) >> 23;
			targetAddr = r15 + offset;
			return true;
		}
		else if (hasLink && instr.Info.Kind == ARMInstrInfo::tk_BX && instr.A_Reg(3) == 14)
		{
			JIT_DEBUGPRINT("returning!\n");
			targetAddr = lr;
			return true;
		}
	}
	else
	{
		link = instr.Info.Kind == ARMInstrInfo::ak_BL;
		linkAddr = instr.Addr + 4;

		cond = instr.Cond();
		if (instr.Info.Kind == ARMInstrInfo::ak_BL 
			|| instr.Info.Kind == ARMInstrInfo::ak_B)
		{
			s32 offset = (s32)(instr.Instr << 8) >> 6;
			u32 r15 = instr.Addr + 8;
			targetAddr = r15 + offset;
			return true;
		}
		else if (hasLink && instr.Info.Kind == ARMInstrInfo::ak_BX && instr.A_Reg(0) == 14)
		{
			JIT_DEBUGPRINT("returning!\n");
			targetAddr = lr;
			return true;
		}
	}
	return false;
}

bool IsIdleLoop(FetchedInstr* instrs, int instrsCount)
{
	// see https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/PowerPC/PPCAnalyst.cpp#L678
	// it basically checks if one iteration of a loop depends on another
	// the rules are quite simple

	u16 regsWrittenTo = 0;
	u16 regsDisallowedToWrite = 0;
	for (int i = 0; i < instrsCount; i++)
	{
		//printf("instr %d %x regs(%x %x) %x %x\n", i, instrs[i].Instr, instrs[i].Info.DstRegs, instrs[i].Info.SrcRegs, regsWrittenTo, regsDisallowedToWrite);
		if (instrs[i].Info.SpecialKind == ARMInstrInfo::special_WriteMem)
			return false;
		if (i < instrsCount - 1 && instrs[i].Info.Branches())
			return false;

		u16 srcRegs = instrs[i].Info.SrcRegs & ~(1 << 15);
		u16 dstRegs = instrs[i].Info.DstRegs & ~(1 << 15);

		regsDisallowedToWrite |= srcRegs & ~regsWrittenTo;
		
		if (dstRegs & regsDisallowedToWrite)
			return false;
		regsWrittenTo |= dstRegs;
	}
	return true;
}

typedef void (*InterpreterFunc)(ARM* cpu);

void NOP(ARM* cpu) {}

#define F(x) &ARMInterpreter::A_##x
#define F_ALU(name, s) \
	F(name##_REG_LSL_IMM##s), F(name##_REG_LSR_IMM##s), F(name##_REG_ASR_IMM##s), F(name##_REG_ROR_IMM##s), \
	F(name##_REG_LSL_REG##s), F(name##_REG_LSR_REG##s), F(name##_REG_ASR_REG##s), F(name##_REG_ROR_REG##s), F(name##_IMM##s)
#define F_MEM_WB(name) \
	F(name##_REG_LSL), F(name##_REG_LSR), F(name##_REG_ASR), F(name##_REG_ROR), F(name##_IMM), \
	F(name##_POST_REG_LSL), F(name##_POST_REG_LSR), F(name##_POST_REG_ASR), F(name##_POST_REG_ROR), F(name##_POST_IMM)
#define F_MEM_HD(name) \
	F(name##_REG), F(name##_IMM), F(name##_POST_REG), F(name##_POST_IMM)
InterpreterFunc InterpretARM[ARMInstrInfo::ak_Count] =
{
	F_ALU(AND,), F_ALU(AND,_S),
	F_ALU(EOR,), F_ALU(EOR,_S),
	F_ALU(SUB,), F_ALU(SUB,_S),
	F_ALU(RSB,), F_ALU(RSB,_S),
	F_ALU(ADD,), F_ALU(ADD,_S),
	F_ALU(ADC,), F_ALU(ADC,_S),
	F_ALU(SBC,), F_ALU(SBC,_S),
	F_ALU(RSC,), F_ALU(RSC,_S),
	F_ALU(ORR,), F_ALU(ORR,_S),
	F_ALU(MOV,), F_ALU(MOV,_S),
	F_ALU(BIC,), F_ALU(BIC,_S),
	F_ALU(MVN,), F_ALU(MVN,_S),
	F_ALU(TST,),
	F_ALU(TEQ,),
	F_ALU(CMP,),
	F_ALU(CMN,),

	F(MUL), F(MLA), F(UMULL), F(UMLAL), F(SMULL), F(SMLAL), F(SMLAxy), F(SMLAWy), F(SMULWy), F(SMLALxy), F(SMULxy),
	F(CLZ), F(QADD), F(QDADD), F(QSUB), F(QDSUB),

	F_MEM_WB(STR),
	F_MEM_WB(STRB),
	F_MEM_WB(LDR),
	F_MEM_WB(LDRB),

	F_MEM_HD(STRH),
	F_MEM_HD(LDRD),
	F_MEM_HD(STRD),
	F_MEM_HD(LDRH),
	F_MEM_HD(LDRSB),
	F_MEM_HD(LDRSH),

	F(SWP), F(SWPB),
	F(LDM), F(STM),

	F(B), F(BL), F(BLX_IMM), F(BX), F(BLX_REG),
	F(UNK), F(MSR_IMM), F(MSR_REG), F(MRS), F(MCR), F(MRC), F(SVC),
	NOP
};
#undef F_ALU
#undef F_MEM_WB
#undef F_MEM_HD
#undef F

void T_BL_LONG(ARM* cpu)
{
	ARMInterpreter::T_BL_LONG_1(cpu);
	cpu->R[15] += 2;
	ARMInterpreter::T_BL_LONG_2(cpu);
}

#define F(x) ARMInterpreter::T_##x
InterpreterFunc InterpretTHUMB[ARMInstrInfo::tk_Count] =
{
	F(LSL_IMM), F(LSR_IMM), F(ASR_IMM),
	F(ADD_REG_), F(SUB_REG_), F(ADD_IMM_), F(SUB_IMM_),
	F(MOV_IMM), F(CMP_IMM), F(ADD_IMM), F(SUB_IMM),
	F(AND_REG), F(EOR_REG), F(LSL_REG), F(LSR_REG), F(ASR_REG),
	F(ADC_REG), F(SBC_REG), F(ROR_REG), F(TST_REG), F(NEG_REG),
	F(CMP_REG), F(CMN_REG), F(ORR_REG), F(MUL_REG), F(BIC_REG), F(MVN_REG),
	F(ADD_HIREG), F(CMP_HIREG), F(MOV_HIREG),
	F(ADD_PCREL), F(ADD_SPREL), F(ADD_SP),
	F(LDR_PCREL), F(STR_REG), F(STRB_REG), F(LDR_REG), F(LDRB_REG), F(STRH_REG),
	F(LDRSB_REG), F(LDRH_REG), F(LDRSH_REG), F(STR_IMM), F(LDR_IMM), F(STRB_IMM),
	F(LDRB_IMM), F(STRH_IMM), F(LDRH_IMM), F(STR_SPREL), F(LDR_SPREL),
	F(PUSH), F(POP), F(LDMIA), F(STMIA),
	F(BCOND), F(BX), F(BLX_REG), F(B), F(BL_LONG_1), F(BL_LONG_2),
	F(UNK), F(SVC), 
	T_BL_LONG // BL_LONG psudo opcode
};
#undef F


extern u32 literalsPerBlock;
void CompileBlock(ARM* cpu)
{
    bool thumb = cpu->CPSR & 0x20;

	if (Config::JIT_MaxBlockSize < 1)
		Config::JIT_MaxBlockSize = 1;
	if (Config::JIT_MaxBlockSize > 32)
		Config::JIT_MaxBlockSize = 32;

	u32 blockAddr = cpu->R[15] - (thumb ? 2 : 4);
	u32 pseudoPhysicalAddr = cpu->Num == 0
			? TranslateAddr9(blockAddr)
			: TranslateAddr7(blockAddr);
    if (pseudoPhysicalAddr < ExeMemRegionSizes[exeMem_Unmapped])
    {
        printf("Trying to compile a block in unmapped memory: %x\n", blockAddr);
    }
	
    FetchedInstr instrs[Config::JIT_MaxBlockSize];
    int i = 0;
    u32 r15 = cpu->R[15];

	u32 addressRanges[Config::JIT_MaxBlockSize];
	u32 addressMasks[Config::JIT_MaxBlockSize] = {0};
	u32 numAddressRanges = 0;

	u32 numLiterals = 0;
	u32 literalLoadAddrs[Config::JIT_MaxBlockSize];
	// they are going to be hashed
	u32 literalValues[Config::JIT_MaxBlockSize];
	u32 instrValues[Config::JIT_MaxBlockSize];

	cpu->FillPipeline();
    u32 nextInstr[2] = {cpu->NextInstr[0], cpu->NextInstr[1]};
	u32 nextInstrAddr[2] = {blockAddr, r15};

	JIT_DEBUGPRINT("start block %x %08x (%x)\n", blockAddr, cpu->CPSR, pseudoPhysicalAddr);

	u32 lastSegmentStart = blockAddr;
	u32 lr;
	bool hasLink = false;

    do
    {
        r15 += thumb ? 2 : 4;

		instrs[i].BranchFlags = 0;
		instrs[i].SetFlags = 0;
        instrs[i].Instr = nextInstr[0];
        nextInstr[0] = nextInstr[1];
	
		instrs[i].Addr = nextInstrAddr[0];
		nextInstrAddr[0] = nextInstrAddr[1];
		nextInstrAddr[1] = r15;
		JIT_DEBUGPRINT("instr %08x %x\n", instrs[i].Instr & (thumb ? 0xFFFF : ~0), instrs[i].Addr);

		instrValues[i] = instrs[i].Instr;

		u32 translatedAddr = cpu->Num == 0
			? TranslateAddr9(instrs[i].Addr)
			: TranslateAddr7(instrs[i].Addr);
		u32 translatedAddrRounded = translatedAddr & ~0x1FF;
		if (i == 0 || translatedAddrRounded != addressRanges[numAddressRanges - 1])
		{
			bool returning = false;
			for (int j = 0; j < numAddressRanges; j++)
			{
				if (addressRanges[j] == translatedAddrRounded)
				{
					std::swap(addressRanges[j], addressRanges[numAddressRanges - 1]);
					std::swap(addressMasks[j], addressMasks[numAddressRanges - 1]);
					returning = true;
					break;
				}
			}
			if (!returning)
				addressRanges[numAddressRanges++] = translatedAddrRounded;
		}
		addressMasks[numAddressRanges - 1] |= 1 << ((translatedAddr & 0x1FF) / 16);

        if (cpu->Num == 0)
        {
            ARMv5* cpuv5 = (ARMv5*)cpu;
            if (thumb && r15 & 0x2)
            {
                nextInstr[1] >>= 16;
                instrs[i].CodeCycles = 0;
            }
            else
            {
                nextInstr[1] = cpuv5->CodeRead32(r15, false);
                instrs[i].CodeCycles = cpu->CodeCycles;
            }
        }
        else
        {
            ARMv4* cpuv4 = (ARMv4*)cpu;
            if (thumb)
                nextInstr[1] = cpuv4->CodeRead16(r15);
            else
                nextInstr[1] = cpuv4->CodeRead32(r15);
            instrs[i].CodeCycles = cpu->CodeCycles;
        }
        instrs[i].Info = ARMInstrInfo::Decode(thumb, cpu->Num, instrs[i].Instr);

		cpu->R[15] = r15;
		cpu->CurInstr = instrs[i].Instr;
		cpu->CodeCycles = instrs[i].CodeCycles;

		if (instrs[i].Info.DstRegs & (1 << 14))
			hasLink = false;

		if (thumb)
		{
			InterpretTHUMB[instrs[i].Info.Kind](cpu);
		}
		else
		{
			if (cpu->Num == 0 && instrs[i].Info.Kind == ARMInstrInfo::ak_BLX_IMM)
			{
				ARMInterpreter::A_BLX_IMM(cpu);
			}
			else
			{
                u32 icode = ((instrs[i].Instr >> 4) & 0xF) | ((instrs[i].Instr >> 16) & 0xFF0);
				assert(InterpretARM[instrs[i].Info.Kind] == ARMInterpreter::ARMInstrTable[icode]
					|| instrs[i].Info.Kind == ARMInstrInfo::ak_MOV_REG_LSL_IMM
					|| instrs[i].Info.Kind == ARMInstrInfo::ak_Nop
					|| instrs[i].Info.Kind == ARMInstrInfo::ak_UNK);
				if (cpu->CheckCondition(instrs[i].Cond()))
					InterpretARM[instrs[i].Info.Kind](cpu);
				else
					cpu->AddCycles_C();
			}
		}

		instrs[i].DataCycles = cpu->DataCycles;
		instrs[i].DataRegion = cpu->DataRegion;

		u32 literalAddr;
		if (Config::JIT_LiteralOptimisations
			&& instrs[i].Info.SpecialKind == ARMInstrInfo::special_LoadLiteral
			&& DecodeLiteral(thumb, instrs[i], literalAddr))
		{
			u32 translatedAddr = cpu->Num == 0
				? TranslateAddr9(literalAddr)
				: TranslateAddr7(literalAddr);
			u32 translatedAddrRounded = translatedAddr & ~0x1FF;

			u32 j = 0;
			for (; j < numAddressRanges; j++)
				if (addressRanges[j] == translatedAddrRounded)
					break;
			if (j == numAddressRanges)
				addressRanges[numAddressRanges++] = translatedAddrRounded;
			addressMasks[j] |= 1 << ((translatedAddr & 0x1FF) / 16);
			JIT_DEBUGPRINT("literal loading %08x %08x %08x %08x\n", literalAddr, translatedAddr, addressMasks[j], addressRanges[j]);
			cpu->DataRead32(literalAddr, &literalValues[numLiterals]);
			literalLoadAddrs[numLiterals++] = translatedAddr;
		}

		if (thumb && instrs[i].Info.Kind == ARMInstrInfo::tk_BL_LONG_2 && i > 0
			&& instrs[i - 1].Info.Kind == ARMInstrInfo::tk_BL_LONG_1)
		{
			instrs[i - 1].Info.Kind = ARMInstrInfo::tk_BL_LONG;
			instrs[i - 1].Instr = (instrs[i - 1].Instr & 0xFFFF) | (instrs[i].Instr << 16);
			instrs[i - 1].Info.DstRegs = 0xC000;
			instrs[i - 1].Info.SrcRegs = 0;
			instrs[i - 1].Info.EndBlock = true;
			i--;
		}

		if (instrs[i].Info.Branches() && Config::JIT_BrancheOptimisations)
		{
			bool hasBranched = cpu->R[15] != r15;

			bool link;
			u32 cond, target, linkAddr;
			bool staticBranch = DecodeBranch(thumb, instrs[i], cond, hasLink, lr, link, linkAddr, target);
			JIT_DEBUGPRINT("branch cond %x target %x (%d)\n", cond, target, hasBranched);

			if (staticBranch)
			{
				instrs[i].BranchFlags |= branch_StaticTarget;

				bool isBackJump = false;
				if (hasBranched)
				{
					for (int j = 0; j < i; j++)
					{
						if (instrs[i].Addr == target)
						{
							isBackJump = true;
							break;
						}
					}
				}

				if (cond < 0xE && target < instrs[i].Addr && target >= lastSegmentStart)
				{
					// we might have an idle loop
					u32 backwardsOffset = (instrs[i].Addr - target) / (thumb ? 2 : 4);
					if (IsIdleLoop(&instrs[i - backwardsOffset], backwardsOffset + 1))
					{
						instrs[i].BranchFlags |= branch_IdleBranch;
						JIT_DEBUGPRINT("found %s idle loop %d in block %x\n", thumb ? "thumb" : "arm", cpu->Num, blockAddr);
					}
				}
				else if (hasBranched && !isBackJump && i + 1 < Config::JIT_MaxBlockSize)
				{
					u32 targetPseudoPhysical = cpu->Num == 0
						? TranslateAddr9(target)
						: TranslateAddr7(target);

					if (link)
					{
						lr = linkAddr;
						hasLink = true;
					}
					
					r15 = target + (thumb ? 2 : 4);
					assert(r15 == cpu->R[15]);

					JIT_DEBUGPRINT("block lengthened by static branch (target %x)\n", target);

					nextInstr[0] = cpu->NextInstr[0];
					nextInstr[1] = cpu->NextInstr[1];

					nextInstrAddr[0] = target;
					nextInstrAddr[1] = r15;

					lastSegmentStart = target;

					instrs[i].Info.EndBlock = false;

					if (cond < 0xE)
						instrs[i].BranchFlags |= branch_FollowCondTaken;
				}
			}

			if (!hasBranched && cond < 0xE && i + 1 < Config::JIT_MaxBlockSize)
			{
				instrs[i].Info.EndBlock = false;
				instrs[i].BranchFlags |= branch_FollowCondNotTaken;
			}
		}

        i++;

		bool canCompile = JITCompiler->CanCompile(thumb, instrs[i - 1].Info.Kind);
		bool secondaryFlagReadCond = !canCompile || (instrs[i - 1].BranchFlags & (branch_FollowCondTaken | branch_FollowCondNotTaken));
		if (instrs[i - 1].Info.ReadFlags != 0 || secondaryFlagReadCond)
			FloodFillSetFlags(instrs, i - 2, !secondaryFlagReadCond ? instrs[i - 1].Info.ReadFlags : 0xF);
    } while(!instrs[i - 1].Info.EndBlock && i < Config::JIT_MaxBlockSize && !cpu->Halted && (!cpu->IRQ || (cpu->CPSR & 0x80)));

	u32 literalHash = (u32)XXH3_64bits(literalValues, numLiterals * 4);
	u32 instrHash = (u32)XXH3_64bits(instrValues, i * 4);

	JitBlock* prevBlock = RestoreCandidates.LookUp(pseudoPhysicalAddr);
	bool mayRestore = true;
	if (prevBlock)
	{
		RestoreCandidates.Remove(pseudoPhysicalAddr);

		mayRestore = prevBlock->LiteralHash == literalHash && prevBlock->InstrHash == instrHash;

		if (mayRestore && prevBlock->NumAddresses == numAddressRanges)
		{
			for (int j = 0; j < numAddressRanges; j++)
			{
				if (prevBlock->AddressRanges()[j] != addressRanges[j]
					|| prevBlock->AddressMasks()[j] != addressMasks[j])
				{
					mayRestore = false;
					break;
				}
			}
		}
		else
			mayRestore = false;
	}
	else
	{
		mayRestore = false;
		prevBlock = NULL;
	}

	JitBlock* block;
	if (!mayRestore)
	{
		if (prevBlock)
			delete prevBlock;

		block = new JitBlock(cpu->Num, i, numAddressRanges, numLiterals);
		block->LiteralHash = literalHash;
		block->InstrHash = instrHash;
		for (int j = 0; j < numAddressRanges; j++)
			block->AddressRanges()[j] = addressRanges[j];
		for (int j = 0; j < numAddressRanges; j++)
			block->AddressMasks()[j] = addressMasks[j];
		for (int j = 0; j < numLiterals; j++)
			block->Literals()[j] = literalLoadAddrs[j];

		block->PseudoPhysicalAddr = pseudoPhysicalAddr;

		FloodFillSetFlags(instrs, i - 1, 0xF);

		block->EntryPoint = JITCompiler->CompileBlock(pseudoPhysicalAddr, cpu, thumb, instrs, i);
	}
	else
	{
		JIT_DEBUGPRINT("restored! %p\n", prevBlock);
		block = prevBlock;
	}

	for (int j = 0; j < numAddressRanges; j++)
	{
		assert(addressRanges[j] == block->AddressRanges()[j]);
		assert(addressMasks[j] == block->AddressMasks()[j]);
		assert(addressMasks[j] != 0);
		CodeRanges[addressRanges[j] / 512].Code |= addressMasks[j];
		CodeRanges[addressRanges[j] / 512].Blocks.Add(block);

		UpdateRegionByPseudoPhyiscal(addressRanges[j], true);
	}

	if (cpu->Num == 0)
	{
		JitBlocks9[pseudoPhysicalAddr] = block;
		FastBlockLookUp9.Insert(pseudoPhysicalAddr, JITCompiler->SubEntryOffset(block->EntryPoint));
	}
	else
	{
		JitBlocks7[pseudoPhysicalAddr] = block;
		FastBlockLookUp7.Insert(pseudoPhysicalAddr, JITCompiler->SubEntryOffset(block->EntryPoint));
	}
}

void InvalidateByAddr(u32 pseudoPhysical)
{
	JIT_DEBUGPRINT("invalidating by addr %x\n", pseudoPhysical);
	AddressRange* range = &CodeRanges[pseudoPhysical / 512];
	u32 mask = 1 << ((pseudoPhysical & 0x1FF) / 16);

	range->Code = 0;
	for (int i = 0; i < range->Blocks.Length;)
	{
		JitBlock* block = range->Blocks[i];

		bool invalidated = false;
		u32 mask = 0;
		for (int j = 0; j < block->NumAddresses; j++)
		{
			if (block->AddressRanges()[j] == (pseudoPhysical & ~0x1FF))
			{
				mask = block->AddressMasks()[j];
				invalidated = block->AddressMasks()[j] & mask;
				break;
			}
		}
		assert(mask);
		if (!invalidated)
		{
			range->Code |= mask;
			i++;
			continue;
		}
		range->Blocks.Remove(i);

		bool literalInvalidation = false;
		for (int j = 0; j < block->NumLiterals; j++)
		{
			u32 addr = block->Literals()[j];
			if (addr == pseudoPhysical)
			{
				if (InvalidLiterals.Find(pseudoPhysical) != -1)
				{
					InvalidLiterals.Add(pseudoPhysical);
					JIT_DEBUGPRINT("found invalid literal %d\n", InvalidLiterals.Length);
				}
				literalInvalidation = true;
				break;
			}
		}
		for (int j = 0; j < block->NumAddresses; j++)
		{
			u32 addr = block->AddressRanges()[j];
			if ((addr / 512) != (pseudoPhysical / 512))
			{
				AddressRange* otherRange = &CodeRanges[addr / 512];
				assert(otherRange != range);
				bool removed = otherRange->Blocks.RemoveByValue(block);
				assert(removed);

				if (otherRange->Blocks.Length == 0)
				{
					otherRange->Code = 0;
					UpdateRegionByPseudoPhyiscal(addr, false);
				}
			}
		}

		for (int j = 0; j < block->NumLinks(); j++)
			JITCompiler->UnlinkBlock(block->Links()[j]);
		block->ResetLinks();

		if (block->Num == 0)
		{
			JitBlocks9.erase(block->PseudoPhysicalAddr);
			FastBlockLookUp9.Remove(block->PseudoPhysicalAddr);
		}
		else
		{
			JitBlocks7.erase(block->PseudoPhysicalAddr);
			FastBlockLookUp7.Remove(block->PseudoPhysicalAddr);
		}

		if (!literalInvalidation)
		{
			JitBlock* prevBlock = RestoreCandidates.Insert(block->PseudoPhysicalAddr, block);
			if (prevBlock)
				delete prevBlock;
		}
		else
		{
			delete block;
		}
	}

	if (range->Blocks.Length == 0)
		UpdateRegionByPseudoPhyiscal(pseudoPhysical, false);
}

void InvalidateRegionIfNecessary(u32 pseudoPhyisical)
{
	if (CodeRanges[pseudoPhyisical / 512].Code & (1 << ((pseudoPhyisical & 0x1FF) / 16)))
		InvalidateByAddr(pseudoPhyisical);
}

void ResetBlockCache()
{
	printf("Resetting JIT block cache...\n");

	InvalidLiterals.Clear();
	FastBlockLookUp9.Reset();
	FastBlockLookUp7.Reset();
	RestoreCandidates.Reset();
	for (int i = 0; i < sizeof(RestoreCandidates.Table)/sizeof(RestoreCandidates.Table[0]); i++)
	{
		if (RestoreCandidates.Table[i].ValA)
		{
			delete RestoreCandidates.Table[i].ValA;
			RestoreCandidates.Table[i].ValA = NULL;
		}
		if (RestoreCandidates.Table[i].ValA)
		{
			delete RestoreCandidates.Table[i].ValB;
			RestoreCandidates.Table[i].ValB = NULL;
		}
	}
	for (auto it : JitBlocks9)
	{
		JitBlock* block = it.second;
		for (int j = 0; j < block->NumAddresses; j++)
		{
			u32 addr = block->AddressRanges()[j];
			CodeRanges[addr / 512].Blocks.Clear();
			CodeRanges[addr / 512].Code = 0;
		}
		delete block;
	}
	for (auto it : JitBlocks7)
	{
		JitBlock* block = it.second;
		for (int j = 0; j < block->NumAddresses; j++)
		{
			u32 addr = block->AddressRanges()[j];
			CodeRanges[addr / 512].Blocks.Clear();
			CodeRanges[addr / 512].Code = 0;
		}
	}
	JitBlocks9.clear();
	JitBlocks7.clear();

	JITCompiler->Reset();
}

template <u32 Num>
JitBlockEntry LookUpBlockEntry(u32 addr)
{
	auto& fastMap = Num == 0 ? FastBlockLookUp9 : FastBlockLookUp7;
	u32 entryOffset = fastMap.LookUp(addr);
	if (entryOffset != UINT32_MAX)
		return JITCompiler->AddEntryOffset(entryOffset);

	auto& slowMap = Num == 0 ? JitBlocks9 : JitBlocks7;
	auto block = slowMap.find(addr);
	if (block != slowMap.end())
	{
		fastMap.Insert(addr, JITCompiler->SubEntryOffset(block->second->EntryPoint));
		return block->second->EntryPoint;
	}
	return NULL;
}

template JitBlockEntry LookUpBlockEntry<0>(u32);
template JitBlockEntry LookUpBlockEntry<1>(u32);

template <u32 Num>
void LinkBlock(ARM* cpu, u32 codeOffset)
{
	auto& blockMap = Num == 0 ? JitBlocks9 : JitBlocks7;
	u32 instrAddr = cpu->R[15] - ((cpu->CPSR&0x20)?2:4);
	u32 targetPseudoPhys = Num == 0 ? TranslateAddr9(instrAddr) : TranslateAddr7(instrAddr);
	auto block = blockMap.find(targetPseudoPhys);
	if (block == blockMap.end())
	{
		CompileBlock(cpu);
		block = blockMap.find(targetPseudoPhys);
	}

	JIT_DEBUGPRINT("linking to block %08x\n", targetPseudoPhys);

	block->second->AddLink(codeOffset);
	JITCompiler->LinkBlock(codeOffset, block->second->EntryPoint);
}

template void LinkBlock<0>(ARM*, u32);
template void LinkBlock<1>(ARM*, u32);

void WifiWrite32(u32 addr, u32 val)
{
	Wifi::Write(addr, val & 0xFFFF);
	Wifi::Write(addr + 2, val >> 16);
}

u32 WifiRead32(u32 addr)
{
	return Wifi::Read(addr) | (Wifi::Read(addr + 2) << 16);
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

			switch (size | store)
			{
			case 8: return (void*)NDS::ARM9IORead8;
			case 9: return (void*)NDS::ARM9IOWrite8;
			case 16: return (void*)NDS::ARM9IORead16;
			case 17: return (void*)NDS::ARM9IOWrite16;
			case 32: return (void*)NDS::ARM9IORead32;
			case 33: return (void*)NDS::ARM9IOWrite32;
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

			switch (size | store)
			{
			case 8: return (void*)NDS::ARM7IORead8;
			case 9: return (void*)NDS::ARM7IOWrite8;		
			case 16: return (void*)NDS::ARM7IORead16;
			case 17: return (void*)NDS::ARM7IOWrite16;
			case 32: return (void*)NDS::ARM7IORead32;
			case 33: return (void*)NDS::ARM7IOWrite32;
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
