/*
    Copyright 2016-2024 melonDS team

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

    template <CPUExecuteMode mode>
    void TriggerIRQ();

    void SetupCodeMem(u32 addr);


    virtual bool DataRead8(u32 addr, u32* val) = 0;
    virtual bool DataRead16(u32 addr, u32* val) = 0;
    virtual bool DataRead32(u32 addr, u32* val) = 0;
    virtual bool DataRead32S(u32 addr, u32* val) = 0;
    virtual bool DataWrite8(u32 addr, u8 val) = 0;
    virtual bool DataWrite16(u32 addr, u16 val) = 0;
    virtual bool DataWrite32(u32 addr, u32 val) = 0;
    virtual bool DataWrite32S(u32 addr, u32 val) = 0;

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

    u64 MainRAMTimestamp;

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

    template <CPUExecuteMode mode>
    void Execute();

    // all code accesses are forced nonseq 32bit
    u32 CodeRead32(const u32 addr, const bool branch);

    bool DataRead8(u32 addr, u32* val) override;
    bool DataRead16(u32 addr, u32* val) override;
    bool DataRead32(u32 addr, u32* val) override;
    bool DataRead32S(u32 addr, u32* val) override;
    bool DataWrite8(u32 addr, u8 val) override;
    bool DataWrite16(u32 addr, u16 val) override;
    bool DataWrite32(u32 addr, u32 val) override;
    bool DataWrite32S(u32 addr, u32 val) override;

    void CodeFetch();

    void AddCycles_C() override { CodeFetch(); }

    void AddCycles_CI(s32 numX) override;

    void AddCycles_MW(s32 numM);

    void AddCycles_CDI() override
    {
        AddCycles_MW(DataCycles);
        DataCycles = 0;
    }

    void AddCycles_CD() override
    {
        Store = true;
        AddCycles_MW(DataCycles);
        DataCycles = 0;
    }
    
    template <bool bitfield>
    void HandleInterlocksExecute(u16 ilmask);
    void HandleInterlocksMemory(u8 reg);

    void GetCodeMemRegion(const u32 addr, MemRegion* region);

    /**
     * @brief Resets the state of all CP15 registers and variables
     * to power up state.
     * @par Returns
     *      Nothing
     */
    void CP15Reset();

    /**
     * @brief handles read and write operations to a save-state
     * file.
     * @param [in] file Savestate file
     * @par Returns
     *      Nothing
     */
    void CP15DoSavestate(Savestate* file);

    /**
     * @brief Calculates the internal state from @ref DTCMSettings
     * @par Returns
     *      Nothing
     */
    void UpdateDTCMSetting();

    /**
     * @brief Calculates the internal state from @ref ITCMSettings
     * @par Returns
     *      Nothing
     */
    void UpdateITCMSetting();

    /**
     * @brief Calculates the internal state from the
     * region protection bits of a specific region number
     * @details
     * This function updates the PU_####Map array in all
     * parts that are occupied by this region. Updating a single
     * region does not take into account the priority of the 
     * regions.
     * @param [in] n index of the region from 0 to @ref CP15_REGION_COUNT - 1
     * @par Returns
     *      Nothing
     */
    void UpdatePURegion(const u32 n);

    /** 
     * @brief Calculates the internal state from all region
     * protection bits.
     * @details
     * This function updates the internal state in order from the
     * least to the most priotized regions, so that the 
     * priority of the regions match the internal state
     * @par Returns
     *      Nothing
     */
    void UpdatePURegions(const bool update_all);

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
    
    template <bool force> inline bool WriteBufferHandle();
    void WriteBufferCheck();
    void WriteBufferWrite(u32 val, u8 flag, u8 cycles, u32 addr = 0);
    void WriteBufferDrain();

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
     * @retval true, if the data was written into the cache and
     *               does not need to be updated until cache is
     *               cleaned
     *         false, to write through
     */
    bool DCacheWrite32(const u32 addr, const u32 val);

    /**
     * @brief Updates a word in the data cache if present
     * @param [in] addr Memory address which is written
     * @param [in] val Half-Word value to be written
     * @retval true, if the data was written into the cache and
     *               does not need to be updated until cache is
     *               cleaned
     *         false, to write through
     */
    bool DCacheWrite16(const u32 addr, const u16 val);

    /**
     * @brief Updates a word in the data cache if present
     * @param [in] addr Memory address which is written
     * @param [in] val Byte value to be written
     * @retval true, if the data was written into the cache and
     *               does not need to be updated until cache is
     *               cleaned
     *         false, to write through
     */
    bool DCacheWrite8(const u32 addr, const u8 val);

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
     * If write-back is enabled in conjunction with the data cache
     * the dirty flags in tags are set if the corresponding cache
     * line is written to.
     * A clean will write the parts of the cache line back
     * that is marked dirty and adds the required cycles to the
     * @ref DataCyces member.
     * @par Returns
     *      Nothing
     */
    void DCacheClearAll();

    /**
     * @brief Cleans a data cache line
     * @details
     * If write-back is enabled in conjunction with the data cache
     * the dirty flags in tags are set if the corresponding cache
     * line is written to.
     * A clean will write the parts of the cache line back
     * that is marked dirty and adds the required cycles to the
     * @ref DataCyces member.
     * @param [in] addr Memory address of the data in the cache line
     * @par Returns
     *      Nothing
     */
    void DCacheClearByAddr(const u32 addr);

    /**
     * @brief Cleans a data cache line
     * @details
     * If write-back is enabled in conjunction with the data cache
     * the dirty flags in tags are set if the corresponding cache
     * line is written to.
     * A clean will write the parts of the cache line back
     * that is marked dirty and adds the required cycles to the
     * @ref DataCyces member.
     * @param [in] cacheSet index of the internal cache set from
     * 0 to @ref DCACHE_SETS - 1
     * @param [in] cacheLine index of the line within the cache set
     * from 0 to @ref DCACHE_LINESPERSET - 1
     * @par Returns
     *      Nothing
     */
    void DCacheClearByASetAndWay(const u8 cacheSet, const u8 cacheLine);

    /**
     * @brief Handles MCR operations writing to cp15 registers
     * @details
     * This function updates the internal state of the emulator when
     * a cp15 register is written, or triggers the corresponding action
     * like flushing caches.
     * 
     * @param [in] id the operation id to be performed, consisting of 
     * (from lower to higher nibble) opcode2, intermediate register, 
     * register and opcode1. Most write operations just take the first 3
     * into account.
     * param [in] val value to be written to the cp15 register
     * @par Returns
     *      Nothing
     */
    void CP15Write(const u32 id, const u32 val);

    /**
     * @brief handles MRC operations reading from cp15 registers
     * @details
     * This function accumulates the regsiter states from the internal
     * emulator state. It does not modify the internal state of the
     * emulator or cp15.
     * @param  [in] id the operation id to be performed, consisting of 
     * (from lower to higher nibble) opcode2, intermediate register, 
     * register and opcode1. Most read operations just take the first 3
     * into account.
     * @return Value of the cp15 register
     */
    u32 CP15Read(const u32 id) const;

    u32 CP15Control;                                //! CP15 Register 1: Control Register

    u32 RNGSeed;                                    //! Global cache line fill seed. Used for pseudo random replacement strategy with the instruction and data cache

    u32 DTCMSetting;                                //! CP15 Register 9 Intermediate 1 Opcode2 0: Data Tightly-Coupled Memory register
    u32 ITCMSetting;                                //! CP15 Register 9 Intermediate 1 Opcode2 1: Instruction Tightly-Coupled Memory register
    u32 DCacheLockDown;                             //! CP15 Register 9 Intermediate 0 Opcode2 0: Data Cache Lockdown Register
    u32 ICacheLockDown;                             //! CP15 Register 9 Intermediate 0 Opcode2 1: Instruction Cache Lockdown Register
    u32 CacheDebugRegisterIndex;                    //! CP15: Cache Debug Index Register
    u32 CP15TraceProcessId;                         //! CP15: Trace Process Id Register
    u32 CP15BISTTestStateRegister;                  //! CP15: BIST Test State Register

    // for aarch64 JIT they need to go up here
    // to be addressable by a 12-bit immediate
    u32 ITCMSize;                                   //! Internal: Size of the memory ITCM is mapped to. @ref ITCM data repeats every @ref ITCMPhysicalSize withhin
    u32 DTCMBase;                                   //! Internal: DTCMBase Address. The DTCM can be accessed if the address & ~ @ref DTCMMask is equal to thhis base address
    u32 DTCMMask;                                   //! Internal: DTCM Address Mask used in conjunction with @ref DTCMBase to check for DTCM access
    s32 RegionCodeCycles;                           //! Internal: Cached amount of cycles to fetch instruction from the current code region.

    u8 ITCM[ITCMPhysicalSize];                      //! Content of the ITCM
    u8* DTCM;                                       //! Content of the DTCM

    u8 ICache[ICACHE_SIZE];                         //! Instruction Cache Content organized in @ref ICACHE_LINESPERSET times @ref ICACHE_SETS times @ref ICACHE_LINELENGTH bytes
    u32 ICacheTags[ICACHE_LINESPERSET*ICACHE_SETS]; //! Instruction Cache Tags organized in @ref ICACHE_LINESPERSET times @ref ICACHE_SETS Tags
    u8 ICacheCount;                                 //! Global instruction line fill counter. Used for round-robin replacement strategy with the instruction cache

    u8 DCache[DCACHE_SIZE];                         //! Data Cache Content organized in @ref DCACHE_LINESPERSET times @ref DCACHE_SETS times @ref DCACHE_LINELENGTH bytes
    u32 DCacheTags[DCACHE_LINESPERSET*DCACHE_SETS]; //! Data Cache Tags organized in @ref DCACHE_LINESPERSET times @ref DCACHE_SETS Tags
    u8 DCacheCount;                                 //! Global data line fill counter. Used for round-robin replacement strategy with the instruction cache

    u32 PU_CodeCacheable;                           //! CP15 Register 2 Opcode2 1: Code Cachable Bits
    u32 PU_DataCacheable;                           //! CP15 Register 2 Opcode2 0: Data Cachable Bits
    u32 PU_DataCacheWrite;                          //! CP15 Register 3 Opcode2 0: WriteBuffer Control Register

    u32 PU_CodeRW;                                  //! CP15 Register 5 Opcode2 3: Code Access Permission register
    u32 PU_DataRW;                                  //! CP15 Register 5 Opcode2 2: Data Access Permission register

    u32 PU_Region[CP15_REGION_COUNT];               //! CP15 Register 6 Opcode2 0..7: Protection Region Base and Size Register

    // 0=dataR 1=dataW 2=codeR 4=datacache 5=datawrite 6=codecache
    u8 PU_PrivMap[CP15_MAP_ENTRYCOUNT];                        /** 
                                                     * Memory mapping flags for Privileged Modes
                                                     * Bits:
                                                     *      0 - CP15_MAP_READABLE
                                                     *      1 - CP15_MAP_WRITEABLE
                                                     *      2 - CP15_MAP_EXECUTABLE
                                                     *      4 - CP15_MAP_DCACHEABLE
                                                     *      5 - CP15_MAP_DCACHEWRITEBACK
                                                     *      6 - CP15_MAP_ICACHEABLE
                                                     */
    u8 PU_UserMap[CP15_MAP_ENTRYCOUNT];             //! Memory mapping flags for User Mode
    u8* PU_Map;                                     //! Current valid Region Mapping (is either @ref PU_PrivMap or PU_UserMap)

    // code/16N/32N/32S
    u8 MemTimings[0x40000][3];

    bool (*GetMemRegion)(u32 addr, bool write, MemRegion* region);
    
    u64 ITCMTimestamp;
    u64 TimestampActual;
    u32 PC;
    bool NullFetch;
    bool Store;

    u8 ILCurrReg;
    u8 ILPrevReg;
    u64 ILCurrTime;
    u64 ILPrevTime;

    u8 ICacheFillPtr;
    u8 DCacheFillPtr;
    u64 ICacheFillTimes[7];
    u64 DCacheFillTimes[7];

    u8 WBWritePointer; // which entry to attempt to write next; should always be ANDed with 0xF after incrementing
    u8 WBFillPointer; // where the next entry should be added; should always be ANDed with 0xF after incrementing
    bool WBWriting; // whether the buffer is actively trying to perform a write
    u8 WBCurCycles; // how long the current write will take; bit 7 is a flag used to indicate main ram
    u64 WBCurVal; // current value being written; 0-31: val | 62-32: flag; 0 = byte; 1 = halfword; 2 = word; 3 = address (invalid in this variable)
    u32 WBCurAddr; // address the write buffer is currently writing to
    u32 storeaddr[16]; // temp until i figure out why using the fifo address entries directly didn't work
    u8 WBCycles[16]; // num cycles for each write; bit 7 is a flag used to indicate main ram
    u64 WriteBufferFifo[16]; // 0-31: val | 62-32: flag; 0 = byte; 1 = halfword; 2 = word; 3 = address
    u64 WBTimestamp; // current timestamp in bus cycles
    u64 WBMainRAMDelay; // timestamp in bus cycles used to emulate the delay before the next main ram write can begin
    u64 WBDelay; // timestamp in bus cycles use for the delay before next write to the write buffer can occur (seems to be a 1 cycle delay after a write to it)

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
    
    void Reset() override;

    void FillPipeline() override;

    void JumpTo(u32 addr, bool restorecpsr = false) override;

    template <CPUExecuteMode mode>
    void Execute();

    bool Nonseq;

    u16 CodeRead16(u32 addr);
    u32 CodeRead32(u32 addr);

    bool DataRead8(u32 addr, u32* val) override;
    bool DataRead16(u32 addr, u32* val) override;
    bool DataRead32(u32 addr, u32* val) override;
    bool DataRead32S(u32 addr, u32* val) override;
    bool DataWrite8(u32 addr, u8 val) override;
    bool DataWrite16(u32 addr, u16 val) override;
    bool DataWrite32(u32 addr, u32 val) override;
    bool DataWrite32S(u32 addr, u32 val) override;
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
