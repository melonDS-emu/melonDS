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
#include "GBACartGBAMP.h"
#include "GBACartMemoryPak.h"
#include "GBACartNone.h"
#include "GBACartSuperCardCF.h"

#define RAM_TO_CART(addr) (((addr) >> 1) & 0x00FFFFFF)

namespace GBACartHelper {
    static GBACart *cart = &GBACartNone::singleton();

    void Init(GBACartConfig &config) {
        GBACart *singleton = &GBACartNone::singleton();
        if (cart != singleton) {
            delete cart;
            cart = singleton;
        }

        GBACart *newCart = NULL;

        switch (config.Type) {
            case GBACart_Empty:
                break;
            case GBACart_GBAMP: {
                newCart = new GBACartGBAMP(config.DiskImagePath);
            } break;
            case GBACart_MemoryPak: {
                newCart = new GBACartMemoryPak();
            } break;
            case GBACart_SuperCardCF: {
                newCart = new GBACartSuperCardCF(config.DiskImagePath);
            } break;
            default: {
                printf("Unknown Slot-2 cartridge type %d!\n", config.Type);
            } break;
        }

        if (newCart != NULL) {
            if (!newCart->IsValid()) {
                printf("Could not load Slot-2 cartridge!\n");
                delete newCart;
            } else {
                cart = newCart;
            }
        }
    }

    u8 RomRead8(u32 addr) {
        if (addr & 1) return (cart->RomRead16(RAM_TO_CART(addr)) >> 8) & 0xFF;
        else return cart->RomRead16(RAM_TO_CART(addr)) & 0xFF;
    }

    u16 RomRead16(u32 addr) {
        return cart->RomRead16(RAM_TO_CART(addr));
    }

    u32 RomRead32(u32 addr) {
        u16 low = cart->RomRead16(RAM_TO_CART(addr));
        u16 high = cart->RomRead16(RAM_TO_CART(addr + 2));
        return (low | (high << 16));
    }

    void RomWrite8(u32 addr, u8 value) {
        printf("GBACart: TODO Write8\n");
    }

    void RomWrite16(u32 addr, u16 value) {
        cart->RomWrite16(RAM_TO_CART(addr), value);
    }

    void RomWrite32(u32 addr, u32 value) {
        cart->RomWrite16(RAM_TO_CART(addr), value & 0xFFFF);
        cart->RomWrite16(RAM_TO_CART(addr + 2), (value >> 16) & 0xFFFF);
    }
}