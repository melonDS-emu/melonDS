/*
    Copyright 2016-2022 melonDS team, WaluigiWare64

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

#include <string.h>
#include "types.h"

/// Set to indicate the console regions that a ROM (including DSiWare)
/// can be played on.
enum RegionMask : u32
{
    NoRegion = 0,
    Japan = 1 << 0,
    USA = 1 << 1,
    Europe = 1 << 2,
    Australia = 1 << 3,
    China = 1 << 4,
    Korea = 1 << 5,
    Reserved = ~(Japan | USA | Europe | Australia | China | Korea),
    RegionFree = 0xFFFFFFFF,
};

// Consult GBATEK for info on what these are
struct NDSHeader
{
    char GameTitle[12];
    char GameCode[4];
    char MakerCode[2];
    u8 UnitCode;
    u8 EncryptionSeedSelect;
    u8 CardSize;
    u8 Reserved1[7];
    u8 DSiCryptoFlags;
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

    u32 ROMSize; // excluding DSi area
    u32 HeaderSize;

    // GBATEK lists the following two with a question mark
    u32 DSiARM9ParamTableOffset;
    u32 DSiARM7ParamTableOffset;

    // expressed in 0x80000-byte units
    u16 NDSRegionEnd;
    u16 DSiRegionStart;

    // specific to NAND games
    u16 NANDROMEnd;
    u16 NANDRWStart;

    u8 Reserved2[40];

    u8 NintendoLogo[156];
    u16 NintendoLogoCRC16;
    u16 HeaderCRC16;

    u32 DebugROMOffset;
    u32 DebugSize;
    u32 DebugRAMAddress;

    u32 Reserved4;
    u8 Reserved5[16];

    u32 DSiMBKSlots[5]; // global MBK1..MBK5 settings
    u32 DSiARM9MBKAreas[3]; // local MBK6..MBK8 settings for ARM9
    u32 DSiARM7MBKAreas[3]; // local MBK6..MBK8 settings for ARM7
    u8 DSiMBKWriteProtect[3]; // global MBK9 setting
    u8 DSiWRAMCntSetting; // global WRAMCNT setting

    RegionMask DSiRegionMask;
    u32 DSiPermissions[2];
    u8 Reserved6[3];
    u8 AppFlags; // flags at 1BF

    u32 DSiARM9iROMOffset;
    u32 Reserved7;
    u32 DSiARM9iRAMAddress;
    u32 DSiARM9iSize;

    u32 DSiARM7iROMOffset;
    u32 DSiSDMMCDeviceList;
    u32 DSiARM7iRAMAddress;
    u32 DSiARM7iSize;

    u32 DSiDigestNTROffset;
    u32 DSiDigestNTRSize;
    u32 DSiDigestTWLOffset;
    u32 DSiDigestTWLSize;
    u32 DSiDigestSecHashtblOffset;
    u32 DSiDigestSecHashtblSize;
    u32 DSiDigestBlkHashtblOffset;
    u32 DSiDigestBlkHashtblSize;
    u32 DSiDigestSecSize; // sector size in bytes
    u32 DSiDigestBlkSecCount; // sectors per block

    u32 DSiBannerSize;

    // ???
    u8 DSiShared0Size;
    u8 DSiShared1Size;
    u8 DSiEULARatings;
    u8 DSiUseRatings;
    u32 DSiTotalROMSize;
    u8 DSiShared2Size;
    u8 DSiShared3Size;
    u8 DSiShared4Size;
    u8 DSiShared5Size;

    // ???
    u32 DSiARM9iParamTableOffset;
    u32 DSiARM7iParamTableOffset;

    u32 DSiModcrypt1Offset;
    u32 DSiModcrypt1Size;
    u32 DSiModcrypt2Offset;
    u32 DSiModcrypt2Size;

    u32 DSiTitleIDLow;
    u32 DSiTitleIDHigh;

    u32 DSiPublicSavSize;
    u32 DSiPrivateSavSize;

    u8 Reserved8[176];

    u8 DSiAgeRatingFlags[16];

    // 0x300 - hashes (SHA1-HMAC)
    u8 DSiARM9Hash[20];
    u8 DSiARM7Hash[20];
    u8 DSiDigestMasterHash[20];
    u8 BannerHash[20];
    u8 DSiARM9iHash[20];
    u8 DSiARM7iHash[20];
    u8 HeaderBinariesHash[20]; // 0x160-byte header + ARM9/ARM7 binaries
    u8 ARM9OverlayHash[20]; // ARM9 overlay and NitroFAT
    u8 DSiARM9NoSecureHash[20]; // ARM9 binary without secure area

    u8 Reserved9[2636];

    // reserved and unchecked region at 0xE00
    u8 Reserved10[384];

    u8 HeaderSignature[128]; // RSA-SHA1 across 0x000..0xDFF

    /// @return \c true if this header represents a DSi title
    /// (either a physical cartridge or a DSiWare title).
    [[nodiscard]] bool IsDSi() const { return (UnitCode & 0x02) != 0; }
    [[nodiscard]] u32 GameCodeAsU32() const {
        return (u32)GameCode[3] << 24 |
               (u32)GameCode[2] << 16 |
               (u32)GameCode[1] << 8 |
               (u32)GameCode[0];
    }
    [[nodiscard]] bool IsHomebrew() const
    {
        return (ARM9ROMOffset < 0x4000) || (strncmp(GameCode, "####", 4) == 0);
    }

    /// @return \c true if this header represents a DSiWare title.
    [[nodiscard]] bool IsDSiWare() const { return IsDSi() && DSiRegionStart == 0; }
};

static_assert(sizeof(NDSHeader) == 4096, "NDSHeader is not 4096 bytes!");

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
