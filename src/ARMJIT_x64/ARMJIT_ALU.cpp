#include "ARMJIT_Compiler.h"

using namespace Gen;

namespace ARMJIT
{

// uses RSCRATCH3
void Compiler::Comp_ArithTriOp(void (Compiler::*op)(int, const OpArg&, const OpArg&),
    OpArg rd, OpArg rn, OpArg op2, bool carryUsed, int opFlags)
{
    if (opFlags & opSyncCarry)
    {
        BT(32, R(RCPSR), Imm8(29));
        if (opFlags & opInvertCarry)
            CMC();
    }

    if (rd == rn && !(opFlags & opInvertOp2))
        (this->*op)(32, rd, op2);
    else if (opFlags & opSymmetric && op2 == R(RSCRATCH))
    {
        if (opFlags & opInvertOp2)
            NOT(32, op2);
        (this->*op)(32, op2, rn);
        MOV(32, rd, op2);
    }
    else
    {
        if (opFlags & opInvertOp2)
        {
            if (op2 != R(RSCRATCH))
            {
                MOV(32, R(RSCRATCH), op2);
                op2 = R(RSCRATCH);
            }
            NOT(32, op2);
        }
        MOV(32, R(RSCRATCH3), rn);
        (this->*op)(32, R(RSCRATCH3), op2);
        MOV(32, rd, R(RSCRATCH3));
    }

    if (opFlags & opSetsFlags)
        Comp_RetriveFlags(opFlags & opInvertCarry, opFlags & opRetriveCV, carryUsed);
}

void Compiler::Comp_ArithTriOpReverse(void (Compiler::*op)(int, const Gen::OpArg&, const Gen::OpArg&),
        Gen::OpArg rd, Gen::OpArg rn, Gen::OpArg op2, bool carryUsed, int opFlags)
{
    if (opFlags & opSyncCarry)
    {
        BT(32, R(RCPSR), Imm8(29));
        if (opFlags & opInvertCarry)
            CMC();
    }

    if (op2 != R(RSCRATCH))
    {
        MOV(32, R(RSCRATCH), op2);
        op2 = R(RSCRATCH);
    }
    (this->*op)(32, op2, rn);
    MOV(32, rd, op2);

    if (opFlags & opSetsFlags)
        Comp_RetriveFlags(opFlags & opInvertCarry, opFlags & opRetriveCV, carryUsed);
}

void Compiler::Comp_CmpOp(int op, Gen::OpArg rn, Gen::OpArg op2, bool carryUsed)
{
    switch (op)
    {
        case 0: // TST
            if (rn.IsImm())
            {
                MOV(32, R(RSCRATCH3), rn);
                rn = R(RSCRATCH3);
            }
            TEST(32, rn, op2);
        break;
        case 1: // TEQ
            MOV(32, R(RSCRATCH3), rn);
            XOR(32, R(RSCRATCH3), op2);
        break;
        case 2: // CMP
            if (rn.IsImm())
            {
                MOV(32, R(RSCRATCH3), rn);
                rn = R(RSCRATCH3);
            }
            CMP(32, rn, op2);
        break;
        case 3: // CMN
            MOV(32, R(RSCRATCH3), rn);
            ADD(32, R(RSCRATCH3), op2);
        break;
    }

    Comp_RetriveFlags(op == 2, op >= 2, carryUsed);
}

// also calculates cycles
OpArg Compiler::A_Comp_GetALUOp2(bool S, bool& carryUsed)
{
    if (CurrentInstr.Instr & (1 << 25))
    {
        Comp_AddCycles_C();
        carryUsed = false;
        return Imm32(ROR(CurrentInstr.Instr & 0xFF, (CurrentInstr.Instr >> 7) & 0x1E));
    }
    else
    {
        int op = (CurrentInstr.Instr >> 5) & 0x3;
        if (CurrentInstr.Instr & (1 << 4))
        {
            Comp_AddCycles_CI(1);
            OpArg rm = MapReg(CurrentInstr.A_Reg(0));
            if (rm.IsImm() && CurrentInstr.A_Reg(0) == 15)
                rm = Imm32(rm.Imm32() + 4);
            return Comp_RegShiftReg(op, MapReg(CurrentInstr.A_Reg(8)), rm, S, carryUsed);
        }
        else
        {
            Comp_AddCycles_C();
            return Comp_RegShiftImm(op, (CurrentInstr.Instr >> 7) & 0x1F,
                    MapReg(CurrentInstr.A_Reg(0)), S, carryUsed);
        }
    }
}

void Compiler::A_Comp_CmpOp()
{
    u32 op = (CurrentInstr.Instr >> 21) & 0xF;

    bool carryUsed;
    OpArg rn = MapReg(CurrentInstr.A_Reg(16));
    OpArg op2 = A_Comp_GetALUOp2((1 << op) & 0xF303, carryUsed);

    Comp_CmpOp(op - 0x8, rn, op2, carryUsed);
}

void Compiler::A_Comp_Arith()
{
    bool S = CurrentInstr.Instr & (1 << 20);
    u32 op = (CurrentInstr.Instr >> 21) & 0xF;

    bool carryUsed;
    OpArg rn = MapReg(CurrentInstr.A_Reg(16));
    OpArg rd = MapReg(CurrentInstr.A_Reg(12));
    OpArg op2 = A_Comp_GetALUOp2(S && (1 << op) & 0xF303, carryUsed);

    u32 sFlag = S ? opSetsFlags : 0;
    switch (op)
    {
    case 0x0: // AND
        Comp_ArithTriOp(AND, rd, rn, op2, carryUsed, opSymmetric|sFlag);
        return;
    case 0x1: // EOR
        Comp_ArithTriOp(XOR, rd, rn, op2, carryUsed, opSymmetric|sFlag);
        return;
    case 0x2: // SUB
        Comp_ArithTriOp(SUB, rd, rn, op2, carryUsed, sFlag|opRetriveCV|opInvertCarry);
        return;
    case 0x3: // RSB
        if (op2.IsZero())
        {
            if (rd != rn)
                MOV(32, rd, rn);
            NEG(32, rd);
            if (S)
                Comp_RetriveFlags(true, true, false);
        }
        else
            Comp_ArithTriOpReverse(SUB, rd, rn, op2, carryUsed, sFlag|opRetriveCV|opInvertCarry);
        return;
    case 0x4: // ADD
        Comp_ArithTriOp(ADD, rd, rn, op2, carryUsed, opSymmetric|sFlag|opRetriveCV);
        return;
    case 0x5: // ADC
        Comp_ArithTriOp(ADC, rd, rn, op2, carryUsed, opSymmetric|sFlag|opRetriveCV|opSyncCarry);
        return;
    case 0x6: // SBC
        Comp_ArithTriOp(SBB, rd, rn, op2, carryUsed, opSymmetric|sFlag|opRetriveCV|opSyncCarry|opInvertCarry);
        return;
    case 0x7: // RSC
        Comp_ArithTriOpReverse(SBB, rd, rn, op2, carryUsed, sFlag|opRetriveCV|opInvertCarry|opSyncCarry);
        return;
    case 0xC: // ORR
        Comp_ArithTriOp(OR, rd, rn, op2, carryUsed, opSymmetric|sFlag);
        return;
    case 0xE: // BIC
        Comp_ArithTriOp(AND, rd, rn, op2, carryUsed, sFlag|opSymmetric|opInvertOp2);
        return;
    default:
        assert("unimplemented");
    }
}

void Compiler::A_Comp_MovOp()
{
    bool carryUsed;
    bool S = CurrentInstr.Instr & (1 << 20);
    OpArg op2 = A_Comp_GetALUOp2(S, carryUsed);
    OpArg rd = MapReg(CurrentInstr.A_Reg(12));

    if (rd != op2)
        MOV(32, rd, op2);

    if (((CurrentInstr.Instr >> 21) & 0xF) == 0xF)
        NOT(32, rd);

    if (S)
    {
        TEST(32, rd, rd);
        Comp_RetriveFlags(false, false, carryUsed);
    }
}

void Compiler::Comp_RetriveFlags(bool sign, bool retriveCV, bool carryUsed)
{
    CPSRDirty = true;

    bool carryOnly = !retriveCV && carryUsed;
    if (retriveCV)
    {
        SETcc(CC_O, R(RSCRATCH));
        SETcc(sign ? CC_NC : CC_C, R(RSCRATCH3));
        LEA(32, RSCRATCH2, MComplex(RSCRATCH, RSCRATCH3, SCALE_2, 0));
    }

    if (carryUsed == 983298)
        printf("etwas ist faul im lande daenemark %x\n", CurrentInstr.Instr);

    SETcc(CC_S, R(RSCRATCH));
    SETcc(CC_Z, R(RSCRATCH3));
    LEA(32, RSCRATCH, MComplex(RSCRATCH3, RSCRATCH, SCALE_2, 0));
    int shiftAmount = 30;
    if (retriveCV || carryUsed)
    {
        LEA(32, RSCRATCH, MComplex(RSCRATCH2, RSCRATCH, carryOnly ? SCALE_2 : SCALE_4, 0));
        shiftAmount = carryOnly ? 29 : 28;
    }
    SHL(32, R(RSCRATCH), Imm8(shiftAmount));

    AND(32, R(RCPSR), Imm32(0x3FFFFFFF & ~(carryUsed << 29) & ~((retriveCV ? 3 : 0) << 28)));
    OR(32, R(RCPSR), R(RSCRATCH));
}

// always uses RSCRATCH, RSCRATCH2 only if S == true
OpArg Compiler::Comp_RegShiftReg(int op, Gen::OpArg rs, Gen::OpArg rm, bool S, bool& carryUsed)
{
    carryUsed = S;

    if (S)
    {
        XOR(32, R(RSCRATCH2), R(RSCRATCH2));
        BT(32, R(RCPSR), Imm8(29));
        SETcc(CC_C, R(RSCRATCH2));
    }

    MOV(32, R(RSCRATCH), rm);
    static_assert(RSCRATCH3 == ECX);
    MOV(32, R(ECX), rs);
    AND(32, R(ECX), Imm32(0xFF));

    FixupBranch zero = J_CC(CC_Z);
    if (op < 3)
    {
        void (Compiler::*shiftOp)(int, const OpArg&, const OpArg&) = NULL;
        if (op == 0)
            shiftOp = SHL;
        else if (op == 1)
            shiftOp = SHR;
        else if (op == 2)
            shiftOp = SAR;

        CMP(32, R(ECX), Imm8(32));
        FixupBranch lt32 = J_CC(CC_L);
        FixupBranch done1;
        if (op < 2)
        {
            FixupBranch eq32 = J_CC(CC_E);
            XOR(32, R(RSCRATCH), R(RSCRATCH));
            if (S)
                XOR(32, R(RSCRATCH2), R(RSCRATCH2));
            done1 = J();
            SetJumpTarget(eq32);
        }
        (this->*shiftOp)(32, R(RSCRATCH), Imm8(31));
        (this->*shiftOp)(32, R(RSCRATCH), Imm8(1));
        if (S)
            SETcc(CC_C, R(RSCRATCH2));

        FixupBranch done2 = J();

        SetJumpTarget(lt32);
        (this->*shiftOp)(32, R(RSCRATCH), R(ECX));
        if (S)
            SETcc(CC_C, R(RSCRATCH2));

        if (op < 2)
            SetJumpTarget(done1);
        SetJumpTarget(done2);

    }
    else if (op == 3)
    {
        if (S)
            BT(32, R(RSCRATCH), Imm8(31));
        ROR_(32, R(RSCRATCH), R(ECX));
        if (S)
            SETcc(CC_C, R(RSCRATCH2));
    }
    SetJumpTarget(zero);

    return R(RSCRATCH);
}

// may uses RSCRATCH for op2 and RSCRATCH2 for the carryValue
OpArg Compiler::Comp_RegShiftImm(int op, int amount, OpArg rm, bool S, bool& carryUsed)
{
    carryUsed = true;

    switch (op)
    {
        case 0: // LSL
            if (amount > 0)
            {
                MOV(32, R(RSCRATCH), rm);
                SHL(32, R(RSCRATCH), Imm8(amount));
                if (S)
                    SETcc(CC_C, R(RSCRATCH2));

                return R(RSCRATCH);
            }
            else
            {
                carryUsed = false;
                return rm;
            }
        case 1: // LSR
            if (amount > 0)
            {
                MOV(32, R(RSCRATCH), rm);
                SHR(32, R(RSCRATCH), Imm8(amount));
                if (S)
                    SETcc(CC_C, R(RSCRATCH2));
                return R(RSCRATCH);
            }
            else
            {
                if (S)
                {
                    MOV(32, R(RSCRATCH2), rm);
                    SHR(32, R(RSCRATCH2), Imm8(31));
                }
                return Imm32(0);
            }
        case 2: // ASR
            MOV(32, R(RSCRATCH), rm);
            SAR(32, R(RSCRATCH), Imm8(amount ? amount : 31));
            if (S)
            {
                if (amount == 0)
                    BT(32, rm, Imm8(31));
                SETcc(CC_C, R(RSCRATCH2));
            }
            return R(RSCRATCH);
        case 3: // ROR
            MOV(32, R(RSCRATCH), rm);
            if (amount > 0)
                ROR_(32, R(RSCRATCH), Imm8(amount));
            else
            {
                BT(32, R(RCPSR), Imm8(29));
                RCR(32, R(RSCRATCH), Imm8(1));
            }
            if (S)
                SETcc(CC_C, R(RSCRATCH2));
            return R(RSCRATCH);
    }

    assert(false);
}

void Compiler::T_Comp_ShiftImm()
{
    OpArg rd = MapReg(CurrentInstr.T_Reg(0));
    OpArg rs = MapReg(CurrentInstr.T_Reg(3));

    int op = (CurrentInstr.Instr >> 11) & 0x3;
    int amount = (CurrentInstr.Instr >> 6) & 0x1F;

    Comp_AddCycles_C();

    bool carryUsed;
    OpArg shifted = Comp_RegShiftImm(op, amount, rs, true, carryUsed);

    if (shifted != rd)
        MOV(32, rd, shifted);

    TEST(32, rd, rd);
    Comp_RetriveFlags(false, false, carryUsed);
}

void Compiler::T_Comp_AddSub_()
{
    OpArg rd = MapReg(CurrentInstr.T_Reg(0));
    OpArg rs = MapReg(CurrentInstr.T_Reg(3));

    int op = (CurrentInstr.Instr >> 9) & 0x3;

    OpArg rn = op >= 2 ? Imm32((CurrentInstr.Instr >> 6) & 0x7) : MapReg(CurrentInstr.T_Reg(6));
    
    Comp_AddCycles_C();

    if (op & 1)
        Comp_ArithTriOp(SUB, rd, rs, rn, false, opSetsFlags|opInvertCarry|opRetriveCV);
    else
        Comp_ArithTriOp(ADD, rd, rs, rn, false, opSetsFlags|opSymmetric|opRetriveCV);
}

void Compiler::T_Comp_ALU_Imm8()
{
    OpArg rd = MapReg(CurrentInstr.T_Reg(8));

    u32 op = (CurrentInstr.Instr >> 11) & 0x3;
    OpArg imm = Imm32(CurrentInstr.Instr & 0xFF);

    Comp_AddCycles_C();

    switch (op)
    {
        case 0x0:
            MOV(32, rd, imm);
            TEST(32, rd, rd);
            Comp_RetriveFlags(false, false, false);
            return;
        case 0x1:
            Comp_CmpOp(2, rd, imm, false);
            return;
        case 0x2:
            Comp_ArithTriOp(ADD, rd, rd, imm, false, opSetsFlags|opSymmetric|opRetriveCV);
            return;
        case 0x3:
            Comp_ArithTriOp(SUB, rd, rd, imm, false, opSetsFlags|opInvertCarry|opRetriveCV);
            return;
    }
}

void Compiler::T_Comp_ALU()
{
    OpArg rd = MapReg(CurrentInstr.T_Reg(0));
    OpArg rs = MapReg(CurrentInstr.T_Reg(3));

    u32 op = (CurrentInstr.Instr >> 6) & 0xF;

    Comp_AddCycles_C();

    switch (op)
    {
    case 0x0: // AND
        Comp_ArithTriOp(AND, rd, rd, rs, false, opSetsFlags|opSymmetric);
        return;
    case 0x1: // EOR
        Comp_ArithTriOp(XOR, rd, rd, rs, false, opSetsFlags|opSymmetric);
        return;
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x7:
        {
            int shiftOp = op == 7 ? 3 : op - 0x2;
            bool carryUsed;
            OpArg shifted = Comp_RegShiftReg(shiftOp, rs, rd, true, carryUsed);
            TEST(32, shifted, shifted);
            MOV(32, rd, shifted);
            Comp_RetriveFlags(false, false, true);
        }
        return;
    case 0x5: // ADC
        Comp_ArithTriOp(ADC, rd, rd, rs, false, opSetsFlags|opSymmetric|opSyncCarry|opRetriveCV);
        return;
    case 0x6: // SBC
        Comp_ArithTriOp(SBB, rd, rd, rs, false, opSetsFlags|opSyncCarry|opInvertCarry|opRetriveCV);
        return;
    case 0x8: // TST
        Comp_CmpOp(0, rd, rs, false);
        return;
    case 0x9: // NEG
        if (rd != rs)
            MOV(32, rd, rs);
        NEG(32, rd);
        Comp_RetriveFlags(true, true, false);
        return;
    case 0xA: // CMP
        Comp_CmpOp(2, rd, rs, false);
        return;
    case 0xB: // CMN
        Comp_CmpOp(3, rd, rs, false);
        return;
    case 0xC: // ORR
        Comp_ArithTriOp(OR, rd, rd, rs, false, opSetsFlags|opSymmetric);
        return;
    case 0xE: // BIC
        Comp_ArithTriOp(AND, rd, rd, rs, false, opSetsFlags|opSymmetric|opInvertOp2);
        return;
    case 0xF: // MVN
        if (rd != rs)
            MOV(32, rd, rs);
        NOT(32, rd);
        Comp_RetriveFlags(false, false, false);
        return;
    default:
        break;
    }
}

void Compiler::T_Comp_ALU_HiReg()
{
    OpArg rd = MapReg(((CurrentInstr.Instr & 0x7) | ((CurrentInstr.Instr >> 4) & 0x8)));
    OpArg rs = MapReg((CurrentInstr.Instr >> 3) & 0xF);

    u32 op = (CurrentInstr.Instr >> 8) & 0x3;

    Comp_AddCycles_C();

    switch (op)
    {
        case 0x0: // ADD
            Comp_ArithTriOp(ADD, rd, rd, rs, false, opSymmetric|opRetriveCV);
            return;
        case 0x1: // CMP
            Comp_CmpOp(2, rd, rs, false);
            return;
        case 0x2: // MOV
            if (rd != rs)
                MOV(32, rd, rs);
            TEST(32, rd, rd);
            Comp_RetriveFlags(false, false, false);
            return;
    }
}

}