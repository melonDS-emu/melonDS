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
#include "GBACart.h"
#include "GBACartNone.h"

#define RAM_TO_CART(addr) (((addr) >> 1) & 0x00FFFFFF)

namespace GBACartHelper {
    static GBACart *cart = &GBACartNone::singleton();

    void Use(GBACart *inputCart) {
        if (inputCart == NULL) {
            cart = &GBACartNone::singleton();
        } else {
            cart = inputCart;
        }
    }

    u8 RomRead8(u32 addr) {
        if (addr & 1) return (cart->RomReadWord(RAM_TO_CART(addr)) >> 8) & 0xFF;
        else return cart->RomReadWord(RAM_TO_CART(addr)) & 0xFF;
    }

    u16 RomRead16(u32 addr) {
        return cart->RomReadWord(RAM_TO_CART(addr));
    }

    u32 RomRead32(u32 addr) {
        printf("GBACart: TODO Read32\n");
        return 0xFFFFFFFF;
    }

    void RomWrite8(u32 addr, u8 value) {
        printf("GBACart: TODO Write8\n");
    }

    void RomWrite16(u32 addr, u16 value) {
        cart->RomWriteWord(RAM_TO_CART(addr), value);
    }

    void RomWrite32(u32 addr, u32 value) {
        printf("GBACart: TODO Write32\n");
    }
}