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
#include "Wifi.h"


namespace Wifi
{

u16 BBCnt;
u8 BBWrite;
u8 BBRegs[0x100];
u8 BBRegsRO[0x100];


void Reset()
{
    BBCnt = 0;
    BBWrite = 0;
    memset(BBRegs, 0, 0x100);
    memset(BBRegsRO, 0, 0x100);

    #define BBREG_FIXED(id, val)  BBRegs[id] = val; BBRegsRO[id] = 1;
    BBREG_FIXED(0x00, 0x6D);
    BBREG_FIXED(0x0D, 0x00);
    BBREG_FIXED(0x0E, 0x00);
    BBREG_FIXED(0x0F, 0x00);
    BBREG_FIXED(0x10, 0x00);
    BBREG_FIXED(0x11, 0x00);
    BBREG_FIXED(0x12, 0x00);
    BBREG_FIXED(0x16, 0x00);
    BBREG_FIXED(0x17, 0x00);
    BBREG_FIXED(0x18, 0x00);
    BBREG_FIXED(0x19, 0x00);
    BBREG_FIXED(0x1A, 0x00);
    BBREG_FIXED(0x27, 0x00);
    BBREG_FIXED(0x4D, 0x00); // 00 or BF
    BBREG_FIXED(0x5D, 0x01);
    BBREG_FIXED(0x5E, 0x00);
    BBREG_FIXED(0x5F, 0x00);
    BBREG_FIXED(0x60, 0x00);
    BBREG_FIXED(0x61, 0x00);
    BBREG_FIXED(0x64, 0xFF); // FF or 3F
    BBREG_FIXED(0x66, 0x00);
    for (int i = 0x69; i < 0x100; i++)
    {
        BBREG_FIXED(i, 0x00);
    }
    #undef BBREG_FIXED
}


u16 Read(u32 addr)
{
    addr &= 0x7FFF;

    switch (addr)
    {
    case 0x158:
        return BBCnt;

    case 0x15C:
        if ((BBCnt & 0xF000) != 0x6000)
        {
            printf("WIFI: bad BB read, CNT=%04X\n", BBCnt);
            return 0;
        }
        return BBRegs[BBCnt & 0xFF];

    case 0x15E:
        return 0; // cheap
    }

    printf("WIFI: unknown read %08X\n", addr);
    return 0;
}

void Write(u32 addr, u16 val)
{
    addr &= 0x7FFF;

    switch (addr)
    {
    case 0x158:
        BBCnt = val;
        if ((BBCnt & 0xF000) == 0x5000)
        {
            u32 regid = BBCnt & 0xFF;
            if (!BBRegsRO[regid])
                BBRegs[regid] = val & 0xFF;
        }
        return;

    case 0x15A:
        BBWrite = val;
        return;
    }

    printf("WIFI: unknown write %08X %04X\n", addr, val);
}

}
