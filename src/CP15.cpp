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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "ARM.h"



void ARMv5::CP15Reset()
{
    CP15Control = 0x78; // dunno

    DTCMSetting = 0;
    ITCMSetting = 0;

    memset(ITCM, 0, 0x8000);
    memset(DTCM, 0, 0x4000);

    ITCMSize = 0;
    DTCMBase = 0xFFFFFFFF;
    DTCMSize = 0;
}

void ARMv5::CP15DoSavestate(Savestate* file)
{
    file->Section("CP15");

    file->Var32(&CP15Control);

    file->Var32(&DTCMSetting);
    file->Var32(&ITCMSetting);

    if (!file->Saving)
    {
        UpdateDTCMSetting();
        UpdateITCMSetting();
    }

    file->VarArray(ITCM, 0x8000);
    file->VarArray(DTCM, 0x4000);
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


void ARMv5::CP15Write(u32 id, u32 val)
{
    //printf("CP15 write op %03X %08X %08X\n", id, val, NDS::ARM9->R[15]);

    switch (id)
    {
    case 0x100:
        val &= 0x000FF085;
        CP15Control &= ~0x000FF085;
        CP15Control |= val;
        UpdateDTCMSetting();
        UpdateITCMSetting();
        return;


    case 0x200: // data cacheable
        PU_DataCacheable = val;
        printf("PU: DataCacheable=%08X\n", val);
        return;

    case 0x201: // code cacheable
        PU_CodeCacheable = val;
        printf("PU: CodeCacheable=%08X\n", val);
        return;


    case 0x300: // data cache write-buffer
        PU_DataCacheWrite = val;
        printf("PU: DataCacheWrite=%08X\n", val);
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
        return;

    case 0x502: // data permissions
        PU_DataRW = val;
        printf("PU: DataRW=%08X\n", PU_DataRW);
        return;

    case 0x503: // code permissions
        PU_CodeRW = val;
        printf("PU: CodeRW=%08X\n", PU_CodeRW);
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
        return;


    case 0x704:
    case 0x782:
        Halt(1);
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

u32 ARMv5::CodeRead32(u32 addr)
{
    // PU/cache check here

    if (addr < ITCMSize)
    {
        CodeRegion = NDS::Region9_ITCM;
        return *(u32*)&ITCM[addr & 0x7FFF];
    }

    u32 ret;
    CodeRegion = NDS::ARM9Read32(addr, &ret);
    return ret;
}


bool ARMv5::DataRead8(u32 addr, u32* val, u32 flags)
{
    // PU/cache check here

    if (addr < ITCMSize)
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *val = *(u8*)&ITCM[addr & 0x7FFF];
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *val = *(u8*)&DTCM[(addr - DTCMBase) & 0x3FFF];
        return true;
    }

    DataRegion = NDS::ARM9Read8(addr, val);
    if (flags & RWFlags_Nonseq)
        DataCycles = NDS::ARM9MemTimings[DataRegion][0];
    else
        DataCycles += NDS::ARM9MemTimings[DataRegion][1];
    return true;
}

bool ARMv5::DataRead16(u32 addr, u32* val, u32 flags)
{
    addr &= ~1;

    // PU/cache check here

    if (addr < ITCMSize)
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *val = *(u16*)&ITCM[addr & 0x7FFF];
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *val = *(u16*)&DTCM[(addr - DTCMBase) & 0x3FFF];
        return true;
    }

    DataRegion = NDS::ARM9Read16(addr, val);
    if (flags & RWFlags_Nonseq)
        DataCycles = NDS::ARM9MemTimings[DataRegion][0];
    else
        DataCycles += NDS::ARM9MemTimings[DataRegion][1];
    return true;
}

bool ARMv5::DataRead32(u32 addr, u32* val, u32 flags)
{
    addr &= ~3;

    // PU/cache check here

    if (addr < ITCMSize)
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *val = *(u32*)&ITCM[addr & 0x7FFF];
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *val = *(u32*)&DTCM[(addr - DTCMBase) & 0x3FFF];
        return true;
    }

    DataRegion = NDS::ARM9Read32(addr, val);
    if (flags & RWFlags_Nonseq)
        DataCycles = NDS::ARM9MemTimings[DataRegion][2];
    else
        DataCycles += NDS::ARM9MemTimings[DataRegion][3];
    return true;
}

bool ARMv5::DataWrite8(u32 addr, u8 val, u32 flags)
{
    // PU/cache check here

    if (addr < ITCMSize)
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *(u8*)&ITCM[addr & 0x7FFF] = val;
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *(u8*)&DTCM[(addr - DTCMBase) & 0x3FFF] = val;
        return true;
    }

    DataRegion = NDS::ARM9Write8(addr, val);
    if (flags & RWFlags_Nonseq)
        DataCycles = NDS::ARM9MemTimings[DataRegion][0];
    else
        DataCycles += NDS::ARM9MemTimings[DataRegion][1];
    return true;
}

bool ARMv5::DataWrite16(u32 addr, u16 val, u32 flags)
{
    addr &= ~1;

    // PU/cache check here

    if (addr < ITCMSize)
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *(u16*)&ITCM[addr & 0x7FFF] = val;
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *(u16*)&DTCM[(addr - DTCMBase) & 0x3FFF] = val;
        return true;
    }

    DataRegion = NDS::ARM9Write16(addr, val);
    if (flags & RWFlags_Nonseq)
        DataCycles = NDS::ARM9MemTimings[DataRegion][0];
    else
        DataCycles += NDS::ARM9MemTimings[DataRegion][1];
    return true;
}

bool ARMv5::DataWrite32(u32 addr, u32 val, u32 flags)
{
    addr &= ~3;

    // PU/cache check here

    if (addr < ITCMSize)
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *(u32*)&ITCM[addr & 0x7FFF] = val;
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        DataRegion = NDS::Region9_ITCM;
        DataCycles += 1;
        *(u32*)&DTCM[(addr - DTCMBase) & 0x3FFF] = val;
        return true;
    }

    DataRegion = NDS::ARM9Write32(addr, val);
    if (flags & RWFlags_Nonseq)
        DataCycles = NDS::ARM9MemTimings[DataRegion][2];
    else
        DataCycles += NDS::ARM9MemTimings[DataRegion][3];
    return true;
}

void ARMv5::GetCodeMemRegion(u32 addr, NDS::MemRegion* region)
{
    if (addr < ITCMSize)
    {
        region->Region = NDS::Region9_ITCM;
        region->Mem = ITCM;
        region->Mask = 0x7FFF;
        return;
    }

    NDS::ARM9GetMemRegion(addr, false, &CodeMem);
}

