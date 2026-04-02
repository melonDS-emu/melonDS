/*
    Copyright 2016-2026 melonDS team

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

#ifndef MELONDS_MEMREGION_H
#define MELONDS_MEMREGION_H

#include "types.h"

// this file exists to break #include cycle loops
namespace melonDS
{

enum
{
    Mem9_ITCM       = 0x00000001,
    Mem9_DTCM       = 0x00000002,
    Mem9_BIOS       = 0x00000004,
    Mem9_MainRAM    = 0x00000008,
    Mem9_WRAM       = 0x00000010,
    Mem9_IO         = 0x00000020,
    Mem9_Pal        = 0x00000040,
    Mem9_OAM        = 0x00000080,
    Mem9_VRAM       = 0x00000100,
    Mem9_GBAROM     = 0x00020000,
    Mem9_GBARAM     = 0x00040000,

    Mem7_BIOS       = 0x00000001,
    Mem7_MainRAM    = 0x00000002,
    Mem7_WRAM       = 0x00000004,
    Mem7_IO         = 0x00000008,
    Mem7_Wifi0      = 0x00000010,
    Mem7_Wifi1      = 0x00000020,
    Mem7_VRAM_C     = 0x00000040,
    Mem7_VRAM_D     = 0x00000080,
    Mem7_GBAROM     = 0x00000100,
    Mem7_GBARAM     = 0x00000200,

    // TODO: add DSi regions!
};

struct MemInfo
{
    u32 Region = 0;
    u8 Cycles_N16 = 1;
    u8 Cycles_S16 = 1;
    u8 Cycles_N32 = 1;
    u8 Cycles_S32 = 1;
};

struct MemRegion
{
    u8* Mem;
    u32 Mask;
};

}
#endif //MELONDS_MEMREGION_H
