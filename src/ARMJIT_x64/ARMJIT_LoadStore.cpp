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
    MOV(32, R(ABI_PARAM3), R(ABI_PARAM1)); // free up ECX
    AND(32, R(ABI_PARAM3), Imm32(0x7FFF & addressMask));
    if (store)
    {
        MOV(size, MComplex(RCPU, ABI_PARAM3, SCALE_1, offsetof(ARMv5, ITCM)), R(ABI_PARAM2));
        
        // if CodeRanges[pseudoPhysical/256].Blocks.Length > 0 we're writing into code!
        static_assert(sizeof(AddressRange) == 16);
        LEA(32, ABI_PARAM1, MDisp(ABI_PARAM3, ExeMemRegionOffsets[exeMem_ITCM]));
        MOV(32, R(RSCRATCH), R(ABI_PARAM1));
        SHR(32, R(RSCRATCH), Imm8(9));
        SHL(32, R(RSCRATCH), Imm8(4));
        CMP(32, MDisp(RSCRATCH, squeezePointer(CodeRanges) + offsetof(AddressRange, Blocks.Length)), Imm8(0));
        FixupBranch noCode = J_CC(CC_Z);
        JMP((u8*)InvalidateByAddr, true);
        SetJumpTarget(noCode);
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

    Dolphin x64CodeEmitter is my favourite assembler
 */
void* Compiler::Gen_MemoryRoutineSeq9(bool store, bool preinc)
{
    void* res = (void*)GetWritableCodePtr();

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
    RET();

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
    RET();

    SetJumpTarget(insideITCM);
    MOV(32, R(RSCRATCH), R(ABI_PARAM1));
    AND(32, R(RSCRATCH), Imm32(0x7FFF & ~3));
    if (store)
    {
        MOV(32, R(ABI_PARAM4), currentElement);
        MOV(32, MComplex(RCPU, RSCRATCH, SCALE_1, offsetof(ARMv5, ITCM)), R(ABI_PARAM4));

        ADD(32, R(RSCRATCH), Imm32(ExeMemRegionOffsets[exeMem_ITCM]));
        MOV(32, R(ABI_PARAM4), R(RSCRATCH));
        SHR(32, R(RSCRATCH), Imm8(9));
        SHL(32, R(RSCRATCH), Imm8(4));
        CMP(32, MDisp(RSCRATCH, squeezePointer(CodeRanges) + offsetof(AddressRange, Blocks.Length)), Imm8(0));
        FixupBranch noCode = J_CC(CC_Z);
        ABI_PushRegistersAndAdjustStack({ABI_PARAM1, ABI_PARAM2, ABI_PARAM3}, 8);
        MOV(32, R(ABI_PARAM1), R(ABI_PARAM4));
        CALL((u8*)InvalidateByAddr);
        ABI_PopRegistersAndAdjustStack({ABI_PARAM1, ABI_PARAM2, ABI_PARAM3}, 8);
        SetJumpTarget(noCode);
    }
    else
        MOV(32, R(RSCRATCH), MComplex(RCPU, RSCRATCH, SCALE_1, offsetof(ARMv5, ITCM)));

    MEMORY_SEQ_WHILE_COND
    RET();

    return res;
}

void* Compiler::Gen_MemoryRoutineSeq7(bool store, bool preinc, bool codeMainRAM)
{
    void* res = (void*)GetWritableCodePtr();

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
    RET();

    return res;
}

#undef MEMORY_SEQ_WHILE_COND

void Compiler::Comp_MemLoadLiteral(int size, int rd, u32 addr)
{
    u32 val;
    // make sure arm7 bios is accessible
    u32 tmpR15 = CurCPU->R[15];
    CurCPU->R[15] = R15;
    if (size == 32)
    {
        CurCPU->DataRead32(addr & ~0x3, &val);
        val = ROR(val, (addr & 0x3) << 3);
    }
    else if (size == 16)
        CurCPU->DataRead16(addr & ~0x1, &val);
    else
        CurCPU->DataRead8(addr, &val);
    CurCPU->R[15] = tmpR15;

    MOV(32, MapReg(rd), Imm32(val));

    if (Thumb || CurInstr.Cond() == 0xE)
        RegCache.PutLiteral(rd, val);

    Comp_AddCycles_CDI();
}

void fault(u32 a, u32 b)
{
    printf("actually not static! %x %x\n", a, b);
}

void Compiler::Comp_MemAccess(int rd, int rn, const ComplexOperand& op2, int size, int flags)
{
    u32 addressMask = ~0;
    if (size == 32)
        addressMask = ~3;
    if (size == 16)
        addressMask = ~1;

    if (rn == 15 && rd != 15 && op2.IsImm && !(flags & (memop_SignExtend|memop_Post|memop_Store|memop_Writeback)))
    {
        u32 addr = R15 + op2.Imm * ((flags & memop_SubtractOffset) ? -1 : 1);
        Comp_MemLoadLiteral(size, rd, addr);
        return;
    }

    {
        if (flags & memop_Store)
        {
            Comp_AddCycles_CD();
        }
        else
        {
            Comp_AddCycles_CDI();
        }

        OpArg rdMapped = MapReg(rd);
        OpArg rnMapped = MapReg(rn);

        bool inlinePreparation = Num == 1;
        u32 constLocalROR32 = 4;

        void* memoryFunc = Num == 0
            ? MemoryFuncs9[size >> 4][!!(flags & memop_Store)]
            : MemoryFuncs7[size >> 4][!!((flags & memop_Store))];

        if ((rd != 15 || (flags & memop_Store)) && op2.IsImm && RegCache.IsLiteral(rn))
        {
            u32 addr = RegCache.LiteralValues[rn] + op2.Imm * ((flags & memop_SubtractOffset) ? -1 : 1);

            /*MOV(32, R(ABI_PARAM1), Imm32(CurInstr.Instr));
            MOV(32, R(ABI_PARAM1), Imm32(R15));
            MOV_sum(32, RSCRATCH, rnMapped, Imm32(op2.Imm * ((flags & memop_SubtractOffset) ? -1 : 1)));
            CMP(32, R(RSCRATCH), Imm32(addr));
            FixupBranch eq = J_CC(CC_E);
            CALL((void*)fault);
            SetJumpTarget(eq);*/

            NDS::MemRegion region;
            region.Mem = NULL;
            if (Num == 0)
            {
                ARMv5* cpu5 = (ARMv5*)CurCPU;

                // stupid dtcm...
                if (addr >= cpu5->DTCMBase && addr < (cpu5->DTCMBase + cpu5->DTCMSize))
                {
                    region.Mem = cpu5->DTCM;
                    region.Mask = 0x3FFF;
                }
                else
                {
                    NDS::ARM9GetMemRegion(addr, flags & memop_Store, &region);
                }
            }
            else
                NDS::ARM7GetMemRegion(addr, flags & memop_Store, &region);

            if (region.Mem != NULL)
            {
                void* ptr = &region.Mem[addr & addressMask & region.Mask];

                if (flags & memop_Store)
                {
                    MOV(size, M(ptr), MapReg(rd));
                }
                else
                {
                    if (flags & memop_SignExtend)
                        MOVSX(32, size, rdMapped.GetSimpleReg(), M(ptr));
                    else
                        MOVZX(32, size, rdMapped.GetSimpleReg(), M(ptr));

                    if (size == 32 && addr & ~0x3)
                    {
                        ROR_(32, rdMapped, Imm8((addr & 0x3) << 3));
                    }
                }

                return;
            }

            void* specialFunc = GetFuncForAddr(CurCPU, addr, flags & memop_Store, size);
            if (specialFunc)
            {
                memoryFunc = specialFunc;
                inlinePreparation = true;
                constLocalROR32 = addr & 0x3;
            }
        }

        X64Reg finalAddr = ABI_PARAM1;
        if (flags & memop_Post)
        {
            MOV(32, R(ABI_PARAM1), rnMapped);

            finalAddr = rnMapped.GetSimpleReg();
        }

        if (op2.IsImm)
        {
            MOV_sum(32, finalAddr, rnMapped, Imm32(op2.Imm * ((flags & memop_SubtractOffset) ? -1 : 1)));
        }
        else
        {
            OpArg rm = MapReg(op2.Reg.Reg);

            if (!(flags & memop_SubtractOffset) && rm.IsSimpleReg() && rnMapped.IsSimpleReg()
                && op2.Reg.Op == 0 && op2.Reg.Amount > 0 && op2.Reg.Amount <= 3)
            {
                LEA(32, finalAddr, 
                    MComplex(rnMapped.GetSimpleReg(), rm.GetSimpleReg(), 1 << op2.Reg.Amount, 0));
            }
            else
            {
                bool throwAway;
                OpArg offset =
                    Comp_RegShiftImm(op2.Reg.Op, op2.Reg.Amount, rm, false, throwAway);
                
                if (flags & memop_SubtractOffset)
                {
                    MOV(32, R(finalAddr), rnMapped);
                    if (!offset.IsZero())
                        SUB(32, R(finalAddr), offset);
                }
                else
                    MOV_sum(32, finalAddr, rnMapped, offset);
            }
        }

        if ((flags & memop_Writeback) && !(flags & memop_Post))
            MOV(32, rnMapped, R(finalAddr));

        if (flags & memop_Store)
            MOV(32, R(ABI_PARAM2), rdMapped);

        if (!(flags & memop_Store) && inlinePreparation && constLocalROR32 == 4 && size == 32)
            MOV(32, rdMapped, R(ABI_PARAM1));

        if (inlinePreparation && size > 8)
            AND(32, R(ABI_PARAM1), Imm8(addressMask));

        CALL(memoryFunc);

        if (!(flags & memop_Store))
        {
            if (inlinePreparation && size == 32)
            {
                if (constLocalROR32 == 4)
                {
                    static_assert(RSCRATCH3 == ECX);
                    MOV(32, R(ECX), rdMapped);
                    AND(32, R(ECX), Imm8(3));
                    SHL(32, R(ECX), Imm8(3));
                    ROR_(32, R(RSCRATCH), R(ECX));
                }
                else if (constLocalROR32 != 0)
                    ROR_(32, R(RSCRATCH), Imm8(constLocalROR32 << 3));
            }

            if (flags & memop_SignExtend)
                MOVSX(32, size, rdMapped.GetSimpleReg(), R(RSCRATCH));
            else
                MOVZX(32, size, rdMapped.GetSimpleReg(), R(RSCRATCH));
        }

        if (!(flags & memop_Store) && rd == 15)
        {
            if (size < 32)
                printf("!!! LDR <32 bit PC %08X %x\n", R15, CurInstr.Instr);
            {
                if (Num == 1)
                    AND(32, rdMapped, Imm8(0xFE)); // immediate is sign extended
                Comp_JumpTo(rdMapped.GetSimpleReg());
            }
        }
    }
}

s32 Compiler::Comp_MemAccessBlock(int rn, BitSet16 regs, bool store, bool preinc, bool decrement, bool usermode)
{
    IrregularCycles = true;

    int regsCount = regs.Count();

    if (decrement)
    {
        MOV_sum(32, ABI_PARAM1, MapReg(rn), Imm32(-regsCount * 4));
        preinc ^= true;
    }
    else
        MOV(32, R(ABI_PARAM1), MapReg(rn));

    s32 offset = (regsCount * 4) * (decrement ? -1 : 1);

    // we need to make sure that the stack stays aligned to 16 bytes
    u32 stackAlloc = ((regsCount + 1) & ~1) * 8;

    if (!store)
    {
        Comp_AddCycles_CDI();

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
        Comp_AddCycles_CD();

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


void Compiler::A_Comp_MemWB()
{
    bool load = CurInstr.Instr & (1 << 20);
    bool byte = CurInstr.Instr & (1 << 22);
    int size = byte ? 8 : 32;
    
    int flags = 0;
    if (!load)
        flags |= memop_Store;
    if (!(CurInstr.Instr & (1 << 24)))
        flags |= memop_Post;
    if (CurInstr.Instr & (1 << 21))
        flags |= memop_Writeback;
    if (!(CurInstr.Instr & (1 << 23)))
        flags |= memop_SubtractOffset;

    ComplexOperand offset;
    if (!(CurInstr.Instr & (1 << 25)))
    {
        offset = ComplexOperand(CurInstr.Instr & 0xFFF);
    }
    else
    {
        int op = (CurInstr.Instr >> 5) & 0x3;
        int amount = (CurInstr.Instr >> 7) & 0x1F;
        int rm = CurInstr.A_Reg(0);

        offset = ComplexOperand(rm, op, amount);
    }

    Comp_MemAccess(CurInstr.A_Reg(12), CurInstr.A_Reg(16), offset, size, flags);
}

void Compiler::A_Comp_MemHalf()
{
    ComplexOperand offset = CurInstr.Instr & (1 << 22)
        ? ComplexOperand(CurInstr.Instr & 0xF | ((CurInstr.Instr >> 4) & 0xF0))
        : ComplexOperand(CurInstr.A_Reg(0), 0, 0);

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

    int flags = 0;
    if (signExtend)
        flags |= memop_SignExtend;
    if (!load)
        flags |= memop_Store;
    if (!(CurInstr.Instr & (1 << 24)))
        flags |= memop_Post;
    if (!(CurInstr.Instr & (1 << 23)))
        flags |= memop_SubtractOffset;
    if (CurInstr.Instr & (1 << 21))
        flags |= memop_Writeback;

    Comp_MemAccess(CurInstr.A_Reg(12), CurInstr.A_Reg(16), offset, size, flags);
}

void Compiler::T_Comp_MemReg()
{
    int op = (CurInstr.Instr >> 10) & 0x3;
    bool load = op & 0x2;
    bool byte = op & 0x1;

    Comp_MemAccess(CurInstr.T_Reg(0), CurInstr.T_Reg(3), ComplexOperand(CurInstr.T_Reg(6), 0, 0), 
        byte ? 8 : 32, load ? 0 : memop_Store);
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
    int op = (CurInstr.Instr >> 11) & 0x3;
    bool load = op & 0x1;
    bool byte = op & 0x2;
    u32 offset = ((CurInstr.Instr >> 6) & 0x1F) * (byte ? 1 : 4);

    Comp_MemAccess(CurInstr.T_Reg(0), CurInstr.T_Reg(3), ComplexOperand(offset),
        byte ? 8 : 32, load ? 0 : memop_Store);
}

void Compiler::T_Comp_MemRegHalf()
{
    int op = (CurInstr.Instr >> 10) & 0x3;
    bool load = op != 0;
    int size = op != 1 ? 16 : 8;
    bool signExtend = op & 1;

    int flags = 0;
    if (signExtend)
        flags |= memop_SignExtend;
    if (!load)
        flags |= memop_Store;

    Comp_MemAccess(CurInstr.T_Reg(0), CurInstr.T_Reg(3), ComplexOperand(CurInstr.T_Reg(6), 0, 0),
        size, flags);
}

void Compiler::T_Comp_MemImmHalf()
{
    u32 offset = (CurInstr.Instr >> 5) & 0x3E;
    bool load = CurInstr.Instr & (1 << 11);

    Comp_MemAccess(CurInstr.T_Reg(0), CurInstr.T_Reg(3), ComplexOperand(offset), 16,
        load ? 0 : memop_Store);
}

void Compiler::T_Comp_LoadPCRel()
{
    u32 addr = (R15 & ~0x2) + ((CurInstr.Instr & 0xFF) << 2);

    Comp_MemLoadLiteral(32, CurInstr.T_Reg(8), addr);
}

void Compiler::T_Comp_MemSPRel()
{
    u32 offset = (CurInstr.Instr & 0xFF) * 4;
    bool load = CurInstr.Instr & (1 << 11);

    Comp_MemAccess(CurInstr.T_Reg(8), 13, ComplexOperand(offset), 32,
        load ? 0 : memop_Store);
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