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

#ifndef NDSCART_CARTSD_H
#define NDSCART_CARTSD_H

#include "CartCommon.h"
#include "../FATStorage.h"

namespace melonDS::NDSCart
{

// CartSD -- any 'cart' with an SD card slot
class CartSD : public CartCommon
{
public:
    CartSD(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, void* userdata, std::optional<FATStorage>&& sdcard = std::nullopt);
    CartSD(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, void* userdata, std::optional<FATStorage>&& sdcard = std::nullopt);
    ~CartSD() override;

    [[nodiscard]] const std::optional<FATStorage>& GetSDCard() const noexcept { return SD; }
    void SetSDCard(FATStorage&& sdcard) noexcept { SD = std::move(sdcard); }
    void SetSDCard(std::optional<FATStorage>&& sdcard) noexcept
    {
        SD = std::move(sdcard);
        sdcard = std::nullopt;
        // moving from an optional doesn't set it to nullopt,
        // it just leaves behind an optional with a moved-from value
    }

    void SetSDCard(std::optional<FATStorageArgs>&& args) noexcept
    {
        // Close the open SD card (if any) so that its contents are flushed to disk.
        // Also, if args refers to the same image file that SD is currently using,
        // this will ensure that we don't have two open read-write handles
        // to the same file.
        SD = std::nullopt;

        if (args)
            SD = FATStorage(std::move(*args));

        args = std::nullopt;
        // moving from an optional doesn't set it to nullopt,
        // it just leaves behind an optional with a moved-from value
    }

protected:
    void ApplyDLDIPatchAt(u8* binary, u32 dldioffset, const u8* patch, u32 patchlen, bool readonly) const;
    void ApplyDLDIPatch(const u8* patch, u32 patchlen, bool readonly);

    std::optional<FATStorage> SD {};

    u32 SectorAddr = 0;
    u32 SectorPos = 0;
    u8 SectorBuffer[512] {};
};

}

#endif
