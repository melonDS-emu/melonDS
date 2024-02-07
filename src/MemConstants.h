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
}

#endif // MELONDS_MEMCONSTANTS_H