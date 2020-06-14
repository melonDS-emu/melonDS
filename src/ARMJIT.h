#ifndef ARMJIT_H
#define ARMJIT_H

#include "types.h"

#include "ARM.h"
#include "ARM_InstrInfo.h"

namespace ARMJIT
{

typedef void (*JitBlockEntry)();

void Init();
void DeInit();

void Reset();

void InvalidateByAddr(u32 pseudoPhysical);

template <u32 num, int region>
void CheckAndInvalidate(u32 addr);

void CompileBlock(ARM* cpu);

void ResetBlockCache();

JitBlockEntry LookUpBlock(u32 num, u64* entries, u32 offset, u32 addr);
bool SetupExecutableRegion(u32 num, u32 blockAddr, u64*& entry, u32& start, u32& size);

}

extern "C" void ARM_Dispatch(ARM* cpu, ARMJIT::JitBlockEntry entry);

#endif