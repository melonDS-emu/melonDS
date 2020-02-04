#include "ARMJIT_Compiler.h"

#include "../Config.h"

using namespace Arm64Gen;

namespace ARMJIT
{

// W0 - address
// (if store) W1 - value to store
// W2 - code cycles
void* Compiler::Gen_MemoryRoutine9(int size, bool store)
{
    AlignCode16();
    void* res = GetRXPtr();

    u32 addressMask;
    switch (size)
    {
    case 32: addressMask = ~3; break;
    case 16: addressMask = ~1; break;
    case 8:  addressMask = ~0; break;
    }

    LDR(INDEX_UNSIGNED, W3, RCPU, offsetof(ARMv5, DTCMBase));
    LDR(INDEX_UNSIGNED, W4, RCPU, offsetof(ARMv5, DTCMSize));
    SUB(W3, W0, W3);
    CMP(W3, W4);
    FixupBranch insideDTCM = B(CC_LO);

    UBFX(W4, W0, 24, 8);
    CMP(W4, 0x02);
    FixupBranch outsideMainRAM = B(CC_NEQ);
    ANDI2R(W3, W0, addressMask & (MAIN_RAM_SIZE - 1));
    MOVP2R(X4, NDS::MainRAM);
    if (!store && size == 32)
    {
        LDR(W3, X3, X4);
        ANDI2R(W0, W0, 3);
        LSL(W0, W0, 3);
        RORV(W0, W3, W0);
    }
    else if (store)
        STRGeneric(size, W1, X3, X4);
    else
        LDRGeneric(size, false, W0, X3, X4);
    RET();

    SetJumpTarget(outsideMainRAM);

    LDR(INDEX_UNSIGNED, W3, RCPU, offsetof(ARMv5, ITCMSize));
    CMP(W0, W3);
    FixupBranch insideITCM = B(CC_LO);

    if (store)
    {
        if (size > 8)
            ANDI2R(W0, W0, addressMask);

        switch (size)
        {
        case 32: QuickTailCall(X4, NDS::ARM9Write32); break;
        case 16: QuickTailCall(X4, NDS::ARM9Write16); break;
        case 8:  QuickTailCall(X4, NDS::ARM9Write8);  break;
        }
    }
    else
    {
        if (size == 32)
            ABI_PushRegisters({0, 30});
        if (size > 8)
            ANDI2R(W0, W0, addressMask);

        switch (size)
        {
        case 32: QuickCallFunction(X4, NDS::ARM9Read32); break;
        case 16: QuickTailCall    (X4, NDS::ARM9Read16); break;
        case 8:  QuickTailCall    (X4, NDS::ARM9Read8 ); break;
        }
        if (size == 32)
        {
            ABI_PopRegisters({1, 30});
            ANDI2R(W1, W1, 3);
            LSL(W1, W1, 3);
            RORV(W0, W0, W1);
            RET();
        }
    }

    SetJumpTarget(insideDTCM);
    ANDI2R(W3, W3, 0x3FFF & addressMask);
    ADDI2R(W3, W3, offsetof(ARMv5, DTCM), W4);
    if (!store && size == 32)
    {
        ANDI2R(W4, W0, 3);
        LDR(W0, RCPU, W3);
        LSL(W4, W4, 3);
        RORV(W0, W0, W4);
    }
    else if (store)
        STRGeneric(size, W1, RCPU, W3);
    else
        LDRGeneric(size, false, W0, RCPU, W3);
    
    RET();

    SetJumpTarget(insideITCM);
    ANDI2R(W3, W0, 0x7FFF & addressMask);
    if (store)
    {
        LSR(W0, W3, 8);
        ADDI2R(W0, W0, ExeMemRegionOffsets[exeMem_ITCM], W4);
        MOVP2R(X4, CodeRanges);
        ADD(X4, X4, X0, ArithOption(X0, ST_LSL, 4));
        static_assert(sizeof(AddressRange) == 16);
        LDR(INDEX_UNSIGNED, W4, X4, offsetof(AddressRange, Blocks.Length));
        FixupBranch null = CBZ(W4);
        ABI_PushRegisters({1, 3, 30});
        QuickCallFunction(X4, InvalidateByAddr);
        ABI_PopRegisters({1, 3, 30});
        SetJumpTarget(null);
    }
    ADDI2R(W3, W3, offsetof(ARMv5, ITCM), W4);
    if (!store && size == 32)
    {
        ANDI2R(W4, W0, 3);
        LDR(W0, RCPU, W3);
        LSL(W4, W4, 3);
        RORV(W0, W0, W4);
    }
    else if (store)
        STRGeneric(size, W1, RCPU, W3);
    else
        LDRGeneric(size, false, W0, RCPU, W3);
    RET();

    return res;
}

/*
    W0 - base address
    X1 - stack space
    W2 - values count
*/
void* Compiler::Gen_MemoryRoutine9Seq(bool store, bool preinc)
{
    AlignCode16();
    void* res = GetRXPtr();
    
    void* loopStart = GetRXPtr();
    SUB(W2, W2, 1);

    if (preinc)
        ADD(W0, W0, 4);

    LDR(INDEX_UNSIGNED, W4, RCPU, offsetof(ARMv5, DTCMBase));
    LDR(INDEX_UNSIGNED, W5, RCPU, offsetof(ARMv5, DTCMSize));
    SUB(W4, W0, W4);
    CMP(W4, W5);
    FixupBranch insideDTCM = B(CC_LO);

    LDR(INDEX_UNSIGNED, W4, RCPU, offsetof(ARMv5, ITCMSize));
    CMP(W0, W4);
    FixupBranch insideITCM = B(CC_LO);

    ABI_PushRegisters({0, 1, 2, 30}); // TODO: move SP only once
    if (store)
    {
        LDR(X1, X1, ArithOption(X2, true));
        QuickCallFunction(X4, NDS::ARM9Write32);

        ABI_PopRegisters({0, 1, 2, 30});
    }
    else
    {
        QuickCallFunction(X4, NDS::ARM9Read32);
        MOV(W4, W0);

        ABI_PopRegisters({0, 1, 2, 30});

        STR(X4, X1, ArithOption(X2, true));
    }

    if (!preinc)
        ADD(W0, W0, 4);
    CBNZ(W2, loopStart);
    RET();

    SetJumpTarget(insideDTCM);

    ANDI2R(W4, W4, ~3 & 0x3FFF);
    ADDI2R(X4, X4, offsetof(ARMv5, DTCM));
    if (store)
    {
        LDR(X5, X1, ArithOption(X2, true));
        STR(W5, RCPU, X4);
    }
    else
    {
        LDR(W5, RCPU, X4);
        STR(X5, X1, ArithOption(X2, true));
    }

    if (!preinc)
        ADD(W0, W0, 4);
    CBNZ(W2, loopStart);
    RET();

    SetJumpTarget(insideITCM);

    ANDI2R(W4, W0, ~3 & 0x7FFF);

    if (store)
    {
        LSR(W6, W4, 8);
        ADDI2R(W6, W6, ExeMemRegionOffsets[exeMem_ITCM], W5);
        MOVP2R(X5, CodeRanges);
        ADD(X5, X5, X6, ArithOption(X6, ST_LSL, 4));
        static_assert(sizeof(AddressRange) == 16);
        LDR(INDEX_UNSIGNED, W5, X5, offsetof(AddressRange, Blocks.Length));
        FixupBranch null = CBZ(W5);
        ABI_PushRegisters({0, 1, 2, 4, 30});
        MOV(W0, W6);
        QuickCallFunction(X5, InvalidateByAddr);
        ABI_PopRegisters({0, 1, 2, 4, 30});
        SetJumpTarget(null);
    }

    ADDI2R(W4, W4, offsetof(ARMv5, ITCM), W5);
    if (store)
    {
        LDR(X5, X1, ArithOption(X2, true));
        STR(W5, RCPU, X4);
    }
    else
    {
        LDR(W5, RCPU, X4);
        STR(X5, X1, ArithOption(X2, true));
    }

    if (!preinc)
        ADD(W0, W0, 4);
    CBNZ(W2, loopStart);
    RET();
    return res;
}

void* Compiler::Gen_MemoryRoutine7Seq(bool store, bool preinc)
{
    AlignCode16();
    void* res = GetRXPtr();

    void* loopStart = GetRXPtr();
    SUB(W2, W2, 1);

    if (preinc)
        ADD(W0, W0, 4);

    ABI_PushRegisters({0, 1, 2, 30});
    if (store)
    {
        LDR(X1, X1, ArithOption(X2, true));
        QuickCallFunction(X4, NDS::ARM7Write32);
        ABI_PopRegisters({0, 1, 2, 30});
    }
    else
    {
        QuickCallFunction(X4, NDS::ARM7Read32);
        MOV(W4, W0);
        ABI_PopRegisters({0, 1, 2, 30});
        STR(X4, X1, ArithOption(X2, true));
    }

    if (!preinc)
        ADD(W0, W0, 4);
    CBNZ(W2, loopStart);
    RET();

    return res;
}

void Compiler::Comp_MemLoadLiteral(int size, bool signExtend, int rd, u32 addr)
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
    {
        CurCPU->DataRead16(addr & ~0x1, &val);
        if (signExtend)
            val = ((s32)val << 16) >> 16;
    }
    else
    {
        CurCPU->DataRead8(addr, &val);
        if (signExtend)
            val = ((s32)val << 24) >> 24;
    }
    CurCPU->R[15] = tmpR15;

    MOVI2R(MapReg(rd), val);

    if (Thumb || CurInstr.Cond() == 0xE)
        RegCache.PutLiteral(rd, val);
}

void Compiler::Comp_MemAccess(int rd, int rn, Op2 offset, int size, int flags)
{
    u32 addressMask = ~0;
    if (size == 32)
        addressMask = ~3;
    if (size == 16)
        addressMask = ~1;
    
    if (flags & memop_Store)
        Comp_AddCycles_CD();
    else
        Comp_AddCycles_CDI();

    if (Config::JIT_LiteralOptimisations && rn == 15 && rd != 15 && offset.IsImm && !(flags & (memop_Post|memop_Store|memop_Writeback)))
    {
        u32 addr = R15 + offset.Imm * ((flags & memop_SubtractOffset) ? -1 : 1);
        u32 translatedAddr = Num == 0 ? TranslateAddr<0>(addr) : TranslateAddr<1>(addr);

        if (!(CodeRanges[translatedAddr / 512].InvalidLiterals & (1 << ((translatedAddr & 0x1FF) / 16))))
        {
            Comp_MemLoadLiteral(size, flags & memop_SignExtend, rd, addr);
            return;
        }
    }

    {
        ARM64Reg rdMapped = MapReg(rd);
        ARM64Reg rnMapped = MapReg(rn);

        bool inlinePreparation = Num == 1;
        u32 constLocalROR32 = 4;

        void* memFunc = Num == 0
            ? MemFunc9[size >> 4][!!(flags & memop_Store)]
            : MemFunc7[size >> 4][!!((flags & memop_Store))];

        if (Config::JIT_LiteralOptimisations && (rd != 15 || (flags & memop_Store)) && offset.IsImm && RegCache.IsLiteral(rn))
        {
            u32 addr = RegCache.LiteralValues[rn] + offset.Imm * ((flags & memop_SubtractOffset) ? -1 : 1);

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

                MOVP2R(X0, ptr);
                if (flags & memop_Store)
                    STRGeneric(size, INDEX_UNSIGNED, rdMapped, X0, 0);
                else
                {
                    LDRGeneric(size, flags & memop_SignExtend, INDEX_UNSIGNED, rdMapped, X0, 0);
                    if (size == 32 && addr & ~0x3)
                        ROR_(rdMapped, rdMapped, (addr & 0x3) << 3);
                }
                return;
            }

            void* specialFunc = GetFuncForAddr(CurCPU, addr, flags & memop_Store, size);
            if (specialFunc)
            {
                memFunc = specialFunc;
                inlinePreparation = true;
                constLocalROR32 = addr & 0x3;
            }
        }

        ARM64Reg finalAddr = W0;
        if (flags & memop_Post)
        {
            finalAddr = rnMapped;
            MOV(W0, rnMapped);
        }

        if (flags & memop_Store)
            MOV(W1, rdMapped);

        if (!offset.IsImm)
            Comp_RegShiftImm(offset.Reg.ShiftType, offset.Reg.ShiftAmount, false, offset, W2);
        // offset might become an immediate
        if (offset.IsImm)
        {
            if (flags & memop_SubtractOffset)
                SUB(finalAddr, rnMapped, offset.Imm);
            else
                ADD(finalAddr, rnMapped, offset.Imm);
        }
        else
        {
            if (offset.Reg.ShiftType == ST_ROR)
            {
                ROR_(W0, offset.Reg.Rm, offset.Reg.ShiftAmount);
                offset = Op2(W0);
            }

            if (flags & memop_SubtractOffset)
                SUB(finalAddr, rnMapped, offset.Reg.Rm, offset.ToArithOption());
            else
                ADD(finalAddr, rnMapped, offset.Reg.Rm, offset.ToArithOption());
        }

        if (!(flags & memop_Post) && (flags & memop_Writeback))
            MOV(rnMapped, W0);

        if (inlinePreparation)
        {
            if (size == 32 && !(flags & memop_Store) && constLocalROR32 == 4)
                ANDI2R(rdMapped, W0, 3);
            if (size > 8)
                ANDI2R(W0, W0, addressMask);
        }
        QuickCallFunction(X2, memFunc);
        if (!(flags & memop_Store))
        {
            if (inlinePreparation && !(flags & memop_Store) && size == 32)
            {
                if (constLocalROR32 == 4)
                {
                    LSL(rdMapped, rdMapped, 3);
                    RORV(rdMapped, W0, rdMapped);
                }
                else if (constLocalROR32 > 0)
                    ROR_(rdMapped, W0, constLocalROR32 << 3);
                else
                    MOV(rdMapped, W0);
            }
            else if (flags & memop_SignExtend)
            {
                if (size == 16)
                    SXTH(rdMapped, W0);
                else if (size == 8)
                    SXTB(rdMapped, W0);
                else
                    assert("What's wrong with you?");
            }
            else
                MOV(rdMapped, W0);
            
            if (CurInstr.Info.Branches())
            {
                if (size < 32)
                    printf("LDR size < 32 branching?\n");
                Comp_JumpTo(rdMapped, Num == 0, false);
            }
        }
    }
}

void Compiler::A_Comp_MemWB()
{
    Op2 offset;
    if (CurInstr.Instr & (1 << 25))
        offset = Op2(MapReg(CurInstr.A_Reg(0)), (ShiftType)((CurInstr.Instr >> 5) & 0x3), (CurInstr.Instr >> 7) & 0x1F);
    else
        offset = Op2(CurInstr.Instr & 0xFFF);

    bool load = CurInstr.Instr & (1 << 20);
    bool byte = CurInstr.Instr & (1 << 22);

    int flags = 0;
    if (!load)
        flags |= memop_Store;
    if (!(CurInstr.Instr & (1 << 24)))
        flags |= memop_Post;
    if (CurInstr.Instr & (1 << 21))
        flags |= memop_Writeback;
    if (!(CurInstr.Instr & (1 << 23)))
        flags |= memop_SubtractOffset;

    Comp_MemAccess(CurInstr.A_Reg(12), CurInstr.A_Reg(16), offset, byte ? 8 : 32, flags);
}

void Compiler::A_Comp_MemHD()
{
    bool load = CurInstr.Instr & (1 << 20);
    bool signExtend;
    int op = (CurInstr.Instr >> 5) & 0x3;
    int size;
    
    if (load)
    {
        signExtend = op >= 2;
        size = op == 2 ? 8 : 16;
    }
    else
    {
        size = 16;
        signExtend = false;
    }

    Op2 offset;
    if (CurInstr.Instr & (1 << 22))
        offset = Op2((CurInstr.Instr & 0xF) | ((CurInstr.Instr >> 4) & 0xF0));
    else
        offset = Op2(MapReg(CurInstr.A_Reg(0)));
    
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

    Comp_MemAccess(CurInstr.T_Reg(0), CurInstr.T_Reg(3), 
        Op2(MapReg(CurInstr.T_Reg(6))), byte ? 8 : 32, load ? 0 : memop_Store);
}

void Compiler::T_Comp_MemImm()
{
    int op = (CurInstr.Instr >> 11) & 0x3;
    bool load = op & 0x1;
    bool byte = op & 0x2;
    u32 offset = ((CurInstr.Instr >> 6) & 0x1F) * (byte ? 1 : 4);

    Comp_MemAccess(CurInstr.T_Reg(0), CurInstr.T_Reg(3), Op2(offset), 
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

    Comp_MemAccess(CurInstr.T_Reg(0), CurInstr.T_Reg(3), Op2(MapReg(CurInstr.T_Reg(6))),
        size, flags);
}

void Compiler::T_Comp_MemImmHalf()
{
    u32 offset = (CurInstr.Instr >> 5) & 0x3E;
    bool load = CurInstr.Instr & (1 << 11);

    Comp_MemAccess(CurInstr.T_Reg(0), CurInstr.T_Reg(3), Op2(offset), 16,
        load ? 0 : memop_Store);
}

void Compiler::T_Comp_LoadPCRel()
{
    u32 addr = (R15 & ~0x2) + ((CurInstr.Instr & 0xFF) << 2);

    if (Config::JIT_LiteralOptimisations)
    {
        Comp_MemLoadLiteral(32, false, CurInstr.T_Reg(8), addr);
        Comp_AddCycles_CDI();
    }
    else
    {
        bool negative = addr < R15;
        u32 abs = negative ? R15 - addr : addr - R15;
        Comp_MemAccess(CurInstr.T_Reg(8), 15, Op2(abs), 32, negative ? memop_SubtractOffset : 0);
    }
}

void Compiler::T_Comp_MemSPRel()
{
    u32 offset = (CurInstr.Instr & 0xFF) * 4;
    bool load = CurInstr.Instr & (1 << 11);

    Comp_MemAccess(CurInstr.T_Reg(8), 13, Op2(offset), 32, load ? 0 : memop_Store);
}

s32 Compiler::Comp_MemAccessBlock(int rn, BitSet16 regs, bool store, bool preinc, bool decrement, bool usermode)
{
    IrregularCycles = true;

    int regsCount = regs.Count();

    if (regsCount == 0)
        return 0; // actually not the right behaviour TODO: fix me

    SUB(SP, SP, ((regsCount + 1) & ~1) * 8);
    if (store)
    {
        Comp_AddCycles_CD();

        if (usermode && (regs & BitSet16(0x7f00)))
            UBFX(W0, RCPSR, 0, 5);

        int i = regsCount - 1;

        BitSet16::Iterator it = regs.begin();
        while (it != regs.end())
        {
            BitSet16::Iterator nextReg = it;
            nextReg++;

            int reg = *it;

            if (usermode && !regs[15] && reg >= 8 && reg < 15)
            {
                if (RegCache.Mapping[reg] != INVALID_REG)
                    MOV(W3, MapReg(reg));
                else
                    LoadReg(reg, W3);
                MOVI2R(W1, reg - 8);
                BL(ReadBanked);
                STR(INDEX_UNSIGNED, W3, SP, i * 8);
            }
            else if (!usermode && nextReg != regs.end())
            {
                ARM64Reg first = W3;
                ARM64Reg second = W4;

                if (RegCache.Mapping[reg] != INVALID_REG)
                    first = MapReg(reg);
                else
                    LoadReg(reg, W3);

                if (RegCache.Mapping[*nextReg] != INVALID_REG)
                    second = MapReg(*nextReg);
                else
                    LoadReg(*nextReg, W4);
                
                STP(INDEX_SIGNED, EncodeRegTo64(second), EncodeRegTo64(first), SP, i * 8 - 8);

                i--;
                it++;
            }
            else if (RegCache.Mapping[reg] != INVALID_REG)
                STR(INDEX_UNSIGNED, MapReg(reg), SP, i * 8);
            else
            {
                LoadReg(reg, W3);
                STR(INDEX_UNSIGNED, W3, SP, i * 8);
            }
            i--;
            it++;
        }
    }
    if (decrement)
    {
        SUB(W0, MapReg(rn), regsCount * 4);
        preinc ^= true;
    }
    else
        MOV(W0, MapReg(rn));
    ADD(X1, SP, 0);
    MOVI2R(W2, regsCount);

    BL(Num ? MemFuncsSeq7[store][preinc] : MemFuncsSeq9[store][preinc]);

    if (!store)
    {
        Comp_AddCycles_CDI();

        if (usermode && (regs & BitSet16(0x7f00)))
            UBFX(W0, RCPSR, 0, 5);

        int i = regsCount - 1;
        BitSet16::Iterator it = regs.begin();
        while (it != regs.end())
        {
            BitSet16::Iterator nextReg = it;
            nextReg++;

            int reg = *it;

            if (usermode && reg >= 8 && reg < 15)
            {
                LDR(INDEX_UNSIGNED, W3, SP, i * 8);
                MOVI2R(W1, reg - 8);
                BL(WriteBanked);
                FixupBranch alreadyWritten = CBNZ(W4);
                if (RegCache.Mapping[reg] != INVALID_REG)
                {
                    MOV(MapReg(reg), W3);
                    RegCache.DirtyRegs |= 1 << reg;
                }
                else
                    SaveReg(reg, W3);
                SetJumpTarget(alreadyWritten);
            }
            else if (!usermode && nextReg != regs.end())
            {
                ARM64Reg first = W3, second = W4;
                
                if (RegCache.Mapping[reg] != INVALID_REG)
                {
                    first = MapReg(reg);
                    if (reg != 15)
                        RegCache.DirtyRegs |= 1 << reg;
                }
                if (RegCache.Mapping[*nextReg] != INVALID_REG)
                {
                    second = MapReg(*nextReg);
                    if (*nextReg != 15)
                        RegCache.DirtyRegs |= 1 << *nextReg;
                }
                
                LDP(INDEX_SIGNED, EncodeRegTo64(second), EncodeRegTo64(first), SP, i * 8 - 8);

                if (first == W3)
                    SaveReg(reg, W3);
                if (second == W4)
                    SaveReg(*nextReg, W4);

                it++;
                i--;
            }
            else if (RegCache.Mapping[reg] != INVALID_REG)
            {
                ARM64Reg mapped = MapReg(reg);
                LDR(INDEX_UNSIGNED, mapped, SP, i * 8);

                if (reg != 15)
                    RegCache.DirtyRegs |= 1 << reg;
            }
            else
            {
                LDR(INDEX_UNSIGNED, W3, SP, i * 8);
                SaveReg(reg, W3);
            }

            it++;
            i--;
        }
    }
    ADD(SP, SP, ((regsCount + 1) & ~1) * 8);

    if (!store && regs[15])
    {
        ARM64Reg mapped = MapReg(15);
        Comp_JumpTo(mapped, Num == 0, usermode);
    }

    return regsCount * 4 * (decrement ? -1 : 1);
}

void Compiler::A_Comp_LDM_STM()
{
    BitSet16 regs(CurInstr.Instr & 0xFFFF);

    bool load = CurInstr.Instr & (1 << 20);
    bool pre = CurInstr.Instr & (1 << 24);
    bool add = CurInstr.Instr & (1 << 23);
    bool writeback = CurInstr.Instr & (1 << 21);
    bool usermode = CurInstr.Instr & (1 << 22);

    ARM64Reg rn = MapReg(CurInstr.A_Reg(16));

    s32 offset = Comp_MemAccessBlock(CurInstr.A_Reg(16), regs, !load, pre, !add, usermode);

    if (load && writeback && regs[CurInstr.A_Reg(16)])
        writeback = Num == 0
            ? (!(regs & ~BitSet16(1 << CurInstr.A_Reg(16)))) || (regs & ~BitSet16((2 << CurInstr.A_Reg(16)) - 1))
            : false;
    if (writeback)
    {
        if (offset > 0)
            ADD(rn, rn, offset);
        else
            SUB(rn, rn, -offset);
    }
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

    ARM64Reg sp = MapReg(13);
    s32 offset = Comp_MemAccessBlock(13, regs, !load, !load, !load, false);

    if (offset > 0)
            ADD(sp, sp, offset);
        else
            SUB(sp, sp, -offset);
}

void Compiler::T_Comp_LDMIA_STMIA()
{
    BitSet16 regs(CurInstr.Instr & 0xFF);
    ARM64Reg rb = MapReg(CurInstr.T_Reg(8));
    bool load = CurInstr.Instr & (1 << 11);
    u32 regsCount = regs.Count();
    
    s32 offset = Comp_MemAccessBlock(CurInstr.T_Reg(8), regs, !load, false, false, false);

    if (!load || !regs[CurInstr.T_Reg(8)])
    {
        if (offset > 0)
            ADD(rb, rb, offset);
        else
            SUB(rb, rb, -offset);
    }
}

}