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

#ifndef NDSCART_CARTRETAIL_H
#define NDSCART_CARTRETAIL_H

#include "CartCommon.h"

namespace melonDS::NDSCart
{

// CartRetail -- regular retail cart (ROM, SPI SRAM)
class CartRetail : public CartCommon
{
public:
    CartRetail(
            const u8* rom,
            u32 len,
            u32 chipid,
            bool badDSiDump,
            ROMListEntry romparams,
            std::unique_ptr<u8[]>&& sram,
            u32 sramlen,
            void* userdata,
            melonDS::NDSCart::CartType type = CartType::Retail
    );
    CartRetail(
            std::unique_ptr<u8[]>&& rom,
            u32 len, u32 chipid,
            bool badDSiDump,
            ROMListEntry romparams,
            std::unique_ptr<u8[]>&& sram,
            u32 sramlen,
            void* userdata,
            melonDS::NDSCart::CartType type = CartType::Retail
    );
    ~CartRetail() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void SetSaveMemory(const u8* savedata, u32 savelen) override;

    void SPISelect() override;
    void SPIRelease() override;
    u8 SPITransmitReceive(u8 val) override;

    u8* GetSaveMemory() override { return SRAM.get(); }
    const u8* GetSaveMemory() const override { return SRAM.get(); }
    u32 GetSaveMemoryLength() const override { return SRAMLength; }

protected:
    u8 SRAMWrite_EEPROMTiny(u8 val);
    u8 SRAMWrite_EEPROM(u8 val);
    u8 SRAMWrite_FLASH(u8 val);

    std::unique_ptr<u8[]> SRAM = nullptr;
    u32 SRAMLength = 0;
    u32 SRAMType = 0;

    u32 SRAMPos = 0;
    u8 SRAMCmd = 0;
    u32 SRAMAddr = 0;
    //u32 SRAMFirstAddr = 0;
    u8 SRAMStatus = 0;

    //bool SRAMNeedsSaving = false;
    u32 SRAMSaveAddr = 0;
    u32 SRAMSaveLen = 0;
};

}

#endif
