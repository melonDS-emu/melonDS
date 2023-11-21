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

#include "NDSCart.h"
#include "GBACart.h"
#include "SPI_Firmware.h"
#include "DSi_NAND.h"
#include "FreeBIOS.h"
#include "types.h"

namespace melonDS
{
class FATStorage;

#ifdef NDEBUG
constexpr bool FastMemoryDefault = true;
#else
// Off by default in debug builds so that hapless contributors
// don't get confused by the mass of segfaults that will occur
constexpr bool FastMemoryDefault = false;
#endif

struct JITArguments
{
    bool Enabled;
    int MaxBlockSize = 32;
    bool LiteralOptimizations = true;
    bool BranchOptimizations = true;
    bool FastMemory = FastMemoryDefault;
};

struct GDBArguments
{
    int Port = 0;
    bool BreakOnStartup = false;
};

struct SDCardArguments
{
    std::unique_ptr<FATStorage> SDCard;
};

struct NDSSysfileArguments
{
    std::array<u8, ARM9BIOSLength> ARM9BIOS = bios_arm9_bin;
    std::array<u8, ARM7BIOSLength> ARM7BIOS = bios_arm7_bin;
    std::optional<Firmware> Firmware = std::nullopt;

    std::unique_ptr<u8[]> DSiBIOS7 = nullptr;
    std::unique_ptr<u8[]> DSiBIOS9 = nullptr;
    std::optional<DSi_NAND::NANDImage> DSiNAND = std::nullopt;
};

struct DSiSysfileArguments
{
    /// The ARM9 BIOS image for the DSi.
    /// Must be provided in DSi mode.
    std::array<u8, 0x10000> ARM9iBIOS {};

    /// The ARM7 BIOS image for the DSi.
    std::array<u8, 0x10000> ARM7iBIOS {};

    std::optional<DSi_NAND::NANDImage> DSiNAND = std::nullopt;
};

/// Arguments that can be set when constructing a NDS or DSi.
struct InitArguments
{
    /// Arguments to initialize JIT support with.
    /// To disable the JIT, set this to std::nullopt.
    /// Defaults to on, with the defaults given in JITArguments' declaration.
    /// Ignored in builds without JIT support.
    /// Cannot be adjusted on a live NDS; you will need to destroy it and create a new one.
    std::optional<JITArguments> JIT = std::make_optional<JITArguments>();

    /// Arguments for initializing GDB support
    /// Ignored if GDB support is excluded from the build
    std::optional<GDBArguments> GDBARM7 = std::nullopt;
    std::optional<GDBArguments> GDBARM9 = std::nullopt;

    /// Pointer to the ROM data that will be loaded.
    /// Can be changed on a live DS, although it will need to be reset.
    std::optional<std::unique_ptr<NDSCart::CartCommon>> NDSROM = std::nullopt;

    /// Pointer to a loaded and parsed GBA ROM.
    /// Ignored if initializing a DSi.
    std::unique_ptr<GBACart::CartCommon> GBAROM = nullptr;

    std::unique_ptr<FATStorage> DLDISDCard = nullptr;
    std::unique_ptr<FATStorage> DSiSDCard = nullptr;

    int AudioBitDepth;
    bool DSiFullBIOSBoot = false;

};

/// Arguments that can be set when resetting an existing NDS or DSi.
/// nullopt means that the existing value is unchanged.
struct ResetArguments
{
    std::optional<JITArguments> JIT = std::nullopt;
};
}

#endif // MELONDS_ARGUMENTS_H
