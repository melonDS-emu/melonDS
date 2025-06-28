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

#ifndef DSI_DSP_UCODES_H
#define DSI_DSP_UCODES_H

namespace melonDS
{
class DSi;

// DSP ucodes are COFF files embed into the DSi ROM in some fashion
// DSP ucodes all appear to simply be from the DSi's SDK (as evidenced by the build paths in the COFF)
// Presumingly, developers could not actually make their own ucodes, that was reserved for Nintendo
enum class UCodeID
{
    // AAC decoder in DSi Sound app
    // COFF build timestamp: 8/4/2008
    // This appears to be from an extremely early version of the DSi's SDK
    // This SDK was presumingly never available to 3rd party developers, as the build timestamp is before the DSi's release
    // As such, nothing else appears to use this ucode
    AAC_SOUND_APP,

    // AAC decoder present in v0 SDK
    // COFF build timestamp: 10/21/2008
    // At least 1 DSiWare app (Futo Sutando Tsuki - Banbura DX Rajio) is known to contain this
    AAC_SDK_V0,

    // Graphics ucode present in v0 SDK
    // COFF build timestamp: 10/21/2008
    // As least 1 DSiWare app (Hobonichi Rosenzu 2010, rev 0) is known to contain this
    GRAPHICS_SDK_V0,

    // G711 encoder/decoder present in v1 SDK
    // COFF build timestamp: 2/13/2009
    // Appears to be a replacement for the AAC decoder
    G711_SDK_V1,

    // Graphics ucode present in v1 SDK
    // COFF build timestamp: 2/13/2009
    GRAPHICS_SDK_V1,

    // Graphics ucode present in v1 SDK
    // COFF build timestamp: 4/3/2009
    // Appears to be a patch against the previous graphics ucode
    // Known to sometimes be present alongside the SDK v1 G711 ucode
    GRAPHICS_SDK_V1_PATCH,

    // G711 encoder/decoder present in v2 SDK
    // COFF build timestamp: 4/8/2009
    G711_SDK_V2,

    // Graphics ucode present in v2 SDK
    // COFF build timestamp: 4/8/2009
    GRAPHICS_SDK_V2,

    // G711 encoder/decoder present in v3 SDK
    // COFF build timestamp: 9/16/2009
    G711_SDK_V3,

    // Graphics ucode present in v3 SDK
    // COFF build timestamp: 9/16/2009
    GRAPHICS_SDK_V3,

    // G711 encoder/decoder present in v4 SDK
    // COFF build timestamp: 11/9/2009
    G711_SDK_V4,

    // Graphics ucode present in v4 SDK
    // COFF build timestamp: 11/9/2009
    GRAPHICS_SDK_V4,

    // G711 encoder/decoder present in v5 SDK
    // COFF build timestamp: 1/13/2010
    G711_SDK_V5,

    // Graphics ucode present in v5 SDK
    // COFF build timestamp: 1/13/2010
    GRAPHICS_SDK_V5,

    // Unknown ucode...
    UNKNOWN = -1,
};

UCodeID IdentifyUCode(melonDS::DSi& dsi);

}

#endif // DSI_DSP_UCODES_H
