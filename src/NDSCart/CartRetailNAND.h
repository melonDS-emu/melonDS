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

#ifndef NDSCART_CARTRETAILNAND_H
#define NDSCART_CARTRETAILNAND_H

#include "CartRetail.h"

namespace melonDS::NDSCart
{

// CartRetailNAND -- retail cart with NAND SRAM (WarioWare DIY, Jam with the Band, ...)
class CartRetailNAND : public CartRetail
{
public:
    CartRetailNAND(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata);
    CartRetailNAND(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata);
    ~CartRetailNAND() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void SetSaveMemory(const u8* savedata, u32 savelen) override;

    void ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd) override;
    u32 ROMCommandReceive() override;
    void ROMCommandTransmit(u32 val) override;

    // NAND cartridges have no SPI interface
    void SPISelect() override {}
    void SPIRelease() override {}
    u8 SPITransmitReceive(u8 val) override { return 0xFF; }

private:
    void BuildSRAMID();
    u32 SRAMRead32();

    u32 SRAMBase = 0;
    u32 SRAMWindow = 0;

    u8 SRAMWriteBuffer[0x800] {};
    u32 SRAMWritePos = 0;
    u32 SRAMWriteLen = 0;

    u8 SRAMID[0x30];
};

}

#endif
