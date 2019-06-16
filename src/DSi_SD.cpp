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
#include "DSi.h"
#include "DSi_SD.h"


DSi_SD::DSi_SD(u32 num)
{
    Num = num;
}

DSi_SD::~DSi_SD()
{
    //
}

void DSi_SD::Reset()
{
    if (Num == 0)
    {
        PortSelect = 0x0200; // CHECKME
    }
    else
    {
        PortSelect = 0x0100; // CHECKME
    }
}

void DSi_SD::DoSavestate(Savestate* file)
{
    // TODO!
}


u16 DSi_SD::Read(u32 addr)
{
    switch (addr & 0x1FF)
    {
    case 0x002: return PortSelect & 0x030F;
    }

    printf("unknown %s read %08X\n", Num?"SDIO":"SD/MMC", addr);
    return 0;
}

void DSi_SD::Write(u32 addr, u16 val)
{
    switch (addr & 0x1FF)
    {
    case 0x002: PortSelect = val; return;
    }

    printf("unknown %s write %08X %04X\n", Num?"SDIO":"SD/MMC", addr, val);
}
