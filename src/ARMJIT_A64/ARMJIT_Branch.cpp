/*
    Copyright 2016-2022 melonDS team, RSDuck

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
#include "../NDS.h"

using namespace Arm64Gen;

// hack
const int kCodeCacheTiming = 3;

namespace melonDS
{

template <typename T>
void JumpToTrampoline(T* cpu, u32 addr, bool changeCPSR)
{
    cpu->JumpTo(addr, changeCPSR);
}

void Compiler::Comp_JumpTo(u32 addr, bool forceNonConstantCycles)
{
    // we can simplify constant branches by a lot
    // it's not completely safe to assume stuff like, which instructions to preload
    // we'll see how it works out

    IrregularCycles = true;

    u32 newPC;
    u32 cycles = 0;
    bool setupRegion = false;

    if (addr & 0x1 && !Thumb)
    {
        CPSRDirty = true;
        ORRI2R(RCPSR, RCPSR, 0x20);
    }
    else if (!(addr & 0x1) && Thumb)
    {
        CPSRDirty = true;
        ANDI2R(RCPSR, RCPSR, ~0x20);
    }

    if (Num == 0)
    {
        ARMv5* cpu9 = (ARMv5*)CurCPU;

        u32 oldregion = R15 >> 24;
        u32 newregion = addr >> 24;

        u32 regionCodeCycles = cpu9->MemTimings[addr >> 12][0];
        u32 compileTimeCodeCycles = cpu9->RegionCodeCycles;
        cpu9->RegionCodeCycles = regionCodeCycles;

        MOVI2R(W0, regionCodeCycles);
        STR(INDEX_UNSIGNED, W0, RCPU, offsetof(ARMv5, RegionCodeCycles));

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
                cpu9->CodeRead32(addr-2, true) >> 16;
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

        MOVI2R(W0, codeRegion);
        STR(INDEX_UNSIGNED, W0, RCPU, offsetof(ARM, CodeRegion));
        MOVI2R(W0, codeCycles);
        STR(INDEX_UNSIGNED, W0, RCPU, offsetof(ARM, CodeCycles));

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
    {
        MOVI2R(W0, newPC);
        STR(INDEX_UNSIGNED, W0, RCPU, offsetof(ARM, R[15]));
    }
    if ((Thumb || CurInstr.Cond() >= 0xE) && !forceNonConstantCycles)
        ConstantCycles += cycles;
    else
        ADD(RCycles, RCycles, cycles);
}


void* Compiler::Gen_JumpTo9(int kind)
{
    AlignCode16();
    void* res = GetRXPtr();

    LSR(W1, W0, 12);
    ADDI2R(W1, W1, offsetof(ARMv5, MemTimings), W2);
    LDRB(W1, RCPU, W1);

    LDR(INDEX_UNSIGNED, W2, RCPU, offsetof(ARMv5, ITCMSize));

    STR(INDEX_UNSIGNED, W1, RCPU, offsetof(ARMv5, RegionCodeCycles));

    CMP(W1, 0xFF);
    MOVI2R(W3, kCodeCacheTiming);
    CSEL(W1, W3, W1, CC_EQ);
    CMP(W0, W2);
    CSINC(W1, W1, WZR, CC_HS);

    FixupBranch switchToThumb;
    if (kind == 0)
        switchToThumb = TBNZ(W0, 0);

    if (kind == 0 || kind == 1)
    {
        // ARM
        if (kind == 0)
            ANDI2R(RCPSR, RCPSR, ~0x20);

        ANDI2R(W0, W0, ~3);
        ADD(W0, W0, 4);
        STR(INDEX_UNSIGNED, W0, RCPU, offsetof(ARMv5, R[15]));

        ADD(W1, W1, W1);
        ADD(RCycles, RCycles, W1);
        RET();
    }

    if (kind == 0 || kind == 2)
    {
        // Thumb
        if (kind == 0)
        {
            SetJumpTarget(switchToThumb);
            ORRI2R(RCPSR, RCPSR, 0x20);
        }

        ANDI2R(W0, W0, ~1);
        ADD(W0, W0, 2);
        STR(INDEX_UNSIGNED, W0, RCPU, offsetof(ARMv5, R[15]));

        ADD(W2, W1, W1);
        TSTI2R(W0, 0x2);
        CSEL(W1, W1, W2, CC_EQ);
        ADD(RCycles, RCycles, W1);
        RET();
    }

    return res;
}

void* Compiler::Gen_JumpTo7(int kind)
{
    void* res = GetRXPtr();

    LSR(W1, W0, 24);
    STR(INDEX_UNSIGNED, W1, RCPU, offsetof(ARM, CodeRegion));
    LSR(W1, W0, 15);
    STR(INDEX_UNSIGNED, W1, RCPU, offsetof(ARM, CodeCycles));

    MOVP2R(X2, NDS.ARM7MemTimings);
    LDR(W3, X2, ArithOption(W1, true));

    FixupBranch switchToThumb;
    if (kind == 0)
        switchToThumb = TBNZ(W0, 0);
    
    if (kind == 0 || kind == 1)
    {
        UBFX(W2, W3, 0, 8);
        UBFX(W3, W3, 8, 8);
        ADD(W2, W3, W2);
        ADD(RCycles, RCycles, W2);

        ANDI2R(W0, W0, ~3);

        if (kind == 0)
            ANDI2R(RCPSR, RCPSR, ~0x20);

        ADD(W3, W0, 4);
        STR(INDEX_UNSIGNED, W3, RCPU, offsetof(ARM, R[15]));

        RET();
    }
    if (kind == 0 || kind == 2)
    {
        if (kind == 0)
        {
            SetJumpTarget(switchToThumb);

            ORRI2R(RCPSR, RCPSR, 0x20);
        }

        UBFX(W2, W3, 16, 8);
        UBFX(W3, W3, 24, 8);
        ADD(W2, W3, W2);
        ADD(RCycles, RCycles, W2);

        ANDI2R(W0, W0, ~1);

        ADD(W3, W0, 2);
        STR(INDEX_UNSIGNED, W3, RCPU, offsetof(ARM, R[15]));

        RET();
    }

    return res;
}

void Compiler::Comp_JumpTo(Arm64Gen::ARM64Reg addr, bool switchThumb, bool restoreCPSR)
{
    IrregularCycles = true;

    if (!restoreCPSR)
    {
        if (switchThumb)
            CPSRDirty = true;
        MOV(W0, addr);
        BL((Num ? JumpToFuncs7 : JumpToFuncs9)[switchThumb ? 0 : (Thumb + 1)]);
    }
    else
    {
        
        bool cpsrDirty = CPSRDirty;
        SaveCPSR();
        SaveCycles();
        PushRegs(restoreCPSR, true);

        if (switchThumb)
            MOV(W1, addr);
        else
        {
            if (Thumb)
                ORRI2R(W1, addr, 1);
            else
                ANDI2R(W1, addr, ~1);
        }
        MOV(X0, RCPU);
        MOVI2R(W2, restoreCPSR);
        if (Num == 0)
            QuickCallFunction(X3, JumpToTrampoline<ARMv5>);
        else
            QuickCallFunction(X3, JumpToTrampoline<ARMv4>);

        PopRegs(restoreCPSR, true);
        LoadCycles();
        LoadCPSR();
        if (CurInstr.Cond() < 0xE)
            CPSRDirty = cpsrDirty;
    }
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
        MOVI2R(MapReg(14), R15 - 4);

    Comp_JumpTo(target);
}

void Compiler::A_Comp_BranchXchangeReg()
{
    ARM64Reg rn = MapReg(CurInstr.A_Reg(0));
    MOV(W0, rn);
    if ((CurInstr.Instr & 0xF0) == 0x30) // BLX_reg
        MOVI2R(MapReg(14), R15 - 4);
    Comp_JumpTo(W0, true);
}

void Compiler::T_Comp_BCOND()
{
    u32 cond = (CurInstr.Instr >> 8) & 0xF;
    FixupBranch skipExecute = CheckCondition(cond);

    s32 offset = (s32)(CurInstr.Instr << 24) >> 23;
    Comp_JumpTo(R15 + offset + 1, true);

    Comp_BranchSpecialBehaviour(true);

    FixupBranch skipFailed = B();
    SetJumpTarget(skipExecute);
    Comp_AddCycles_C(true);

    Comp_BranchSpecialBehaviour(false);

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
        MOV(W0, MapReg(CurInstr.A_Reg(3)));
        MOVI2R(MapReg(14), R15 - 1);
        Comp_JumpTo(W0, true);
    }
    else
    {
        ARM64Reg rn = MapReg(CurInstr.A_Reg(3));
        Comp_JumpTo(rn, true);
    }
}

void Compiler::T_Comp_BL_LONG_1()
{
    s32 offset = (s32)((CurInstr.Instr & 0x7FF) << 21) >> 9;
    MOVI2R(MapReg(14), R15 + offset);
    Comp_AddCycles_C();
}

void Compiler::T_Comp_BL_LONG_2()
{
    ARM64Reg lr = MapReg(14);
    s32 offset = (CurInstr.Instr & 0x7FF) << 1;
    ADD(W0, lr, offset);
    MOVI2R(lr, (R15 - 2) | 1);
    Comp_JumpTo(W0, Num == 0 && !(CurInstr.Instr & (1 << 12)));
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

    MOVI2R(MapReg(14), (R15 - 2) | 1);
    
    Comp_JumpTo(target);
}

}