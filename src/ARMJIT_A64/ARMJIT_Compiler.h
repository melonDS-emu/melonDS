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

#ifndef ARMJIT_A64_COMPILER_H
#define ARMJIT_A64_COMPILER_H

#if defined(JIT_ENABLED) && defined(__aarch64__)

#include "../ARM.h"

#include "../dolphin/Arm64Emitter.h"

#include "../ARMJIT_Internal.h"
#include "../ARMJIT_RegisterCache.h"

#include <unordered_map>

namespace melonDS
{
class ARMJIT;
const Arm64Gen::ARM64Reg RMemBase = Arm64Gen::X26;
const Arm64Gen::ARM64Reg RCPSR = Arm64Gen::W27;
const Arm64Gen::ARM64Reg RCycles = Arm64Gen::W28;
const Arm64Gen::ARM64Reg RCPU = Arm64Gen::X29;

struct Op2
{
    Op2()
    {}

    Op2(Arm64Gen::ARM64Reg rm) : IsImm(false)
    {
        Reg.Rm = rm;
        Reg.ShiftType = Arm64Gen::ST_LSL;
        Reg.ShiftAmount = 0;
    }

    Op2(u32 imm) : IsImm(true), Imm(imm)
    {}

    Op2(Arm64Gen::ARM64Reg rm, Arm64Gen::ShiftType st, int amount) : IsImm(false)
    {
        Reg.Rm = rm;
        Reg.ShiftType = st;
        Reg.ShiftAmount = amount;
    }

    Arm64Gen::ArithOption ToArithOption()
    {
        assert(!IsImm);
        return Arm64Gen::ArithOption(Reg.Rm, Reg.ShiftType, Reg.ShiftAmount);
    }

    bool IsSimpleReg()
    { return !IsImm && !Reg.ShiftAmount && Reg.ShiftType == Arm64Gen::ST_LSL; }
    bool ImmFits12Bit()
    { return IsImm && ((Imm & 0xFFF) == Imm); }
    bool IsZero()
    { return IsImm && !Imm; }

    bool IsImm;
    union
    {
        struct
        {
            Arm64Gen::ARM64Reg Rm;
            Arm64Gen::ShiftType ShiftType;
            int ShiftAmount;
        } Reg;
        u32 Imm;
    };
};

struct LoadStorePatch
{
    void* PatchFunc;
    s32 PatchOffset;
    u32 PatchSize;
};

class Compiler : public Arm64Gen::ARM64XEmitter
{
public:
    typedef void (Compiler::*CompileFunc)();

    explicit Compiler(melonDS::NDS& nds);
    ~Compiler() override;

    void PushRegs(bool saveHiRegs, bool saveRegsToBeChanged, bool allowUnload = true);
    void PopRegs(bool saveHiRegs, bool saveRegsToBeChanged);

    Arm64Gen::ARM64Reg MapReg(int reg)
    {
        assert(RegCache.Mapping[reg] != Arm64Gen::INVALID_REG);
        return RegCache.Mapping[reg];
    }

    JitBlockEntry CompileBlock(ARM* cpu, bool thumb, FetchedInstr instrs[], int instrsCount, bool hasMemInstr);

    bool CanCompile(bool thumb, u16 kind);

    bool FlagsNZNeeded() const
    {
        return CurInstr.SetFlags & 0xC;
    }

    void Reset();

    void Comp_AddCycles_C(bool forceNonConstant = false);
    void Comp_AddCycles_CI(u32 numI);
    void Comp_AddCycles_CI(u32 c, Arm64Gen::ARM64Reg numI, Arm64Gen::ArithOption shift);
    void Comp_AddCycles_CD();
    void Comp_AddCycles_CDI();

    void MovePC();

    void LoadReg(int reg, Arm64Gen::ARM64Reg nativeReg);
    void SaveReg(int reg, Arm64Gen::ARM64Reg nativeReg);

    void LoadCPSR();
    void SaveCPSR(bool markClean = true);

    void LoadCycles();
    void SaveCycles();

    void Nop() {}

    void A_Comp_ALUTriOp();
    void A_Comp_ALUMovOp();
    void A_Comp_ALUCmpOp();

    void A_Comp_Mul();
    void A_Comp_Mul_Long();
    void A_Comp_Mul_Short();

    void A_Comp_Clz();

    void A_Comp_MemWB();
    void A_Comp_MemHD();

    void A_Comp_LDM_STM();
    
    void A_Comp_BranchImm();
    void A_Comp_BranchXchangeReg();

    void A_Comp_MRS();
    void A_Comp_MSR();

    void T_Comp_ShiftImm();
    void T_Comp_AddSub_();
    void T_Comp_ALUImm8();
    void T_Comp_ALU();
    void T_Comp_ALU_HiReg();
    void T_Comp_AddSP();
    void T_Comp_RelAddr();

    void T_Comp_MemReg();
    void T_Comp_MemImm();
    void T_Comp_MemRegHalf();
    void T_Comp_MemImmHalf();
    void T_Comp_LoadPCRel();
    void T_Comp_MemSPRel();

    void T_Comp_LDMIA_STMIA();
    void T_Comp_PUSH_POP();

    void T_Comp_BCOND();
    void T_Comp_B();
    void T_Comp_BranchXchangeReg();
    void T_Comp_BL_LONG_1();
    void T_Comp_BL_LONG_2();
    void T_Comp_BL_Merged();

    s32 Comp_MemAccessBlock(int rn, BitSet16 regs, bool store, bool preinc, bool decrement, bool usermode, bool skipLoadingRn);

    void Comp_Mul_Mla(bool S, bool mla, Arm64Gen::ARM64Reg rd, Arm64Gen::ARM64Reg rm, Arm64Gen::ARM64Reg rs, Arm64Gen::ARM64Reg rn);

    void Comp_Compare(int op, Arm64Gen::ARM64Reg rn, Op2 op2);
    void Comp_Logical(int op, bool S, Arm64Gen::ARM64Reg rd, Arm64Gen::ARM64Reg rn, Op2 op2);
    void Comp_Arithmetic(int op, bool S, Arm64Gen::ARM64Reg rd, Arm64Gen::ARM64Reg rn, Op2 op2);

    void Comp_RetriveFlags(bool retriveCV);

    Arm64Gen::FixupBranch CheckCondition(u32 cond);

    void Comp_JumpTo(Arm64Gen::ARM64Reg addr, bool switchThumb, bool restoreCPSR = false);
    void Comp_JumpTo(u32 addr, bool forceNonConstantCycles = false);

    void A_Comp_GetOp2(bool S, Op2& op2);

    void Comp_RegShiftImm(int op, int amount, bool S, Op2& op2, Arm64Gen::ARM64Reg tmp = Arm64Gen::W0);
    void Comp_RegShiftReg(int op, bool S, Op2& op2, Arm64Gen::ARM64Reg rs);

    bool Comp_MemLoadLiteral(int size, bool signExtend, int rd, u32 addr);

    enum
    {
        memop_Writeback = 1 << 0,
        memop_Post = 1 << 1,
        memop_SignExtend = 1 << 2,
        memop_Store = 1 << 3,
        memop_SubtractOffset = 1 << 4
    };
    void Comp_MemAccess(int rd, int rn, Op2 offset, int size, int flags);

    // 0 = switch mode, 1 = stay arm, 2 = stay thumb
    void* Gen_JumpTo9(int kind);
    void* Gen_JumpTo7(int kind);

    void Comp_BranchSpecialBehaviour(bool taken);

    JitBlockEntry AddEntryOffset(u32 offset)
    {
        return (JitBlockEntry)(GetRXBase() + offset);
    }

    u32 SubEntryOffset(JitBlockEntry entry)
    {
        return (u8*)entry - GetRXBase();
    }

    bool IsJITFault(const u8* pc);
    u8* RewriteMemAccess(u8* pc);

    void SwapCodeRegion()
    {
        ptrdiff_t offset = GetCodeOffset();
        SetCodePtrUnsafe(OtherCodeRegion);
        OtherCodeRegion = offset;
    }

    melonDS::NDS& NDS;
    ptrdiff_t OtherCodeRegion;

    bool Exit;

    FetchedInstr CurInstr;
    bool Thumb;
    u32 R15;
    u32 Num;
    ARM* CurCPU;
    u32 ConstantCycles;
    u32 CodeRegion;

    BitSet32 SavedRegs;

    u32 JitMemSecondarySize;
    u32 JitMemMainSize;

    std::unordered_map<ptrdiff_t, LoadStorePatch> LoadStorePatches; 

    RegisterCache<Compiler, Arm64Gen::ARM64Reg> RegCache;

    bool CPSRDirty = false;

    bool IrregularCycles = false;

#ifdef __SWITCH__
    void* JitRWBase;
    void* JitRWStart;
    void* JitRXStart;
#endif

    void* ReadBanked, *WriteBanked;

    void* JumpToFuncs9[3];
    void* JumpToFuncs7[3];

    // [Console Type][Num][Size][Sign Extend][Output register]
    void* PatchedLoadFuncs[2][2][3][2][32];
    void* PatchedStoreFuncs[2][2][3][32];
};

}

#endif

#endif
