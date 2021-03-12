/*
    Copyright 2016-2021 Arisotura

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

u32 crctable[256];
bool tableinited = false;

u32 _reflect(u32 refl, char ch)
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

void _inittable()
{
	u32 polynomial = 0x04C11DB7;

	for (int i = 0; i < 0x100; i++)
    {
        crctable[i] = _reflect(i, 8) << 24;

        for (int j = 0; j < 8; j++)
            crctable[i] = (crctable[i] << 1) ^ (crctable[i] & (1 << 31) ? polynomial : 0);

        crctable[i] = _reflect(crctable[i],  32);
    }
}

u32 CRC32(u8 *data, int len)
{
    if (!tableinited)
    {
        _inittable();
        tableinited = true;
    }

	u32 crc = 0xFFFFFFFF;

	while (len--)
        crc = (crc >> 8) ^ crctable[(crc & 0xFF) ^ *data++];

	return (crc ^ 0xFFFFFFFF);
}
