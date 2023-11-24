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

#ifndef MELONDS_ARGUMENTS_H
#define MELONDS_ARGUMENTS_H

#include "Constants.h"


#include <memory>
#include <optional>

#include "GPU3D_Soft.h"
#include "GPU2D_Soft.h"
#include "SPI_Firmware.h"
#include "DSi_NAND.h"
#include "FATStorage.h"
#include "FreeBIOS.h"
#include "SPU.h"
#include "RTC.h"
#include "types.h"
#include "GBACart.h"
#include "NDSCart.h"

namespace melonDS
{
class FATStorage;
class Renderer3D;
namespace GPU2D { class Renderer2D; }
namespace NDSCart { class CartCommon; }
namespace GBACart { class CartCommon; }

struct JITArguments
{
    int MaxBlockSize = 32;
    bool LiteralOptimisations = true;
    bool BranchOptimisations = true;
    bool FastMemory = true;
};

struct GDBArguments
{
    int Port = 0;
    bool BreakOnStartup = false;
};

/// Arguments that can be set when constructing a NDS or DSi.
/// Most of these correspond to fields or methods that can be set on the resulting object.
/// @note The constructors for NDS and DSi move the contents of this struct,
/// so it will be invalid after the constructor returns.
struct InitArgs
{
    /// Pointer to the ROM data that will be loaded.
    /// To load a homebrew ROM with an SD card,
    /// pass a FATStorageArgs object to NDSCart::ParseROM.
    /// @see NDSCart::ParseROM
    /// @see NDS::LoadCart
    std::unique_ptr<NDSCart::CartCommon> NDSROM = nullptr;

    /// Pointer to the SRAM data that will be loaded, if any.
    /// The SRAM data will be copied into the NDS object,
    /// so the caller is free to deallocate it after passing it to the constructor.
    /// Ignored if NDSSRAMLength is 0 or NDSROM is nullptr.
    u8* NDSSRAM = nullptr;

    /// The length of the data given in NDSSRAM, in bytes.
    /// Ignored if NDSSRAM or NDSROM are nullptr.
    u32 NDSSRAMLength = 0;

    /// Pointer to a loaded and parsed GBA ROM.
    /// Ignored if initializing a DSi.
    /// @see GBACart::ParseROM
    /// @see GBACartSlot::InsertROM
    std::unique_ptr<GBACart::CartCommon> GBAROM = nullptr;

    /// Pointer to the SRAM data that will be loaded, if any.
    /// The SRAM data will be copied into the cartridge object,
    /// so the caller is free to deallocate it after passing it to the constructor.
    /// Ignored if GBASRAMLength is 0 or GBAROM is nullptr.
    u8* GBASRAM = nullptr;

    /// The length of the data given in GBASRAM, in bytes.
    /// Ignored if GBASRAM or GBAROM are nullptr.
    u32 GBASRAMLength = 0;

    /// ARM 9 BIOS image for NDS mode.
    /// Defaults to FreeBIOS.
    /// Must be a native BIOS image in DSi mode.
    /// Native and built-in BIOS images must not be mixed.
    /// @see NDS::ARM9BIOS
    std::array<u8, ARM9BIOSLength> ARM9BIOS = bios_arm9_bin;

    /// ARM 7 BIOS image for NDS mode.
    /// Defaults to FreeBIOS.
    /// Optional in NDS mode, in which case FreeBIOS will be used.
    /// Must be a native BIOS image in DSi mode.
    std::array<u8, ARM7BIOSLength> ARM7BIOS = bios_arm7_bin;

    /// Firmware image for NDS or DSi mode.
    /// Defaults to generated NDS firmware.
    /// Native firmware required in DSi mode.
    /// Will be moved-from if passed to the NDS constructor,
    /// at which point the object will be invalid.
    Firmware Firmware = melonDS::Firmware(0);

    /// The ARM9 BIOS image for the DSi.
    /// Must be a native BIOS image in DSi mode.
    /// Ignored in NDS mode.
    std::array<u8, DSiBIOSLength> ARM9iBIOS {};

    /// The ARM7 BIOS image for the DSi.
    /// Must be a native BIOS image in DSi mode.
    /// Ignored in NDS mode.
    std::array<u8, DSiBIOSLength> ARM7iBIOS {};

    /// A NAND image for DSi mode.
    /// Moved-from when constructing a DSi,
    /// so this will be nullopt after its constructor returns.
    /// Ignored in NDS mode.
    std::optional<DSi_NAND::NANDImage> NANDImage;

    /// An SD card image for DSi mode.
    /// Moved-from when constructing a DSi,
    /// so this will be nullopt after its constructor returns.
    /// Ignored in NDS mode.
    std::optional<FATStorage> DSiSDCard;

    /// Arguments to initialize JIT support with.
    /// To disable the JIT, set this to std::nullopt.
    /// Defaults to on, with the defaults given in JITArguments' declaration.
    /// Ignored in builds without JIT support.
    std::optional<JITArguments> JIT = std::make_optional<JITArguments>();

    /// Arguments for initializing GDB support for the ARM7 CPU.
    /// Ignored if GDB support is excluded from the build
    std::optional<GDBArguments> GDBARM7 = std::nullopt;

    /// Arguments for initializing GDB support for the ARM7 CPU.
    /// Ignored if GDB support is excluded from the build
    std::optional<GDBArguments> GDBARM9 = std::nullopt;

    /// The bit depth for the audio output.
    /// Defaults to 16-bit for the DSi and 10-bit for the NDS.
    std::optional<AudioBitDepth> AudioBitDepth;

    Interpolation AudioInterpolation;

    /// Whether to use the full DSi boot process.
    /// Ignored in NDS mode.
    /// Defaults to false.
    bool DSiFullBIOSBoot = false;

    /// The renderer to use for the 3D engine.
    /// Defaults to the software renderer.
    std::unique_ptr<Renderer3D> Renderer3D = nullptr;

    /// The renderer to use for the 2D engine.
    /// Defaults to the software renderer.
    std::unique_ptr<GPU2D::Renderer2D> Renderer2D = nullptr;

    /// The date and time to start the console off with.
    /// If unset, defaults to the host system time.
    std::optional<RTC::DateTime> InitialDateTime = std::nullopt;
};

}

#endif // MELONDS_ARGUMENTS_H
