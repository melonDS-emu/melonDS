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
#include <cstring>

#include "types.h"
#include "MemRegion.h"
#include "MemConstants.h"
#include "CP15_Constants.h"
#include "Platform.h"

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

enum class WBMode
{
    Check,
    Force,
    SingleBurst,
    WaitEntry,
};

enum class MainRAMType : u8
{
    Null = 0,
    Fetch,
    ICacheStream,
    DCacheStream,
    DMA16,
    DMA32,

    WriteBufferCmds, // all write buffer commands must be below this one; wb cmds are not strictly used for main ram

    WBDrain,
    WBWrite,
    WBCheck,
    WBWaitRead,
    WBWaitWrite,
};

// each one represents a bit in the field
enum FetchFlags
{
    MR8 = 0x00, // tbh it only exists because it felt wrong to write nothing to the field for 8 bit reads
    MR16 = 0x01,
    MR32 = 0x02,
    MRWrite = 0x20,
    MRSequential = 0x40,
    MRCodeFetch = 0x80,
};

struct MainRAMTrackers
{
    MainRAMType Type;
    u8 Var;
    u8 Progress;
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

    void SetGdbArgs(std::optional<GDBArgs> gdb);

    virtual void Reset();

    virtual void DoSavestate(Savestate* file);

    virtual void FillPipeline() = 0;

    virtual void JumpTo(u32 addr, bool restorecpsr = false, u8 R15 = 0) = 0;
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


    virtual bool DataRead8(u32 addr, u8 reg) = 0;
    virtual bool DataRead16(u32 addr, u8 reg) = 0;
    virtual bool DataRead32(u32 addr, u8 reg) = 0;
    virtual bool DataRead32S(u32 addr, u8 reg) = 0;
    virtual bool DataWrite8(u32 addr, u8 val, u8 reg) = 0;
    virtual bool DataWrite16(u32 addr, u16 val, u8 reg) = 0;
    virtual bool DataWrite32(u32 addr, u32 val, u8 reg) = 0;
    virtual bool DataWrite32S(u32 addr, u32 val, u8 reg) = 0;

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

    alignas(64) u32 R[16]; // heh
    u32 CPSR;
    u32 R_FIQ[8]; // holding SPSR too
    u32 R_SVC[3];
    u32 R_ABT[3];
    u32 R_IRQ[3];
    u32 R_UND[3];
    u64 CurInstr;
    u64 NextInstr[2];

    u32 ExceptionBase;

    MemRegion CodeMem;

    MainRAMTrackers MRTrack;

    u32 BranchAddr;
    u8 BranchUpdate;
    bool BranchRestore;

    u32 QueueMode[2];
    u8 ExtReg;
    u8 ExtROROffs;

    u64 RetVal;

    u16 LDRRegs;
    u16 LDRFailedRegs;
    u16 STRRegs;
    u32 FetchAddr[17];
    u32 STRVal[16];

    u64 IRQTimestamp;

    u8 FuncQueueFill;
    u8 FuncQueueEnd;
    u8 FuncQueueProg;
    u8 ExecuteCycles;
    bool FuncQueueActive;
    bool CheckInterlock;

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

    void JumpTo(u32 addr, bool restorecpsr = false, u8 R15 = 0) override;

    void PrefetchAbort();
    void DataAbort();

    template <CPUExecuteMode mode>
    void Execute();

    // all code accesses are forced nonseq 32bit
    void CodeRead32(const u32 addr);

    bool DataRead8(u32 addr, u8 reg) override;
    bool DataRead16(u32 addr, u8 reg) override;
    bool DataRead32(u32 addr, u8 reg) override;
    bool DataRead32S(u32 addr, u8 reg) override;
    bool DataWrite8(u32 addr, u8 val, u8 reg) override;
    bool DataWrite16(u32 addr, u16 val, u8 reg) override;
    bool DataWrite32(u32 addr, u32 val, u8 reg) override;
    bool DataWrite32S(u32 addr, u32 val, u8 reg) override;

    void CodeFetch();

    void AddCycles_C() override
    {
        //ExecuteCycles = 0;
        //CodeFetch();
    }

    void AddCycles_CI(s32 numX) override
    {
        ExecuteCycles = numX;
        QueueFunction(&ARMv5::AddExecute);
    }

    void AddCycles_MW(s32 numM)
    {
        DataCycles = numM;
        QueueFunction(&ARMv5::AddCycles_MW_2);
    }

    void AddCycles_CDI() override
    {
        AddCycles_MW(DataCycles);
    }

    void AddCycles_CD() override
    {
        Store = true; // todo: queue this
        AddCycles_MW(DataCycles);
    }

    void DelayIfITCM(s8 delay)
    {
        ITCMDelay = delay;
        QueueFunction(&ARMv5::DelayIfITCM_2);
    }

    inline void SetupInterlock(u8 reg, s8 delay = 0)
    {
        ILQueueReg = reg;
        ILQueueDelay = delay;

        QueueFunction(&ARMv5::SetupInterlock_2);
    }

    template <bool bitfield>
    inline void HandleInterlocksExecute(u16 ilmask, u8* times = NULL)
    {
        if constexpr (bitfield) ILQueueMask = ilmask;
        else ILQueueMask = 1<<ilmask;

        if (times == NULL) memset(ILQueueTimes, 0, sizeof(ILQueueTimes));
        else               memcpy(ILQueueTimes, times, sizeof(ILQueueTimes));

        QueueFunction(&ARMv5::HandleInterlocksExecute_2);
    }

    inline void ForceInterlock(s8 delay = 0)
    {
        ILForceDelay = delay;
        QueueFunction(&ARMv5::ForceInterlock_2);
    }

    inline void HandleInterlocksMemory(u8 reg)
    {
        ILQueueMemReg = reg;

        QueueFunction(&ARMv5::HandleInterlocksMemory_2);
    }

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
    bool ICacheLookup(const u32 addr);

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
    
    template <WBMode mode> bool WriteBufferHandle();
    template <int next> void WriteBufferCheck();
    void WriteBufferWrite(u32 val, u8 flag, u32 addr = 0);
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
    bool DCacheLookup(const u32 addr);

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

    void QueueFunction(void (ARMv5::*QueueEntry)(void));
    
    int GetIDFromQueueFunc(void (ARMv5::*funcptr)(void))
    {
        if (funcptr == &ARMv5::StartExecARM) return 0;
        else if (funcptr == &ARMv5::ContExecARM) return 1;
        else if (funcptr == &ARMv5::StartExecTHUMB) return 2;
        else if (funcptr == &ARMv5::ContExecTHUMB) return 3;
        else if (funcptr == &ARMv5::AddExecute) return 4;
        else if (funcptr == &ARMv5::AddCycles_MW_2) return 5;
        else if (funcptr == &ARMv5::DelayIfITCM_2) return 6;
        else if (funcptr == &ARMv5::JumpTo_2) return 7;
        else if (funcptr == &ARMv5::JumpTo_3A) return 8;
        else if (funcptr == &ARMv5::JumpTo_3B) return 9;
        else if (funcptr == &ARMv5::JumpTo_3C) return 10;
        else if (funcptr == &ARMv5::JumpTo_4) return 11;
        else if (funcptr == &ARMv5::CodeRead32_2) return 12;
        else if (funcptr == &ARMv5::CodeRead32_3) return 13;
        else if (funcptr == &ARMv5::CodeRead32_4) return 14;
        else if (funcptr == &ARMv5::ICacheLookup_2) return 15;
        else if (funcptr == &ARMv5::DAbortHandle) return 16;
        else if (funcptr == &ARMv5::DCacheFin8) return 17;
        else if (funcptr == &ARMv5::DRead8_2) return 18;
        else if (funcptr == &ARMv5::DRead8_3) return 19;
        else if (funcptr == &ARMv5::DRead8_4) return 20;
        else if (funcptr == &ARMv5::DRead8_5) return 21;
        else if (funcptr == &ARMv5::DCacheFin16) return 22;
        else if (funcptr == &ARMv5::DRead16_2) return 23;
        else if (funcptr == &ARMv5::DRead16_3) return 24;
        else if (funcptr == &ARMv5::DRead16_4) return 25;
        else if (funcptr == &ARMv5::DRead16_5) return 26;
        else if (funcptr == &ARMv5::DCacheFin32) return 27;
        else if (funcptr == &ARMv5::DRead32_2) return 28;
        else if (funcptr == &ARMv5::DRead32_3) return 29;
        else if (funcptr == &ARMv5::DRead32_4) return 30;
        else if (funcptr == &ARMv5::DRead32_5) return 31;
        else if (funcptr == &ARMv5::DRead32S_2) return 32;
        else if (funcptr == &ARMv5::DRead32S_3) return 33;
        else if (funcptr == &ARMv5::DRead32S_4) return 34;
        else if (funcptr == &ARMv5::DRead32S_5A) return 35;
        else if (funcptr == &ARMv5::DRead32S_5B) return 36;
        else if (funcptr == &ARMv5::DWrite8_2) return 37;
        else if (funcptr == &ARMv5::DWrite8_3) return 38;
        else if (funcptr == &ARMv5::DWrite8_4) return 39;
        else if (funcptr == &ARMv5::DWrite8_5) return 40;
        else if (funcptr == &ARMv5::DWrite16_2) return 41;
        else if (funcptr == &ARMv5::DWrite16_3) return 42;
        else if (funcptr == &ARMv5::DWrite16_4) return 43;
        else if (funcptr == &ARMv5::DWrite16_5) return 44;
        else if (funcptr == &ARMv5::DWrite32_2) return 45;
        else if (funcptr == &ARMv5::DWrite32_3) return 46;
        else if (funcptr == &ARMv5::DWrite32_4) return 47;
        else if (funcptr == &ARMv5::DWrite32_5) return 48;
        else if (funcptr == &ARMv5::DWrite32S_2) return 49;
        else if (funcptr == &ARMv5::DWrite32S_3) return 50;
        else if (funcptr == &ARMv5::DWrite32S_4) return 51;
        else if (funcptr == &ARMv5::DWrite32S_5A) return 52;
        else if (funcptr == &ARMv5::DWrite32S_5B) return 53;
        else if (funcptr == &ARMv5::WBCheck_2) return 54;
        else if (funcptr == &ARMv5::ICachePrefetch_2) return 55;
        else if (funcptr == &ARMv5::DCacheLookup_2) return 56;
        else if (funcptr == &ARMv5::DCacheLookup_3) return 57;
        else if (funcptr == &ARMv5::DCClearAddr_2) return 58;
        else if (funcptr == &ARMv5::DCClearSetWay_2) return 59;
        else if (funcptr == &ARMv5::DCClearInvalidateAddr_2) return 60;
        else if (funcptr == &ARMv5::DCClearInvalidateSetWay_2) return 61;
        else if (funcptr == &ARMv5::SetupInterlock_2) return 62;
        else if (funcptr == &ARMv5::HandleInterlocksExecute_2) return 63;
        else if (funcptr == &ARMv5::HandleInterlocksMemory_2) return 64;
        else if (funcptr == &ARMv5::ForceInterlock_2) return 65;
        else if (funcptr == &ARMv5::QueueUpdateMode) return 66;
        else if (funcptr == &ARMv5::SignExtend8) return 67;
        else if (funcptr == &ARMv5::SignExtend16) return 68;
        else if (funcptr == &ARMv5::ROR32) return 69;
        else { Platform::Log(Platform::LogLevel::Error, "ARM9: INVALID FUNCTION POINTER FOR SAVESTATES; DID SOMEONE FORGET TO UPDATE SERIALIZATION?\n"); return -1; }
    }

    typedef void (ARMv5::*funcptrA9)(void);
    funcptrA9 GetQueueFuncFromID(int funcid)
    {
        switch(funcid)
        {
        case 0: return &ARMv5::StartExecARM;
        case 1: return &ARMv5::ContExecARM;
        case 2: return &ARMv5::StartExecTHUMB;
        case 3: return &ARMv5::ContExecTHUMB;
        case 4: return &ARMv5::AddExecute;
        case 5: return &ARMv5::AddCycles_MW_2;
        case 6: return &ARMv5::DelayIfITCM_2;
        case 7: return &ARMv5::JumpTo_2;
        case 8: return &ARMv5::JumpTo_3A;
        case 9: return &ARMv5::JumpTo_3B;
        case 10: return &ARMv5::JumpTo_3C;
        case 11: return &ARMv5::JumpTo_4;
        case 12: return &ARMv5::CodeRead32_2;
        case 13: return &ARMv5::CodeRead32_3;
        case 14: return &ARMv5::CodeRead32_4;
        case 15: return &ARMv5::ICacheLookup_2;
        case 16: return &ARMv5::DAbortHandle;
        case 17: return &ARMv5::DCacheFin8;
        case 18: return &ARMv5::DRead8_2;
        case 19: return &ARMv5::DRead8_3;
        case 20: return &ARMv5::DRead8_4;
        case 21: return &ARMv5::DRead8_5;
        case 22: return &ARMv5::DCacheFin16;
        case 23: return &ARMv5::DRead16_2;
        case 24: return &ARMv5::DRead16_3;
        case 25: return &ARMv5::DRead16_4;
        case 26: return &ARMv5::DRead16_5;
        case 27: return &ARMv5::DCacheFin32;
        case 28: return &ARMv5::DRead32_2;
        case 29: return &ARMv5::DRead32_3;
        case 30: return &ARMv5::DRead32_4;
        case 31: return &ARMv5::DRead32_5;
        case 32: return &ARMv5::DRead32S_2;
        case 33: return &ARMv5::DRead32S_3;
        case 34: return &ARMv5::DRead32S_4;
        case 35: return &ARMv5::DRead32S_5A;
        case 36: return &ARMv5::DRead32S_5B;
        case 37: return &ARMv5::DWrite8_2;
        case 38: return &ARMv5::DWrite8_3;
        case 39: return &ARMv5::DWrite8_4;
        case 40: return &ARMv5::DWrite8_5;
        case 41: return &ARMv5::DWrite16_2;
        case 42: return &ARMv5::DWrite16_3;
        case 43: return &ARMv5::DWrite16_4;
        case 44: return &ARMv5::DWrite16_5;
        case 45: return &ARMv5::DWrite32_2;
        case 46: return &ARMv5::DWrite32_3;
        case 47: return &ARMv5::DWrite32_4;
        case 48: return &ARMv5::DWrite32_5;
        case 49: return &ARMv5::DWrite32S_2;
        case 50: return &ARMv5::DWrite32S_3;
        case 51: return &ARMv5::DWrite32S_4;
        case 52: return &ARMv5::DWrite32S_5A;
        case 53: return &ARMv5::DWrite32S_5B;
        case 54: return &ARMv5::WBCheck_2;
        case 55: return &ARMv5::ICachePrefetch_2;
        case 56: return &ARMv5::DCacheLookup_2;
        case 57: return &ARMv5::DCacheLookup_3;
        case 58: return &ARMv5::DCClearAddr_2;
        case 59: return &ARMv5::DCClearSetWay_2;
        case 60: return &ARMv5::DCClearInvalidateAddr_2;
        case 61: return &ARMv5::DCClearInvalidateSetWay_2;
        case 62: return &ARMv5::SetupInterlock_2;
        case 63: return &ARMv5::HandleInterlocksExecute_2;
        case 64: return &ARMv5::HandleInterlocksMemory_2;
        case 65: return &ARMv5::ForceInterlock_2;
        case 66: return &ARMv5::QueueUpdateMode;
        case 67: return &ARMv5::SignExtend8;
        case 68: return &ARMv5::SignExtend16;
        case 69: return &ARMv5::ROR32;
        default: Platform::Log(Platform::LogLevel::Error, "ARM9: INVALID FUNCTION ID FOR LOADING SAVESTATES; EITHER THE SAVESTATE IS BORKED OR SOMEONE FORGOT TO UPDATE SERIALIZATION\n"); return nullptr;
        }
    }

    // Queue Functions
    void StartExecARM();
    void ContExecARM();
    void StartExecTHUMB();
    void ContExecTHUMB();
    void AddExecute();
    void AddCycles_MW_2();
    void DelayIfITCM_2();
    void JumpTo_2();
    void JumpTo_3A();
    void JumpTo_3B();
    void JumpTo_3C();
    void JumpTo_4();
    void CodeRead32_2();
    void CodeRead32_3();
    void CodeRead32_4();
    void ICacheLookup_2();
    void DAbortHandle();
    void DCacheFin8();
    void DRead8_2();
    void DRead8_3();
    void DRead8_4();
    void DRead8_5();
    void DCacheFin16();
    void DRead16_2();
    void DRead16_3();
    void DRead16_4();
    void DRead16_5();
    void DCacheFin32();
    void DRead32_2();
    void DRead32_3();
    void DRead32_4();
    void DRead32_5();
    void DRead32S_2();
    void DRead32S_3();
    void DRead32S_4();
    void DRead32S_5A();
    void DRead32S_5B();
    void DWrite8_2();
    void DWrite8_3();
    void DWrite8_4();
    void DWrite8_5();
    void DWrite16_2();
    void DWrite16_3();
    void DWrite16_4();
    void DWrite16_5();
    void DWrite32_2();
    void DWrite32_3();
    void DWrite32_4();
    void DWrite32_5();
    void DWrite32S_2();
    void DWrite32S_3();
    void DWrite32S_4();
    void DWrite32S_5A();
    void DWrite32S_5B();
    void WBCheck_2();
    void ICachePrefetch_2();
    void DCacheLookup_2();
    void DCacheLookup_3();
    void DCClearAddr_2();
    void DCClearSetWay_2();
    void DCClearInvalidateAddr_2();
    void DCClearInvalidateSetWay_2();
    void SetupInterlock_2();
    void HandleInterlocksExecute_2();
    void HandleInterlocksMemory_2();
    void ForceInterlock_2();
    void QueueUpdateMode() { UpdateMode(QueueMode[0], QueueMode[1], true); }
    void SignExtend8() { R[ExtReg] = (s8)R[ExtReg]; }
    void SignExtend16() { R[ExtReg] = (s16)R[ExtReg]; }
    void ROR32() { R[ExtReg] = ROR(R[ExtReg], ExtROROffs); }



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

    alignas(u32) u8 ITCM[ITCMPhysicalSize];                      //! Content of the ITCM
    u8* DTCM;                                       //! Content of the DTCM

    alignas(u32) u8 ICache[ICACHE_SIZE];                         //! Instruction Cache Content organized in @ref ICACHE_LINESPERSET times @ref ICACHE_SETS times @ref ICACHE_LINELENGTH bytes
    u32 ICacheTags[ICACHE_LINESPERSET*ICACHE_SETS]; //! Instruction Cache Tags organized in @ref ICACHE_LINESPERSET times @ref ICACHE_SETS Tags
    u8 ICacheCount;                                 //! Global instruction line fill counter. Used for round-robin replacement strategy with the instruction cache

    alignas(u32) u8 DCache[DCACHE_SIZE];                         //! Data Cache Content organized in @ref DCACHE_LINESPERSET times @ref DCACHE_SETS times @ref DCACHE_LINELENGTH bytes
    u32 DCacheTags[DCACHE_LINESPERSET*DCACHE_SETS]; //! Data Cache Tags organized in @ref DCACHE_LINESPERSET times @ref DCACHE_SETS Tags
    u8 DCacheCount;                                 //! Global data line fill counter. Used for round-robin replacement strategy with the instruction cache

    u32 PU_CodeCacheable;                           //! CP15 Register 2 Opcode2 1: Code Cachable Bits
    u32 PU_DataCacheable;                           //! CP15 Register 2 Opcode2 0: Data Cachable Bits
    u32 PU_WriteBufferability;                      //! CP15 Register 3 Opcode2 0: Write Buffer Control Register

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
                                                     *      5 - CP15_MAP_BUFFERABLE
                                                     *      6 - CP15_MAP_ICACHEABLE
                                                     */
    u8 PU_UserMap[CP15_MAP_ENTRYCOUNT];             //! Memory mapping flags for User Mode
    u8* PU_Map;                                     //! Current valid Region Mapping (is either @ref PU_PrivMap or PU_UserMap)

    // code/16N/32N/32S
    u8 MemTimings[0x40000][3];

    bool (*GetMemRegion)(u32 addr, bool write, MemRegion* region);

    alignas(64) void (ARMv5::*DelayedQueue)(void); // adding more than one new entry to the queue while it's already active does not work. so uh. we use this to work around that. it's less than ideal...
    void (ARMv5::*StartExec)(void);
    void (ARMv5::*FuncQueue[32])(void);
    u64 ITCMTimestamp;
    u64 TimestampMemory;
    bool Store;
    s8 ITCMDelay;
    u32 QueuedDCacheLine;
    u32 CP15Queue;

    u8 ILCurrReg;
    u8 ILPrevReg;
    u64 ILCurrTime;
    u64 ILPrevTime;
    u8 ILQueueReg;
    s8 ILQueueDelay;
    u8 ILQueueMemReg;
    u8 ILQueueTimes[16];
    u16 ILQueueMask;

    u8 ICacheStreamPtr;
    u8 DCacheStreamPtr;
    u64 ICacheStreamTimes[7];
    u64 DCacheStreamTimes[7];

    s8 ILForceDelay;
    u8 WBWritePointer; // which entry to attempt to write next; should always be ANDed with 0xF after incrementing
    u8 WBFillPointer; // where the next entry should be added; should always be ANDed with 0xF after incrementing
    u8 WBWriting; // whether the buffer is actively trying to perform a write
    u32 WBCurAddr; // address the write buffer is currently writing to
    u64 WBCurVal; // current value being written; 0-31: val | 61-63: flag; 0 = byte ns; 1 = halfword ns; 2 = word ns; 3 = word s; 4 = address (invalid in this variable)
    u32 WBAddrQueued[40];
    u32 storeaddr[16]; // temp until i figure out why using the fifo address entries directly didn't work
    u64 WBValQueued[40];
    u64 WriteBufferFifo[16]; // 0-31: val | 61-63: flag; 0 = byte ns; 1 = halfword ns; 2 = word ns; 3 = word s; 4 = address
    u64 WBTimestamp; // current timestamp
    //u64 WBMainRAMDelay; // timestamp used to emulate the delay before the next main ram write can begin
    u64 WBDelay; // timestamp in bus cycles use for the delay before next write to the write buffer can occur (seems to be a 1 cycle delay after a write to it)
    u32 WBLastRegion; // the last region written to by the write buffer
    u64 WBReleaseTS; // the timestamp on which the write buffer relinquished control of the bus back
    u64 WBInitialTS; // what cycle the entry was first sent in

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

    void DoSavestate(Savestate* file) override;

    void FillPipeline() override;

    void JumpTo(u32 addr, bool restorecpsr = false, u8 R15 = 0) override;

    template <CPUExecuteMode mode>
    void Execute();

    alignas(64) void (ARMv4::*StartExec)(void);
    void (ARMv4::*FuncQueue[32])(void);
    bool Nonseq;

    void CodeRead16(u32 addr);
    void CodeRead32(u32 addr);

    bool DataRead8(u32 addr, u8 reg) override;
    bool DataRead16(u32 addr, u8 reg) override;
    bool DataRead32(u32 addr, u8 reg) override;
    bool DataRead32S(u32 addr, u8 reg) override;
    bool DataWrite8(u32 addr, u8 val, u8 reg) override;
    bool DataWrite16(u32 addr, u16 val, u8 reg) override;
    bool DataWrite32(u32 addr, u32 val, u8 reg) override;
    bool DataWrite32S(u32 addr, u32 val, u8 reg) override;
    void AddCycles_C() override;
    void AddCycles_CI(s32 num) override;
    void AddCycles_CDI() override;
    void AddCycles_CD() override;
    
    void QueueFunction(void (ARMv4::*QueueEntry)(void));

    int GetIDFromQueueFunc(void (ARMv4::*funcptr)(void))
    {
        if (funcptr == &ARMv4::StartExecARM) return 0;
        else if (funcptr == &ARMv4::StartExecTHUMB) return 1;
        else if (funcptr == &ARMv4::UpdateNextInstr1) return 2;
        else if (funcptr == &ARMv4::JumpTo_2) return 3;
        else if (funcptr == &ARMv4::JumpTo_3A) return 4;
        else if (funcptr == &ARMv4::JumpTo_3B) return 5;
        else if (funcptr == &ARMv4::DRead8_2) return 6; 
        else if (funcptr == &ARMv4::DRead16_2) return 7;
        else if (funcptr == &ARMv4::DRead32_2) return 8;
        else if (funcptr == &ARMv4::DRead32S_2) return 9;
        else if (funcptr == &ARMv4::DWrite8_2) return 10;
        else if (funcptr == &ARMv4::DWrite16_2) return 11;
        else if (funcptr == &ARMv4::DWrite32_2) return 12;
        else if (funcptr == &ARMv4::DWrite32S_2) return 13;
        else if (funcptr == &ARMv4::AddExecute) return 14;
        else if (funcptr == &ARMv4::AddExtraCycle) return 15;
        else if (funcptr == &ARMv4::QueueUpdateMode) return 16;
        else if (funcptr == &ARMv4::SignExtend8) return 17;
        else if (funcptr == &ARMv4::SignExtend16) return 18;
        else if (funcptr == &ARMv4::ROR32) return 19;
        else { Platform::Log(Platform::LogLevel::Error, "ARM7: INVALID FUNCTION POINTER FOR SAVESTATES; DID SOMEONE FORGET TO UPDATE SERIALIZATION?\n"); return -1; }
    }

    typedef void (ARMv4::*funcptrA7)(void);
    funcptrA7 GetQueueFuncFromID(int funcid)
    {
        switch (funcid)
        {
        case 0: return &ARMv4::StartExecARM;
        case 1: return &ARMv4::StartExecTHUMB;
        case 2: return &ARMv4::UpdateNextInstr1;
        case 3: return &ARMv4::JumpTo_2;
        case 4: return &ARMv4::JumpTo_3A;
        case 5: return &ARMv4::JumpTo_3B;
        case 6: return &ARMv4::DRead8_2;
        case 7: return &ARMv4::DRead16_2;
        case 8: return &ARMv4::DRead32_2;
        case 9: return &ARMv4::DRead32S_2;
        case 10: return &ARMv4::DWrite8_2;
        case 11: return &ARMv4::DWrite16_2;
        case 12: return &ARMv4::DWrite32_2;
        case 13: return &ARMv4::DWrite32S_2;
        case 14: return &ARMv4::AddExecute;
        case 15: return &ARMv4::AddExtraCycle;
        case 16: return &ARMv4::QueueUpdateMode;
        case 17: return &ARMv4::SignExtend8;
        case 18: return &ARMv4::SignExtend16;
        case 19: return &ARMv4::ROR32;
        default: Platform::Log(Platform::LogLevel::Error, "ARM7: INVALID FUNCTION ID FOR LOADING SAVESTATES; EITHER THE SAVESTATE IS BORKED OR SOMEONE FORGOT TO UPDATE SERIALIZATION\n"); return nullptr;
        }
    }

    // Queue Functions
    void StartExecARM();
    void StartExecTHUMB();
    void UpdateNextInstr1() { NextInstr[1] = RetVal; }
    void JumpTo_2();
    void JumpTo_3A();
    void JumpTo_3B();
    void DRead8_2();
    void DRead16_2();
    void DRead32_2();
    void DRead32S_2();
    void DWrite8_2();
    void DWrite16_2();
    void DWrite32_2();
    void DWrite32S_2();
    void AddExecute();
    void AddExtraCycle();
    void QueueUpdateMode() { UpdateMode(QueueMode[0], QueueMode[1], true); }
    void SignExtend8() { if (!(LDRFailedRegs & 1<<ExtReg)) R[ExtReg] = (s8)R[ExtReg]; }
    void SignExtend16() { if (!(LDRFailedRegs & 1<<ExtReg)) R[ExtReg] = (s16)R[ExtReg]; }
    void ROR32() { if (!(LDRFailedRegs & 1<<ExtReg)) R[ExtReg] = ROR(R[ExtReg], ExtROROffs); }

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
