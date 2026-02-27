/*
    Copyright 2016-2026 melonDS team

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

#ifndef NDSCART_CARTHOMEBREW_H
#define NDSCART_CARTHOMEBREW_H

#include "CartSD.h"

namespace melonDS::NDSCart
{

// CartHomebrew -- homebrew 'cart' (no SRAM, DLDI)
class CartHomebrew : public CartSD
{
public:
    CartHomebrew(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, void* userdata, std::optional<FATStorage>&& sdcard = std::nullopt);
    CartHomebrew(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, void* userdata, std::optional<FATStorage>&& sdcard = std::nullopt);
    ~CartHomebrew() override;

    void Reset() override;
    void SetupDirectBoot(const std::string& romname, NDS& nds) override;

    void ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd) override;
    u32 ROMCommandReceive() override;
    void ROMCommandTransmit(u32 val) override;
    void ROMCommandFinish() override;
};

}

#endif
