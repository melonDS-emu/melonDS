/*
    Copyright 2016-2022 melonDS team

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

#ifndef ARMJIT_H
#define ARMJIT_H

#include "types.h"

#include "ARM.h"
#include "ARM_InstrInfo.h"

#if defined(__APPLE__) && defined(__aarch64__)
    #include <pthread.h>
#endif

namespace ARMJIT
{

typedef void (*JitBlockEntry)();

extern int MaxBlockSize;
extern bool LiteralOptimizations;
extern bool BranchOptimizations;
extern bool FastMemory;

void Init();
void DeInit();

void Reset();

void CheckAndInvalidateITCM();
void CheckAndInvalidateWVRAM(int bank);

void InvalidateByAddr(u32 pseudoPhysical);

template <u32 num, int region>
void CheckAndInvalidate(u32 addr);

void CompileBlock(ARM* cpu);

void ResetBlockCache();

JitBlockEntry LookUpBlock(u32 num, u64* entries, u32 offset, u32 addr);
bool SetupExecutableRegion(u32 num, u32 blockAddr, u64*& entry, u32& start, u32& size);

void JitEnableWrite();
void JitEnableExecute();
}

extern "C" void ARM_Dispatch(ARM* cpu, ARMJIT::JitBlockEntry entry);

#endif
