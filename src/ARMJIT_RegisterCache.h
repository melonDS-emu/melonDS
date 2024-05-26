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

#ifndef ARMJIT_REGCACHE_H
#define ARMJIT_REGCACHE_H

#include "ARMJIT_Internal.h"
#include "Platform.h"

// TODO: replace this in the future
#include "dolphin/BitSet.h"

#include <assert.h>

namespace melonDS
{
    using Platform::Log;
    using Platform::LogLevel;
    using namespace Common;
    // Imported inside the namespace so that other headers aren't polluted

template <typename T, typename Reg>
class RegisterCache
{
public:
    RegisterCache()
    {}

    RegisterCache(T* compiler, FetchedInstr instrs[], int instrsCount, bool pcAllocatableAsSrc = false)
        : Compiler(compiler), Instrs(instrs), InstrsCount(instrsCount)
    {
        for (int i = 0; i < 16; i++)
            Mapping[i] = (Reg)-1;

        PCAllocatableAsSrc = ~(pcAllocatableAsSrc
            ? 0
            : (1 << 15));
    }

    void UnloadRegister(int reg)
    {
        assert(Mapping[reg] != -1);

        if (DirtyRegs & (1 << reg))
            Compiler->SaveReg(reg, Mapping[reg]);

        DirtyRegs &= ~(1 << reg);
        LoadedRegs &= ~(1 << reg);
        NativeRegsUsed &= ~(1 << (int)Mapping[reg]);
        Mapping[reg] = (Reg)-1;
    }

    void LoadRegister(int reg, bool loadValue)
    {
        assert(Mapping[reg] == -1);
        for (int i = 0; i < NativeRegsAvailable; i++)
        {
            Reg nativeReg = NativeRegAllocOrder[i];
            if (!(NativeRegsUsed & (1 << nativeReg)))
            {
                Mapping[reg] = nativeReg;
                NativeRegsUsed |= 1 << (int)nativeReg;
                LoadedRegs |= 1 << reg;

                if (loadValue)
                    Compiler->LoadReg(reg, nativeReg);

                return;
            }
        }

        Log(LogLevel::Error, "this is a JIT bug! LoadRegister failed\n");
        abort();
    }

    void PutLiteral(int reg, u32 val)
    {
        LiteralsLoaded |= (1 << reg);
        LiteralValues[reg] = val;
    }

    void UnloadLiteral(int reg)
    {
        LiteralsLoaded &= ~(1 << reg);
    }

    bool IsLiteral(int reg) const
    {
        return LiteralsLoaded & (1 << reg);
    }

    void PrepareExit()
    {
        BitSet16 dirtyRegs(DirtyRegs);
        for (int reg : dirtyRegs)
            Compiler->SaveReg(reg, Mapping[reg]);
    }

    void Flush()
    {
        BitSet16 loadedSet(LoadedRegs);
        for (int reg : loadedSet)
            UnloadRegister(reg);
        LiteralsLoaded = 0;
    }

    void Prepare(bool thumb, int i)
    {
        FetchedInstr instr = Instrs[i];

        if (LoadedRegs & (1 << 15))
            UnloadRegister(15);

        BitSet16 invalidedLiterals(LiteralsLoaded & instr.Info.DstRegs);
        for (int reg : invalidedLiterals)
            UnloadLiteral(reg);

        u16 futureNeeded = 0;
        int ranking[16];
        for (int j = 0; j < 16; j++)
            ranking[j] = 0;
        for (int j = i; j < InstrsCount; j++)
        {
            BitSet16 regsNeeded((Instrs[j].Info.SrcRegs & ~(1 << 15)) | Instrs[j].Info.DstRegs);
            futureNeeded |= regsNeeded.m_val;
            regsNeeded &= BitSet16(~Instrs[j].Info.NotStrictlyNeeded);
            for (int reg : regsNeeded)
                ranking[reg]++;
        }

        // we'll unload all registers which are never used again
        BitSet16 neverNeededAgain(LoadedRegs & ~futureNeeded);
        for (int reg : neverNeededAgain)
            UnloadRegister(reg);

        u16 necessaryRegs = ((instr.Info.SrcRegs & PCAllocatableAsSrc) | instr.Info.DstRegs) & ~instr.Info.NotStrictlyNeeded;
        BitSet16 needToBeLoaded(necessaryRegs & ~LoadedRegs);
        if (needToBeLoaded != BitSet16(0))
        {
            int neededCount = needToBeLoaded.Count();
            BitSet16 loadedSet(LoadedRegs);
            while (loadedSet.Count() + neededCount > NativeRegsAvailable)
            {
                int leastReg = -1;
                int rank = 1000;
                for (int reg : loadedSet)
                {
                    if (!((1 << reg) & necessaryRegs) && ranking[reg] < rank)
                    {
                        leastReg = reg;
                        rank = ranking[reg];
                    }
                }

                assert(leastReg != -1);
                UnloadRegister(leastReg);

                loadedSet.m_val = LoadedRegs;
            }

            // we don't need to load a value which is always going to be overwritten
            BitSet16 needValueLoaded(needToBeLoaded);
            if (thumb || instr.Cond() >= 0xE)
                needValueLoaded = BitSet16(instr.Info.SrcRegs);
            for (int reg : needToBeLoaded)
                LoadRegister(reg, needValueLoaded[reg]);
        }
        {
            BitSet16 loadedSet(LoadedRegs);
            BitSet16 loadRegs(instr.Info.NotStrictlyNeeded & futureNeeded & ~LoadedRegs);
            if (loadRegs && loadedSet.Count() < NativeRegsAvailable)
            {
                int left = NativeRegsAvailable - loadedSet.Count();
                for (int reg : loadRegs)
                {
                    if (left-- == 0)
                        break;

                    LoadRegister(reg, !(thumb || instr.Cond() >= 0xE) || (1 << reg) & instr.Info.SrcRegs);
                }
            }
        }

        DirtyRegs |= (LoadedRegs & instr.Info.DstRegs) & ~(1 << 15);
    }

    static const Reg NativeRegAllocOrder[];
    static const int NativeRegsAvailable;

    Reg Mapping[16];
    u32 LiteralValues[16];

    u16 LiteralsLoaded = 0;
    u32 NativeRegsUsed = 0;
    u16 LoadedRegs = 0;
    u16 DirtyRegs = 0;

    u16 PCAllocatableAsSrc = 0;

    T* Compiler;

    FetchedInstr* Instrs;
    int InstrsCount;
};

}

#endif
