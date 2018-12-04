/*
    Copyright 2016-2019 StapleButter

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

#include "types.h"
#include "NDS.h"

#define ROR(x, n) (((x) >> (n)) | ((x) << (32-(n))))

enum
{
    RWFlags_Nonseq = (1<<5),
    RWFlags_ForceUser = (1<<21),
};

class ARM
{
public:
    ARM(u32 num);
    ~ARM(); // destroy shit

    virtual void Reset();

    virtual void DoSavestate(Savestate* file);

    void SetClockShift(u32 shift)
    {
        ClockShift = shift;
        ClockDiffMask = (1<<shift) - 1;
    }

    virtual void CalculateTimings() = 0;

    virtual void JumpTo(u32 addr, bool restorecpsr = false) = 0;
    void RestoreCPSR();

    void Halt(u32 halt)
    {
        if (halt==2 && Halted==1) return;
        Halted = halt;
    }

    // TODO: is this actually used??
    void CheckIRQ()
    {
        if (!(NDS::IME[Num] & 0x1)) return;
        if (NDS::IF[Num] & NDS::IE[Num])
        {
            TriggerIRQ();
        }
    }

    virtual s32 Execute() = 0;

    bool CheckCondition(u32 code)
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

    void UpdateMode(u32 oldmode, u32 newmode);

    void TriggerIRQ();

    void SetupCodeMem(u32 addr);


    virtual bool DataRead8(u32 addr, u32* val, u32 flags) = 0;
    virtual bool DataRead16(u32 addr, u32* val, u32 flags) = 0;
    virtual bool DataRead32(u32 addr, u32* val, u32 flags) = 0;
    virtual bool DataWrite8(u32 addr, u8 val, u32 flags) = 0;
    virtual bool DataWrite16(u32 addr, u16 val, u32 flags) = 0;
    virtual bool DataWrite32(u32 addr, u32 val, u32 flags) = 0;

    virtual void AddCycles_C() = 0;
    virtual void AddCycles_CI(s32 num) = 0;
    virtual void AddCycles_CDI() = 0;
    virtual void AddCycles_CD() = 0;


    u32 Num;

    // shift relative to system clock
    // 0=33MHz 1=66MHz 2=133MHz
    u32 ClockShift;
    u32 ClockDiffMask;

    s32 Cycles;
    s32 CyclesToRun;
    u32 Halted;

    int CodeRegion;

    int DataRegion;
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

    NDS::MemRegion CodeMem;

    static u32 ConditionTable[16];
};

class ARMv5 : public ARM
{
public:
    ARMv5();

    void Reset();

    void DoSavestate(Savestate* file);

    void CalculateTimings();

    void JumpTo(u32 addr, bool restorecpsr = false);

    s32 Execute();

    // all code accesses are forced nonseq 32bit
    u32 CodeRead32(u32 addr);

    bool DataRead8(u32 addr, u32* val, u32 flags);
    bool DataRead16(u32 addr, u32* val, u32 flags);
    bool DataRead32(u32 addr, u32* val, u32 flags);
    bool DataWrite8(u32 addr, u8 val, u32 flags);
    bool DataWrite16(u32 addr, u16 val, u32 flags);
    bool DataWrite32(u32 addr, u32 val, u32 flags);

    void AddCycles_C()
    {
        // code only. always nonseq 32-bit for ARM9.
        Cycles += NDS::ARM9MemTimings[CodeRegion][2];
    }

    void AddCycles_CI(s32 num)
    {
        // code+internal
        Cycles += NDS::ARM9MemTimings[CodeRegion][2] + num;
    }

    void AddCycles_CDI()
    {
        // LDR/LDM cycles. ARM9 seems to skip the internal cycle there.
        // TODO: ITCM data fetches shouldn't be parallelized, they say
        s32 numC = NDS::ARM9MemTimings[CodeRegion][2];
        s32 numD = DataCycles;

        if (DataRegion != CodeRegion)
            Cycles += std::max(numC + numD - 6, std::max(numC, numD));
        else
            Cycles += numC + numD;
    }

    void AddCycles_CD()
    {
        // TODO: ITCM data fetches shouldn't be parallelized, they say
        s32 numC = NDS::ARM9MemTimings[CodeRegion][2];
        s32 numD = DataCycles;

        if (DataRegion != CodeRegion)
            Cycles += std::max(numC + numD - 6, std::max(numC, numD));
        else
            Cycles += numC + numD;
    }

    void GetCodeMemRegion(u32 addr, NDS::MemRegion* region);

    void CP15Reset();
    void CP15DoSavestate(Savestate* file);

    void UpdateDTCMSetting();
    void UpdateITCMSetting();

    void CP15Write(u32 id, u32 val);
    u32 CP15Read(u32 id);

    u32 CP15Control;

    u32 DTCMSetting, ITCMSetting;

    u8 ITCM[0x8000];
    u32 ITCMSize;
    u8 DTCM[0x4000];
    u32 DTCMBase, DTCMSize;

    u32 PU_CodeCacheable;
    u32 PU_DataCacheable;
    u32 PU_DataCacheWrite;

    u32 PU_CodeRW;
    u32 PU_DataRW;

    u32 PU_Region[8];
};

class ARMv4 : public ARM
{
public:
    ARMv4();

    void CalculateTimings();

    void JumpTo(u32 addr, bool restorecpsr = false);

    s32 Execute();

    u16 CodeRead16(u32 addr)
    {
        u32 ret;
        CodeRegion = NDS::ARM7Read16(addr, &ret);
        return ret;
    }

    u32 CodeRead32(u32 addr)
    {
        u32 ret;
        CodeRegion = NDS::ARM7Read32(addr, &ret);
        return ret;
    }

    bool DataRead8(u32 addr, u32* val, u32 flags)
    {
        DataRegion = NDS::ARM7Read8(addr, val);
        if (flags & RWFlags_Nonseq)
            DataCycles = NDS::ARM7MemTimings[DataRegion][0];
        else
            DataCycles += NDS::ARM7MemTimings[DataRegion][1];

        return true;
    }

    bool DataRead16(u32 addr, u32* val, u32 flags)
    {
        addr &= ~1;

        DataRegion = NDS::ARM7Read16(addr, val);
        if (flags & RWFlags_Nonseq)
            DataCycles = NDS::ARM7MemTimings[DataRegion][0];
        else
            DataCycles += NDS::ARM7MemTimings[DataRegion][1];

        return true;
    }

    bool DataRead32(u32 addr, u32* val, u32 flags)
    {
        addr &= ~3;

        DataRegion = NDS::ARM7Read32(addr, val);
        if (flags & RWFlags_Nonseq)
            DataCycles = NDS::ARM7MemTimings[DataRegion][2];
        else
            DataCycles += NDS::ARM7MemTimings[DataRegion][3];

        return true;
    }

    bool DataWrite8(u32 addr, u8 val, u32 flags)
    {
        DataRegion = NDS::ARM7Write8(addr, val);
        if (flags & RWFlags_Nonseq)
            DataCycles = NDS::ARM7MemTimings[DataRegion][0];
        else
            DataCycles += NDS::ARM7MemTimings[DataRegion][1];

        return true;
    }

    bool DataWrite16(u32 addr, u16 val, u32 flags)
    {
        addr &= ~1;

        DataRegion = NDS::ARM7Write16(addr, val);
        if (flags & RWFlags_Nonseq)
            DataCycles = NDS::ARM7MemTimings[DataRegion][0];
        else
            DataCycles += NDS::ARM7MemTimings[DataRegion][1];

        return true;
    }

    bool DataWrite32(u32 addr, u32 val, u32 flags)
    {
        addr &= ~3;

        DataRegion = NDS::ARM7Write32(addr, val);
        if (flags & RWFlags_Nonseq)
            DataCycles = NDS::ARM7MemTimings[DataRegion][2];
        else
            DataCycles += NDS::ARM7MemTimings[DataRegion][3];

        return true;
    }


    void AddCycles_C()
    {
        // code only. this code fetch is sequential.
        Cycles += NDS::ARM7MemTimings[CodeRegion][(CPSR&0x20)?1:3];
    }

    void AddCycles_CI(s32 num)
    {
        // code+internal. results in a nonseq code fetch.
        Cycles += NDS::ARM7MemTimings[CodeRegion][(CPSR&0x20)?0:2] + num;
    }

    void AddCycles_CDI()
    {
        // LDR/LDM cycles.
        s32 numC = NDS::ARM7MemTimings[CodeRegion][(CPSR&0x20)?0:2];
        s32 numD = DataCycles;

        if (DataRegion == NDS::Region7_MainRAM)
        {
            if (CodeRegion == NDS::Region7_MainRAM)
                Cycles += numC + numD;
            else
            {
                numC++;
                Cycles += std::max(numC + numD - 3, std::max(numC, numD));
            }
        }
        else if (CodeRegion == NDS::Region7_MainRAM)
        {
            numD++;
            Cycles += std::max(numC + numD - 3, std::max(numC, numD));
        }
        else
        {
            Cycles += numC + numD + 1;
        }
    }

    void AddCycles_CD()
    {
        // TODO: max gain should be 5c when writing to mainRAM
        s32 numC = NDS::ARM7MemTimings[CodeRegion][(CPSR&0x20)?0:2];
        s32 numD = DataCycles;

        if (DataRegion == NDS::Region7_MainRAM)
        {
            if (CodeRegion == NDS::Region7_MainRAM)
                Cycles += numC + numD;
            else
                Cycles += std::max(numC + numD - 3, std::max(numC, numD));
        }
        else if (CodeRegion == NDS::Region7_MainRAM)
        {
            Cycles += std::max(numC + numD - 3, std::max(numC, numD));
        }
        else
        {
            Cycles += numC + numD;
        }
    }
};

namespace ARMInterpreter
{

void A_UNK(ARM* cpu);
void T_UNK(ARM* cpu);

}

#endif // ARM_H
