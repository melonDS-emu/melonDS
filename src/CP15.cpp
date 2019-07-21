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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "ARM.h"
#include "ARMJIT.h"


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

    memset(ITCM, 0, 0x8000);
    memset(DTCM, 0, 0x4000);

    ITCMSize = 0;
    DTCMBase = 0xFFFFFFFF;
    DTCMSize = 0;

    memset(ICache, 0, 0x2000);
    ICacheInvalidateAll();
    memset(ICacheCount, 0, 64);

    PU_CodeCacheable = 0;
    PU_DataCacheable = 0;
    PU_DataCacheWrite = 0;

    PU_CodeRW = 0;
    PU_DataRW = 0;

    memset(PU_Region, 0, 8*sizeof(u32));
    UpdatePURegions();

    CurICacheLine = NULL;
}

void ARMv5::CP15DoSavestate(Savestate* file)
{
    file->Section("CP15");

    file->Var32(&CP15Control);

    file->Var32(&DTCMSetting);
    file->Var32(&ITCMSetting);

    file->VarArray(ITCM, 0x8000);
    file->VarArray(DTCM, 0x4000);

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
        UpdatePURegions();
    }
}


void ARMv5::UpdateDTCMSetting()
{
    if (CP15Control & (1<<16))
    {
        DTCMBase = DTCMSetting & 0xFFFFF000;
        DTCMSize = 0x200 << ((DTCMSetting >> 1) & 0x1F);
        //printf("DTCM [%08X] enabled at %08X, size %X\n", DTCMSetting, DTCMBase, DTCMSize);
    }
    else
    {
        DTCMBase = 0xFFFFFFFF;
        DTCMSize = 0;
        //printf("DTCM disabled\n");
    }
}

void ARMv5::UpdateITCMSetting()
{
    if (CP15Control & (1<<18))
    {
        ITCMSize = 0x200 << ((ITCMSetting >> 1) & 0x1F);
        //printf("ITCM [%08X] enabled at %08X, size %X\n", ITCMSetting, 0, ITCMSize);
    }
    else
    {
        ITCMSize = 0;
        //printf("ITCM disabled\n");
    }
}


void ARMv5::UpdatePURegions()
{
    if (!(CP15Control & (1<<0)))
    {
        // PU disabled

        u8 mask = 0x07;
        if (CP15Control & (1<<2))  mask |= 0x30;
        if (CP15Control & (1<<12)) mask |= 0x40;

        memset(PU_UserMap, mask, 0x100000);
        memset(PU_PrivMap, mask, 0x100000);

        return;
    }

    memset(PU_UserMap, 0, 0x100000);
    memset(PU_PrivMap, 0, 0x100000);

    u32 coderw = PU_CodeRW;
    u32 datarw = PU_DataRW;

    u32 codecache, datacache, datawrite;

    // datacache/datawrite
    // 0/0: goes to memory
    // 0/1: goes to memory
    // 1/0: goes to memory and cache
    // 1/1: goes to cache

    if (CP15Control & (1<<12))
        codecache = PU_CodeCacheable;
    else
        codecache = 0;

    if (CP15Control & (1<<2))
    {
        datacache = PU_DataCacheable;
        datawrite = PU_DataCacheWrite;
    }
    else
    {
        datacache = 0;
        datawrite = 0;
    }

    for (int n = 0; n < 8; n++)
    {
        u32 rgn = PU_Region[n];
        if (!(rgn & (1<<0)))
        {
            coderw >>= 4;
            datarw >>= 4;
            codecache >>= 1;
            datacache >>= 1;
            datawrite >>= 1;
            continue;
        }

        u32 start = rgn >> 12;
        u32 sz = 2 << ((rgn >> 1) & 0x1F);
        u32 end = start + (sz >> 12);
        // TODO: check alignment of start

        u8 usermask = 0;
        u8 privmask = 0;

        switch (datarw & 0xF)
        {
        case 0: break;
        case 1: privmask |= 0x03; break;
        case 2: privmask |= 0x03; usermask |= 0x01; break;
        case 3: privmask |= 0x03; usermask |= 0x03; break;
        case 5: privmask |= 0x01; break;
        case 6: privmask |= 0x01; usermask |= 0x01; break;
        default: printf("!! BAD DATARW VALUE %d\n", datarw&0xF);
        }

        switch (coderw & 0xF)
        {
        case 0: break;
        case 1: privmask |= 0x04; break;
        case 2: privmask |= 0x04; usermask |= 0x04; break;
        case 3: privmask |= 0x04; usermask |= 0x04; break;
        case 5: privmask |= 0x04; break;
        case 6: privmask |= 0x04; usermask |= 0x04; break;
        default: printf("!! BAD CODERW VALUE %d\n", datarw&0xF);
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

        printf("PU region %d: %08X-%08X, user=%02X priv=%02X\n", n, start<<12, end<<12, usermask, privmask);

        for (u32 i = start; i < end; i++)
        {
            PU_UserMap[i] = usermask;
            PU_PrivMap[i] = privmask;
        }

        coderw >>= 4;
        datarw >>= 4;
        codecache >>= 1;
        datacache >>= 1;
        datawrite >>= 1;

        // TODO: this will not be enough if they change their PU regions after the intial setup
        //UpdateRegionTimings(start<<12, end<<12);
    }

    // TODO: this is way unoptimized
    // should be okay unless the game keeps changing shit, tho
    UpdateRegionTimings(0x00000000, 0xFFFFFFFF);
}

void ARMv5::UpdateRegionTimings(u32 addrstart, u32 addrend)
{
    printf("initialising region timings %x %x\n", addrstart, addrend);
    addrstart >>= 12;
    addrend   >>= 12;


    if (addrend == 0xFFFFF) addrend++;

    for (u32 i = addrstart; i < addrend; i++)
    {
        u8 pu = PU_Map[i];
        u8* bustimings = NDS::ARM9MemTimings[i >> 2];

        if (pu & 0x40)
        {
            MemTimings[i][0] = 0xFF;//kCodeCacheTiming;
        }
        else
        {
            MemTimings[i][0] = bustimings[2] << NDS::ARM9ClockShift;
        }

        if (pu & 0x10)
        {
            MemTimings[i][1] = kDataCacheTiming;
            MemTimings[i][2] = kDataCacheTiming;
            MemTimings[i][3] = 1;
        }
        else
        {
            MemTimings[i][1] = bustimings[0] << NDS::ARM9ClockShift;
            MemTimings[i][2] = bustimings[2] << NDS::ARM9ClockShift;
            MemTimings[i][3] = bustimings[3] << NDS::ARM9ClockShift;
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

int zog=1;
void ARMv5::ICacheLookup(u32 addr)
{
    u32 tag = addr & 0xFFFFF800;
    u32 id = (addr >> 5) & 0x3F;

    id <<= 2;
    if (ICacheTags[id+0] == tag)
    {
        CodeCycles = 1;zog=1;
        CurICacheLine = &ICache[(id+0) << 5];
        return;
    }
    if (ICacheTags[id+1] == tag)
    {
        CodeCycles = 1;zog=2;
        CurICacheLine = &ICache[(id+1) << 5];
        return;
    }
    if (ICacheTags[id+2] == tag)
    {
        CodeCycles = 1;zog=3;
        CurICacheLine = &ICache[(id+2) << 5];
        return;
    }
    if (ICacheTags[id+3] == tag)
    {
        CodeCycles = 1;zog=4;
        CurICacheLine = &ICache[(id+3) << 5];
        return;
    }

    // cache miss

    u32 line;
    if (CP15Control & (1<<14))
    {
        line = ICacheCount[id>>2];
        ICacheCount[id>>2] = (line+1) & 0x3;
    }
    else
    {
        line = RandomLineIndex();
    }

    line += id;

    addr &= ~0x1F;
    u8* ptr = &ICache[line << 5];

    if (CodeMem.Mem)
    {
        memcpy(ptr, &CodeMem.Mem[addr & CodeMem.Mask], 32);
    }
    else
    {
        for (int i = 0; i < 32; i+=4)
            *(u32*)&ptr[i] = NDS::ARM9Read32(addr+i);
    }

    ICacheTags[line] = tag;

    // ouch :/
    //printf("cache miss %08X: %d/%d\n", addr, NDS::ARM9MemTimings[addr >> 14][2], NDS::ARM9MemTimings[addr >> 14][3]);
    CodeCycles = (NDS::ARM9MemTimings[addr >> 14][2] + (NDS::ARM9MemTimings[addr >> 14][3] * 7)) << NDS::ARM9ClockShift;
    CurICacheLine = ptr;
}

void ARMv5::ICacheInvalidateByAddr(u32 addr)
{
    u32 tag = addr & 0xFFFFF800;
    u32 id = (addr >> 5) & 0x3F;

    id <<= 2;
    if (ICacheTags[id+0] == tag)
    {
        ICacheTags[id+0] = 1;
        return;
    }
    if (ICacheTags[id+1] == tag)
    {
        ICacheTags[id+1] = 1;
        return;
    }
    if (ICacheTags[id+2] == tag)
    {
        ICacheTags[id+2] = 1;
        return;
    }
    if (ICacheTags[id+3] == tag)
    {
        ICacheTags[id+3] = 1;
        return;
    }
}

void ARMv5::ICacheInvalidateAll()
{
    for (int i = 0; i < 64*4; i++)
        ICacheTags[i] = 1;
}


void ARMv5::CP15Write(u32 id, u32 val)
{
    //printf("CP15 write op %03X %08X %08X\n", id, val, R[15]);

    switch (id)
    {
    case 0x100:
        {
            u32 old = CP15Control;
            val &= 0x000FF085;
            CP15Control &= ~0x000FF085;
            CP15Control |= val;
            printf("CP15Control = %08X (%08X->%08X)\n", CP15Control, old, val);
            UpdateDTCMSetting();
            UpdateITCMSetting();
            if ((old & 0x1005) != (val & 0x1005)) UpdatePURegions();
            if (val & (1<<7)) printf("!!!! ARM9 BIG ENDIAN MODE. VERY BAD. SHIT GONNA ASPLODE NOW\n");
            if (val & (1<<13)) ExceptionBase = 0xFFFF0000;
            else               ExceptionBase = 0x00000000;
        }
        return;


    case 0x200: // data cacheable
        PU_DataCacheable = val;
        printf("PU: DataCacheable=%08X\n", val);
        UpdatePURegions();
        return;

    case 0x201: // code cacheable
        PU_CodeCacheable = val;
        printf("PU: CodeCacheable=%08X\n", val);
        UpdatePURegions();
        return;


    case 0x300: // data cache write-buffer
        PU_DataCacheWrite = val;
        printf("PU: DataCacheWrite=%08X\n", val);
        UpdatePURegions();
        return;


    case 0x500: // legacy data permissions
        PU_DataRW = 0;
        PU_DataRW |= (val & 0x0003);
        PU_DataRW |= ((val & 0x000C) << 2);
        PU_DataRW |= ((val & 0x0030) << 4);
        PU_DataRW |= ((val & 0x00C0) << 6);
        PU_DataRW |= ((val & 0x0300) << 8);
        PU_DataRW |= ((val & 0x0C00) << 10);
        PU_DataRW |= ((val & 0x3000) << 12);
        PU_DataRW |= ((val & 0xC000) << 14);
        printf("PU: DataRW=%08X (legacy %08X)\n", PU_DataRW, val);
        UpdatePURegions();
        return;

    case 0x501: // legacy code permissions
        PU_CodeRW = 0;
        PU_CodeRW |= (val & 0x0003);
        PU_CodeRW |= ((val & 0x000C) << 2);
        PU_CodeRW |= ((val & 0x0030) << 4);
        PU_CodeRW |= ((val & 0x00C0) << 6);
        PU_CodeRW |= ((val & 0x0300) << 8);
        PU_CodeRW |= ((val & 0x0C00) << 10);
        PU_CodeRW |= ((val & 0x3000) << 12);
        PU_CodeRW |= ((val & 0xC000) << 14);
        printf("PU: CodeRW=%08X (legacy %08X)\n", PU_CodeRW, val);
        UpdatePURegions();
        return;

    case 0x502: // data permissions
        PU_DataRW = val;
        printf("PU: DataRW=%08X\n", PU_DataRW);
        UpdatePURegions();
        return;

    case 0x503: // code permissions
        PU_CodeRW = val;
        printf("PU: CodeRW=%08X\n", PU_CodeRW);
        UpdatePURegions();
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
        PU_Region[(id >> 4) & 0xF] = val;
        printf("PU: region %d = %08X : ", (id>>4)&0xF, val);
        printf("%s, ", val&1 ? "enabled":"disabled");
        printf("%08X-", val&0xFFFFF000);
        printf("%08X\n", (val&0xFFFFF000)+(2<<((val&0x3E)>>1)));
        UpdatePURegions();
        return;


    case 0x704:
    case 0x782:
        Halt(1);
        return;


    case 0x750:
        ICacheInvalidateAll();
        return;
    case 0x751:
        ICacheInvalidateByAddr(val);
        return;
    case 0x752:
        printf("CP15: ICACHE INVALIDATE WEIRD. %08X\n", val);
        return;


    case 0x761:
        //printf("inval data cache %08X\n", val);
        return;
    case 0x762:
        //printf("inval data cache SI\n");
        return;

    case 0x7A1:
        //printf("flush data cache %08X\n", val);
        return;
    case 0x7A2:
        //printf("flush data cache SI\n");
        return;


    case 0x910:
        DTCMSetting = val;
        UpdateDTCMSetting();
        return;

    case 0x911:
        ITCMSetting = val;
        UpdateITCMSetting();
        return;
    }

    if ((id&0xF00)!=0x700)
        printf("unknown CP15 write op %03X %08X\n", id, val);
}

u32 ARMv5::CP15Read(u32 id)
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
        return 0x0F0D2112;

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


    case 0x910:
        return DTCMSetting;
    case 0x911:
        return ITCMSetting;
    }

    printf("unknown CP15 read op %03X\n", id);
    return 0;
}


// TCM are handled here.
// TODO: later on, handle PU, and maybe caches

u32 ARMv5::CodeRead32(u32 addr, bool branch)
{
    if (addr < ITCMSize)
    {
        CodeCycles = 1;
        return *(u32*)&ITCM[addr & 0x7FFF];
    }

    CodeCycles = RegionCodeCycles;
    if (CodeCycles == 0xFF) // cached memory. hax
    {
        if (branch || !(addr & 0x1F))
            CodeCycles = kCodeCacheTiming;//ICacheLookup(addr);
        else
            CodeCycles = 1;

        //return *(u32*)&CurICacheLine[addr & 0x1C];
    }

    if (CodeMem.Mem) return *(u32*)&CodeMem.Mem[addr & CodeMem.Mask];

    return NDS::ARM9Read32(addr);
}


void ARMv5::DataRead8(u32 addr, u32* val)
{
    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *val = *(u8*)&ITCM[addr & 0x7FFF];
        return;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataCycles = 1;
        *val = *(u8*)&DTCM[(addr - DTCMBase) & 0x3FFF];
        return;
    }

    *val = NDS::ARM9Read8(addr);
    DataCycles = MemTimings[addr >> 12][1];
}

void ARMv5::DataRead16(u32 addr, u32* val)
{
    addr &= ~1;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *val = *(u16*)&ITCM[addr & 0x7FFF];
        return;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataCycles = 1;
        *val = *(u16*)&DTCM[(addr - DTCMBase) & 0x3FFF];
        return;
    }

    *val = NDS::ARM9Read16(addr);
    DataCycles = MemTimings[addr >> 12][1];
}

void ARMv5::DataRead32(u32 addr, u32* val)
{
    addr &= ~3;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *val = *(u32*)&ITCM[addr & 0x7FFF];
        return;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataCycles = 1;
        *val = *(u32*)&DTCM[(addr - DTCMBase) & 0x3FFF];
        return;
    }

    *val = NDS::ARM9Read32(addr);
    DataCycles = MemTimings[addr >> 12][2];
}

void ARMv5::DataRead32S(u32 addr, u32* val)
{
    addr &= ~3;

    if (addr < ITCMSize)
    {
        DataCycles += 1;
        *val = *(u32*)&ITCM[addr & 0x7FFF];
        return;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataCycles += 1;
        *val = *(u32*)&DTCM[(addr - DTCMBase) & 0x3FFF];
        return;
    }

    *val = NDS::ARM9Read32(addr);
    DataCycles += MemTimings[addr >> 12][3];
}

void ARMv5::DataWrite8(u32 addr, u8 val)
{
    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *(u8*)&ITCM[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
        ARMJIT::cache.ARM9_ITCM[(addr & 0x7FFF) >> 1] = NULL;
#endif
        return;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataCycles = 1;
        *(u8*)&DTCM[(addr - DTCMBase) & 0x3FFF] = val;
        return;
    }

    NDS::ARM9Write8(addr, val);
    DataCycles = MemTimings[addr >> 12][1];
}

void ARMv5::DataWrite16(u32 addr, u16 val)
{
    addr &= ~1;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *(u16*)&ITCM[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
        ARMJIT::cache.ARM9_ITCM[(addr & 0x7FFF) >> 1] = NULL;
#endif
        return;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataCycles = 1;
        *(u16*)&DTCM[(addr - DTCMBase) & 0x3FFF] = val;
        return;
    }

    NDS::ARM9Write16(addr, val);
    DataCycles = MemTimings[addr >> 12][1];
}

void ARMv5::DataWrite32(u32 addr, u32 val)
{
    addr &= ~3;

    if (addr < ITCMSize)
    {
        DataCycles = 1;
        *(u32*)&ITCM[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
        ARMJIT::cache.ARM9_ITCM[(addr & 0x7FFF) >> 1] = NULL;
        ARMJIT::cache.ARM9_ITCM[((addr + 2) & 0x7FFF) >> 1] = NULL;
#endif
        return;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataCycles = 1;
        *(u32*)&DTCM[(addr - DTCMBase) & 0x3FFF] = val;
        return;
    }

    NDS::ARM9Write32(addr, val);
    DataCycles = MemTimings[addr >> 12][2];
}

void ARMv5::DataWrite32S(u32 addr, u32 val)
{
    addr &= ~3;

    if (addr < ITCMSize)
    {
        DataCycles += 1;
        *(u32*)&ITCM[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
        ARMJIT::cache.ARM9_ITCM[(addr & 0x7FFF) >> 1] = NULL;
        ARMJIT::cache.ARM9_ITCM[((addr & 0x7FFF) >> 1) + 1] = NULL;
#endif
        return;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataCycles += 1;
        *(u32*)&DTCM[(addr - DTCMBase) & 0x3FFF] = val;
        return;
    }

    NDS::ARM9Write32(addr, val);
    DataCycles += MemTimings[addr >> 12][3];
}

void ARMv5::GetCodeMemRegion(u32 addr, NDS::MemRegion* region)
{
    /*if (addr < ITCMSize)
    {
        region->Mem = ITCM;
        region->Mask = 0x7FFF;
        return;
    }*/

    NDS::ARM9GetMemRegion(addr, false, &CodeMem);
}

