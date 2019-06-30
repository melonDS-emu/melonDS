#include "ARMJIT.h"

#include <string.h>

#include "ARMJIT_x64/ARMJIT_Compiler.h"

namespace ARMJIT
{

Compiler* compiler;
BlockCache cache;

#define DUP2(x) x, x

static ptrdiff_t JIT_MEM[2][32] = {
	//arm9
	{
		/* 0X*/	DUP2(offsetof(BlockCache, ARM9_ITCM)),
		/* 1X*/	DUP2(offsetof(BlockCache, ARM9_ITCM)), // mirror
		/* 2X*/	DUP2(offsetof(BlockCache, MainRAM)),
		/* 3X*/	DUP2(offsetof(BlockCache, SWRAM)),
		/* 4X*/	DUP2(-1),
		/* 5X*/	DUP2(-1),
		/* 6X*/		 -1, 
					 offsetof(BlockCache, ARM9_LCDC),   // Plain ARM9-CPU Access (LCDC mode) (max 656KB)
		/* 7X*/	DUP2(-1),
		/* 8X*/	DUP2(-1),
		/* 9X*/	DUP2(-1),
		/* AX*/	DUP2(-1),
		/* BX*/	DUP2(-1),
		/* CX*/	DUP2(-1),
		/* DX*/	DUP2(-1),
		/* EX*/	DUP2(-1),
		/* FX*/	DUP2(offsetof(BlockCache, ARM9_BIOS))
	},
	//arm7
	{
		/* 0X*/	DUP2(offsetof(BlockCache, ARM7_BIOS)),
		/* 1X*/	DUP2(-1),
		/* 2X*/	DUP2(offsetof(BlockCache, MainRAM)),
		/* 3X*/	     offsetof(BlockCache, SWRAM),
		             offsetof(BlockCache, ARM7_WRAM),
		/* 4X*/	DUP2(-1),
		/* 5X*/	DUP2(-1),
		/* 6X*/ DUP2(offsetof(BlockCache, ARM7_WVRAM)), /* contrary to Gbatek, melonDS and itself, 
														DeSmuME doesn't mirror the 64 MB region at 0x6800000 */
		/* 7X*/	DUP2(-1),
		/* 8X*/	DUP2(-1),
		/* 9X*/	DUP2(-1),
		/* AX*/	DUP2(-1),
		/* BX*/	DUP2(-1),
		/* CX*/	DUP2(-1),
		/* DX*/	DUP2(-1),
		/* EX*/	DUP2(-1),
		/* FX*/	DUP2(-1)
		}
};

static u32 JIT_MASK[2][32] = {
	//arm9
	{
		/* 0X*/	DUP2(0x00007FFF),
		/* 1X*/	DUP2(0x00007FFF),
		/* 2X*/	DUP2(0x003FFFFF),
		/* 3X*/	DUP2(0x00007FFF),
		/* 4X*/	DUP2(0x00000000),
		/* 5X*/	DUP2(0x00000000),
		/* 6X*/		 0x00000000,
					 0x000FFFFF,
		/* 7X*/	DUP2(0x00000000),
		/* 8X*/	DUP2(0x00000000),
		/* 9X*/	DUP2(0x00000000),
		/* AX*/	DUP2(0x00000000),
		/* BX*/	DUP2(0x00000000),
		/* CX*/	DUP2(0x00000000),
		/* DX*/	DUP2(0x00000000),
		/* EX*/	DUP2(0x00000000),
		/* FX*/	DUP2(0x00007FFF)
	},
	//arm7
	{
		/* 0X*/	DUP2(0x00003FFF),
		/* 1X*/	DUP2(0x00000000),
		/* 2X*/	DUP2(0x003FFFFF),
		/* 3X*/	     0x00007FFF,
		             0x0000FFFF,
		/* 4X*/	     0x00000000,
		             0x0000FFFF,
		/* 5X*/	DUP2(0x00000000),
		/* 6X*/ DUP2(0x0003FFFF),
		/* 7X*/	DUP2(0x00000000),
		/* 8X*/	DUP2(0x00000000),
		/* 9X*/	DUP2(0x00000000),
		/* AX*/	DUP2(0x00000000),
		/* BX*/	DUP2(0x00000000),
		/* CX*/	DUP2(0x00000000),
		/* DX*/	DUP2(0x00000000),
		/* EX*/	DUP2(0x00000000),
		/* FX*/	DUP2(0x00000000)
		}
};

#undef DUP2


void Init()
{
    memset(&cache, 0, sizeof(BlockCache));

    for (int cpu = 0; cpu < 2; cpu++)
        for (int i = 0; i < 0x4000; i++)
            cache.AddrMapping[cpu][i] = JIT_MEM[cpu][i >> 9] == -1 ? NULL :
				(CompiledBlock*)((u8*)&cache + JIT_MEM[cpu][i >> 9])
                + (((i << 14) & JIT_MASK[cpu][i >> 9]) >> 1);

	compiler = new Compiler();
}

void DeInit()
{
	delete compiler;
}

CompiledBlock CompileBlock(ARM* cpu)
{
    bool thumb = cpu->CPSR & 0x20;

    FetchedInstr instrs[12];
    int i = 0;
    u32 r15 = cpu->R[15];
    u32 nextInstr[2] = {cpu->NextInstr[0], cpu->NextInstr[1]};
    //printf("block %x %d\n", r15, thumb);
    do
    {
        r15 += thumb ? 2 : 4;

        instrs[i].Instr = nextInstr[0];
        //printf("%x %x\n", instrs[i].Instr, r15);
        instrs[i].NextInstr[0] = nextInstr[0] = nextInstr[1];

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
        instrs[i].NextInstr[1] = nextInstr[1];
        instrs[i].Info = ARMInstrInfo::Decode(thumb, cpu->Num, instrs[i].Instr);

        i++;
    } while(!instrs[i - 1].Info.Branches() && i < 10);

    CompiledBlock block = compiler->CompileBlock(cpu, instrs, i);

    InsertBlock(cpu->Num, cpu->R[15] - (thumb ? 2 : 4), block);

    return block;
}

void ResetBlocks()
{
	memset(cache.MainRAM, 0, sizeof(cache.MainRAM));
	memset(cache.SWRAM, 0, sizeof(cache.SWRAM));
	memset(cache.ARM9_BIOS, 0, sizeof(cache.ARM9_BIOS));
	memset(cache.ARM9_ITCM, 0, sizeof(cache.ARM9_ITCM));
	memset(cache.ARM9_LCDC, 0, sizeof(cache.ARM9_LCDC));
	memset(cache.ARM7_BIOS, 0, sizeof(cache.ARM7_BIOS));
	memset(cache.ARM7_WRAM, 0, sizeof(cache.ARM7_WRAM));
	memset(cache.ARM7_WVRAM, 0, sizeof(cache.ARM7_WVRAM));
}

}