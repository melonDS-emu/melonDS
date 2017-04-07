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
#include "SPI.h"
#include "Wifi.h"


namespace Wifi
{

u8 RAM[0x2000];

u16 Random;

u16 BBCnt;
u8 BBWrite;
u8 BBRegs[0x100];
u8 BBRegsRO[0x100];

u8 RFVersion;
u16 RFCnt;
u16 RFData1;
u16 RFData2;
u32 RFRegs[0x40];


void Reset()
{
    memset(RAM, 0, 0x2000);

    Random = 0x7FF;

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

    RFVersion = SPI_Firmware::GetRFVersion();
    RFCnt = 0;
    RFData1 = 0;
    RFData2 = 0;
    memset(RFRegs, 0, 4*0x40);
}


void RFTransfer_Type2()
{
    u32 id = (RFData2 >> 2) & 0x1F;

    if (RFData2 & 0x0080)
    {
        u32 data = RFRegs[id];
        RFData1 = data & 0xFFFF;
        RFData2 = (RFData2 & 0xFFFC) | ((data >> 16) & 0x3);
    }
    else
    {
        u32 data = RFData1 | ((RFData2 & 0x0003) << 16);
        RFRegs[id] = data;
    }
}

void RFTransfer_Type3()
{
    u32 id = (RFData1 >> 8) & 0x3F;

    u32 cmd = RFData2 & 0xF;
    if (cmd == 6)
    {
        RFData1 = (RFData1 & 0xFF00) | (RFRegs[id] & 0xFF);
    }
    else if (cmd == 5)
    {
        u32 data = RFData1 & 0xFF;
        RFRegs[id] = data;
    }
}


// TODO: wifi waitstates

u16 Read(u32 addr)
{
    addr &= 0x7FFF;

    if (addr >= 0x4000 && addr < 0x6000)
    {
        return *(u16*)&RAM[addr & 0x1FFF];
    }

    switch (addr)
    {
    case 0x044: // random generator. not accurate
        Random = (Random & 0x1) ^ (((Random & 0x3FF) << 1) | (Random >> 10));
        return Random;

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
        return 0; // TODO eventually (BB busy flag)

    case 0x17C:
        return RFData2;
    case 0x17E:
        return RFData1;
    case 0x180:
        return 0; // TODO eventually (RF busy flag)
    case 0x184:
        return RFCnt;
    }

    printf("WIFI: unknown read %08X\n", addr);
    return 0;
}

void Write(u32 addr, u16 val)
{
    addr &= 0x7FFF;

    if (addr >= 0x4000 && addr < 0x6000)
    {
        *(u16*)&RAM[addr & 0x1FFF] = val;
        return;
    }

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

    case 0x17C:
        RFData2 = val;
        if (RFVersion == 3) RFTransfer_Type3();
        else                RFTransfer_Type2();
        return;
    case 0x17E:
        RFData1 = val;
        return;
    case 0x184:
        RFCnt = val & 0x413F;
        return;
    }

    printf("WIFI: unknown write %08X %04X\n", addr, val);
}

}
