#include "ARMJIT_Compiler.h"

#include "../ARMInterpreter.h"
#include "../Config.h"

#include <assert.h>

#include "../dolphin/CommonFuncs.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace Gen;

extern "C" void ARM_Ret();

namespace ARMJIT
{
template <>
const X64Reg RegisterCache<Compiler, X64Reg>::NativeRegAllocOrder[] =
{
#ifdef _WIN32
    RBX, RSI, RDI, R12, R13, R14
#else
    RBX, R12, R13, R14 // this is sad
#endif
};
template <>
const int RegisterCache<Compiler, X64Reg>::NativeRegsAvailable =
#ifdef _WIN32
    6
#else
    4
#endif
;

void Compiler::A_Comp_MRS()
{
    Comp_AddCycles_C();

    OpArg rd = MapReg(CurInstr.A_Reg(12));

    if (CurInstr.Instr & (1 << 22))
    {
        MOV(32, R(RSCRATCH), R(RCPSR));
        AND(32, R(RSCRATCH), Imm8(0x1F));
        XOR(32, R(RSCRATCH3), R(RSCRATCH3));
        MOV(32, R(RSCRATCH2), Imm32(15 - 8));
        CALL(ReadBanked);
        MOV(32, rd, R(RSCRATCH3));
    }
    else
        MOV(32, rd, R(RCPSR));
}

void Compiler::A_Comp_MSR()
{
    Comp_AddCycles_C();

    OpArg val = CurInstr.Instr & (1 << 25)
        ? Imm32(ROR((CurInstr.Instr & 0xFF), ((CurInstr.Instr >> 7) & 0x1E)))
        : MapReg(CurInstr.A_Reg(0));

    u32 mask = 0;
    if (CurInstr.Instr & (1<<16)) mask |= 0x000000FF;
    if (CurInstr.Instr & (1<<17)) mask |= 0x0000FF00;
    if (CurInstr.Instr & (1<<18)) mask |= 0x00FF0000;
    if (CurInstr.Instr & (1<<19)) mask |= 0xFF000000;

    if (CurInstr.Instr & (1 << 22))
    {
        MOV(32, R(RSCRATCH), R(RCPSR));
        AND(32, R(RSCRATCH), Imm8(0x1F));
        XOR(32, R(RSCRATCH3), R(RSCRATCH3));
        MOV(32, R(RSCRATCH2), Imm32(15 - 8));
        CALL(ReadBanked);

        MOV(32, R(RSCRATCH2), Imm32(mask));
        MOV(32, R(RSCRATCH4), R(RSCRATCH2));
        AND(32, R(RSCRATCH4), Imm32(0xFFFFFF00));
        MOV(32, R(RSCRATCH), R(RCPSR));
        AND(32, R(RSCRATCH), Imm8(0x1F));
        CMP(32, R(RSCRATCH), Imm8(0x10));
        CMOVcc(32, RSCRATCH2, R(RSCRATCH4), CC_E);

        MOV(32, R(RSCRATCH4), R(RSCRATCH2));
        NOT(32, R(RSCRATCH4));
        AND(32, R(RSCRATCH3), R(RSCRATCH4));

        AND(32, R(RSCRATCH2), val);
        OR(32, R(RSCRATCH3), R(RSCRATCH2));

        MOV(32, R(RSCRATCH2), Imm32(15 - 8));
        CALL(WriteBanked);
    }
    else
    {
        mask &= 0xFFFFFFDF;
        CPSRDirty = true;

        if ((mask & 0xFF) == 0)
        {
            AND(32, R(RCPSR), Imm32(~mask));
            if (!val.IsImm())
            {
                MOV(32, R(RSCRATCH), val);
                AND(32, R(RSCRATCH), Imm32(mask));
                OR(32, R(RCPSR), R(RSCRATCH));
            }
            else
            {
                OR(32, R(RCPSR), Imm32(val.Imm32() & mask));
            }
        }
        else
        {
            MOV(32, R(RSCRATCH2), Imm32(mask));
            MOV(32, R(RSCRATCH3), R(RSCRATCH2));
            AND(32, R(RSCRATCH3), Imm32(0xFFFFFF00));
            MOV(32, R(RSCRATCH), R(RCPSR));
            AND(32, R(RSCRATCH), Imm8(0x1F));
            CMP(32, R(RSCRATCH), Imm8(0x10));
            CMOVcc(32, RSCRATCH2, R(RSCRATCH3), CC_E);

            MOV(32, R(RSCRATCH3), R(RCPSR));

            // I need you ANDN
            MOV(32, R(RSCRATCH), R(RSCRATCH2));
            NOT(32, R(RSCRATCH));
            AND(32, R(RCPSR), R(RSCRATCH));

            AND(32, R(RSCRATCH2), val);
            OR(32, R(RCPSR), R(RSCRATCH2));

            BitSet16 hiRegsLoaded(RegCache.LoadedRegs & 0x7F00);
            if (Thumb || CurInstr.Cond() >= 0xE)
                RegCache.Flush();
            else
            {
                // the ugly way...
                // we only save them, to load and save them again
                for (int reg : hiRegsLoaded)
                    SaveReg(reg, RegCache.Mapping[reg]);
            }

            MOV(32, R(ABI_PARAM3), R(RCPSR));
            MOV(32, R(ABI_PARAM2), R(RSCRATCH3));
            MOV(64, R(ABI_PARAM1), R(RCPU));
            CALL((void*)&ARM::UpdateMode);

            if (!Thumb && CurInstr.Cond() < 0xE)
            {
                for (int reg : hiRegsLoaded)
                    LoadReg(reg, RegCache.Mapping[reg]);
            }
        }
    }
}

/*
    We'll repurpose this .bss memory

 */
u8 CodeMemory[1024 * 1024 * 32];

Compiler::Compiler()
{
    {
    #ifdef _WIN32
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        u64 pageSize = (u64)sysInfo.dwPageSize;
    #else
        u64 pageSize = sysconf(_SC_PAGE_SIZE);
    #endif

        u8* pageAligned = (u8*)(((u64)CodeMemory & ~(pageSize - 1)) + pageSize);
        u64 alignedSize = (((u64)CodeMemory + sizeof(CodeMemory)) & ~(pageSize - 1)) - (u64)pageAligned;

    #ifdef _WIN32
        DWORD dummy;
        VirtualProtect(pageAligned, alignedSize, PAGE_EXECUTE_READWRITE, &dummy);
    #else
        mprotect(pageAligned, alignedSize, PROT_EXEC | PROT_READ | PROT_WRITE);
    #endif

        ResetStart = pageAligned;
        CodeMemSize = alignedSize;
    }

    Reset();

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 2; j++)
            MemoryFuncs9[i][j] = Gen_MemoryRoutine9(j, 8 << i);
    }
    MemoryFuncs7[0][0] = (void*)NDS::ARM7Read8;
    MemoryFuncs7[0][1] = (void*)NDS::ARM7Write8;
    MemoryFuncs7[1][0] = (void*)NDS::ARM7Read16;
    MemoryFuncs7[1][1] = (void*)NDS::ARM7Write16;
    MemoryFuncs7[2][0] = (void*)NDS::ARM7Read32;
    MemoryFuncs7[2][1] = (void*)NDS::ARM7Write32;

    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
        {
            MemoryFuncsSeq9[i][j] = Gen_MemoryRoutineSeq9(i, j);
            MemoryFuncsSeq7[i][j][0] = Gen_MemoryRoutineSeq7(i, j, false);
            MemoryFuncsSeq7[i][j][1] = Gen_MemoryRoutineSeq7(i, j, true);
        }

    {
        // RSCRATCH mode
        // RSCRATCH2 reg number
        // RSCRATCH3 value in current mode
        // ret - RSCRATCH3
        ReadBanked = (void*)GetWritableCodePtr();
        CMP(32, R(RSCRATCH), Imm8(0x11));
        FixupBranch fiq = J_CC(CC_E);
        SUB(32, R(RSCRATCH2), Imm8(13 - 8));
        FixupBranch notEverything = J_CC(CC_L);
        CMP(32, R(RSCRATCH), Imm8(0x12));
        FixupBranch irq = J_CC(CC_E);
        CMP(32, R(RSCRATCH), Imm8(0x13));
        FixupBranch svc = J_CC(CC_E);
        CMP(32, R(RSCRATCH), Imm8(0x17));
        FixupBranch abt = J_CC(CC_E);
        CMP(32, R(RSCRATCH), Imm8(0x1B));
        FixupBranch und = J_CC(CC_E);
        SetJumpTarget(notEverything);
        RET();

        SetJumpTarget(fiq);
        MOV(32, R(RSCRATCH3), MComplex(RCPU, RSCRATCH2, SCALE_4, offsetof(ARM, R_FIQ)));
        RET();
        SetJumpTarget(irq);
        MOV(32, R(RSCRATCH3), MComplex(RCPU, RSCRATCH2, SCALE_4, offsetof(ARM, R_IRQ)));
        RET();
        SetJumpTarget(svc);
        MOV(32, R(RSCRATCH3), MComplex(RCPU, RSCRATCH2, SCALE_4, offsetof(ARM, R_SVC)));
        RET();
        SetJumpTarget(abt);
        MOV(32, R(RSCRATCH3), MComplex(RCPU, RSCRATCH2, SCALE_4, offsetof(ARM, R_ABT)));
        RET();
        SetJumpTarget(und);
        MOV(32, R(RSCRATCH3), MComplex(RCPU, RSCRATCH2, SCALE_4, offsetof(ARM, R_UND)));
        RET();
    }
    {
        // RSCRATCH  mode
        // RSCRATCH2 reg n
        // RSCRATCH3 value
        // carry flag set if the register isn't banked
        WriteBanked = (void*)GetWritableCodePtr();
        CMP(32, R(RSCRATCH), Imm8(0x11));
        FixupBranch fiq = J_CC(CC_E);
        SUB(32, R(RSCRATCH2), Imm8(13 - 8));
        FixupBranch notEverything = J_CC(CC_L);
        CMP(32, R(RSCRATCH), Imm8(0x12));
        FixupBranch irq = J_CC(CC_E);
        CMP(32, R(RSCRATCH), Imm8(0x13));
        FixupBranch svc = J_CC(CC_E);
        CMP(32, R(RSCRATCH), Imm8(0x17));
        FixupBranch abt = J_CC(CC_E);
        CMP(32, R(RSCRATCH), Imm8(0x1B));
        FixupBranch und = J_CC(CC_E);
        SetJumpTarget(notEverything);
        STC();
        RET();

        SetJumpTarget(fiq);
        MOV(32, MComplex(RCPU, RSCRATCH2, SCALE_4, offsetof(ARM, R_FIQ)), R(RSCRATCH3));
        CLC();
        RET();
        SetJumpTarget(irq);
        MOV(32, MComplex(RCPU, RSCRATCH2, SCALE_4, offsetof(ARM, R_IRQ)), R(RSCRATCH3));
        CLC();
        RET();
        SetJumpTarget(svc);
        MOV(32, MComplex(RCPU, RSCRATCH2, SCALE_4, offsetof(ARM, R_SVC)), R(RSCRATCH3));
        CLC();
        RET();
        SetJumpTarget(abt);
        MOV(32, MComplex(RCPU, RSCRATCH2, SCALE_4, offsetof(ARM, R_ABT)), R(RSCRATCH3));
        CLC();
        RET();
        SetJumpTarget(und);
        MOV(32, MComplex(RCPU, RSCRATCH2, SCALE_4, offsetof(ARM, R_UND)), R(RSCRATCH3));
        CLC();
        RET();
    }

    {
        CPSRDirty = true;
        BranchStub[0] = GetWritableCodePtr();
        SaveCPSR();
        MOV(64, R(ABI_PARAM1), R(RCPU));
        CALL((u8*)ARMJIT::LinkBlock<0>);
        LoadCPSR();
        JMP((u8*)ARM_Ret, true);

        CPSRDirty = true;
        BranchStub[1] = GetWritableCodePtr();
        SaveCPSR();
        MOV(64, R(ABI_PARAM1), R(RCPU));
        CALL((u8*)ARMJIT::LinkBlock<1>);
        LoadCPSR();
        JMP((u8*)ARM_Ret, true);
    }

    // move the region forward to prevent overwriting the generated functions
    CodeMemSize -= GetWritableCodePtr() - ResetStart;
    ResetStart = GetWritableCodePtr();
}

void Compiler::LoadCPSR()
{
    assert(!CPSRDirty);

    MOV(32, R(RCPSR), MDisp(RCPU, offsetof(ARM, CPSR)));
}

void Compiler::SaveCPSR(bool flagClean)
{
    if (CPSRDirty)
    {
        MOV(32, MDisp(RCPU, offsetof(ARM, CPSR)), R(RCPSR));
        if (flagClean)
            CPSRDirty = false;
    }
}

void Compiler::LoadReg(int reg, X64Reg nativeReg)
{
    if (reg != 15)
        MOV(32, R(nativeReg), MDisp(RCPU, offsetof(ARM, R[reg])));
    else
        MOV(32, R(nativeReg), Imm32(R15));
}

void Compiler::SaveReg(int reg, X64Reg nativeReg)
{
    MOV(32, MDisp(RCPU, offsetof(ARM, R[reg])), R(nativeReg));
}

// invalidates RSCRATCH and RSCRATCH3
Gen::FixupBranch Compiler::CheckCondition(u32 cond)
{
    // hack, ldm/stm can get really big TODO: make this better
    bool ldmStm = !Thumb &&
        (CurInstr.Info.Kind == ARMInstrInfo::ak_LDM || CurInstr.Info.Kind == ARMInstrInfo::ak_STM);
    if (cond >= 0x8)
    {
        static_assert(RSCRATCH3 == ECX, "RSCRATCH has to be equal to ECX!");
        MOV(32, R(RSCRATCH3), R(RCPSR));
        SHR(32, R(RSCRATCH3), Imm8(28));
        MOV(32, R(RSCRATCH), Imm32(1));
        SHL(32, R(RSCRATCH), R(RSCRATCH3));
        TEST(32, R(RSCRATCH), Imm32(ARM::ConditionTable[cond]));

        return J_CC(CC_Z, ldmStm);
    }
    else
    {
        // could have used a LUT, but then where would be the fun?
        TEST(32, R(RCPSR), Imm32(1 << (28 + ((~(cond >> 1) & 1) << 1 | (cond >> 2 & 1) ^ (cond >> 1 & 1)))));

        return J_CC(cond & 1 ? CC_NZ : CC_Z, ldmStm);
    }
}

#define F(x) &Compiler::x
const Compiler::CompileFunc A_Comp[ARMInstrInfo::ak_Count] =
{
    // AND
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    // EOR
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    // SUB
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    // RSB
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    // ADD
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    // ADC
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    // SBC
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    // RSC
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    // ORR
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    // MOV
    F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp),
    F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp),
    // BIC
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith), F(A_Comp_Arith),
    // MVN
    F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp),
    F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp), F(A_Comp_MovOp),
    // TST
    F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp),
    // TEQ
    F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp),
    // CMP
    F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp),
    // CMN
    F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp), F(A_Comp_CmpOp),
    // Mul
    F(A_Comp_MUL_MLA), F(A_Comp_MUL_MLA), F(A_Comp_Mul_Long), F(A_Comp_Mul_Long), F(A_Comp_Mul_Long), F(A_Comp_Mul_Long), NULL, NULL, NULL, NULL, NULL,
    // ARMv5 stuff
    F(A_Comp_CLZ), NULL, NULL, NULL, NULL,
    // STR
    F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB),
    // STRB
    F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB),
    // LDR
    F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB),
    // LDRB
    F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB), F(A_Comp_MemWB),
    // STRH
    F(A_Comp_MemHalf), F(A_Comp_MemHalf), F(A_Comp_MemHalf), F(A_Comp_MemHalf),
    // LDRD, STRD never used by anything so they stay interpreted (by anything I mean the 5 games I checked)
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    // LDRH
    F(A_Comp_MemHalf), F(A_Comp_MemHalf), F(A_Comp_MemHalf), F(A_Comp_MemHalf),
    // LDRSB
    F(A_Comp_MemHalf), F(A_Comp_MemHalf), F(A_Comp_MemHalf), F(A_Comp_MemHalf),
    // LDRSH
    F(A_Comp_MemHalf), F(A_Comp_MemHalf), F(A_Comp_MemHalf), F(A_Comp_MemHalf),
    // swap
    NULL, NULL,
    // LDM/STM
    F(A_Comp_LDM_STM), F(A_Comp_LDM_STM),
    // Branch
    F(A_Comp_BranchImm), F(A_Comp_BranchImm), F(A_Comp_BranchImm), F(A_Comp_BranchXchangeReg), F(A_Comp_BranchXchangeReg),
    // system stuff
    NULL, F(A_Comp_MSR), F(A_Comp_MSR), F(A_Comp_MRS), NULL, NULL, NULL,
    F(Nop)
};

const Compiler::CompileFunc T_Comp[ARMInstrInfo::tk_Count] = {
    // Shift imm
    F(T_Comp_ShiftImm), F(T_Comp_ShiftImm), F(T_Comp_ShiftImm),
    // Three operand ADD/SUB
    F(T_Comp_AddSub_), F(T_Comp_AddSub_), F(T_Comp_AddSub_), F(T_Comp_AddSub_),
    // 8 bit imm
    F(T_Comp_ALU_Imm8), F(T_Comp_ALU_Imm8), F(T_Comp_ALU_Imm8), F(T_Comp_ALU_Imm8),
    // general ALU
    F(T_Comp_ALU), F(T_Comp_ALU), F(T_Comp_ALU), F(T_Comp_ALU),
    F(T_Comp_ALU), F(T_Comp_ALU), F(T_Comp_ALU), F(T_Comp_ALU),
    F(T_Comp_ALU), F(T_Comp_ALU), F(T_Comp_ALU), F(T_Comp_ALU),
    F(T_Comp_ALU), F(T_Comp_MUL), F(T_Comp_ALU), F(T_Comp_ALU),
    // hi reg
    F(T_Comp_ALU_HiReg), F(T_Comp_ALU_HiReg), F(T_Comp_ALU_HiReg),
    // pc/sp relative
    F(T_Comp_RelAddr), F(T_Comp_RelAddr), F(T_Comp_AddSP),
    // LDR pcrel
    F(T_Comp_LoadPCRel),
    // LDR/STR reg offset
    F(T_Comp_MemReg), F(T_Comp_MemReg), F(T_Comp_MemReg), F(T_Comp_MemReg),
    // LDR/STR sign extended, half
    F(T_Comp_MemRegHalf), F(T_Comp_MemRegHalf), F(T_Comp_MemRegHalf), F(T_Comp_MemRegHalf),
    // LDR/STR imm offset
    F(T_Comp_MemImm), F(T_Comp_MemImm), F(T_Comp_MemImm), F(T_Comp_MemImm),
    // LDR/STR half imm offset
    F(T_Comp_MemImmHalf), F(T_Comp_MemImmHalf),
    // LDR/STR sp rel
    F(T_Comp_MemSPRel), F(T_Comp_MemSPRel),
    // PUSH/POP
    F(T_Comp_PUSH_POP), F(T_Comp_PUSH_POP), 
    // LDMIA, STMIA
    F(T_Comp_LDMIA_STMIA), F(T_Comp_LDMIA_STMIA), 
    // Branch
    F(T_Comp_BCOND), F(T_Comp_BranchXchangeReg), F(T_Comp_BranchXchangeReg), F(T_Comp_B), F(T_Comp_BL_LONG_1), F(T_Comp_BL_LONG_2), 
    // Unk, SVC
    NULL, NULL,
    F(T_Comp_BL_Merged)
};
#undef F

bool Compiler::CanCompile(bool thumb, u16 kind)
{
    return (thumb ? T_Comp[kind] : A_Comp[kind]) != NULL;
}

void Compiler::Reset()
{
    memset(ResetStart, 0xcc, CodeMemSize);
    SetCodePtr(ResetStart);
}

void Compiler::Comp_SpecialBranchBehaviour(bool taken)
{
    if (taken && CurInstr.BranchFlags & branch_IdleBranch)
        OR(8, MDisp(RCPU, offsetof(ARM, IdleLoop)), Imm8(0x1));

    if ((CurInstr.BranchFlags & branch_FollowCondNotTaken && taken)
        || (CurInstr.BranchFlags & branch_FollowCondTaken && !taken))
    {
        RegCache.PrepareExit();

        SUB(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm32(ConstantCycles));

        if (Config::JIT_BrancheOptimisations == 2 && !(CurInstr.BranchFlags & branch_IdleBranch)
            && (!taken || (CurInstr.BranchFlags & branch_StaticTarget)))
        {
            FixupBranch ret = J_CC(CC_S);
            CMP(32, MDisp(RCPU, offsetof(ARM, StopExecution)), Imm8(0));
            FixupBranch ret2 = J_CC(CC_NZ);

            u8* rewritePart = GetWritableCodePtr();
            NOP(5);

            MOV(32, R(ABI_PARAM2), Imm32(rewritePart - ResetStart));
            JMP((u8*)BranchStub[Num], true);

            SetJumpTarget(ret);
            SetJumpTarget(ret2);
            JMP((u8*)ARM_Ret, true);
        }
        else
        {
            JMP((u8*)&ARM_Ret, true);
        }
    }
}

JitBlockEntry Compiler::CompileBlock(u32 translatedAddr, ARM* cpu, bool thumb, FetchedInstr instrs[], int instrsCount)
{
    if (CodeMemSize - (GetWritableCodePtr() - ResetStart) < 1024 * 32) // guess...
        ResetBlockCache();

    ConstantCycles = 0;
    Thumb = thumb;
    Num = cpu->Num;
    CodeRegion = instrs[0].Addr >> 24;
    CurCPU = cpu;
    // CPSR might have been modified in a previous block
    CPSRDirty = Config::JIT_BrancheOptimisations == 2;

    JitBlockEntry res = (JitBlockEntry)GetWritableCodePtr();

    RegCache = RegisterCache<Compiler, X64Reg>(this, instrs, instrsCount);

    for (int i = 0; i < instrsCount; i++)
    {
        CurInstr = instrs[i];
        R15 = CurInstr.Addr + (Thumb ? 4 : 8);
        CodeRegion = R15 >> 24;

        Exit = i == instrsCount - 1 || (CurInstr.BranchFlags & branch_FollowCondNotTaken);

        CompileFunc comp = Thumb
            ? T_Comp[CurInstr.Info.Kind]
            : A_Comp[CurInstr.Info.Kind];

        bool isConditional = Thumb ? CurInstr.Info.Kind == ARMInstrInfo::tk_BCOND : CurInstr.Cond() < 0xE;
        if (comp == NULL || (CurInstr.BranchFlags & branch_FollowCondTaken) || (i == instrsCount - 1 && (!CurInstr.Info.Branches() || isConditional)))
        {
            MOV(32, MDisp(RCPU, offsetof(ARM, R[15])), Imm32(R15));
            if (comp == NULL)
            {
                MOV(32, MDisp(RCPU, offsetof(ARM, CodeCycles)), Imm32(CurInstr.CodeCycles));
                MOV(32, MDisp(RCPU, offsetof(ARM, CurInstr)), Imm32(CurInstr.Instr));

                SaveCPSR();
            }
        }

        if (comp != NULL)
            RegCache.Prepare(Thumb, i);
        else
            RegCache.Flush();

        if (Thumb)
        {
            if (comp == NULL)
            {
                MOV(64, R(ABI_PARAM1), R(RCPU));

                ABI_CallFunction(InterpretTHUMB[CurInstr.Info.Kind]);
            }
            else
                (this->*comp)();
        }
        else
        {
            u32 cond = CurInstr.Cond();
            if (CurInstr.Info.Kind == ARMInstrInfo::ak_BLX_IMM)
            {
                if (comp)
                    (this->*comp)();
                else
                {
                    MOV(64, R(ABI_PARAM1), R(RCPU));
                    ABI_CallFunction(ARMInterpreter::A_BLX_IMM);
                }
            }
            else if (cond == 0xF)
            {
                Comp_AddCycles_C();
            }
            else
            {
                IrregularCycles = false;

                FixupBranch skipExecute;
                if (cond < 0xE)
                    skipExecute = CheckCondition(cond);

                if (comp == NULL)
                {
                    MOV(64, R(ABI_PARAM1), R(RCPU));

                    ABI_CallFunction(InterpretARM[CurInstr.Info.Kind]);
                }
                else
                    (this->*comp)();

                Comp_SpecialBranchBehaviour(true);

                if (CurInstr.Cond() < 0xE)
                {
                    if (IrregularCycles || (CurInstr.BranchFlags & branch_FollowCondTaken))
                    {
                        FixupBranch skipFailed = J();
                        SetJumpTarget(skipExecute);

                        Comp_AddCycles_C(true);

                        Comp_SpecialBranchBehaviour(false);

                        SetJumpTarget(skipFailed);
                    }
                    else
                        SetJumpTarget(skipExecute);
                }
                
            }
        }

        if (comp == NULL)
            LoadCPSR();
    }

    RegCache.Flush();

    SUB(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm32(ConstantCycles));

    if (Config::JIT_BrancheOptimisations == 2
        && !(instrs[instrsCount - 1].BranchFlags & branch_IdleBranch)
        && (!instrs[instrsCount - 1].Info.Branches()
        || instrs[instrsCount - 1].BranchFlags & branch_FollowCondNotTaken
        || (instrs[instrsCount - 1].BranchFlags & branch_FollowCondTaken && instrs[instrsCount - 1].BranchFlags & branch_StaticTarget)))
    {
        FixupBranch ret = J_CC(CC_S);
        CMP(32, MDisp(RCPU, offsetof(ARM, StopExecution)), Imm8(0));
        FixupBranch ret2 = J_CC(CC_NZ);

        u8* rewritePart = GetWritableCodePtr();
        NOP(5);

        MOV(32, R(ABI_PARAM2), Imm32(rewritePart - ResetStart));
        JMP((u8*)BranchStub[Num], true);

        SetJumpTarget(ret);
        SetJumpTarget(ret2);
        JMP((u8*)ARM_Ret, true);
    }
    else
    {
        JMP((u8*)ARM_Ret, true);
    }

    /*FILE* codeout = fopen("codeout", "a");
    fprintf(codeout, "beginning block argargarg__ %x!!!", instrs[0].Addr);
    fwrite((u8*)res, GetWritableCodePtr() - (u8*)res, 1, codeout);

    fclose(codeout);*/

    return res;
}

void Compiler::LinkBlock(u32 offset, JitBlockEntry entry)
{
    u8* curPtr = GetWritableCodePtr();
    SetCodePtr(ResetStart + offset);
    JMP((u8*)entry, true);
    SetCodePtr(curPtr);
}

void Compiler::UnlinkBlock(u32 offset)
{
    u8* curPtr = GetWritableCodePtr();
    SetCodePtr(ResetStart + offset);
    NOP(5);
    SetCodePtr(curPtr);
}

void Compiler::Comp_AddCycles_C(bool forceNonConstant)
{
    s32 cycles = Num ?
        NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 1 : 3]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles);

    if ((!Thumb && CurInstr.Cond() < 0xE) || forceNonConstant)
        SUB(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

void Compiler::Comp_AddCycles_CI(u32 i)
{
    s32 cycles = (Num ?
        NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles)) + i;

    if (!Thumb && CurInstr.Cond() < 0xE)
        SUB(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

void Compiler::Comp_AddCycles_CI(Gen::X64Reg i, int add)
{
    s32 cycles = Num ?
        NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles);
    
    if (!Thumb && CurInstr.Cond() < 0xE)
    {
        LEA(32, RSCRATCH, MDisp(i, add + cycles));
        SUB(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(RSCRATCH));
    }
    else
    {
        ConstantCycles += cycles;
        SUB(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(i));
    }
}

void Compiler::Comp_AddCycles_CDI()
{
    if (Num == 0)
        Comp_AddCycles_CD();
    else
    {
        s32 cycles;

        s32 numC = NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2];
        s32 numD = CurInstr.DataCycles;

        if ((CurInstr.DataRegion >> 4) == 0x02) // mainRAM
        {
            if (CodeRegion == 0x02)
                cycles = numC + numD;
            else
            {
                numC++;
                cycles = std::max(numC + numD - 3, std::max(numC, numD));
            }
        }
        else if (CodeRegion == 0x02)
        {
            numD++;
            cycles = std::max(numC + numD - 3, std::max(numC, numD));
        }
        else
        {
            cycles = numC + numD + 1;
        }
        
        if (!Thumb && CurInstr.Cond() < 0xE)
            SUB(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
        else
            ConstantCycles += cycles;
    }
}

void Compiler::Comp_AddCycles_CD()
{
    u32 cycles = 0;
    if (Num == 0)
    {
        s32 numC = (R15 & 0x2) ? 0 : CurInstr.CodeCycles;
        s32 numD = CurInstr.DataCycles;

        //if (DataRegion != CodeRegion)
            cycles = std::max(numC + numD - 6, std::max(numC, numD));

        IrregularCycles = cycles != numC;
    }
    else
    {
        s32 numC = NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2];
        s32 numD = CurInstr.DataCycles;

        if ((CurInstr.DataRegion >> 4) == 0x02)
        {
            if (CodeRegion == 0x02)
                cycles += numC + numD;
            else
                cycles += std::max(numC + numD - 3, std::max(numC, numD));
        }
        else if (CodeRegion == 0x02)
        {
            cycles += std::max(numC + numD - 3, std::max(numC, numD));
        }
        else
        {
            cycles += numC + numD;
        }

        IrregularCycles = true;
    }

    if (IrregularCycles && !Thumb && CurInstr.Cond() < 0xE)
        SUB(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

}