#include <stdio.h>
#include "NDS.h"

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
        NDS::ARM9DTCMSize = 256 << (DTCMSetting & 0x3E);
        printf("DTCM enabled at %08X, size %X\n", NDS::ARM9DTCMBase, NDS::ARM9DTCMSize);
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
        NDS::ARM9ITCMSize = 256 << (DTCMSetting & 0x3E);
        printf("ITCM enabled at %08X, size %X\n", 0, NDS::ARM9DTCMSize);
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
