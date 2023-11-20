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

#include <memory>
#include <optional>

#include "NDSCart.h"
#include "GBACart.h"
#include "SPI_Firmware.h"
#include "DSi_NAND.h"
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
    int MaxBlockSize = 32;
    bool LiteralOptimizations = true;
    bool BranchOptimizations = true;
    bool FastMemory = FastMemoryDefault;
};

struct GDBArguments
{
    int PortARM7 = 0;
    int PortARM9 = 0;
    bool ARM7BreakOnStartup = false;
    bool ARM9BreakOnStartup = false;
};

struct SDCardArguments
{
    std::unique_ptr<FATStorage> SDCard;
};

struct NDSSysfileArguments
{
    // TODO: Change this to an array that defaults to FreeBIOS7
    std::unique_ptr<u8[]> NDSBIOS7 = nullptr;
    // TODO: Change this to an array that defaults to FreeBIOS9
    std::unique_ptr<u8[]> NDSBIOS9 = nullptr;
    // TODO: Change this to an array that defaults to generated firmware
    std::optional<Firmware> Firmware = std::nullopt;

    std::unique_ptr<u8[]> DSiBIOS7 = nullptr;
    std::unique_ptr<u8[]> DSiBIOS9 = nullptr;
    std::optional<DSi_NAND::NANDImage> DSiNAND = std::nullopt;
};

struct DSiSysfileArguments
{
    std::unique_ptr<u8[]> DSiBIOS7 = nullptr;
    std::unique_ptr<u8[]> DSiBIOS9 = nullptr;
    std::optional<DSi_NAND::NANDImage> DSiNAND = std::nullopt;
};

struct Arguments
{
    /// Arguments to initialize JIT support with.
    /// To disable the JIT, set this to std::nullopt.
    /// Defaults to on, with the defaults given in JITArguments' declaration.
    /// Ignored in builds without JIT support.
    /// Cannot be adjusted on a live NDS; you will need to destroy it and create a new one.
    std::optional<JITArguments> JIT = std::make_optional<JITArguments>();

    /// Arguments for initializing GDB support
    /// Ignored if GDB support is excluded from the build
    std::optional<GDBArguments> GDB = std::nullopt;

    /// Pointer to the ROM data that will be loaded.
    /// Can be changed on a live DS, although it will need to be reset.
    std::unique_ptr<NDSCart::CartCommon> NDSROM = nullptr;

    /// Pointer to a loaded and parsed GBA ROM.
    /// Ignored if initializing a DSi.
    std::unique_ptr<GBACart::CartCommon> GBAROM = nullptr;

    std::unique_ptr<FATStorage> DLDISDCard = nullptr;
    std::unique_ptr<FATStorage> DSiSDCard = nullptr;

    int AudioBitDepth;
    bool DSiFullBIOSBoot = false;

};
}

#endif // MELONDS_ARGUMENTS_H
