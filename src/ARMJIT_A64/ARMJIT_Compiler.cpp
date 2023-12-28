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

#include "../ARMJIT_Internal.h"
#include "../ARMInterpreter.h"
#include "../ARMJIT.h"
#include "../NDS.h"

#if defined(__SWITCH__)
#include <switch.h>

extern char __start__;
#elif defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <stdlib.h>

using namespace Arm64Gen;

extern "C" void ARM_Ret();

namespace melonDS
{

/*

    Recompiling classic ARM to ARMv8 code is at the same time
    easier and trickier than compiling to a less related architecture
    like x64. At one hand you can translate a lot of instructions directly.
    But at the same time, there are a ton of exceptions, like for
    example ADD and SUB can't have a RORed second operand on ARMv8.
 
    While writing a JIT when an instruction is recompiled into multiple ones
    not to write back until you've read all the other operands!
*/

template <>
const ARM64Reg RegisterCache<Compiler, ARM64Reg>::NativeRegAllocOrder[] =
{
    W19, W20, W21, W22, W23, W24, W25,
    W8, W9, W10, W11, W12, W13, W14, W15
};
template <>
const int RegisterCache<Compiler, ARM64Reg>::NativeRegsAvailable = 15;

const BitSet32 CallerSavedPushRegs({W8, W9, W10, W11, W12, W13, W14, W15});

const int JitMemSize = 16 * 1024 * 1024;
#ifndef __SWITCH__
u8 JitMem[JitMemSize];
#endif

void Compiler::MovePC()
{
    ADD(MapReg(15), MapReg(15), Thumb ? 2 : 4);
}

void Compiler::A_Comp_MRS()
{
    Comp_AddCycles_C();

    ARM64Reg rd = MapReg(CurInstr.A_Reg(12));

    if (CurInstr.Instr & (1 << 22))
    {
        ANDI2R(W5, RCPSR, 0x1F);
        MOVI2R(W3, 0);
        MOVI2R(W1, 15 - 8);
        BL(ReadBanked);
        MOV(rd, W3);
    }
    else
        MOV(rd, RCPSR);
}

void UpdateModeTrampoline(ARM* arm, u32 oldmode, u32 newmode)
{
    arm->UpdateMode(oldmode, newmode);
}

void Compiler::A_Comp_MSR()
{
    Comp_AddCycles_C();

    ARM64Reg val;
    if (CurInstr.Instr & (1 << 25))
    {
        val = W0;
        MOVI2R(val, melonDS::ROR((CurInstr.Instr & 0xFF), ((CurInstr.Instr >> 7) & 0x1E)));
    }
    else
    {
        val = MapReg(CurInstr.A_Reg(0));
    }

    u32 mask = 0;
    if (CurInstr.Instr & (1<<16)) mask |= 0x000000FF;
    if (CurInstr.Instr & (1<<17)) mask |= 0x0000FF00;
    if (CurInstr.Instr & (1<<18)) mask |= 0x00FF0000;
    if (CurInstr.Instr & (1<<19)) mask |= 0xFF000000;

    if (CurInstr.Instr & (1 << 22))
    {
        ANDI2R(W5, RCPSR, 0x1F);
        MOVI2R(W3, 0);
        MOVI2R(W1, 15 - 8);
        BL(ReadBanked);

        MOVI2R(W1, mask);
        MOVI2R(W2, mask & 0xFFFFFF00);
        ANDI2R(W5, RCPSR, 0x1F);
        CMP(W5, 0x10);
        CSEL(W1, W2, W1, CC_EQ);

        BIC(W3, W3, W1);
        AND(W0, val, W1);
        ORR(W3, W3, W0);

        MOVI2R(W1, 15 - 8);

        BL(WriteBanked);
    }
    else
    {
        mask &= 0xFFFFFFDF;
        CPSRDirty = true;

        if ((mask & 0xFF) == 0)
        {
            ANDI2R(RCPSR, RCPSR, ~mask);
            ANDI2R(W0, val, mask);
            ORR(RCPSR, RCPSR, W0);
        }
        else
        {
            MOVI2R(W2, mask);
            MOVI2R(W3, mask & 0xFFFFFF00);
            ANDI2R(W1, RCPSR, 0x1F);
            // W1 = first argument
            CMP(W1, 0x10);
            CSEL(W2, W3, W2, CC_EQ);

            BIC(RCPSR, RCPSR, W2);
            AND(W0, val, W2);
            ORR(RCPSR, RCPSR, W0);

            MOV(W2, RCPSR);
            MOV(X0, RCPU);

            PushRegs(true, true);
            QuickCallFunction(X3, UpdateModeTrampoline);
            PopRegs(true, true);
        }
    }
}


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

Compiler::Compiler(melonDS::NDS& nds) : Arm64Gen::ARM64XEmitter(), NDS(nds)
{
#ifdef __SWITCH__
    JitRWBase = aligned_alloc(0x1000, JitMemSize);

    JitRXStart = (u8*)&__start__ - JitMemSize - 0x1000;
    virtmemLock();
    JitRWStart = virtmemFindAslr(JitMemSize, 0x1000);
    MemoryInfo info = {0};
    u32 pageInfo = {0};
    int i = 0;
    while (JitRXStart != NULL)
    {
        svcQueryMemory(&info, &pageInfo, (u64)JitRXStart);
        if (info.type != MemType_Unmapped)
            JitRXStart = (void*)((u8*)info.addr - JitMemSize - 0x1000);
        else
            break;
        if (i++ > 8)
        {
            Log(LogLevel::Error, "couldn't find unmapped place for jit memory\n");
            JitRXStart = NULL;
        }
    }

    assert(JitRXStart != NULL);

    bool succeded = R_SUCCEEDED(svcMapProcessCodeMemory(envGetOwnProcessHandle(), (u64)JitRXStart, (u64)JitRWBase, JitMemSize));
    assert(succeded);
    succeded = R_SUCCEEDED(svcSetProcessMemoryPermission(envGetOwnProcessHandle(), (u64)JitRXStart, JitMemSize, Perm_Rx));
    assert(succeded);
    succeded = R_SUCCEEDED(svcMapProcessMemory(JitRWStart, envGetOwnProcessHandle(), (u64)JitRXStart, JitMemSize));
    assert(succeded);

    virtmemUnlock();

    SetCodeBase((u8*)JitRWStart, (u8*)JitRXStart);
    JitMemMainSize = JitMemSize;
#else
    #ifdef _WIN32
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        u64 pageSize = (u64)sysInfo.dwPageSize;
    #else
        u64 pageSize = sysconf(_SC_PAGE_SIZE);
    #endif
    u8* pageAligned = (u8*)(((u64)JitMem & ~(pageSize - 1)) + pageSize);
    u64 alignedSize = (((u64)JitMem + sizeof(JitMem)) & ~(pageSize - 1)) - (u64)pageAligned;

    #if defined(_WIN32)
        DWORD dummy;
        VirtualProtect(pageAligned, alignedSize, PAGE_EXECUTE_READWRITE, &dummy);
    #elif defined(__APPLE__)
        pageAligned = (u8*)mmap(NULL, 1024*1024*16, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT,-1, 0);
        nds.JIT.JitEnableWrite();
    #else
        mprotect(pageAligned, alignedSize, PROT_EXEC | PROT_READ | PROT_WRITE);
    #endif

    SetCodeBase(pageAligned, pageAligned);
    JitMemMainSize = alignedSize;
#endif
    SetCodePtr(0);

    for (int i = 0; i < 3; i++)
    {
        JumpToFuncs9[i] = Gen_JumpTo9(i);
        JumpToFuncs7[i] = Gen_JumpTo7(i);
    }

    /*
        W4 - whether the register was written to
        W5 - mode
        W1 - reg num
        W3 - in/out value of reg
    */
    {
        ReadBanked = GetRXPtr();

        ADD(X2, RCPU, X1, ArithOption(X2, ST_LSL, 2));
        CMP(W5, 0x11);
        FixupBranch fiq = B(CC_EQ);
        SUBS(W1, W1, 13 - 8);
        ADD(X2, RCPU, X1, ArithOption(X2, ST_LSL, 2));
        FixupBranch notEverything = B(CC_LT);
        CMP(W5, 0x12);
        FixupBranch irq = B(CC_EQ);
        CMP(W5, 0x13);
        FixupBranch svc = B(CC_EQ);
        CMP(W5, 0x17);
        FixupBranch abt = B(CC_EQ);
        CMP(W5, 0x1B);
        FixupBranch und = B(CC_EQ);
        SetJumpTarget(notEverything);
        RET();

        SetJumpTarget(fiq);
        LDR(INDEX_UNSIGNED, W3, X2, offsetof(ARM, R_FIQ));
        RET();
        SetJumpTarget(irq);
        LDR(INDEX_UNSIGNED, W3, X2, offsetof(ARM, R_IRQ));
        RET();
        SetJumpTarget(svc);
        LDR(INDEX_UNSIGNED, W3, X2, offsetof(ARM, R_SVC));
        RET();
        SetJumpTarget(abt);
        LDR(INDEX_UNSIGNED, W3, X2, offsetof(ARM, R_ABT));
        RET();
        SetJumpTarget(und);
        LDR(INDEX_UNSIGNED, W3, X2, offsetof(ARM, R_UND));
        RET();
    }
    {
        WriteBanked = GetRXPtr();

        ADD(X2, RCPU, X1, ArithOption(X2, ST_LSL, 2));
        CMP(W5, 0x11);
        FixupBranch fiq = B(CC_EQ);
        SUBS(W1, W1, 13 - 8);
        ADD(X2, RCPU, X1, ArithOption(X2, ST_LSL, 2));
        FixupBranch notEverything = B(CC_LT);
        CMP(W5, 0x12);
        FixupBranch irq = B(CC_EQ);
        CMP(W5, 0x13);
        FixupBranch svc = B(CC_EQ);
        CMP(W5, 0x17);
        FixupBranch abt = B(CC_EQ);
        CMP(W5, 0x1B);
        FixupBranch und = B(CC_EQ);
        SetJumpTarget(notEverything);
        MOVI2R(W4, 0);
        RET();

        SetJumpTarget(fiq);
        STR(INDEX_UNSIGNED, W3, X2, offsetof(ARM, R_FIQ));
        MOVI2R(W4, 1);
        RET();
        SetJumpTarget(irq);
        STR(INDEX_UNSIGNED, W3, X2, offsetof(ARM, R_IRQ));
        MOVI2R(W4, 1);
        RET();
        SetJumpTarget(svc);
        STR(INDEX_UNSIGNED, W3, X2, offsetof(ARM, R_SVC));
        MOVI2R(W4, 1);
        RET();
        SetJumpTarget(abt);
        STR(INDEX_UNSIGNED, W3, X2, offsetof(ARM, R_ABT));
        MOVI2R(W4, 1);
        RET();
        SetJumpTarget(und);
        STR(INDEX_UNSIGNED, W3, X2, offsetof(ARM, R_UND));
        MOVI2R(W4, 1);
        RET();
    }

    for (int consoleType = 0; consoleType < 2; consoleType++)
    {
        for (int num = 0; num < 2; num++)
        {
            for (int size = 0; size < 3; size++)
            {
                for (int reg = 0; reg < 32; reg++)
                {
                    if (!(reg == W4 || (reg >= W8 && reg <= W15) || (reg >= W19 && reg <= W25)))
                        continue;
                    ARM64Reg rdMapped = (ARM64Reg)reg;
                    PatchedStoreFuncs[consoleType][num][size][reg] = GetRXPtr();
                    if (num == 0)
                    {
                        MOV(X1, RCPU);
                        MOV(W2, rdMapped);
                    }
                    else
                    {
                        MOV(W1, rdMapped);
                    }
                    ABI_PushRegisters(BitSet32({30}) | CallerSavedPushRegs);
                    if (consoleType == 0)
                    {
                        switch ((8 << size) |  num)
                        {
                        case 32: QuickCallFunction(X3, SlowWrite9<u32, 0>); break;
                        case 33: QuickCallFunction(X3, SlowWrite7<u32, 0>); break;
                        case 16: QuickCallFunction(X3, SlowWrite9<u16, 0>); break;
                        case 17: QuickCallFunction(X3, SlowWrite7<u16, 0>); break;
                        case 8: QuickCallFunction(X3, SlowWrite9<u8, 0>); break;
                        case 9: QuickCallFunction(X3, SlowWrite7<u8, 0>); break;
                        }
                    }
                    else
                    {
                        switch ((8 << size) |  num)
                        {
                        case 32: QuickCallFunction(X3, SlowWrite9<u32, 1>); break;
                        case 33: QuickCallFunction(X3, SlowWrite7<u32, 1>); break;
                        case 16: QuickCallFunction(X3, SlowWrite9<u16, 1>); break;
                        case 17: QuickCallFunction(X3, SlowWrite7<u16, 1>); break;
                        case 8: QuickCallFunction(X3, SlowWrite9<u8, 1>); break;
                        case 9: QuickCallFunction(X3, SlowWrite7<u8, 1>); break;
                        }
                    }
                    
                    ABI_PopRegisters(BitSet32({30}) | CallerSavedPushRegs);
                    RET();

                    for (int signextend = 0; signextend < 2; signextend++)
                    {
                        PatchedLoadFuncs[consoleType][num][size][signextend][reg] = GetRXPtr();
                        if (num == 0)
                            MOV(X1, RCPU);
                        ABI_PushRegisters(BitSet32({30}) | CallerSavedPushRegs);
                        if (consoleType == 0)
                        {
                            switch ((8 << size) |  num)
                            {
                            case 32: QuickCallFunction(X3, SlowRead9<u32, 0>); break;
                            case 33: QuickCallFunction(X3, SlowRead7<u32, 0>); break;
                            case 16: QuickCallFunction(X3, SlowRead9<u16, 0>); break;
                            case 17: QuickCallFunction(X3, SlowRead7<u16, 0>); break;
                            case 8: QuickCallFunction(X3, SlowRead9<u8, 0>); break;
                            case 9: QuickCallFunction(X3, SlowRead7<u8, 0>); break;
                            }
                        }
                        else
                        {
                            switch ((8 << size) |  num)
                            {
                            case 32: QuickCallFunction(X3, SlowRead9<u32, 1>); break;
                            case 33: QuickCallFunction(X3, SlowRead7<u32, 1>); break;
                            case 16: QuickCallFunction(X3, SlowRead9<u16, 1>); break;
                            case 17: QuickCallFunction(X3, SlowRead7<u16, 1>); break;
                            case 8: QuickCallFunction(X3, SlowRead9<u8, 1>); break;
                            case 9: QuickCallFunction(X3, SlowRead7<u8, 1>); break;
                            }
                        }
                        ABI_PopRegisters(BitSet32({30}) | CallerSavedPushRegs);
                        if (size == 32)
                            MOV(rdMapped, W0);
                        else if (signextend)
                            SBFX(rdMapped, W0, 0, 8 << size);
                        else
                            UBFX(rdMapped, W0, 0, 8 << size);
                        RET();
                    }
                }
            }
        }
    }

    FlushIcache();

    JitMemSecondarySize = 1024*1024*4;

    JitMemMainSize -= GetCodeOffset();
    JitMemMainSize -= JitMemSecondarySize;

    SetCodeBase((u8*)GetRWPtr(), (u8*)GetRXPtr());
}

Compiler::~Compiler()
{
#ifdef __SWITCH__
    if (JitRWStart != NULL)
    {
        bool succeded = R_SUCCEEDED(svcUnmapProcessMemory(JitRWStart, envGetOwnProcessHandle(), (u64)JitRXStart, JitMemSize));
        assert(succeded);
        succeded = R_SUCCEEDED(svcUnmapProcessCodeMemory(envGetOwnProcessHandle(), (u64)JitRXStart, (u64)JitRWBase, JitMemSize));
        assert(succeded);
        free(JitRWBase);
    }
#endif
}

void Compiler::LoadCycles()
{
    LDR(INDEX_UNSIGNED, RCycles, RCPU, offsetof(ARM, Cycles));
}

void Compiler::SaveCycles()
{
    STR(INDEX_UNSIGNED, RCycles, RCPU, offsetof(ARM, Cycles));
}

void Compiler::LoadReg(int reg, ARM64Reg nativeReg)
{
    if (reg == 15)
        MOVI2R(nativeReg, R15);
    else
        LDR(INDEX_UNSIGNED, nativeReg, RCPU, offsetof(ARM, R) + reg*4);
}

void Compiler::SaveReg(int reg, ARM64Reg nativeReg)
{
    STR(INDEX_UNSIGNED, nativeReg, RCPU, offsetof(ARM, R) + reg*4);
}

void Compiler::LoadCPSR()
{
    assert(!CPSRDirty);
    LDR(INDEX_UNSIGNED, RCPSR, RCPU, offsetof(ARM, CPSR));
}

void Compiler::SaveCPSR(bool markClean)
{
    if (CPSRDirty)
    {
        STR(INDEX_UNSIGNED, RCPSR, RCPU, offsetof(ARM, CPSR));
        CPSRDirty = CPSRDirty && !markClean;
    }
}

FixupBranch Compiler::CheckCondition(u32 cond)
{
    if (cond >= 0x8)
    {
        LSR(W1, RCPSR, 28);
        MOVI2R(W2, 1);
        LSLV(W2, W2, W1);
        ANDI2R(W2, W2, ARM::ConditionTable[cond], W3);

        return CBZ(W2);
    }
    else
    {
        u8 bit = (28 + ((~(cond >> 1) & 1) << 1 | (cond >> 2 & 1) ^ (cond >> 1 & 1)));

        if (cond & 1)
            return TBNZ(RCPSR, bit);
        else
            return TBZ(RCPSR, bit);
    }
}

#define F(x) &Compiler::A_Comp_##x
const Compiler::CompileFunc A_Comp[ARMInstrInfo::ak_Count] =
{
    // AND
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    // EOR
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    // SUB
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    // RSB
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    // ADD
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    // ADC
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    // SBC
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    // RSC
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    // ORR
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    // MOV
    F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp),
    F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp),
    // BIC
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp), F(ALUTriOp),
    // MVN
    F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp),
    F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp), F(ALUMovOp),
    // TST
    F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp),
    // TEQ
    F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp),
    // CMP
    F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp),
    // CMN
    F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp), F(ALUCmpOp),
    // Mul
    F(Mul), F(Mul), F(Mul_Long), F(Mul_Long), F(Mul_Long), F(Mul_Long), F(Mul_Short), F(Mul_Short), F(Mul_Short), F(Mul_Short), F(Mul_Short),
    // ARMv5 exclusives
    F(Clz), NULL, NULL, NULL, NULL, 
    
    // STR
    F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB),
    // STRB
    F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB),
    // LDR
    F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB),
    // LDRB
    F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB), F(MemWB),
    // STRH
    F(MemHD), F(MemHD), F(MemHD), F(MemHD),
    // LDRD
    NULL, NULL, NULL, NULL,
    // STRD
    NULL, NULL, NULL, NULL,
    // LDRH
    F(MemHD), F(MemHD), F(MemHD), F(MemHD),
    // LDRSB
    F(MemHD), F(MemHD), F(MemHD), F(MemHD),
    // LDRSH
    F(MemHD), F(MemHD), F(MemHD), F(MemHD),
    // Swap
    NULL, NULL,
    // LDM, STM
    F(LDM_STM), F(LDM_STM),
    // Branch
    F(BranchImm), F(BranchImm), F(BranchImm), F(BranchXchangeReg), F(BranchXchangeReg),
    // Special
    NULL, F(MSR), F(MSR), F(MRS), NULL, NULL, NULL,
    &Compiler::Nop
};
#undef F
#define F(x) &Compiler::T_Comp_##x
const Compiler::CompileFunc T_Comp[ARMInstrInfo::tk_Count] =
{
    // Shift imm
    F(ShiftImm), F(ShiftImm), F(ShiftImm),
    // Add/sub tri operand
    F(AddSub_), F(AddSub_), F(AddSub_), F(AddSub_),
    // 8 bit imm
    F(ALUImm8), F(ALUImm8), F(ALUImm8), F(ALUImm8),
    // ALU
    F(ALU), F(ALU), F(ALU), F(ALU), F(ALU), F(ALU), F(ALU), F(ALU),
    F(ALU), F(ALU), F(ALU), F(ALU), F(ALU), F(ALU), F(ALU), F(ALU),
    // ALU hi reg
    F(ALU_HiReg), F(ALU_HiReg), F(ALU_HiReg),
    // PC/SP relative ops
    F(RelAddr), F(RelAddr), F(AddSP),
    // LDR PC rel
    F(LoadPCRel),
    // LDR/STR reg offset
    F(MemReg), F(MemReg), F(MemReg), F(MemReg),
    // LDR/STR sign extended, half
    F(MemRegHalf), F(MemRegHalf), F(MemRegHalf), F(MemRegHalf),
    // LDR/STR imm offset
    F(MemImm), F(MemImm), F(MemImm), F(MemImm),
    // LDR/STR half imm offset
    F(MemImmHalf), F(MemImmHalf),
    // LDR/STR sp rel
    F(MemSPRel), F(MemSPRel),
    // PUSH/POP
    F(PUSH_POP), F(PUSH_POP),
    // LDMIA, STMIA
    F(LDMIA_STMIA), F(LDMIA_STMIA),
    // Branch
    F(BCOND), F(BranchXchangeReg), F(BranchXchangeReg), F(B), F(BL_LONG_1), F(BL_LONG_2),
    // Unk, SVC
    NULL, NULL,
    F(BL_Merged)
};

bool Compiler::CanCompile(bool thumb, u16 kind)
{
    return (thumb ? T_Comp[kind] : A_Comp[kind]) != NULL;
}

void Compiler::Comp_BranchSpecialBehaviour(bool taken)
{
    if (taken && CurInstr.BranchFlags & branch_IdleBranch)
    {
        MOVI2R(W0, 1);
        STRB(INDEX_UNSIGNED, W0, RCPU, offsetof(ARM, IdleLoop));
    }

    if ((CurInstr.BranchFlags & branch_FollowCondNotTaken && taken)
        || (CurInstr.BranchFlags & branch_FollowCondTaken && !taken))
    {
        RegCache.PrepareExit();

        if (ConstantCycles)
            ADD(RCycles, RCycles, ConstantCycles);
        QuickTailCall(X0, ARM_Ret);
    }
}

JitBlockEntry Compiler::CompileBlock(ARM* cpu, bool thumb, FetchedInstr instrs[], int instrsCount, bool hasMemInstr)
{
    if (JitMemMainSize - GetCodeOffset() < 1024 * 16)
    {
        Log(LogLevel::Debug, "JIT near memory full, resetting...\n");
        NDS.JIT.ResetBlockCache();
    }
    if ((JitMemMainSize +  JitMemSecondarySize) - OtherCodeRegion < 1024 * 8)
    {
        Log(LogLevel::Debug, "JIT far memory full, resetting...\n");
        NDS.JIT.ResetBlockCache();
    }

    JitBlockEntry res = (JitBlockEntry)GetRXPtr();

    Thumb = thumb;
    Num = cpu->Num;
    CurCPU = cpu;
    ConstantCycles = 0;
    RegCache = RegisterCache<Compiler, ARM64Reg>(this, instrs, instrsCount, true);
    CPSRDirty = false;

    if (hasMemInstr)
        MOVP2R(RMemBase, Num == 0 ? NDS.JIT.Memory.FastMem9Start : NDS.JIT.Memory.FastMem7Start);

    for (int i = 0; i < instrsCount; i++)
    {
        CurInstr = instrs[i];
        R15 = CurInstr.Addr + (Thumb ? 4 : 8);
        CodeRegion = R15 >> 24;

        CompileFunc comp = Thumb
            ? T_Comp[CurInstr.Info.Kind]
            : A_Comp[CurInstr.Info.Kind];

        Exit = i == (instrsCount - 1) || (CurInstr.BranchFlags & branch_FollowCondNotTaken);

        //printf("%x instr %x regs: r%x w%x n%x flags: %x %x %x\n", R15, CurInstr.Instr, CurInstr.Info.SrcRegs, CurInstr.Info.DstRegs, CurInstr.Info.ReadFlags, CurInstr.Info.NotStrictlyNeeded, CurInstr.Info.WriteFlags, CurInstr.SetFlags);

        bool isConditional = Thumb ? CurInstr.Info.Kind == ARMInstrInfo::tk_BCOND : CurInstr.Cond() < 0xE;
        if (comp == NULL || (CurInstr.BranchFlags & branch_FollowCondTaken) || (i == instrsCount - 1 && (!CurInstr.Info.Branches() || isConditional)))
        {
            MOVI2R(W0, R15);
            STR(INDEX_UNSIGNED, W0, RCPU, offsetof(ARM, R[15]));
            if (comp == NULL)
            {
                MOVI2R(W0, CurInstr.Instr);
                STR(INDEX_UNSIGNED, W0, RCPU, offsetof(ARM, CurInstr));
            }
            if (Num == 0)
            {
                MOVI2R(W0, (s32)CurInstr.CodeCycles);
                STR(INDEX_UNSIGNED, W0, RCPU, offsetof(ARM, CodeCycles));
            }
        }

        if (comp == NULL)
        {
            SaveCycles();
            SaveCPSR();
            RegCache.Flush();
        }
        else
            RegCache.Prepare(Thumb, i);

        if (Thumb)
        {
            if (comp == NULL)
            {
                MOV(X0, RCPU);
                QuickCallFunction(X1, InterpretTHUMB[CurInstr.Info.Kind]);
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
                    MOV(X0, RCPU);
                    QuickCallFunction(X1, ARMInterpreter::A_BLX_IMM);
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
                    MOV(X0, RCPU);
                    QuickCallFunction(X1, InterpretARM[CurInstr.Info.Kind]);
                }
                else
                {
                    (this->*comp)();
                }

                Comp_BranchSpecialBehaviour(true);

                if (cond < 0xE)
                {
                    if (IrregularCycles || (CurInstr.BranchFlags & branch_FollowCondTaken))
                    {
                        FixupBranch skipNop = B();
                        SetJumpTarget(skipExecute);

                        if (IrregularCycles)
                            Comp_AddCycles_C(true);

                        Comp_BranchSpecialBehaviour(false);

                        SetJumpTarget(skipNop);
                    }
                    else
                    {
                        SetJumpTarget(skipExecute);
                    }
                }

            }
        }

        if (comp == NULL)
        {
            LoadCycles();
            LoadCPSR();
        }
    }

    RegCache.Flush();

    if (ConstantCycles)
        ADD(RCycles, RCycles, ConstantCycles);
    QuickTailCall(X0, ARM_Ret);

    FlushIcache();

    return res;
}

void Compiler::Reset()
{
    LoadStorePatches.clear();

    SetCodePtr(0);
    OtherCodeRegion = JitMemMainSize;

    const u32 brk_0 = 0xD4200000;

    for (int i = 0; i < (JitMemMainSize + JitMemSecondarySize) / 4; i++)
        *(((u32*)GetRWPtr()) + i) = brk_0;
}

void Compiler::Comp_AddCycles_C(bool forceNonConstant)
{
    s32 cycles = Num ?
        NDS.ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 1 : 3]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles);

    if (forceNonConstant)
        ConstantCycles += cycles;
    else
        ADD(RCycles, RCycles, cycles);
}

void Compiler::Comp_AddCycles_CI(u32 numI)
{
    IrregularCycles = true;

    s32 cycles = (Num ?
        NDS.ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles)) + numI;

    if (Thumb || CurInstr.Cond() == 0xE)
        ConstantCycles += cycles;
    else
        ADD(RCycles, RCycles, cycles);
}

void Compiler::Comp_AddCycles_CI(u32 c, ARM64Reg numI, ArithOption shift)
{
    IrregularCycles = true;

    s32 cycles = (Num ?
        NDS.ARM7MemTimings[CurInstr.CodeCycles][Thumb ? 0 : 2]
        : ((R15 & 0x2) ? 0 : CurInstr.CodeCycles)) + c;

    ADD(RCycles, RCycles, cycles);
    if (Thumb || CurInstr.Cond() >= 0xE)
        ConstantCycles += cycles;
    else
        ADD(RCycles, RCycles, cycles);
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
            ADD(RCycles, RCycles, cycles);
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

        if ((CurInstr.DataRegion >> 24) == 0x02)
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

    if ((!Thumb && CurInstr.Cond() < 0xE) && IrregularCycles)
        ADD(RCycles, RCycles, cycles);
    else
        ConstantCycles += cycles;
}

}
