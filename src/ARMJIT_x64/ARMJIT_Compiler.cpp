#include "ARMJIT_Compiler.h"

#include "../ARMInterpreter.h"

#include <assert.h>

using namespace Gen;

namespace ARMJIT
{

const int RegCache::NativeRegAllocOrder[] = {(int)RBX, (int)RSI, (int)RDI, (int)R12, (int)R13};
const int RegCache::NativeRegsCount = 5;

Compiler::Compiler()
{
    AllocCodeSpace(1024 * 1024 * 4);
}

typedef void (Compiler::*CompileFunc)();
typedef void (*InterpretFunc)(ARM*);

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

CompiledBlock Compiler::CompileBlock(ARM* cpu, FetchedInstr instrs[], int instrsCount)
{
    if (IsAlmostFull())
    {
        ResetBlocks();
        ResetCodePtr();
    }

    CompiledBlock res = (CompiledBlock)GetWritableCodePtr();

    ConstantCycles = 0;
    Thumb = cpu->CPSR & 0x20;
    Num = cpu->Num;
    R15 = cpu->R[15];

    ABI_PushRegistersAndAdjustStack({ABI_ALL_CALLEE_SAVED}, 8, 0);

    MOV(64, R(RCPU), ImmPtr(cpu));
    XOR(32, R(RCycles), R(RCycles));

    LoadCPSR();

    for (int i = 0; i < instrsCount; i++)
    {
        R15 += Thumb ? 2 : 4;
        CurrentInstr = instrs[i];

        CompileFunc comp = NULL;

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

        if (Thumb)
        {
            if (comp == NULL)
            {
                MOV(64, R(ABI_PARAM1), R(RCPU));

                u32 icode = (CurrentInstr.Instr >> 6) & 0x3FF;
                ABI_CallFunction(ARMInterpreter::THUMBInstrTable[icode]);
            }
            else
            {
            }
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
                AddCycles_C();
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
                        BT(32, R(RCPSR), Imm8(28 + ((~(cond >> 1) & 1) << 1 | (cond >> 2 & 1) ^ (cond >> 1 & 1))));
                        
                        skipExecute = J_CC(cond & 1 ? CC_C : CC_NC);
                    }
                    
                }

                if (comp == NULL)
                {
                    MOV(64, R(ABI_PARAM1), R(RCPU));

                    u32 icode = ((CurrentInstr.Instr >> 4) & 0xF) | ((CurrentInstr.Instr >> 16) & 0xFF0);
                    ABI_CallFunction(ARMInterpreter::ARMInstrTable[icode]);
                }
                else
                {
                }

                FixupBranch skipFailed;
                if (CurrentInstr.Cond() < 0xE)
                {
                    skipFailed = J();
                    SetJumpTarget(skipExecute);

                    AddCycles_C();

                    SetJumpTarget(skipFailed);
                }
            }
        }

        /*
            we don't need to collect the interpreted cycles,
            since all functions only add to it, the dispatcher
            can take care of it.
        */

        if (comp == NULL && i != instrsCount - 1)
            LoadCPSR();
    }

    SaveCPSR();

    LEA(32, RAX, MDisp(RCycles, ConstantCycles));

    ABI_PopRegistersAndAdjustStack({ABI_ALL_CALLEE_SAVED}, 8, 0);
    RET();

    return res;
}

void Compiler::Compile(RegCache& regs, const FetchedInstr& instr)
{
    const CompileFunc A_Comp[ARMInstrInfo::ak_Count] =
    {
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    };

    const CompileFunc T_Comp[ARMInstrInfo::tk_Count] = {
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL
    };
}

void Compiler::AddCycles_C()
{
    s32 cycles = Num ?
        NDS::ARM7MemTimings[CurrentInstr.CodeCycles][Thumb ? 1 : 3]
        : ((R15 & 0x2) ? 0 : CurrentInstr.CodeCycles);

    if (CurrentInstr.Cond() < 0xE)
        ADD(32, R(RCycles), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

// may uses RSCRATCH for op2 and RSCRATCH2 for the carryValue
OpArg Compiler::Comp_ShiftRegImm(int op, int amount, Gen::X64Reg rm, bool S, bool& carryUsed)
{
    carryUsed = true;

    switch (op)
    {
        case 0: // LSL
            if (amount > 0)
            {
                MOV(32, R(RSCRATCH), R(rm));
                SHL(32, R(RSCRATCH), Imm8(amount));
                if (S)
                    SETcc(CC_C, R(RSCRATCH2));

                return R(RSCRATCH);
            }
            else
            {
                carryUsed = false;
                return R(rm);
            }
        case 1: // LSR
            if (amount > 0)
            {
                MOV(32, R(RSCRATCH), R(rm));
                SHR(32, R(RSCRATCH), Imm8(amount));
                if (S)
                    SETcc(CC_C, R(RSCRATCH2));
                return R(RSCRATCH);
            }
            else
            {
                if (S)
                {
                    MOV(32, R(RSCRATCH2), R(rm));
                    SHR(32, R(RSCRATCH2), Imm8(31));
                }
                return Imm32(0);
            }
        case 2: // ASR
            MOV(32, R(RSCRATCH), R(rm));
            SAR(32, R(RSCRATCH), Imm8(amount ? amount : 31));
            if (S)
            {
                if (amount == 0)
                {
                    MOV(32, R(RSCRATCH2), R(rm));
                    SHR(32, R(RSCRATCH2), Imm8(31));
                }
                else
                    SETcc(CC_C, R(RSCRATCH2));
            }
            return R(RSCRATCH);
        case 3: // ROR
            if (amount > 0)
            {
                MOV(32, R(RSCRATCH), R(rm));
                ROR_(32, R(RSCRATCH), Imm8(amount));
            }
            else
            {
                BT(32, R(RCPSR), Imm8(29));
                MOV(32, R(RSCRATCH), R(rm));
                RCR(32, R(RSCRATCH), Imm8(1));
            }
            if (S)
                SETcc(CC_C, R(RSCRATCH2));
            return R(RSCRATCH);
    }
}

void Compiler::A_Comp_ALU(const FetchedInstr& instr)
{
}

}