#include "ARMJIT_Compiler.h"

#include "../ARMInterpreter.h"

#include <assert.h>

#include "../dolphin/CommonFuncs.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace Gen;

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

        region = pageAligned;
        region_size = alignedSize;
        total_region_size = region_size;
    }

    ClearCodeSpace();

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            MemoryFuncs9[i][j] = Gen_MemoryRoutine9(j, 8 << i);
            MemoryFuncs7[i][j][0] = Gen_MemoryRoutine7(j, false, 8 << i);
            MemoryFuncs7[i][j][1] = Gen_MemoryRoutine7(j, true, 8 << i);
        }
    }
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
        {
            MemoryFuncsSeq9[i][j] = Gen_MemoryRoutineSeq9(i, j);
            MemoryFuncsSeq7[i][j][0] = Gen_MemoryRoutineSeq7(i, j, false);
            MemoryFuncsSeq7[i][j][1] = Gen_MemoryRoutineSeq7(i, j, true);
        }

    {
        // RSCRATCH mode
        // ABI_PARAM2 reg number
        // ABI_PARAM3 value in current mode
        // ret - ABI_PARAM3
        ReadBanked = (void*)GetWritableCodePtr();
        CMP(32, R(RSCRATCH), Imm8(0x11));
        FixupBranch fiq = J_CC(CC_E);
        SUB(32, R(ABI_PARAM2), Imm8(13 - 8));
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
        MOV(32, R(ABI_PARAM3), MComplex(RCPU, ABI_PARAM2, SCALE_4, offsetof(ARM, R_FIQ)));
        RET();
        SetJumpTarget(irq);
        MOV(32, R(ABI_PARAM3), MComplex(RCPU, ABI_PARAM2, SCALE_4, offsetof(ARM, R_IRQ)));
        RET();
        SetJumpTarget(svc);
        MOV(32, R(ABI_PARAM3), MComplex(RCPU, ABI_PARAM2, SCALE_4, offsetof(ARM, R_SVC)));
        RET();
        SetJumpTarget(abt);
        MOV(32, R(ABI_PARAM3), MComplex(RCPU, ABI_PARAM2, SCALE_4, offsetof(ARM, R_ABT)));
        RET();
        SetJumpTarget(und);
        MOV(32, R(ABI_PARAM3), MComplex(RCPU, ABI_PARAM2, SCALE_4, offsetof(ARM, R_UND)));
        RET();
    }
    {
        // RSCRATCH  mode
        // ABI_PARAM2 reg n
        // ABI_PARAM3 value
        // carry flag set if the register isn't banked
        WriteBanked = (void*)GetWritableCodePtr();
        CMP(32, R(RSCRATCH), Imm8(0x11));
        FixupBranch fiq = J_CC(CC_E);
        SUB(32, R(ABI_PARAM2), Imm8(13 - 8));
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
        MOV(32, MComplex(RCPU, ABI_PARAM2, SCALE_4, offsetof(ARM, R_FIQ)), R(ABI_PARAM3));
        CLC();
        RET();
        SetJumpTarget(irq);
        MOV(32, MComplex(RCPU, ABI_PARAM2, SCALE_4, offsetof(ARM, R_IRQ)), R(ABI_PARAM3));
        CLC();
        RET();
        SetJumpTarget(svc);
        MOV(32, MComplex(RCPU, ABI_PARAM2, SCALE_4, offsetof(ARM, R_SVC)), R(ABI_PARAM3));
        CLC();
        RET();
        SetJumpTarget(abt);
        MOV(32, MComplex(RCPU, ABI_PARAM2, SCALE_4, offsetof(ARM, R_ABT)), R(ABI_PARAM3));
        CLC();
        RET();
        SetJumpTarget(und);
        MOV(32, MComplex(RCPU, ABI_PARAM2, SCALE_4, offsetof(ARM, R_UND)), R(ABI_PARAM3));
        CLC();
        RET();
    }

    // move the region forward to prevent overwriting the generated functions
    region_size -= GetWritableCodePtr() - region;
    total_region_size = region_size;
    region = GetWritableCodePtr();
}

void Compiler::LoadCPSR()
{
    assert(!CPSRDirty);

    MOV(32, R(RCPSR), MDisp(RCPU, offsetof(ARM, CPSR)));
}

void Compiler::SaveCPSR()
{
    if (CPSRDirty)
    {
        MOV(32, MDisp(RCPU, offsetof(ARM, CPSR)), R(RCPSR));
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
    if (cond >= 0x8)
    {
        static_assert(RSCRATCH3 == ECX);
        MOV(32, R(RSCRATCH3), R(RCPSR));
        SHR(32, R(RSCRATCH3), Imm8(28));
        MOV(32, R(RSCRATCH), Imm32(1));
        SHL(32, R(RSCRATCH), R(RSCRATCH3));
        TEST(32, R(RSCRATCH), Imm32(ARM::ConditionTable[cond]));

        return J_CC(CC_Z);
    }
    else
    {
        // could have used a LUT, but then where would be the fun?
        TEST(32, R(RCPSR), Imm32(1 << (28 + ((~(cond >> 1) & 1) << 1 | (cond >> 2 & 1) ^ (cond >> 1 & 1)))));

        return J_CC(cond & 1 ? CC_NZ : CC_Z);
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
    F(A_Comp_MUL_MLA), F(A_Comp_MUL_MLA), NULL, NULL, NULL, F(A_Comp_SMULL_SMLAL), NULL, NULL, NULL, NULL, NULL,
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
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,
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
    NULL, NULL
};
#undef F

void Compiler::Reset()
{
    ClearCodeSpace();
}

CompiledBlock Compiler::CompileBlock(ARM* cpu, FetchedInstr instrs[], int instrsCount)
{
    if (IsAlmostFull())
        InvalidateBlockCache();

    ConstantCycles = 0;
    Thumb = cpu->CPSR & 0x20;
    Num = cpu->Num;
    R15 = cpu->R[15];
    CodeRegion = cpu->CodeRegion;
    CurCPU = cpu;

    CompiledBlock res = (CompiledBlock)GetWritableCodePtr();

    if (!(Num == 0 
        ? IsMapped<0>(R15 - (Thumb ? 2 : 4)) 
        : IsMapped<1>(R15 - (Thumb ? 2 : 4))))
    {
        printf("Trying to compile a block in unmapped memory\n");
    }

    bool mergedThumbBL = false;

    ABI_PushRegistersAndAdjustStack(BitSet32(ABI_ALL_CALLEE_SAVED & ABI_ALL_GPRS & ~BitSet32({RSP})), 8);

    MOV(64, R(RCPU), ImmPtr(cpu));

    LoadCPSR();

    // TODO: this is ugly as a whole, do better
    RegCache = RegisterCache<Compiler, X64Reg>(this, instrs, instrsCount);

    for (int i = 0; i < instrsCount; i++)
    {
        R15 += Thumb ? 2 : 4;
        CurInstr = instrs[i];

        CompileFunc comp = Thumb
            ? T_Comp[CurInstr.Info.Kind]
            : A_Comp[CurInstr.Info.Kind];

        bool isConditional = Thumb ? CurInstr.Info.Kind == ARMInstrInfo::tk_BCOND : CurInstr.Cond() < 0xE;
        if (comp == NULL || (i == instrsCount - 1 && (!CurInstr.Info.Branches() || isConditional)))
        {
            MOV(32, MDisp(RCPU, offsetof(ARM, R[15])), Imm32(R15));
            MOV(32, MDisp(RCPU, offsetof(ARM, CodeCycles)), Imm32(CurInstr.CodeCycles));
            MOV(32, MDisp(RCPU, offsetof(ARM, CurInstr)), Imm32(CurInstr.Instr));
            if (i == instrsCount - 1)
            {
                MOV(32, MDisp(RCPU, offsetof(ARM, NextInstr[0])), Imm32(CurInstr.NextInstr[0]));
                MOV(32, MDisp(RCPU, offsetof(ARM, NextInstr[1])), Imm32(CurInstr.NextInstr[1]));
            }

            if (comp == NULL)
                SaveCPSR();
        }
        
        if (comp != NULL)
            RegCache.Prepare(i);
        else
            RegCache.Flush();

        if (Thumb)
        {
            if (i < instrsCount - 1 && CurInstr.Info.Kind == ARMInstrInfo::tk_BL_LONG_1
                && instrs[i + 1].Info.Kind == ARMInstrInfo::tk_BL_LONG_2)
                mergedThumbBL = true;
            else
            {
                u32 icode = (CurInstr.Instr >> 6) & 0x3FF;
                if (comp == NULL)
                {
                    MOV(64, R(ABI_PARAM1), R(RCPU));

                    ABI_CallFunction(ARMInterpreter::THUMBInstrTable[icode]);
                }
                else if (mergedThumbBL)
                    T_Comp_BL_Merged(instrs[i - 1]);
                else
                    (this->*comp)();
            }
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
                Comp_AddCycles_C();
            else
            {
                FixupBranch skipExecute;
                if (cond < 0xE)
                    skipExecute = CheckCondition(cond);

                u32 icode = ((CurInstr.Instr >> 4) & 0xF) | ((CurInstr.Instr >> 16) & 0xFF0);
                if (comp == NULL)
                {
                    MOV(64, R(ABI_PARAM1), R(RCPU));

                    ABI_CallFunction(ARMInterpreter::ARMInstrTable[icode]);
                }
                else
                    (this->*comp)();

                if (CurInstr.Cond() < 0xE)
                {
                    FixupBranch skipFailed = J();
                    SetJumpTarget(skipExecute);

                    Comp_AddCycles_C();

                    SetJumpTarget(skipFailed);
                }
            }
        }

        if (comp == NULL && i != instrsCount - 1)
            LoadCPSR();
    }

    RegCache.Flush();
    SaveCPSR();

    MOV(32, R(RAX), Imm32(ConstantCycles));

    ABI_PopRegistersAndAdjustStack(BitSet32(ABI_ALL_CALLEE_SAVED & ABI_ALL_GPRS & ~BitSet32({RSP})), 8);
    RET();

    return res;
}

void Compiler::Comp_AddCycles_C(bool forceNonConstant)
{
    s32 cycles = Num ?
        NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 1 : 3]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles);

    if ((!Thumb && CurInstr.Cond() < 0xE) || forceNonConstant)
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

void Compiler::Comp_AddCycles_CI(u32 i)
{
    s32 cycles = (Num ?
        NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles)) + i;

    if (!Thumb && CurInstr.Cond() < 0xE)
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

void Compiler::Comp_AddCycles_CI(Gen::X64Reg i, int add)
{
    s32 cycles = Num ?
        NDS::ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles);
    
    LEA(32, RSCRATCH, MDisp(i, add + cycles));
    ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(RSCRATCH));
}

}