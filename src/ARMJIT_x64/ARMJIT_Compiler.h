#ifndef ARMJIT_COMPILER_H
#define ARMJIT_COMPILER_H

#include "../dolphin/x64Emitter.h"

#include "../ARMJIT.h"
#include "../ARMJIT_RegCache.h"

#include <tuple>

namespace ARMJIT
{

const Gen::X64Reg RCPU = Gen::RBP;
const Gen::X64Reg RCycles = Gen::R14;
const Gen::X64Reg RCPSR = Gen::R15;

const Gen::X64Reg RSCRATCH = Gen::EAX;
const Gen::X64Reg RSCRATCH2 = Gen::EDX;
const Gen::X64Reg RSCRATCH3 = Gen::ECX;

class Compiler;

typedef void (Compiler::*CompileFunc)();

enum DataRegion
{
    dataRegionGeneric, // hey, that's me!
    dataRegionMainRAM,
    dataRegionSWRAM,
    dataRegionVRAM,
    dataRegionIO,
    dataRegionExclusive,
    dataRegionsCount,
    dataRegionDTCM = dataRegionExclusive,
    dataRegionWRAM7 = dataRegionExclusive,
};

class Compiler : public Gen::X64CodeBlock
{
public:
    Compiler();

    CompiledBlock CompileBlock(ARM* cpu, FetchedInstr instrs[], int instrsCount);

    void LoadReg(int reg, Gen::X64Reg nativeReg);
    void SaveReg(int reg, Gen::X64Reg nativeReg);

private:
    CompileFunc GetCompFunc(int kind);

    void Comp_JumpTo(Gen::X64Reg addr, bool restoreCPSR = false);

    void Comp_AddCycles_C();
    void Comp_AddCycles_CI(u32 i);

    enum
    {
        opSetsFlags = 1 << 0,
        opSymmetric = 1 << 1,
        opRetriveCV = 1 << 2,
        opInvertCarry = 1 << 3,
        opSyncCarry = 1 << 4,
        opInvertOp2 = 1 << 5,
    };

    DataRegion ClassifyAddress(u32 addr);

    void A_Comp_Arith();
    void A_Comp_MovOp();
    void A_Comp_CmpOp();

    void A_Comp_MemWB();
    void A_Comp_MemHalf();

    void T_Comp_ShiftImm();
    void T_Comp_AddSub_();
    void T_Comp_ALU_Imm8();
    void T_Comp_ALU();
    void T_Comp_ALU_HiReg();

    void T_Comp_RelAddr();
    void T_Comp_AddSP();

    void T_Comp_MemReg();
    void T_Comp_MemImm();
    void T_Comp_MemRegHalf();
    void T_Comp_MemImmHalf();

    void Comp_MemAccess(Gen::OpArg rd, bool signExtend, bool store, int size);

    void Comp_ArithTriOp(void (Compiler::*op)(int, const Gen::OpArg&, const Gen::OpArg&), 
        Gen::OpArg rd, Gen::OpArg rn, Gen::OpArg op2, bool carryUsed, int opFlags);
    void Comp_ArithTriOpReverse(void (Compiler::*op)(int, const Gen::OpArg&, const Gen::OpArg&),
        Gen::OpArg rd, Gen::OpArg rn, Gen::OpArg op2, bool carryUsed, int opFlags);
    void Comp_CmpOp(int op, Gen::OpArg rn, Gen::OpArg op2, bool carryUsed);

    void Comp_RetriveFlags(bool sign, bool retriveCV, bool carryUsed);

    void* Gen_MemoryRoutine9(bool store, int size);
    void* Gen_MemoryRoutine7(bool store, bool codeMainRAM, int size);

    Gen::OpArg Comp_RegShiftImm(int op, int amount, Gen::OpArg rm, bool S, bool& carryUsed);
    Gen::OpArg Comp_RegShiftReg(int op, Gen::OpArg rs, Gen::OpArg rm, bool S, bool& carryUsed);

    Gen::OpArg A_Comp_GetALUOp2(bool S, bool& carryUsed);
    Gen::OpArg A_Comp_GetMemWBOffset();

    void LoadCPSR();
    void SaveCPSR();

    Gen::OpArg MapReg(int reg)
    {
        if (reg == 15 && RegCache.Mapping[reg] == Gen::INVALID_REG)
            return Gen::Imm32(R15);

        assert(RegCache.Mapping[reg] != Gen::INVALID_REG);
        return Gen::R(RegCache.Mapping[reg]);
    }

    void* ResetStart;
    void* MemoryFuncs9[3][2];
    void* MemoryFuncs7[3][2][2];

    bool CPSRDirty = false;

    FetchedInstr CurInstr;

    RegCache<Compiler, Gen::X64Reg> RegCache;

    bool Thumb;
    u32 Num;
    u32 R15;
    u32 CodeRegion;

    u32 ConstantCycles;

    ARM* CurCPU;
};

}

#endif