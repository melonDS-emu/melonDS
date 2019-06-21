#ifndef ARMJIT_COMPILER_H
#define ARMJIT_COMPILER_H

#include "../dolphin/x64Emitter.h"

#include "../ARMJIT.h"


namespace ARMJIT
{

const Gen::X64Reg RCPU = Gen::RBP;
const Gen::X64Reg RCycles = Gen::R14;
const Gen::X64Reg RCPSR = Gen::R15;

const Gen::X64Reg RSCRATCH = Gen::EAX;
const Gen::X64Reg RSCRATCH2 = Gen::EDX;
const Gen::X64Reg RSCRATCH3 = Gen::ECX;

class Compiler : public Gen::X64CodeBlock
{
public:
    Compiler();

    CompiledBlock CompileBlock(ARM* cpu, FetchedInstr instrs[], int instrsCount);

    void StartBlock(ARM* cpu);
    CompiledBlock FinaliseBlock();

    void Compile(RegCache& regs, const FetchedInstr& instr);
private:
    void AddCycles_C();

    Gen::OpArg Comp_ShiftRegImm(int op, int amount, Gen::X64Reg rm, bool S, bool& carryUsed);

    void A_Comp_ALU(const FetchedInstr& instr);

    void LoadCPSR();
    void SaveCPSR();

    bool CPSRDirty = false;

    FetchedInstr CurrentInstr;

    bool Thumb;
    u32 Num;
    u32 R15;

    u32 ConstantCycles;
};

}

#endif