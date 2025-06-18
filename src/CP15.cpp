/*
    Copyright 2016-2025 melonDS team

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
#if defined(__x86_64__)
#include <emmintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif
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


void ARMv5::CP15Reset()
{
    CP15Control = 0x2078; // dunno

    RNGSeed = 44203;

    // Memory Regions Protection
    PU_CodeRW = 0;
    PU_DataRW = 0;

    memset(PU_Region, 0, CP15_REGION_COUNT*sizeof(*PU_Region));

    // TCM-Settings
    DTCMSetting = 0;
    ITCMSetting = 0;

    memset(ITCM, 0, ITCMPhysicalSize);
    memset(DTCM, 0, DTCMPhysicalSize);

    // Cache Settings
    PU_CodeCacheable = 0;
    PU_DataCacheable = 0;
    PU_WriteBufferability = 0;

    ICacheLockDown = 0;
    DCacheLockDown = 0;

    memset(ICache, 0, ICACHE_SIZE);
    ICacheInvalidateAll();
    ICacheCount = 0;

    memset(DCache, 0, DCACHE_SIZE);
    DCacheInvalidateAll();
    DCacheCount = 0;

    // Debug / Misc Registers
    CacheDebugRegisterIndex = 0;
    CP15BISTTestStateRegister = 0;
    CP15TraceProcessId = 0;

    // And now Update the internal state
    UpdateDTCMSetting();
    UpdateITCMSetting();
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
    file->Var32(&PU_WriteBufferability);

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
        newDTCMSize = CP15_DTCM_SIZE_BASE << ((DTCMSetting  & CP15_DTCM_SIZE_MASK) >> CP15_DTCM_SIZE_POS);
        if (newDTCMSize < (CP15_DTCM_SIZE_BASE << CP15_DTCM_SIZE_MIN))
            newDTCMSize = CP15_DTCM_SIZE_BASE << CP15_DTCM_SIZE_MIN;

        newDTCMMask = CP15_DTCM_BASE_MASK & ~(newDTCMSize-1);
        newDTCMBase = DTCMSetting & newDTCMMask;
    }
    else
    {
        // DTCM Disabled
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
        ITCMSize = CP15_ITCM_SIZE_BASE << ((ITCMSetting  & CP15_ITCM_SIZE_MASK) >> CP15_ITCM_SIZE_POS);
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
void ARMv5::UpdatePURegion(const u32 n)
{
    if (!(CP15Control & CP15_CR_MPUENABLE))
        return;

    if (n >= CP15_REGION_COUNT)
        return;

    u32 coderw = (PU_CodeRW >> (CP15_REGIONACCESS_BITS_PER_REGION * n)) & CP15_REGIONACCESS_REGIONMASK;
    u32 datarw = (PU_DataRW >> (CP15_REGIONACCESS_BITS_PER_REGION * n)) & CP15_REGIONACCESS_REGIONMASK;

    bool codecache, datacache, datawrite;

    // datacache/datawrite
    // 0/0: goes directly to memory
    // 0/1: goes to write buffer
    // 1/0: goes to write buffer and cache
    // 1/1: goes to cache

    if (CP15Control & CP15_CACHE_CR_ICACHEENABLE)
        codecache = (PU_CodeCacheable >> n) & 0x1;
    else
        codecache = false;

    if (CP15Control & CP15_CACHE_CR_DCACHEENABLE)
    {
        datacache = (PU_DataCacheable >> n) & 0x1;
    }
    else
    {
        datacache = false;
    }
    
    datawrite = (PU_WriteBufferability >> n) & 0x1;

    u32 rgn = PU_Region[n];
    if (!(rgn & CP15_REGION_ENABLE))
    {
        return;
    }

    // notes:
    // * min size of a pu region is 4KiB (12 bits)
    // * size is calculated as size + 1, but the 12 lsb of address space are ignored, therefore we need it as size + 1 - 12, or size - 11
    // * pu regions are aligned based on their size
    u32 size = std::max((int)((rgn>>1) & 0x1F) - 11, 0); // obtain the size, subtract 11 and clamp to a min of 0.
    u32 start = ((rgn >> 12) >> size) << size; // determine the start offset, and use shifts to force alignment with a multiple of the size.
    u32 end = start + (1<<size); // add 1 left shifted by size to start to determine end point
    // dont need to bounds check the end point because the force alignment inherently prevents it from breaking

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
    default: Log(LogLevel::Warn, "!! BAD DATARW VALUE %d\n", datarw & ((1 << CP15_REGIONACCESS_BITS_PER_REGION)-1));
    }

    switch (coderw)
    {
    case 0: break;
    case 1: privmask |= CP15_MAP_EXECUTABLE; break;
    case 2: privmask |= CP15_MAP_EXECUTABLE; usermask |= CP15_MAP_EXECUTABLE; break;
    case 3: privmask |= CP15_MAP_EXECUTABLE; usermask |= CP15_MAP_EXECUTABLE; break;
    case 5: privmask |= CP15_MAP_EXECUTABLE; break;
    case 6: privmask |= CP15_MAP_EXECUTABLE; usermask |= CP15_MAP_EXECUTABLE; break;
    default: Log(LogLevel::Warn, "!! BAD CODERW VALUE %d\n", datarw & ((1 << CP15_REGIONACCESS_BITS_PER_REGION)-1));
    }

    if (datacache)
    {
        privmask |= 0x10;
        usermask |= 0x10;
    }
    
    if (datawrite & 0x1)
    {
        privmask |= 0x20;
        usermask |= 0x20;
    }

    if (codecache)
    {
        privmask |= CP15_MAP_ICACHEABLE;
        usermask |= CP15_MAP_ICACHEABLE;
    }

    Log(
        LogLevel::Debug,
        "PU region %d: %08X-%08X, user=%02X priv=%02X, %08X/%08X\n",
        n,
        start << CP15_MAP_ENTRYSIZE_LOG2,
        (end << CP15_MAP_ENTRYSIZE_LOG2) - 1,
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
}

void ARMv5::UpdatePURegions(const bool update_all)
{
    if (!(CP15Control & CP15_CR_MPUENABLE))
    {
        // PU disabled

        u8 mask = CP15_MAP_READABLE | CP15_MAP_WRITEABLE | CP15_MAP_EXECUTABLE;

        memset(PU_UserMap, mask, CP15_MAP_ENTRYCOUNT);
        memset(PU_PrivMap, mask, CP15_MAP_ENTRYCOUNT);

        return;
    }

    if (update_all)
    {
        memset(PU_UserMap, CP15_MAP_NOACCESS, CP15_MAP_ENTRYCOUNT);
        memset(PU_PrivMap, CP15_MAP_NOACCESS, CP15_MAP_ENTRYCOUNT);
    }

    for (int n = 0; n < CP15_REGION_COUNT; n++)
    {
        UpdatePURegion(n);
    }

    // TODO: throw exception if the region we're running in has become non-executable, I guess
}

void ARMv5::UpdateRegionTimings(u32 addrstart, u32 addrend)
{
    for (u32 i = addrstart; i < addrend; i++)
    {
        u8* bustimings = NDS.ARM9MemTimings[i];

        MemTimings[i][0] = (bustimings[0] << NDS.ARM9ClockShift) - 1;
        MemTimings[i][1] = (bustimings[2] << NDS.ARM9ClockShift) - 1;
        MemTimings[i][2] = (bustimings[3] << NDS.ARM9ClockShift) - 1; // sequentials technically should probably be -1 as well?
                                                                // but it doesn't really matter as long as i also dont force align the start of sequential accesses, now does it?
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

bool ARMv5::ICacheLookup(const u32 addr)
{
    const u32 tag = (addr & ~(ICACHE_LINELENGTH - 1));
    const u32 id = ((addr >> ICACHE_LINELENGTH_LOG2) & (ICACHE_LINESPERSET-1)) << ICACHE_SETS_LOG2;
    
#if defined(__x86_64__)
    // we use sse here to greatly speed up checking for valid sets vs the fallback for loop

    __m128i tags; memcpy(&tags, &ICacheTags[id], 16); // load the tags for all 4 sets, one for each 32 bits
    __m128i mask = _mm_set1_epi32(~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)); // load copies of the mask into each 32 bits
    __m128i cmp = _mm_set1_epi32(tag | CACHE_FLAG_VALID); // load the tag we're checking for into each 32 bit
    tags = _mm_and_si128(tags, mask); // mask out the bits we dont want to check for
    cmp = _mm_cmpeq_epi32(tags, cmp); // compare to see if any bits match; sets all bits of each value to either 0 or 1 depending on the result
    u32 set = _mm_movemask_ps(_mm_castsi128_ps(cmp)); // move the "sign bits" of each field into the low 4 bits of a 32 bit integer

    if (!set) goto miss; // check if none of them were a match
    else set = __builtin_ctz(set); // count trailing zeros and right shift to figure out which set had a match 

    {
#elif defined(__ARM_NEON)
    uint32x4_t tags = { ICacheTags[id+0], ICacheTags[id+1], ICacheTags[id+2], ICacheTags[id+3] }; // load tags
    uint32x4_t mask = { ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK) }; // load mask
    uint32x4_t cmp = { tag | CACHE_FLAG_VALID,
                       tag | CACHE_FLAG_VALID,
                       tag | CACHE_FLAG_VALID,
                       tag | CACHE_FLAG_VALID }; // load tag and flag we're checking for
    tags = vandq_u32(tags, mask); // mask out bits we dont wanna check for
    cmp = vceqq_u32(tags, cmp);
    uint16x4_t res = vmovn_u32(cmp);
    u64 set; memcpy(&set, &res, 4);
    
    if (!set) goto miss;
    else set = __builtin_ctz(set) >> 4;

    {
#else
    // fallback for loop; slow
    for (int set = 0; set < ICACHE_SETS; set++)
    {
        if ((ICacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == (tag | CACHE_FLAG_VALID))
#endif
        {
            u32 *cacheLine = (u32 *)&ICache[(id+set) << ICACHE_LINELENGTH_LOG2];

            if (ICacheStreamPtr >= 7)
            {
                if (NDS.ARM9Timestamp < ITCMTimestamp) NDS.ARM9Timestamp = ITCMTimestamp; // does this apply to streamed fetches?
                NDS.ARM9Timestamp++;
            }
            else
            {
                u64 nextfill = ICacheStreamTimes[ICacheStreamPtr++];
                if (NDS.ARM9Timestamp < nextfill)
                {
                    NDS.ARM9Timestamp = nextfill;
                }
                else
                {
                    u64 fillend = ICacheStreamTimes[6] + 2;
                    if (NDS.ARM9Timestamp < fillend) NDS.ARM9Timestamp = fillend;
                    else // checkme
                    {
                        if (NDS.ARM9Timestamp < ITCMTimestamp) NDS.ARM9Timestamp = ITCMTimestamp;
                        NDS.ARM9Timestamp++;
                    }
                    ICacheStreamPtr = 7;
                }
            }
            if (NDS.ARM9Timestamp < TimestampMemory) NDS.ARM9Timestamp = TimestampMemory;
            DataRegion = Mem9_Null;
            Store = false;

            RetVal = cacheLine[(addr & (ICACHE_LINELENGTH -1)) / 4];
            if (DelayedQueue != nullptr) QueueFunction(DelayedQueue);
            return true;
        }
    }

    // cache miss
    miss:
    // We do not fill the cacheline if it is disabled in the 
    // BIST test State register (See arm946e-s Rev 1 technical manual, 2.3.15 "Register 15, test State Register")
    if (CP15BISTTestStateRegister & CP15_BIST_TR_DISABLE_ICACHE_LINEFILL) [[unlikely]]
        return false;
        
    //if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    WriteBufferDrain();
    FetchAddr[16] = addr;
    QueueFunction(&ARMv5::ICacheLookup_2);
    return true;
}

void ARMv5::ICacheLookup_2()
{
    u32 addr = FetchAddr[16];
    const u32 tag = (addr & ~(ICACHE_LINELENGTH - 1));
    const u32 id = ((addr >> ICACHE_LINELENGTH_LOG2) & (ICACHE_LINESPERSET-1)) << ICACHE_SETS_LOG2;

    u32 line;

    if (CP15Control & CP15_CACHE_CR_ROUNDROBIN) [[likely]]
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
        if (ICacheLockDown & CACHE_LOCKUP_L) [[unlikely]]
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

    line += id;

    u32* ptr = (u32 *)&ICache[line << ICACHE_LINELENGTH_LOG2];
    
    // bus reads can only overlap with dcache streaming by 6 cycles
    if (DCacheStreamPtr < 7)
    {
        u64 time = DCacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }

    ICacheTags[line] = tag | (line & (ICACHE_SETS-1)) | CACHE_FLAG_VALID;
    
    // timing logic
    NDS.ARM9Timestamp = NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1) & ~((1<<NDS.ARM9ClockShift)-1);

    if (NDS.ARM9Regions[addr>>14] == Mem9_MainRAM)
    {
        MRTrack.Type = MainRAMType::ICacheStream;
        MRTrack.Var = line;
        FetchAddr[16] = addr & ~3;
        if (CP15BISTTestStateRegister & CP15_BIST_TR_DISABLE_ICACHE_STREAMING) [[unlikely]]
            ICacheStreamPtr = 7;
        else ICacheStreamPtr = (addr & 0x1F) / 4;
    }
    else
    {
        for (int i = 0; i < ICACHE_LINELENGTH; i+=sizeof(u32))
            ptr[i/4] = NDS.ARM9Read32(tag+i);

        if (((NDS.ARM9Timestamp <= WBReleaseTS) && (NDS.ARM9Regions[addr>>14] == WBLastRegion)) // check write buffer
            || (Store && (NDS.ARM9Regions[addr>>14] == DataRegion))) //check the actual store
                NDS.ARM9Timestamp += 1<<NDS.ARM9ClockShift;

        // Disabled ICACHE Streaming:
        // Wait until the entire cache line is filled before continuing with execution
        if (CP15BISTTestStateRegister & CP15_BIST_TR_DISABLE_ICACHE_STREAMING) [[unlikely]]
        {
            u32 stall = (4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift;
            NDS.ARM9Timestamp += (MemTimings[tag >> 14][1] + stall) + ((MemTimings[tag >> 14][2] + 1) * ((DCACHE_LINELENGTH / 4) - 1));
            if (NDS.ARM9Timestamp < TimestampMemory) NDS.ARM9Timestamp = TimestampMemory; // this should never trigger in practice
        }
        else // ICache Streaming logic
        {
            u32 stall = (4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift;
            u8 ns = MemTimings[addr>>14][1] + stall;
            u8 seq = MemTimings[addr>>14][2] + 1;
        
            u8 linepos = (addr & 0x1F) / 4; // technically this is one too low, but we want that actually

            u64 cycles = ns + (seq * linepos);
            NDS.ARM9Timestamp = cycles += NDS.ARM9Timestamp;

            ICacheStreamPtr = linepos;
            for (int i = linepos; i < 7; i++)
            {
                cycles += seq;
                ICacheStreamTimes[i] = cycles;
            }
        }
        RetVal = ptr[(addr & (ICACHE_LINELENGTH-1)) / 4];
    }
    Store = false;
    DataRegion = Mem9_Null;
    if (DelayedQueue != nullptr) QueueFunction(DelayedQueue);
}

void ARMv5::ICacheInvalidateByAddr(const u32 addr)
{
    const u32 tag = (addr & ~(ICACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
    const u32 id = ((addr >> ICACHE_LINELENGTH_LOG2) & (ICACHE_LINESPERSET-1)) << ICACHE_SETS_LOG2;

    for (int set = 0; set < ICACHE_SETS; set++)
    {
        if ((ICacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
        {
            ICacheTags[id+set] &= ~CACHE_FLAG_VALID;
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
    ICacheTags[idx] &= ~CACHE_FLAG_VALID;
}


void ARMv5::ICacheInvalidateAll()
{
    #pragma GCC ivdep
    for (int i = 0; i < ICACHE_SIZE / ICACHE_LINELENGTH; i++)
        ICacheTags[i] &= ~CACHE_FLAG_VALID;
}

bool ARMv5::IsAddressICachable(const u32 addr) const
{
    return PU_Map[addr >> CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_ICACHEABLE;
}

bool ARMv5::DCacheLookup(const u32 addr)
{
    //Log(LogLevel::Debug,"DCache load @ %08x\n", addr);
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1));
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;

#if defined(__x86_64__)
    // we use sse here to greatly speed up checking for valid sets vs the fallback for loop

    __m128i tags; memcpy(&tags, &DCacheTags[id], 16); // load the tags for all 4 sets, one for each 32 bits
    __m128i mask = _mm_set1_epi32(~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)); // load copies of the mask into each 32 bits
    __m128i cmp = _mm_set1_epi32(tag | CACHE_FLAG_VALID); // load the tag we're checking for into each 32 bit
    tags = _mm_and_si128(tags, mask); // mask out the bits we dont want to check for
    cmp = _mm_cmpeq_epi32(tags, cmp); // compare to see if any bits match; sets all bits of each value to either 0 or 1 depending on the result
    u32 set = _mm_movemask_ps(_mm_castsi128_ps(cmp)); // move the "sign bits" of each field into the low 4 bits of a 32 bit integer

    if (!set) goto miss; // check if none of them were a match
    else set = __builtin_ctz(set); // count trailing zeros and right shift to figure out which set had a match 

    {
#elif defined(__ARM_NEON)
    uint32x4_t tags = { DCacheTags[id+0], DCacheTags[id+1], DCacheTags[id+2], DCacheTags[id+3] }; // load tags
    uint32x4_t mask = { ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK) }; // load mask
    uint32x4_t cmp = { tag | CACHE_FLAG_VALID,
                       tag | CACHE_FLAG_VALID,
                       tag | CACHE_FLAG_VALID,
                       tag | CACHE_FLAG_VALID }; // load tag and flag we're checking for
    tags = vandq_u32(tags, mask); // mask out bits we dont wanna check for
    cmp = vceqq_u32(tags, cmp);
    uint16x4_t res = vmovn_u32(cmp);
    u64 set; memcpy(&set, &res, 4);
    
    if (!set) goto miss;
    else set = __builtin_ctz(set) >> 4;

    {
#else
    // fallback for loop; slow
    for (int set = 0; set < DCACHE_SETS; set++)
    {
        if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == (tag | CACHE_FLAG_VALID))
#endif
        {
            u32 *cacheLine = (u32 *)&DCache[(id+set) << DCACHE_LINELENGTH_LOG2];

            if (DCacheStreamPtr >= 7)
            {
                NDS.ARM9Timestamp += DataCycles = 1;
            }
            else
            {
                u64 nextfill = DCacheStreamTimes[DCacheStreamPtr++];
                //if (NDS.ARM9Timestamp < nextfill) // can this ever really fail?
                {
                    DataCycles = nextfill - NDS.ARM9Timestamp;
                    if (DataCycles > (3<<NDS.ARM9ClockShift)) DataCycles = 3<<NDS.ARM9ClockShift;
                    NDS.ARM9Timestamp = nextfill;
                }
                /*else
                {
                    u64 fillend = DCacheStreamTimes[6] + 2;
                    if (NDS.ARM9Timestamp < fillend) DataCycles = fillend - NDS.ARM9Timestamp;
                    else DataCycles = 1;
                    DCacheStreamPtr = 7;
                }*/
            }
            DataRegion = Mem9_DCache;
            //Log(LogLevel::Debug, "DCache hit at %08lx returned %08x from set %i, line %i\n", addr, cacheLine[(addr & (DCACHE_LINELENGTH -1)) >> 2], set, id>>2);
            RetVal = cacheLine[(addr & (DCACHE_LINELENGTH -1)) >> 2];
            (this->*DelayedQueue)();
            return true;
        }
    }
    
    // bus reads can only overlap with icache streaming by 6 cycles
    // checkme: does cache trigger this?
    if (ICacheStreamPtr < 7)
    {
        u64 time = ICacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }

    // cache miss
    miss:
    // We do not fill the cacheline if it is disabled in the 
    // BIST test State register (See arm946e-s Rev 1 technical manual, 2.3.15 "Register 15, test State Register")
    if (CP15BISTTestStateRegister & CP15_BIST_TR_DISABLE_DCACHE_LINEFILL) [[unlikely]]
        return false;

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    WriteBufferDrain(); // checkme?

    FetchAddr[16] = addr;
    QueueFunction(&ARMv5::DCacheLookup_2);
    return true;
}

void ARMv5::DCacheLookup_2()
{
    u32 addr = FetchAddr[16];
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1));
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;
    u32 line;

    if (CP15Control & CP15_CACHE_CR_ROUNDROBIN) [[likely]]
    {
        line = DCacheCount;
        DCacheCount = (line+1) & (DCACHE_SETS-1);
    }
    else
    {
        line = RandomLineIndex();
    }

    if (DCacheLockDown)
    {
        if (DCacheLockDown & CACHE_LOCKUP_L) [[unlikely]]
        {
            // load into locked up cache
            // into the selected set
            line = DCacheLockDown & (DCACHE_SETS-1);
        } else
        {
            u8 minSet = DCacheLockDown & (DCACHE_SETS-1);
            line = line | minSet;
        }
    }
    line += id;

    #if !DISABLE_CACHEWRITEBACK
        // Before we fill the cacheline, we need to write back dirty content
        // Datacycles will be incremented by the required cycles to do so
        DCacheClearByASetAndWay(line & (DCACHE_SETS-1), line >> DCACHE_SETS_LOG2);
    #endif
    
    QueuedDCacheLine = line;
    QueueFunction(&ARMv5::DCacheLookup_3);
}

void ARMv5::DCacheLookup_3()
{
    u32 addr = FetchAddr[16];
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1));
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;
    u32 line = QueuedDCacheLine;
    u32* ptr = (u32 *)&DCache[line << DCACHE_LINELENGTH_LOG2];
    DCacheTags[line] = tag | (line & (DCACHE_SETS-1)) | CACHE_FLAG_VALID;
    
    // timing logic
    
    if (NDS.ARM9Regions[addr>>14] == Mem9_MainRAM)
    {
        MRTrack.Type = MainRAMType::DCacheStream;
        MRTrack.Var = line;

        if (CP15BISTTestStateRegister & CP15_BIST_TR_DISABLE_DCACHE_STREAMING) [[unlikely]]
            DCacheStreamPtr = 7;
        else DCacheStreamPtr = (addr & 0x1F) / 4;

        QueueFunction(DelayedQueue);
    }
    else
    {
        for (int i = 0; i < DCACHE_LINELENGTH; i+=sizeof(u32))
        {
            ptr[i >> 2] = BusRead32(tag+i);    
        }
        // Disabled DCACHE Streaming:
        // Wait until the entire cache line is filled before continuing with execution
        if (CP15BISTTestStateRegister & CP15_BIST_TR_DISABLE_DCACHE_STREAMING) [[unlikely]]
        {
            NDS.ARM9Timestamp = NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1) & ~((1<<NDS.ARM9ClockShift)-1);

            u32 stall = (4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift;

            NDS.ARM9Timestamp += (MemTimings[tag >> 14][1] + stall) + ((MemTimings[tag >> 14][2] + 1) * ((DCACHE_LINELENGTH / 4) - 1));
            DataCycles = MemTimings[tag>>14][2]; // checkme
        
            DataRegion = NDS.ARM9Regions[addr>>14];
            if (((NDS.ARM9Timestamp <= WBReleaseTS) && (NDS.ARM9Regions[addr>>14] == WBLastRegion)) // check write buffer
                || (Store && (NDS.ARM9Regions[addr>>14] == DataRegion))) //check the actual store
                NDS.ARM9Timestamp += 1<<NDS.ARM9ClockShift;
        }
        else // DCache Streaming logic
        {
            DataRegion = NDS.ARM9Regions[addr>>14];
            if ((NDS.ARM9Timestamp <= WBReleaseTS) && (DataRegion == WBLastRegion)) // check write buffer
                NDS.ARM9Timestamp += 1<<NDS.ARM9ClockShift;

            NDS.ARM9Timestamp = NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1) & ~((1<<NDS.ARM9ClockShift)-1);
            
            u32 stall = (4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift;
            u8 ns = MemTimings[addr>>14][1] + stall;
            u8 seq = MemTimings[addr>>14][2] + 1;

            u8 linepos = (addr & 0x1F) >> 2; // technically this is one too low, but we want that actually

            u64 cycles = ns + (seq * linepos);
            DataCycles = 3<<NDS.ARM9ClockShift; // checkme
            NDS.ARM9Timestamp += DataCycles;
            cycles = NDS.ARM9Timestamp;

            DCacheStreamPtr = linepos;
            for (int i = linepos; i < 7; i++)
            {
                cycles += seq;
                DCacheStreamTimes[i] = cycles;
            }
        }
        RetVal = ptr[(addr & (DCACHE_LINELENGTH-1)) >> 2];
        (this->*DelayedQueue)();
    }
}

bool ARMv5::DCacheWrite32(const u32 addr, const u32 val)
{
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;

    //Log(LogLevel::Debug, "Cache write 32: %08lx <= %08lx\n", addr, val);
    
#if defined(__x86_64__)
    // we use sse here to greatly speed up checking for valid sets vs the fallback for loop

    __m128i tags; memcpy(&tags, &DCacheTags[id], 16); // load the tags for all 4 sets, one for each 32 bits
    __m128i mask = _mm_set1_epi32(~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)); // load copies of the mask into each 32 bits
    __m128i cmp = _mm_set1_epi32(tag); // load the tag we're checking for into each 32 bit
    tags = _mm_and_si128(tags, mask); // mask out the bits we dont want to check for
    cmp = _mm_cmpeq_epi32(tags, cmp); // compare to see if any bits match; sets all bits of each value to either 0 or 1 depending on the result
    u32 set = _mm_movemask_ps(_mm_castsi128_ps(cmp)); // move the "sign bits" of each field into the low 4 bits of a 32 bit integer

    if (!set) return false; // check if none of them were a match
    else set = __builtin_ctz(set); // count trailing zeros and right shift to figure out which set had a match 

    {
#elif defined(__ARM_NEON)
    uint32x4_t tags = { DCacheTags[id+0], DCacheTags[id+1], DCacheTags[id+2], DCacheTags[id+3] }; // load tags
    uint32x4_t mask = { ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK) }; // load mask
    uint32x4_t cmp = { tag, tag, tag, tag }; // load tag and flag we're checking for
    tags = vandq_u32(tags, mask); // mask out bits we dont wanna check for
    cmp = vceqq_u32(tags, cmp);
    uint16x4_t res = vmovn_u32(cmp);
    u64 set; memcpy(&set, &res, 4);
    
    if (!set) return false;
    else set = __builtin_ctz(set) >> 4;

    {
#else
    // fallback for loop; slow
    for (int set = 0; set < DCACHE_SETS; set++)
    {
        if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
#endif
        {
            u32 *cacheLine = (u32 *)&DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            cacheLine[(addr & (DCACHE_LINELENGTH-1)) >> 2] = val;
            NDS.ARM9Timestamp += DataCycles = 1;
            DataRegion = Mem9_DCache;
            #if !DISABLE_CACHEWRITEBACK
                if (PU_Map[addr >> CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_BUFFERABLE)
                {
                    if (addr & (DCACHE_LINELENGTH / 2))
                    {
                        DCacheTags[id+set] |= CACHE_FLAG_DIRTY_UPPERHALF;
                    }
                    else
                    {
                        DCacheTags[id+set] |= CACHE_FLAG_DIRTY_LOWERHALF;
                    } 
                    // just mark dirty and abort the data write through the bus
                    return true;
                }
            #endif
            return false;
        }
    }    
    return false;
}

bool ARMv5::DCacheWrite16(const u32 addr, const u16 val)
{
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;
    //Log(LogLevel::Debug, "Cache write 16: %08lx <= %04x\n", addr, val);
    
#if defined(__x86_64__)
    // we use sse here to greatly speed up checking for valid sets vs the fallback for loop

    __m128i tags; memcpy(&tags, &DCacheTags[id], 16); // load the tags for all 4 sets, one for each 32 bits
    __m128i mask = _mm_set1_epi32(~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)); // load copies of the mask into each 32 bits
    __m128i cmp = _mm_set1_epi32(tag); // load the tag we're checking for into each 32 bit
    tags = _mm_and_si128(tags, mask); // mask out the bits we dont want to check for
    cmp = _mm_cmpeq_epi32(tags, cmp); // compare to see if any bits match; sets all bits of each value to either 0 or 1 depending on the result
    u32 set = _mm_movemask_ps(_mm_castsi128_ps(cmp)); // move the "sign bits" of each field into the low 4 bits of a 32 bit integer

    if (!set) return false; // check if none of them were a match
    else set = __builtin_ctz(set); // count trailing zeros and right shift to figure out which set had a match 

    {
#elif defined(__ARM_NEON)
    uint32x4_t tags = { DCacheTags[id+0], DCacheTags[id+1], DCacheTags[id+2], DCacheTags[id+3] }; // load tags
    uint32x4_t mask = { ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK) }; // load mask
    uint32x4_t cmp = { tag, tag, tag, tag }; // load tag and flag we're checking for
    tags = vandq_u32(tags, mask); // mask out bits we dont wanna check for
    cmp = vceqq_u32(tags, cmp);
    uint16x4_t res = vmovn_u32(cmp);
    u64 set; memcpy(&set, &res, 4);
    
    if (!set) return false;
    else set = __builtin_ctz(set) >> 4;

    {
#else
    // fallback for loop; slow
    for (int set = 0; set < DCACHE_SETS; set++)
    {
        if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
#endif
        {
            u16 *cacheLine = (u16 *)&DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            cacheLine[(addr & (DCACHE_LINELENGTH-1)) >> 1] = val;
            NDS.ARM9Timestamp += DataCycles = 1;
            DataRegion = Mem9_DCache;
            #if !DISABLE_CACHEWRITEBACK
                if (PU_Map[addr >> CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_BUFFERABLE)
                {
                    if (addr & (DCACHE_LINELENGTH / 2))
                    {
                        DCacheTags[id+set] |= CACHE_FLAG_DIRTY_UPPERHALF;
                    }
                    else
                    {
                        DCacheTags[id+set] |= CACHE_FLAG_DIRTY_LOWERHALF;
                    } 
                    // just mark dirtyand abort the data write through the bus
                    return true;
                }
            #endif
            return false;
        }
    }    
    return false;
}

bool ARMv5::DCacheWrite8(const u32 addr, const u8 val)
{
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;

    //Log(LogLevel::Debug, "Cache write 8: %08lx <= %02x\n", addr, val);
    
#if defined(__x86_64__)
    // we use sse here to greatly speed up checking for valid sets vs the fallback for loop

    __m128i tags; memcpy(&tags, &DCacheTags[id], 16); // load the tags for all 4 sets, one for each 32 bits
    __m128i mask = _mm_set1_epi32(~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)); // load copies of the mask into each 32 bits
    __m128i cmp = _mm_set1_epi32(tag); // load the tag we're checking for into each 32 bit
    tags = _mm_and_si128(tags, mask); // mask out the bits we dont want to check for
    cmp = _mm_cmpeq_epi32(tags, cmp); // compare to see if any bits match; sets all bits of each value to either 0 or 1 depending on the result
    u32 set = _mm_movemask_ps(_mm_castsi128_ps(cmp)); // move the "sign bits" of each field into the low 4 bits of a 32 bit integer

    if (!set) return false; // check if none of them were a match
    else set = __builtin_ctz(set); // count trailing zeros and right shift to figure out which set had a match 

    {
#elif defined(__ARM_NEON)
    uint32x4_t tags = { DCacheTags[id+0], DCacheTags[id+1], DCacheTags[id+2], DCacheTags[id+3] }; // load tags
    uint32x4_t mask = { ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK) }; // load mask
    uint32x4_t cmp = { tag, tag, tag, tag }; // load tag and flag we're checking for
    tags = vandq_u32(tags, mask); // mask out bits we dont wanna check for
    cmp = vceqq_u32(tags, cmp);
    uint16x4_t res = vmovn_u32(cmp);
    u64 set; memcpy(&set, &res, 4);
    
    if (!set) return false;
    else set = __builtin_ctz(set) >> 4;

    {
#else
    // fallback for loop; slow
    for (int set = 0; set < DCACHE_SETS; set++)
    {
        if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
#endif
        {
            u8 *cacheLine = &DCache[(id+set) << DCACHE_LINELENGTH_LOG2];
            cacheLine[addr & (DCACHE_LINELENGTH-1)] = val;
            NDS.ARM9Timestamp += DataCycles = 1;
            DataRegion = Mem9_DCache;
            #if !DISABLE_CACHEWRITEBACK
                if (PU_Map[addr >> CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_BUFFERABLE)
                {
                    if (addr & (DCACHE_LINELENGTH / 2))
                    {
                        DCacheTags[id+set] |= CACHE_FLAG_DIRTY_UPPERHALF;
                    }
                    else
                    {
                        DCacheTags[id+set] |= CACHE_FLAG_DIRTY_LOWERHALF;
                    } 
                    
                    // just mark dirty and abort the data write through the bus                
                    return true;
                }
            #endif
            return false;
        }
    }  
    return false;
}

void ARMv5::DCacheInvalidateByAddr(const u32 addr)
{
    const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
    const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;
    
#if defined(__x86_64__)
    // we use sse here to greatly speed up checking for valid sets vs the fallback for loop

    __m128i tags; memcpy(&tags, &DCacheTags[id], 16); // load the tags for all 4 sets, one for each 32 bits
    __m128i mask = _mm_set1_epi32(~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)); // load copies of the mask into each 32 bits
    __m128i cmp = _mm_set1_epi32(tag); // load the tag we're checking for into each 32 bit
    tags = _mm_and_si128(tags, mask); // mask out the bits we dont want to check for
    cmp = _mm_cmpeq_epi32(tags, cmp); // compare to see if any bits match; sets all bits of each value to either 0 or 1 depending on the result
    u32 set = _mm_movemask_ps(_mm_castsi128_ps(cmp)); // move the "sign bits" of each field into the low 4 bits of a 32 bit integer

    if (!set) return; // check if none of them were a match
    else set = __builtin_ctz(set); // count trailing zeros and right shift to figure out which set had a match 

    {
#elif defined(__ARM_NEON)
    uint32x4_t tags = { DCacheTags[id+0], DCacheTags[id+1], DCacheTags[id+2], DCacheTags[id+3] }; // load tags
    uint32x4_t mask = { ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK),
                        ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK) }; // load mask
    uint32x4_t cmp = { tag, tag, tag, tag }; // load tag and flag we're checking for
    tags = vandq_u32(tags, mask); // mask out bits we dont wanna check for
    cmp = vceqq_u32(tags, cmp);
    uint16x4_t res = vmovn_u32(cmp);
    u64 set; memcpy(&set, &res, 4);
    
    if (!set) return;
    else set = __builtin_ctz(set) >> 4;

    {
#else
    // fallback for loop; slow
    for (int set = 0; set < DCACHE_SETS; set++)
    {
        if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
#endif
        {
            //Log(LogLevel::Debug,"DCache invalidated %08lx\n", addr & ~(ICACHE_LINELENGTH-1));
            DCacheTags[id+set] &= ~CACHE_FLAG_VALID;
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
    DCacheTags[idx] &= ~CACHE_FLAG_VALID;
}


void ARMv5::DCacheInvalidateAll()
{
    #pragma GCC ivdep
    for (int i = 0; i < DCACHE_SIZE / DCACHE_LINELENGTH; i++)
        DCacheTags[i] &= ~CACHE_FLAG_VALID;
}

void ARMv5::DCacheClearAll()
{
    #if !DISABLE_CACHEWRITEBACK
        for (int set = 0; set < DCACHE_SETS; set++)
            for (int line = 0; line <= DCACHE_LINESPERSET; line++)
                DCacheClearByASetAndWay(set, line);
    #endif
}

void ARMv5::DCacheClearByAddr(const u32 addr)
{
    #if !DISABLE_CACHEWRITEBACK
        const u32 tag = (addr & ~(DCACHE_LINELENGTH - 1)) | CACHE_FLAG_VALID;
        const u32 id = ((addr >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET-1)) << DCACHE_SETS_LOG2;

        for (int set = 0; set < DCACHE_SETS; set++)
        {
            if ((DCacheTags[id+set] & ~(CACHE_FLAG_DIRTY_MASK | CACHE_FLAG_SET_MASK)) == tag)
            {
                DCacheClearByASetAndWay(set, id >> DCACHE_SETS_LOG2);
                return;
            }
        }
    #endif
}

void ARMv5::DCacheClearByASetAndWay(const u8 cacheSet, const u8 cacheLine)
{
    #if !DISABLE_CACHEWRITEBACK
        const u32 index = cacheSet | (cacheLine << DCACHE_SETS_LOG2);

        // Only write back if valid
        if (!(DCacheTags[index] & CACHE_FLAG_VALID))
            return;

        const u32 tag = DCacheTags[index] & ~CACHE_FLAG_MASK;
        u32* ptr = (u32 *)&DCache[index << DCACHE_LINELENGTH_LOG2];

        if (DCacheTags[index] & CACHE_FLAG_DIRTY_LOWERHALF)
        {
            if (WBDelay > NDS.ARM9Timestamp) NDS.ARM9Timestamp = WBDelay;

            WriteBufferWrite(tag, 4);
            WriteBufferWrite(ptr[0], 2, tag+0x00);
            WriteBufferWrite(ptr[1], 3, tag+0x04);
            WriteBufferWrite(ptr[2], 3, tag+0x08);
            WriteBufferWrite(ptr[3], 3, tag+0x0C);
            //NDS.ARM9Timestamp += 4; //DataCycles += 5; CHECKME: does this function like a write does but with mcr?
        }
        if (DCacheTags[index] & CACHE_FLAG_DIRTY_UPPERHALF) // todo: check how this behaves when both fields need to be written
        {
            if (WBDelay > NDS.ARM9Timestamp) NDS.ARM9Timestamp = WBDelay;

            if (DCacheTags[index] & CACHE_FLAG_DIRTY_LOWERHALF)
            {
                WriteBufferWrite(ptr[4], 3, tag+0x10);
            }
            else
            {
                WriteBufferWrite(tag+0x10, 4);
                WriteBufferWrite(ptr[4], 2, tag+0x10);
            }
            WriteBufferWrite(ptr[5], 3, tag+0x14);
            WriteBufferWrite(ptr[6], 3, tag+0x18);
            WriteBufferWrite(ptr[7], 3, tag+0x1C);
            //NDS.ARM9Timestamp += 4;
        }
        DCacheTags[index] &= ~(CACHE_FLAG_DIRTY_LOWERHALF | CACHE_FLAG_DIRTY_UPPERHALF);
    #endif
}

bool ARMv5::IsAddressDCachable(const u32 addr) const
{
    return PU_Map[addr >> CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_DCACHEABLE;
}

#define A9WENTLAST (!NDS.MainRAMLastAccess)
#define A7WENTLAST ( NDS.MainRAMLastAccess)
#define A9LAST false
#define A7LAST true
#define A9PRIORITY !(NDS.ExMemCnt[0] & 0x8000)
#define A7PRIORITY  (NDS.ExMemCnt[0] & 0x8000)

template <WBMode mode>
bool ARMv5::WriteBufferHandle()
{
    while (true)
    {
        if (WBWriting)
        {
            if ((mode == WBMode::Check) && ((NDS.A9ContentionTS << NDS.ARM9ClockShift) > NDS.ARM9Timestamp)) return true;
            // look up timings
            // TODO: handle interrupted bursts?
            u32 cycles;
            switch (WBCurVal >> 61)
            {
                case 0:
                {
                    if (NDS.ARM9Regions[WBCurAddr>>14] == Mem9_MainRAM)
                    {
                        if (A7PRIORITY) { if (NDS.A9ContentionTS >= NDS.ARM7Timestamp) return false; }
                        else            { if (NDS.A9ContentionTS >  NDS.ARM7Timestamp) return false; }
                        if (NDS.A9ContentionTS < NDS.MainRAMTimestamp) { NDS.A9ContentionTS = NDS.MainRAMTimestamp; if (A7PRIORITY) return false; }
                        cycles = 4;
                        NDS.MainRAMTimestamp = NDS.A9ContentionTS + 9;
                        NDS.MainRAMLastAccess = A9LAST;
                    }
                    else cycles = NDS.ARM9MemTimings[WBCurAddr>>14][0]; // todo: twl timings
                    break;
                }
                case 1:
                {
                    if (NDS.ARM9Regions[WBCurAddr>>14] == Mem9_MainRAM)
                    {
                        if (A7PRIORITY) { if (NDS.A9ContentionTS >= NDS.ARM7Timestamp) return false; }
                        else            { if (NDS.A9ContentionTS >  NDS.ARM7Timestamp) return false; }
                        if (NDS.A9ContentionTS < NDS.MainRAMTimestamp) { NDS.A9ContentionTS = NDS.MainRAMTimestamp; if (A7PRIORITY) return false; }
                        NDS.MainRAMTimestamp = NDS.A9ContentionTS + 8;
                        cycles = 3;
                        NDS.MainRAMLastAccess = A9LAST;
                    }
                    else cycles = NDS.ARM9MemTimings[WBCurAddr>>14][0]; // todo: twl timings
                    break;
                }
                case 3:
                {
                    if (NDS.ARM9Regions[WBCurAddr>>14] == Mem9_MainRAM)
                    {
                        if (A7PRIORITY) { if (NDS.A9ContentionTS >= NDS.ARM7Timestamp) return false; }
                        else            { if (NDS.A9ContentionTS >  NDS.ARM7Timestamp) return false; }
                        if (A9WENTLAST)
                        {
                            NDS.MainRAMTimestamp += 2;
                            cycles = 2;
                            break;
                        }
                    }
                    else
                    {
                        cycles = NDS.ARM9MemTimings[WBCurAddr>>14][3];
                        break;
                    }
                }
                case 2:
                {
                    if (NDS.ARM9Regions[WBCurAddr>>14] == Mem9_MainRAM)
                    {
                        if (A7PRIORITY) { if (NDS.A9ContentionTS >= NDS.ARM7Timestamp) return false; }
                        else            { if (NDS.A9ContentionTS >  NDS.ARM7Timestamp) return false; }
                        if (NDS.A9ContentionTS < NDS.MainRAMTimestamp) { NDS.A9ContentionTS = NDS.MainRAMTimestamp; if (A7PRIORITY) return false; }
                        NDS.MainRAMTimestamp = NDS.A9ContentionTS + 9;
                        cycles = 4;
                        NDS.MainRAMLastAccess = A9LAST;
                    }
                    else cycles = NDS.ARM9MemTimings[WBCurAddr>>14][2]; // todo: twl timings
                    break;
                }
            }

            NDS.A9ContentionTS += cycles;
            WBReleaseTS = (NDS.A9ContentionTS << NDS.ARM9ClockShift) - 1;
            if (NDS.ARM9Regions[WBCurAddr>>14] != Mem9_MainRAM && ((WBCurVal >> 61) != 3))
            {
                NDS.A9ContentionTS += 1;
                WBTimestamp = WBReleaseTS + 2; // todo: twl timings
            }
            else
            {
                WBTimestamp = WBReleaseTS;
            }
            if (WBWritePointer != 16 && (WriteBufferFifo[WBWritePointer] >> 61) != 3) WBInitialTS = WBTimestamp;
            
            switch (WBCurVal >> 61)
            {
                case 0: // byte
                    BusWrite8 (WBCurAddr, WBCurVal);
                    break;
                case 1: // halfword
                    BusWrite16(WBCurAddr, WBCurVal);
                    break;
                case 2: // word
                case 3:
                    BusWrite32(WBCurAddr, WBCurVal);
                    break;
                default: // invalid
                    Platform::Log(Platform::LogLevel::Warn, "WHY ARE WE TRYING TO WRITE NONSENSE VIA THE WRITE BUFFER! PANIC!!! Flag: %i\n", (u8)(WBCurVal >> 61));
                    break;
            }

            WBLastRegion = NDS.ARM9Regions[WBCurAddr>>14];
            WBWriting = false;        
            if ((mode == WBMode::SingleBurst) && ((WriteBufferFifo[WBWritePointer] >> 61) != 3)) return true;
        }

        // check if write buffer is empty
        if (WBWritePointer == 16) return true;

        // attempt to drain write buffer
        if ((WriteBufferFifo[WBWritePointer] >> 61) != 4) // not an address
        {
            if (WBInitialTS > NDS.ARM9Timestamp)
            {
                if (mode == WBMode::Check) return true;
                else NDS.ARM9Timestamp = WBInitialTS;
            }

            //if ((WriteBufferFifo[WBWritePointer] >> 61) == 3) WBCurAddr+=4; // TODO
            //if (storeaddr[WBWritePointer] != WBCurAddr) printf("MISMATCH: %08X %08X\n", storeaddr[WBWritePointer], WBCurAddr);

            WBCurAddr = storeaddr[WBWritePointer];
            WBCurVal = WriteBufferFifo[WBWritePointer];
            WBWriting = true;
        }
        else
        {
            //WBCurAddr = (u32)WriteBufferFifo[WBWritePointer]; // TODO
        }
        
        WBWritePointer = (WBWritePointer + 1) & 0xF;
        if (WBWritePointer == WBFillPointer)
        {
            WBWritePointer = 16;
            WBFillPointer = 0;
        }
        if ((mode == WBMode::WaitEntry) && (WBWritePointer != WBFillPointer)) return true;
    }
}
template bool ARMv5::WriteBufferHandle<WBMode::Check>();
template bool ARMv5::WriteBufferHandle<WBMode::Force>();
template bool ARMv5::WriteBufferHandle<WBMode::SingleBurst>();
template bool ARMv5::WriteBufferHandle<WBMode::WaitEntry>();

#undef A9WENTLAST
#undef A7WENTLAST
#undef A9LAST
#undef A7LAST
#undef A9PRIORITY
#undef A7PRIORITY

template <int next>
void ARMv5::WriteBufferCheck()
{
    if ((WBWritePointer != 16) || WBWriting)
    {
        if constexpr (next == 0)
        {
            MRTrack.Type = MainRAMType::WBCheck;
        }
        else if constexpr (next == 2)
        {
            MRTrack.Type = MainRAMType::WBWaitWrite;
        }
        else
        {
            MRTrack.Type = MainRAMType::WBWaitRead;
        }
    }
    /*
    while (!WriteBufferHandle<0>()); // loop until we've cleared out all writeable entries

    if constexpr (next == 1 || next == 3) // check if the next write is occuring
    {
        if (NDS.ARM9Timestamp >= WBInitialTS)// + (NDS.ARM9Regions[WBCurAddr>>14] == Mem9_MainRAM)))// || ((NDS.ARM9Regions[WBCurAddr>>14] == Mem9_MainRAM) && WBWriting))
        {
            u64 tsold = NDS.ARM9Timestamp;
            while(!WriteBufferHandle<2>());

            //if constexpr (next == 3) NDS.ARM9Timestamp = std::max(tsold, NDS.ARM9Timestamp - (2<<NDS.ARM9ClockShift));
        }
    }
    else if constexpr (next == 2)
    {
        //if (NDS.ARM9Timestamp >= WBInitialTS)
            while(!WriteBufferHandle<2>());
    }*/
}
template void ARMv5::WriteBufferCheck<3>();
template void ARMv5::WriteBufferCheck<2>();
template void ARMv5::WriteBufferCheck<1>();
template void ARMv5::WriteBufferCheck<0>();

void ARMv5::WriteBufferWrite(u32 val, u8 flag, u32 addr)
{
    MRTrack.Type = MainRAMType::WBWrite;
    WBAddrQueued[MRTrack.Var] = addr;
    WBValQueued[MRTrack.Var++] = val | (u64)flag << 61;
    /*switch (flag)
    {
        case 0: // byte
            BusWrite8 (addr, val);
            break;
        case 1: // halfword
            BusWrite16(addr, val);
            break;
        case 2: // word
        case 3:
            BusWrite32(addr, val);
            break;
        default: // invalid
            //Platform::Log(Platform::LogLevel::Warn, "WHY ARE WE TRYING TO WRITE NONSENSE VIA THE WRITE BUFFER! PANIC!!! Flag: %i\n", (u8)(WBCurVal >> 61));
            break;
    }*/
    /*WriteBufferCheck<0>();

    if (WBFillPointer == WBWritePointer) // if the write buffer is full then we stall the cpu until room is made
        WriteBufferHandle<1>();
    else if (WBWritePointer == 16) // indicates empty write buffer
    {
        WBWritePointer = 0;
        if (!WBWriting)
        {
            u64 ts = ((NDS.ARM9Regions[addr>>14] == Mem9_MainRAM) ? std::max(MainRAMTimestamp, (NDS.ARM9Timestamp + 1)) : (NDS.ARM9Timestamp + 1));

            if (!WBWriting && (WBTimestamp < ((ts + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1))))
                WBTimestamp = (ts + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

            WBInitialTS = WBTimestamp;
        }
    }

    WriteBufferFifo[WBFillPointer] = val | (u64)flag << 61;
    storeaddr[WBFillPointer] = addr;
    WBFillPointer = (WBFillPointer + 1) & 0xF;*/
}

void ARMv5::WriteBufferDrain()
{
    if ((WBWritePointer != 16) || WBWriting)
        MRTrack.Type = MainRAMType::WBDrain;
    //while (!WriteBufferHandle<1>()); // loop until drained fully
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
                if (diff) UpdatePURegions(true);
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
                if (diff) UpdatePURegions(true);
            #endif
        }
        return;


    case 0x300: // write-buffer
        {
            u32 diff = PU_WriteBufferability ^ val;
            PU_WriteBufferability = val;
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
                if (diff) UpdatePURegions(true);
            #endif
        }
        return;


    case 0x500: // legacy data permissions
        {
            u32 old = PU_DataRW;
            PU_DataRW = 0;
            #pragma GCC ivdep
            #pragma GCC unroll 8
            for (int i = 0; i < CP15_REGION_COUNT; i++)
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
                u32 diff = old ^ PU_DataRW;
                if (diff) UpdatePURegions(true);
            #endif
        }
        return;

    case 0x501: // legacy code permissions
        {
            u32 old = PU_CodeRW;
            PU_CodeRW = 0;
            #pragma GCC ivdep
            #pragma GCC unroll 8
            for (int i = 0; i < CP15_REGION_COUNT; i++)
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
                u32 diff = old ^ PU_CodeRW;
                if (diff) UpdatePURegions(true);
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
                if (diff) UpdatePURegions(true);
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
                if (diff) UpdatePURegions(true);
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
        {
            char log_output[1024];
            u32 old = PU_Region[(id >> 4) & 0xF];
            PU_Region[(id >> 4) & 0xF] = val & ~(0x3F<<6);
            u32 diff = old ^ PU_Region[(id >> 4) & 0xF];

            std::snprintf(log_output,
                     sizeof(log_output),
                     "PU: region %d = %08X : %s, start: %08X size: %02X\n",
                     (id >> 4) & 0xF,
                     val,
                     val & 1 ? "enabled" : "disabled",
                     val & 0xFFFFF000,
                     (val & 0x3E) >> 1
            );
            // TODO: smarter region update for this?
            if (diff) UpdatePURegions(true);
            return;
        }


    case 0x704:
    case 0x782:
        //WriteBufferDrain(); // checkme
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
    /*case 0x752:
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
        */

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
    /*case 0x762:
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
        */
    /*case 0x770:
        // invalidate both caches
        // can be called from user and privileged
        ICacheInvalidateAll();
        DCacheInvalidateAll();
        break;
        */
    /*case 0x7A0:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        //Log(LogLevel::Debug,"clean data cache\n");
        DCacheClearAll();
        return;*/
    case 0x7A1:
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        //Log(LogLevel::Debug,"clean data cache MVA\n");=
        CP15Queue = val;
        QueueFunction(&ARMv5::DCClearAddr_2);
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
            CP15Queue = val;
            QueueFunction(&ARMv5::DCClearSetWay_2);
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
        QueueFunction(&ARMv5::WriteBufferDrain);
        return;

    case 0x7D1:
        //Log(LogLevel::Debug,"Prefetch instruction cache MVA\n");
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        // we force a fill by looking up the value from cache
        // if it wasn't cached yet, it will be loaded into cache
        // low bits are set to 0x1C to trick cache streaming
        CP15Queue = val;
        DelayedQueue = nullptr;
        QueueFunction(&ARMv5::ICachePrefetch_2);
        return;

    /*case 0x7E0:
        //Log(LogLevel::Debug,"clean & invalidate data cache\n");
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        DCacheClearAll();
        DCacheInvalidateAll();
        return;*/
    case 0x7E1:
        //Log(LogLevel::Debug,"clean & invalidate data cache MVA\n");
        // requires priv mode or causes UNKNOWN INSTRUCTION exception
        if (PU_Map != PU_PrivMap)
        {            
            return ARMInterpreter::A_UNK(this);
        }
        CP15Queue = val;
        QueueFunction(&ARMv5::DCClearInvalidateAddr_2);
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
            CP15Queue = val;
            QueueFunction(&ARMv5::DCClearInvalidateSetWay_2);
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
        DCacheLockDown = val;
        Log(LogLevel::Debug,"DCacheLockDown\n");
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
        DTCMSetting = val & (CP15_DTCM_BASE_MASK | CP15_DTCM_SIZE_MASK);
        UpdateDTCMSetting();
        return;

    case 0x911:
        ITCMSetting = val & (CP15_ITCM_BASE_MASK | CP15_ITCM_SIZE_MASK);
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
            *(u32 *)&DCache[(((index << DCACHE_SETS_LOG2) + segment) << DCACHE_LINELENGTH_LOG2) + wordAddress*4] = val;            
        }
        return;

    }

    Log(LogLevel::Debug, "unknown CP15 write op %04X %08X\n", id, val);
}

u32 ARMv5::CP15Read(const u32 id) const
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
        return CP15_TCMSIZE_ITCM_32KB | CP15_TCMSIZE_DTCM_16KB;


    case 0x100: // control reg
        return CP15Control;


    case 0x200:
        return PU_DataCacheable;
    case 0x201:
        return PU_CodeCacheable;
    case 0x300:
        return PU_WriteBufferability;


    case 0x500:
        {
            // this format  has 2 bits per region, but we store 4 per region
            // so we reduce and consoldate the bits
            // 0x502 returns all 4 bits per region
            u32 ret = 0;
            #pragma GCC ivdep
            #pragma GCC unroll 8
            for (int i = 0; i < CP15_REGION_COUNT; i++)
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
            for (int i = 0; i < CP15_REGION_COUNT; i++)
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
void ARMv5::ICachePrefetch_2()
{
    u32 val = CP15Queue;
    ICacheLookup((val & ~0x03) | 0x1C);
}

void ARMv5::DCClearAddr_2()
{
    u32 val = CP15Queue;
    DCacheClearByAddr(val);
}

void ARMv5::DCClearSetWay_2()
{
    u32 val = CP15Queue;
    u8 cacheSet = val >> (32 - DCACHE_SETS_LOG2) & (DCACHE_SETS -1);
    u8 cacheLine = (val >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET -1);
    DCacheClearByASetAndWay(cacheSet, cacheLine);
}

void ARMv5::DCClearInvalidateAddr_2()
{
    u32 val = CP15Queue;
    DCacheClearByAddr(val);
    DCacheInvalidateByAddr(val);
}

void ARMv5::DCClearInvalidateSetWay_2()
{
    u32 val = CP15Queue;
    u8 cacheSet = val >> (32 - DCACHE_SETS_LOG2) & (DCACHE_SETS -1);
    u8 cacheLine = (val >> DCACHE_LINELENGTH_LOG2) & (DCACHE_LINESPERSET -1);
    DCacheClearByASetAndWay(cacheSet, cacheLine);
    DCacheInvalidateBySetAndWay(cacheSet, cacheLine);
}

// TCM are handled here.
// TODO: later on, handle PU

void ARMv5::CodeRead32(u32 addr)
{
    // prefetch abort
    // the actual exception is not raised until the aborted instruction is executed
    if (!(PU_Map[addr>>12] & CP15_MAP_EXECUTABLE)) [[unlikely]]
    {        
        NDS.ARM9Timestamp += 1;
        if (NDS.ARM9Timestamp < TimestampMemory) NDS.ARM9Timestamp = TimestampMemory;
        DataRegion = Mem9_Null;
        Store = false;
        RetVal = ((u64)1<<63);
        QueueFunction(DelayedQueue);
        return;
    }

    if (addr < ITCMSize)
    {
        if (NDS.ARM9Timestamp < ITCMTimestamp) NDS.ARM9Timestamp = ITCMTimestamp;
        NDS.ARM9Timestamp += 1;
        if (NDS.ARM9Timestamp < TimestampMemory) NDS.ARM9Timestamp = TimestampMemory;
        DataRegion = Mem9_Null;
        Store = false;
        RetVal = *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        QueueFunction(DelayedQueue);
        return;
    }
    
    #if !DISABLE_ICACHE
        #ifdef JIT_ENABLED
        //if (!NDS.IsJITEnabled())
        #endif    
        {
            if (IsAddressICachable(addr))
            {
                if (ICacheLookup(addr)) return;
            }
    #endif 
        }
        
    FetchAddr[16] = addr;
    QueueFunction(&ARMv5::CodeRead32_2);
}

void ARMv5::CodeRead32_2()
{
    //if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    // bus reads can only overlap with dcache streaming by 6 cycles
    if (DCacheStreamPtr < 7)
    {
        u64 time = DCacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }
    
    if (PU_Map[FetchAddr[16]>>12] & 0x30)
        WriteBufferDrain();
    else
        WriteBufferCheck<3>();

    QueueFunction(&ARMv5::CodeRead32_3);
}

void ARMv5::CodeRead32_3()
{
    u32 addr = FetchAddr[16];

    NDS.ARM9Timestamp = NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1) & ~((1<<NDS.ARM9ClockShift)-1);

    if ((addr >> 24) == 0x02)
    {
        FetchAddr[16] = addr;
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MRCodeFetch | MR32;

        QueueFunction(DelayedQueue);
    }
    else
    {
        if (((NDS.ARM9Timestamp <= WBReleaseTS) && (NDS.ARM9Regions[addr>>14] == WBLastRegion)) // check write buffer
         || (Store && (NDS.ARM9Regions[addr>>14] == DataRegion))) //check the actual store
            NDS.ARM9Timestamp += 1<<NDS.ARM9ClockShift;
            
        QueueFunction(&ARMv5::CodeRead32_4);
    }
}

void ARMv5::CodeRead32_4()
{
    u32 addr = FetchAddr[16];

    //if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp += (4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift;

    RetVal = BusRead32(addr);

    u8 cycles = MemTimings[addr >> 14][1];

    NDS.DMA9Timestamp = NDS.ARM9Timestamp += cycles;

    if (WBTimestamp < ((NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    Store = false;
    DataRegion = Mem9_Null;
    QueueFunction(DelayedQueue);
}


void ARMv5::DAbortHandle()
{
    if (DCacheStreamPtr < 7)
    {
        u64 fillend = DCacheStreamTimes[6] + 1;
        if (NDS.ARM9Timestamp < fillend) NDS.ARM9Timestamp = fillend; // checkme: should this be data cycles?
        DCacheStreamPtr = 7;
    }
    
    NDS.ARM9Timestamp += DataCycles = 1;
}

void ARMv5::DCacheFin8()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    *val = (RetVal >> (8 * (addr & 3))) & 0xff;
}

bool ARMv5::DataRead8(u32 addr, u8 reg)
{
    // Data Aborts
    // Exception is handled in the actual instruction implementation
    if (!(PU_Map[addr>>12] & CP15_MAP_READABLE)) [[unlikely]]
    {
        QueueFunction(&ARMv5::DAbortHandle);
        return false;
    }

    FetchAddr[reg] = addr;
    LDRRegs = 1<<reg;

    QueueFunction(&ARMv5::DRead8_2);
    return true;
}

void ARMv5::DRead8_2()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (DCacheStreamPtr < 7)
    {
        u64 fillend = DCacheStreamTimes[6] + 1;
        if (NDS.ARM9Timestamp < fillend) NDS.ARM9Timestamp = fillend; // checkme: should this be data cycles?
        DCacheStreamPtr = 7;
    }

    if (addr < ITCMSize)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        ITCMTimestamp = NDS.ARM9Timestamp;
        DataRegion = Mem9_ITCM;
        *val = *(u8*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        DataRegion = Mem9_DTCM;
        *val = *(u8*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        return;
    }

    #if !DISABLE_DCACHE
        #ifdef JIT_ENABLED
        //if (!NDS.IsJITEnabled())
        #endif  
        {
            if (IsAddressDCachable(addr))
            {
                DelayedQueue = &ARMv5::DCacheFin8;
                if (DCacheLookup(addr)) return;
            }
        }
    #endif

    QueueFunction(&ARMv5::DRead8_3);
}

void ARMv5::DRead8_3()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    // bus reads can only overlap with icache streaming by 6 cycles
    // checkme: does dcache trigger this?
    if (ICacheStreamPtr < 7)
    {
        u64 time = ICacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }

    if (PU_Map[addr>>12] & 0x30)
        WriteBufferDrain();
    else
        WriteBufferCheck<1>();

    QueueFunction(&ARMv5::DRead8_4);
}

void ARMv5::DRead8_4()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
    
    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MR8;
        MRTrack.Progress = reg;
    }
    else
    {
        DataRegion = NDS.ARM9Regions[addr>>14];
        if ((NDS.ARM9Timestamp <= WBReleaseTS) && (DataRegion == WBLastRegion)) // check write buffer
            NDS.ARM9Timestamp += 1<<NDS.ARM9ClockShift;

        QueueFunction(&ARMv5::DRead8_5);
    }
}

void ARMv5::DRead8_5()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp += (4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift;

    *val = BusRead8(addr);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp += MemTimings[addr >> 14][0];
    DataCycles = 3<<NDS.ARM9ClockShift;
    
    if (WBTimestamp < ((NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
}

void ARMv5::DCacheFin16()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    *val = (RetVal >> (8 * (addr & 2))) & 0xffff;
}

bool ARMv5::DataRead16(u32 addr, u8 reg)
{
    // Data Aborts
    // Exception is handled in the actual instruction implementation
    if (!(PU_Map[addr>>12] & CP15_MAP_READABLE)) [[unlikely]]
    {
        QueueFunction(&ARMv5::DAbortHandle);
        return false;
    }

    FetchAddr[reg] = addr;
    LDRRegs = 1<<reg;

    QueueFunction(&ARMv5::DRead16_2);
    return true;
}

void ARMv5::DRead16_2()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (DCacheStreamPtr < 7)
    {
        u64 fillend = DCacheStreamTimes[6] + 1;
        if (NDS.ARM9Timestamp < fillend) NDS.ARM9Timestamp = fillend; // checkme: should this be data cycles?
        DCacheStreamPtr = 7;
    }

    addr &= ~1;

    if (addr < ITCMSize)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        ITCMTimestamp = NDS.ARM9Timestamp + DataCycles;
        DataRegion = Mem9_ITCM;
        *val = *(u16*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        DataRegion = Mem9_DTCM;
        *val = *(u16*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        return;
    }
    
    #if !DISABLE_DCACHE
        #ifdef JIT_ENABLED
        //if (!NDS.IsJITEnabled())
        #endif  
        {
            if (IsAddressDCachable(addr))
            {
                DelayedQueue = &ARMv5::DCacheFin16;
                if (DCacheLookup(addr)) return;
            }
        }
    #endif

    QueueFunction(&ARMv5::DRead16_3);
}

void ARMv5::DRead16_3()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    // bus reads can only overlap with icache streaming by 6 cycles
    // checkme: does cache trigger this?
    if (ICacheStreamPtr < 7)
    {
        u64 time = ICacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }

    if (PU_Map[addr>>12] & 0x30)
        WriteBufferDrain();
    else
        WriteBufferCheck<1>();

    QueueFunction(&ARMv5::DRead16_4);
}

void ARMv5::DRead16_4()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
    
    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MR16;
        MRTrack.Progress = reg;
    }
    else
    {
        DataRegion = NDS.ARM9Regions[addr>>14];
        if ((NDS.ARM9Timestamp <= WBReleaseTS) && (DataRegion == WBLastRegion)) // check write buffer
            NDS.ARM9Timestamp += 1<<NDS.ARM9ClockShift;
            
        QueueFunction(&ARMv5::DRead16_5);
    }
}

void ARMv5::DRead16_5()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp += (4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift;

    *val = BusRead16(addr);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp += MemTimings[addr >> 14][0];
    DataCycles = 3<<NDS.ARM9ClockShift;

    if (WBTimestamp < ((NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
}

void ARMv5::DCacheFin32()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];
    *val = RetVal;
    LDRRegs &= ~1<<reg;
}

bool ARMv5::DataRead32(u32 addr, u8 reg)
{
    // Data Aborts
    // Exception is handled in the actual instruction implementation
    if (!(PU_Map[addr>>12] & CP15_MAP_READABLE)) [[unlikely]]
    {
        QueueFunction(&ARMv5::DAbortHandle);
        return false;
    }
    
    FetchAddr[reg] = addr;
    LDRRegs = 1<<reg;

    QueueFunction(&ARMv5::DRead32_2);
    return true;
}

void ARMv5::DRead32_2()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (DCacheStreamPtr < 7)
    {
        u64 fillend = DCacheStreamTimes[6] + 1;
        if (NDS.ARM9Timestamp < fillend) NDS.ARM9Timestamp = fillend; // checkme: should this be data cycles?
        DCacheStreamPtr = 7;
    }

    addr &= ~3;

    if (addr < ITCMSize)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        ITCMTimestamp = NDS.ARM9Timestamp + DataCycles;
        DataRegion = Mem9_ITCM;
        *val = *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        LDRRegs &= ~1<<reg;
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        DataRegion = Mem9_DTCM;
        *val = *(u32*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        LDRRegs &= ~1<<reg;
        return;
    }

    #if !DISABLE_DCACHE
        #ifdef JIT_ENABLED
        //if (!NDS.IsJITEnabled())
        #endif  
        {
            if (IsAddressDCachable(addr))
            {
                DelayedQueue = &ARMv5::DCacheFin32;
                if (DCacheLookup(addr)) return;
            }
        }
    #endif

    QueueFunction(&ARMv5::DRead32_3);
}

void ARMv5::DRead32_3()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    // bus reads can only overlap with icache streaming by 6 cycles
    // checkme: does cache trigger this?
    if (ICacheStreamPtr < 7)
    {
        u64 time = ICacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }
    
    if (PU_Map[addr>>12] & 0x30)
        WriteBufferDrain();
    else
        WriteBufferCheck<1>();

    QueueFunction(&ARMv5::DRead32_4);
}

void ARMv5::DRead32_4()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
    
    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MR32;
        MRTrack.Progress = reg;

        LDRRegs &= ~1<<reg;
    }
    else
    {
        DataRegion = NDS.ARM9Regions[addr>>14];
        if ((NDS.ARM9Timestamp <= WBReleaseTS) && (DataRegion == WBLastRegion)) // check write buffer
            NDS.ARM9Timestamp += 1<<NDS.ARM9ClockShift;

        QueueFunction(&ARMv5::DRead32_5);
    }
}

void ARMv5::DRead32_5()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp += (4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift;
    
    *val = BusRead32(addr);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp += MemTimings[addr >> 14][1];
    DataCycles = 3<<NDS.ARM9ClockShift;
    
    if (WBTimestamp < ((NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    LDRRegs &= ~1<<reg;
}

bool ARMv5::DataRead32S(u32 addr, u8 reg)
{
    // Data Aborts
    // Exception is handled in the actual instruction implementation
    if (!(PU_Map[addr>>12] & CP15_MAP_READABLE)) [[unlikely]]
    {
        QueueFunction(&ARMv5::DAbortHandle);
        return false;
    }
    
    FetchAddr[reg] = addr;
    LDRRegs |= 1<<reg;

    QueueFunction(&ARMv5::DRead32S_2);
    return true;
}

void ARMv5::DRead32S_2()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    addr &= ~3;

    if (addr < ITCMSize)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        // we update the timestamp during the actual function, as a sequential itcm access can only occur during instructions with strange itcm wait cycles
        DataRegion = Mem9_ITCM;
        *val = *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        LDRRegs &= ~1<<reg;
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        DataRegion = Mem9_DTCM;
        *val = *(u32*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        LDRRegs &= ~1<<reg;
        return;
    }

    #if !DISABLE_DCACHE
        #ifdef JIT_ENABLED
        //if (!NDS.IsJITEnabled())
        #endif  
        {
            if (IsAddressDCachable(addr))
            {
                DelayedQueue = &ARMv5::DCacheFin32;
                if (DCacheLookup(addr)) return;
            }
        }
    #endif

    QueueFunction(&ARMv5::DRead32S_3);
}

void ARMv5::DRead32S_3()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    // bus reads can only overlap with icache streaming by 6 cycles
    // checkme: does cache trigger this?
    if (ICacheStreamPtr < 7)
    {
        u64 time = ICacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }
    
    if (PU_Map[addr>>12] & 0x30) // checkme
        WriteBufferDrain();
    else
        WriteBufferCheck<1>();

    QueueFunction(&ARMv5::DRead32S_4);
}

void ARMv5::DRead32S_4()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    // bursts cannot cross a 1kb boundary
    if (addr & 0x3FF) // s
    {
        if ((addr >> 24) == 0x02)
        {
            MRTrack.Type = MainRAMType::Fetch;
            MRTrack.Var = MR32 | MRSequential;
            MRTrack.Progress = reg;

            LDRRegs &= ~1<<reg;
        }
        else
        {
            NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

            DataRegion = NDS.ARM9Regions[addr>>14];
            if ((NDS.ARM9Timestamp <= WBReleaseTS) && (DataRegion == WBLastRegion)) // check write buffer
                NDS.ARM9Timestamp += 1<<NDS.ARM9ClockShift;

            QueueFunction(&ARMv5::DRead32S_5A);
        }
    }
    else // ns
    {
        if ((addr >> 24) == 0x02)
        {
            MRTrack.Type = MainRAMType::Fetch;
            MRTrack.Var = MR32;
            MRTrack.Progress = reg;

            LDRRegs &= ~1<<reg;
        }
        else
        {
            NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

            DataRegion = NDS.ARM9Regions[addr>>14];
            if ((NDS.ARM9Timestamp <= WBReleaseTS) && (DataRegion == WBLastRegion)) // check write buffer
                NDS.ARM9Timestamp += 1<<NDS.ARM9ClockShift;

            QueueFunction(&ARMv5::DRead32S_5B);
        }
    }
}

void ARMv5::DRead32S_5A()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
    
    *val = BusRead32(addr);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp += MemTimings[addr>>14][2];
    DataCycles = MemTimings[addr>>14][2];
    
    if (WBTimestamp < ((NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    LDRRegs &= ~1<<reg;
}

void ARMv5::DRead32S_5B()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];
    u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
    
    *val = BusRead32(addr);

    NDS.ARM9Timestamp += MemTimings[addr>>14][1];
    DataCycles = 3<<NDS.ARM9ClockShift;
    
    if (WBTimestamp < ((NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp - (3<<NDS.ARM9ClockShift) + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    LDRRegs &= ~1<<reg;
}

bool ARMv5::DataWrite8(u32 addr, u8 val, u8 reg)
{
    // Data Aborts
    // Exception is handled in the actual instruction implementation
    if (!(PU_Map[addr>>12] & CP15_MAP_WRITEABLE)) [[unlikely]]
    {
        QueueFunction(&ARMv5::DAbortHandle);
        return false;
    }

    FetchAddr[reg] = addr;
    STRRegs = 1<<reg;
    STRVal[reg] = val;

    QueueFunction(&ARMv5::DWrite8_2);
    return true;
}

void ARMv5::DWrite8_2()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u8 val = STRVal[reg];

    if (DCacheStreamPtr < 7)
    {
        u64 fillend = DCacheStreamTimes[6] + 1;
        if (NDS.ARM9Timestamp < fillend) NDS.ARM9Timestamp = fillend; // checkme: should this be data cycles?
        DCacheStreamPtr = 7;
    }

    if (addr < ITCMSize)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        // does not stall (for some reason?)
        DataRegion = Mem9_ITCM;
        *(u8*)&ITCM[addr & (ITCMPhysicalSize - 1)] = val;
#ifdef JIT_ENABLED
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
#endif
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        DataRegion = Mem9_DTCM;
        *(u8*)&DTCM[addr & (DTCMPhysicalSize - 1)] = val;
        return;
    }

    #if !DISABLE_DCACHE
        #ifdef JIT_ENABLED
        //if (!NDS.IsJITEnabled())
        #endif  
        {
            if (IsAddressDCachable(addr))
            {
                if (DCacheWrite8(addr, val))
                    return;
            }
        }
    #endif

    if (!(PU_Map[addr>>12] & (0x30)))
    {
        QueueFunction(&ARMv5::DWrite8_3);
    }
    else
    {
        if (WBDelay > NDS.ARM9Timestamp) NDS.ARM9Timestamp = WBDelay;

        WriteBufferWrite(addr, 4);
        WriteBufferWrite(val, 0, addr);
    }
}

void ARMv5::DWrite8_3()
{
    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    // bus reads can only overlap with icache streaming by 6 cycles
    // checkme: do buffered writes trigger this?
    if (ICacheStreamPtr < 7)
    {
        u64 time = ICacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }

    WriteBufferCheck<2>();
    QueueFunction(&ARMv5::DWrite8_4);
}

void ARMv5::DWrite8_4()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u8 val = STRVal[reg];

    NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MRWrite | MR8;
        MRTrack.Progress = reg;
    }
    else
    {
        QueueFunction(&ARMv5::DWrite8_5);
    }
}

void ARMv5::DWrite8_5()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u8 val = STRVal[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    NDS.ARM9Timestamp += ((4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift);
    NDS.DMA9Timestamp = NDS.ARM9Timestamp += MemTimings[addr >> 14][0] + 1;

    BusWrite8(addr, val);
    NDS.DMA9Timestamp = NDS.ARM9Timestamp -= 1;

    DataCycles = 3<<NDS.ARM9ClockShift;
    DataRegion = NDS.ARM9Regions[addr>>14];
        
    if (WBTimestamp < ((NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
}

bool ARMv5::DataWrite16(u32 addr, u16 val, u8 reg)
{
    // Data Aborts
    // Exception is handled in the actual instruction implementation
    if (!(PU_Map[addr>>12] & CP15_MAP_WRITEABLE)) [[unlikely]]
    {
        QueueFunction(&ARMv5::DAbortHandle);
        return false;
    }

    FetchAddr[reg] = addr;
    STRRegs = 1<<reg;
    STRVal[reg] = val;

    QueueFunction(&ARMv5::DWrite16_2);
    return true;
}

void ARMv5::DWrite16_2()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u16 val = STRVal[reg];

    if (DCacheStreamPtr < 7)
    {
        u64 fillend = DCacheStreamTimes[6] + 1;
        if (NDS.ARM9Timestamp < fillend) NDS.ARM9Timestamp = fillend; // checkme: should this be data cycles?
        DCacheStreamPtr = 7;
    }

    addr &= ~1;

    if (addr < ITCMSize)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        // does not stall (for some reason?)
        DataRegion = Mem9_ITCM;
        *(u16*)&ITCM[addr & (ITCMPhysicalSize - 1)] = val;
#ifdef JIT_ENABLED
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
#endif
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        DataRegion = Mem9_DTCM;
        *(u16*)&DTCM[addr & (DTCMPhysicalSize - 1)] = val;
        return;
    }

    #if !DISABLE_DCACHE
        #ifdef JIT_ENABLED
        //if (!NDS.IsJITEnabled())
        #endif  
        {
            if (IsAddressDCachable(addr))
            {
                if (DCacheWrite16(addr, val))
                    return;
            }
        }
    #endif

    if (!(PU_Map[addr>>12] & 0x30))
    {
        QueueFunction(&ARMv5::DWrite16_3);
    }
    else
    {
        if (WBDelay > NDS.ARM9Timestamp) NDS.ARM9Timestamp = WBDelay;

        WriteBufferWrite(addr, 4);
        WriteBufferWrite(val, 1, addr);
    }
}

void ARMv5::DWrite16_3()
{
    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    // bus reads can only overlap with icache streaming by 6 cycles
    // checkme: do buffered writes trigger this?
    if (ICacheStreamPtr < 7)
    {
        u64 time = ICacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }

    WriteBufferCheck<2>();
    QueueFunction(&ARMv5::DWrite16_4);
}

void ARMv5::DWrite16_4()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u16 val = STRVal[reg];

    NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MRWrite | MR16;
        MRTrack.Progress = reg;
    }
    else
    {
        QueueFunction(&ARMv5::DWrite16_5);
    }
}

void ARMv5::DWrite16_5()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u16 val = STRVal[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    NDS.ARM9Timestamp += ((4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift);
    NDS.DMA9Timestamp = NDS.ARM9Timestamp += MemTimings[addr >> 14][0] + 1;

    BusWrite16(addr, val);
    NDS.DMA9Timestamp = NDS.ARM9Timestamp -= 1;

    DataCycles = 3<<NDS.ARM9ClockShift;
    DataRegion = NDS.ARM9Regions[addr>>14];
        
    if (WBTimestamp < ((NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
}

bool ARMv5::DataWrite32(u32 addr, u32 val, u8 reg)
{
    // Data Aborts
    // Exception is handled in the actual instruction implementation
    if (!(PU_Map[addr>>12] & CP15_MAP_WRITEABLE)) [[unlikely]]
    {
        QueueFunction(&ARMv5::DAbortHandle);
        return false;
    }
    
    FetchAddr[reg] = addr;
    STRRegs = 1<<reg;
    STRVal[reg] = val;

    QueueFunction(&ARMv5::DWrite32_2);
    return true;
}

void ARMv5::DWrite32_2()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u32 val = STRVal[reg];

    if (DCacheStreamPtr < 7)
    {
        u64 fillend = DCacheStreamTimes[6] + 1;
        if (NDS.ARM9Timestamp < fillend) NDS.ARM9Timestamp = fillend; // checkme: should this be data cycles?
        DCacheStreamPtr = 7;
    }

    addr &= ~3;

    if (addr < ITCMSize)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        // does not stall (for some reason?)
        DataRegion = Mem9_ITCM;
        *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)] = val;
#ifdef JIT_ENABLED
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
#endif
        STRRegs &= ~1<<reg;
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        DataRegion = Mem9_DTCM;
        *(u32*)&DTCM[addr & (DTCMPhysicalSize - 1)] = val;
        STRRegs &= ~1<<reg;
        return;
    }

    #if !DISABLE_DCACHE
        #ifdef JIT_ENABLED
        //if (!NDS.IsJITEnabled())
        #endif  
        {
            if (IsAddressDCachable(addr))
            {
                if (DCacheWrite32(addr, val))
                {
                    STRRegs &= ~1<<reg;
                    return;
                }
            }
        }
    #endif

    if (!(PU_Map[addr>>12] & 0x30))
    {
        QueueFunction(&ARMv5::DWrite32_3);
    }
    else
    {
        if (WBDelay > NDS.ARM9Timestamp) NDS.ARM9Timestamp = WBDelay;

        WriteBufferWrite(addr, 4);
        WriteBufferWrite(val, 2, addr);
        STRRegs &= ~1<<reg;
    }
}

void ARMv5::DWrite32_3()
{
    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    // bus reads can only overlap with icache streaming by 6 cycles
    // checkme: do buffered writes trigger this?
    if (ICacheStreamPtr < 7)
    {
        u64 time = ICacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }

    WriteBufferCheck<2>();
    QueueFunction(&ARMv5::DWrite32_4);
}

void ARMv5::DWrite32_4()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u32 val = STRVal[reg];

    NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MRWrite | MR32;
        MRTrack.Progress = reg;
        
        STRRegs &= ~1<<reg;
    }
    else
    {
        QueueFunction(&ARMv5::DWrite32_5);
    }
}

void ARMv5::DWrite32_5()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u32 val = STRVal[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    NDS.ARM9Timestamp += ((4 - NDS.ARM9ClockShift) << NDS.ARM9ClockShift);
    NDS.DMA9Timestamp = NDS.ARM9Timestamp += MemTimings[addr >> 14][1] + 1;

    BusWrite32(addr, val);
    NDS.DMA9Timestamp = NDS.ARM9Timestamp -= 1;

    DataCycles = 3<<NDS.ARM9ClockShift;
    DataRegion = NDS.ARM9Regions[addr>>14];
        
    if (WBTimestamp < ((NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
    STRRegs &= ~1<<reg;
}

bool ARMv5::DataWrite32S(u32 addr, u32 val, u8 reg)
{
    // Data Aborts
    // Exception is handled in the actual instruction implementation
    if (!(PU_Map[addr>>12] & CP15_MAP_WRITEABLE)) [[unlikely]]
    {
        QueueFunction(&ARMv5::DAbortHandle);
        return false;
    }
    
    FetchAddr[reg] = addr;
    STRRegs |= 1<<reg;
    STRVal[reg] = val;

    QueueFunction(&ARMv5::DWrite32S_2);
    return true;
}

void ARMv5::DWrite32S_2()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u32 val = STRVal[reg];

    addr &= ~3;

    if (addr < ITCMSize)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        // we update the timestamp during the actual function, as a sequential itcm access can only occur during instructions with strange itcm wait cycles
        DataRegion = Mem9_ITCM;
        *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)] = val;
#ifdef JIT_ENABLED
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
#endif
        STRRegs &= ~1<<reg;
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        NDS.ARM9Timestamp += DataCycles = 1;
        DataRegion = Mem9_DTCM;
        *(u32*)&DTCM[addr & (DTCMPhysicalSize - 1)] = val;
        STRRegs &= ~1<<reg;
        return;
    }

    #if !DISABLE_DCACHE
        #ifdef JIT_ENABLED
        //if (!NDS.IsJITEnabled())
        #endif  
        {
            if (IsAddressDCachable(addr))
            {
                if (DCacheWrite32(addr, val))
                {
                    STRRegs &= ~1<<reg;
                    return;
                }
            }
        }
    #endif

    if (!(PU_Map[addr>>12] & 0x30)) // non-bufferable
    {
        QueueFunction(&ARMv5::DWrite32S_3);
    }
    else
    {
        WriteBufferWrite(val, 3, addr);
        STRRegs &= ~1<<reg;
    }
}

void ARMv5::DWrite32S_3()
{
    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = NDS.DMA9Timestamp;
    // bus reads can only overlap with icache streaming by 6 cycles
    // checkme: do buffered writes trigger this?
    if (ICacheStreamPtr < 7)
    {
        u64 time = ICacheStreamTimes[6] - 6; // checkme: minus 6?
        if (NDS.ARM9Timestamp < time) NDS.ARM9Timestamp = time;
    }
    WriteBufferCheck<2>();
    QueueFunction(&ARMv5::DWrite32S_4);
}

void ARMv5::DWrite32S_4()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u32 val = STRVal[reg];

    // bursts cannot cross a 1kb boundary
    if (addr & 0x3FF) // s
    {
        if ((addr >> 24) == 0x02)
        {
            MRTrack.Type = MainRAMType::Fetch;
            MRTrack.Var = MRWrite | MR32 | MRSequential;
            MRTrack.Progress = reg;

            STRRegs &= ~1<<reg;
        }
        else
        {
            NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
            QueueFunction(&ARMv5::DWrite32S_5A);
        }
    }
    else // ns
    {
        if ((addr >> 24) == 0x02)
        {
            MRTrack.Type = MainRAMType::Fetch;
            MRTrack.Var = MRWrite | MR32;
            MRTrack.Progress = reg;

            STRRegs &= ~1<<reg;
        }
        else
        {
            NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
            QueueFunction(&ARMv5::DWrite32S_5B);
        }
    }
}

void ARMv5::DWrite32S_5A()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u32 val = STRVal[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp += MemTimings[addr>>14][2] + 1;

    BusWrite32(addr, val);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp -= 1;

    DataRegion = NDS.ARM9Regions[addr>>14];

    if (WBTimestamp < ((NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    STRRegs &= ~1<<reg;
}

void ARMv5::DWrite32S_5B()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];
    u32 val = STRVal[reg];

    if (NDS.ARM9Timestamp < NDS.DMA9Timestamp) NDS.ARM9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);
    
    NDS.DMA9Timestamp = NDS.ARM9Timestamp += MemTimings[addr>>14][1] + 1;

    BusWrite32(addr, val);

    NDS.DMA9Timestamp = NDS.ARM9Timestamp -= 1;

    DataCycles = 3 << NDS.ARM9ClockShift; // checkme
    DataRegion = NDS.ARM9Regions[addr>>14];

    if (WBTimestamp < ((NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1)))
        WBTimestamp = (NDS.ARM9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    STRRegs &= ~1<<reg;
}

void ARMv5::GetCodeMemRegion(const u32 addr, MemRegion* region)
{
    NDS.ARM9GetMemRegion(addr, false, &CodeMem);
}

}
