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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Platform.h"
#include "GBACartMemoryPak.h"

// TODO: This is insufficient. Testing with real hardware is necessary.

GBACartMemoryPak::GBACartMemoryPak() {
    this->Memory = new u16[0x400000];
    this->Unlocked = 0;
}

GBACartMemoryPak::~GBACartMemoryPak() {
    if (this->Memory != NULL) {
        delete this->Memory;
    }
}

u16 GBACartMemoryPak::RomRead16(u32 addr) {
    printf("GBACartMemoryPak read %08X\n", addr);
    if (addr & 0x800000) {
        return this->Memory[addr & 0x3FFFFF];
    } else {
        return 0xFFFF;
    }
}

void GBACartMemoryPak::RomWrite16(u32 addr, u16 value) {
    printf("GBACartMemoryPak write %08X = %04X\n", addr, value);
    if (addr & 0x800000) {
        if (this->Unlocked) {
            this->Memory[addr & 0x3FFFFF] = value;
        }
    } else if (addr == 0x120000) {
        this->Unlocked = value & 0x1;
    }
}