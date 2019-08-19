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
    if (addr & 0x800000) {
        return this->Memory[addr & 0x3FFFFF];
    } else {
        printf("GBACartMemoryPak read %08X\n", addr);
        switch (addr) {
            case 0x5A:
                return 0x24FF; // 0x24 is the Memory Pak's Device Type
            case 0x5F:
            case 0xFFFF:
                return 0x7FFF; // must be cleared for Memory Pak to be detected
            case 0x120000:
                return (this->Unlocked) & 0xFFFF;
            case 0x120001:
                return (this->Unlocked >> 16) & 0xFFFF;
            default:
                return 0xFFFF;
        }
    }
}

void GBACartMemoryPak::RomWrite16(u32 addr, u16 value) {
    if (addr & 0x800000) {
        if (this->Unlocked) {
            this->Memory[addr & 0x3FFFFF] = value;
        }
    } else {
        printf("GBACartMemoryPak write %08X = %04X\n", addr, value);
        switch (addr) {
            case 0x120000:
                this->Unlocked = (this->Unlocked & 0xFFFF0000) | value;
                break;
            case 0x120001:
                this->Unlocked = (this->Unlocked & 0x0000FFFF) | (value << 16);
                break;   
        }
    }
}