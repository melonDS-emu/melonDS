#include "ARMJIT_Compiler.h"

#include "../Config.h"

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

s32 Compiler::RewriteMemAccess(u64 pc)
{
    return 0;
}

/*
    According to DeSmuME and my own research, approx. 99% (seriously, that's an empirical number)
    of all memory load and store instructions always access addresses in the same region as
    during the their first execution.

    I tried multiple optimisations, which would benefit from this behaviour
    (having fast paths for the first region, …), though none of them yielded a measureable
    improvement.
*/

bool Compiler::Comp_MemLoadLiteral(int size, int rd, u32 addr)
{
    return false;
    //u32 translatedAddr = Num == 0 ? TranslateAddr9(addr) : TranslateAddr7(addr);

    /*int invalidLiteralIdx = InvalidLiterals.Find(translatedAddr);
    if (invalidLiteralIdx != -1)
    {
        InvalidLiterals.Remove(invalidLiteralIdx);
        return false;
    }*/

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

    return true;
}


void Compiler::Comp_MemAccess(int rd, int rn, const ComplexOperand& op2, int size, int flags)
{
    u32 addressMask = ~0;
    if (size == 32)
        addressMask = ~3;
    if (size == 16)
        addressMask = ~1;

    if (Config::JIT_LiteralOptimisations && rn == 15 && rd != 15 && op2.IsImm && !(flags & (memop_SignExtend|memop_Post|memop_Store|memop_Writeback)))
    {
        u32 addr = R15 + op2.Imm * ((flags & memop_SubtractOffset) ? -1 : 1);
        
        if (Comp_MemLoadLiteral(size, rd, addr))
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

        bool addrIsStatic = Config::JIT_LiteralOptimisations
            && RegCache.IsLiteral(rn) && op2.IsImm && !(flags & (memop_Writeback|memop_Post));
        u32 staticAddress;
        if (addrIsStatic)
            staticAddress = RegCache.LiteralValues[rn] + op2.Imm * ((flags & memop_SubtractOffset) ? -1 : 1);
        OpArg rdMapped = MapReg(rd);

        if (true)
        {
            OpArg rnMapped = MapReg(rn);
            if (Thumb && rn == 15)
                rnMapped = Imm32(R15 & ~0x2);

            X64Reg finalAddr = RSCRATCH3;
            if (flags & memop_Post)
            {
                MOV(32, R(RSCRATCH3), rnMapped);

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
                        if (R(finalAddr) != rnMapped)
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
        }

        /*int expectedTarget = Num == 0
            ? ClassifyAddress9(addrIsStatic ? staticAddress : CurInstr.DataRegion) 
            : ClassifyAddress7(addrIsStatic ? staticAddress : CurInstr.DataRegion);
        if (CurInstr.Cond() < 0xE)
            expectedTarget = memregion_Other;

        bool compileFastPath = false, compileSlowPath = !addrIsStatic || (flags & memop_Store);

        switch (expectedTarget)
        {
        case memregion_MainRAM:
        case memregion_DTCM:
        case memregion_WRAM7:
        case memregion_SWRAM9:
        case memregion_SWRAM7:
        case memregion_IO9:
        case memregion_IO7:
        case memregion_VWRAM:
            compileFastPath = true;
            break;
        case memregion_Wifi:
            compileFastPath = size >= 16;
            break;
        case memregion_VRAM:
            compileFastPath = !(flags & memop_Store) || size >= 16;
        case memregion_BIOS9:
            compileFastPath = !(flags & memop_Store);
            break;
        default: break;
        }

        if (addrIsStatic && !compileFastPath)
        {
            compileFastPath = false;
            compileSlowPath = true;
        }

        if (addrIsStatic && compileSlowPath)
            MOV(32, R(RSCRATCH3), Imm32(staticAddress));
*/
        /*if (compileFastPath)
        {
            FixupBranch slowPath;
            if (compileSlowPath)
            {
                MOV(32, R(RSCRATCH), R(RSCRATCH3));
                SHR(32, R(RSCRATCH), Imm8(9));
                if (flags & memop_Store)
                {
                    CMP(8, MDisp(RSCRATCH, squeezePointer(Num == 0 ? MemoryStatus9 : MemoryStatus7)), Imm8(expectedTarget));
                }
                else
                {
                    MOVZX(32, 8, RSCRATCH, MDisp(RSCRATCH, squeezePointer(Num == 0 ? MemoryStatus9 : MemoryStatus7)));
                    AND(32, R(RSCRATCH), Imm8(~0x80));
                    CMP(32, R(RSCRATCH), Imm8(expectedTarget));
                }

                slowPath = J_CC(CC_NE, true);
            }

            if (expectedTarget == memregion_MainRAM || expectedTarget == memregion_WRAM7
                || expectedTarget == memregion_BIOS9)
            {
                u8* data;
                u32 mask;
                if (expectedTarget == memregion_MainRAM)
                {
                    data = NDS::MainRAM;
                    mask = MAIN_RAM_SIZE - 1;
                }
                else if (expectedTarget == memregion_BIOS9)
                {
                    data = NDS::ARM9BIOS;
                    mask = 0xFFF;
                }
                else
                {
                    data = NDS::ARM7WRAM;
                    mask = 0xFFFF;
                }
                OpArg memLoc;
                if (addrIsStatic)
                {
                    memLoc = M(data + ((staticAddress & mask & addressMask)));
                }
                else
                {
                    MOV(32, R(RSCRATCH), R(RSCRATCH3));
                    AND(32, R(RSCRATCH), Imm32(mask & addressMask));
                    memLoc = MDisp(RSCRATCH, squeezePointer(data));
                }
                if (flags & memop_Store)
                    MOV(size, memLoc, rdMapped);
                else if (flags & memop_SignExtend)
                    MOVSX(32, size, rdMapped.GetSimpleReg(), memLoc);
                else
                    MOVZX(32, size, rdMapped.GetSimpleReg(), memLoc);
            }
            else if (expectedTarget == memregion_DTCM)
            {
                if (addrIsStatic)
                    MOV(32, R(RSCRATCH), Imm32(staticAddress));
                else
                    MOV(32, R(RSCRATCH), R(RSCRATCH3));
                SUB(32, R(RSCRATCH), MDisp(RCPU, offsetof(ARMv5, DTCMBase)));
                AND(32, R(RSCRATCH), Imm32(0x3FFF & addressMask));
                OpArg memLoc = MComplex(RCPU, RSCRATCH, SCALE_1, offsetof(ARMv5, DTCM));
                if (flags & memop_Store)
                    MOV(size, memLoc, rdMapped);
                else if (flags & memop_SignExtend)
                    MOVSX(32, size, rdMapped.GetSimpleReg(), memLoc);
                else
                    MOVZX(32, size, rdMapped.GetSimpleReg(), memLoc);
            }
            else if (expectedTarget == memregion_SWRAM9 || expectedTarget == memregion_SWRAM7)
            {
                MOV(64, R(RSCRATCH2), M(expectedTarget == memregion_SWRAM9 ? &NDS::SWRAM_ARM9 : &NDS::SWRAM_ARM7));
                if (addrIsStatic)
                {
                    MOV(32, R(RSCRATCH), Imm32(staticAddress & addressMask));
                }
                else
                {
                    MOV(32, R(RSCRATCH), R(RSCRATCH3));
                    AND(32, R(RSCRATCH), Imm8(addressMask));
                }
                AND(32, R(RSCRATCH), M(expectedTarget == memregion_SWRAM9 ? &NDS::SWRAM_ARM9Mask : &NDS::SWRAM_ARM7Mask));
                OpArg memLoc = MRegSum(RSCRATCH, RSCRATCH2);
                if (flags & memop_Store)
                    MOV(size, memLoc, rdMapped);
                else if (flags & memop_SignExtend)
                    MOVSX(32, size, rdMapped.GetSimpleReg(), memLoc);
                else
                    MOVZX(32, size, rdMapped.GetSimpleReg(), memLoc);
            }
            else
            {
                u32 maskedDataRegion;

                if (addrIsStatic)
                {
                    maskedDataRegion = staticAddress;
                    MOV(32, R(ABI_PARAM1), Imm32(staticAddress));
                }
                else
                {
                    if (ABI_PARAM1 != RSCRATCH3)
                        MOV(32, R(ABI_PARAM1), R(RSCRATCH3));
                    AND(32, R(ABI_PARAM1), Imm8(addressMask));

                    maskedDataRegion = CurInstr.DataRegion;
                    if (Num == 0)
                        maskedDataRegion &= ~0xFFFFFF;
                    else
                        maskedDataRegion &= ~0x7FFFFF;
                }

                void* func = GetFuncForAddr(CurCPU, maskedDataRegion, flags & memop_Store, size);

                if (flags & memop_Store)
                {
                    PushRegs(false);

                    MOV(32, R(ABI_PARAM2), rdMapped);

                    ABI_CallFunction((void(*)())func);

                    PopRegs(false);
                }
                else
                {
                    if (!addrIsStatic)
                        MOV(32, rdMapped, R(RSCRATCH3));

                    PushRegs(false);

                    ABI_CallFunction((void(*)())func);

                    PopRegs(false);

                    if (!addrIsStatic)
                        MOV(32, R(RSCRATCH3), rdMapped);

                    if (flags & memop_SignExtend)
                        MOVSX(32, size, rdMapped.GetSimpleReg(), R(RSCRATCH));
                    else
                        MOVZX(32, size, rdMapped.GetSimpleReg(), R(RSCRATCH));
                }
            }

            if ((size == 32 && !(flags & memop_Store)))
            {
                if (addrIsStatic)
                {
                    if (staticAddress & 0x3)
                        ROR_(32, rdMapped, Imm8((staticAddress & 0x3) * 8));
                }
                else
                {
                    AND(32, R(RSCRATCH3), Imm8(0x3));
                    SHL(32, R(RSCRATCH3), Imm8(3));
                    ROR_(32, rdMapped, R(RSCRATCH3));
                }
            }

            if (compileSlowPath)
            {
                SwitchToFarCode();
                SetJumpTarget(slowPath);
            }
        }
*/
        if (true)
        {
            PushRegs(false);

            if (Num == 0)
            {
                MOV(64, R(ABI_PARAM2), R(RCPU));
                if (ABI_PARAM1 != RSCRATCH3)
                    MOV(32, R(ABI_PARAM1), R(RSCRATCH3));
                if (flags & memop_Store)
                {
                    MOV(32, R(ABI_PARAM3), rdMapped);

                    switch (size)
                    {
                    case 32: CALL((void*)&SlowWrite9<u32>); break;
                    case 16: CALL((void*)&SlowWrite9<u16>); break;
                    case 8: CALL((void*)&SlowWrite9<u8>); break;
                    }
                }
                else
                {
                    switch (size)
                    {
                    case 32: CALL((void*)&SlowRead9<u32>); break;
                    case 16: CALL((void*)&SlowRead9<u16>); break;
                    case 8: CALL((void*)&SlowRead9<u8>); break;
                    }
                }
            }
            else
            {
                if (ABI_PARAM1 != RSCRATCH3)
                    MOV(32, R(ABI_PARAM1), R(RSCRATCH3));
                if (flags & memop_Store)
                {
                    MOV(32, R(ABI_PARAM2), rdMapped);

                    switch (size)
                    {
                    case 32: CALL((void*)&SlowWrite7<u32>); break;
                    case 16: CALL((void*)&SlowWrite7<u16>); break;
                    case 8: CALL((void*)&SlowWrite7<u8>); break;
                    }
                }
                else
                {
                    switch (size)
                    {
                    case 32: CALL((void*)&SlowRead7<u32>); break;
                    case 16: CALL((void*)&SlowRead7<u16>); break;
                    case 8: CALL((void*)&SlowRead7<u8>); break;
                    }
                }
            }

            PopRegs(false);

            if (!(flags & memop_Store))
            {
                if (flags & memop_SignExtend)
                    MOVSX(32, size, rdMapped.GetSimpleReg(), R(RSCRATCH));
                else
                    MOVZX(32, size, rdMapped.GetSimpleReg(), R(RSCRATCH));
            }
        }
/*
        if (compileFastPath && compileSlowPath)
        {
            FixupBranch ret = J(true);
            SwitchToNearCode();
            SetJumpTarget(ret);
        }*/

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
    int regsCount = regs.Count();

    s32 offset = (regsCount * 4) * (decrement ? -1 : 1);

    // we need to make sure that the stack stays aligned to 16 bytes
#ifdef _WIN32
    // include shadow
    u32 stackAlloc = ((regsCount + 4 + 1) & ~1) * 8;
#else
    u32 stackAlloc = ((regsCount + 1) & ~1) * 8;
#endif
    u32 allocOffset = stackAlloc - regsCount * 8;
/*
    int expectedTarget = Num == 0
        ? ClassifyAddress9(CurInstr.DataRegion)
        : ClassifyAddress7(CurInstr.DataRegion);
    if (usermode || CurInstr.Cond() < 0xE)
        expectedTarget = memregion_Other;

    bool compileFastPath = false;

    switch (expectedTarget)
    {
    case memregion_DTCM:
    case memregion_MainRAM:
    case memregion_SWRAM9:
    case memregion_SWRAM7:
    case memregion_WRAM7:
        compileFastPath = true;
        break;
    default:
        break;
    }
*/
    if (!store)
        Comp_AddCycles_CDI();
    else
        Comp_AddCycles_CD();

    if (decrement)
    {
        MOV_sum(32, RSCRATCH4, MapReg(rn), Imm32(-regsCount * 4));
        preinc ^= true;
    }
    else
        MOV(32, R(RSCRATCH4), MapReg(rn));
/*
    if (compileFastPath)
    {
        assert(!usermode);

        MOV(32, R(RSCRATCH), R(RSCRATCH4));
        SHR(32, R(RSCRATCH), Imm8(9));

        if (store)
        {
            CMP(8, MDisp(RSCRATCH, squeezePointer(Num == 0 ? MemoryStatus9 : MemoryStatus7)), Imm8(expectedTarget));
        }
        else
        {
            MOVZX(32, 8, RSCRATCH, MDisp(RSCRATCH, squeezePointer(Num == 0 ? MemoryStatus9 : MemoryStatus7)));
            AND(32, R(RSCRATCH), Imm8(~0x80));
            CMP(32, R(RSCRATCH), Imm8(expectedTarget));
        }
        FixupBranch slowPath = J_CC(CC_NE, true);

        if (expectedTarget == memregion_DTCM)
        {
            SUB(32, R(RSCRATCH4), MDisp(RCPU, offsetof(ARMv5, DTCMBase)));
            AND(32, R(RSCRATCH4), Imm32(0x3FFF & ~3));
            LEA(64, RSCRATCH4, MComplex(RCPU, RSCRATCH4, 1, offsetof(ARMv5, DTCM)));
        }
        else if (expectedTarget == memregion_MainRAM)
        {
            AND(32, R(RSCRATCH4), Imm32((MAIN_RAM_SIZE - 1) & ~3));
            ADD(64, R(RSCRATCH4), Imm32(squeezePointer(NDS::MainRAM)));
        }
        else if (expectedTarget == memregion_WRAM7)
        {
            AND(32, R(RSCRATCH4), Imm32(0xFFFF & ~3));
            ADD(64, R(RSCRATCH4), Imm32(squeezePointer(NDS::ARM7WRAM)));
        }
        else // SWRAM
        {
            AND(32, R(RSCRATCH4), Imm8(~3));
            AND(32, R(RSCRATCH4), M(expectedTarget == memregion_SWRAM9 ? &NDS::SWRAM_ARM9Mask : &NDS::SWRAM_ARM7Mask));
            ADD(64, R(RSCRATCH4), M(expectedTarget == memregion_SWRAM9 ? &NDS::SWRAM_ARM9 : &NDS::SWRAM_ARM7));
        }
        u32 offset = 0;
        for (int reg : regs)
        {
            if (preinc)
                offset += 4;
            OpArg mem = MDisp(RSCRATCH4, offset);
            if (store)
            {
                if (RegCache.LoadedRegs & (1 << reg))
                {
                    MOV(32, mem, MapReg(reg));
                }
                else
                {
                    LoadReg(reg, RSCRATCH);
                    MOV(32, mem, R(RSCRATCH));
                }
            }
            else
            {
                if (RegCache.LoadedRegs & (1 << reg))
                {
                    MOV(32, MapReg(reg), mem);
                }
                else
                {
                    MOV(32, R(RSCRATCH), mem);
                    SaveReg(reg, RSCRATCH);
                }
            }
            if (!preinc)
                offset += 4;
        }

        SwitchToFarCode();
        SetJumpTarget(slowPath);
    }*/

    if (!store)
    {
        PushRegs(false);

        MOV(32, R(ABI_PARAM1), R(RSCRATCH4));
        MOV(32, R(ABI_PARAM3), Imm32(regsCount));
        SUB(64, R(RSP), stackAlloc <= INT8_MAX ? Imm8(stackAlloc) : Imm32(stackAlloc));
        if (allocOffset == 0)
            MOV(64, R(ABI_PARAM2), R(RSP));
        else
            LEA(64, ABI_PARAM2, MDisp(RSP, allocOffset));

        if (Num == 0)
            MOV(64, R(ABI_PARAM4), R(RCPU));

        switch (Num * 2 | preinc)
        {
        case 0: CALL((void*)&SlowBlockTransfer9<false, false>); break;
        case 1: CALL((void*)&SlowBlockTransfer9<true, false>); break;
        case 2: CALL((void*)&SlowBlockTransfer7<false, false>); break;
        case 3: CALL((void*)&SlowBlockTransfer7<true, false>); break;
        }

        PopRegs(false);

        if (allocOffset)
            ADD(64, R(RSP), Imm8(allocOffset));

        bool firstUserMode = true;
        for (int reg : regs)
        {
            if (usermode && !regs[15] && reg >= 8 && reg < 15)
            {
                if (firstUserMode)
                {
                    MOV(32, R(RSCRATCH), R(RCPSR));
                    AND(32, R(RSCRATCH), Imm8(0x1F));
                    firstUserMode = false;
                }
                MOV(32, R(RSCRATCH2), Imm32(reg - 8));
                POP(RSCRATCH3);
                CALL(WriteBanked);
                FixupBranch sucessfulWritten = J_CC(CC_NC);
                if (RegCache.LoadedRegs & (1 << reg))
                    MOV(32, R(RegCache.Mapping[reg]), R(RSCRATCH3));
                else
                    SaveReg(reg, RSCRATCH3);
                SetJumpTarget(sucessfulWritten);
            }
            else if (!(RegCache.LoadedRegs & (1 << reg)))
            {
                assert(reg != 15);

                POP(RSCRATCH);
                SaveReg(reg, RSCRATCH);
            }
            else
            {
                POP(MapReg(reg).GetSimpleReg());
            }
        }
    }
    else
    {
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
                    if (RegCache.Mapping[reg] == INVALID_REG)
                        LoadReg(reg, RSCRATCH3);
                    else
                        MOV(32, R(RSCRATCH3), R(RegCache.Mapping[reg]));
                    MOV(32, R(RSCRATCH2), Imm32(reg - 8));
                    CALL(ReadBanked);
                    PUSH(RSCRATCH3);
                }
                else if (!(RegCache.LoadedRegs & (1 << reg)))
                {
                    LoadReg(reg, RSCRATCH);
                    PUSH(RSCRATCH);
                }
                else
                {
                    PUSH(MapReg(reg).GetSimpleReg());
                }
            }
        }

        if (allocOffset)
            SUB(64, R(RSP), Imm8(allocOffset));

        PushRegs(false);

        MOV(32, R(ABI_PARAM1), R(RSCRATCH4));
        if (allocOffset)
            LEA(64, ABI_PARAM2, MDisp(RSP, allocOffset));
        else
            MOV(64, R(ABI_PARAM2), R(RSP));
        
        MOV(32, R(ABI_PARAM3), Imm32(regsCount));
        if (Num == 0)
            MOV(64, R(ABI_PARAM4), R(RCPU));

        switch (Num * 2 | preinc)
        {
        case 0: CALL((void*)&SlowBlockTransfer9<false, true>); break;
        case 1: CALL((void*)&SlowBlockTransfer9<true, true>); break;
        case 2: CALL((void*)&SlowBlockTransfer7<false, true>); break;
        case 3: CALL((void*)&SlowBlockTransfer7<true, true>); break;
        }

        ADD(64, R(RSP), stackAlloc <= INT8_MAX ? Imm8(stackAlloc) : Imm32(stackAlloc));
    
        PopRegs(false);
    }
/*
    if (compileFastPath)
    {
        FixupBranch ret = J(true);
        SwitchToNearCode();
        SetJumpTarget(ret);
    }*/

    if (!store && regs[15])
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
    u32 offset = (CurInstr.Instr & 0xFF) << 2;
    u32 addr = (R15 & ~0x2) + offset;
    if (!Config::JIT_LiteralOptimisations || !Comp_MemLoadLiteral(32, CurInstr.T_Reg(8), addr))
        Comp_MemAccess(CurInstr.T_Reg(8), 15, ComplexOperand(offset), 32, 0);
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