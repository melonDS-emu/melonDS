/*
    Copyright 2016-2026 melonDS team

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

#ifndef ARM_H
#define ARM_H

#include <algorithm>
#include <optional>

#include "types.h"
#include "MemRegion.h"
#include "MemConstants.h"
#include "CP15_Constants.h"

#ifdef GDBSTUB_ENABLED
#include "debug/GdbStub.h"
#endif

namespace melonDS
{
inline u32 ROR(u32 x, u32 n)
{
    return (x >> (n&0x1F)) | (x << ((32-n)&0x1F));
}

enum
{
    RWFlags_Nonseq = (1<<5),
    RWFlags_ForceUser = (1<<21),
};

enum class CPUExecuteMode : u32
{
    Interpreter,
    InterpreterGDB,
#ifdef JIT_ENABLED
    JIT
#endif
};

struct GDBArgs;
class ARMJIT;
class GPU;
class ARMJIT_Memory;
class NDS;
class Savestate;

class ARMv4;
class ARMv5;

class ARM
#ifdef GDBSTUB_ENABLED
    : public Gdb::StubCallbacks
#endif
{
public:
    ARM(u32 num, bool jit, std::optional<GDBArgs> gdb, NDS& nds);
    virtual ~ARM(); // destroy shit

    void SetGdbArgs(std::optional<GDBArgs> gdb);

    virtual void Reset();

    virtual void DoSavestate(Savestate* file);

    virtual void FillPipeline() = 0;

    virtual void JumpTo(u32 addr, bool restorecpsr = false) = 0;
    void RestoreCPSR();

    void Halt(u32 halt)
    {
        if (halt==2 && Halted==1) return;
        Halted = halt;
    }

    void NocashPrint(u32 addr) noexcept;

    bool CheckCondition(u32 code) const
    {
        if (code == 0xE) return true;
        if (ConditionTable[code] & (1 << (CPSR>>28))) return true;
        return false;
    }

    void SetC(bool c)
    {
        if (c) CPSR |= 0x20000000;
        else CPSR &= ~0x20000000;
    }

    void SetNZ(bool n, bool z)
    {
        CPSR &= ~0xC0000000;
        if (n) CPSR |= 0x80000000;
        if (z) CPSR |= 0x40000000;
    }

    void SetNZCV(bool n, bool z, bool c, bool v)
    {
        CPSR &= ~0xF0000000;
        if (n) CPSR |= 0x80000000;
        if (z) CPSR |= 0x40000000;
        if (c) CPSR |= 0x20000000;
        if (v) CPSR |= 0x10000000;
    }

    inline bool ModeIs(u32 mode) const
    {
        u32 cm = CPSR & 0x1f;
        mode &= 0x1f;

        if (mode == cm) return true;
        if (mode == 0x17) return cm >= 0x14 && cm <= 0x17; // abt
        if (mode == 0x1b) return cm >= 0x18 && cm <= 0x1b; // und

        return false;
    }

    void UpdateMode(u32 oldmode, u32 newmode, bool phony = false);

    void TriggerIRQ();

    void SetupCodeMem(u32 addr);


    virtual void DataRead8(const u32 addr, u32* val) = 0;
    virtual void DataRead16(const u32 addr, u32* val) = 0;
    virtual void DataRead32(const u32 addr, u32* val) = 0;
    virtual void DataRead32S(const u32 addr, u32* val) = 0;
    virtual void DataWrite8(const u32 addr, const u8 val) = 0;
    virtual void DataWrite16(const u32 addr, const u16 val) = 0;
    virtual void DataWrite32(const u32 addr, const u32 val) = 0;
    virtual void DataWrite32S(const u32 addr, const u32 val) = 0;

    virtual void AddCycles_C() = 0;
    virtual void AddCycles_CI(s32 numI) = 0;
    virtual void AddCycles_CDI() = 0;
    virtual void AddCycles_CD() = 0;

    void CheckGdbIncoming();

    u32 Num;

    s32 Cycles;
    union
    {
        struct
        {
            u8 Halted;
            u8 IRQ; // nonzero to trigger IRQ
            u8 IdleLoop;
        };
        u32 StopExecution;
    };

    u32 CodeRegion;
    s32 CodeCycles;

    u32 DataRegion;
    s32 DataCycles;

    u32 R[16]; // heh
    u32 CPSR;
    u32 R_FIQ[8]; // holding SPSR too
    u32 R_SVC[3];
    u32 R_ABT[3];
    u32 R_IRQ[3];
    u32 R_UND[3];
    u32 CurInstr;
    u32 NextInstr[2];

    u32 ExceptionBase;

    MemRegion CodeMem;

#ifdef JIT_ENABLED
    u32 FastBlockLookupStart, FastBlockLookupSize;
    u64* FastBlockLookup;
#endif

    static const u32 ConditionTable[16];
#ifdef GDBSTUB_ENABLED
    Gdb::GdbStub GdbStub;
#endif

    melonDS::NDS& NDS;

protected:
    virtual void Prefetch(bool branch) = 0;

#ifdef GDBSTUB_ENABLED
    bool IsSingleStep;
    bool BreakReq;
    bool BreakOnStartup;
    u16 Port;

public:
    int GetCPU() const override { return Num ? 7 : 9; }

    u32 ReadReg(Gdb::Register reg) override;
    void WriteReg(Gdb::Register reg, u32 v) override;
    u32 ReadMem(u32 addr, int size) override;
    void WriteMem(u32 addr, int size, u32 v) override;

    void ResetGdb() override;
    int RemoteCmd(const u8* cmd, size_t len) override;

protected:
#endif

    void GdbCheckA();
    void GdbCheckB();
    void GdbCheckC();
};

namespace ARMInterpreter
{

void A_UNK(ARM* cpu);
void T_UNK(ARM* cpu);

}

}
#endif // ARM_H
