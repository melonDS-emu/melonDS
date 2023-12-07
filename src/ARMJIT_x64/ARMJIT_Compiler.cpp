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
#include "../ARMInterpreter.h"
#include "../NDS.h"

#include <assert.h>
#include <stdarg.h>

#include "../dolphin/CommonFuncs.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace Gen;
using namespace Common;

extern "C" void ARM_Ret();

namespace melonDS
{
template <>
const X64Reg RegisterCache<Compiler, X64Reg>::NativeRegAllocOrder[] =
{
#ifdef _WIN32
    RBX, RSI, RDI, R12, R13, R14, // callee saved
    R10, R11, // caller saved
#else
    RBX, R12, R13, R14, // callee saved, this is sad
    R9, R10, R11, // caller saved
#endif
};
template <>
const int RegisterCache<Compiler, X64Reg>::NativeRegsAvailable =
#ifdef _WIN32
    8
#else
    7
#endif
;

#ifdef _WIN32
const BitSet32 CallerSavedPushRegs({R10, R11});
#else
const BitSet32 CallerSavedPushRegs({R9, R10, R11});
#endif

void Compiler::PushRegs(bool saveHiRegs, bool saveRegsToBeChanged, bool allowUnload)
{
    BitSet32 loadedRegs(RegCache.LoadedRegs);

    if (saveHiRegs)
    {
        BitSet32 hiRegsLoaded(RegCache.LoadedRegs & 0x7F00);
        for (int reg : hiRegsLoaded)
        {
            if (Thumb || CurInstr.Cond() == 0xE)
                RegCache.UnloadRegister(reg);
            else
                SaveReg(reg, RegCache.Mapping[reg]);
            // prevent saving the register twice
            loadedRegs[reg] = false;
        }
    }

    for (int reg : loadedRegs)
    {
        if (CallerSavedPushRegs[RegCache.Mapping[reg]]
            && (saveRegsToBeChanged || !((1<<reg) & CurInstr.Info.DstRegs && !((1<<reg) & CurInstr.Info.SrcRegs))))
        {
            if ((Thumb || CurInstr.Cond() == 0xE) && !((1 << reg) & (CurInstr.Info.DstRegs|CurInstr.Info.SrcRegs)) && allowUnload)
                RegCache.UnloadRegister(reg);
            else
                SaveReg(reg, RegCache.Mapping[reg]);
        }
    }
}

void Compiler::PopRegs(bool saveHiRegs, bool saveRegsToBeChanged)
{
    BitSet32 loadedRegs(RegCache.LoadedRegs);
    for (int reg : loadedRegs)
    {
        if ((saveHiRegs && reg >= 8 && reg < 15)
            || (CallerSavedPushRegs[RegCache.Mapping[reg]]
                && (saveRegsToBeChanged || !((1<<reg) & CurInstr.Info.DstRegs && !((1<<reg) & CurInstr.Info.SrcRegs)))))
        {
            LoadReg(reg, RegCache.Mapping[reg]);
        }
    }
}

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
    {
        MOV(32, rd, R(RCPSR));
    }
}

void UpdateModeTrampoline(ARM* arm, u32 oldmode, u32 newmode)
{
    arm->UpdateMode(oldmode, newmode);
}

void Compiler::A_Comp_MSR()
{
    Comp_AddCycles_C();

    OpArg val = CurInstr.Instr & (1 << 25)
        ? Imm32(melonDS::ROR((CurInstr.Instr & 0xFF), ((CurInstr.Instr >> 7) & 0x1E)))
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

            PushRegs(true, true);

            MOV(32, R(ABI_PARAM3), R(RCPSR));
            MOV(32, R(ABI_PARAM2), R(RSCRATCH3));
            MOV(64, R(ABI_PARAM1), R(RCPU));
            CALL((void*)&UpdateModeTrampoline);

            PopRegs(true, true);
        }
    }
}

/*
    We'll repurpose this .bss memory

 */
u8 CodeMemory[1024 * 1024 * 32];

Compiler::Compiler(melonDS::NDS& nds) : XEmitter(), NDS(nds)
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
    #elif defined(__APPLE__)
        pageAligned = (u8*)mmap(NULL, 1024*1024*32, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS ,-1, 0);
    #else
        mprotect(pageAligned, alignedSize, PROT_EXEC | PROT_READ | PROT_WRITE);
    #endif

        ResetStart = pageAligned;
        CodeMemSize = alignedSize;
    }

    Reset();

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

#ifdef JIT_PROFILING_ENABLED
        CreateMethod("ReadBanked", ReadBanked);
#endif
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

#ifdef JIT_PROFILING_ENABLED
        CreateMethod("WriteBanked", WriteBanked);
#endif
    }

    for (int consoleType = 0; consoleType < 2; consoleType++)
    {
        for (int num = 0; num < 2; num++)
        {
            for (int size = 0; size < 3; size++)
            {
                for (int reg = 0; reg < 16; reg++)
                {
                    if (reg == RSCRATCH || reg == ABI_PARAM1 || reg == ABI_PARAM2)
                    {
                        PatchedStoreFuncs[consoleType][num][size][reg] = NULL;
                        PatchedLoadFuncs[consoleType][num][size][0][reg] = NULL;
                        PatchedLoadFuncs[consoleType][num][size][1][reg] = NULL;
                        continue;
                    }

                    X64Reg rdMapped = (X64Reg)reg;
                    PatchedStoreFuncs[consoleType][num][size][reg] = GetWritableCodePtr();
                    if (RSCRATCH3 != ABI_PARAM1)
                        MOV(32, R(ABI_PARAM1), R(RSCRATCH3));
                    if (num == 0)
                    {
                        MOV(64, R(ABI_PARAM2), R(RCPU));
                        if (rdMapped != ABI_PARAM3)
                            MOV(32, R(ABI_PARAM3), R(rdMapped));
                    }
                    else
                    {
                        MOV(32, R(ABI_PARAM2), R(rdMapped));
                    }
                    ABI_PushRegistersAndAdjustStack(CallerSavedPushRegs, 8);
                    if (consoleType == 0)
                    {
                        switch ((8 << size) | num)
                        {
                        case 32: ABI_CallFunction(SlowWrite9<u32, 0>); break;
                        case 33: ABI_CallFunction(SlowWrite7<u32, 0>); break;
                        case 16: ABI_CallFunction(SlowWrite9<u16, 0>); break;
                        case 17: ABI_CallFunction(SlowWrite7<u16, 0>); break;
                        case 8: ABI_CallFunction(SlowWrite9<u8, 0>); break;
                        case 9: ABI_CallFunction(SlowWrite7<u8, 0>); break;
                        }
                    }
                    else
                    {
                        switch ((8 << size) | num)
                        {
                        case 32: ABI_CallFunction(SlowWrite9<u32, 1>); break;
                        case 33: ABI_CallFunction(SlowWrite7<u32, 1>); break;
                        case 16: ABI_CallFunction(SlowWrite9<u16, 1>); break;
                        case 17: ABI_CallFunction(SlowWrite7<u16, 1>); break;
                        case 8: ABI_CallFunction(SlowWrite9<u8, 1>); break;
                        case 9: ABI_CallFunction(SlowWrite7<u8, 1>); break;
                        }
                    }
                    ABI_PopRegistersAndAdjustStack(CallerSavedPushRegs, 8);
                    RET();

#ifdef JIT_PROFILING_ENABLED
                    CreateMethod("FastMemStorePatch%d_%d_%d", PatchedStoreFuncs[consoleType][num][size][reg], num, size, reg);
#endif

                    for (int signextend = 0; signextend < 2; signextend++)
                    {
                        PatchedLoadFuncs[consoleType][num][size][signextend][reg] = GetWritableCodePtr();
                        if (RSCRATCH3 != ABI_PARAM1)
                            MOV(32, R(ABI_PARAM1), R(RSCRATCH3));
                        if (num == 0)
                            MOV(64, R(ABI_PARAM2), R(RCPU));
                        ABI_PushRegistersAndAdjustStack(CallerSavedPushRegs, 8);
                        if (consoleType == 0)
                        {
                            switch ((8 << size) | num)
                            {
                            case 32: ABI_CallFunction(SlowRead9<u32, 0>); break;
                            case 33: ABI_CallFunction(SlowRead7<u32, 0>); break;
                            case 16: ABI_CallFunction(SlowRead9<u16, 0>); break;
                            case 17: ABI_CallFunction(SlowRead7<u16, 0>); break;
                            case 8: ABI_CallFunction(SlowRead9<u8, 0>); break;
                            case 9: ABI_CallFunction(SlowRead7<u8, 0>); break;
                            }
                        }
                        else
                        {
                            switch ((8 << size) | num)
                            {
                            case 32: ABI_CallFunction(SlowRead9<u32, 1>); break;
                            case 33: ABI_CallFunction(SlowRead7<u32, 1>); break;
                            case 16: ABI_CallFunction(SlowRead9<u16, 1>); break;
                            case 17: ABI_CallFunction(SlowRead7<u16, 1>); break;
                            case 8: ABI_CallFunction(SlowRead9<u8, 1>); break;
                            case 9: ABI_CallFunction(SlowRead7<u8, 1>); break;
                            }
                        }
                        ABI_PopRegistersAndAdjustStack(CallerSavedPushRegs, 8);
                        if (signextend)
                            MOVSX(32, 8 << size, rdMapped, R(RSCRATCH));
                        else
                            MOVZX(32, 8 << size, rdMapped, R(RSCRATCH));
                        RET();

#ifdef JIT_PROFILING_ENABLED
                        CreateMethod("FastMemLoadPatch%d_%d_%d_%d", PatchedLoadFuncs[consoleType][num][size][signextend][reg], num, size, reg, signextend);
#endif
                    }
                }
            }
        }
    }

    // move the region forward to prevent overwriting the generated functions
    CodeMemSize -= GetWritableCodePtr() - ResetStart;
    ResetStart = GetWritableCodePtr();

    NearStart = ResetStart;
    FarStart = ResetStart + 1024*1024*24;

    NearSize = FarStart - ResetStart;
    FarSize = (ResetStart + CodeMemSize) - FarStart;
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
        MOV(32, R(nativeReg), MDisp(RCPU, offsetof(ARM, R) + reg*4));
    else
        MOV(32, R(nativeReg), Imm32(R15));
}

void Compiler::SaveReg(int reg, X64Reg nativeReg)
{
    MOV(32, MDisp(RCPU, offsetof(ARM, R) + reg*4), R(nativeReg));
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

bool Compiler::CanCompile(bool thumb, u16 kind) const
{
    return (thumb ? T_Comp[kind] : A_Comp[kind]) != NULL;
}

void Compiler::Reset()
{
    memset(ResetStart, 0xcc, CodeMemSize);
    SetCodePtr(ResetStart);

    NearCode = NearStart;
    FarCode = FarStart;

    LoadStorePatches.clear();
}

bool Compiler::IsJITFault(const u8* addr)
{
    return (u64)addr >= (u64)ResetStart && (u64)addr < (u64)ResetStart + CodeMemSize;
}

void Compiler::Comp_SpecialBranchBehaviour(bool taken)
{
    if (taken && CurInstr.BranchFlags & branch_IdleBranch)
        OR(8, MDisp(RCPU, offsetof(ARM, IdleLoop)), Imm8(0x1));

    if ((CurInstr.BranchFlags & branch_FollowCondNotTaken && taken)
        || (CurInstr.BranchFlags & branch_FollowCondTaken && !taken))
    {
        RegCache.PrepareExit();

        if (ConstantCycles)
            ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm32(ConstantCycles));
        JMP((u8*)&ARM_Ret, true);
    }
}

#ifdef JIT_PROFILING_ENABLED
void Compiler::CreateMethod(const char* namefmt, void* start, ...)
{
    if (iJIT_IsProfilingActive())
    {
        va_list args;
        va_start(args, start);
        char name[64];
        vsprintf(name, namefmt, args);
        va_end(args);

        iJIT_Method_Load method = {0};
        method.method_id = iJIT_GetNewMethodID();
        method.method_name = name;
        method.method_load_address = start;
        method.method_size = GetWritableCodePtr() - (u8*)start;

        iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&method);
    }
}
#endif

JitBlockEntry Compiler::CompileBlock(ARM* cpu, bool thumb, FetchedInstr instrs[], int instrsCount, bool hasMemoryInstr)
{
    if (NearSize - (GetCodePtr() - NearStart) < 1024 * 32) // guess...
    {
        Log(LogLevel::Debug, "near reset\n");
        NDS.JIT.ResetBlockCache();
    }
    if (FarSize - (FarCode - FarStart) < 1024 * 32) // guess...
    {
        Log(LogLevel::Debug, "far reset\n");
        NDS.JIT.ResetBlockCache();
    }

    ConstantCycles = 0;
    Thumb = thumb;
    Num = cpu->Num;
    CodeRegion = instrs[0].Addr >> 24;
    CurCPU = cpu;
    // CPSR might have been modified in a previous block
    CPSRDirty = false;

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
            {
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
            {
                Comp_AddCycles_C();
            }
            else
            {
                IrregularCycles = comp == NULL;

                FixupBranch skipExecute;
                if (cond < 0xE)
                    skipExecute = CheckCondition(cond);

                if (comp == NULL)
                {
                    MOV(64, R(ABI_PARAM1), R(RCPU));

                    ABI_CallFunction(InterpretARM[CurInstr.Info.Kind]);
                }
                else
                {
                    (this->*comp)();
                }

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
                    {
                        SetJumpTarget(skipExecute);
                    }
                }

            }
        }

        if (comp == NULL)
            LoadCPSR();
    }

    RegCache.Flush();

    if (ConstantCycles)
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm32(ConstantCycles));
    JMP((u8*)ARM_Ret, true);

#ifdef JIT_PROFILING_ENABLED
    CreateMethod("JIT_Block_%d_%d_%08X", (void*)res, Num, Thumb, instrs[0].Addr);
#endif

    /*FILE* codeout = fopen("codeout", "a");
    fprintf(codeout, "beginning block argargarg__ %x!!!", instrs[0].Addr);
    fwrite((u8*)res, GetWritableCodePtr() - (u8*)res, 1, codeout);

    fclose(codeout);*/

    return res;
}

void Compiler::Comp_AddCycles_C(bool forceNonConstant)
{
    s32 cycles = Num ?
        NDS.ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 1 : 3]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles);

    if ((!Thumb && CurInstr.Cond() < 0xE) || forceNonConstant)
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

void Compiler::Comp_AddCycles_CI(u32 i)
{
    s32 cycles = (Num ?
        NDS.ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles)) + i;

    if (!Thumb && CurInstr.Cond() < 0xE)
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
    else
        ConstantCycles += cycles;
}

void Compiler::Comp_AddCycles_CI(Gen::X64Reg i, int add)
{
    s32 cycles = Num ?
        NDS.ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles);

    if (!Thumb && CurInstr.Cond() < 0xE)
    {
        LEA(32, RSCRATCH, MDisp(i, add + cycles));
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(RSCRATCH));
    }
    else
    {
        ConstantCycles += cycles;
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), R(i));
    }
}

void Compiler::Comp_AddCycles_CDI()
{
    if (Num == 0)
        Comp_AddCycles_CD();
    else
    {
        IrregularCycles = true;

        s32 cycles;

        s32 numC = NDS.ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2];
        s32 numD = CurInstr.DataCycles;

        if ((CurInstr.DataRegion >> 24) == 0x02) // mainRAM
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
            ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
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
        s32 numC = NDS.ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2];
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
        ADD(32, MDisp(RCPU, offsetof(ARM, Cycles)), Imm8(cycles));
    else
        ConstantCycles += cycles;
}
}
