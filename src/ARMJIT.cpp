#include "ARMJIT.h"

#include <string.h>
#include <assert.h>

#include "Config.h"

#include "ARMJIT_Internal.h"
#include "ARMJIT_x64/ARMJIT_Compiler.h"

#include "ARMInterpreter_ALU.h"
#include "ARMInterpreter_LoadStore.h"
#include "ARMInterpreter_Branch.h"
#include "ARMInterpreter.h"

#include "GPU3D.h"
#include "SPU.h"
#include "Wifi.h"

namespace ARMJIT
{

#define JIT_DEBUGPRINT(msg, ...)

Compiler* compiler;

const u32 ExeMemRegionSizes[] = {
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

const u32 ExeMemRegionOffsets[] = {
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

#define DUP2(x) x, x

const static ExeMemKind JIT_MEM[2][32] = {
	//arm9
	{
		/* 0X*/	DUP2(exeMem_ITCM),
		/* 1X*/	DUP2(exeMem_ITCM), // mirror
		/* 2X*/	DUP2(exeMem_MainRAM),
		/* 3X*/	DUP2(exeMem_SWRAM),
		/* 4X*/	DUP2(exeMem_Unmapped),
		/* 5X*/	DUP2(exeMem_Unmapped),
		/* 6X*/		 exeMem_Unmapped, 
					 exeMem_LCDC,   // Plain ARM9-CPU Access (LCDC mode) (max 656KB)
		/* 7X*/	DUP2(exeMem_Unmapped),
		/* 8X*/	DUP2(exeMem_Unmapped),
		/* 9X*/	DUP2(exeMem_Unmapped),
		/* AX*/	DUP2(exeMem_Unmapped),
		/* BX*/	DUP2(exeMem_Unmapped),
		/* CX*/	DUP2(exeMem_Unmapped),
		/* DX*/	DUP2(exeMem_Unmapped),
		/* EX*/	DUP2(exeMem_Unmapped),
		/* FX*/	DUP2(exeMem_ARM9_BIOS)
	},
	//arm7
	{
		/* 0X*/	DUP2(exeMem_ARM7_BIOS),
		/* 1X*/	DUP2(exeMem_Unmapped),
		/* 2X*/	DUP2(exeMem_MainRAM),
		/* 3X*/	     exeMem_SWRAM,
		             exeMem_ARM7_WRAM,
		/* 4X*/	DUP2(exeMem_Unmapped),
		/* 5X*/	DUP2(exeMem_Unmapped),
		/* 6X*/ DUP2(exeMem_ARM7_WVRAM), /* contrary to Gbatek, melonDS and itself, 
														DeSmuME doesn't mirror the 64 MB region at 0x6800000 */
		/* 7X*/	DUP2(exeMem_Unmapped),
		/* 8X*/	DUP2(exeMem_Unmapped),
		/* 9X*/	DUP2(exeMem_Unmapped),
		/* AX*/	DUP2(exeMem_Unmapped),
		/* BX*/	DUP2(exeMem_Unmapped),
		/* CX*/	DUP2(exeMem_Unmapped),
		/* DX*/	DUP2(exeMem_Unmapped),
		/* EX*/	DUP2(exeMem_Unmapped),
		/* FX*/	DUP2(exeMem_Unmapped)
		}
};

#undef DUP2

/*
	translates address to pseudo physical address
		- more compact, eliminates mirroring, everything comes in a row
		- we only need one translation table
*/
u32 AddrTranslate9[0x2000];
u32 AddrTranslate7[0x4000];

JitBlockEntry FastBlockAccess[ExeMemSpaceSize / 2];
AddressRange CodeRanges[ExeMemSpaceSize / 512];

TinyVector<JitBlock*> JitBlocks;
JitBlock* RestoreCandidates[0x1000] = {NULL};

u32 HashRestoreCandidate(u32 pseudoPhysicalAddr)
{
	return (u32)(((u64)pseudoPhysicalAddr * 11400714819323198485llu) >> 53);
}

void Init()
{
	for (int i = 0; i < 0x2000; i++)
	{
		ExeMemKind kind = JIT_MEM[0][i >> 8];
		u32 size = ExeMemRegionSizes[kind];

		AddrTranslate9[i] = ExeMemRegionOffsets[kind] + ((i << 15) & (size - 1));
	}
	for (int i = 0; i < 0x4000; i++)
	{
		ExeMemKind kind = JIT_MEM[1][i >> 9];
		u32 size = ExeMemRegionSizes[kind];

		AddrTranslate7[i] = ExeMemRegionOffsets[kind] + ((i << 14) & (size - 1));
	}

	compiler = new Compiler();
}

void DeInit()
{
	delete compiler;
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

bool DecodeBranch(bool thumb, const FetchedInstr& instr, u32& cond, u32& targetAddr)
{
	if (thumb)
	{
		u32 r15 = instr.Addr + 4;
		cond = 0xE;

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
	}
	else
	{
		cond = instr.Cond();
		if (instr.Info.Kind == ARMInstrInfo::ak_BL 
			|| instr.Info.Kind == ARMInstrInfo::ak_B)
		{
			s32 offset = (s32)(instr.Instr << 8) >> 6;
			u32 r15 = instr.Addr + 8;
			targetAddr = r15 + offset;
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
	F(UNK), F(MSR_IMM), F(MSR_REG), F(MRS), F(MCR), F(MRC), F(SVC)
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

void CompileBlock(ARM* cpu)
{
    bool thumb = cpu->CPSR & 0x20;

	if (Config::JIT_MaxBlockSize < 1)
		Config::JIT_MaxBlockSize = 1;
	if (Config::JIT_MaxBlockSize > 32)
		Config::JIT_MaxBlockSize = 32;

	u32 blockAddr = cpu->R[15] - (thumb ? 2 : 4);
    if (!(cpu->Num == 0 
        ? IsMapped<0>(blockAddr)
        : IsMapped<1>(blockAddr)))
    {
        printf("Trying to compile a block in unmapped memory: %x\n", blockAddr);
    }
	
	u32 pseudoPhysicalAddr = cpu->Num == 0
			? TranslateAddr<0>(blockAddr)
			: TranslateAddr<1>(blockAddr);

    FetchedInstr instrs[Config::JIT_MaxBlockSize];
    int i = 0;
    u32 r15 = cpu->R[15];

	u32 addresseRanges[32] = {};
	u32 numAddressRanges = 0;

	cpu->FillPipeline();
    u32 nextInstr[2] = {cpu->NextInstr[0], cpu->NextInstr[1]};
	u32 nextInstrAddr[2] = {blockAddr, r15};

	JIT_DEBUGPRINT("start block %x (%x) %p %p (region invalidates %dx)\n", 
		blockAddr, pseudoPhysicalAddr, FastBlockAccess[pseudoPhysicalAddr / 2], 
		cpu->Num == 0 ? LookUpBlock<0>(blockAddr) : LookUpBlock<1>(blockAddr),
		CodeRanges[pseudoPhysicalAddr / 512].TimesInvalidated);

	u32 lastSegmentStart = blockAddr;

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

		u32 translatedAddr = (cpu->Num == 0
			? TranslateAddr<0>(instrs[i].Addr)
			: TranslateAddr<1>(instrs[i].Addr)) & ~0x1FF;
		if (i == 0 || translatedAddr != addresseRanges[numAddressRanges - 1])
		{
			bool returning = false;
			for (int j = 0; j < numAddressRanges; j++)
			{
				if (addresseRanges[j] == translatedAddr)
				{
					returning = true;
					break;
				}
			}
			if (!returning)
				addresseRanges[numAddressRanges++] = translatedAddr;
		}

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
				assert(InterpretARM[instrs[i].Info.Kind] == ARMInterpreter::ARMInstrTable[icode] || instrs[i].Info.Kind == ARMInstrInfo::ak_MOV_REG_LSL_IMM);
				if (cpu->CheckCondition(instrs[i].Cond()))
					InterpretARM[instrs[i].Info.Kind](cpu);
				else
					cpu->AddCycles_C();
			}
		}

		instrs[i].DataCycles = cpu->DataCycles;
		instrs[i].DataRegion = cpu->DataRegion;

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

			u32 cond, target;
			bool staticBranch = DecodeBranch(thumb, instrs[i], cond, target);
			JIT_DEBUGPRINT("branch cond %x target %x (%d)\n", cond, target, hasBranched);

			if (staticBranch)
			{
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
					u32 offset = (target - blockAddr) / (thumb ? 2 : 4);
					if (IsIdleLoop(instrs + offset, i - offset + 1))
					{
						instrs[i].BranchFlags |= branch_IdleBranch;
						JIT_DEBUGPRINT("found %s idle loop %d in block %x\n", thumb ? "thumb" : "arm", cpu->Num, blockAddr);
					}
				}
				else if (hasBranched && (!thumb || cond == 0xE) && !isBackJump && i + 1 < Config::JIT_MaxBlockSize)
				{
					u32 targetPseudoPhysical = cpu->Num == 0
						? TranslateAddr<0>(target)
						: TranslateAddr<1>(target);
					
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

		bool canCompile = compiler->CanCompile(thumb, instrs[i - 1].Info.Kind);
		bool secondaryFlagReadCond = !canCompile || (instrs[i - 1].BranchFlags & (branch_FollowCondTaken | branch_FollowCondNotTaken));
		if (instrs[i - 1].Info.ReadFlags != 0 || secondaryFlagReadCond)
			FloodFillSetFlags(instrs, i - 2, !secondaryFlagReadCond ? instrs[i - 1].Info.ReadFlags : 0xF);
    } while(!instrs[i - 1].Info.EndBlock && i < Config::JIT_MaxBlockSize && !cpu->Halted);

	u32 restoreSlot = HashRestoreCandidate(pseudoPhysicalAddr);
	JitBlock* prevBlock = RestoreCandidates[restoreSlot];
	bool mayRestore = true;
	if (prevBlock && prevBlock->PseudoPhysicalAddr == pseudoPhysicalAddr)
	{
		RestoreCandidates[restoreSlot] = NULL;	
		if (prevBlock->NumInstrs == i)
		{
			for (int j = 0; j < i; j++)
			{
				if (prevBlock->Instrs()[j] != instrs[j].Instr)
				{
					mayRestore = false;
					break;
				}
			}
		}
		else
			mayRestore = false;

		if (prevBlock->NumAddresses == numAddressRanges)
		{
			for (int j = 0; j < numAddressRanges; j++)
			{
				if (prevBlock->AddressRanges()[j] != addresseRanges[j])
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

		block = new JitBlock(i, numAddressRanges);
		for (int j = 0; j < i; j++)
			block->Instrs()[j] = instrs[j].Instr;
		for (int j = 0; j < numAddressRanges; j++)
			block->AddressRanges()[j] = addresseRanges[j];

		block->StartAddr = blockAddr;
		block->PseudoPhysicalAddr = pseudoPhysicalAddr;

		FloodFillSetFlags(instrs, i - 1, 0xF);

		block->EntryPoint = compiler->CompileBlock(cpu, thumb, instrs, i);
	}
	else
	{
		JIT_DEBUGPRINT("restored! %p\n", prevBlock);
		block = prevBlock;
	}

	for (int j = 0; j < numAddressRanges; j++)
	{
		assert(addresseRanges[j] == block->AddressRanges()[j]);
		CodeRanges[addresseRanges[j] / 512].Blocks.Add(block);
	}

	FastBlockAccess[block->PseudoPhysicalAddr / 2] = block->EntryPoint;

	JitBlocks.Add(block);
}

void InvalidateByAddr(u32 pseudoPhysical)
{
	JIT_DEBUGPRINT("invalidating by addr %x\n", pseudoPhysical);
	AddressRange* range = &CodeRanges[pseudoPhysical / 512];
	int startLength = range->Blocks.Length;
	for (int i = 0; i < range->Blocks.Length; i++)
	{
		assert(range->Blocks.Length == startLength);
		JitBlock* block = range->Blocks[i];
		for (int j = 0; j < block->NumAddresses; j++)
		{
			u32 addr = block->AddressRanges()[j];
			if ((addr / 512) != (pseudoPhysical / 512))
			{
				AddressRange* otherRange = &CodeRanges[addr / 512];
				assert(otherRange != range);
				bool removed = otherRange->Blocks.RemoveByValue(block);
				assert(removed);
			}
		}

		bool removed = JitBlocks.RemoveByValue(block);
		assert(removed);

		FastBlockAccess[block->PseudoPhysicalAddr / 2] = NULL;

		u32 slot = HashRestoreCandidate(block->PseudoPhysicalAddr);
		if (RestoreCandidates[slot] && RestoreCandidates[slot] != block)
			delete RestoreCandidates[slot];

		RestoreCandidates[slot] = block;
	}
	if ((range->TimesInvalidated + 1) > range->TimesInvalidated)
		range->TimesInvalidated++;
	
	range->Blocks.Clear();
}

void InvalidateByAddr7(u32 addr)
{
	u32 pseudoPhysical = TranslateAddr<1>(addr);
	if (__builtin_expect(CodeRanges[pseudoPhysical / 512].Blocks.Length > 0, false))
		InvalidateByAddr(pseudoPhysical);
}

void InvalidateITCM(u32 addr)
{
	u32 pseudoPhysical = addr + ExeMemRegionOffsets[exeMem_ITCM];
	if (CodeRanges[pseudoPhysical / 512].Blocks.Length > 0)
		InvalidateByAddr(pseudoPhysical);
}

void InvalidateAll()
{
	JIT_DEBUGPRINT("invalidating all %x\n", JitBlocks.Length);
	for (int i = 0; i < JitBlocks.Length; i++)
	{
		JitBlock* block = JitBlocks[i];

		FastBlockAccess[block->PseudoPhysicalAddr / 2] = NULL;
		
		for (int j = 0; j < block->NumAddresses; j++)
		{
			u32 addr = block->AddressRanges()[j];
			AddressRange* range = &CodeRanges[addr / 512];
			range->Blocks.Clear();
			if (range->TimesInvalidated + 1 > range->TimesInvalidated)
				range->TimesInvalidated++;
		}

		u32 slot = HashRestoreCandidate(block->PseudoPhysicalAddr);
		if (RestoreCandidates[slot] && RestoreCandidates[slot] != block)
			delete RestoreCandidates[slot];
		
		RestoreCandidates[slot] = block;
	}

	JitBlocks.Clear();
}

void ResetBlockCache()
{
	printf("Resetting JIT block cache...\n");
	
	memset(FastBlockAccess, 0, sizeof(FastBlockAccess));
	for (int i = 0; i < sizeof(RestoreCandidates)/sizeof(RestoreCandidates[0]); i++)
	{
		if (RestoreCandidates[i])
		{
			delete RestoreCandidates[i];
			RestoreCandidates[i] = NULL;
		}
	}
	for (int i = 0; i < JitBlocks.Length; i++)
	{
		JitBlock* block = JitBlocks[i];
		for (int j = 0; j < block->NumAddresses; j++)
		{
			u32 addr = block->AddressRanges()[j];
			CodeRanges[addr / 512].Blocks.Clear();
			CodeRanges[addr / 512].TimesInvalidated = 0;
		}
		delete block;
	}
	JitBlocks.Clear();

	compiler->Reset();
}

void* GetFuncForAddr(ARM* cpu, u32 addr, bool store, int size)
{
	if (cpu->Num == 0)
	{
		if ((addr & 0xFF000000) == 0x04000000)
		{
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
			if (addr < 0x04810000 && size == 16)
			{
				if (store)
					return (void*)Wifi::Write;
				else
					return (void*)Wifi::Read;
			}
			break;
		}
	}
	return NULL;
}

}