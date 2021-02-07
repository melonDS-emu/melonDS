/*
    Copyright 2016-2020 Arisotura

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

#include "CRC32.h"

#include <array>
#include <numeric>

// Statically generates a CRC32 table at compile-time using the provided
// polynomial
constexpr std::array<u32, 256> CRC32Table(
    u32 Polynomial
) noexcept
{
    std::array<u32, 256> Table = {};
    for( std::size_t i = 0; i < 256; ++i )
    {
        u32 CRC = i;
        for( std::size_t CurBit = 0; CurBit < 8; ++CurBit )
        {
            CRC = (CRC >> 1) ^ ( -(CRC & 0b1) & Polynomial);
        }
        Table[i] = CRC;
    }
    return Table;
}


u32 CRC32(const u8* data, size_t len)
{
    static constexpr auto Table = CRC32Table(0xEDB88320u);
    return ~std::accumulate( data, data + len, ~u32(0),
        [](u32 CRC, u8 Byte) 
        {
            return (CRC >> 8) ^ Table[u8(CRC) ^ Byte];
        }
    );
}
