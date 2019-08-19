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
#include <string.h>
#include "../Platform.h"
#include "GBACartGBAMP.h"

// TODO: This is all based on guesswork and I/O code. Real specimen required to verify.

#define ADDR_TO_DRIVE(addr) (((addr) >> 16) & 0x7 | (((addr) >> 19) & 0x8))

GBACartGBAMP::GBACartGBAMP(const char *path) {
    this->Valid = this->Drive.Open(path);
}

GBACartGBAMP::~GBACartGBAMP() {
    this->Drive.Close();
}

u16 GBACartGBAMP::RomRead16(u32 addr) {
    u16 value = this->Drive.Read(ADDR_TO_DRIVE(addr));
    printf("GBACartGBAMP read %08X(%d) = %04X\n", addr, ADDR_TO_DRIVE(addr), value);
    return value;
}

void GBACartGBAMP::RomWrite16(u32 addr, u16 value) {
    printf("GBACartGBAMP write %08X = %04X\n", addr, value);
    this->Drive.Write(ADDR_TO_DRIVE(addr), value);
}