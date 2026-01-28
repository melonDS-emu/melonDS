/*
    Copyright 2016-2025 melonDS team

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

#ifndef GPU_COLOROP_H
#define GPU_COLOROP_H

#include "types.h"

namespace melonDS
{

static constexpr u32 ColorBlend4(u32 val1, u32 val2, u32 eva, u32 evb) noexcept
{
    u32 r =  (((val1 & 0x00003F) * eva) + ((val2 & 0x00003F) * evb) + 0x000008) >> 4;
    u32 g = ((((val1 & 0x003F00) * eva) + ((val2 & 0x003F00) * evb) + 0x000800) >> 4) & 0x007F00;
    u32 b = ((((val1 & 0x3F0000) * eva) + ((val2 & 0x3F0000) * evb) + 0x080000) >> 4) & 0x7F0000;

    if (r > 0x00003F) r = 0x00003F;
    if (g > 0x003F00) g = 0x003F00;
    if (b > 0x3F0000) b = 0x3F0000;

    return r | g | b | 0xFF000000;
}

static constexpr u32 ColorBlend5(u32 val1, u32 val2) noexcept
{
    u32 eva = ((val1 >> 24) & 0x1F) + 1;
    u32 evb = 32 - eva;

    if (eva == 32) return val1;

    u32 r =  (((val1 & 0x00003F) * eva) + ((val2 & 0x00003F) * evb) + 0x000010) >> 5;
    u32 g = ((((val1 & 0x003F00) * eva) + ((val2 & 0x003F00) * evb) + 0x001000) >> 5) & 0x007F00;
    u32 b = ((((val1 & 0x3F0000) * eva) + ((val2 & 0x3F0000) * evb) + 0x100000) >> 5) & 0x7F0000;

    if (r > 0x00003F) r = 0x00003F;
    if (g > 0x003F00) g = 0x003F00;
    if (b > 0x3F0000) b = 0x3F0000;

    return r | g | b | 0xFF000000;
}

static constexpr u32 ColorBrightnessUp(u32 val, u32 factor, u32 bias) noexcept
{
    u32 rb = val & 0x3F003F;
    u32 g = val & 0x003F00;

    rb += (((((0x3F003F - rb) * factor) + (bias*0x010001)) >> 4) & 0x3F003F);
    g +=  (((((0x003F00 - g ) * factor) + (bias*0x000100)) >> 4) & 0x003F00);

    return rb | g | 0xFF000000;
}

static constexpr u32 ColorBrightnessDown(u32 val, u32 factor, u32 bias) noexcept
{
    u32 rb = val & 0x3F003F;
    u32 g = val & 0x003F00;

    rb -= ((((rb * factor) + (bias*0x010001)) >> 4) & 0x3F003F);
    g -=  ((((g  * factor) + (bias*0x000100)) >> 4) & 0x003F00);

    return rb | g | 0xFF000000;
}

}

#endif // GPU_COLOROP_H
