#include "ARMJIT_Compiler.h"

#include "../ARMInterpreter.h"

#include <assert.h>

using namespace Gen;

namespace ARMJIT
{
template <>
const X64Reg RegCache<Compiler, X64Reg>::NativeRegAllocOrder[] = 
{
#ifdef _WIN32
    RBX, RSI, RDI, R12, R13
#else
    RBX, R12, R13
#endif
};
template <>
const int RegCache<Compiler, X64Reg>::NativeRegsAvailable = 
#ifdef _WIN32
    5
#else
    3
#endif
;

Compiler::Compiler()
{
    AllocCodeSpace(1024 * 1024 * 16);

    for (int i = 0; i < 15; i++)
    {
        ReadMemFuncs9[i] = Gen_MemoryRoutine9(false, 32, 0x1000000 * i);
        WriteMemFuncs9[i] = Gen_MemoryRoutine9(true, 32, 0x1000000 * i);
        for (int j = 0; j < 2; j++)
        {
            ReadMemFuncs7[j][i] = Gen_MemoryRoutine7(false, 32, j, 0x1000000 * i);
            WriteMemFuncs7[j][i] = Gen_MemoryRoutine7(true, 32, j, 0x1000000 * i);
        }
    }
    ReadMemFuncs9[15] = Gen_MemoryRoutine9(false, 32, 0xFF000000);
    WriteMemFuncs9[15] = Gen_MemoryRoutine9(true, 32, 0xFF000000);
    ReadMemFuncs7[15][0] = ReadMemFuncs7[15][1] = Gen_MemoryRoutine7(false, 32, false, 0xFF000000);
    WriteMemFuncs7[15][0] = WriteMemFuncs7[15][1] = Gen_MemoryRoutine7(true, 32, false, 0xFF000000);

    ResetStart = GetWritableCodePtr();
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

    ABI_PushRegistersAndAdjustStack({ABI_ALL_CALLEE_SAVED & ABI_ALL_GPRS}, 8, 16);

    MOV(64, R(RCPU), ImmPtr(cpu));
    XOR(32, R(RCycles), R(RCycles));

    LoadCPSR();

    // TODO: this is ugly as a whole, do better
    RegCache = ARMJIT::RegCache<Compiler, X64Reg>(this, instrs, instrsCount);

    for (int i = 0; i < instrsCount; i++)
    {
        R15 += Thumb ? 2 : 4;
        CurrentInstr = instrs[i];

        CompileFunc comp = GetCompFunc(CurrentInstr.Info.Kind);

        if (CurrentInstr.Info.Branches())
            comp = NULL;

        if (comp == NULL || i == instrsCount - 1)
        {
            MOV(32, MDisp(RCPU, offsetof(ARM, R[15])), Imm32(R15));
            MOV(32, MDisp(RCPU, offsetof(ARM, CodeCycles)), Imm32(CurrentInstr.CodeCycles));
            MOV(32, MDisp(RCPU, offsetof(ARM, CurInstr)), Imm32(CurrentInstr.Instr));
            if (i == instrsCount - 1)
            {
                MOV(32, MDisp(RCPU, offsetof(ARM, NextInstr[0])), Imm32(CurrentInstr.NextInstr[0]));
                MOV(32, MDisp(RCPU, offsetof(ARM, NextInstr[1])), Imm32(CurrentInstr.NextInstr[1]));
            }

            SaveCPSR();
        }

        if (comp != NULL)
            RegCache.Prepare(i);
        else
            RegCache.Flush();

        if (Thumb)
        {
            if (comp == NULL)
            {
                MOV(64, R(ABI_PARAM1), R(RCPU));

                u32 icode = (CurrentInstr.Instr >> 6) & 0x3FF;
                ABI_CallFunction(ARMInterpreter::THUMBInstrTable[icode]);
            }
            else
                (this->*comp)();
        }
        else
        {
            u32 cond = CurrentInstr.Cond();
            if (CurrentInstr.Info.Kind == ARMInstrInfo::ak_BLX_IMM)
            {
                MOV(64, R(ABI_PARAM1), R(RCPU));
                ABI_CallFunction(ARMInterpreter::A_BLX_IMM);
            }
            else if (cond == 0xF)
                Comp_AddCycles_C();
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

                if (comp == NULL)
                {
                    MOV(64, R(ABI_PARAM1), R(RCPU));

                    u32 icode = ((CurrentInstr.Instr >> 4) & 0xF) | ((CurrentInstr.Instr >> 16) & 0xFF0);
                    ABI_CallFunction(ARMInterpreter::ARMInstrTable[icode]);
                }
                else
                    (this->*comp)();

                FixupBranch skipFailed;
                if (CurrentInstr.Cond() < 0xE)
                {
                    skipFailed = J();
                    SetJumpTarget(skipExecute);

                    Comp_AddCycles_C();

                    SetJumpTarget(skipFailed);
                }
            }
        }

        /*
            we don't need to collect the interpreted cycles,
            since all functions only add to it, the dispatcher
            takes care of it.
        */

        if (comp == NULL && i != instrsCount - 1)
            LoadCPSR();
    }

    RegCache.Flush();
    SaveCPSR();

    LEA(32, RAX, MDisp(RCycles, ConstantCycles));

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
        // STRB
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        // LDR
        A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB, A_Comp_MemWB,
        // LDRB
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        // STRH
        NULL, NULL, NULL, NULL, 
        // LDRD
        NULL, NULL, NULL, NULL,
        // STRD
        NULL, NULL, NULL, NULL,
        // LDRH
        NULL, NULL, NULL, NULL, 
        // LDRSB
        NULL, NULL, NULL, NULL,
        // LDRSH
        NULL, NULL, NULL, NULL, 
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
        NULL, NULL, NULL, 
        // LDR pcrel
        NULL, 
        // LDR/STR reg offset
        T_Comp_MemReg, NULL, T_Comp_MemReg, NULL,
        // LDR/STR sign extended, half 
        NULL, NULL, NULL, NULL,
        // LDR/STR imm offset
        T_Comp_MemImm, T_Comp_MemImm, NULL, NULL, 
        // LDR/STR half imm offset
        NULL, NULL,
        // branch, etc.
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL
    };

    return Thumb ? T_Comp[kind] : A_Comp[kind];
}

void Compiler::Comp_AddCycles_C()
{
    s32 cycles = Num ?
        NDS::ARM7MemTimings[CurrentInstr.CodeCycles][Thumb ? 1 : 3]
        : ((R15 & 0x2) ? 0 : CurrentInstr.CodeCycles);

    if (CurrentInstr.Cond() < 0xE)
        ADD(32, R(RCycles), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

void Compiler::Comp_AddCycles_CI(u32 i)
{
    s32 cycles = (Num ?
        NDS::ARM7MemTimings[CurrentInstr.CodeCycles][Thumb ? 0 : 2]
        : ((R15 & 0x2) ? 0 : CurrentInstr.CodeCycles)) + i;
    
    if (CurrentInstr.Cond() < 0xE)
        ADD(32, R(RCycles), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

}