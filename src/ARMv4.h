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

#ifndef ARMV4_H
#define ARMV4_H

#include "ARM.h"

namespace melonDS
{

class ARMv4 : public ARM
{
public:
    ARMv4(melonDS::NDS& nds, std::optional<GDBArgs> gdb, bool jit);
    void Reset() override;

    void FillPipeline() override;

    void JumpTo(u32 addr, bool restorecpsr = false) override;

    template <CPUExecuteMode mode>
    void Execute();

    u16 CodeRead16(const u32 addr);
    u32 CodeRead32(const u32 addr);

    void DataRead8(const u32 addr, u32* val) override;
    void DataRead16(const u32 addr, u32* val) override;
    void DataRead32(const u32 addr, u32* val) override;
    void DataRead32S(const u32 addr, u32* val) override;
    void DataWrite8(const u32 addr, const u8 val) override;
    void DataWrite16(const u32 addr, const u16 val) override;
    void DataWrite32(const u32 addr, const u32 val) override;
    void DataWrite32S(const u32 addr, const u32 val) override;
    void AddCycles_C() override;
    void AddCycles_CI(s32 num) override;
    void AddCycles_CDI() override;
    void AddCycles_CD() override;

protected:
    //void Prefetch(bool branch) override;

private:
    u32 CodeRegion;

    // TODO change to just pointer?
    //MemRegion CodeMem;
    MemInfo CodeMem;
    //u8 CodeMemTimings[2];
    bool NextCodeFetchSeq;

    // mainRAM burst tracking
    u32 MainRAMStartAddr;
    u64 MainRAMTerminate;

    void BeginMainRAMBurst(int begin, int term);
    void TerminateMainRAMBurst();

    void DoDataAccessTimings(const u32 addr, bool write, int width);
};

}
#endif // ARMV4_H
