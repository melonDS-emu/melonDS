/*
    Copyright 2016-2023 melonDS team

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

#ifndef MELONDS_MEMCONSTANTS_H
#define MELONDS_MEMCONSTANTS_H

#include "types.h"

namespace melonDS
{
constexpr u32 MainRAMMaxSize = 0x1000000;
constexpr u32 SharedWRAMSize = 0x8000;
constexpr u32 ARM7WRAMSize = 0x10000;
constexpr u32 NWRAMSize = 0x40000;
constexpr u32 ARM9BIOSSize = 0x1000;
constexpr u32 ARM7BIOSSize = 0x4000;
constexpr u32 DSiBIOSSize = 0x10000;
constexpr u32 ITCMPhysicalSize = 0x8000;
constexpr u32 DTCMPhysicalSize = 0x4000;
constexpr u32 ARM7BIOSCRC32 = 0x1280f0d5;
constexpr u32 ARM9BIOSCRC32 = 0x2ab23573;

constexpr u32 ICACHE_SIZE_LOG2 = 13;
constexpr u32 ICACHE_SIZE = 1 << ICACHE_SIZE_LOG2;
constexpr u32 ICACHE_SETS_LOG2 = 2;
constexpr u32 ICACHE_SETS = 1 << ICACHE_SETS_LOG2;
constexpr u32 ICACHE_LINELENGTH_ENCODED = 2; 
constexpr u32 ICACHE_LINELENGTH_LOG2 = ICACHE_LINELENGTH_ENCODED + 3; 
constexpr u32 ICACHE_LINELENGTH = 8 * (1 << ICACHE_LINELENGTH_ENCODED); 
constexpr u32 ICACHE_LINESPERSET = ICACHE_SIZE / (ICACHE_SETS * ICACHE_LINELENGTH);

constexpr u32 CP15_CR_MPUENABLE = (1 << 0);
constexpr u32 CP15_CR_BIGENDIAN = (1 << 7);
constexpr u32 CP15_CR_HIGHEXCEPTIONBASE = (1 << 13);
constexpr u32 CP15_CACHE_CR_ROUNDROBIN = (1 << 14);
constexpr u32 CP15_CACHE_CR_ICACHEENABLE = (1 << 12);
constexpr u32 CP15_CACHE_CR_DCACHEENABLE = (1 << 2);
constexpr u32 CP15_CACHE_CR_WRITEBUFFERENABLE = (1 << 3);
constexpr u32 CP15_TCM_CR_DTCM_ENABLE = (1 << 16);
constexpr u32 CP15_TCM_CR_ITCM_ENABLE = (1 << 18);

}

#endif // MELONDS_MEMCONSTANTS_H