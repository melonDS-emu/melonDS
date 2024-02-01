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

#ifndef MELONDS_CP15CONSTANTS_H
#define MELONDS_CP15CONSTANTS_H

#include "types.h"

namespace melonDS
{

/* ICACHE Layout constants */
constexpr u32 ICACHE_SIZE_LOG2 = 13;
constexpr u32 ICACHE_SIZE = 1 << ICACHE_SIZE_LOG2;
constexpr u32 ICACHE_SETS_LOG2 = 2;
constexpr u32 ICACHE_SETS = 1 << ICACHE_SETS_LOG2;
constexpr u32 ICACHE_LINELENGTH_ENCODED = 2; 
constexpr u32 ICACHE_LINELENGTH_LOG2 = ICACHE_LINELENGTH_ENCODED + 3; 
constexpr u32 ICACHE_LINELENGTH = 8 * (1 << ICACHE_LINELENGTH_ENCODED); 
constexpr u32 ICACHE_LINESPERSET = ICACHE_SIZE / (ICACHE_SETS * ICACHE_LINELENGTH);

/* DCACHE Layout constants */
constexpr u32 DCACHE_SIZE_LOG2 = 12;
constexpr u32 DCACHE_SIZE = 1 << DCACHE_SIZE_LOG2;
constexpr u32 DCACHE_SETS_LOG2 = 2;
constexpr u32 DCACHE_SETS = 1 << DCACHE_SETS_LOG2;
constexpr u32 DCACHE_LINELENGTH_ENCODED = 2; 
constexpr u32 DCACHE_LINELENGTH_LOG2 = DCACHE_LINELENGTH_ENCODED + 3; 
constexpr u32 DCACHE_LINELENGTH = 8 * (1 << DCACHE_LINELENGTH_ENCODED); 
constexpr u32 DCACHE_LINESPERSET = DCACHE_SIZE / (DCACHE_SETS * DCACHE_LINELENGTH);

/* CP15 Cache Data TAGs */
constexpr u32 CACHE_FLAG_VALID = (1 << 4);
constexpr u32 CACHE_FLAG_DIRTY_LOWERHALF = (1 << 2);
constexpr u32 CACHE_FLAG_DIRTY_UPPERHALF = (1 << 3);
constexpr u32 CACHE_FLAG_DIRTY_MASK = (3 << 2);
constexpr u32 CACHE_FLAG_SET_MASK = (3 << 0);

/* CP15 Cache Type Register */
constexpr u32 CACHE_TR_LOCKDOWN_TYPE_B = (7 << 25);
constexpr u32 CACHE_TR_NONUNIFIED = (1 << 24);

/* CP15 I/DCache LockDown registers */
constexpr u32 CACHE_LOCKUP_L = (1 << 31);

/* CP15 Main ID register */
constexpr u32 CP15_MAINID_IMPLEMENTOR_ARM = (0x41 << 24);
constexpr u32 CP15_MAINID_IMPLEMENTOR_DEC = (0x44 << 24);
constexpr u32 CP15_MAINID_IMPLEMENTOR_MOTOROLA = (0x4D << 24);
constexpr u32 CP15_MAINID_IMPLEMENTOR_MARVELL = (0x56 << 24);
constexpr u32 CP15_MAINID_IMPLEMENTOR_INTEL = (0x69 << 24);
constexpr u32 CP15_MAINID_VARIANT_0 = (0 << 20);
constexpr u32 CP15_MAINID_ARCH_v4 = (1 << 16);
constexpr u32 CP15_MAINID_ARCH_v4T = (2 << 16);
constexpr u32 CP15_MAINID_ARCH_v5 = (3 << 16);
constexpr u32 CP15_MAINID_ARCH_v5T = (4 << 16);
constexpr u32 CP15_MAINID_ARCH_v5TE = (5 << 16);
constexpr u32 CP15_MAINID_ARCH_v5TEJ = (6 << 16);
constexpr u32 CP15_MAINID_ARCH_v6 = (7 << 16);
constexpr u32 CP15_MAINID_IMPLEMENTATION_946 = (0x946 << 4);
constexpr u32 CP15_MAINID_REVISION_0 = (0 << 0);
constexpr u32 CP15_MAINID_REVISION_1 = (1 << 0);

/* CP15 Control Register */
constexpr u32 CP15_CR_MPUENABLE = (1 << 0);
constexpr u32 CP15_CR_BIGENDIAN = (1 << 7);
constexpr u32 CP15_CR_HIGHEXCEPTIONBASE = (1 << 13);

/* CP15 Internal Exception base value */
constexpr u32 CP15_EXCEPTIONBASE_HIGH = 0xFFFF0000;
constexpr u32 CP15_EXCEPTIONBASE_LOW = 0x00000000;

/* CP15 Cache and Write Buffer Conrol Register */
constexpr u32 CP15_CACHE_CR_ROUNDROBIN = (1 << 14);
constexpr u32 CP15_CACHE_CR_ICACHEENABLE = (1 << 12);
constexpr u32 CP15_CACHE_CR_DCACHEENABLE = (1 << 2);
constexpr u32 CP15_CACHE_CR_WRITEBUFFERENABLE = (1 << 3);

/* CP15 TCM Control Register */
constexpr u32 CP15_TCM_CR_DTCM_ENABLE = (1 << 16);
constexpr u32 CP15_TCM_CR_ITCM_ENABLE = (1 << 18);

/* CP15 Region Base and Size Register */
constexpr u32 CP15_REGION_COUNT = 8;
constexpr u32 CP15_REGION_ENABLE = (1 << 0);
constexpr u32 CP15_REGION_SIZE_MASK = (0x1F << 1);
constexpr u32 CP15_REGION_BASE_GRANULARITY_LOG2 = 12;
constexpr u32 CP15_REGION_BASE_GRANULARITY = (1 << CP15_REGION_BASE_GRANULARITY_LOG2);
constexpr u32 CP15_REGION_BASE_MASK = ~(CP15_REGION_BASE_GRANULARITY_LOG2-1);

/* CP15 Region access mask registers */
constexpr u32 CP15_REGIONACCESS_BITS_PER_REGION = 4;
constexpr u32 CP15_REGIONACCESS_REGIONMASK = (1 << CP15_REGIONACCESS_BITS_PER_REGION) - 1;

/* Flags in the melonDS internal PU_PrivMap and PU_UserMap */
constexpr u32 CP15_MAP_NOACCESS = 0x00;
constexpr u32 CP15_MAP_READABLE = 0x01;
constexpr u32 CP15_MAP_WRITEABLE = 0x02;
constexpr u32 CP15_MAP_EXECUTABLE = 0x04;
constexpr u32 CP15_MAP_DCACHEABLE = 0x10;
constexpr u32 CP15_MAP_DCACHEWRITEBACK = 0x20;
constexpr u32 CP15_MAP_ICACHEABLE = 0x40;

constexpr u32 CP15_MAP_ENTRYSIZE_LOG2 = CP15_REGION_BASE_GRANULARITY_LOG2;
constexpr u32 CP15_MAP_ENTRYSIZE = (1 << CP15_MAP_ENTRYSIZE_LOG2); 

/* Internal Timing Constants */
constexpr u32 BUSCYCLES_N16 = 0;
constexpr u32 BUSCYCLES_S16 = 1;
constexpr u32 BUSCYCLES_N32 = 2;
constexpr u32 BUSCYCLES_S32 = 3;

constexpr u32 BUSCYCLES_MAP_GRANULARITY_LOG2 = CP15_REGION_BASE_GRANULARITY_LOG2; 
}

#endif // MELONDS_CP15CONSTANTS_H