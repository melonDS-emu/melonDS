#include "ARMJIT_Compiler.h"


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

#define CALC_CYCLES_9(numC, numD, scratch) \
    LEA(32, scratch, MComplex(numD, numC, SCALE_1, -6)); \
    CMP(32, R(numC), R(numD)); \
    CMOVcc(32, numD, R(numC), CC_G); \
    CMP(32, R(numD), R(scratch)); \
    CMOVcc(32, scratch, R(numD), CC_G); \
    ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(scratch));
#define CALC_CYCLES_7_DATA_MAIN_RAM(numC, numD, scratch) \
    if (codeMainRAM) \
    { \
        LEA(32, scratch, MRegSum(numD, numC)); \
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(scratch)); \
    } \
    else \
    { \
        if (!store) \
            ADD(32, R(numC), Imm8(1)); \
        LEA(32, scratch, MComplex(numD, numC, SCALE_1, -3)); \
        CMP(32, R(numD), R(numC)); \
        CMOVcc(32, numC, R(numD), CC_G); \
        CMP(32, R(numC), R(scratch)); \
        CMOVcc(32, scratch, R(numC), CC_G); \
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(scratch)); \
    }
#define CALC_CYCLES_7_DATA_NON_MAIN_RAM(numC, numD, scratch) \
    if (codeMainRAM) \
    { \
        if (!store) \
            ADD(32, R(numD), Imm8(1)); \
        LEA(32, scratch, MComplex(numD, numC, SCALE_1, -3)); \
        CMP(32, R(numD), R(numC)); \
        CMOVcc(32, numC, R(numD), CC_G); \
        CMP(32, R(numC), R(scratch)); \
        CMOVcc(32, scratch, R(numC), CC_G); \
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(scratch)); \
    } \
    else \
    { \
        LEA(32, scratch, MComplex(numD, numC, SCALE_1, store ? 0 : 1)); \
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(scratch)); \
    }

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
    MOV(32, R(ABI_PARAM4), R(ABI_PARAM1));
    SHR(32, R(ABI_PARAM4), Imm8(12));
    MOVZX(32, 8, ABI_PARAM4, MComplex(RCPU, ABI_PARAM4, SCALE_4, offsetof(ARMv5, MemTimings) + (size == 32 ? 2 : 1)));
    CALC_CYCLES_9(ABI_PARAM3, ABI_PARAM4, RSCRATCH)

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
    ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(ABI_PARAM3));
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
    ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(ABI_PARAM3));
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

    static_assert(RSCRATCH == EAX, "Someone changed RSCRATCH!");

    return res;
}

void* Compiler::Gen_MemoryRoutine7(bool store, bool codeMainRAM, int size)
{
    u32 addressMask = ~(size == 32 ? 3 : (size == 16 ? 1 : 0));
    AlignCode4();
    void* res = GetWritableCodePtr();

    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    SHR(32, R(RSCRATCH), Imm8(15));
    MOVZX(32, 8, ABI_PARAM4, MScaled(RSCRATCH, SCALE_4, (size == 32 ? 2 : 0) + squeezePointer(NDS::ARM7MemTimings)));

    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    AND(32, R(RSCRATCH), Imm32(0xFF000000));
    CMP(32, R(RSCRATCH), Imm32(0x02000000));
    FixupBranch outsideMainRAM = J_CC(CC_NE);
    CALC_CYCLES_7_DATA_MAIN_RAM(ABI_PARAM3, ABI_PARAM4, RSCRATCH)
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
    CALC_CYCLES_7_DATA_NON_MAIN_RAM(ABI_PARAM3, ABI_PARAM4, RSCRATCH)
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

#define MEMORY_SEQ_WHILE_COND \
        if (!store) \
            MOV(32, currentElement, R(EAX));\
        if (!preinc) \
            ADD(32, R(ABI_PARAM1), Imm8(4)); \
        \
        SUB(32, R(ABI_PARAM3), Imm8(1)); \
        J_CC(CC_NZ, repeat);

/*
    ABI_PARAM1 address
    ABI_PARAM2 address where registers are stored
    ABI_PARAM3 how many values to read/write
    ABI_PARAM4 code cycles

    Dolphin x64CodeEmitter is my favourite assembler
 */
void* Compiler::Gen_MemoryRoutineSeq9(bool store, bool preinc)
{
    const u8* zero = GetCodePtr();
    ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(ABI_PARAM4));
    RET();

    void* res = (void*)GetWritableCodePtr();

    TEST(32, R(ABI_PARAM3), R(ABI_PARAM3));
    J_CC(CC_Z, zero);

    PUSH(ABI_PARAM3);
    PUSH(ABI_PARAM4); // we need you later

    const u8* repeat = GetCodePtr();

    if (preinc)
        ADD(32, R(ABI_PARAM1), Imm8(4));

    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    SUB(32, R(RSCRATCH), MDisp(RCPU, offsetof(ARMv5, DTCMBase)));
    CMP(32, R(RSCRATCH), MDisp(RCPU, offsetof(ARMv5, DTCMSize)));
    FixupBranch insideDTCM = J_CC(CC_B);

    CMP(32, R(ABI_PARAM1), MDisp(RCPU, offsetof(ARMv5, ITCMSize)));
    FixupBranch insideITCM = J_CC(CC_B);

    OpArg currentElement = MComplex(ABI_PARAM2, ABI_PARAM3, SCALE_8, -8); // wasting stack space like a gangster

    ABI_PushRegistersAndAdjustStack({ABI_PARAM1, ABI_PARAM2, ABI_PARAM3}, 8);
    AND(32, R(ABI_PARAM1), Imm8(~3));
    if (store)
    {
        MOV(32, R(ABI_PARAM2), currentElement);
        CALL((void*)NDS::ARM9Write32);
    }
    else
        CALL((void*)NDS::ARM9Read32);
    ABI_PopRegistersAndAdjustStack({ABI_PARAM1, ABI_PARAM2, ABI_PARAM3}, 8);

    MEMORY_SEQ_WHILE_COND
    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    SHR(32, R(RSCRATCH), Imm8(12));
    MOVZX(32, 8, ABI_PARAM2, MComplex(RCPU, RSCRATCH, SCALE_4, 2 + offsetof(ARMv5, MemTimings)));
    MOVZX(32, 8, RSCRATCH, MComplex(RCPU, RSCRATCH, SCALE_4, 3 + offsetof(ARMv5, MemTimings)));

    FixupBranch finishIt1 = J();

    SetJumpTarget(insideDTCM);
    AND(32, R(RSCRATCH), Imm32(0x3FFF & ~3));
    if (store)
    {
        MOV(32, R(ABI_PARAM4), currentElement);
        MOV(32, MComplex(RCPU, RSCRATCH, SCALE_1, offsetof(ARMv5, DTCM)), R(ABI_PARAM4));
    }
    else
        MOV(32, R(RSCRATCH), MComplex(RCPU, RSCRATCH, SCALE_1, offsetof(ARMv5, DTCM)));

    MEMORY_SEQ_WHILE_COND
    MOV(32, R(RSCRATCH), Imm32(1)); // sequential access time
    MOV(32, R(ABI_PARAM2), Imm32(1)); // non sequential
    FixupBranch finishIt2 = J();

    SetJumpTarget(insideITCM);
    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    AND(32, R(RSCRATCH), Imm32(0x7FFF & ~3));
    if (store)
    {
        MOV(32, R(ABI_PARAM4), currentElement);
        MOV(32, MComplex(RCPU, RSCRATCH, SCALE_1, offsetof(ARMv5, ITCM)), R(ABI_PARAM4));
        XOR(32, R(ABI_PARAM4), R(ABI_PARAM4));
        MOV(64, MScaled(RSCRATCH, SCALE_4, squeezePointer(cache.ARM9_ITCM)), R(ABI_PARAM4));
        MOV(64, MScaled(RSCRATCH, SCALE_4, squeezePointer(cache.ARM9_ITCM) + 8), R(ABI_PARAM4));
    }
    else
        MOV(32, R(RSCRATCH), MComplex(RCPU, RSCRATCH, SCALE_1, offsetof(ARMv5, ITCM)));

    MEMORY_SEQ_WHILE_COND
    MOV(32, R(RSCRATCH), Imm32(1));
    MOV(32, R(ABI_PARAM2), Imm32(1));

    SetJumpTarget(finishIt1);
    SetJumpTarget(finishIt2);

    POP(ABI_PARAM4);
    POP(ABI_PARAM3);

    CMP(32, R(ABI_PARAM3), Imm8(1));
    FixupBranch skipSequential = J_CC(CC_E);
    SUB(32, R(ABI_PARAM3), Imm8(1));
    IMUL(32, RSCRATCH, R(ABI_PARAM3));
    ADD(32, R(ABI_PARAM2), R(RSCRATCH));
    SetJumpTarget(skipSequential);

    CALC_CYCLES_9(ABI_PARAM4, ABI_PARAM2, RSCRATCH)
    RET();

    return res;
}

void* Compiler::Gen_MemoryRoutineSeq7(bool store, bool preinc, bool codeMainRAM)
{
    const u8* zero = GetCodePtr();
    ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(ABI_PARAM4));
    RET();

    void* res = (void*)GetWritableCodePtr();

    TEST(32, R(ABI_PARAM3), R(ABI_PARAM3));
    J_CC(CC_Z, zero);

    PUSH(ABI_PARAM3);
    PUSH(ABI_PARAM4); // we need you later

    const u8* repeat = GetCodePtr();

    if (preinc)
        ADD(32, R(ABI_PARAM1), Imm8(4));

    OpArg currentElement = MComplex(ABI_PARAM2, ABI_PARAM3, SCALE_8, -8);

    ABI_PushRegistersAndAdjustStack({ABI_PARAM1, ABI_PARAM2, ABI_PARAM3}, 8);
    AND(32, R(ABI_PARAM1), Imm8(~3));
    if (store)
    {
        MOV(32, R(ABI_PARAM2), currentElement);
        CALL((void*)NDS::ARM7Write32);
    }
    else
        CALL((void*)NDS::ARM7Read32);
    ABI_PopRegistersAndAdjustStack({ABI_PARAM1, ABI_PARAM2, ABI_PARAM3}, 8);

    MEMORY_SEQ_WHILE_COND
    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    SHR(32, R(RSCRATCH), Imm8(15));
    MOVZX(32, 8, ABI_PARAM2, MScaled(RSCRATCH, SCALE_4, 2 + squeezePointer(NDS::ARM7MemTimings)));
    MOVZX(32, 8, RSCRATCH, MScaled(RSCRATCH, SCALE_4, 3 + squeezePointer(NDS::ARM7MemTimings)));

    POP(ABI_PARAM4);
    POP(ABI_PARAM3);

    // TODO: optimise this
    CMP(32, R(ABI_PARAM3), Imm8(1));
    FixupBranch skipSequential = J_CC(CC_E);
    SUB(32, R(ABI_PARAM3), Imm8(1));
    IMUL(32, RSCRATCH, R(ABI_PARAM3));
    ADD(32, R(ABI_PARAM2), R(RSCRATCH));
    SetJumpTarget(skipSequential);

    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    AND(32, R(RSCRATCH), Imm32(0xFF000000));
    CMP(32, R(RSCRATCH), Imm32(0x02000000));
    FixupBranch outsideMainRAM = J_CC(CC_NE);
    CALC_CYCLES_7_DATA_MAIN_RAM(ABI_PARAM4, ABI_PARAM2, RSCRATCH)
    RET();

    SetJumpTarget(outsideMainRAM);
    CALC_CYCLES_7_DATA_NON_MAIN_RAM(ABI_PARAM4, ABI_PARAM2, RSCRATCH)
    RET();

    return res;
}

#undef CALC_CYCLES_9
#undef MEMORY_SEQ_WHILE_COND

void Compiler::Comp_MemAccess(OpArg rd, bool signExtend, bool store, int size)
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

s32 Compiler::Comp_MemAccessBlock(int rn, BitSet16 regs, bool store, bool preinc, bool decrement, bool usermode)
{
    int regsCount = regs.Count();

    if (decrement)
    {
        MOV_sum(32, ABI_PARAM1, MapReg(rn), Imm32(-regsCount * 4));
        preinc ^= true;
    }
    else
        MOV(32, R(ABI_PARAM1), MapReg(rn));

    s32 offset = (regsCount * 4) * (decrement ? -1 : 1);

    u32 cycles = Num
            ? NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
            : (R15 & 0x2 ? 0 : CurInstr.CodeCycles);

    // we need to make sure that the stack stays aligned to 16 bytes
    u32 stackAlloc = ((regsCount + 1) & ~1) * 8;

    MOV(32, R(ABI_PARAM4), Imm32(cycles));
    if (!store)
    {
        MOV(32, R(ABI_PARAM3), Imm32(regsCount));
        SUB(64, R(RSP), stackAlloc <= INT8_MAX ? Imm8(stackAlloc) : Imm32(stackAlloc));
        MOV(64, R(ABI_PARAM2), R(RSP));

        CALL(Num == 0
            ? MemoryFuncsSeq9[0][preinc]
            : MemoryFuncsSeq7[0][preinc][CodeRegion == 0x02]);

        bool firstUserMode = true;
        for (int reg = 15; reg >= 0; reg--)
        {
            if (regs[reg])
            {
                if (usermode && reg >= 8 && reg < 15)
                {
                    if (firstUserMode)
                    {
                        MOV(32, R(RSCRATCH), R(RCPSR));
                        AND(32, R(RSCRATCH), Imm8(0x1F));
                        firstUserMode = false;
                    }
                    MOV(32, R(ABI_PARAM2), Imm32(reg - 8));
                    POP(ABI_PARAM3);
                    CALL(WriteBanked);
                    FixupBranch sucessfulWritten = J_CC(CC_NC);
                    if (RegCache.Mapping[reg] != INVALID_REG)
                        MOV(32, R(RegCache.Mapping[reg]), R(ABI_PARAM3));
                    SaveReg(reg, ABI_PARAM3);
                    SetJumpTarget(sucessfulWritten);
                }
                else if (RegCache.Mapping[reg] == INVALID_REG)
                {
                    assert(reg != 15);

                    POP(RSCRATCH);
                    SaveReg(reg, RSCRATCH);
                }
                else
                {
                    if (reg != 15)
                        RegCache.DirtyRegs |= (1 << reg);
                    POP(MapReg(reg).GetSimpleReg());
                }
            }
        }

        if (regsCount & 1)
            POP(RSCRATCH);

        if (regs[15])
        {
            if (Num == 1)
            {
                if (Thumb)
                    OR(32, MapReg(15), Imm8(1));
                else
                    AND(32, MapReg(15), Imm8(0xFE));
            }
            Comp_JumpTo(MapReg(15).GetSimpleReg(), usermode);
        }
    }
    else
    {
        if (regsCount & 1)
            PUSH(RSCRATCH);

        bool firstUserMode = true;
        for (int reg : regs)
        {
            if (usermode && reg >= 8 && reg < 15)
            {
                if (firstUserMode)
                {
                    MOV(32, R(RSCRATCH), R(RCPSR));
                    AND(32, R(RSCRATCH), Imm8(0x1F));
                    firstUserMode = false;
                }
                if (RegCache.Mapping[reg] == INVALID_REG)
                    LoadReg(reg, ABI_PARAM3);
                else
                    MOV(32, R(ABI_PARAM3), R(RegCache.Mapping[reg]));
                MOV(32, R(ABI_PARAM2), Imm32(reg - 8));
                CALL(ReadBanked);
                PUSH(ABI_PARAM3);
            }
            else if (RegCache.Mapping[reg] == INVALID_REG)
            {
                LoadReg(reg, RSCRATCH);
                PUSH(RSCRATCH);
            }
            else
            {
                PUSH(MapReg(reg).GetSimpleReg());
            }
        }

        MOV(64, R(ABI_PARAM2), R(RSP));
        MOV(32, R(ABI_PARAM3), Imm32(regsCount));

        CALL(Num == 0
            ? MemoryFuncsSeq9[1][preinc]
            : MemoryFuncsSeq7[1][preinc][CodeRegion == 0x02]);

        ADD(64, R(RSP), stackAlloc <= INT8_MAX ? Imm8(stackAlloc) : Imm32(stackAlloc));
    }

    return offset;
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

    int op = (CurInstr.Instr >> 5) & 0x3;
    bool load = CurInstr.Instr & (1 << 20);

    bool signExtend = false;
    int size;
    if (!load)
    {
        size = op == 1 ? 16 : 32;
        load = op == 2;
    }
    else if (load)
    {
        size = op == 2 ? 8 : 16;
        signExtend = op > 1;
    }

    if (size == 32 && Num == 1)
        return; // NOP

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

void Compiler::A_Comp_LDM_STM()
{
    BitSet16 regs(CurInstr.Instr & 0xFFFF);

    bool load = CurInstr.Instr & (1 << 20);
    bool pre = CurInstr.Instr & (1 << 24);
    bool add = CurInstr.Instr & (1 << 23);
    bool writeback = CurInstr.Instr & (1 << 21);
    bool usermode = CurInstr.Instr & (1 << 22);

    OpArg rn = MapReg(CurInstr.A_Reg(16));

    s32 offset = Comp_MemAccessBlock(CurInstr.A_Reg(16), regs, !load, pre, !add, usermode);

    if (load && writeback && regs[CurInstr.A_Reg(16)])
        writeback = Num == 0
            ? (!(regs & ~BitSet16(1 << CurInstr.A_Reg(16)))) || (regs & ~BitSet16((2 << CurInstr.A_Reg(16)) - 1))
            : false;
    if (writeback)
        ADD(32, rn, offset >= INT8_MIN && offset < INT8_MAX ? Imm8(offset) : Imm32(offset));
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

void Compiler::T_Comp_LoadPCRel()
{
    OpArg rd = MapReg(CurInstr.T_Reg(8));
    u32 addr = (R15 & ~0x2) + ((CurInstr.Instr & 0xFF) << 2);

    // hopefully this doesn't break
    u32 val; CurCPU->DataRead32(addr, &val);
    MOV(32, rd, Imm32(val));
}

void Compiler::T_Comp_MemSPRel()
{
    u32 offset = (CurInstr.Instr & 0xFF) * 4;
    OpArg rd = MapReg(CurInstr.T_Reg(8));
    bool load = CurInstr.Instr & (1 << 11);

    LEA(32, ABI_PARAM1, MDisp(MapReg(13).GetSimpleReg(), offset));

    Comp_MemAccess(rd, false, !load, 32);
}

void Compiler::T_Comp_PUSH_POP()
{
    bool load = CurInstr.Instr & (1 << 11);
    BitSet16 regs(CurInstr.Instr & 0xFF);
    if (CurInstr.Instr & (1 << 8))
    {
        if (load)
            regs[15] = true;
        else
            regs[14] = true;
    }

    OpArg sp = MapReg(13);
    s32 offset = Comp_MemAccessBlock(13, regs, !load, !load, !load, false);

    ADD(32, sp, Imm8(offset)); // offset will be always be in range since PUSH accesses 9 regs max
}

void Compiler::T_Comp_LDMIA_STMIA()
{
    BitSet16 regs(CurInstr.Instr & 0xFF);
    OpArg rb = MapReg(CurInstr.T_Reg(8));
    bool load = CurInstr.Instr & (1 << 11);

    s32 offset = Comp_MemAccessBlock(CurInstr.T_Reg(8), regs, !load, false, false, false);

    if (!load || !regs[CurInstr.T_Reg(8)])
        ADD(32, rb, Imm8(offset));
}

}