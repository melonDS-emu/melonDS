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

struct GDBArgs;
class ARMJIT;
class GPU;
class ARMJIT_Memory;
class NDS;
class Savestate;

class ARM
#ifdef GDBSTUB_ENABLED
    : public Gdb::StubCallbacks
#endif
{
public:
    ARM(u32 num, bool jit, std::optional<GDBArgs> gdb, NDS& nds);
    virtual ~ARM(); // destroy shit

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
    virtual void Execute() = 0;
#ifdef JIT_ENABLED
    virtual void ExecuteJIT() = 0;
#endif

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
    virtual u8 BusRead8(u32 addr) = 0;
    virtual u16 BusRead16(u32 addr) = 0;
    virtual u32 BusRead32(u32 addr) = 0;
    virtual void BusWrite8(u32 addr, u8 val) = 0;
    virtual void BusWrite16(u32 addr, u16 val) = 0;
    virtual void BusWrite32(u32 addr, u32 val) = 0;

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

class ARMv5 : public ARM
{
public:
    ARMv5(melonDS::NDS& nds, std::optional<GDBArgs> gdb, bool jit);
    ~ARMv5();

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void UpdateRegionTimings(u32 addrstart, u32 addrend);

    void FillPipeline() override;

    void JumpTo(u32 addr, bool restorecpsr = false) override;

    void PrefetchAbort();
    void DataAbort();

    void Execute() override;
#ifdef JIT_ENABLED
    void ExecuteJIT() override;
#endif

    // all code accesses are forced nonseq 32bit
    u32 CodeRead32(const u32 addr, const bool branch);

    void DataRead8(const u32 addr, u32* val) override;
    void DataRead16(const u32 addr, u32* val) override;
    void DataRead32(const u32 addr, u32* val) override;
    void DataRead32S(const u32 addr, u32* val) override;
    void DataWrite8(const u32 addr, const u8 val) override;
    void DataWrite16(const u32 addr, const u16 val) override;
    void DataWrite32(const u32 addr, const u32 val) override;
    void DataWrite32S(const u32 addr, const u32 val) override;

    void AddCycles_C() override
    {
        // code only. always nonseq 32-bit for ARM9.
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        Cycles += numC;
    }

    void AddCycles_CI(s32 numI) override
    {
        // code+internal
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        Cycles += numC + numI;
    }

    void AddCycles_CDI() override
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

    void AddCycles_CD() override
    {
        // TODO: ITCM data fetches shouldn't be parallelized, they say
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        s32 numD = DataCycles;

        //if (DataRegion != CodeRegion)
            Cycles += std::max(numC + numD - 6, std::max(numC, numD));
        //else
        //    Cycles += numC + numD;
    }

    void GetCodeMemRegion(u32 addr, MemRegion* region);

    void CP15Reset();
    void CP15DoSavestate(Savestate* file);

    void UpdateDTCMSetting();
    void UpdateITCMSetting();

    void UpdatePURegion(u32 n);
    void UpdatePURegions(bool update_all);

    u32 RandomLineIndex();

    /**
     * @brief Perform an instruction cache lookup handle
     * @details
     * A cache lookup is performed, if not disabled in 
     * @ref CP15BISTTestStateRegister, a hit will returned the 
     * cached data, otherwise it returns the result of an memory 
     * access instead.
     * If the cache lookup results in a cachemiss and linefill is
     * not disabled in @ref CP15BISTTestStateRegister, will fill
     * fetch all data to fill the entire cacheline directly
     * from the ITCM or bus
     * @param [in] addr Address of the memory to be retreived from
     * cache. The address is internally aligned to an word boundary
     * @return Value of the word at addr
     */
    u32 ICacheLookup(const u32 addr);

    /**
     * @brief Check if an address is within a instruction cachable 
     * region
     * @details
     * Checks the address by looking up the PU_map flags for 
     * the address and returns the status of the instruction
     * cache enable flag
     * 
     * @param [in] addr Address. May be unaligned.
     * @retval true If the address points to a region, that is
     * enabled for instruction fetches to be cached.
     */
    inline bool IsAddressICachable(const u32 addr) const;

    /**
     * @brief Invalidates all instruction cache lines
     * @details
     * Clears the @ref CACHE_FLAG_VALID of each cache line in the 
     * instruction cache. All other flags and values are kept.
     * @par Returns
     *      Nothing
     */
    void ICacheInvalidateAll();

    /** 
     * @brief Invalidates the instruction cacheline containing 
     * the data of an address.
     * @details 
     * Searches the cacheline containing the data of an address, and
     * if found clears the @ref CACHE_FLAG_VALID of this cache line.
     * Nothing is done if the address is not present in the cache.
     * @param [in] addr Memory address of the data in the cache line
     * @par Returns
     *      Nothing
     */
    void ICacheInvalidateByAddr(const u32 addr);

    /**
     * @brief Invalidates an instruction cache line
     * @details
     * Clears the @ref CACHE_FLAG_VALID of the cacheline given by 
     * set and index within the set. Nothing is done if the cache
     * line does not exist.
     * @param [in] cacheSet index of the internal cache set from
     * 0 to @ref ICACHE_SETS - 1
     * @param [in] cacheLine index of the line within the cache set
     * from 0 to @ref ICACHE_LINESPERSET - 1
     * @par Returns
     *      Nothing
     */
    void ICacheInvalidateBySetAndWay(const u8 cacheSet, const u8 cacheLine);

    /**
     * @brief Perform an data cache lookup handle
     * @details
     * A cache lookup is performed, if not disabled in 
     * @ref CP15BISTTestStateRegister, a hit will returned the 
     * cached data, otherwise it returns the result of an memory 
     * access instead.
     * If the cache lookup results in a cachemiss and linefill is
     * not disabled in @ref CP15BISTTestStateRegister, will fill
     * fetch all data to fill the entire cacheline directly
     * from the ITCM, DTCM or bus
     * @param [in] addr Address of the memory to be retreived from
     * cache. The address is internally aligned to an word boundary
     * @return Value of the word at addr
     */
    u32 DCacheLookup(const u32 addr);

    /**
     * @brief Updates a word in the data cache if present
     * @param [in] addr Memory address which is written
     * @param [in] val Word value to be written
     * @par Returns
     *      Nothing
     */
    void DCacheWrite32(const u32 addr, const u32 val);

    /**
     * @brief Updates a word in the data cache if present
     * @param [in] addr Memory address which is written
     * @param [in] val Half-Word value to be written
     * @par Returns
     *      Nothing
     */
    void DCacheWrite16(const u32 addr, const u16 val);

    /**
     * @brief Updates a word in the data cache if present
     * @param [in] addr Memory address which is written
     * @param [in] val Byte value to be written
     * @par Returns
     *      Nothing
     */
    void DCacheWrite8(const u32 addr, const u8 val);

    /**
     * @brief Check if an address is within a data cachable region
     * @details
     * Checks the address by looking up the PU_map flags for 
     * the address and returns the status of the data cache enable 
     * flag
     * 
     * @param [in] addr Address. May be unaligned.
     * @retval true If the address points to a region, that is
     * enabled for instruction fetches to be cached.
     */
    inline bool IsAddressDCachable(const u32 addr) const;

    /** 
     * @brief Invalidates the data cacheline containing the data of 
     * an address.
     * @details 
     * Searches the cacheline containing the data of an address, and
     * if found clears the @ref CACHE_FLAG_VALID of this cache line.
     * Nothing is done if the address is not present in the cache.
     * @par Returns
     *      Nothing
     */
    void DCacheInvalidateAll();
    
     /** 
     * @brief Invalidates the data cacheline containing the data of 
     * an address.
     * @details 
     * Searches the cacheline containing the data of an address, and
     * if found clears the @ref CACHE_FLAG_VALID of this cache line.
     * Nothing is done if the address is not present in the cache.
     * @par Returns
     *      Nothing
     */     
    void DCacheInvalidateByAddr(const u32 addr);
    
    /**
     * @brief Invalidates an data cache line
     * @details
     * Clears the @ref CACHE_FLAG_VALID of the cacheline given by 
     * set and index within the set. Nothing is done if the cache
     * line does not exist.
     * @param [in] cacheSet index of the internal cache set from
     * 0 to @ref DCACHE_SETS - 1
     * @param [in] cacheLine index of the line within the cache set
     * from 0 to @ref DCACHE_LINESPERSET - 1
     * @par Returns
     *      Nothing
     */
    void DCacheInvalidateBySetAndWay(const u8 cacheSet, const u8 cacheLine);
    
    /**
     * @brief Cleans the entire data cache
     * @details
     * In melonDS, the data cache is instantly cleaned on writes, the 
     * @ref CACHE_FLAG_DIRTY_LOWERHALF and @ref CACHE_FLAG_DIRTY_UPPERHALF are
     * not set. 
     * If they are implemented at a later time, the cache content has to be 
     * written to memory, the dirty bit cleared. The call should require
     * as much cycles as needed for this write operation. 
     * @par Returns
     *      Nothing
     */
    void DCacheClearAll();

    /**
     * @brief Cleans a data cache line
     * @details
     * In melonDS, the data cache is instantly cleaned on writes, the 
     * @ref CACHE_FLAG_DIRTY_LOWERHALF and @ref CACHE_FLAG_DIRTY_UPPERHALF are
     * not set. 
     * If they are implemented at a later time, the cache content has to be 
     * written to memory, the dirty bit cleared. The call should require
     * as much cycles as needed for this write operation. 
     * @param [in] addr Memory address of the data in the cache line
     * @par Returns
     *      Nothing
     */
    void DCacheClearByAddr(const u32 addr);

    /**
     * @brief Cleans a data cache line
     * @details
     * In melonDS, the data cache is instantly cleaned on writes, the 
     * @ref CACHE_FLAG_DIRTY_LOWERHALF and @ref CACHE_FLAG_DIRTY_UPPERHALF are
     * not set. 
     * If they are implemented at a later time, the cache content has to be 
     * written to memory, the dirty bit cleared. The call should require
     * as much cycles as needed for this write operation. 
     * @param [in] cacheSet index of the internal cache set from
     * 0 to @ref DCACHE_SETS - 1
     * @param [in] cacheLine index of the line within the cache set
     * from 0 to @ref DCACHE_LINESPERSET - 1
     * @par Returns
     *      Nothing
     */
    void DCacheClearByASetAndWay(const u8 cacheSet, const u8 cacheLine);

    void CP15Write(u32 id, u32 val);
    u32 CP15Read(u32 id) const;

    u32 CP15Control;                                //! CP15 Register 1: Control Register

    u32 RNGSeed;                                    //! Global cache line fill seed. Used for pseudo random replacement strategy with the instruction and data cache

    u32 DTCMSetting;
    u32 ITCMSetting;
    u32 DCacheLockDown;                             //! CP15: Data Cache Lockdown Register
    u32 ICacheLockDown;                             //! CP15: Instruction Cache Lockdown Register
    u32 CacheDebugRegisterIndex;                    //! CP15: Cache Debug Index Register
    u32 CP15TraceProcessId;                         //! CP15: Trace Process Id Register
    u32 CP15BISTTestStateRegister;                  //! CP15: BIST Test State Register

    // for aarch64 JIT they need to go up here
    // to be addressable by a 12-bit immediate
    u32 ITCMSize;
    u32 DTCMBase, DTCMMask;
    s32 RegionCodeCycles;

    u8 ITCM[ITCMPhysicalSize];
    u8* DTCM;

    u8 ICache[ICACHE_SIZE];                         //! Instruction Cache Content organized in @ref ICACHE_LINESPERSET times @ref ICACHE_SETS times @ref ICACHE_LINELENGTH bytes
    u32 ICacheTags[ICACHE_LINESPERSET*ICACHE_SETS]; //! Instruction Cache Tags organized in @ref ICACHE_LINESPERSET times @ref ICACHE_SETS Tags
    u8 ICacheCount;                                 //! Global instruction line fill counter. Used for round-robin replacement strategy with the instruction cache

    u8 DCache[DCACHE_SIZE];                         //! Data Cache Content organized in @ref DCACHE_LINESPERSET times @ref DCACHE_SETS times @ref DCACHE_LINELENGTH bytes
    u32 DCacheTags[DCACHE_LINESPERSET*DCACHE_SETS]; //! Data Cache Tags organized in @ref DCACHE_LINESPERSET times @ref DCACHE_SETS Tags
    u8 DCacheCount;                                 //! Global data line fill counter. Used for round-robin replacement strategy with the instruction cache

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
    //#define PU_Map PU_PrivMap
    u8* PU_Map;

    // code/16N/32N/32S
    u8 MemTimings[0x100000][4];

    bool (*GetMemRegion)(u32 addr, bool write, MemRegion* region);

#ifdef GDBSTUB_ENABLED
    u32 ReadMem(u32 addr, int size) override;
    void WriteMem(u32 addr, int size, u32 v) override;
#endif

protected:
    u8 BusRead8(u32 addr) override;
    u16 BusRead16(u32 addr) override;
    u32 BusRead32(u32 addr) override;
    void BusWrite8(u32 addr, u8 val) override;
    void BusWrite16(u32 addr, u16 val) override;
    void BusWrite32(u32 addr, u32 val) override;
};

class ARMv4 : public ARM
{
public:
    ARMv4(melonDS::NDS& nds, std::optional<GDBArgs> gdb, bool jit);

    void FillPipeline() override;

    void JumpTo(u32 addr, bool restorecpsr = false) override;

    void Execute() override;
#ifdef JIT_ENABLED
    void ExecuteJIT() override;
#endif

    u16 CodeRead16(const u32 addr)
    {
        return BusRead16(addr);
    }

    u32 CodeRead32(const u32 addr)
    {
        return BusRead32(addr);
    }

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
    u8 BusRead8(u32 addr) override;
    u16 BusRead16(u32 addr) override;
    u32 BusRead32(u32 addr) override;
    void BusWrite8(u32 addr, u8 val) override;
    void BusWrite16(u32 addr, u16 val) override;
    void BusWrite32(u32 addr, u32 val) override;
};

namespace ARMInterpreter
{

void A_UNK(ARM* cpu);
void T_UNK(ARM* cpu);

}
}
#endif // ARM_H
