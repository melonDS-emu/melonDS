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

#ifndef ARMJIT_X64_COMPILER_H
#define ARMJIT_X64_COMPILER_H

#if defined(JIT_ENABLED) && defined(__x86_64__)

#include "../dolphin/x64Emitter.h"

#include "../ARMJIT_Internal.h"
#include "../ARMJIT_RegisterCache.h"

#ifdef JIT_PROFILING_ENABLED
#include <jitprofiling.h>
#endif

#include <unordered_map>


namespace melonDS
{
class ARMJIT;
class ARMJIT_Memory;
class NDS;
const Gen::X64Reg RCPU = Gen::RBP;
const Gen::X64Reg RCPSR = Gen::R15;

const Gen::X64Reg RSCRATCH = Gen::EAX;
const Gen::X64Reg RSCRATCH2 = Gen::EDX;
const Gen::X64Reg RSCRATCH3 = Gen::ECX;
const Gen::X64Reg RSCRATCH4 = Gen::R8;

struct LoadStorePatch
{
    void* PatchFunc;
    s16 Offset;
    u16 Size;
};

struct Op2
{
    Op2()
    {}

    Op2(u32 imm)
        : IsImm(true), Imm(imm)
    {}
    Op2(int reg, int op, int amount)
        : IsImm(false)
    {
        Reg.Reg = reg;
        Reg.Op = op;
        Reg.Amount = amount;
    }

    bool IsImm;
    union
    {
        struct
        {
            int Reg, Op, Amount;
        } Reg;
        u32 Imm;
    };
};

class Compiler : public Gen::XEmitter
{
public:
    explicit Compiler(melonDS::NDS& nds);

    void Reset();

    JitBlockEntry CompileBlock(ARM* cpu, bool thumb, FetchedInstr instrs[], int instrsCount, bool hasMemoryInstr);

    void LoadReg(int reg, Gen::X64Reg nativeReg);
    void SaveReg(int reg, Gen::X64Reg nativeReg);

    bool CanCompile(bool thumb, u16 kind) const;

    typedef void (Compiler::*CompileFunc)();

    void Comp_JumpTo(Gen::X64Reg addr, bool restoreCPSR = false);
    void Comp_JumpTo(u32 addr, bool forceNonConstantCycles = false);

    void Comp_AddCycles_C(bool forceNonConstant = false);
    void Comp_AddCycles_CI(u32 i);
    void Comp_AddCycles_CI(Gen::X64Reg i, int add);
    void Comp_AddCycles_CDI();
    void Comp_AddCycles_CD();

    enum
    {
        opSetsFlags = 1 << 0,
        opSymmetric = 1 << 1,
        opRetriveCV = 1 << 2,
        opInvertCarry = 1 << 3,
        opSyncCarry = 1 << 4,
        opInvertOp2 = 1 << 5,
    };

    void Nop() {}

    void A_Comp_Arith();
    void A_Comp_MovOp();
    void A_Comp_CmpOp();

    void A_Comp_MUL_MLA();
    void A_Comp_Mul_Long();

    void A_Comp_CLZ();

    void A_Comp_MemWB();
    void A_Comp_MemHalf();
    void A_Comp_LDM_STM();

    void A_Comp_BranchImm();
    void A_Comp_BranchXchangeReg();

    void A_Comp_MRS();
    void A_Comp_MSR();

    void T_Comp_ShiftImm();
    void T_Comp_AddSub_();
    void T_Comp_ALU_Imm8();
    void T_Comp_ALU();
    void T_Comp_ALU_HiReg();
    void T_Comp_MUL();

    void T_Comp_RelAddr();
    void T_Comp_AddSP();

    void T_Comp_MemReg();
    void T_Comp_MemImm();
    void T_Comp_MemRegHalf();
    void T_Comp_MemImmHalf();
    void T_Comp_LoadPCRel();
    void T_Comp_MemSPRel();
    void T_Comp_PUSH_POP();
    void T_Comp_LDMIA_STMIA();

    void T_Comp_BCOND();
    void T_Comp_B();
    void T_Comp_BranchXchangeReg();
    void T_Comp_BL_LONG_1();
    void T_Comp_BL_LONG_2();
    void T_Comp_BL_Merged();

    enum
    {
        memop_Writeback = 1 << 0,
        memop_Post = 1 << 1,
        memop_SignExtend = 1 << 2,
        memop_Store = 1 << 3,
        memop_SubtractOffset = 1 << 4
    };
    void Comp_MemAccess(int rd, int rn, const Op2& op2, int size, int flags);
    s32 Comp_MemAccessBlock(int rn, Common::BitSet16 regs, bool store, bool preinc, bool decrement, bool usermode, bool skipLoadingRn);
    bool Comp_MemLoadLiteral(int size, bool signExtend, int rd, u32 addr);

    void Comp_ArithTriOp(void (Compiler::*op)(int, const Gen::OpArg&, const Gen::OpArg&),
        Gen::OpArg rd, Gen::OpArg rn, Gen::OpArg op2, bool carryUsed, int opFlags);
    void Comp_ArithTriOpReverse(void (Compiler::*op)(int, const Gen::OpArg&, const Gen::OpArg&),
        Gen::OpArg rd, Gen::OpArg rn, Gen::OpArg op2, bool carryUsed, int opFlags);
    void Comp_CmpOp(int op, Gen::OpArg rn, Gen::OpArg op2, bool carryUsed);

    void Comp_MulOp(bool S, bool add, Gen::OpArg rd, Gen::OpArg rm, Gen::OpArg rs, Gen::OpArg rn);

    void Comp_RetriveFlags(bool sign, bool retriveCV, bool carryUsed);

    void Comp_SpecialBranchBehaviour(bool taken);


    Gen::OpArg Comp_RegShiftImm(int op, int amount, Gen::OpArg rm, bool S, bool& carryUsed);
    Gen::OpArg Comp_RegShiftReg(int op, Gen::OpArg rs, Gen::OpArg rm, bool S, bool& carryUsed);

    Gen::OpArg A_Comp_GetALUOp2(bool S, bool& carryUsed);

    void LoadCPSR();
    void SaveCPSR(bool flagClean = true);

    bool FlagsNZRequired()
    { return CurInstr.SetFlags & 0xC; }

    Gen::FixupBranch CheckCondition(u32 cond);

    void PushRegs(bool saveHiRegs, bool saveRegsToBeChanged, bool allowUnload = true);
    void PopRegs(bool saveHiRegs, bool saveRegsToBeChanged);

    Gen::OpArg MapReg(int reg)
    {
        if (reg == 15 && !(RegCache.LoadedRegs & (1 << 15)))
            return Gen::Imm32(R15);

        assert(RegCache.Mapping[reg] != Gen::INVALID_REG);
        return Gen::R(RegCache.Mapping[reg]);
    }

    JitBlockEntry AddEntryOffset(u32 offset)
    {
        return (JitBlockEntry)(ResetStart + offset);
    }

    u32 SubEntryOffset(JitBlockEntry entry)
    {
        return (u8*)entry - ResetStart;
    }

    void SwitchToNearCode()
    {
        FarCode = GetWritableCodePtr();
        SetCodePtr(NearCode);
    }

    void SwitchToFarCode()
    {
        NearCode = GetWritableCodePtr();
        SetCodePtr(FarCode);
    }

    bool IsJITFault(const u8* addr);

    u8* RewriteMemAccess(u8* pc);

#ifdef JIT_PROFILING_ENABLED
    void CreateMethod(const char* namefmt, void* start, ...);
#endif

    melonDS::NDS& NDS;
    u8* FarCode {};
    u8* NearCode {};
    u32 FarSize {};
    u32 NearSize {};

    u8* NearStart {};
    u8* FarStart {};

    void* PatchedStoreFuncs[2][2][3][16] {};
    void* PatchedLoadFuncs[2][2][3][2][16] {};

    std::unordered_map<u8*, LoadStorePatch> LoadStorePatches {};

    u8* ResetStart {};
    u32 CodeMemSize {};

    bool Exit {};
    bool IrregularCycles {};

    void* ReadBanked {};
    void* WriteBanked {};

    bool CPSRDirty = false;

    FetchedInstr CurInstr {};

    RegisterCache<Compiler, Gen::X64Reg> RegCache {};

    bool Thumb {};
    u32 Num {};
    u32 R15 {};
    u32 CodeRegion {};

    u32 ConstantCycles {};

    ARM* CurCPU {};
};

}
#endif

#endif
