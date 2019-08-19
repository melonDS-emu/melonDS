/*
    Copyright (c) 2019 Adrian "asie" Siekierka

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

#ifndef GBACART_H
#define GBACART_H

#include "../types.h"

enum
{
    GBACart_Empty = 0,
    GBACart_GBAMP,
    GBACart_MemoryPak,
    GBACart_SuperCardCF,
    GBACart_MAX
};

typedef struct
{
    int Type;
    char *ROMPath;
    char *DiskImagePath;
} GBACartConfig;

// Addresses are per-word, so 24 bits wide.
class GBACart {
public:
    virtual ~GBACart() {};
    // initialization verification
    virtual bool IsValid() { return true; }
    // memory access
    virtual u16 RomRead16(u32 addr) { return 0xFFFF; };
    virtual void RomWrite16(u32 addr, u16 value) { };
};

namespace GBACartHelper {
    void Init(GBACartConfig &config);

    u8 RomRead8(u32 addr);
    u16 RomRead16(u32 addr);
    u32 RomRead32(u32 addr);
    void RomWrite8(u32 addr, u8 value);
    void RomWrite16(u32 addr, u16 value);
    void RomWrite32(u32 addr, u32 value);
}

#endif /* GBACART_H */