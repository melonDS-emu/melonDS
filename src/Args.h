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

#include "NDSCart.h"
#include "GBACart.h"
#include "types.h"
#include "MemConstants.h"
#include "DSi_NAND.h"
#include "FATStorage.h"
#include "FreeBIOS.h"
#include "GPU3D_Soft.h"
#include "SPI_Firmware.h"
#include "SPU.h"

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

/// Arguments that configure the JIT.
/// Ignored in builds that don't have the JIT included.
struct JITArgs
{
    unsigned MaxBlockSize = 32;
    bool LiteralOptimizations = true;
    bool BranchOptimizations = true;

    /// Ignored in builds that have fast memory excluded
    /// (even if the JIT is otherwise available).
    /// Enabled by default, but frontends should disable this when debugging
    /// so the constants segfaults don't hinder debugging.
    bool FastMemory = true;
};

using ARM9BIOSImage = std::array<u8, ARM9BIOSSize>;
using ARM7BIOSImage = std::array<u8, ARM7BIOSSize>;
using DSiBIOSImage = std::array<u8, DSiBIOSSize>;

struct GDBArgs
{
    u16 PortARM7 = 0;
    u16 PortARM9 = 0;
    bool ARM7BreakOnStartup = false;
    bool ARM9BreakOnStartup = false;
};

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
    std::unique_ptr<ARM9BIOSImage> ARM9BIOS = std::make_unique<ARM9BIOSImage>(bios_arm9_bin);

    /// NDS ARM7 BIOS to install.
    /// Defaults to FreeBIOS, which is not compatible with DSi mode.
    std::unique_ptr<ARM7BIOSImage> ARM7BIOS = std::make_unique<ARM7BIOSImage>(bios_arm7_bin);

    /// Firmware image to install.
    /// Defaults to generated NDS firmware.
    /// Generated firmware is not compatible with DSi mode.
    melonDS::Firmware Firmware {0};

    /// How the JIT should be configured when initializing.
    /// Defaults to enabled, with default settings.
    /// To disable the JIT, set this to std::nullopt.
    /// Ignored in builds that don't have the JIT included.
    std::optional<JITArgs> JIT = JITArgs();

    AudioBitDepth BitDepth = AudioBitDepth::Auto;
    AudioInterpolation Interpolation = AudioInterpolation::None;

    /// How the GDB stub should be handled.
    /// Defaults to disabled.
    /// Ignored in builds that don't have the GDB stub included.
    std::optional<GDBArgs> GDB = std::nullopt;

    /// The 3D renderer to initialize the DS with.
    /// Defaults to the software renderer.
    /// Can be changed later at any time.
    std::unique_ptr<melonDS::Renderer3D> Renderer3D = std::make_unique<SoftRenderer>();
};

/// Arguments to pass into the DSi constructor.
/// New fields here should have default values if possible.
/// Contains no virtual methods, so there's no vtable.
struct DSiArgs final : public NDSArgs
{
    std::unique_ptr<DSiBIOSImage> ARM9iBIOS = std::make_unique<DSiBIOSImage>(BrokenBIOS<DSiBIOSSize>);
    std::unique_ptr<DSiBIOSImage> ARM7iBIOS = std::make_unique<DSiBIOSImage>(BrokenBIOS<DSiBIOSSize>);

    /// NAND image to install.
    /// Required, there is no default value.
    DSi_NAND::NANDImage NANDImage;

    /// SD card to install.
    /// Defaults to std::nullopt, which means no SD card.
    std::optional<FATStorage> DSiSDCard;

    bool FullBIOSBoot = false;
};
}
#endif //MELONDS_ARGS_H
