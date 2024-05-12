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

#include "ARMJIT_Compiler.h"
#include "../ARM.h"
#include "../NDS.h"

using namespace Gen;

namespace melonDS
{

template <typename T>
int squeezePointer(T* ptr)
{
    int truncated = (int)((u64)ptr);
    assert((T*)((u64)truncated) == ptr);
    return truncated;
}

void Compiler::Comp_JumpTo(u32 addr, bool forceNonConstantCycles)
{
    // we can simplify constant branches by a lot
    IrregularCycles = true;

    u32 newPC;
    u32 cycles = 0;

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

        u32 regionCodeCycles = cpu9->MemTimings[addr >> 12][0];
        u32 compileTimeCodeCycles = cpu9->RegionCodeCycles;
        cpu9->RegionCodeCycles = regionCodeCycles;

        if (Exit)
            MOV(32, MDisp(RCPU, offsetof(ARMv5, RegionCodeCycles)), Imm32(regionCodeCycles));

        if (addr & 0x1)
        {
            addr &= ~0x1;
            newPC = addr+2;

            // two-opcodes-at-once fetch
            // doesn't matter if we put garbage in the MSbs there
            if (addr & 0x2)
            {
                cpu9->CodeRead32(addr-2, true);
                cycles += cpu9->CodeCycles;
                cpu9->CodeRead32(addr+2, false);
                cycles += CurCPU->CodeCycles;
            }
            else
            {
                cpu9->CodeRead32(addr, true);
                cycles += cpu9->CodeCycles;
            }
        }
        else
        {
            addr &= ~0x3;
            newPC = addr+4;

            cpu9->CodeRead32(addr, true);
            cycles += cpu9->CodeCycles;
            cpu9->CodeRead32(addr+4, false);
            cycles += cpu9->CodeCycles;
        }

        cpu9->RegionCodeCycles = compileTimeCodeCycles;
    }
    else
    {
        ARMv4* cpu7 = (ARMv4*)CurCPU;

        u32 codeRegion = addr >> 24;
        u32 codeCycles = addr >> 15; // cheato

        cpu7->CodeRegion = codeRegion;
        cpu7->CodeCycles = codeCycles;

        if (Exit)
        {
            MOV(32, MDisp(RCPU, offsetof(ARM, CodeRegion)), Imm32(codeRegion));
            MOV(32, MDisp(RCPU, offsetof(ARM, CodeCycles)), Imm32(codeCycles));
        }

        if (addr & 0x1)
        {
            addr &= ~0x1;
            newPC = addr+2;

            // this is necessary because ARM7 bios protection
            u32 compileTimePC = CurCPU->R[15];
            CurCPU->R[15] = newPC;

            cycles += NDS.ARM7MemTimings[codeCycles][0] + NDS.ARM7MemTimings[codeCycles][1];

            CurCPU->R[15] = compileTimePC;
        }
        else
        {
            addr &= ~0x3;
            newPC = addr+4;

            u32 compileTimePC = CurCPU->R[15];
            CurCPU->R[15] = newPC;

            cycles += NDS.ARM7MemTimings[codeCycles][2] + NDS.ARM7MemTimings[codeCycles][3];

            CurCPU->R[15] = compileTimePC;
        }

        cpu7->CodeRegion = R15 >> 24;
        cpu7->CodeCycles = addr >> 15;
    }

    if (Exit)
        MOV(32, MDisp(RCPU, offsetof(ARM, R[15])), Imm32(newPC));
    if ((Thumb || CurInstr.Cond() >= 0xE) && !forceNonConstantCycles)
        ConstantCycles += cycles;
    else
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
}

void ARMv4JumpToTrampoline(ARMv4* arm, u32 addr, bool restorecpsr)
{
    arm->JumpTo(addr, restorecpsr);
}

void ARMv5JumpToTrampoline(ARMv5* arm, u32 addr, bool restorecpsr)
{
    arm->JumpTo(addr, restorecpsr);
}

void Compiler::Comp_JumpTo(Gen::X64Reg addr, bool restoreCPSR)
{
    IrregularCycles = true;

    bool cpsrDirty = CPSRDirty;
    SaveCPSR();

    PushRegs(restoreCPSR, true);

    MOV(64, R(ABI_PARAM1), R(RCPU));
    MOV(32, R(ABI_PARAM2), R(addr));
    if (!restoreCPSR)
        XOR(32, R(ABI_PARAM3), R(ABI_PARAM3));
    else
        MOV(32, R(ABI_PARAM3), Imm32(true)); // what a waste
    if (Num == 0)
        CALL((void*)&ARMv5JumpToTrampoline);
    else
        CALL((void*)&ARMv4JumpToTrampoline);

    PopRegs(restoreCPSR, true);

    LoadCPSR();
    // in case this instruction is skipped
    if (CurInstr.Cond() < 0xE)
        CPSRDirty = cpsrDirty;
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

    Comp_SpecialBranchBehaviour(true);

    FixupBranch skipFailed = J();
    SetJumpTarget(skipExecute);

    Comp_SpecialBranchBehaviour(false);

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

    if (link)
    {
        if (Num == 1)
        {
            Log(LogLevel::Warn, "BLX unsupported on ARM7!!!\n");
            return;
        }
        MOV(32, R(RSCRATCH), MapReg(CurInstr.A_Reg(3)));
        MOV(32, MapReg(14), Imm32(R15 - 1));
        Comp_JumpTo(RSCRATCH);
    }
    else
    {
        OpArg rn = MapReg(CurInstr.A_Reg(3));
        Comp_JumpTo(rn.GetSimpleReg());
    }
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

void Compiler::T_Comp_BL_Merged()
{
    Comp_AddCycles_C();

    R15 += 2;

    u32 upperPart = CurInstr.Instr >> 16;
    u32 target = (R15 - 2) + ((s32)((CurInstr.Instr & 0x7FF) << 21) >> 9);
    target += (upperPart & 0x7FF) << 1;

    if (Num == 1 || upperPart & (1 << 12))
        target |= 1;

    MOV(32, MapReg(14), Imm32((R15 - 2) | 1));

    Comp_JumpTo(target);
}

}
