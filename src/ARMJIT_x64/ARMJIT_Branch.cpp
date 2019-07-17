#include "ARMJIT_Compiler.h"

using namespace Gen;

namespace ARMJIT
{
    
void Compiler::Comp_JumpTo(u32 addr, bool forceNonConstantCycles)
{
    // we can simplify constant branches by a lot
    // it's not completely safe to assume stuff like, which instructions to preload
    // we'll see how it works out

    u32 newPC;
    u32 nextInstr[2];
    u32 cycles = 0;
    bool setupRegion = false;

    if (addr & 0x1 && !Thumb)
    {
        CPSRDirty = true;
        OR(32, R(RCPSR), Imm8(0x20));
    }
    else if (!(addr & 0x1) && Thumb)
    {
        CPSRDirty = true;
        AND(32, R(RCPSR), Imm32(~0x20));
    }

    if (Num == 0)
    {
        ARMv5* cpu9 = (ARMv5*)CurCPU;

        u32 oldregion = R15 >> 24;
        u32 newregion = addr >> 24;

        u32 regionCodeCycles = cpu9->MemTimings[addr >> 12][0];
        u32 compileTimeCodeCycles = cpu9->RegionCodeCycles;
        cpu9->RegionCodeCycles = regionCodeCycles;

        MOV(32, MDisp(RCPU, offsetof(ARMv5, RegionCodeCycles)), Imm32(regionCodeCycles));

        setupRegion = newregion != oldregion;
        if (setupRegion)
            cpu9->SetupCodeMem(addr);

        if (addr & 0x1)
        {
            addr &= ~0x1;
            newPC = addr+2;

            // two-opcodes-at-once fetch
            // doesn't matter if we put garbage in the MSbs there
            if (addr & 0x2)
            {
                nextInstr[0] = cpu9->CodeRead32(addr-2, true) >> 16;
                cycles += cpu9->CodeCycles;
                nextInstr[1] = cpu9->CodeRead32(addr+2, false);
                cycles += CurCPU->CodeCycles;
            }
            else
            {
                nextInstr[0] = cpu9->CodeRead32(addr, true);
                nextInstr[1] = nextInstr[0] >> 16;
                cycles += cpu9->CodeCycles;
            }
        }
        else
        {
            addr &= ~0x3;
            newPC = addr+4;

            nextInstr[0] = cpu9->CodeRead32(addr, true);
            cycles += cpu9->CodeCycles;
            nextInstr[1] = cpu9->CodeRead32(addr+4, false);
            cycles += cpu9->CodeCycles;
        }

        cpu9->RegionCodeCycles = compileTimeCodeCycles;
        if (setupRegion)
            cpu9->SetupCodeMem(R15);
    }
    else
    {
        ARMv4* cpu7 = (ARMv4*)CurCPU;

        u32 codeRegion = addr >> 24;
        u32 codeCycles = addr >> 15; // cheato

        cpu7->CodeRegion = codeRegion;
        cpu7->CodeCycles = codeCycles;

        MOV(32, MDisp(RCPU, offsetof(ARM, CodeRegion)), Imm32(codeRegion));
        MOV(32, MDisp(RCPU, offsetof(ARM, CodeCycles)), Imm32(codeCycles));

        if (addr & 0x1)
        {
            addr &= ~0x1;
            newPC = addr+2;

            // this is necessary because ARM7 bios protection
            u32 compileTimePC = CurCPU->R[15];
            CurCPU->R[15] = newPC;

            nextInstr[0] = ((ARMv4*)CurCPU)->CodeRead16(addr);
            nextInstr[1] = ((ARMv4*)CurCPU)->CodeRead16(addr+2);
            cycles += NDS::ARM7MemTimings[codeCycles][0] + NDS::ARM7MemTimings[codeCycles][1];

            CurCPU->R[15] = compileTimePC;
        }
        else
        {
            addr &= ~0x3;
            newPC = addr+4;

            u32 compileTimePC = CurCPU->R[15];
            CurCPU->R[15] = newPC;

            nextInstr[0] = cpu7->CodeRead32(addr);
            nextInstr[1] = cpu7->CodeRead32(addr+4);
            cycles += NDS::ARM7MemTimings[codeCycles][2] + NDS::ARM7MemTimings[codeCycles][3];

            CurCPU->R[15] = compileTimePC;
        }

        cpu7->CodeRegion = R15 >> 24;
        cpu7->CodeCycles = addr >> 15;
    }

    MOV(32, MDisp(RCPU, offsetof(ARM, R[15])), Imm32(newPC));
    MOV(32, MDisp(RCPU, offsetof(ARM, NextInstr[0])), Imm32(nextInstr[0]));
    MOV(32, MDisp(RCPU, offsetof(ARM, NextInstr[1])), Imm32(nextInstr[1]));
    if ((Thumb || CurInstr.Cond() >= 0xE) && !forceNonConstantCycles)
        ConstantCycles += cycles;
    else
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));

    if (setupRegion)
    {
        MOV(64, R(ABI_PARAM1), R(RCPU));
        MOV(32, R(ABI_PARAM2), Imm32(newPC));
        CALL((void*)&ARMv5::SetupCodeMem);
    }
}

void Compiler::Comp_JumpTo(Gen::X64Reg addr, bool restoreCPSR)
{
    BitSet16 hiRegsLoaded(RegCache.DirtyRegs & 0xFF00);
    bool previouslyDirty = CPSRDirty;
    SaveCPSR();

    if (restoreCPSR)
    {
        if (Thumb || CurInstr.Cond() >= 0xE)
            RegCache.Flush();
        else
        {
            // the ugly way...
            // we only save them, to load and save them again
            for (int reg : hiRegsLoaded)
                SaveReg(reg, RegCache.Mapping[reg]);
        }
    }

    MOV(64, R(ABI_PARAM1), R(RCPU));
    MOV(32, R(ABI_PARAM2), R(addr));
    if (!restoreCPSR)
        XOR(32, R(ABI_PARAM3), R(ABI_PARAM3));
    else
        MOV(32, R(ABI_PARAM3), Imm32(restoreCPSR));
    if (Num == 0)
        CALL((void*)&ARMv5::JumpTo);
    else
        CALL((void*)&ARMv4::JumpTo);
    
    if (!Thumb && restoreCPSR && CurInstr.Cond() < 0xE)
    {
        for (int reg : hiRegsLoaded)
            LoadReg(reg, RegCache.Mapping[reg]);
    }

    if (previouslyDirty)
        LoadCPSR();
    CPSRDirty = previouslyDirty;
}

void Compiler::A_Comp_BranchImm()
{
    int op = (CurInstr.Instr >> 24) & 1;
    s32 offset = (s32)(CurInstr.Instr << 8) >> 6;
    u32 target = R15 + offset;
    bool link = op;

    if (CurInstr.Cond() == 0xF) // BLX_imm
    {
        target += (op << 1) + 1;
        link = true;
    }

    if (link)
        MOV(32, MapReg(14), Imm32(R15 - 4));

    Comp_JumpTo(target);
}

void Compiler::A_Comp_BranchXchangeReg()
{
    OpArg rn = MapReg(CurInstr.A_Reg(0));
    MOV(32, R(RSCRATCH), rn);
    if ((CurInstr.Instr & 0xF0) == 0x30) // BLX_reg
        MOV(32, MapReg(14), Imm32(R15 - 4));
    Comp_JumpTo(RSCRATCH);
}

void Compiler::T_Comp_BCOND()
{
    u32 cond = (CurInstr.Instr >> 8) & 0xF;
    FixupBranch skipExecute = CheckCondition(cond);

    s32 offset = (s32)(CurInstr.Instr << 24) >> 23;
    Comp_JumpTo(R15 + offset + 1, true);

    FixupBranch skipFailed = J();
    SetJumpTarget(skipExecute);
    Comp_AddCycles_C(true);
   SetJumpTarget(skipFailed);
}

void Compiler::T_Comp_B()
{
    s32 offset = (s32)((CurInstr.Instr & 0x7FF) << 21) >> 20;
    Comp_JumpTo(R15 + offset + 1);
}

void Compiler::T_Comp_BranchXchangeReg()
{
    bool link = CurInstr.Instr & (1 << 7);
    if (link && Num == 1)
    {
        printf("BLX unsupported on ARM7!!!\n");
        return;
    }

    OpArg rn = MapReg(CurInstr.A_Reg(3));
    if (link)
        MOV(32, MapReg(14), Imm32(R15 - 1));
    Comp_JumpTo(rn.GetSimpleReg());
}

void Compiler::T_Comp_BL_LONG_1()
{
    s32 offset = (s32)((CurInstr.Instr & 0x7FF) << 21) >> 9;
    MOV(32, MapReg(14), Imm32(R15 + offset));
    Comp_AddCycles_C();
}

void Compiler::T_Comp_BL_LONG_2()
{
    OpArg lr = MapReg(14);
    s32 offset = (CurInstr.Instr & 0x7FF) << 1;
    LEA(32, RSCRATCH, MDisp(lr.GetSimpleReg(), offset));
    MOV(32, lr, Imm32((R15 - 2) | 1));
    if (Num == 1 || CurInstr.Instr & (1 << 12))
        OR(32, R(RSCRATCH), Imm8(1));
    Comp_JumpTo(RSCRATCH);
}

void Compiler::T_Comp_BL_Merged(FetchedInstr part1)
{
    assert(part1.Info.Kind == ARMInstrInfo::tk_BL_LONG_1);
    Comp_AddCycles_C();

    u32 target = (R15 - 2) + ((s32)((part1.Instr & 0x7FF) << 21) >> 9);
    target += (CurInstr.Instr & 0x7FF) << 1;

    if (Num == 1 || CurInstr.Instr & (1 << 12))
        target |= 1;

    MOV(32, MapReg(14), Imm32((R15 - 2) | 1));
    
    Comp_JumpTo(target);
}

}