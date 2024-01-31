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

    memset(ICache, 0, ICACHE_SIZE);
    ICacheInvalidateAll();
    ICacheCount = 0;

    memset(DCache, 0, DCACHE_SIZE);
    DCacheInvalidateAll();
    DCacheCount = 0;

    // make sure that both half words are not the same otherwise the random of the DCache set selection only produces
    // '00' and '11'
    DCacheLFSRStates = 0xDEADBEEF;  

    PU_CodeCacheable = 0;
    PU_DataCacheable = 0;
    PU_DataCacheWrite = 0;

    PU_CodeRW = 0;
    PU_DataRW = 0;

    memset(PU_Region, 0, 8*sizeof(u32));
    UpdatePURegions(true);

    CurICacheLine = NULL;
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
    file->Var32(&DCacheLFSRStates);

    file->Var32(&DCacheLockDown);
    file->Var32(&ICacheLockDown);
    file->Var32(&CacheDebugRegisterIndex);

    file->Var32(&PU_CodeCacheable);
    file->Var32(&PU_DataCacheable);
    file->Var32(&PU_DataCacheWrite);

    file->Var32(&PU_CodeRW);
    file->Var32(&PU_DataRW);

    file->VarArray(PU_Region, 8*sizeof(u32));

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

    u32 coderw = (PU_CodeRW >> (4*n)) & 0xF;
    u32 datarw = (PU_DataRW >> (4*n)) & 0xF;

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

    u32 start = rgn >> 12;
    u32 sz = 2 << ((rgn >> 1) & 0x1F);
    u32 end = start + (sz >> 12);
    // TODO: check alignment of start

    u8 usermask = 0;
    u8 privmask = 0;

    switch (datarw)
    {
    case 0: break;
    case 1: privmask |= 0x03; break;
    case 2: privmask |= 0x03; usermask |= 0x01; break;
    case 3: privmask |= 0x03; usermask |= 0x03; break;
    case 5: privmask |= 0x01; break;
    case 6: privmask |= 0x01; usermask |= 0x01; break;
    default: Log(LogLevel::Warn, "!! BAD DATARW VALUE %d\n", datarw&0xF);
    }

    switch (coderw)
    {
    case 0: break;
    case 1: privmask |= 0x04; break;
    case 2: privmask |= 0x04; usermask |= 0x04; break;
    case 3: privmask |= 0x04; usermask |= 0x04; break;
    case 5: privmask |= 0x04; break;
    case 6: privmask |= 0x04; usermask |= 0x04; break;
    default: Log(LogLevel::Warn, "!! BAD CODERW VALUE %d\n", datarw&0xF);
    }

    if (datacache & 0x1)
    {
        privmask |= 0x10;
        usermask |= 0x10;

        if (datawrite & 0x1)
        {
            privmask |= 0x20;
            usermask |= 0x20;
        }
    }

    if (codecache & 0x1)
    {
        privmask |= 0x40;
        usermask |= 0x40;
    }

    Log(
        LogLevel::Debug,
        "PU region %d: %08X-%08X, user=%02X priv=%02X, %08X/%08X\n",
        n,
        start << 12,
        end << 12,
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

        u8 mask = 0x07;
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) mask |= 0x30;
        if (CP15Control & CP15_CACHE_CR_ICACHEENABLE) mask |= 0x40;

        memset(PU_UserMap, mask, 0x100000);
        memset(PU_PrivMap, mask, 0x100000);

        UpdateRegionTimings(0x00000, 0x100000);
        return;
    }

    if (update_all)
    {
        memset(PU_UserMap, 0, 0x100000);
        memset(PU_PrivMap, 0, 0x100000);
    }

    for (int n = 0; n < 8; n++)
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

        if (pu & 0x40)
        {
            MemTimings[i][0] = 0xFF;//kCodeCacheTiming;
        }
        else
        {
            MemTimings[i][0] = bustimings[2] << NDS.ARM9ClockShift;
        }

        if (pu & 0x10)
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
        if ((ICacheTags[id+set] & ~0x0F) == (tag | CACHE_FLAG_VALID))
        {
            CodeCycles = 1;
            u32 *cacheLine = (u32 *)&ICache[(id+set) << ICACHE_LINELENGTH_LOG2];
            return cacheLine[(addr & (ICACHE_LINELENGTH -1)) >> 2];
        }
    }

    // cache miss
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
        if ((ICacheTags[id+set] & ~0x0F) == tag)
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
    for (int i = 0; i < ICACHE_SIZE / ICACHE_LINELENGTH; i++)
        ICacheTags[i] &= ~CACHE_FLAG_VALID;  ;
}

bool ARMv5::IsAddressICachable(const u32 addr) const
{
    return PU_Map[addr >> 12] & 0x40 ;
}

u32 ARMv5::DCacheLookup(const u32 addr)
{
    //Log(LogLevel::Debug,"DCache load @ %08x\n", addr);
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) ;
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;

    for (int set=0;set<DCACHE_SETS;set++)
    {
        if ((DCacheTags[id+set] & ~0x0F) == (tag | CACHE_FLAG_VALID))
        {
            DataCycles = 1;
            u32 *cacheLine = (u32 *)&DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            return cacheLine[(addr & (ICACHE_LINELENGTH -1)) >> 2];
        }
    }

    // cache miss
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
        if ((DCacheTags[id+set] & ~0x0F) == tag)
        {
            u32 *cacheLine = (u32 *)&DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            cacheLine[(addr & (ICACHE_LINELENGTH-1)) >> 2] = val;
            DataCycles = 1;

            //Log(LogLevel::Debug,"DCache write32 hit @ %08x -> %08lx\n", addr, ((u32 *)CurDCacheLine)[(addr & (DCACHE_LINELENGTH-1)) >> 2]);
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
        if ((DCacheTags[id+set] & ~0x0F) == tag)
        {
            u16 *cacheLine = (u16 *)&DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            cacheLine[(addr & (ICACHE_LINELENGTH-1)) >> 1] = val;
            DataCycles = 1;

            //Log(LogLevel::Debug,"DCache write16 hit @ %08x -> %04x\n", addr, ((u16 *)CurDCacheLine)[(addr & (DCACHE_LINELENGTH-1)) >> 2]);
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
        if ((DCacheTags[id+set] & ~0x0F) == tag)
        {
            u8 *cacheLine = &DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            cacheLine[addr & (ICACHE_LINELENGTH-1)] = val;
            DataCycles = 1;

            //Log(LogLevel::Debug,"DCache write hit8 @ %08x -> %02x\n", addr, ((u8 *)CurDCacheLine)[(addr & (DCACHE_LINELENGTH-1)) >> 2]);
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
        if ((DCacheTags[id+set] & ~0x0Ful) == tag)
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
    return PU_Map[addr >> 12] & 0x10 ;
}

void ARMv5::CP15Write(u32 id, u32 val)
{
    //if(id!=0x704)printf("CP15 write op %03X %08X %08X\n", id, val, R[15]);

    switch (id)
    {
    case 0x100:
        {
            u32 old = CP15Control;
            val &= 0x000FF085;
            CP15Control &= ~0x000FF085;
            CP15Control |= val;
            //Log(LogLevel::Debug, "CP15Control = %08X (%08X->%08X)\n", CP15Control, old, val);
            UpdateDTCMSetting();
            UpdateITCMSetting();
            if ((old & 0x1005) != (val & 0x1005))
            {
                UpdatePURegions((old & 0x1) != (val & 0x1));
            }
            if (val & CP15_CR_BIGENDIAN) Log(LogLevel::Warn, "!!!! ARM9 BIG ENDIAN MODE. VERY BAD. SHIT GONNA ASPLODE NOW\n");
            if (val & CP15_CR_HIGHEXCEPTIONBASE) ExceptionBase = 0xFFFF0000;
            else                                 ExceptionBase = 0x00000000;
        }
        return;


    case 0x200: // data cacheable
        {
            u32 diff = PU_DataCacheable ^ val;
            PU_DataCacheable = val;
            for (u32 i = 0; i < 8; i++)
            {
                if (diff & (1<<i)) UpdatePURegion(i);
            }
        }
        return;

    case 0x201: // code cacheable
        {
            u32 diff = PU_CodeCacheable ^ val;
            PU_CodeCacheable = val;
            for (u32 i = 0; i < 8; i++)
            {
                if (diff & (1<<i)) UpdatePURegion(i);
            }
        }
        return;


    case 0x300: // data cache write-buffer
        {
            u32 diff = PU_DataCacheWrite ^ val;
            PU_DataCacheWrite = val;
            for (u32 i = 0; i < 8; i++)
            {
                if (diff & (1<<i)) UpdatePURegion(i);
            }
        }
        return;


    case 0x500: // legacy data permissions
        {
            u32 old = PU_DataRW;
            PU_DataRW = 0;
            PU_DataRW |= (val & 0x0003);
            PU_DataRW |= ((val & 0x000C) << 2);
            PU_DataRW |= ((val & 0x0030) << 4);
            PU_DataRW |= ((val & 0x00C0) << 6);
            PU_DataRW |= ((val & 0x0300) << 8);
            PU_DataRW |= ((val & 0x0C00) << 10);
            PU_DataRW |= ((val & 0x3000) << 12);
            PU_DataRW |= ((val & 0xC000) << 14);
            u32 diff = old ^ PU_DataRW;
            for (u32 i = 0; i < 8; i++)
            {
                if (diff & (0xF<<(i*4))) UpdatePURegion(i);
            }
        }
        return;

    case 0x501: // legacy code permissions
        {
            u32 old = PU_CodeRW;
            PU_CodeRW = 0;
            PU_CodeRW |= (val & 0x0003);
            PU_CodeRW |= ((val & 0x000C) << 2);
            PU_CodeRW |= ((val & 0x0030) << 4);
            PU_CodeRW |= ((val & 0x00C0) << 6);
            PU_CodeRW |= ((val & 0x0300) << 8);
            PU_CodeRW |= ((val & 0x0C00) << 10);
            PU_CodeRW |= ((val & 0x3000) << 12);
            PU_CodeRW |= ((val & 0xC000) << 14);
            u32 diff = old ^ PU_CodeRW;
            for (u32 i = 0; i < 8; i++)
            {
                if (diff & (0xF<<(i*4))) UpdatePURegion(i);
            }
        }
        return;

    case 0x502: // data permissions
        {
            u32 diff = PU_DataRW ^ val;
            PU_DataRW = val;
            for (u32 i = 0; i < 8; i++)
            {
                if (diff & (0xF<<(i*4))) UpdatePURegion(i);
            }
        }
        return;

    case 0x503: // code permissions
        {
            u32 diff = PU_CodeRW ^ val;
            PU_CodeRW = val;
            for (u32 i = 0; i < 8; i++)
            {
                if (diff & (0xF<<(i*4))) UpdatePURegion(i);
            }
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
        char log_output[1024];
        PU_Region[(id >> 4) & 0xF] = val;

        std::snprintf(log_output,
                 sizeof(log_output),
                 "PU: region %d = %08X : %s, %08X-%08X\n",
                 (id >> 4) & 0xF,
                 val,
                 val & 1 ? "enabled" : "disabled",
                 val & 0xFFFFF000,
                 (val & 0xFFFFF000) + (2 << ((val & 0x3E) >> 1))
        );
        Log(LogLevel::Debug, "%s", log_output);
        // Some implementations of Log imply a newline, so we build up the line before printing it

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
        //Halt(255);
        return;
    case 0x751:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
                return ARMInterpreter::A_UNK(this);
        }
        ICacheInvalidateByAddr(val);
        //Halt(255);
        return;
    case 0x752:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
                return ARMInterpreter::A_UNK(this);
        }
        DCacheInvalidateAll();
        //printf("inval data cache %08X\n", val);
        return;
    case 0x761:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
                return ARMInterpreter::A_UNK(this);
        }
        DCacheInvalidateByAddr(val);
        //printf("inval data cache SI\n");
        return;
    case 0x762:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
                return ARMInterpreter::A_UNK(this);
        }
        //Log(LogLevel::Debug,"clean data cache\n");
        DCacheClearAll();
        return;
    case 0x7A1:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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

    case 0xF00:
        if (PU_Map != PU_PrivMap)
        {            
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
                return ARMInterpreter::A_UNK(this);
        } else
            CacheDebugRegisterIndex = val;
        return;

    case 0xF10:
        // instruction cache Tag register
        if (PU_Map != PU_PrivMap)
        {            
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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
            if (CPSR & 0x20) // THUMB
                return ARMInterpreter::T_UNK(this);
            else
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

    if ((id & 0xF00) == 0xF00) // test/debug shit?
        return;

    if ((id & 0xF00) != 0x700)
        Log(LogLevel::Debug, "unknown CP15 write op %03X %08X\n", id, val);
}

u32 ARMv5::CP15Read(u32 id) const
{
    //printf("CP15 read op %03X %08X\n", id, NDS::ARM9->R[15]);

    switch (id)
    {
    case 0x000: // CPU ID
    case 0x003:
    case 0x004:
    case 0x005:
    case 0x006:
    case 0x007:
        return 0x41059461;

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
            u32 ret = 0;
            ret |=  (PU_DataRW & 0x00000003);
            ret |= ((PU_DataRW & 0x00000030) >> 2);
            ret |= ((PU_DataRW & 0x00000300) >> 4);
            ret |= ((PU_DataRW & 0x00003000) >> 6);
            ret |= ((PU_DataRW & 0x00030000) >> 8);
            ret |= ((PU_DataRW & 0x00300000) >> 10);
            ret |= ((PU_DataRW & 0x03000000) >> 12);
            ret |= ((PU_DataRW & 0x30000000) >> 14);
            return ret;
        }
    case 0x501:
        {
            u32 ret = 0;
            ret |=  (PU_CodeRW & 0x00000003);
            ret |= ((PU_CodeRW & 0x00000030) >> 2);
            ret |= ((PU_CodeRW & 0x00000300) >> 4);
            ret |= ((PU_CodeRW & 0x00003000) >> 6);
            ret |= ((PU_CodeRW & 0x00030000) >> 8);
            ret |= ((PU_CodeRW & 0x00300000) >> 10);
            ret |= ((PU_CodeRW & 0x03000000) >> 12);
            ret |= ((PU_CodeRW & 0x30000000) >> 14);
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
        return PU_Region[(id >> 4) & 0xF];

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

    case 0xF00:
        if (PU_Map != PU_PrivMap)
        {            
            return 0;
        } else
            return CacheDebugRegisterIndex;
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

    if ((id & 0xF00) == 0xF00) // test/debug shit?
        return 0;

    Log(LogLevel::Debug, "unknown CP15 read op %03X\n", id);
    return 0;
}


// TCM are handled here.
// TODO: later on, handle PU

u32 ARMv5::CodeRead32(u32 addr, bool branch)
{
    /*if (branch || (!(addr & 0xFFF)))
    {
        if (!(PU_Map[addr>>12] & 0x04))
        {
            PrefetchAbort();
            return 0;
        }
    }*/

    if (addr < ITCMSize)
    {
        CodeCycles = 1;
        return *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)];
    }

    CodeCycles = RegionCodeCycles;
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
    } else
    {
        if (CodeCycles == 0xFF) // cached memory. hax
        {
            if (branch || !(addr & 0x1F))
                CodeCycles = kCodeCacheTiming;//ICacheLookup(addr);
            else
                CodeCycles = 1;

            //return *(u32*)&CurICacheLine[addr & 0x1C];
        }
    }
    if (CodeMem.Mem) return *(u32*)&CodeMem.Mem[addr & CodeMem.Mask];

    return BusRead32(addr);
}


void ARMv5::DataRead8(u32 addr, u32* val)
{
    if (!(PU_Map[addr>>12] & 0x01))
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
            if (PU_Map[addr >> 12] & 0x10)
            {
                *val = (DCacheLookup(addr) >> (8* (addr & 3))) & 0xff;
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
    DataCycles = MemTimings[addr >> 12][1];
}

void ARMv5::DataRead16(u32 addr, u32* val)
{
    if (!(PU_Map[addr>>12] & 0x01))
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
            if (PU_Map[addr >> 12] & 0x10)
            {
                *val = (DCacheLookup(addr) >> (8* (addr & 2))) & 0xffff;
                return;
            }
        }
    }

    addr &= ~1;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *val = *(u16*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles = 1;
        *val = *(u16*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        return;
    }

    *val = BusRead16(addr);
    DataCycles = MemTimings[addr >> 12][1];
}

void ARMv5::DataRead32(u32 addr, u32* val)
{
    if (!(PU_Map[addr>>12] & 0x01))
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
            if (PU_Map[addr >> 12] & 0x10)
            {
                *val = DCacheLookup(addr);
                return;
            }
        }
    }

    addr &= ~3;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *val = *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles = 1;
        *val = *(u32*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        return;
    }

    *val = BusRead32(addr);
    DataCycles = MemTimings[addr >> 12][2];
}

void ARMv5::DataRead32S(u32 addr, u32* val)
{
    addr &= ~3;

#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif  
    {
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) 
        {
            if (PU_Map[addr >> 12] & 0x10)
            {
                *val = DCacheLookup(addr);
                return;
            }
        }
    }

    if (addr < ITCMSize)
    {
        DataCycles += 1;
        *val = *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles += 1;
        *val = *(u32*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        return;
    }

    *val = BusRead32(addr);
    DataCycles += MemTimings[addr >> 12][3];
}

void ARMv5::DataWrite8(u32 addr, u8 val)
{
    if (!(PU_Map[addr>>12] & 0x02))
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
            if (PU_Map[addr >> 12] & 0x10)
            {
                DCacheWrite8(addr, val);
                //DCacheInvalidateByAddr(addr);
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
    DataCycles = MemTimings[addr >> 12][1];
}

void ARMv5::DataWrite16(u32 addr, u16 val)
{
    if (!(PU_Map[addr>>12] & 0x02))
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
            if (PU_Map[addr >> 12] & 0x10)
            {
                DCacheWrite16(addr, val);
    //            DCacheInvalidateByAddr(addr);
            }
        }
    }

    DataRegion = addr;

    addr &= ~1;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *(u16*)&ITCM[addr & (ITCMPhysicalSize - 1)] = val;
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles = 1;
        *(u16*)&DTCM[addr & (DTCMPhysicalSize - 1)] = val;
        return;
    }

    BusWrite16(addr, val);
    DataCycles = MemTimings[addr >> 12][1];
}

void ARMv5::DataWrite32(u32 addr, u32 val)
{
    if (!(PU_Map[addr>>12] & 0x02))
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
            if (PU_Map[addr >> 12] & 0x10)
            {
                DCacheWrite32(addr, val);
    //            DCacheInvalidateByAddr(addr);
            }
        }
    }

    DataRegion = addr;

    addr &= ~3;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)] = val;
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles = 1;
        *(u32*)&DTCM[addr & (DTCMPhysicalSize - 1)] = val;
        return;
    }

    BusWrite32(addr, val);
    DataCycles = MemTimings[addr >> 12][2];
}

void ARMv5::DataWrite32S(u32 addr, u32 val)
{
    addr &= ~3;

#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif  
    {
        if (CP15Control & CP15_CACHE_CR_DCACHEENABLE) 
        {
            if (PU_Map[addr >> 12] & 0x10)
            {
                DCacheWrite32(addr, val);
    //            DCacheInvalidateByAddr(addr);
            }
        }
    }

    if (addr < ITCMSize)
    {
        DataCycles += 1;
        *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)] = val;
#ifdef JIT_ENABLED
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
#endif
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        DataCycles += 1;
        *(u32*)&DTCM[addr & (DTCMPhysicalSize - 1)] = val;
        return;
    }

    BusWrite32(addr, val);
    DataCycles += MemTimings[addr >> 12][3];
}

void ARMv5::GetCodeMemRegion(u32 addr, MemRegion* region)
{
    /*if (addr < ITCMSize)
    {
        region->Mem = ITCM;
        region->Mask = 0x7FFF;
        return;
    }*/

    NDS.ARM9GetMemRegion(addr, false, &CodeMem);
}

}
