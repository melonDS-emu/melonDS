/*
    Copyright 2016-2019 Arisotura

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

    virtual void JumpTo(u32 addr, bool restorecpsr = false) = 0;
    void RestoreCPSR();

    void Halt(u32 halt)
    {
        if (halt==2 && Halted==1) return;
        Halted = halt;
    }

    virtual void Execute() = 0;
#ifdef ENABLE_JIT
    virtual void ExecuteJIT() = 0;
#endif

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


    virtual void DataRead8(u32 addr, u32* val) = 0;
    virtual void DataRead16(u32 addr, u32* val) = 0;
    virtual void DataRead32(u32 addr, u32* val) = 0;
    virtual void DataRead32S(u32 addr, u32* val) = 0;
    virtual void DataWrite8(u32 addr, u8 val) = 0;
    virtual void DataWrite16(u32 addr, u16 val) = 0;
    virtual void DataWrite32(u32 addr, u32 val) = 0;
    virtual void DataWrite32S(u32 addr, u32 val) = 0;

    virtual void AddCycles_C() = 0;
    virtual void AddCycles_CI(s32 numI) = 0;
    virtual void AddCycles_CDI() = 0;
    virtual void AddCycles_CD() = 0;


    u32 Num;

    s32 Cycles;
    u32 Halted;

    u32 IRQ; // nonzero to trigger IRQ

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

    NDS::MemRegion CodeMem;

    static u32 ConditionTable[16];
};

class ARMv5 : public ARM
{
public:
    ARMv5();

    void Reset();

    void DoSavestate(Savestate* file);

    void UpdateRegionTimings(u32 addrstart, u32 addrend);

    void JumpTo(u32 addr, bool restorecpsr = false);

    void PrefetchAbort();
    void DataAbort();

    void Execute();
#ifdef JIT_ENABLED
    void ExecuteJIT();
#endif

    // all code accesses are forced nonseq 32bit
    u32 CodeRead32(u32 addr, bool branch);

    void DataRead8(u32 addr, u32* val);
    void DataRead16(u32 addr, u32* val);
    void DataRead32(u32 addr, u32* val);
    void DataRead32S(u32 addr, u32* val);
    void DataWrite8(u32 addr, u8 val);
    void DataWrite16(u32 addr, u16 val);
    void DataWrite32(u32 addr, u32 val);
    void DataWrite32S(u32 addr, u32 val);

    void AddCycles_C()
    {
        // code only. always nonseq 32-bit for ARM9.
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        Cycles += numC;
    }

    void AddCycles_CI(s32 numI)
    {
        // code+internal
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        Cycles += numC + numI;
    }

    void AddCycles_CDI()
    {
        // LDR/LDM cycles. ARM9 seems to skip the internal cycle there.
        // TODO: ITCM data fetches shouldn't be parallelized, they say
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        s32 numD = DataCycles;

        //if (DataRegion != CodeRegion)
            Cycles += std::max(numC + numD - 6, std::max(numC, numD));
        //else
        //    Cycles += numC + numD;
    }

    void AddCycles_CD()
    {
        // TODO: ITCM data fetches shouldn't be parallelized, they say
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        s32 numD = DataCycles;

        //if (DataRegion != CodeRegion)
            Cycles += std::max(numC + numD - 6, std::max(numC, numD));
        //else
        //    Cycles += numC + numD;
    }

    void GetCodeMemRegion(u32 addr, NDS::MemRegion* region);

    void CP15Reset();
    void CP15DoSavestate(Savestate* file);

    void UpdateDTCMSetting();
    void UpdateITCMSetting();

    void UpdatePURegions();

    u32 RandomLineIndex();

    void ICacheLookup(u32 addr);
    void ICacheInvalidateByAddr(u32 addr);
    void ICacheInvalidateAll();

    void CP15Write(u32 id, u32 val);
    u32 CP15Read(u32 id);

    u32 CP15Control;

    u32 RNGSeed;

    u32 DTCMSetting, ITCMSetting;

    u8 ITCM[0x8000];
    u32 ITCMSize;
    u8 DTCM[0x4000];
    u32 DTCMBase, DTCMSize;

    u8 ICache[0x2000];
    u32 ICacheTags[64*4];
    u8 ICacheCount[64];

    u32 PU_CodeCacheable;
    u32 PU_DataCacheable;
    u32 PU_DataCacheWrite;

    u32 PU_CodeRW;
    u32 PU_DataRW;

    u32 PU_Region[8];

    // 0=dataR 1=dataW 2=codeR 4=datacache 5=datawrite 6=codecache
    u8 PU_PrivMap[0x100000];
    u8 PU_UserMap[0x100000];

    // games operate under system mode, generally
    #define PU_Map PU_PrivMap

    // code/16N/32N/32S
    u8 MemTimings[0x100000][4];

    s32 RegionCodeCycles;
    u8* CurICacheLine;
};

class ARMv4 : public ARM
{
public:
    ARMv4();

    void JumpTo(u32 addr, bool restorecpsr = false);

    void Execute();
#ifdef JIT_ENABLED
    void ExecuteJIT();
#endif

    u16 CodeRead16(u32 addr)
    {
        return NDS::ARM7Read16(addr);
    }

    u32 CodeRead32(u32 addr)
    {
        return NDS::ARM7Read32(addr);
    }

    void DataRead8(u32 addr, u32* val)
    {
        *val = NDS::ARM7Read8(addr);
        DataRegion = addr >> 24;
        DataCycles = NDS::ARM7MemTimings[DataRegion][0];
    }

    void DataRead16(u32 addr, u32* val)
    {
        addr &= ~1;

        *val = NDS::ARM7Read16(addr);
        DataRegion = addr >> 24;
        DataCycles = NDS::ARM7MemTimings[DataRegion][0];
    }

    void DataRead32(u32 addr, u32* val)
    {
        addr &= ~3;

        *val = NDS::ARM7Read32(addr);
        DataRegion = addr >> 24;
        DataCycles = NDS::ARM7MemTimings[DataRegion][2];
    }

    void DataRead32S(u32 addr, u32* val)
    {
        addr &= ~3;

        *val = NDS::ARM7Read32(addr);
        DataCycles += NDS::ARM7MemTimings[DataRegion][3];
    }

    void DataWrite8(u32 addr, u8 val)
    {
        NDS::ARM7Write8(addr, val);
        DataRegion = addr >> 24;
        DataCycles = NDS::ARM7MemTimings[DataRegion][0];
    }

    void DataWrite16(u32 addr, u16 val)
    {
        addr &= ~1;

        NDS::ARM7Write16(addr, val);
        DataRegion = addr >> 24;
        DataCycles = NDS::ARM7MemTimings[DataRegion][0];
    }

    void DataWrite32(u32 addr, u32 val)
    {
        addr &= ~3;

        NDS::ARM7Write32(addr, val);
        DataRegion = addr >> 24;
        DataCycles = NDS::ARM7MemTimings[DataRegion][2];
    }

    void DataWrite32S(u32 addr, u32 val)
    {
        addr &= ~3;

        NDS::ARM7Write32(addr, val);
        DataCycles += NDS::ARM7MemTimings[DataRegion][3];
    }


    void AddCycles_C()
    {
        // code only. this code fetch is sequential.
        Cycles += NDS::ARM7MemTimings[CodeCycles][(CPSR&0x20)?1:3];
    }

    void AddCycles_CI(s32 num)
    {
        // code+internal. results in a nonseq code fetch.
        Cycles += NDS::ARM7MemTimings[CodeCycles][(CPSR&0x20)?0:2] + num;
    }

    void AddCycles_CDI()
    {
        // LDR/LDM cycles.
        s32 numC = NDS::ARM7MemTimings[CodeCycles][(CPSR&0x20)?0:2];
        s32 numD = DataCycles;

        if (DataRegion == 0x02) // mainRAM
        {
            if (CodeRegion == 0x02)
                Cycles += numC + numD;
            else
            {
                numC++;
                Cycles += std::max(numC + numD - 3, std::max(numC, numD));
            }
        }
        else if (CodeRegion == 0x02)
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
        s32 numC = NDS::ARM7MemTimings[CodeCycles][(CPSR&0x20)?0:2];
        s32 numD = DataCycles;

        if (DataRegion == 0x02)
        {
            if (CodeRegion == 0x02)
                Cycles += numC + numD;
            else
                Cycles += std::max(numC + numD - 3, std::max(numC, numD));
        }
        else if (CodeRegion == 0x02)
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
