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
#include "SPI_Firmware.h"

namespace melonDS
{
namespace NDSCart { class CartCommon; }
namespace GBACart { class CartCommon; }

struct NDSArgs
{
    std::unique_ptr<NDSCart::CartCommon> NDSROM;
    std::unique_ptr<GBACart::CartCommon> GBAROM;
    std::array<u8, ARM9BIOSSize> ARM9BIOS;
    std::array<u8, ARM7BIOSSize> ARM7BIOS;
    melonDS::Firmware Firmware;
};

struct DSiArgs final : public NDSArgs
{
    std::array<u8, DSiBIOSSize> ARM9iBIOS;
    std::array<u8, DSiBIOSSize> ARM7iBIOS;
    DSi_NAND::NANDImage NANDImage;
    std::optional<FATStorage> DSiSDCard;
};
}
#endif //MELONDS_ARGS_H
