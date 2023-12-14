/*
    Copyright 2016-2022 melonDS team, RSDuck

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

#include "../ARMJIT_Memory.h"
#include "../NDS.h"

using namespace Arm64Gen;

namespace melonDS
{

bool Compiler::IsJITFault(const u8* pc)
{
    return (u64)pc >= (u64)GetRXBase() && (u64)pc - (u64)GetRXBase() < (JitMemMainSize + JitMemSecondarySize);
}

u8* Compiler::RewriteMemAccess(u8* pc)
{
    ptrdiff_t pcOffset = pc - GetRXBase();

    auto it = LoadStorePatches.find(pcOffset);

    if (it != LoadStorePatches.end())
    {
        LoadStorePatch patch = it->second;
        LoadStorePatches.erase(it);

        ptrdiff_t curCodeOffset = GetCodeOffset();

        SetCodePtrUnsafe(pcOffset + patch.PatchOffset);

        BL(patch.PatchFunc);
        for (int i = 0; i < patch.PatchSize / 4 - 1; i++)
            HINT(HINT_NOP);
        FlushIcacheSection((u8*)pc + patch.PatchOffset, (u8*)GetRXPtr());

        SetCodePtrUnsafe(curCodeOffset);

        return pc + (ptrdiff_t)patch.PatchOffset;
    }
    Log(LogLevel::Error, "this is a JIT bug! %08x\n", __builtin_bswap32(*(u32*)pc));
    abort();
}

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

    MOVI2R(MapReg(rd), val);

    if (Thumb || CurInstr.Cond() == 0xE)
        RegCache.PutLiteral(rd, val);
    
    return true;
}

void Compiler::Comp_MemAccess(int rd, int rn, Op2 offset, int size, int flags)
{
    u32 addressMask = ~0;
    if (size == 32)
        addressMask = ~3;
    if (size == 16)
        addressMask = ~1;

    if (NDS.JIT.LiteralOptimizationsEnabled() && rn == 15 && rd != 15 && offset.IsImm && !(flags & (memop_Post|memop_Store|memop_Writeback)))
    {
        u32 addr = R15 + offset.Imm * ((flags & memop_SubtractOffset) ? -1 : 1);
        
        if (Comp_MemLoadLiteral(size, flags & memop_SignExtend, rd, addr))
            return;
    }
    
    if (flags & memop_Store)
        Comp_AddCycles_CD();
    else
        Comp_AddCycles_CDI();

    ARM64Reg rdMapped = MapReg(rd);
    ARM64Reg rnMapped = MapReg(rn);

    if (Thumb && rn == 15)
    {
        ANDI2R(W3, rnMapped, ~2);
        rnMapped = W3;
    }

    if (flags & memop_Store && flags & (memop_Post|memop_Writeback) && rd == rn)
    {
        MOV(W4, rdMapped);
        rdMapped = W4;
    }

    ARM64Reg finalAddr = W0;
    if (flags & memop_Post)
    {
        finalAddr = rnMapped;
        MOV(W0, rnMapped);
    }

    bool addrIsStatic = NDS.JIT.LiteralOptimizationsEnabled()
        && RegCache.IsLiteral(rn) && offset.IsImm && !(flags & (memop_Writeback|memop_Post));
    u32 staticAddress;
    if (addrIsStatic)
        staticAddress = RegCache.LiteralValues[rn] + offset.Imm * ((flags & memop_SubtractOffset) ? -1 : 1);

    if (!offset.IsImm)
        Comp_RegShiftImm(offset.Reg.ShiftType, offset.Reg.ShiftAmount, false, offset, W2);
    // offset might has become an immediate
    if (offset.IsImm)
    {
        if (offset.Imm)
        {
            if (flags & memop_SubtractOffset)
                SUB(finalAddr, rnMapped, offset.Imm);
            else
                ADD(finalAddr, rnMapped, offset.Imm);
        }
        else if (finalAddr != rnMapped)
            MOV(finalAddr, rnMapped);
    }
    else
    {
        if (offset.Reg.ShiftType == ST_ROR)
        {
            ROR(W0, offset.Reg.Rm, offset.Reg.ShiftAmount);
            offset = Op2(W0);
        }

        if (flags & memop_SubtractOffset)
            SUB(finalAddr, rnMapped, offset.Reg.Rm, offset.ToArithOption());
        else
            ADD(finalAddr, rnMapped, offset.Reg.Rm, offset.ToArithOption());
    }

    if (!(flags & memop_Post) && (flags & memop_Writeback))
        MOV(rnMapped, W0);

    u32 expectedTarget = Num == 0
        ? NDS.JIT.Memory.ClassifyAddress9(addrIsStatic ? staticAddress : CurInstr.DataRegion)
        : NDS.JIT.Memory.ClassifyAddress7(addrIsStatic ? staticAddress : CurInstr.DataRegion);

    if (NDS.JIT.FastMemoryEnabled() && ((!Thumb && CurInstr.Cond() != 0xE) || NDS.JIT.Memory.IsFastmemCompatible(expectedTarget)))
    {
        ptrdiff_t memopStart = GetCodeOffset();
        LoadStorePatch patch;

        assert((rdMapped >= W8 && rdMapped <= W15) || (rdMapped >= W19 && rdMapped <= W25) || rdMapped == W4);
        patch.PatchFunc = flags & memop_Store
            ? PatchedStoreFuncs[NDS.ConsoleType][Num][__builtin_ctz(size) - 3][rdMapped]
            : PatchedLoadFuncs[NDS.ConsoleType][Num][__builtin_ctz(size) - 3][!!(flags & memop_SignExtend)][rdMapped];

        // take a chance at fastmem
        if (size > 8)
            ANDI2R(W1, W0, addressMask);
        
        ptrdiff_t loadStorePosition = GetCodeOffset();
        if (flags & memop_Store)
        {
            STRGeneric(size, rdMapped, size > 8 ? X1 : X0, RMemBase);
        }
        else
        {
            LDRGeneric(size, flags & memop_SignExtend, rdMapped, size > 8 ? X1 : X0, RMemBase);
            if (size == 32 && !addrIsStatic)
            {
                UBFIZ(W0, W0, 3, 2);
                RORV(rdMapped, rdMapped, W0);
            }
        }

        patch.PatchOffset = memopStart - loadStorePosition;
        patch.PatchSize = GetCodeOffset() - memopStart;
        LoadStorePatches[loadStorePosition] = patch;
    }
    else
    {
        void* func = NULL;
        if (addrIsStatic)
            func = NDS.JIT.Memory.GetFuncForAddr(CurCPU, staticAddress, flags & memop_Store, size);

        PushRegs(false, false);

        if (func)
        {
            if (flags & memop_Store)
                MOV(W1, rdMapped);
            QuickCallFunction(X2, (void (*)())func);

            PopRegs(false, false);

            if (!(flags & memop_Store))
            {
                if (size == 32)
                {
                    if (staticAddress & 0x3)
                        ROR(rdMapped, W0, (staticAddress & 0x3) << 3);
                    else
                        MOV(rdMapped, W0);
                }
                else
                {
                    if (flags & memop_SignExtend)
                        SBFX(rdMapped, W0, 0, size);
                    else
                        UBFX(rdMapped, W0, 0, size);
                }
            }
        }
        else
        {
            if (Num == 0)
            {
                MOV(X1, RCPU);
                if (flags & memop_Store)
                {
                    MOV(W2, rdMapped);
                    switch (size | NDS.ConsoleType)
                    {
                    case 32: QuickCallFunction(X3, SlowWrite9<u32, 0>); break;
                    case 33: QuickCallFunction(X3, SlowWrite9<u32, 1>); break;
                    case 16: QuickCallFunction(X3, SlowWrite9<u16, 0>); break;
                    case 17: QuickCallFunction(X3, SlowWrite9<u16, 1>); break;
                    case 8: QuickCallFunction(X3, SlowWrite9<u8, 0>); break;
                    case 9: QuickCallFunction(X3, SlowWrite9<u8, 1>); break;
                    }
                }
                else
                {
                    switch (size | NDS.ConsoleType)
                    {
                    case 32: QuickCallFunction(X3, SlowRead9<u32, 0>); break;
                    case 33: QuickCallFunction(X3, SlowRead9<u32, 1>); break;
                    case 16: QuickCallFunction(X3, SlowRead9<u16, 0>); break;
                    case 17: QuickCallFunction(X3, SlowRead9<u16, 1>); break;
                    case 8: QuickCallFunction(X3, SlowRead9<u8, 0>); break;
                    case 9: QuickCallFunction(X3, SlowRead9<u8, 1>); break;
                    }
                }
            }
            else
            {
                if (flags & memop_Store)
                {
                    MOV(W1, rdMapped);
                    switch (size | NDS.ConsoleType)
                    {
                    case 32: QuickCallFunction(X3, SlowWrite7<u32, 0>); break;
                    case 33: QuickCallFunction(X3, SlowWrite7<u32, 1>); break;
                    case 16: QuickCallFunction(X3, SlowWrite7<u16, 0>); break;
                    case 17: QuickCallFunction(X3, SlowWrite7<u16, 1>); break;
                    case 8: QuickCallFunction(X3, SlowWrite7<u8, 0>); break;
                    case 9: QuickCallFunction(X3, SlowWrite7<u8, 1>); break;
                    }
                }
                else
                {
                    switch (size | NDS.ConsoleType)
                    {
                    case 32: QuickCallFunction(X3, SlowRead7<u32, 0>); break;
                    case 33: QuickCallFunction(X3, SlowRead7<u32, 1>); break;
                    case 16: QuickCallFunction(X3, SlowRead7<u16, 0>); break;
                    case 17: QuickCallFunction(X3, SlowRead7<u16, 1>); break;
                    case 8: QuickCallFunction(X3, SlowRead7<u8, 0>); break;
                    case 9: QuickCallFunction(X3, SlowRead7<u8, 1>); break;
                    }
                }
            }

            PopRegs(false, false);

            if (!(flags & memop_Store))
            {
                if (size == 32)
                    MOV(rdMapped, W0);
                else if (flags & memop_SignExtend)
                    SBFX(rdMapped, W0, 0, size);
                else
                    UBFX(rdMapped, W0, 0, size);
            }
        }
    }

    if (CurInstr.Info.Branches())
    {
        if (size < 32)
            Log(LogLevel::Debug, "LDR size < 32 branching?\n");
        Comp_JumpTo(rdMapped, Num == 0, false);
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
    u32 offset = ((CurInstr.Instr & 0xFF) << 2);
    u32 addr = (R15 & ~0x2) + offset;

    if (!NDS.JIT.LiteralOptimizationsEnabled() || !Comp_MemLoadLiteral(32, false, CurInstr.T_Reg(8), addr))
        Comp_MemAccess(CurInstr.T_Reg(8), 15, Op2(offset), 32, 0);
}

void Compiler::T_Comp_MemSPRel()
{
    u32 offset = (CurInstr.Instr & 0xFF) * 4;
    bool load = CurInstr.Instr & (1 << 11);

    Comp_MemAccess(CurInstr.T_Reg(8), 13, Op2(offset), 32, load ? 0 : memop_Store);
}

s32 Compiler::Comp_MemAccessBlock(int rn, BitSet16 regs, bool store, bool preinc, bool decrement, bool usermode, bool skipLoadingRn)
{
    IrregularCycles = true;

    int regsCount = regs.Count();

    if (regsCount == 0)
        return 0; // actually not the right behaviour TODO: fix me

    int firstReg = *regs.begin();
    if (regsCount == 1 && !usermode && RegCache.LoadedRegs & (1 << firstReg) && !(firstReg == rn && skipLoadingRn))
    {
        int flags = 0;
        if (store)
            flags |= memop_Store;
        if (decrement)
            flags |= memop_SubtractOffset;
        Op2 offset = preinc ? Op2(4) : Op2(0);

        Comp_MemAccess(firstReg, rn, offset, 32, flags);

        return decrement ? -4 : 4;
    }

    if (store)
        Comp_AddCycles_CD();
    else
        Comp_AddCycles_CDI();

    int expectedTarget = Num == 0
        ? NDS.JIT.Memory.ClassifyAddress9(CurInstr.DataRegion)
        : NDS.JIT.Memory.ClassifyAddress7(CurInstr.DataRegion);

    bool compileFastPath = NDS.JIT.FastMemoryEnabled()
        && store && !usermode && (CurInstr.Cond() < 0xE || NDS.JIT.Memory.IsFastmemCompatible(expectedTarget));

    {
        s32 offset = decrement
            ? -regsCount * 4 + (preinc ? 0 : 4)
            : (preinc ? 4 : 0);

        if (offset)
            ADDI2R(W0, MapReg(rn), offset);
        else if (compileFastPath)
            ANDI2R(W0, MapReg(rn), ~3);
        else
            MOV(W0, MapReg(rn));
    }

    u8* patchFunc;
    if (compileFastPath)
    {
        ptrdiff_t fastPathStart = GetCodeOffset();
        ptrdiff_t loadStoreOffsets[8];

        ADD(X1, RMemBase, X0);

        u32 offset = 0;
        BitSet16::Iterator it = regs.begin();
        u32 i = 0;

        if (regsCount & 1)
        {
            int reg = *it;
            it++;

            ARM64Reg first = W3;
            if (RegCache.LoadedRegs & (1 << reg))
                first = MapReg(reg);
            else if (store)
                LoadReg(reg, first);

            loadStoreOffsets[i++] = GetCodeOffset();

            if (store)
            {
                STR(INDEX_UNSIGNED, first, X1, offset);
            }
            else if (!(reg == rn && skipLoadingRn))
            {
                LDR(INDEX_UNSIGNED, first, X1, offset);

                if (!(RegCache.LoadedRegs & (1 << reg)))
                    SaveReg(reg, first);
            }

            offset += 4;
        }

        while (it != regs.end())
        {
            int reg = *it;
            it++;
            int nextReg = *it;
            it++;

            ARM64Reg first = W3, second = W4;
            if (RegCache.LoadedRegs & (1 << reg))
            {
                if (!(reg == rn && skipLoadingRn))
                    first = MapReg(reg);
            }
            else if (store)
            {
                LoadReg(reg, first);
            }
            if (RegCache.LoadedRegs & (1 << nextReg))
            {
                if (!(nextReg == rn && skipLoadingRn))
                    second = MapReg(nextReg);
            }
            else if (store)
            {
                LoadReg(nextReg, second);
            }

            loadStoreOffsets[i++] = GetCodeOffset();
            if (store)
            {
                STP(INDEX_SIGNED, first, second, X1, offset);
            }
            else
            {
                LDP(INDEX_SIGNED, first, second, X1, offset);
            
                if (!(RegCache.LoadedRegs & (1 << reg)))
                    SaveReg(reg, first);
                if (!(RegCache.LoadedRegs & (1 << nextReg)))
                    SaveReg(nextReg, second);
            }

            offset += 8;
        }

        LoadStorePatch patch;
        patch.PatchSize = GetCodeOffset() - fastPathStart;
        SwapCodeRegion();
        patchFunc = (u8*)GetRXPtr();
        patch.PatchFunc = patchFunc;
        u32 numLoadStores = i;
        for (i = 0; i < numLoadStores; i++)
        {
            patch.PatchOffset = fastPathStart - loadStoreOffsets[i];
            LoadStorePatches[loadStoreOffsets[i]] = patch;
        }

        ABI_PushRegisters({30});
    }

    int i = 0;

    SUB(SP, SP, ((regsCount + 1) & ~1) * 8);
    if (store)
    {
        if (usermode && (regs & BitSet16(0x7f00)))
            UBFX(W5, RCPSR, 0, 5);

        BitSet16::Iterator it = regs.begin();
        while (it != regs.end())
        {
            BitSet16::Iterator nextReg = it;
            nextReg++;

            int reg = *it;

            if (usermode && reg >= 8 && reg < 15)
            {
                if (RegCache.LoadedRegs & (1 << reg))
                    MOV(W3, MapReg(reg));
                else
                    LoadReg(reg, W3);
                MOVI2R(W1, reg - 8);
                BL(ReadBanked);
                STR(INDEX_UNSIGNED, W3, SP, i * 8);
            }
            else if (!usermode && nextReg != regs.end())
            {
                ARM64Reg first = W3, second = W4;

                if (RegCache.LoadedRegs & (1 << reg))
                    first = MapReg(reg);
                else
                    LoadReg(reg, W3);

                if (RegCache.LoadedRegs & (1 << *nextReg))
                    second = MapReg(*nextReg);
                else
                    LoadReg(*nextReg, W4);

                STP(INDEX_SIGNED, EncodeRegTo64(first), EncodeRegTo64(second), SP, i * 8);

                i++;
                it++;
            }
            else if (RegCache.LoadedRegs & (1 << reg))
            {
                STR(INDEX_UNSIGNED, MapReg(reg), SP, i * 8);
            }
            else
            {
                LoadReg(reg, W3);
                STR(INDEX_UNSIGNED, W3, SP, i * 8);
            }
            i++;
            it++;
        }
    }

    PushRegs(false, false, !compileFastPath);

    ADD(X1, SP, 0);
    MOVI2R(W2, regsCount);

    if (Num == 0)
    {
        MOV(X3, RCPU);
        switch ((u32)store * 2 | NDS.ConsoleType)
        {
        case 0: QuickCallFunction(X4, SlowBlockTransfer9<false, 0>); break;
        case 1: QuickCallFunction(X4, SlowBlockTransfer9<false, 1>); break;
        case 2: QuickCallFunction(X4, SlowBlockTransfer9<true, 0>); break;
        case 3: QuickCallFunction(X4, SlowBlockTransfer9<true, 1>); break;
        }
    }
    else
    {
        switch ((u32)store * 2 | NDS.ConsoleType)
        {
        case 0: QuickCallFunction(X4, SlowBlockTransfer7<false, 0>); break;
        case 1: QuickCallFunction(X4, SlowBlockTransfer7<false, 1>); break;
        case 2: QuickCallFunction(X4, SlowBlockTransfer7<true, 0>); break;
        case 3: QuickCallFunction(X4, SlowBlockTransfer7<true, 1>); break;
        }
    }

    PopRegs(false, false);

    if (!store)
    {
        if (usermode && !regs[15] && (regs & BitSet16(0x7f00)))
            UBFX(W5, RCPSR, 0, 5);

        BitSet16::Iterator it = regs.begin();
        while (it != regs.end())
        {
            BitSet16::Iterator nextReg = it;
            nextReg++;

            int reg = *it;

            if (usermode && !regs[15] && reg >= 8 && reg < 15)
            {
                LDR(INDEX_UNSIGNED, W3, SP, i * 8);
                MOVI2R(W1, reg - 8);
                BL(WriteBanked);
                if (!(reg == rn && skipLoadingRn))
                {
                    FixupBranch alreadyWritten = CBNZ(W4);
                    if (RegCache.LoadedRegs & (1 << reg))
                        MOV(MapReg(reg), W3);
                    else
                        SaveReg(reg, W3);
                    SetJumpTarget(alreadyWritten);
                }
            }
            else if (!usermode && nextReg != regs.end())
            {
                ARM64Reg first = W3, second = W4;
                
                if (RegCache.LoadedRegs & (1 << reg) && !(reg == rn && skipLoadingRn))
                    first = MapReg(reg);
                if (RegCache.LoadedRegs & (1 << *nextReg) && !(*nextReg == rn && skipLoadingRn))
                    second = MapReg(*nextReg);

                LDP(INDEX_SIGNED, EncodeRegTo64(first), EncodeRegTo64(second), SP, i * 8);

                if (first == W3)
                    SaveReg(reg, W3);
                if (second == W4)
                    SaveReg(*nextReg, W4);

                it++;
                i++;
            }
            else if (RegCache.LoadedRegs & (1 << reg))
            {
                if (!(reg == rn && skipLoadingRn))
                {
                    ARM64Reg mapped = MapReg(reg);
                    LDR(INDEX_UNSIGNED, mapped, SP, i * 8);
                }
            }
            else
            {
                LDR(INDEX_UNSIGNED, W3, SP, i * 8);
                SaveReg(reg, W3);
            }

            it++;
            i++;
        }
    }
    ADD(SP, SP, ((regsCount + 1) & ~1) * 8);

    if (compileFastPath)
    {
        ABI_PopRegisters({30});
        RET();

        FlushIcacheSection(patchFunc, (u8*)GetRXPtr());
        SwapCodeRegion();
    }

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

    if (load && writeback && regs[CurInstr.A_Reg(16)])
        writeback = Num == 0
            && (!(regs & ~BitSet16(1 << CurInstr.A_Reg(16)))) || (regs & ~BitSet16((2 << CurInstr.A_Reg(16)) - 1));

    s32 offset = Comp_MemAccessBlock(CurInstr.A_Reg(16), regs, !load, pre, !add, usermode, load && writeback);

    if (writeback && offset)
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
    s32 offset = Comp_MemAccessBlock(13, regs, !load, !load, !load, false, false);

    if (offset)
    {
        if (offset > 0)
            ADD(sp, sp, offset);
        else
            SUB(sp, sp, -offset);
    }
}

void Compiler::T_Comp_LDMIA_STMIA()
{
    BitSet16 regs(CurInstr.Instr & 0xFF);
    ARM64Reg rb = MapReg(CurInstr.T_Reg(8));
    bool load = CurInstr.Instr & (1 << 11);
    u32 regsCount = regs.Count();

    bool writeback = !load || !regs[CurInstr.T_Reg(8)];

    s32 offset = Comp_MemAccessBlock(CurInstr.T_Reg(8), regs, !load, false, false, false, load && writeback);

    if (writeback && offset)
    {
        if (offset > 0)
            ADD(rb, rb, offset);
        else
            SUB(rb, rb, -offset);
    }
}

}
