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

#include "CRC32.h"

// http://www.codeproject.com/KB/recipes/crc32_large.aspx

namespace melonDS
{
constexpr u32 _reflect(u32 refl, char ch)
{
    u32 value = 0;

    for(int i = 1; i < (ch + 1); i++)
    {
        if (refl & 1)
            value |= 1 << (ch - i);
        refl >>= 1;
    }

	return value;
}

constexpr auto GetCRC32Table()
{
    std::array<u32, 256> Crc32Table { 0 };
	u32 polynomial = 0x04C11DB7;

	for (int i = 0; i < 0x100; i++)
    {
        Crc32Table[i] = _reflect(i, 8) << 24;

        for (int j = 0; j < 8; j++)
            Crc32Table[i] = (Crc32Table[i] << 1) ^ (Crc32Table[i] & (1 << 31) ? polynomial : 0);

        Crc32Table[i] = _reflect(Crc32Table[i],  32);
    }
    return Crc32Table;
}

u32 CRC32(const u8 *data, int len, u32 start)
{
    auto Crc32Table = GetCRC32Table();

	u32 crc = start ^ 0xFFFFFFFF;

	while (len--)
        crc = (crc >> 8) ^ Crc32Table[(crc & 0xFF) ^ *data++];

	return (crc ^ 0xFFFFFFFF);
}

}