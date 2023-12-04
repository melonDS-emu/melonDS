/*
    Copyright 2016-2023 melonDS team

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
#include "../ARMJIT.h"
#include "../NDS.h"

using namespace Gen;

namespace melonDS
{

template <typename T>
int squeezePointer(T* ptr)
{
    int truncated = (int)((u64)ptr);
    assert((T*)((u64)truncated) == ptr);
    return truncated;
}

u8* Compiler::RewriteMemAccess(u8* pc)
{
    auto it = LoadStorePatches.find(pc);
    if (it != LoadStorePatches.end())
    {
        LoadStorePatch patch = it->second;
        LoadStorePatches.erase(it);

        //printf("rewriting memory access %p %d %d\n", (u8*)pc-ResetStart, patch.Offset, patch.Size);

        XEmitter emitter(pc + (ptrdiff_t)patch.Offset);
        emitter.CALL(patch.PatchFunc);
        ptrdiff_t remainingSize = (ptrdiff_t)patch.Size - 5;
        assert(remainingSize >= 0);
        if (remainingSize > 0)
            emitter.NOP(remainingSize);

        return pc + (ptrdiff_t)patch.Offset;
    }

    Log(LogLevel::Error, "this is a JIT bug %sx\n", pc);
    abort();
}

/*
    According to DeSmuME and my own research, approx. 99% (seriously, that's an empirical number)
    of all memory load and store instructions always access addresses in the same region as
    during the their first execution.

    I tried multiple optimisations, which would benefit from this behaviour
    (having fast paths for the first region, …), though none of them yielded a measureable
    improvement.
*/

bool Compiler::Comp_MemLoadLiteral(int size, bool signExtend, int rd, u32 addr)
{
    u32 localAddr = NDS.JIT.LocaliseCodeAddress(Num, addr);

    int invalidLiteralIdx = NDS.JIT.InvalidLiterals.Find(localAddr);
    if (invalidLiteralIdx != -1)
    {
        return false;
    }

    Comp_AddCycles_CDI();

    u32 val;
    // make sure arm7 bios is accessible
    u32 tmpR15 = CurCPU->R[15];
    CurCPU->R[15] = R15;
    if (size == 32)
    {
        CurCPU->DataRead32(addr & ~0x3, &val);
        val = melonDS::ROR(val, (addr & 0x3) << 3);
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

    MOV(32, MapReg(rd), Imm32(val));

    if (Thumb || CurInstr.Cond() == 0xE)
        RegCache.PutLiteral(rd, val);

    return true;
}


void Compiler::Comp_MemAccess(int rd, int rn, const Op2& op2, int size, int flags)
{
    u32 addressMask = ~0;
    if (size == 32)
        addressMask = ~3;
    if (size == 16)
        addressMask = ~1;

    if (NDS.JIT.LiteralOptimizationsEnabled() && rn == 15 && rd != 15 && op2.IsImm && !(flags & (memop_Post|memop_Store|memop_Writeback)))
    {
        u32 addr = R15 + op2.Imm * ((flags & memop_SubtractOffset) ? -1 : 1);

        if (Comp_MemLoadLiteral(size, flags & memop_SignExtend, rd, addr))
            return;
    }

    if (flags & memop_Store)
    {
        Comp_AddCycles_CD();
    }
    else
    {
        Comp_AddCycles_CDI();
    }

    bool addrIsStatic = NDS.JIT.LiteralOptimizationsEnabled()
        && RegCache.IsLiteral(rn) && op2.IsImm && !(flags & (memop_Writeback|memop_Post));
    u32 staticAddress;
    if (addrIsStatic)
        staticAddress = RegCache.LiteralValues[rn] + op2.Imm * ((flags & memop_SubtractOffset) ? -1 : 1);
    OpArg rdMapped = MapReg(rd);

    OpArg rnMapped = MapReg(rn);
    if (Thumb && rn == 15)
        rnMapped = Imm32(R15 & ~0x2);

    if (flags & memop_Store && flags & (memop_Post|memop_Writeback) && rd == rn)
    {
        MOV(32, R(RSCRATCH4), rdMapped);
        rdMapped = R(RSCRATCH4);
    }

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

    u32 expectedTarget = Num == 0
        ? NDS.JIT.Memory.ClassifyAddress9(CurInstr.DataRegion)
        : NDS.JIT.Memory.ClassifyAddress7(CurInstr.DataRegion);

    if (NDS.JIT.FastMemoryEnabled() && ((!Thumb && CurInstr.Cond() != 0xE) || NDS.JIT.Memory.IsFastmemCompatible(expectedTarget)))
    {
        if (rdMapped.IsImm())
        {
            MOV(32, R(RSCRATCH4), rdMapped);
            rdMapped = R(RSCRATCH4);
        }

        u8* memopStart = GetWritableCodePtr();
        LoadStorePatch patch;

        assert(rdMapped.GetSimpleReg() >= 0 && rdMapped.GetSimpleReg() < 16);
        patch.PatchFunc = flags & memop_Store
            ? PatchedStoreFuncs[NDS.ConsoleType][Num][__builtin_ctz(size) - 3][rdMapped.GetSimpleReg()]
            : PatchedLoadFuncs[NDS.ConsoleType][Num][__builtin_ctz(size) - 3][!!(flags & memop_SignExtend)][rdMapped.GetSimpleReg()];

        assert(patch.PatchFunc != NULL);

        MOV(64, R(RSCRATCH), ImmPtr(Num == 0 ? NDS.JIT.Memory.FastMem9Start : NDS.JIT.Memory.FastMem7Start));

        X64Reg maskedAddr = RSCRATCH3;
        if (size > 8)
        {
            maskedAddr = RSCRATCH2;
            MOV(32, R(RSCRATCH2), R(RSCRATCH3));
            AND(32, R(RSCRATCH2), Imm8(addressMask));
        }

        u8* memopLoadStoreLocation = GetWritableCodePtr();
        if (flags & memop_Store)
        {
            MOV(size, MRegSum(RSCRATCH, maskedAddr), rdMapped);
        }
        else
        {
            if (flags & memop_SignExtend)
                MOVSX(32, size, rdMapped.GetSimpleReg(), MRegSum(RSCRATCH, maskedAddr));
            else
                MOVZX(32, size, rdMapped.GetSimpleReg(), MRegSum(RSCRATCH, maskedAddr));

            if (size == 32)
            {
                if (addrIsStatic)
                {
                    if (staticAddress & 0x3)
                        ROR(32, rdMapped, Imm8((staticAddress & 0x3) * 8));
                }
                else
                {
                    AND(32, R(RSCRATCH3), Imm8(0x3));
                    SHL(32, R(RSCRATCH3), Imm8(3));
                    ROR(32, rdMapped, R(RSCRATCH3));
                }
            }
        }

        patch.Offset = memopStart - memopLoadStoreLocation;
        patch.Size = GetWritableCodePtr() - memopStart;

        assert(patch.Size >= 5);

        LoadStorePatches[memopLoadStoreLocation] = patch;
    }
    else
    {
        PushRegs(false, false);

        void* func = NULL;
        if (addrIsStatic)
            func = NDS.JIT.Memory.GetFuncForAddr(CurCPU, staticAddress, flags & memop_Store, size);

        if (func)
        {
            AND(32, R(RSCRATCH3), Imm8(addressMask));

            if (ABI_PARAM1 != RSCRATCH3)
                MOV(32, R(ABI_PARAM1), R(RSCRATCH3));
            if (flags & memop_Store)
                MOV(32, R(ABI_PARAM2), rdMapped);

            ABI_CallFunction((void (*)())func);

            PopRegs(false, false);

            if (!(flags & memop_Store))
            {
                if (size == 32)
                {
                    MOV(32, rdMapped, R(RSCRATCH));
                    if (staticAddress & 0x3)
                        ROR(32, rdMapped, Imm8((staticAddress & 0x3) * 8));
                }
                else
                {
                    if (flags & memop_SignExtend)
                        MOVSX(32, size, rdMapped.GetSimpleReg(), R(RSCRATCH));
                    else
                        MOVZX(32, size, rdMapped.GetSimpleReg(), R(RSCRATCH));
                }
            }
        }
        else
        {
            if (Num == 0)
            {
                // on Windows param 3 is R8 which is also scratch 4 which can be used for rd
                if (flags & memop_Store)
                    MOV(32, R(ABI_PARAM3), rdMapped);

                MOV(64, R(ABI_PARAM2), R(RCPU));
                if (ABI_PARAM1 != RSCRATCH3)
                    MOV(32, R(ABI_PARAM1), R(RSCRATCH3));
                if (flags & memop_Store)
                {
                    switch (size | NDS.ConsoleType)
                    {
                    case 32: CALL((void*)&SlowWrite9<u32, 0>); break;
                    case 16: CALL((void*)&SlowWrite9<u16, 0>); break;
                    case 8: CALL((void*)&SlowWrite9<u8, 0>); break;
                    case 33: CALL((void*)&SlowWrite9<u32, 1>); break;
                    case 17: CALL((void*)&SlowWrite9<u16, 1>); break;
                    case 9: CALL((void*)&SlowWrite9<u8, 1>); break;
                    }
                }
                else
                {
                    switch (size | NDS.ConsoleType)
                    {
                    case 32: CALL((void*)&SlowRead9<u32, 0>); break;
                    case 16: CALL((void*)&SlowRead9<u16, 0>); break;
                    case 8: CALL((void*)&SlowRead9<u8, 0>); break;
                    case 33: CALL((void*)&SlowRead9<u32, 1>); break;
                    case 17: CALL((void*)&SlowRead9<u16, 1>); break;
                    case 9: CALL((void*)&SlowRead9<u8, 1>); break;
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

                    switch (size | NDS.ConsoleType)
                    {
                    case 32: CALL((void*)&SlowWrite7<u32, 0>); break;
                    case 16: CALL((void*)&SlowWrite7<u16, 0>); break;
                    case 8: CALL((void*)&SlowWrite7<u8, 0>); break;
                    case 33: CALL((void*)&SlowWrite7<u32, 1>); break;
                    case 17: CALL((void*)&SlowWrite7<u16, 1>); break;
                    case 9: CALL((void*)&SlowWrite7<u8, 1>); break;
                    }
                }
                else
                {
                    switch (size | NDS.ConsoleType)
                    {
                    case 32: CALL((void*)&SlowRead7<u32, 0>); break;
                    case 16: CALL((void*)&SlowRead7<u16, 0>); break;
                    case 8: CALL((void*)&SlowRead7<u8, 0>); break;
                    case 33: CALL((void*)&SlowRead7<u32, 1>); break;
                    case 17: CALL((void*)&SlowRead7<u16, 1>); break;
                    case 9: CALL((void*)&SlowRead7<u8, 1>); break;
                    }
                }
            }

            PopRegs(false, false);

            if (!(flags & memop_Store))
            {
                if (flags & memop_SignExtend)
                    MOVSX(32, size, rdMapped.GetSimpleReg(), R(RSCRATCH));
                else
                    MOVZX(32, size, rdMapped.GetSimpleReg(), R(RSCRATCH));
            }
        }
    }

    if (!(flags & memop_Store) && rd == 15)
    {
        if (size < 32)
            Log(LogLevel::Debug, "!!! LDR <32 bit PC %08X %x\n", R15, CurInstr.Instr);
        {
            if (Num == 1)
            {
                if (Thumb)
                    OR(32, rdMapped, Imm8(0x1));
                else
                    AND(32, rdMapped, Imm8(0xFE));
            }
            Comp_JumpTo(rdMapped.GetSimpleReg());
        }
    }
}

s32 Compiler::Comp_MemAccessBlock(int rn, BitSet16 regs, bool store, bool preinc, bool decrement, bool usermode, bool skipLoadingRn)
{
    int regsCount = regs.Count();

    if (regsCount == 0)
        return 0; // actually not the right behaviour TODO: fix me

    int firstReg = *regs.begin();
    if (regsCount == 1 && !usermode && RegCache.LoadedRegs & (1 << firstReg) && !(firstReg == rn && skipLoadingRn))
    {
        int flags = 0;
        if (store)
            flags |= memop_Store;
        if (decrement && preinc)
            flags |= memop_SubtractOffset;
        Op2 offset = preinc ? Op2(4) : Op2(0);

        Comp_MemAccess(firstReg, rn, offset, 32, flags);

        return decrement ? -4 : 4;
    }

    s32 offset = (regsCount * 4) * (decrement ? -1 : 1);

    int expectedTarget = Num == 0
        ? NDS.JIT.Memory.ClassifyAddress9(CurInstr.DataRegion)
        : NDS.JIT.Memory.ClassifyAddress7(CurInstr.DataRegion);

    if (!store)
        Comp_AddCycles_CDI();
    else
        Comp_AddCycles_CD();

    bool compileFastPath = NDS.JIT.FastMemoryEnabled()
        && !usermode && (CurInstr.Cond() < 0xE || NDS.JIT.Memory.IsFastmemCompatible(expectedTarget));

    // we need to make sure that the stack stays aligned to 16 bytes
#ifdef _WIN32
    // include shadow
    u32 stackAlloc = (((regsCount + 4 + 1) & ~1) + (compileFastPath ? 1 : 0)) * 8;
#else
    u32 stackAlloc = (((regsCount + 1) & ~1) + (compileFastPath ? 1 : 0)) * 8;
#endif
    u32 allocOffset = stackAlloc - regsCount * 8;

    if (decrement)
        MOV_sum(32, RSCRATCH4, MapReg(rn), Imm32(-regsCount * 4 + (preinc ? 0 : 4)));
    else
        MOV_sum(32, RSCRATCH4, MapReg(rn), Imm32(preinc ? 4 : 0));

    if (compileFastPath)
    {
        AND(32, R(RSCRATCH4), Imm8(~3));

        u8* fastPathStart = GetWritableCodePtr();
        u8* loadStoreAddr[16];

        MOV(64, R(RSCRATCH2), ImmPtr(Num == 0 ? NDS.JIT.Memory.FastMem9Start : NDS.JIT.Memory.FastMem7Start));
        ADD(64, R(RSCRATCH2), R(RSCRATCH4));

        u32 offset = 0;
        int i = 0;
        for (int reg : regs)
        {
            loadStoreAddr[i] = GetWritableCodePtr();

            OpArg mem = MDisp(RSCRATCH2, offset);
            if (store)
            {
                if (RegCache.LoadedRegs & (1 << reg))
                {
                    MOV(32, mem, MapReg(reg));
                }
                else
                {
                    LoadReg(reg, RSCRATCH);
                    loadStoreAddr[i] = GetWritableCodePtr();
                    MOV(32, mem, R(RSCRATCH));
                }
            }
            else
            {
                if (RegCache.LoadedRegs & (1 << reg))
                {
                    if (!(reg == rn && skipLoadingRn))
                        MOV(32, MapReg(reg), mem);
                    else
                        MOV(32, R(RSCRATCH), mem); // just touch the memory
                }
                else
                {
                    MOV(32, R(RSCRATCH), mem);
                    SaveReg(reg, RSCRATCH);
                }
            }
            offset += 4;
            i++;
        }

        LoadStorePatch patch;
        patch.Size = GetWritableCodePtr() - fastPathStart;
        SwitchToFarCode();
        patch.PatchFunc = GetWritableCodePtr();

        for (i = 0; i < regsCount; i++)
        {
            patch.Offset = fastPathStart - loadStoreAddr[i];
            LoadStorePatches[loadStoreAddr[i]] = patch;
        }
    }

    if (!store)
    {
        PushRegs(false, false, !compileFastPath);

        MOV(32, R(ABI_PARAM1), R(RSCRATCH4));
        MOV(32, R(ABI_PARAM3), Imm32(regsCount));
        SUB(64, R(RSP), stackAlloc <= INT8_MAX ? Imm8(stackAlloc) : Imm32(stackAlloc));
        if (allocOffset == 0)
            MOV(64, R(ABI_PARAM2), R(RSP));
        else
            LEA(64, ABI_PARAM2, MDisp(RSP, allocOffset));

        if (Num == 0)
            MOV(64, R(ABI_PARAM4), R(RCPU));

        switch (Num * 2 | NDS.ConsoleType)
        {
        case 0: CALL((void*)&SlowBlockTransfer9<false, 0>); break;
        case 1: CALL((void*)&SlowBlockTransfer9<false, 1>); break;
        case 2: CALL((void*)&SlowBlockTransfer7<false, 0>); break;
        case 3: CALL((void*)&SlowBlockTransfer7<false, 1>); break;
        }

        PopRegs(false, false);

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
                if (!(reg == rn && skipLoadingRn))
                {
                    FixupBranch sucessfulWritten = J_CC(CC_NC);
                    if (RegCache.LoadedRegs & (1 << reg))
                            MOV(32, R(RegCache.Mapping[reg]), R(RSCRATCH3));
                    else
                        SaveReg(reg, RSCRATCH3);
                    SetJumpTarget(sucessfulWritten);
                }
            }
            else if (!(RegCache.LoadedRegs & (1 << reg)))
            {
                assert(reg != 15);

                POP(RSCRATCH);
                SaveReg(reg, RSCRATCH);
            }
            else if (reg == rn && skipLoadingRn)
            {
                ADD(64, R(RSP), Imm8(8));
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

        PushRegs(false, false, !compileFastPath);

        MOV(32, R(ABI_PARAM1), R(RSCRATCH4));
        if (allocOffset)
            LEA(64, ABI_PARAM2, MDisp(RSP, allocOffset));
        else
            MOV(64, R(ABI_PARAM2), R(RSP));

        MOV(32, R(ABI_PARAM3), Imm32(regsCount));
        if (Num == 0)
            MOV(64, R(ABI_PARAM4), R(RCPU));

        switch (Num * 2 | NDS.ConsoleType)
        {
        case 0: CALL((void*)&SlowBlockTransfer9<true, 0>); break;
        case 1: CALL((void*)&SlowBlockTransfer9<true, 1>); break;
        case 2: CALL((void*)&SlowBlockTransfer7<true, 0>); break;
        case 3: CALL((void*)&SlowBlockTransfer7<true, 1>); break;
        }

        ADD(64, R(RSP), stackAlloc <= INT8_MAX ? Imm8(stackAlloc) : Imm32(stackAlloc));

        PopRegs(false, false);
    }

    if (compileFastPath)
    {
        RET();
        SwitchToNearCode();
    }

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

    Op2 offset;
    if (!(CurInstr.Instr & (1 << 25)))
    {
        offset = Op2(CurInstr.Instr & 0xFFF);
    }
    else
    {
        int op = (CurInstr.Instr >> 5) & 0x3;
        int amount = (CurInstr.Instr >> 7) & 0x1F;
        int rm = CurInstr.A_Reg(0);

        offset = Op2(rm, op, amount);
    }

    Comp_MemAccess(CurInstr.A_Reg(12), CurInstr.A_Reg(16), offset, size, flags);
}

void Compiler::A_Comp_MemHalf()
{
    Op2 offset = CurInstr.Instr & (1 << 22)
        ? Op2(CurInstr.Instr & 0xF | ((CurInstr.Instr >> 4) & 0xF0))
        : Op2(CurInstr.A_Reg(0), 0, 0);

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

    Comp_MemAccess(CurInstr.T_Reg(0), CurInstr.T_Reg(3), Op2(CurInstr.T_Reg(6), 0, 0),
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

    if (load && writeback && regs[CurInstr.A_Reg(16)])
        writeback = Num == 0
            && (!(regs & ~BitSet16(1 << CurInstr.A_Reg(16)))) || (regs & ~BitSet16((2 << CurInstr.A_Reg(16)) - 1));

    s32 offset = Comp_MemAccessBlock(CurInstr.A_Reg(16), regs, !load, pre, !add, usermode, load && writeback);

    if (writeback && offset)
        ADD(32, rn, Imm32(offset));
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

    Comp_MemAccess(CurInstr.T_Reg(0), CurInstr.T_Reg(3), Op2(CurInstr.T_Reg(6), 0, 0),
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
    u32 offset = (CurInstr.Instr & 0xFF) << 2;
    u32 addr = (R15 & ~0x2) + offset;
    if (!NDS.JIT.LiteralOptimizationsEnabled() || !Comp_MemLoadLiteral(32, false, CurInstr.T_Reg(8), addr))
        Comp_MemAccess(CurInstr.T_Reg(8), 15, Op2(offset), 32, 0);
}

void Compiler::T_Comp_MemSPRel()
{
    u32 offset = (CurInstr.Instr & 0xFF) * 4;
    bool load = CurInstr.Instr & (1 << 11);

    Comp_MemAccess(CurInstr.T_Reg(8), 13, Op2(offset), 32,
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
    s32 offset = Comp_MemAccessBlock(13, regs, !load, !load, !load, false, false);

    if (offset)
        ADD(32, sp, Imm8(offset)); // offset will be always be in range since PUSH accesses 9 regs max
}

void Compiler::T_Comp_LDMIA_STMIA()
{
    BitSet16 regs(CurInstr.Instr & 0xFF);
    OpArg rb = MapReg(CurInstr.T_Reg(8));
    bool load = CurInstr.Instr & (1 << 11);

    bool writeback = !load || !regs[CurInstr.T_Reg(8)];

    s32 offset = Comp_MemAccessBlock(CurInstr.T_Reg(8), regs, !load, false, false, false, load && writeback);

    if (writeback && offset)
        ADD(32, rb, Imm8(offset));
}

}
