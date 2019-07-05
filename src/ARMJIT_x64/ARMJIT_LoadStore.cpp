#include "ARMJIT_Compiler.h"

#include "../GPU.h"
#include "../Wifi.h"

namespace NDS
{
extern u8* SWRAM_ARM9;
extern u32 SWRAM_ARM9Mask;
extern u8* SWRAM_ARM7;
extern u32 SWRAM_ARM7Mask;
extern u8 ARM7WRAM[];
extern u16 ARM7BIOSProt;
}

using namespace Gen;

namespace ARMJIT
{

template <typename T>
int squeezePointer(T* ptr)
{
    int truncated = (int)((u64)ptr);
    assert((T*)((u64)truncated) == ptr);
    return truncated;
}

/*
    According to DeSmuME and my own research, approx. 99% (seriously, that's an empirical number)
    of all memory load and store instructions always access addresses in the same region as
    during the their first execution.

    I tried multiple optimisations, which would benefit from this behaviour
    (having fast paths for the first region, …), though none of them yielded a measureable
    improvement.
*/

/*
    address - ABI_PARAM1 (a.k.a. ECX = RSCRATCH3 on Windows)
    store value - ABI_PARAM2 (a.k.a. RDX = RSCRATCH2 on Windows)
    code cycles - ABI_PARAM3
*/
void* Compiler::Gen_MemoryRoutine9(bool store, int size)
{
    u32 addressMask = ~(size == 32 ? 3 : (size == 16 ? 1 : 0));
    AlignCode4();
    void* res = GetWritableCodePtr();

    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    SUB(32, R(RSCRATCH), MDisp(RCPU, offsetof(ARMv5, DTCMBase)));
    CMP(32, R(RSCRATCH), MDisp(RCPU, offsetof(ARMv5, DTCMSize)));
    FixupBranch insideDTCM = J_CC(CC_B);

    CMP(32, R(ABI_PARAM1), MDisp(RCPU, offsetof(ARMv5, ITCMSize)));
    FixupBranch insideITCM = J_CC(CC_B);

    // cycle counting!
    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    SHR(32, R(RSCRATCH), Imm8(12));
    MOVZX(32, 8, RSCRATCH, MComplex(RCPU, RSCRATCH, SCALE_1, offsetof(ARMv5, MemTimings) + (size == 32 ? 2 : 0)));
    LEA(32, ABI_PARAM4, MComplex(RSCRATCH, ABI_PARAM3, SCALE_1, -6));
    CMP(32, R(ABI_PARAM3), R(RSCRATCH));
    CMOVcc(32, RSCRATCH, R(ABI_PARAM3), CC_G);
    CMP(32, R(ABI_PARAM4), R(RSCRATCH));
    CMOVcc(32, RSCRATCH, R(ABI_PARAM4), CC_G);
    ADD(32, R(RCycles), R(RSCRATCH));

    if (store)
    {
        if (size > 8)
            AND(32, R(ABI_PARAM1), Imm32(addressMask));
        switch (size)
        {
        case 32: JMP((u8*)NDS::ARM9Write32, true); break;
        case 16: JMP((u8*)NDS::ARM9Write16, true); break;
        case 8: JMP((u8*)NDS::ARM9Write8, true); break;
        }
    }
    else
    {
        if (size == 32)
        {
            ABI_PushRegistersAndAdjustStack({ABI_PARAM1}, 8);
            AND(32, R(ABI_PARAM1), Imm32(addressMask));
            // everything's already in the appropriate register
            ABI_CallFunction(NDS::ARM9Read32);
            ABI_PopRegistersAndAdjustStack({ECX}, 8);
            AND(32, R(ECX), Imm8(3));
            SHL(32, R(ECX), Imm8(3));
            ROR_(32, R(RSCRATCH), R(ECX));
            RET();
        }
        else if (size == 16)
        {
            AND(32, R(ABI_PARAM1), Imm32(addressMask));
            JMP((u8*)NDS::ARM9Read16, true);
        }
        else
            JMP((u8*)NDS::ARM9Read8, true);
    }

    SetJumpTarget(insideDTCM);
    ADD(32, R(RCycles), R(ABI_PARAM3));
    AND(32, R(RSCRATCH), Imm32(0x3FFF & addressMask));
    if (store)
        MOV(size, MComplex(RCPU, RSCRATCH, SCALE_1, offsetof(ARMv5, DTCM)), R(ABI_PARAM2));
    else
    {
        MOVZX(32, size, RSCRATCH, MComplex(RCPU, RSCRATCH, SCALE_1, offsetof(ARMv5, DTCM)));
        if (size == 32)
        {
            if (ABI_PARAM1 != ECX)
                MOV(32, R(ECX), R(ABI_PARAM1));
            AND(32, R(ECX), Imm8(3));
            SHL(32, R(ECX), Imm8(3));
            ROR_(32, R(RSCRATCH), R(ECX));
        }
    }
    RET();

    SetJumpTarget(insideITCM);
    ADD(32, R(RCycles), R(ABI_PARAM3));
    MOV(32, R(ABI_PARAM3), R(ABI_PARAM1)); // free up ECX
    AND(32, R(ABI_PARAM3), Imm32(0x7FFF & addressMask));
    if (store)
    {
        MOV(size, MComplex(RCPU, ABI_PARAM3, SCALE_1, offsetof(ARMv5, ITCM)), R(ABI_PARAM2));
        XOR(32, R(RSCRATCH), R(RSCRATCH));
        MOV(64, MScaled(ABI_PARAM3, SCALE_4, squeezePointer(cache.ARM9_ITCM)), R(RSCRATCH));
        if (size == 32)
            MOV(64, MScaled(ABI_PARAM3, SCALE_4, squeezePointer(cache.ARM9_ITCM) + 8), R(RSCRATCH));
    }
    else
    {
        MOVZX(32, size, RSCRATCH, MComplex(RCPU, ABI_PARAM3, SCALE_1, offsetof(ARMv5, ITCM)));
        if (size == 32)
        {
            if (ABI_PARAM1 != ECX)
                MOV(32, R(ECX), R(ABI_PARAM1));
            AND(32, R(ECX), Imm8(3));
            SHL(32, R(ECX), Imm8(3));
            ROR_(32, R(RSCRATCH), R(ECX));
        }
    }
    RET();

    static_assert(RSCRATCH == EAX);

    return res;
}

void* Compiler::Gen_MemoryRoutine7(bool store, bool codeMainRAM, int size)
{
    u32 addressMask = ~(size == 32 ? 3 : (size == 16 ? 1 : 0));
    AlignCode4();
    void* res = GetWritableCodePtr();

    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    SHR(32, R(RSCRATCH), Imm8(15));
    MOVZX(32, 8, ABI_PARAM4, MDisp(RSCRATCH, (size == 32 ? 2 : 0) + squeezePointer(NDS::ARM7MemTimings)));

    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    AND(32, R(RSCRATCH), Imm32(0xFF000000));
    CMP(32, R(RSCRATCH), Imm32(0x02000000));
    FixupBranch outsideMainRAM = J_CC(CC_NE);
    if (codeMainRAM)
    {
        LEA(32, RSCRATCH, MRegSum(ABI_PARAM4, ABI_PARAM3));
        ADD(32, R(RCycles), R(RSCRATCH));
    }
    else
    {
        if (!store)
            ADD(32, R(ABI_PARAM3), Imm8(1));
        LEA(32, RSCRATCH, MComplex(ABI_PARAM4, ABI_PARAM3, SCALE_1, -3));
        CMP(32, R(ABI_PARAM4), R(ABI_PARAM3));
        CMOVcc(32, ABI_PARAM3, R(ABI_PARAM4), CC_G);
        CMP(32, R(ABI_PARAM3), R(RSCRATCH));
        CMOVcc(32, RSCRATCH, R(ABI_PARAM3), CC_G);
        ADD(32, R(RCycles), R(RSCRATCH));
    }
    MOV(32, R(ABI_PARAM3), R(ABI_PARAM1));
    AND(32, R(ABI_PARAM3), Imm32((MAIN_RAM_SIZE - 1) & addressMask));
    if (store)
    {
        MOV(size, MDisp(ABI_PARAM3, squeezePointer(NDS::MainRAM)), R(ABI_PARAM2));
        XOR(32, R(RSCRATCH), R(RSCRATCH));
        MOV(64, MScaled(ABI_PARAM3, SCALE_4, squeezePointer(cache.MainRAM)), R(RSCRATCH));
        if (size == 32)
            MOV(64, MScaled(ABI_PARAM3, SCALE_4, squeezePointer(cache.MainRAM) + 8), R(RSCRATCH));
    }
    else
    {
        MOVZX(32, size, RSCRATCH, MDisp(ABI_PARAM3, squeezePointer(NDS::MainRAM)));
        if (size == 32)
        {
            if (ABI_PARAM1 != ECX)
                MOV(32, R(ECX), R(ABI_PARAM1));
            AND(32, R(ECX), Imm8(3));
            SHL(32, R(ECX), Imm8(3));
            ROR_(32, R(RSCRATCH), R(ECX));
        }
    }
    RET();

    SetJumpTarget(outsideMainRAM);
    if (codeMainRAM)
    {
        if (!store)
            ADD(32, R(ABI_PARAM4), Imm8(1));
        LEA(32, RSCRATCH, MComplex(ABI_PARAM4, ABI_PARAM3, SCALE_1, -3));
        CMP(32, R(ABI_PARAM4), R(ABI_PARAM3));
        CMOVcc(32, ABI_PARAM3, R(ABI_PARAM4), CC_G);
        CMP(32, R(ABI_PARAM3), R(RSCRATCH));
        CMOVcc(32, RSCRATCH, R(ABI_PARAM3), CC_G);
        ADD(32, R(RCycles), R(RSCRATCH));
    }
    else
    {
        LEA(32, RSCRATCH, MComplex(ABI_PARAM4, ABI_PARAM3, SCALE_1, store ? 0 : 1));
        ADD(32, R(RCycles), R(RSCRATCH));
    }
    if (store)
    {
        if (size > 8)
            AND(32, R(ABI_PARAM1), Imm32(addressMask));
        switch (size)
        {
        case 32: JMP((u8*)NDS::ARM7Write32, true); break;
        case 16: JMP((u8*)NDS::ARM7Write16, true); break;
        case 8: JMP((u8*)NDS::ARM7Write8, true); break;
        }
    }
    else
    {
        if (size == 32)
        {
            ABI_PushRegistersAndAdjustStack({ABI_PARAM1}, 8);
            AND(32, R(ABI_PARAM1), Imm32(addressMask));
            ABI_CallFunction(NDS::ARM7Read32);
            ABI_PopRegistersAndAdjustStack({ECX}, 8);
            AND(32, R(ECX), Imm8(3));
            SHL(32, R(ECX), Imm8(3));
            ROR_(32, R(RSCRATCH), R(ECX));
            RET();
        }
        else if (size == 16)
        {
            AND(32, R(ABI_PARAM1), Imm32(addressMask));
            JMP((u8*)NDS::ARM7Read16, true);
        }
        else
            JMP((u8*)NDS::ARM7Read8, true);
    }

    return res;
}

void Compiler::Comp_MemAccess(Gen::OpArg rd, bool signExtend, bool store, int size)
{
    if (store)
        MOV(32, R(ABI_PARAM2), rd);
    u32 cycles = Num
        ? NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
        : (R15 & 0x2 ? 0 : CurInstr.CodeCycles);
    MOV(32, R(ABI_PARAM3), Imm32(cycles));
    CALL(Num == 0
        ? MemoryFuncs9[size >> 4][store]
        : MemoryFuncs7[size >> 4][store][CodeRegion == 0x02]);

    if (!store)
    {
        if (signExtend)
            MOVSX(32, size, rd.GetSimpleReg(), R(RSCRATCH));
        else
            MOVZX(32, size, rd.GetSimpleReg(), R(RSCRATCH));
    }
}

OpArg Compiler::A_Comp_GetMemWBOffset()
{
    if (!(CurInstr.Instr & (1 << 25)))
    {
        u32 imm = CurInstr.Instr & 0xFFF;
        return Imm32(imm);
    }
    else
    {
        int op = (CurInstr.Instr >> 5) & 0x3;
        int amount = (CurInstr.Instr >> 7) & 0x1F;
        OpArg rm = MapReg(CurInstr.A_Reg(0));
        bool carryUsed;

        return Comp_RegShiftImm(op, amount, rm, false, carryUsed);
    }
}

void Compiler::A_Comp_MemWB()
{
    OpArg rn = MapReg(CurInstr.A_Reg(16));
    OpArg rd = MapReg(CurInstr.A_Reg(12));
    bool load = CurInstr.Instr & (1 << 20);
    bool byte = CurInstr.Instr & (1 << 22);
    int size = byte ? 8 : 32;

    if (CurInstr.Instr & (1 << 24))
    {
        OpArg offset = A_Comp_GetMemWBOffset();
        if (CurInstr.Instr & (1 << 23))
            MOV_sum(32, ABI_PARAM1, rn, offset);
        else
        {
            MOV(32, R(ABI_PARAM1), rn);
            SUB(32, R(ABI_PARAM1), offset);
        }

        if (CurInstr.Instr & (1 << 21))
            MOV(32, rn, R(ABI_PARAM1));
    }
    else
        MOV(32, R(ABI_PARAM1), rn);

    if (!(CurInstr.Instr & (1 << 24)))
    {
        OpArg offset = A_Comp_GetMemWBOffset();

        if (CurInstr.Instr & (1 << 23))
            ADD(32, rn, offset);
        else
            SUB(32, rn, offset);
    }

    Comp_MemAccess(rd, false, !load, byte ? 8 : 32);
    if (load && CurInstr.A_Reg(12) == 15)
    {
        if (byte)
            printf("!!! LDRB PC %08X\n", R15);
        else
        {
            if (Num == 1)
                AND(32, rd, Imm8(0xFE)); // immediate is sign extended
            Comp_JumpTo(rd.GetSimpleReg());
        }
    }
}

void Compiler::A_Comp_MemHalf()
{
    OpArg rn = MapReg(CurInstr.A_Reg(16));
    OpArg rd = MapReg(CurInstr.A_Reg(12));

    OpArg offset = CurInstr.Instr & (1 << 22)
        ? Imm32(CurInstr.Instr & 0xF | ((CurInstr.Instr >> 4) & 0xF0))
        : MapReg(CurInstr.A_Reg(0));

    if (CurInstr.Instr & (1 << 24))
    {
        if (CurInstr.Instr & (1 << 23))
            MOV_sum(32, ABI_PARAM1, rn, offset);
        else
        {
            MOV(32, R(ABI_PARAM1), rn);
            SUB(32, R(ABI_PARAM1), offset);
        }
        
        if (CurInstr.Instr & (1 << 21))
            MOV(32, rn, R(ABI_PARAM1));
    }
    else
        MOV(32, R(ABI_PARAM1), rn);

    int op = (CurInstr.Instr >> 5) & 0x3;
    bool load = CurInstr.Instr & (1 << 20);

    bool signExtend = false;
    int size;
    if (!load && op == 1)
        size = 16;
    else if (load)
    {
        size = op == 2 ? 8 : 16;
        signExtend = op > 1;
    }

    if (!(CurInstr.Instr & (1 << 24)))
    {
        if (CurInstr.Instr & (1 << 23))
            ADD(32, rn, offset);
        else
            SUB(32, rn, offset);
    }

    Comp_MemAccess(rd, signExtend, !load, size);

    if (load && CurInstr.A_Reg(12) == 15)
        printf("!!! MemHalf op PC %08X\n", R15);;
}

void Compiler::T_Comp_MemReg()
{
    OpArg rd = MapReg(CurInstr.T_Reg(0));
    OpArg rb = MapReg(CurInstr.T_Reg(3));
    OpArg ro = MapReg(CurInstr.T_Reg(6));

    int op = (CurInstr.Instr >> 10) & 0x3;
    bool load = op & 0x2;
    bool byte = op & 0x1;

    MOV_sum(32, ABI_PARAM1, rb, ro);

    Comp_MemAccess(rd, false, !load, byte ? 8 : 32);
}

void Compiler::T_Comp_MemImm()
{
    OpArg rd = MapReg(CurInstr.T_Reg(0));
    OpArg rb = MapReg(CurInstr.T_Reg(3));

    int op = (CurInstr.Instr >> 11) & 0x3;
    bool load = op & 0x1;
    bool byte = op & 0x2;
    u32 offset = ((CurInstr.Instr >> 6) & 0x1F) * (byte ? 1 : 4);

    LEA(32, ABI_PARAM1, MDisp(rb.GetSimpleReg(), offset));

    Comp_MemAccess(rd, false, !load, byte ? 8 : 32);
}

void Compiler::T_Comp_MemRegHalf()
{
    OpArg rd = MapReg(CurInstr.T_Reg(0));
    OpArg rb = MapReg(CurInstr.T_Reg(3));
    OpArg ro = MapReg(CurInstr.T_Reg(6));

    int op = (CurInstr.Instr >> 10) & 0x3;
    bool load = op != 0;
    int size = op != 1 ? 16 : 8;
    bool signExtend = op & 1;

    MOV_sum(32, ABI_PARAM1, rb, ro);

    Comp_MemAccess(rd, signExtend, !load, size);
}

void Compiler::T_Comp_MemImmHalf()
{
    OpArg rd = MapReg(CurInstr.T_Reg(0));
    OpArg rb = MapReg(CurInstr.T_Reg(3));

    u32 offset = (CurInstr.Instr >> 5) & 0x3E;
    bool load = CurInstr.Instr & (1 << 11);

    LEA(32, ABI_PARAM1, MDisp(rb.GetSimpleReg(), offset));

    Comp_MemAccess(rd, false, !load, 16);
}

}