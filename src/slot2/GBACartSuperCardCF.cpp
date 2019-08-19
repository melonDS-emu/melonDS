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
#include "GBACartSuperCardCF.h"

// TODO: This is all based on guesswork and I/O code. Real specimen required to verify.

#define ADDR_TO_DRIVE(addr) (((addr) >> 16) & 0x7 | (((addr) >> 19) & 0x8))
#define ADDR_UNLOCK 0xFFFFFF

#define MODE_MEMORY 0x05
#define MODE_IDE 0x03
#define MODE_MEMORY_READ_ONLY 0x01

GBACartSuperCardCF::GBACartSuperCardCF(const char *drivePath) {
    this->Mode = MODE_MEMORY_READ_ONLY;
    this->Valid = false;
    if (this->Drive.Open(drivePath)) {
        this->Memory = new u16[0x1000000 - 1];
        if (this->Memory != NULL) {
            this->Valid = true;
        }
    }
}

GBACartSuperCardCF::~GBACartSuperCardCF() {
    if (this->Memory != NULL) {
        delete this->Memory;
    }
    this->Drive.Close();
}

u16 GBACartSuperCardCF::RomRead16(u32 addr) {
    printf("GBACartSuperCardCF read %08X\n", addr);
    if (addr < ADDR_UNLOCK) switch (this->Mode) {
        case MODE_MEMORY:
        case MODE_MEMORY_READ_ONLY:
            return this->Memory[addr];
        case MODE_IDE:
            return this->Drive.Read(ADDR_TO_DRIVE(addr));
    }

    // default
    return 0xFFFF;
}

void GBACartSuperCardCF::RomWrite16(u32 addr, u16 value) {
    printf("GBACartSuperCardCF write %08X = %04X\n", addr, value);
    if (addr >= ADDR_UNLOCK) {
        if (value == 0xA55A) {
            this->Unlocked = 1;
            printf("GBACartSuperCardCF: unlocked\n");
        } else if (this->Unlocked) {
            this->Mode = value & 0x7;
            if (this->Mode != MODE_MEMORY
             && this->Mode != MODE_MEMORY_READ_ONLY
             && this->Mode != MODE_IDE) {
                printf("GBACartSuperCardCF: unknown mode %d\n", this->Mode);
            }
            this->Unlocked = 0;
        }
    } else switch (this->Mode) {
        case MODE_MEMORY:
            this->Memory[addr] = value;
            break;
        case MODE_IDE:
            this->Drive.Write(ADDR_TO_DRIVE(addr), value);
            break;
    }
}