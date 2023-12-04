/*
    Copyright 2016-2023 melonDS team

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

#ifndef MELONDS_ARGS_H
#define MELONDS_ARGS_H

#include <array>
#include <optional>
#include <memory>

#include "types.h"
#include "MemConstants.h"
#include "DSi_NAND.h"
#include "FATStorage.h"
#include "FreeBIOS.h"
#include "SPI_Firmware.h"

namespace melonDS
{
namespace NDSCart { class CartCommon; }
namespace GBACart { class CartCommon; }

template<size_t N>
constexpr std::array<u8, N> BrokenBIOS = []() constexpr {
    std::array<u8, N> broken {};

    for (int i = 0; i < 16; i++)
    {
        broken[i*4+0] = 0xE7;
        broken[i*4+1] = 0xFF;
        broken[i*4+2] = 0xDE;
        broken[i*4+3] = 0xFF;
    }

    return broken;
}();

/// Arguments to pass into the NDS constructor.
/// New fields here should have default values if possible.
struct NDSArgs
{
    /// NDS ROM to install.
    /// Defaults to nullptr, which means no cart.
    /// Should be populated with the desired save data beforehand,
    /// including an SD card if applicable.
    std::unique_ptr<NDSCart::CartCommon> NDSROM = nullptr;

    /// GBA ROM to install.
    /// Defaults to nullptr, which means no cart.
    /// Should be populated with the desired save data beforehand.
    /// Ignored in DSi mode.
    std::unique_ptr<GBACart::CartCommon> GBAROM = nullptr;

    /// NDS ARM9 BIOS to install.
    /// Defaults to FreeBIOS, which is not compatible with DSi mode.
    std::array<u8, ARM9BIOSSize> ARM9BIOS = bios_arm9_bin;

    /// NDS ARM7 BIOS to install.
    /// Defaults to FreeBIOS, which is not compatible with DSi mode.
    std::array<u8, ARM7BIOSSize> ARM7BIOS = bios_arm7_bin;

    /// Firmware image to install.
    /// Defaults to generated NDS firmware.
    /// Generated firmware is not compatible with DSi mode.
    melonDS::Firmware Firmware {0};
};

/// Arguments to pass into the DSi constructor.
/// New fields here should have default values if possible.
/// Contains no virtual methods, so there's no vtable.
struct DSiArgs final : public NDSArgs
{
    std::array<u8, DSiBIOSSize> ARM9iBIOS = BrokenBIOS<DSiBIOSSize>;
    std::array<u8, DSiBIOSSize> ARM7iBIOS = BrokenBIOS<DSiBIOSSize>;

    /// NAND image to install.
    /// Required, there is no default value.
    DSi_NAND::NANDImage NANDImage;

    /// SD card to install.
    /// Defaults to std::nullopt, which means no SD card.
    std::optional<FATStorage> DSiSDCard;
};
}
#endif //MELONDS_ARGS_H
