/*
    Copyright 2016-2021 Arisotura, WaluigiWare64

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

#ifndef NDS_HEADER_H
#define NDS_HEADER_H

#include "types.h"

// Consult GBATEK for info on what these are
struct NDSHeader
{
    char GameTitle[12];
    char GameCode[4];
    char MakerCode[2];
    u8 UnitCode;
    u8 EncryptionSeedSelect;
    u8 CardSize;
    u8 Reserved1[8];
    u8 NDSRegion;
    u8 ROMVersion;
    u8 Autostart;

    u32 ARM9ROMOffset;
    u32 ARM9EntryAddress;
    u32 ARM9RAMAddress;
    u32 ARM9Size;

    u32 ARM7ROMOffset;
    u32 ARM7EntryAddress;
    u32 ARM7RAMAddress;
    u32 ARM7Size;
    
    u32 FNTOffset;
    u32 FNTSize;
    u32 FATOffset;
    u32 FATSize;

    u32 ARM9OverlayOffset;
    u32 ARM9OverlaySize;
    u32 ARM7OverlayOffset;
    u32 ARM7OverlaySize;
    
    u32 NormalCommandSettings;
    u32 Key1CommandSettings;
    
    u32 BannerOffset;
    
    u16 SecureAreaCRC16;
    u16 SecureAreaDelay;
    
    // GBATEK lists the following two with a question mark
    u32 ARM9AutoLoadListAddress;
    u32 ARM7AutoLoadListAddress;
    
    u64 SecureAreaDisable;
    
    u32 ROMSize;
    u32 HeaderSize;
    
    u32 Unknown1;
    u8 Reserved2[52];
    
    u8 NintendoLogo[156];
    u16 NintendoLogoCRC16;
    u16 HeaderCRC16;
    
    u32 DebugROMOffset;
    u32 DebugSize;
    u32 DebugRAMAddress;
    
    u32 Reserved4;
    u8 Reserved5[144];
};

static_assert(sizeof(NDSHeader) == 512, "NDSHeader is not 512 bytes!");

struct NDSBanner
{
    u16 Version;
    u16 CRC16[4];
    u8 Reserved1[22];
    u8 Icon[512];
    u16 Palette[16];

    char16_t JapaneseTitle[128];
    char16_t EnglishTitle[128];
    char16_t FrenchTitle[128];
    char16_t GermanTitle[128];
    char16_t ItalianTitle[128];
    char16_t SpanishTitle[128];
    char16_t ChineseTitle[128];
    char16_t KoreanTitle[128];

    u8 Reserved2[2048];

    u8 DSiIcon[8][512];
    u16 DSiPalette[8][16];
    u16 DSiSequence[64];
};

static_assert(sizeof(NDSBanner) == 9152, "NDSBanner is not 9152 bytes!");


#endif //NDS_HEADER_H
