/*
    Copyright 2016-2024 melonDS team

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

#include "DSi.h"
#include "DSi_DSP_UCodes.h"
#include "CRC32.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

// static block of zero'd memory for unmapped DSP memory
static const u8 DSPZeroPage[0x8000] = {};

UCodeID IdentifyUCode(melonDS::DSi& dsi)
{
    u32 crc = 0;

    // Hash NWRAM B, which contains the DSP program
    // The hash should be in the DSP's memory view
    for (u32 addr = 0; addr < 0x40000; addr += 0x8000)
    {
        const u8* ptr = dsi.NWRAMMap_B[2][(addr >> 15) & 0x7];
        if (!ptr) ptr = DSPZeroPage;
        crc = CRC32(ptr, 0x8000, crc);
    }

    switch (crc)
    {
        case 0x7867C94B:
            Log(LogLevel::Debug, "Identified AAC Sound App DSP UCode\n");
            return UCodeID::AAC_SOUND_APP;
        case 0x0CAFEF48:
            Log(LogLevel::Debug, "Identified AAC SDK v0 DSP UCode\n");
            return UCodeID::AAC_SDK_V0;
        case 0xCD2A8B1B:
            Log(LogLevel::Debug, "Identified Graphics SDK v0 DSP UCode\n");
            return UCodeID::GRAPHICS_SDK_V0;
        case 0x7EEE19FE:
            Log(LogLevel::Debug, "Identified G711 SDK v1 DSP UCode\n");
            return UCodeID::G711_SDK_V1;
        case 0x7323B75B:
            Log(LogLevel::Debug, "Identified Graphics SDK v1 DSP UCode\n");
            return UCodeID::GRAPHICS_SDK_V1;
        case 0xBD4B63B6:
            Log(LogLevel::Debug, "Identified Graphics SDK v1 Patch DSP UCode\n");
            return UCodeID::GRAPHICS_SDK_V1_PATCH;
        case 0x6056C6FF:
            Log(LogLevel::Debug, "Identified G711 SDK v2 DSP UCode\n");
            return UCodeID::G711_SDK_V2;
        case 0x448BB6A2:
            Log(LogLevel::Debug, "Identified Graphics SDK v2 DSP UCode\n");
            return UCodeID::GRAPHICS_SDK_V2;
        case 0x2C281DAE:
            Log(LogLevel::Debug, "Identified G711 SDK v3 DSP UCode\n");
            return UCodeID::G711_SDK_V3;
        case 0x63CAEC33:
            Log(LogLevel::Debug, "Identified Graphics SDK v3 DSP UCode\n");
            return UCodeID::GRAPHICS_SDK_V3;
        case 0x2A1D7F94:
            Log(LogLevel::Debug, "Identified G711 SDK v4 DSP UCode\n");
            return UCodeID::G711_SDK_V4;
        case 0x1451EB84:
            Log(LogLevel::Debug, "Identified Graphics SDK v4 DSP UCode\n");
            return UCodeID::GRAPHICS_SDK_V4;
        case 0x4EBEB519:
            Log(LogLevel::Debug, "Identified G711 SDK v5 DSP UCode\n");
            return UCodeID::G711_SDK_V5;
        case 0x2C974FC8:
            Log(LogLevel::Debug, "Identified Graphics SDK v5 DSP UCode\n");
            return UCodeID::GRAPHICS_SDK_V5;
        default:
            Log(LogLevel::Debug, "Unknown DSP UCode (CRC = %08X)\n", crc);
            return UCodeID::UNKNOWN;
    }
}

}