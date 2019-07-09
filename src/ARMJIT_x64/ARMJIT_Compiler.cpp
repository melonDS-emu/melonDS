#include "ARMJIT_Compiler.h"

#include "../ARMInterpreter.h"

#include <assert.h>

using namespace Gen;

namespace ARMJIT
{
template <>
const X64Reg RegisterCache<Compiler, X64Reg>::NativeRegAllocOrder[] =
{
#ifdef _WIN32
    RBX, RSI, RDI, R12, R13, R14
#else
    RBX, R12, R13, R14 // this is sad
#endif
};
template <>
const int RegisterCache<Compiler, X64Reg>::NativeRegsAvailable =
#ifdef _WIN32
    6
#else
    4
#endif
;

Compiler::Compiler()
{
    AllocCodeSpace(1024 * 1024 * 16);

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            MemoryFuncs9[i][j] = Gen_MemoryRoutine9(j, 8 << i);
            MemoryFuncs7[i][j][0] = Gen_MemoryRoutine7(j, false, 8 << i);
            MemoryFuncs7[i][j][1] = Gen_MemoryRoutine7(j, true, 8 << i);
        }
    }
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
        {
            MemoryFuncsSeq9[i][j] = Gen_MemoryRoutineSeq9(i, j);
            MemoryFuncsSeq7[i][j][0] = Gen_MemoryRoutineSeq7(i, j, false);
            MemoryFuncsSeq7[i][j][1] = Gen_MemoryRoutineSeq7(i, j, true);
        }

    ResetStart = GetWritableCodePtr();
}

void* Compiler::Gen_ChangeCPSRRoutine()
{
    void* res = (void*)GetWritableCodePtr();

    MOV(32, R(RSCRATCH), R(RCPSR));
    AND(32, R(RSCRATCH), Imm8(0x1F));
    CMP(32, R(RSCRATCH), Imm8(0x11));
    FixupBranch fiq = J_CC(CC_E);
    CMP(32, R(RSCRATCH), Imm8(0x12));
    FixupBranch irq = J_CC(CC_E);
    CMP(32, R(RSCRATCH), Imm8(0x13));
    FixupBranch svc = J_CC(CC_E);
    CMP(32, R(RSCRATCH), Imm8(0x17));
    FixupBranch abt = J_CC(CC_E);
    CMP(32, R(RSCRATCH), Imm8(0x1B));
    FixupBranch und = J_CC(CC_E);

    SetJumpTarget(fiq);

    SetJumpTarget(irq);

    SetJumpTarget(svc);

    SetJumpTarget(abt);

    SetJumpTarget(und);

    return res;
}

DataRegion Compiler::ClassifyAddress(u32 addr)
{
    if (Num == 0 && addr >= ((ARMv5*)CurCPU)->DTCMBase && addr < ((ARMv5*)CurCPU)->DTCMBase)
        return dataRegionDTCM;
    switch (addr & 0xFF000000)
    {
        case 0x02000000: return dataRegionMainRAM;
        case 0x03000000: return Num == 1 && (addr & 0xF00000) == 0x800000 ? dataRegionWRAM7 : dataRegionSWRAM;
        case 0x04000000: return dataRegionIO;
        case 0x06000000: return dataRegionVRAM;
    }
    return dataRegionGeneric;
}

void Compiler::LoadCPSR()
{
    assert(!CPSRDirty);

    MOV(32, R(RCPSR), MDisp(RCPU, offsetof(ARM, CPSR)));
}

void Compiler::SaveCPSR()
{
    if (CPSRDirty)
    {
        MOV(32, MDisp(RCPU, offsetof(ARM, CPSR)), R(RCPSR));
        CPSRDirty = false;
    }
}

void Compiler::LoadReg(int reg, X64Reg nativeReg)
{
    if (reg != 15)
        MOV(32, R(nativeReg), MDisp(RCPU, offsetof(ARM, R[reg])));
    else
        MOV(32, R(nativeReg), Imm32(R15));
}

void Compiler::SaveReg(int reg, X64Reg nativeReg)
{
    MOV(32, MDisp(RCPU, offsetof(ARM, R[reg])), R(nativeReg));
}

CompiledBlock Compiler::CompileBlock(ARM* cpu, FetchedInstr instrs[], int instrsCount)
{
    if (IsAlmostFull())
    {
        ResetBlocks();
        SetCodePtr((u8*)ResetStart);
    }

    CompiledBlock res = (CompiledBlock)GetWritableCodePtr();

    ConstantCycles = 0;
    Thumb = cpu->CPSR & 0x20;
    Num = cpu->Num;
    R15 = cpu->R[15];
    CodeRegion = cpu->CodeRegion;
    CurCPU = cpu;

    ABI_PushRegistersAndAdjustStack({ABI_ALL_CALLEE_SAVED & ABI_ALL_GPRS}, 8, 16);

    MOV(64, R(RCPU), ImmPtr(cpu));

    LoadCPSR();

    // TODO: this is ugly as a whole, do better
    RegCache = RegisterCache<Compiler, X64Reg>(this, instrs, instrsCount);

    for (int i = 0; i < instrsCount; i++)
    {
        R15 += Thumb ? 2 : 4;
        CurInstr = instrs[i];

        CompileFunc comp = GetCompFunc(CurInstr.Info.Kind);

        if (comp == NULL || i == instrsCount - 1)
        {
            MOV(32, MDisp(RCPU, offsetof(ARM, R[15])), Imm32(R15));
            MOV(32, MDisp(RCPU, offsetof(ARM, CodeCycles)), Imm32(CurInstr.CodeCycles));
            MOV(32, MDisp(RCPU, offsetof(ARM, CurInstr)), Imm32(CurInstr.Instr));
            if (i == instrsCount - 1)
            {
                MOV(32, MDisp(RCPU, offsetof(ARM, NextInstr[0])), Imm32(CurInstr.NextInstr[0]));
                MOV(32, MDisp(RCPU, offsetof(ARM, NextInstr[1])), Imm32(CurInstr.NextInstr[1]));
            }

            if (comp == NULL || CurInstr.Info.Branches())
                SaveCPSR();
        }

        // run interpreter
        cpu->CodeCycles = CurInstr.CodeCycles;
        cpu->R[15] = R15;
        cpu->CurInstr = CurInstr.Instr;
        cpu->NextInstr[0] = CurInstr.NextInstr[0];
        cpu->NextInstr[1] = CurInstr.NextInstr[1];

        if (comp != NULL)
            RegCache.Prepare(i);
        else
            RegCache.Flush();

        if (Thumb)
        {
            u32 icode = (CurInstr.Instr >> 6) & 0x3FF;
            if (comp == NULL)
            {
                MOV(64, R(ABI_PARAM1), R(RCPU));

                ABI_CallFunction(ARMInterpreter::THUMBInstrTable[icode]);
            }
            else
                (this->*comp)();

            ARMInterpreter::THUMBInstrTable[icode](cpu);
        }
        else
        {
            u32 cond = CurInstr.Cond();
            if (CurInstr.Info.Kind == ARMInstrInfo::ak_BLX_IMM)
            {
                MOV(64, R(ABI_PARAM1), R(RCPU));
                ABI_CallFunction(ARMInterpreter::A_BLX_IMM);

                ARMInterpreter::A_BLX_IMM(cpu);
            }
            else if (cond == 0xF)
            {
                Comp_AddCycles_C();
                cpu->AddCycles_C();
            }
            else
            {
                FixupBranch skipExecute;
                if (cond < 0xE)
                {
                    if (cond >= 0x8)
                    {
                        static_assert(RSCRATCH3 == ECX);
                        MOV(32, R(RSCRATCH3), R(RCPSR));
                        SHR(32, R(RSCRATCH3), Imm8(28));
                        MOV(32, R(RSCRATCH), Imm32(1));
                        SHL(32, R(RSCRATCH), R(RSCRATCH3));
                        TEST(32, R(RSCRATCH), Imm32(ARM::ConditionTable[cond]));

                        skipExecute = J_CC(CC_Z);
                    }
                    else
                    {
                        // could have used a LUT, but then where would be the fun?
                        TEST(32, R(RCPSR), Imm32(1 << (28 + ((~(cond >> 1) & 1) << 1 | (cond >> 2 & 1) ^ (cond >> 1 & 1)))));

                        skipExecute = J_CC(cond & 1 ? CC_NZ : CC_Z);
                    }

                }

                u32 icode = ((CurInstr.Instr >> 4) & 0xF) | ((CurInstr.Instr >> 16) & 0xFF0);
                if (comp == NULL)
                {
                    MOV(64, R(ABI_PARAM1), R(RCPU));

                    ABI_CallFunction(ARMInterpreter::ARMInstrTable[icode]);
                }
                else
                    (this->*comp)();

                FixupBranch skipFailed;
                if (CurInstr.Cond() < 0xE)
                {
                    skipFailed = J();
                    SetJumpTarget(skipExecute);

                    Comp_AddCycles_C();

                    SetJumpTarget(skipFailed);
                }

                if (cpu->CheckCondition(cond))
                    ARMInterpreter::ARMInstrTable[icode](cpu);
                else
                    cpu->AddCycles_C();
            }
        }

        /*
            we don't need to collect the interpreted cycles,
            since cpu->Cycles is taken into account by the dispatcher.
        */

        if (comp == NULL && i != instrsCount - 1)
            LoadCPSR();
    }

    RegCache.Flush();
    SaveCPSR();

    MOV(32, R(RAX), Imm32(ConstantCycles));

    ABI_PopRegistersAndAdjustStack({ABI_ALL_CALLEE_SAVED & ABI_ALL_GPRS}, 8, 16);
    RET();

    return res;
}

CompileFunc Compiler::GetCompFunc(int kind)
{
    // this might look like waste of space, so many repeatitions, but it's invaluable for debugging.
    // see ARMInstrInfo.h for the order
    const CompileFunc A_Comp[ARMInstrInfo::ak_Count] =
    {
        // AND
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        // EOR
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        // SUB
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        // RSB
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        // ADD
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        // ADC
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        // SBC
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        // RSC
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        // ORR
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        // MOV
        A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp,
        A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp,
        // BIC
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith, A_Comp_Arith,
        // MVN
        A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp,
        A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp, A_Comp_MovOp,
        // TST
        A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp,
        // TEQ
        A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp,
        // CMP
        A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp,
        // CMN
        A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp, A_Comp_CmpOp,
        // Mul
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        // ARMv5 stuff
        NULL, NULL, NULL, NULL, NULL,
        // STR
        A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB,
        //NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        // STRB
        //NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB,
        // LDR
        //NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB,
        // LDRB
        //NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB,
        // STRH
        A_Comp_MemHalf, A_Comp_MemHalf, A_Comp_MemHalf, A_Comp_MemHalf,
        // LDRD, STRD never used by anything so they stay interpreted (by anything I mean the 5 games I checked)
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        // LDRH
        A_Comp_MemHalf, A_Comp_MemHalf, A_Comp_MemHalf, A_Comp_MemHalf,
        // LDRSB
        A_Comp_MemHalf, A_Comp_MemHalf, A_Comp_MemHalf, A_Comp_MemHalf,
        // LDRSH
        A_Comp_MemHalf, A_Comp_MemHalf, A_Comp_MemHalf, A_Comp_MemHalf,
        // swap
        NULL, NULL,
        // LDM/STM
        NULL, NULL,
        // Branch
        NULL, NULL, NULL, NULL, NULL,
        // system stuff
        NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    };

    const CompileFunc T_Comp[ARMInstrInfo::tk_Count] = {
        // Shift imm
        T_Comp_ShiftImm, T_Comp_ShiftImm, T_Comp_ShiftImm,
        // Three operand ADD/SUB
        T_Comp_AddSub_, T_Comp_AddSub_, T_Comp_AddSub_, T_Comp_AddSub_,
        // 8 bit imm
        T_Comp_ALU_Imm8, T_Comp_ALU_Imm8, T_Comp_ALU_Imm8, T_Comp_ALU_Imm8,
        // general ALU
        T_Comp_ALU, T_Comp_ALU, T_Comp_ALU, T_Comp_ALU,
        T_Comp_ALU, T_Comp_ALU, T_Comp_ALU, T_Comp_ALU,
        T_Comp_ALU, T_Comp_ALU, T_Comp_ALU, T_Comp_ALU,
        T_Comp_ALU, NULL, T_Comp_ALU, T_Comp_ALU,
        // hi reg
        T_Comp_ALU_HiReg, T_Comp_ALU_HiReg, T_Comp_ALU_HiReg,
        // pc/sp relative
        T_Comp_RelAddr, T_Comp_RelAddr, T_Comp_AddSP,
        // LDR pcrel
        NULL,
        // LDR/STR reg offset
        T_Comp_MemReg, T_Comp_MemReg, T_Comp_MemReg, T_Comp_MemReg,
        // LDR/STR sign extended, half
        T_Comp_MemRegHalf, T_Comp_MemRegHalf, T_Comp_MemRegHalf, T_Comp_MemRegHalf,
        // LDR/STR imm offset
        T_Comp_MemImm, T_Comp_MemImm, T_Comp_MemImm, T_Comp_MemImm,
        // LDR/STR half imm offset
        T_Comp_MemImmHalf, T_Comp_MemImmHalf,
        // LDR/STR sp rel
        NULL, NULL,
        // PUSH/POP
        NULL, NULL, 
        // LDMIA, STMIA
        NULL, NULL, 
        NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL
    };

    return Thumb ? T_Comp[kind] : A_Comp[kind];
}

void Compiler::Comp_AddCycles_C()
{
    s32 cycles = Num ?
        NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 1 : 3]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles);

    if (CurInstr.Cond() < 0xE)
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

void Compiler::Comp_AddCycles_CI(u32 i)
{
    s32 cycles = (Num ?
        NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles)) + i;

    if (CurInstr.Cond() < 0xE)
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

void Compiler::Comp_JumpTo(Gen::X64Reg addr, bool restoreCPSR)
{
    // potentieller Bug: falls ein Register das noch gecacht ist, beim Modeswitch gespeichert
    // wird der alte Wert gespeichert
    SaveCPSR();

    MOV(64, R(ABI_PARAM1), R(RCPU));
    MOV(32, R(ABI_PARAM2), R(addr));
    MOV(32, R(ABI_PARAM3), Imm32(restoreCPSR));
    if (Num == 0)
        CALL((void*)&ARMv5::JumpTo);
    else
        CALL((void*)&ARMv4::JumpTo);
}

}