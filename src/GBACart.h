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

#include "types.h"

namespace GBACart_GBAMP
{

bool Load(const char *path);
void Unload();

}

namespace GBACart
{

bool Init();
void DeInit();

bool InitGBAMP(const char *path);

u8 Read8(u32 addr);
u16 Read16(u32 addr);
u32 Read32(u32 addr);

void Write8(u32 addr, u8 val);
void Write16(u32 addr, u16 val);
void Write32(u32 addr, u32 val);

}

#endif /* GBACART_H */