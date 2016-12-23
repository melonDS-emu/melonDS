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


void Reset()
{
    Control = 0x78; // dunno

    DTCMSetting = 0;
    ITCMSetting = 0;
}


void UpdateDTCMSetting()
{
    if (Control & (1<<16))
    {
        NDS::ARM9DTCMBase = DTCMSetting & 0xFFFFF000;
        NDS::ARM9DTCMSize = 0x200 << ((DTCMSetting >> 1) & 0x1F);
        printf("DTCM [%08X] enabled at %08X, size %X\n", DTCMSetting, NDS::ARM9DTCMBase, NDS::ARM9DTCMSize);
    }
    else
    {
        NDS::ARM9DTCMBase = 0xFFFFFFFF;
        NDS::ARM9DTCMSize = 0;
        printf("DTCM disabled\n");
    }
}

void UpdateITCMSetting()
{
    if (Control & (1<<18))
    {
        NDS::ARM9ITCMSize = 0x200 << ((ITCMSetting >> 1) & 0x1F);
        printf("ITCM [%08X] enabled at %08X, size %X\n", ITCMSetting, 0, NDS::ARM9ITCMSize);
    }
    else
    {
        NDS::ARM9ITCMSize = 0;
        printf("ITCM disabled\n");
    }
}


void Write(u32 id, u32 val)
{
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


    case 0x910:
        DTCMSetting = val;
        UpdateDTCMSetting();
        return;
    case 0x911:
        ITCMSetting = val;
        UpdateITCMSetting();
        return;
    }
}

u32 Read(u32 id)
{
    switch (id)
    {
    case 0x000: // CPU ID
    case 0x003:
    case 0x004:
    case 0x005:
    case 0x006:
    case 0x007:
        return 0x41059461;

    case 0x001:
        // cache type. todo
        return 0;

    case 0x002: // TCM size
        return (6 << 6) | (5 << 18);


    case 0x100: // control reg
        return Control;


    case 0x910:
        return DTCMSetting;
    case 0x911:
        return ITCMSetting;
    }

    return 0;
}

}
