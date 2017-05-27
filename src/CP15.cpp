/*
    Copyright 2016-2017 StapleButter

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
#include "CP15.h"


// derp
namespace NDS
{
extern ARM* ARM9;
}

namespace CP15
{

u32 Control;

u32 DTCMSetting, ITCMSetting;

u8 ITCM[0x8000];
u32 ITCMSize;
u8 DTCM[0x4000];
u32 DTCMBase, DTCMSize;


void Reset()
{
    Control = 0x78; // dunno

    DTCMSetting = 0;
    ITCMSetting = 0;

    memset(ITCM, 0, 0x8000);
    memset(DTCM, 0, 0x4000);

    ITCMSize = 0;
    DTCMBase = 0xFFFFFFFF;
    DTCMSize = 0;
}


void UpdateDTCMSetting()
{
    if (Control & (1<<16))
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

void UpdateITCMSetting()
{
    if (Control & (1<<18))
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


void Write(u32 id, u32 val)
{
    //printf("CP15 write op %03X %08X %08X\n", id, val, NDS::ARM9->R[15]);

    switch (id)
    {
    case 0x100:
        val &= 0x000FF085;
        Control &= ~0x000FF085;
        Control |= val;
        UpdateDTCMSetting();
        UpdateITCMSetting();
        return;


    case 0x704:
    case 0x782:
        NDS::ARM9->Halt(1);
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

u32 Read(u32 id)
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
        return Control;


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

bool HandleCodeRead16(u32 addr, u16* val)
{
    if (addr < ITCMSize)
    {
        *val = *(u16*)&ITCM[addr & 0x7FFF];
        return true;
    }

    return false;
}

bool HandleCodeRead32(u32 addr, u32* val)
{
    if (addr < ITCMSize)
    {
        *val = *(u32*)&ITCM[addr & 0x7FFF];
        return true;
    }

    return false;
}


bool HandleDataRead8(u32 addr, u8* val, u32 forceuser)
{
    if (addr < ITCMSize)
    {
        *val = *(u8*)&ITCM[addr & 0x7FFF];
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        *val = *(u8*)&DTCM[(addr - DTCMBase) & 0x3FFF];
        return true;
    }

    return false;
}

bool HandleDataRead16(u32 addr, u16* val, u32 forceuser)
{
    if (addr < ITCMSize)
    {
        *val = *(u16*)&ITCM[addr & 0x7FFF];
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        *val = *(u16*)&DTCM[(addr - DTCMBase) & 0x3FFF];
        return true;
    }

    return false;
}

bool HandleDataRead32(u32 addr, u32* val, u32 forceuser)
{
    if (addr < ITCMSize)
    {
        *val = *(u32*)&ITCM[addr & 0x7FFF];
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        *val = *(u32*)&DTCM[(addr - DTCMBase) & 0x3FFF];
        return true;
    }

    return false;
}

bool HandleDataWrite8(u32 addr, u8 val, u32 forceuser)
{
    if (addr < ITCMSize)
    {
        *(u8*)&ITCM[addr & 0x7FFF] = val;
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        *(u8*)&DTCM[(addr - DTCMBase) & 0x3FFF] = val;
        return true;
    }

    return false;
}

bool HandleDataWrite16(u32 addr, u16 val, u32 forceuser)
{
    if (addr < ITCMSize)
    {
        *(u16*)&ITCM[addr & 0x7FFF] = val;
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        *(u16*)&DTCM[(addr - DTCMBase) & 0x3FFF] = val;
        return true;
    }

    return false;
}

bool HandleDataWrite32(u32 addr, u32 val, u32 forceuser)
{
    if (addr < ITCMSize)
    {
        *(u32*)&ITCM[addr & 0x7FFF] = val;
        return true;
    }
    if (addr >= DTCMBase && addr < (DTCMBase + DTCMSize))
    {
        *(u32*)&DTCM[(addr - DTCMBase) & 0x3FFF] = val;
        return true;
    }

    return false;
}

}
