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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "DSi.h"
#include "ARM.h"
#include "Platform.h"
#include "ARMJIT_Memory.h"
#include "ARMJIT.h"
#include "CP15_Constants.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

// access timing for cached regions
// this would be an average between cache hits and cache misses
// this was measured to be close to hardware average
// a value of 1 would represent a perfect cache, but that causes
// games to run too fast, causing a number of issues
const int kDataCacheTiming = 3;//2;
const int kCodeCacheTiming = 3;//5;


/* CP15 Reset sets the default values within each registers and 
   memories of the CP15. 
   This includes the Settings for 
        DTCM
        ITCM
        Caches
        Regions
        Process Trace
*/

void ARMv5::CP15Reset()
{
    CP15Control = 0x2078; // dunno

    RNGSeed = 44203;

    DTCMSetting = 0;
    ITCMSetting = 0;

    memset(ITCM, 0, ITCMPhysicalSize);
    memset(DTCM, 0, DTCMPhysicalSize);

    ITCMSize = 0;
    DTCMBase = 0xFFFFFFFF;
    DTCMMask = 0;

    ICacheLockDown = 0;
    DCacheLockDown = 0;
    CacheDebugRegisterIndex = 0;
    CP15BISTTestStateRegister = 0;

    memset(ICache, 0, ICACHE_SIZE);
    ICacheInvalidateAll();
    ICacheCount = 0;

    memset(DCache, 0, DCACHE_SIZE);
    DCacheInvalidateAll();
    DCacheCount = 0;

    CP15TraceProcessId = 0;

    PU_CodeCacheable = 0;
    PU_DataCacheable = 0;
    PU_DataCacheWrite = 0;

    PU_CodeRW = 0;
    PU_DataRW = 0;

    memset(PU_Region, 0, CP15_REGION_COUNT*sizeof(u32));
    UpdatePURegions(true);

}

void ARMv5::CP15DoSavestate(Savestate* file)
{
    file->Section("CP15");

    file->Var32(&CP15Control);

    file->Var32(&DTCMSetting);
    file->Var32(&ITCMSetting);

    file->VarArray(ITCM, ITCMPhysicalSize);
    file->VarArray(DTCM, DTCMPhysicalSize);

    file->VarArray(ICache, sizeof(ICache));
    file->VarArray(ICacheTags, sizeof(ICacheTags));
    file->Var8(&ICacheCount);

    file->VarArray(DCache, sizeof(DCache));
    file->VarArray(DCacheTags, sizeof(DCacheTags));
    file->Var8(&DCacheCount);

    file->Var32(&DCacheLockDown);
    file->Var32(&ICacheLockDown);
    file->Var32(&CacheDebugRegisterIndex);
    file->Var32(&CP15TraceProcessId);
    file->Var32(&CP15BISTTestStateRegister);

    file->Var32(&PU_CodeCacheable);
    file->Var32(&PU_DataCacheable);
    file->Var32(&PU_DataCacheWrite);

    file->Var32(&PU_CodeRW);
    file->Var32(&PU_DataRW);

    file->VarArray(PU_Region, CP15_REGION_COUNT*sizeof(u32));

    if (!file->Saving)
    {
        UpdateDTCMSetting();
        UpdateITCMSetting();
        UpdatePURegions(true);
    }
}


void ARMv5::UpdateDTCMSetting()
{
    u32 newDTCMBase;
    u32 newDTCMMask;
    u32 newDTCMSize;

    if (CP15Control & CP15_TCM_CR_DTCM_ENABLE)
    {
        newDTCMSize = 0x200 << ((DTCMSetting >> 1) & 0x1F);
        if (newDTCMSize < 0x1000) newDTCMSize = 0x1000;
        newDTCMMask = 0xFFFFF000 & ~(newDTCMSize-1);
        newDTCMBase = DTCMSetting & newDTCMMask;
    }
    else
    {
        newDTCMSize = 0;
        newDTCMBase = 0xFFFFFFFF;
        newDTCMMask = 0;
    }

    if (newDTCMBase != DTCMBase || newDTCMMask != DTCMMask)
    {
        NDS.JIT.Memory.RemapDTCM(newDTCMBase, newDTCMSize);
        DTCMBase = newDTCMBase;
        DTCMMask = newDTCMMask;
    }
}

void ARMv5::UpdateITCMSetting()
{
    if (CP15Control & CP15_TCM_CR_ITCM_ENABLE)
    {
        ITCMSize = 0x200 << ((ITCMSetting >> 1) & 0x1F);
#ifdef JIT_ENABLED
        FastBlockLookupSize = 0;
#endif
    }
    else
    {
        ITCMSize = 0;
    }
}


// covers updates to a specific PU region's cache/etc settings
// (not to the region range/enabled status)
void ARMv5::UpdatePURegion(u32 n)
{
    if (!(CP15Control & CP15_CR_MPUENABLE))
        return;

    if (n >= CP15_REGION_COUNT)
        return;

    u32 coderw = (PU_CodeRW >> (CP15_REGIONACCESS_BITS_PER_REGION * n)) & CP15_REGIONACCESS_REGIONMASK;
    u32 datarw = (PU_DataRW >> (CP15_REGIONACCESS_BITS_PER_REGION * n)) & CP15_REGIONACCESS_REGIONMASK;

    u32 codecache, datacache, datawrite;

    // datacache/datawrite
    // 0/0: goes to memory
    // 0/1: goes to memory
    // 1/0: goes to memory and cache
    // 1/1: goes to cache

    if (CP15Control & CP15_CACHE_CR_ICACHEENABLE)
        codecache = (PU_CodeCacheable >> n) & 0x1;
    else
        codecache = 0;

    if (CP15Control & CP15_CACHE_CR_DCACHEENABLE)
    {
        datacache = (PU_DataCacheable >> n) & 0x1;
        datawrite = (PU_DataCacheWrite >> n) & 0x1;
    }
    else
    {
        datacache = 0;
        datawrite = 0;
    }

    u32 rgn = PU_Region[n];
    if (!(rgn & (1<<0)))
    {
        return;
    }

    u32 start = (rgn & CP15_REGION_BASE_MASK) >> CP15_MAP_ENTRYSIZE_LOG2;
    u32 sz = 2 << ((rgn & CP15_REGION_SIZE_MASK) >> 1);
    u32 end = start + (sz >> CP15_MAP_ENTRYSIZE_LOG2);
    // TODO: check alignment of start

    u8 usermask = CP15_MAP_NOACCESS;
    u8 privmask = CP15_MAP_NOACCESS;

    switch (datarw)
    {
    case 0: break;
    case 1: privmask |= CP15_MAP_READABLE | CP15_MAP_WRITEABLE; break;
    case 2: privmask |= CP15_MAP_READABLE | CP15_MAP_WRITEABLE; usermask |= CP15_MAP_READABLE; break;
    case 3: privmask |= CP15_MAP_READABLE | CP15_MAP_WRITEABLE; usermask |= CP15_MAP_READABLE | CP15_MAP_WRITEABLE; break;
    case 5: privmask |= CP15_MAP_READABLE; break;
    case 6: privmask |= CP15_MAP_READABLE; usermask |= CP15_MAP_READABLE; break;
    default: Log(LogLevel::Warn, "!! BAD DATARW VALUE %d\n", datarw&0xF);
    }

    switch (coderw)
    {
    case 0: break;
    case 1: privmask |= CP15_MAP_EXECUTABLE; break;
    case 2: privmask |= CP15_MAP_EXECUTABLE; usermask |= CP15_MAP_EXECUTABLE; break;
    case 3: privmask |= CP15_MAP_EXECUTABLE; usermask |= CP15_MAP_EXECUTABLE; break;
    case 5: privmask |= CP15_MAP_EXECUTABLE; break;
    case 6: privmask |= CP15_MAP_EXECUTABLE; usermask |= CP15_MAP_EXECUTABLE; break;
    default: Log(LogLevel::Warn, "!! BAD CODERW VALUE %d\n", datarw&0xF);
    }

    if (datacache & 0x1)
    {
        privmask |= CP15_MAP_DCACHEABLE;
        usermask |= CP15_MAP_DCACHEABLE;

        if (datawrite & 0x1)
        {
            privmask |= CP15_MAP_DCACHEWRITEBACK;
            usermask |= CP15_MAP_DCACHEWRITEBACK;
        }
    }

    if (codecache & 0x1)
    {
        privmask |= CP15_MAP_ICACHEABLE;
        usermask |= CP15_MAP_ICACHEABLE;
    }

    Log(
        LogLevel::Debug,
        "PU region %d: %08X-%08X, user=%02X priv=%02X, %08X/%08X\n",
        n,
        start << CP15_MAP_ENTRYSIZE_LOG2,
        end << CP15_MAP_ENTRYSIZE_LOG2,
        usermask,
        privmask,
        PU_DataRW,
        PU_CodeRW
    );

    for (u32 i = start; i < end; i++)
    {
        PU_UserMap[i] = usermask;
        PU_PrivMap[i] = privmask;
    }

    UpdateRegionTimings(start, end);
}

void ARMv5::UpdatePURegions(bool update_all)
{
    if (!(CP15Control & CP15_CR_MPUENABLE))
    {
        // PU disabled

        u8 mask = CP15_MAP_READABLE | CP15_MAP_WRITEABLE | CP15_MAP_EXECUTABLE;
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) mask |= CP15_MAP_DCACHEABLE | CP15_MAP_DCACHEWRITEBACK ;
        if (CP15Control & CP15_CACHE_CR_ICACHEENABLE) mask |= CP15_MAP_ICACHEABLE;

        memset(PU_UserMap, mask, 0x100000);
        memset(PU_PrivMap, mask, 0x100000);

        UpdateRegionTimings(0x00000, 0x100000);
        return;
    }

    if (update_all)
    {
        memset(PU_UserMap, CP15_MAP_NOACCESS, 0x100000);
        memset(PU_PrivMap, CP15_MAP_NOACCESS, 0x100000);
    }

    for (int n = 0; n < CP15_REGION_COUNT; n++)
    {
        UpdatePURegion(n);
    }

    // TODO: this is way unoptimized
    // should be okay unless the game keeps changing shit, tho
    if (update_all) UpdateRegionTimings(0x00000, 0x100000);

    // TODO: throw exception if the region we're running in has become non-executable, I guess
}

void ARMv5::UpdateRegionTimings(u32 addrstart, u32 addrend)
{
    for (u32 i = addrstart; i < addrend; i++)
    {
        u8 pu = PU_Map[i];
        u8* bustimings = NDS.ARM9MemTimings[i >> 2];

        if (pu & CP15_MAP_ICACHEABLE)
        {
            MemTimings[i][0] = 0xFF;//kCodeCacheTiming;
        }
        else
        {
            MemTimings[i][0] = bustimings[2] << NDS.ARM9ClockShift;
        }

        if (pu & CP15_MAP_DCACHEABLE)
        {
            MemTimings[i][1] = kDataCacheTiming;
            MemTimings[i][2] = kDataCacheTiming;
            MemTimings[i][3] = 1;
        }
        else
        {
            MemTimings[i][1] = bustimings[0] << NDS.ARM9ClockShift;
            MemTimings[i][2] = bustimings[2] << NDS.ARM9ClockShift;
            MemTimings[i][3] = bustimings[3] << NDS.ARM9ClockShift;
        }
    }
}


u32 ARMv5::RandomLineIndex()
{
    // lame RNG, but good enough for this purpose
    u32 s = RNGSeed;
    RNGSeed ^= (s*17);
    RNGSeed ^= (s*7);

    return (RNGSeed >> 17) & 0x3;
}

u32 ARMv5::ICacheLookup(const u32 addr)
{
    const u32 tag = (addr & ~(ICACHE_LINELENGTH - 1));
    const u32 id = ((addr >> ICACHE_LINELENGTH_LOG2) & (ICACHE_LINESPERSET-1)) << ICACHE_SETS_LOG2;

    for (int set=0;set<ICACHE_SETS;set++)
    {
        if ((ICacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == (tag | CACHE_FLAG_VALID))
        {
            CodeCycles = 1;
            u32 *cacheLine = (u32 *)&ICache[(id+set) << ICACHE_LINELENGTH_LOG2];
            if (CP15BISTTestStateRegister & CP15_BIST_TR_DISABLE_ICACHE_STREAMING)
            {
                // Disabled ICACHE Streaming:
                // retreive the data from memory, even if the data was cached
                // See arm946e-s Rev 1 technical manual, 2.3.15 "Register 15, test State Register")
                CodeCycles = NDS.ARM9MemTimings[tag >> 14][2]; 
                if (CodeMem.Mem)
                {
                    return *(u32*)&CodeMem.Mem[(addr & CodeMem.Mask) & ~3];
                } else
                {
                    return NDS.ARM9Read32(addr & ~3);
                }     
            }
            return cacheLine[(addr & (ICACHE_LINELENGTH -1)) >> 2];
        }
    }

    // cache miss

    // We do not fill the cacheline if it is disabled in the 
    // BIST test State register (See arm946e-s Rev 1 technical manual, 2.3.15 "Register 15, test State Register")
    if (CP15BISTTestStateRegister & CP15_BIST_TR_DISABLE_ICACHE_LINEFILL)
    {
        CodeCycles = NDS.ARM9MemTimings[tag >> 14][2]; 
        if (CodeMem.Mem)
        {
            return *(u32*)&CodeMem.Mem[(addr & CodeMem.Mask) & ~3];
        } else
        {
            return NDS.ARM9Read32(addr & ~3);
        }        
    }

    u32 line;
#if 0
    // caclulate in which cacheline the data is to be filled
    // The code below is doing the same as the if-less below
    // It increases performance by reducing banches. 
    // The code is kept here for readability.
    // 
    // NOTE: If you need to update either part, you need
    //       to update the other too to keep them in sync!
    //       

    if (CP15Control & CP15_CACHE_CR_ROUNDROBIN)
    {
        line = ICacheCount;
        ICacheCount = (line+1) & (ICACHE_SETS-1);
    }
    else
    {
        line = RandomLineIndex();
    }

    if (ICacheLockDown)
    {
        if (ICacheLockDown & CACHE_LOCKUP_L)
        {
            // load into locked up cache
            // into the selected set
            line = ICacheLockDown & (ICACHE_SETS-1);
        } else
        {
            u8 minSet = ICacheLockDown & (ICACHE_SETS-1);
            line = line | minSet;
        }
    }

#else
    // Do the same as above but instead of using if-else 
    // utilize the && and || operators to skip parts of the operations
    // With the order of comparison we can put the most likely path
    // checked first

    bool doLockDown = (ICacheLockDown & CACHE_LOCKUP_L);
    bool roundRobin = CP15Control & CP15_CACHE_CR_ROUNDROBIN;
    (!roundRobin && (line = RandomLineIndex())) || (roundRobin && (ICacheCount = line = ((ICacheCount+1) & (ICACHE_SETS-1)))) ;
    (!doLockDown && (line = (line | ICacheLockDown & (ICACHE_SETS-1))+id)) || (doLockDown && (line = (ICacheLockDown & (ICACHE_SETS-1))+id));
#endif

    u32* ptr = (u32 *)&ICache[line << ICACHE_LINELENGTH_LOG2];

    if (CodeMem.Mem)
    {
        memcpy(ptr, &CodeMem.Mem[tag & CodeMem.Mask], ICACHE_LINELENGTH);
    }
    else
    {
        for (int i = 0; i < ICACHE_LINELENGTH; i+=sizeof(u32))
            ptr[i >> 2] = NDS.ARM9Read32(tag+i);
    }

    ICacheTags[line] = tag | (line & (ICACHE_SETS-1)) | CACHE_FLAG_VALID;

    // ouch :/
    //printf("cache miss %08X: %d/%d\n", addr, NDS::ARM9MemTimings[addr >> 14][2], NDS::ARM9MemTimings[addr >> 14][3]);
    //                      first N32                                  remaining S32
    CodeCycles = (NDS.ARM9MemTimings[tag >> 14][2] + (NDS.ARM9MemTimings[tag >> 14][3] * ((DCACHE_LINELENGTH / 4) - 1))) << NDS.ARM9ClockShift;
    return ptr[(addr & (ICACHE_LINELENGTH-1)) >> 2];
}

void ARMv5::ICacheInvalidateByAddr(const u32 addr)
{
    const u32 tag = (addr & ~(ICACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
    const u32 id = ((addr >> ICACHE_LINELENGTH_LOG2) & (ICACHE_LINESPERSET-1)) << ICACHE_SETS_LOG2;

    for (int set=0;set<ICACHE_SETS;set++)
    {
        if ((ICacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
        {
            ICacheTags[id+set] &= ~CACHE_FLAG_VALID;  ;
            return;
        }
    }
}

void ARMv5::ICacheInvalidateBySetAndWay(const u8 cacheSet, const u8 cacheLine)
{
    if (cacheSet >= ICACHE_SETS)
        return;
    if (cacheLine >= ICACHE_LINESPERSET)
        return;

    u32 idx = (cacheLine << ICACHE_SETS_LOG2) + cacheSet;
    ICacheTags[idx] &= ~CACHE_FLAG_VALID;  ;
}


void ARMv5::ICacheInvalidateAll()
{
    #pragma GCC ivdep
    for (int i = 0; i < ICACHE_SIZE / ICACHE_LINELENGTH; i++)
        ICacheTags[i] &= ~CACHE_FLAG_VALID;  ;
}

bool ARMv5::IsAddressICachable(const u32 addr) const
{
    return PU_Map[addr >> CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_ICACHEABLE ;
}

u32 ARMv5::DCacheLookup(const u32 addr)
{
    //Log(LogLevel::Debug,"DCache load @ %08x\n", addr);
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) ;
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;

    for (int set=0;set<DCACHE_SETS;set++)
    {
        if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == (tag | CACHE_FLAG_VALID))
        {
            DataCycles = 1;
            u32 *cacheLine = (u32 *)&DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            if (CP15BISTTestStateRegister & CP15_BIST_TR_DISABLE_DCACHE_STREAMING)
            {
                // Disabled DCACHE Streaming:
                // retreive the data from memory, even if the data was cached
                // See arm946e-s Rev 1 technical manual, 2.3.15 "Register 15, test State Register")
                DataCycles = NDS.ARM9MemTimings[tag >> 14][2]; 
                if (addr < ITCMSize)
                {
                    return *(u32*)&ITCM[addr & (ITCMPhysicalSize - 3)];
                } else
                if ((addr & DTCMMask) == DTCMBase)
                {
                    return *(u32*)&DTCM[addr & (DTCMPhysicalSize - 3)];
                } else
                {
                    return BusRead32(addr & ~3);
                }     
            }
            return cacheLine[(addr & (ICACHE_LINELENGTH -1)) >> 2];
        }
    }

    // cache miss

    // We do not fill the cacheline if it is disabled in the 
    // BIST test State register (See arm946e-s Rev 1 technical manual, 2.3.15 "Register 15, test State Register")
    if (CP15BISTTestStateRegister & CP15_BIST_TR_DISABLE_DCACHE_LINEFILL)
    {
        DataCycles = NDS.ARM9MemTimings[tag >> 14][2]; 
        if (addr < ITCMSize)
        {
            return *(u32*)&ITCM[addr & (ITCMPhysicalSize - 3)];
        } else
        if ((addr & DTCMMask) == DTCMBase)
        {
            return *(u32*)&DTCM[addr & (DTCMPhysicalSize - 3)];
        } else
        {
            return BusRead32(addr & ~3);
        }        
    }

    u32 line;
#if 0
    // caclulate in which cacheline the data is to be filled
    // The code below is doing the same as the if-less below
    // It increases performance by reducing banches. 
    // The code is kept here for readability.
    // 
    // NOTE: If you need to update either part, you need
    //       to update the other too to keep them in sync!
    //  

    if (CP15Control & CP15_CACHE_CR_ROUNDROBIN)
    {
        line = DCacheCount;
        DCacheCount = (line+1) & (DCACHE_SETS-1);
    }
    else
    {
        line = DCacheRandom();
    }

    // Update the selected set depending on the DCache LockDown register
    if (DCacheLockDown)
    {
        if (DCacheLockDown & CACHE_LOCKUP_L)
        {
            // load into locked up cache
            // into the selected set
            line = (DCacheLockDown & (DCACHE_SETS-1)) + id;
        } else
        {
            u8 minSet = ICacheLockDown & (DCACHE_SETS-1);
            line = (line | minSet) + id;
        }
    }
#else
    // Do the same as above but instead of using if-else 
    // utilize the && and || operators to skip parts of the operations
    // With the order of comparison we can put the most likely path
    // checked first

    bool doLockDown = (DCacheLockDown & CACHE_LOCKUP_L);
    bool roundRobin = CP15Control & CP15_CACHE_CR_ROUNDROBIN;
    (!roundRobin && (line = RandomLineIndex())) || (roundRobin && (DCacheCount = line = ((DCacheCount+1) & (DCACHE_SETS-1))));
    (!doLockDown && (line = (line | DCacheLockDown & (DCACHE_SETS-1))+id)) || (doLockDown && (line = (DCacheLockDown & (DCACHE_SETS-1))+id));
#endif

    u32* ptr = (u32 *)&DCache[line << DCACHE_LINELENGTH_LOG2];

    //Log(LogLevel::Debug,"DCache miss, load @ %08x\n", tag);
    for (int i = 0; i < DCACHE_LINELENGTH; i+=sizeof(u32))
    {
        if (tag+i < ITCMSize)
        {
            ptr[i >> 2] = *(u32*)&ITCM[(tag+i) & (ITCMPhysicalSize - 1)];
        } else
        if (((tag+i) & DTCMMask) == DTCMBase)
        {
            ptr[i >> 2] = *(u32*)&DTCM[(tag+i) & (DTCMPhysicalSize - 1)];
        } else
        {
            ptr[i >> 2] = BusRead32(tag+i);
        }
        //Log(LogLevel::Debug,"DCache store @ %08x: %08x\n", tag+i, *(u32*)&ptr[i]);
    }

    DCacheTags[line] = tag | (line & (DCACHE_SETS-1)) | CACHE_FLAG_VALID;

    // ouch :/
    //printf("cache miss %08X: %d/%d\n", addr, NDS::ARM9MemTimings[addr >> 14][2], NDS::ARM9MemTimings[addr >> 14][3]);
    //                      first N32                                  remaining S32
    DataCycles = (NDS.ARM9MemTimings[tag >> 14][2] + (NDS.ARM9MemTimings[tag >> 14][3] * ((DCACHE_LINELENGTH / 4) - 1))) << NDS.ARM9ClockShift;
    return ptr[(addr & (DCACHE_LINELENGTH-1)) >> 2];
}

void ARMv5::DCacheWrite32(const u32 addr, const u32 val)
{
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;

    for (int set=0;set<DCACHE_SETS;set++)
    {
        if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
        {
            u32 *cacheLine = (u32 *)&DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            cacheLine[(addr & (ICACHE_LINELENGTH-1)) >> 2] = val;
            DataCycles = 1;
            return;
        }
    }    
}

void ARMv5::DCacheWrite16(const u32 addr, const u16 val)
{
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;

    for (int set=0;set<DCACHE_SETS;set++)
    {
        if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
        {
            u16 *cacheLine = (u16 *)&DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            cacheLine[(addr & (ICACHE_LINELENGTH-1)) >> 1] = val;
            DataCycles = 1;
            return;
        }
    }    
}

void ARMv5::DCacheWrite8(const u32 addr, const u8 val)
{
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;;

    for (int set=0;set<DCACHE_SETS;set++)
    {
        if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
        {
            u8 *cacheLine = &DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            cacheLine[addr & (ICACHE_LINELENGTH-1)] = val;
            DataCycles = 1;
            return;
        }
    }    
}

void ARMv5::DCacheInvalidateByAddr(const u32 addr)
{
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;

    for (int set=0;set<DCACHE_SETS;set++)
    {
        if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
        {
            //Log(LogLevel::Debug,"DCache invalidated %08lx\n", addr & ~(ICACHE_LINELENGTH-1));
            DCacheTags[id+set] &= ~CACHE_FLAG_VALID;  ;     
            return;
        }
    }
}

void ARMv5::DCacheInvalidateBySetAndWay(const u8 cacheSet, const u8 cacheLine)
{
    if (cacheSet >= DCACHE_SETS)
        return;
    if (cacheLine >= DCACHE_LINESPERSET)
        return;

    u32 idx = (cacheLine << DCACHE_SETS_LOG2) + cacheSet;
    DCacheTags[idx] &= ~CACHE_FLAG_VALID;  ;
}


void ARMv5::DCacheInvalidateAll()
{
    #pragma GCC ivdep
    for (int i = 0; i < DCACHE_SIZE / DCACHE_LINELENGTH; i++)
        DCacheTags[i] &= ~CACHE_FLAG_VALID;  ;
}

void ARMv5::DCacheClearAll()
{
    // TODO: right now any write to cached data goes straight to the 
    // underlying memory and invalidates the cache line.
}

void ARMv5::DCacheClearByAddr(const u32 addr)
{
    // TODO: right now any write to cached data goes straight to the 
    // underlying memory and invalidates the cache line.
}

void ARMv5::DCacheClearByASetAndWay(const u8 cacheSet, const u8 cacheLine)
{
    // TODO: right now any write to cached data goes straight to the 
    // underlying memory and invalidates the cache line.
}

bool ARMv5::IsAddressDCachable(const u32 addr) const
{
    return PU_Map[addr >> CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_DCACHEABLE ;
}

void ARMv5::CP15Write(u32 id, u32 val)
{
    //if(id!=0x704)printf("CP15 write op %03X %08X %08X\n", id, val, R[15]);

    switch (id & 0xFFF)
    {
    case 0x100:
        {
            u32 old = CP15Control;
            CP15Control = (CP15Control & ~CP15_CR_CHANGEABLE_MASK) | (val & CP15_CR_CHANGEABLE_MASK);
            //Log(LogLevel::Debug, "CP15Control = %08X (%08X->%08X)\n", CP15Control, old, val);
            UpdateDTCMSetting();
            UpdateITCMSetting();
            u32 changedBits = old ^ CP15Control;
            if (changedBits & (CP15_CR_MPUENABLE | CP15_CACHE_CR_ICACHEENABLE| CP15_CACHE_CR_DCACHEENABLE))
            {
                UpdatePURegions(changedBits & CP15_CR_MPUENABLE);
            }
            if (val & CP15_CR_BIGENDIAN) Log(LogLevel::Warn, "!!!! ARM9 BIG ENDIAN MODE. VERY BAD. SHIT GONNA ASPLODE NOW\n");
            if (val & CP15_CR_HIGHEXCEPTIONBASE) ExceptionBase = CP15_EXCEPTIONBASE_HIGH;
            else                                 ExceptionBase = CP15_EXCEPTIONBASE_LOW;
        }
        return;


    case 0x200: // data cacheable
        {
            u32 diff = PU_DataCacheable ^ val;
            PU_DataCacheable = val;
            #if 0
                // This code just updates the PU_Map entries of the given region
                // this works fine, if the regions do not overlap 
                // If overlapping and the least priority region cachable bit
                // would change, this results in wrong map entries. On HW the changed 
                // cachable bit would not be applied because of a higher priority
                // region overwriting them.
                // 
                // Writing to the cachable bits is sparse, so we 
                // should just take the long but correct update via all regions
                // so the permission priority is correct

                for (u32 i = 0; i < CP15_REGION_COUNT; i++)
                {
                    if (diff & (1<<i)) UpdatePURegion(i);
                }
            #else
                UpdatePURegions(true);
            #endif
        }
        return;

    case 0x201: // code cacheable
        {
            u32 diff = PU_CodeCacheable ^ val;
            PU_CodeCacheable = val;
            #if 0
                // This code just updates the PU_Map entries of the given region
                // this works fine, if the regions do not overlap 
                // If overlapping and the least priority region cachable bit
                // would change, this results in wrong map entries. On HW the changed 
                // cachable bit would not be applied because of a higher priority
                // region overwriting them.
                // 
                // Writing to the cachable bits is sparse, so we 
                // should just take the long but correct update via all regions
                // so the permission priority is correct

                for (u32 i = 0; i < CP15_REGION_COUNT; i++)
                {
                    if (diff & (1<<i)) UpdatePURegion(i);
                }
            #else
                UpdatePURegions(true);
            #endif
        }
        return;


    case 0x300: // data cache write-buffer
        {
            u32 diff = PU_DataCacheWrite ^ val;
            PU_DataCacheWrite = val;
            #if 0
                // This code just updates the PU_Map entries of the given region
                // this works fine, if the regions do not overlap 
                // If overlapping and the least priority region write buffer 
                // would change, this results in wrong map entries. On HW the changed 
                // write buffer would not be applied because of a higher priority
                // region overwriting them.
                // 
                // Writing to the write buffer bits is sparse, so we 
                // should just take the long but correct update via all regions
                // so the permission priority is correct

                for (u32 i = 0; i < CP15_REGION_COUNT; i++)
                {
                    if (diff & (1<<i)) UpdatePURegion(i);
                }
            #else
                UpdatePURegions(true);
            #endif
        }
        return;


    case 0x500: // legacy data permissions
        {
            u32 old = PU_DataRW;
            PU_DataRW = 0;
            #pragma GCC ivdep
            #pragma GCC unroll 8
            for (int i=0;i<CP15_REGION_COUNT;i++)
                PU_DataRW |= (val  >> (i * 2) & 3) << (i * CP15_REGIONACCESS_BITS_PER_REGION);
            
            #if 0
                // This code just updates the PU_Map entries of the given region
                // this works fine, if the regions do not overlap 
                // If overlapping and the least priority region access permission
                // would change, this results in wrong map entries. On HW the changed 
                // access permissions would not be applied because of a higher priority
                // region overwriting them.
                // 
                // Writing to the data permission bits is sparse, so we 
                // should just take the long but correct update via all regions
                // so the permission priority is correct

                u32 diff = old ^ PU_DataRW;            
                for (u32 i = 0; i < CP15_REGION_COUNT; i++)
                {
                    if (diff & (CP15_REGIONACCESS_REGIONMASK<<(i*CP15_REGIONACCESS_BITS_PER_REGION))) UpdatePURegion(i);
                }
            #else
                UpdatePURegions(true);
            #endif
        }
        return;

    case 0x501: // legacy code permissions
        {
            u32 old = PU_CodeRW;
            PU_CodeRW = 0;
            #pragma GCC ivdep
            #pragma GCC unroll 8
            for (int i=0;i<CP15_REGION_COUNT;i++)
                PU_CodeRW |= (val  >> (i * 2) & 3) << (i * CP15_REGIONACCESS_BITS_PER_REGION);

            #if 0
                // This code just updates the PU_Map entries of the given region
                // this works fine, if the regions do not overlap 
                // If overlapping and the least priority region access permission
                // would change, this results in wrong map entries, because it
                // would on HW be overridden by the higher priority region
                // 
                // Writing to the data permission bits is sparse, so we 
                // should just take the long but correct update via all regions
                // so the permission priority is correct

                u32 diff = old ^ PU_CodeRW;
                for (u32 i = 0; i < CP15_REGION_COUNT; i++)
                {
                    if (diff & (CP15_REGIONACCESS_REGIONMASK<<(i*CP15_REGIONACCESS_BITS_PER_REGION))) UpdatePURegion(i);
                }
            #else
                UpdatePURegions(true);
            #endif
        }
        return;

    case 0x502: // data permissions
        {
            u32 diff = PU_DataRW ^ val;
            PU_DataRW = val;
            #if 0
                // This code just updates the PU_Map entries of the given region
                // this works fine, if the regions do not overlap 
                // If overlapping and the least priority region access permission
                // would change, this results in wrong map entries, because it
                // would on HW be overridden by the higher priority region
                // 
                // Writing to the data permission bits is sparse, so we 
                // should just take the long but correct update via all regions
                // so the permission priority is correct
                for (u32 i = 0; i < CP15_REGION_COUNT; i++)
                {
                    if (diff & (CP15_REGIONACCESS_REGIONMASK<<(i*CP15_REGIONACCESS_BITS_PER_REGION))) UpdatePURegion(i);
                }
            #else
                UpdatePURegions(true);
            #endif
        }
        return;

    case 0x503: // code permissions
        {
            u32 diff = PU_CodeRW ^ val;
            PU_CodeRW = val;
            #if 0
                // This code just updates the PU_Map entries of the given region
                // this works fine, if the regions do not overlap 
                // If overlapping and the least priority region access permission
                // would change, this results in wrong map entries, because it
                // would on HW be overridden by the higher priority region
                // 
                // Writing to the data permission bits is sparse, so we 
                // should just take the long but correct update via all regions
                // so the permission priority is correct
                for (u32 i = 0; i < CP15_REGION_COUNT; i++)
                {
                    if (diff & (CP15_REGIONACCESS_REGIONMASK<<(i*CP15_REGIONACCESS_BITS_PER_REGION))) UpdatePURegion(i);
                }
            #else
                UpdatePURegions(true);
            #endif
        }
        return;


    case 0x600:
    case 0x601:
    case 0x610:
    case 0x611:
    case 0x620:
    case 0x621:
    case 0x630:
    case 0x631:
    case 0x640:
    case 0x641:
    case 0x650:
    case 0x651:
    case 0x660:
    case 0x661:
    case 0x670:
    case 0x671:
        PU_Region[(id >> CP15_REGIONACCESS_BITS_PER_REGION) & CP15_REGIONACCESS_REGIONMASK] = val;

        Log(LogLevel::Debug,
                 "PU: region %d = %08X : %s, %08X-%08X\n",
                 (id >> CP15_REGIONACCESS_BITS_PER_REGION) & CP15_REGIONACCESS_REGIONMASK,
                 val,
                 val & 1 ? "enabled" : "disabled",
                 val & CP15_REGION_BASE_MASK,
                 (val & CP15_REGION_BASE_MASK) + (2 << ((val & CP15_REGION_SIZE_MASK) >> 1))
        );
        // TODO: smarter region update for this?
        UpdatePURegions(true);
        return;


    case 0x704:
    case 0x782:
        Halt(1);
        return;


    case 0x750:
        // Can be executed in user and priv mode
        ICacheInvalidateAll();
        return;
    case 0x751:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        ICacheInvalidateByAddr(val);
        //Halt(255);
        return;
    case 0x752:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        } else
        {
            // Cache invalidat by line number and set number
            u8 cacheSet = val >> (32 - ICACHE_SETS_LOG2) & (ICACHE_SETS -1);
            u8 cacheLine = (val >> ICACHE_LINELENGTH_LOG2) & (ICACHE_LINESPERSET -1);
            ICacheInvalidateBySetAndWay(cacheSet, cacheLine);
        }
        //Halt(255);
        return;


    case 0x760:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        DCacheInvalidateAll();
        //printf("inval data cache %08X\n", val);
        return;
    case 0x761:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        DCacheInvalidateByAddr(val);
        //printf("inval data cache SI\n");
        return;
    case 0x762:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        } else
        {
            // Cache invalidat by line number and set number
            u8 cacheSet = val >> (32 - DCACHE_SETS_LOG2) & (DCACHE_SETS -1);
            u8 cacheLine = (val >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET -1);
            DCacheInvalidateBySetAndWay(cacheSet, cacheLine);
        }
        return;

    case 0x770:
        // invalidate both caches
        // can be called from user and privileged
        ICacheInvalidateAll();
        DCacheInvalidateAll();
        break;

    case 0x7A0:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        //Log(LogLevel::Debug,"clean data cache\n");
        DCacheClearAll();
        return;
    case 0x7A1:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        //Log(LogLevel::Debug,"clean data cache MVA\n");
        DCacheClearByAddr(val);
        return;
    case 0x7A2:
        //Log(LogLevel::Debug,"clean data cache SET/WAY\n");
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        } else
        {
            // Cache invalidat by line number and set number
            u8 cacheSet = val >> (32 - DCACHE_SETS_LOG2) & (DCACHE_SETS -1);
            u8 cacheLine = (val >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET -1);
            DCacheClearByASetAndWay(cacheSet, cacheLine);
        }
        return;
    case 0x7A3:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        // Test and clean (optional)
        // Is not present on the NDS/DSi
        return;   
    case 0x7A4:
        // Can be used in user and privileged mode
        // Drain Write Buffer: Stall until all write back completed
        // TODO when write back was implemented instead of write through
        return;

    case 0x7D1:
        Log(LogLevel::Debug,"Prefetch instruction cache MVA\n");
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        // we force a fill by looking up the value from cache
        // if it wasn't cached yet, it will be loaded into cache
        ICacheLookup(val & ~0x03);
        break;

    case 0x7E0:
        //Log(LogLevel::Debug,"clean & invalidate data cache\n");
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        DCacheClearAll();
        DCacheInvalidateAll();
        return;
    case 0x7E1:
        //Log(LogLevel::Debug,"clean & invalidate data cache MVA\n");
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        DCacheClearByAddr(val);
        DCacheInvalidateByAddr(val);
        return;
    case 0x7E2:
        //Log(LogLevel::Debug,"clean & invalidate data cache SET/WAY\n");
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        } else
        {
            // Cache invalidat by line number and set number
            u8 cacheSet = val >> (32 - DCACHE_SETS_LOG2) & (DCACHE_SETS -1);
            u8 cacheLine = (val >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET -1);
            DCacheClearByASetAndWay(cacheSet, cacheLine);
            DCacheInvalidateBySetAndWay(cacheSet, cacheLine);
        }
        return;

    case 0x900:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        // Cache Lockdown - Format B
        //    Bit 31: Lock bit
        //    Bit 0..Way-1: locked ways
        //      The Cache is 4 way associative
        // But all bits are r/w
        DCacheLockDown = val ;
        Log(LogLevel::Debug,"ICacheLockDown\n");
        return;
    case 0x901:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        // Cache Lockdown - Format B
        //    Bit 31: Lock bit
        //    Bit 0..Way-1: locked ways
        //      The Cache is 4 way associative
        // But all bits are r/w
        ICacheLockDown = val;
        Log(LogLevel::Debug,"ICacheLockDown\n");
        return;

    case 0x910:
        DTCMSetting = val & 0xFFFFF03E;
        UpdateDTCMSetting();
        return;

    case 0x911:
        ITCMSetting = val & 0x0000003E;
        UpdateITCMSetting();
        return;

    case 0xD01:
    case 0xD11:
        CP15TraceProcessId = val;
        return;

    case 0xF00:
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        } else
        {
            if (((id >> 12) & 0x0f) == 0x03)
                CacheDebugRegisterIndex = val;
            else if (((id >> 12) & 0x0f) == 0x00)
                CP15BISTTestStateRegister = val;
            else
            {
                return ARMInterpreter::A_UNK(this);                
            }

        }
        return;

    case 0xF10:
        // instruction cache Tag register
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        } else
        {
            uint8_t segment = (CacheDebugRegisterIndex >> (32-ICACHE_SETS_LOG2)) & (ICACHE_SETS-1);
            uint8_t wordAddress = (CacheDebugRegisterIndex & (ICACHE_LINELENGTH-1)) >> 2;
            uint8_t index = (CacheDebugRegisterIndex >> ICACHE_LINELENGTH_LOG2) & (ICACHE_LINESPERSET-1);
            ICacheTags[(index << ICACHE_SETS_LOG2) + segment] = val;
        }

    case 0xF20:
        // data cache Tag register
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        } else
        {
            uint8_t segment = (CacheDebugRegisterIndex >> (32-DCACHE_SETS_LOG2)) & (DCACHE_SETS-1);
            uint8_t wordAddress = (CacheDebugRegisterIndex & (DCACHE_LINELENGTH-1)) >> 2;
            uint8_t index = (CacheDebugRegisterIndex >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1);
            DCacheTags[(index << DCACHE_SETS_LOG2) + segment] = val;
        }


    case 0xF30:
        //printf("cache debug instruction cache %08X\n", val);
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        } else
        {
            uint8_t segment = (CacheDebugRegisterIndex >> (32-ICACHE_SETS_LOG2)) & (ICACHE_SETS-1);
            uint8_t wordAddress = (CacheDebugRegisterIndex & (ICACHE_LINELENGTH-1)) >> 2;
            uint8_t index = (CacheDebugRegisterIndex >> ICACHE_LINELENGTH_LOG2) & (ICACHE_LINESPERSET-1);
            *(u32 *)&ICache[(((index << ICACHE_SETS_LOG2) + segment) << ICACHE_LINELENGTH_LOG2) + wordAddress*4] = val;            
        }
        return;

    case 0xF40:
        //printf("cache debug data cache %08X\n", val);
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        } else
        {
            uint8_t segment = (CacheDebugRegisterIndex >> (32-DCACHE_SETS_LOG2)) & (DCACHE_SETS-1);
            uint8_t wordAddress = (CacheDebugRegisterIndex & (DCACHE_LINELENGTH-1)) >> 2;
            uint8_t index = (CacheDebugRegisterIndex >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1);
            *(u32 *)&DCache[((index << DCACHE_SETS_LOG2) + segment) << DCACHE_LINELENGTH_LOG2 + wordAddress*4] = val;            
        }
        return;

    }

    Log(LogLevel::Debug, "unknown CP15 write op %04X %08X\n", id, val);
}

u32 ARMv5::CP15Read(u32 id) const
{
    //printf("CP15 read op %03X %08X\n", id, NDS::ARM9->R[15]);

    switch (id & 0xFFF)
    {
    case 0x000: // CPU ID
    case 0x003:
    case 0x004:
    case 0x005:
    case 0x006:
    case 0x007:
        return CP15_MAINID_IMPLEMENTOR_ARM | CP15_MAINID_VARIANT_0 | CP15_MAINID_ARCH_v5TE | CP15_MAINID_IMPLEMENTATION_946 | CP15_MAINID_REVISION_1;

    case 0x001: // cache type
        return CACHE_TR_LOCKDOWN_TYPE_B | CACHE_TR_NONUNIFIED
               | (DCACHE_LINELENGTH_ENCODED << 12) | (DCACHE_SETS_LOG2 << 15) | ((DCACHE_SIZE_LOG2 - 9) << 18)
               | (ICACHE_LINELENGTH_ENCODED << 0) | (ICACHE_SETS_LOG2 << 3) | ((ICACHE_SIZE_LOG2 - 9) << 6);

    case 0x002: // TCM size
        return (6 << 6) | (5 << 18);


    case 0x100: // control reg
        return CP15Control;


    case 0x200:
        return PU_DataCacheable;
    case 0x201:
        return PU_CodeCacheable;
    case 0x300:
        return PU_DataCacheWrite;


    case 0x500:
        {
            // this format  has 2 bits per region, but we store 4 per region
            // so we reduce and consoldate the bits
            // 0x502 returns all 4 bits per region
            u32 ret = 0;
            #pragma GCC ivdep
            #pragma GCC unroll 8
            for (int i=0;i<CP15_REGION_COUNT;i++)
                ret |= (PU_DataRW  >> (i * CP15_REGIONACCESS_BITS_PER_REGION) & CP15_REGIONACCESS_REGIONMASK) << (i*2);
            return ret;
        }
    case 0x501:
        {
            // this format  has 2 bits per region, but we store 4 per region
            // so we reduce and consoldate the bits
            // 0x503 returns all 4 bits per region
            u32 ret = 0;
            #pragma GCC unroll 8
            for (int i=0;i<CP15_REGION_COUNT;i++)
                ret |= (PU_CodeRW  >> (i * CP15_REGIONACCESS_BITS_PER_REGION) & CP15_REGIONACCESS_REGIONMASK) << (i*2);
            return ret;
        }
    case 0x502:
        return PU_DataRW;
    case 0x503:
        return PU_CodeRW;


    case 0x600:
    case 0x601:
    case 0x610:
    case 0x611:
    case 0x620:
    case 0x621:
    case 0x630:
    case 0x631:
    case 0x640:
    case 0x641:
    case 0x650:
    case 0x651:
    case 0x660:
    case 0x661:
    case 0x670:
    case 0x671:
        return PU_Region[(id >> CP15_REGIONACCESS_BITS_PER_REGION) & 0xF];

    case 0x7A6:
        // read Cache Dirty Bit (optional)
        // it is not present on the NDS/DSi
        return 0;

    case 0x900:
        if (PU_Map != PU_PrivMap)
        {            
            return 0;
        } else
            return DCacheLockDown;
    case 0x901:
        if (PU_Map != PU_PrivMap)
        {            
            return 0;
        } else
            return ICacheLockDown;

    case 0x910:
        return DTCMSetting;
    case 0x911:
        return ITCMSetting;

    case 0xD01: // See arm946E-S Rev 1 technical Reference Manual, Chapter 2.3.13 */ 
    case 0xD11: // backwards compatible read/write of the same register
        return CP15TraceProcessId;

    case 0xF00:
        if (PU_Map != PU_PrivMap)
        {            
            return 0;
        } else
        {
            if (((id >> 12) & 0x0f) == 0x03)
                return CacheDebugRegisterIndex;
            if (((id >> 12) & 0x0f) == 0x00)
                return CP15BISTTestStateRegister;
        }
    case 0xF10:
        // instruction cache Tag register
        if (PU_Map != PU_PrivMap)
        {            
            return 0;
        } else
        {
            uint8_t segment = (CacheDebugRegisterIndex >> (32-ICACHE_SETS_LOG2)) & (ICACHE_SETS-1);
            uint8_t wordAddress = (CacheDebugRegisterIndex & (ICACHE_LINELENGTH-1)) >> 2;
            uint8_t index = (CacheDebugRegisterIndex >> ICACHE_LINELENGTH_LOG2) & (ICACHE_LINESPERSET-1);
            Log(LogLevel::Debug, "Read ICache Tag %08lx -> %08lx\n", CacheDebugRegisterIndex, ICacheTags[(index << ICACHE_SETS_LOG2) + segment]);
            return ICacheTags[(index << ICACHE_SETS_LOG2) + segment];
        }
    case 0xF20:
        // data cache Tag register
        if (PU_Map != PU_PrivMap)
        {            
            return 0;
        } else
        {
            uint8_t segment = (CacheDebugRegisterIndex >> (32-DCACHE_SETS_LOG2)) & (DCACHE_SETS-1);
            uint8_t wordAddress = (CacheDebugRegisterIndex & (DCACHE_LINELENGTH-1)) >> 2;
            uint8_t index = (CacheDebugRegisterIndex >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1);
            Log(LogLevel::Debug, "Read DCache Tag %08lx (%u, %02x, %u) -> %08lx\n", CacheDebugRegisterIndex, segment, index, wordAddress, DCacheTags[(index << DCACHE_SETS_LOG2) + segment]);
            return DCacheTags[(index << DCACHE_SETS_LOG2) + segment];
        }
    case 0xF30:
        if (PU_Map != PU_PrivMap)
        {            
            return 0;
        } else
        {
            uint8_t segment = (CacheDebugRegisterIndex >> (32-ICACHE_SETS_LOG2)) & (ICACHE_SETS-1);
            uint8_t wordAddress = (CacheDebugRegisterIndex & (ICACHE_LINELENGTH-1)) >> 2;
            uint8_t index = (CacheDebugRegisterIndex >> ICACHE_LINELENGTH_LOG2) & (ICACHE_LINESPERSET-1);
            return *(u32 *)&ICache[(((index << ICACHE_SETS_LOG2) + segment) << ICACHE_LINELENGTH_LOG2) + wordAddress*4];
        }
    case 0xF40:
        {
            uint8_t segment = (CacheDebugRegisterIndex >> (32-DCACHE_SETS_LOG2)) & (DCACHE_SETS-1);
            uint8_t wordAddress = (CacheDebugRegisterIndex & (DCACHE_LINELENGTH-1)) >> 2;
            uint8_t index = (CacheDebugRegisterIndex >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1);
            return *(u32 *)&DCache[(((index << DCACHE_SETS_LOG2) + segment) << DCACHE_LINELENGTH_LOG2) + wordAddress*4];
        }
    }

    Log(LogLevel::Debug, "unknown CP15 read op %04X\n", id);
    return 0;
}


// TCM are handled here.
// TODO: later on, handle PU

u32 ARMv5::CodeRead32(const u32 addr, bool const branch)
{

#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif    
    {
        if (CP15Control & CP15_CACHE_CR_ICACHEENABLE) 
        {
            if (IsAddressICachable(addr))
            {
                return ICacheLookup(addr);
            }
        }
    } 

    if (addr < ITCMSize)
    {
        CodeCycles = 1;
        return *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)];
    }

    CodeCycles = RegionCodeCycles;

    if (CodeCycles == 0xFF) // cached memory. hax
    {
        if (branch || !(addr & (ICACHE_LINELENGTH-1)))
            CodeCycles = kCodeCacheTiming;//ICacheLookup(addr);
        else
            CodeCycles = 1;
    }

    if (CodeMem.Mem) return *(u32*)&CodeMem.Mem[addr & CodeMem.Mask];

    return BusRead32(addr);
}


void ARMv5::DataRead8(const u32 addr, u32* val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_READABLE))
    {
        Log(LogLevel::Debug, "data8 abort @ %08lx\n", addr);
        DataAbort();
        return;
    }

    DataRegion = addr;

#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif  
    {
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) 
        {
            if (IsAddressDCachable(addr))
            {
                *val = (DCacheLookup(addr) >> (8 * (addr & 3))) & 0xff;
                return;
            }
        }
    }

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *val = *(u8*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles = 1;
        *val = *(u8*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        return;
    }

    *val = BusRead8(addr);
    DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S16];
}

void ARMv5::DataRead16(const u32 addr, u32* val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_READABLE))
    {
        Log(LogLevel::Debug, "data16 abort @ %08lx\n", addr);
        DataAbort();
        return;
    }

    DataRegion = addr;

#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif  
    {
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) 
        {
            if (IsAddressDCachable(addr))
            {
                *val = (DCacheLookup(addr) >> (8* (addr & 2))) & 0xffff;
                return;
            }
        }
    }

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *val = *(u16*)&ITCM[addr & (ITCMPhysicalSize - 2)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles = 1;
        *val = *(u16*)&DTCM[addr & (DTCMPhysicalSize - 2)];
        return;
    }

    *val = BusRead16(addr & ~1);
    DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S16];
}

void ARMv5::DataRead32(const u32 addr, u32* val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_READABLE))
    {
        Log(LogLevel::Debug, "data32 abort @ %08lx\n", addr);
        DataAbort();
        return;
    }

    DataRegion = addr;

#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif  
    {
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) 
        {
            if (IsAddressDCachable(addr))
            {
                *val = DCacheLookup(addr);
                return;
            }
        }
    }

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *val = *(u32*)&ITCM[addr & (ITCMPhysicalSize - 3)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles = 1;
        *val = *(u32*)&DTCM[addr & (DTCMPhysicalSize - 3)];
        return;
    }

    *val = BusRead32(addr & ~0x03);
    DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_N32];
}

void ARMv5::DataRead32S(const u32 addr, u32* val)
{
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif  
    {
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) 
        {
            if (IsAddressDCachable(addr))
            {
                *val = DCacheLookup(addr);
                return;
            }
        }
    }

    if (addr < ITCMSize)
    {
        DataCycles += 1;
        *val = *(u32*)&ITCM[addr & (ITCMPhysicalSize - 3)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles += 1;
        *val = *(u32*)&DTCM[addr & (DTCMPhysicalSize - 3)];
        return;
    }

    *val = BusRead32(addr & ~0x03);
    DataCycles += MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S32];
}

void ARMv5::DataWrite8(const u32 addr, const u8 val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_WRITEABLE))
    {
        DataAbort();
        return;
    }

#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif  
    {
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) 
        {
            if (IsAddressDCachable(addr))
            {
                DCacheWrite8(addr, val);
            }
        }
    }

    DataRegion = addr;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *(u8*)&ITCM[addr & (ITCMPhysicalSize - 1)] = val;
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles = 1;
        *(u8*)&DTCM[addr & (DTCMPhysicalSize - 1)] = val;
        return;
    }

    BusWrite8(addr, val);
    DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S16];
}

void ARMv5::DataWrite16(const u32 addr, const u16 val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_WRITEABLE))
    {
        DataAbort();
        return;
    }

#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif  
    {
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) 
        {
            if (IsAddressDCachable(addr))
            {
                DCacheWrite16(addr, val);
            }
        }
    }

    DataRegion = addr;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *(u16*)&ITCM[addr & (ITCMPhysicalSize - 2)] = val;
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles = 1;
        *(u16*)&DTCM[addr & (DTCMPhysicalSize - 2)] = val;
        return;
    }

    BusWrite16(addr & ~1, val);
    DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S16];
}

void ARMv5::DataWrite32(const u32 addr, const u32 val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_WRITEABLE))
    {
        DataAbort();
        return;
    }

#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif  
    {
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) 
        {
            if (IsAddressDCachable(addr))
            {
                DCacheWrite32(addr, val);
            }
        }
    }

    DataRegion = addr;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *(u32*)&ITCM[addr & (ITCMPhysicalSize - 3)] = val;
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles = 1;
        *(u32*)&DTCM[addr & (DTCMPhysicalSize - 3)] = val;
        return;
    }

    BusWrite32(addr & ~3, val);
    DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_N32];
}

void ARMv5::DataWrite32S(const u32 addr, const u32 val)
{
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif  
    {
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) 
        {
            if (IsAddressDCachable(addr))
            {
                DCacheWrite32(addr, val);
            }
        }
    }

    if (addr < ITCMSize)
    {
        DataCycles += 1;
        *(u32*)&ITCM[addr & (ITCMPhysicalSize - 3)] = val;
#ifdef JIT_ENABLED
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
#endif
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles += 1;
        *(u32*)&DTCM[addr & (DTCMPhysicalSize - 3)] = val;
        return;
    }

    BusWrite32(addr & ~3, val);
    DataCycles += MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S32];
}

void ARMv5::GetCodeMemRegion(u32 addr, MemRegion* region)
{
    NDS.ARM9GetMemRegion(addr, false, &CodeMem);
}

}
