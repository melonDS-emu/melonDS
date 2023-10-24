/*
    Copyright 2016-2022 melonDS team

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

#ifndef DMA_TIMINGS_H
#define DMA_TIMINGS_H

#include <array>
#include "types.h"

namespace DMATiming
{

// DMA timing tables
//
// DMA timings on the DS are normally straightforward, except in one case: when
// main RAM is involved.
// Main RAM to main RAM is the easy case: 16c/unit in 16bit mode, 18c/unit in 32bit
// mode.
// It gets more complicated when transferring from main RAM to somewhere else, or
// vice versa: main RAM supports burst accesses, but the rules dictating how long
// bursts can be are weird and inconsistent. Main RAM also supports parallel
// memory operations, to some extent.
// I haven't figured out the full logic behind it, let alone how to emulate it
// efficiently, so for now we will use these tables.
// A zero denotes the end of a burst pattern.
//
// Note: burst patterns only apply when the main RAM address is incrementing.
// A fixed or decrementing address results in nonsequential accesses.
//
// Note about GBA slot/wifi timings: these take into account the sequential timing
// setting. Timings are such that the nonseq setting only matters for the first
// access, and minor edge cases (like the last of a 0x20000-byte block).

extern const std::array<u8, 256> MRAMDummy;

extern const std::array<u8, 256> MRAMRead16Bursts[3];

extern const std::array<u8, 256> MRAMRead32Bursts[4];

extern const std::array<u8, 256> MRAMWrite16Bursts[3];

extern const std::array<u8, 256> MRAMWrite32Bursts[4];

}

#endif // DMA_TIMINGS_H
