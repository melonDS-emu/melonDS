/*
    Copyright 2016-2022 melonDS team

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

#ifndef DSI_NAND_H
#define DSI_NAND_H

#include "types.h"
#include "NDS_Header.h"
#include "DSi_TMD.h"
#include "SPI_Firmware.h"
#include <array>
#include <vector>
#include <string>

namespace DSi_NAND
{

enum
{
    TitleData_PublicSav,
    TitleData_PrivateSav,
    TitleData_BannerSav,
};

union DSiFirmwareSystemSettings;
union DSiSerialData;
using DSiHardwareInfoN = std::array<u8, 0x9C>;

bool Init(u8* es_keyY);
void DeInit();

Platform::FileHandle* GetFile();

void GetIDs(u8* emmc_cid, u64& consoleid);

void ReadHardwareInfo(DSiSerialData& dataS, DSiHardwareInfoN& dataN);

void ReadUserData(DSiFirmwareSystemSettings& data);
void PatchUserData();

void ListTitles(u32 category, std::vector<u32>& titlelist);
bool TitleExists(u32 category, u32 titleid);
void GetTitleInfo(u32 category, u32 titleid, u32& version, NDSHeader* header, NDSBanner* banner);
bool ImportTitle(const char* appfile, const DSi_TMD::TitleMetadata& tmd, bool readonly);
bool ImportTitle(const u8* app, size_t appLength, const DSi_TMD::TitleMetadata& tmd, bool readonly);
void DeleteTitle(u32 category, u32 titleid);

u32 GetTitleDataMask(u32 category, u32 titleid);
bool ImportTitleData(u32 category, u32 titleid, int type, const char* file);
bool ExportTitleData(u32 category, u32 titleid, int type, const char* file);

typedef std::array<u8, 20> SHA1Hash;
typedef std::array<u8, 8> TitleID;

/// Firmware settings for the DSi, saved to the NAND as TWLCFG0.dat or TWLCFG1.dat.
/// The DSi mirrors this information to its own firmware for compatibility with NDS games.
/// @note The file is normally 16KiB, but only the first 432 bytes are used;
/// the rest is FF-padded.
/// This struct excludes the padding.
/// @see https://problemkaputt.de/gbatek.htm#dsisdmmcfirmwaresystemsettingsdatafiles
union DSiFirmwareSystemSettings
{
    struct
    {
        SHA1Hash Hash;
        u8 Zero00[108];
        u8 Version;
        u8 UpdateCounter;
        u8 Zero01[2];
        u32 BelowRAMAreaSize;
        u32 ConfigFlags;
        u8 Zero02;
        u8 CountryCode;
        SPI_Firmware::Language Language;
        u8 RTCYear;
        u32 RTCOffset;
        u8 Zero3[4];
        u8 EULAVersion;
        u8 Zero04[9];
        u8 AlarmHour;
        u8 AlarmMinute;
        u8 Zero05[2];
        bool AlarmEnable;
        u8 Zero06[2];
        u8 SystemMenuUsedTitleSlots;
        u8 SystemMenuFreeTitleSlots;
        u8 Unknown0;
        u8 Unknown1;
        u8 Zero07[3];
        TitleID SystemMenuMostRecentTitleID;
        std::array<u16, 2> TouchCalibrationADC1;
        std::array<u8, 2> TouchCalibrationPixel1;
        std::array<u16, 2> TouchCalibrationADC2;
        std::array<u8, 2> TouchCalibrationPixel2;
        u8 Unknown2[4];
        u8 Zero08[4];
        u8 FavoriteColor;
        u8 Zero09;
        u8 BirthdayMonth;
        u8 BirthdayDay;
        char16_t Nickname[11];
        char16_t Message[27];
        u8 ParentalControlsFlags;
        u8 Zero10[6];
        u8 ParentalControlsRegion;
        u8 ParentalControlsYearsOfAgeRating;
        u8 ParentalControlsSecretQuestion;
        u8 Unknown3;
        u8 Zero11[2];
        char ParentalControlsPIN[5];
        char16_t ParentalControlsSecretAnswer[65];
    };
    u8 Bytes[432];

    void UpdateHash();
};

static_assert(sizeof(DSiFirmwareSystemSettings) == 432, "DSiFirmwareSystemSettings must be exactly 432 bytes");

enum class ConsoleRegion : u8
{
    Japan,
    USA,
    Europe,
    Australia,
    China,
    Korea,
};

/// Data file saved to 0:/sys/HWINFO_S.dat.
/// @note The file is normally 16KiB, but only the first 164 bytes are used;
/// the rest is FF-padded.
/// This struct excludes the padding.
/// @see https://problemkaputt.de/gbatek.htm#dsisdmmcfirmwaremiscfiles
union DSiSerialData
{
    struct
    {
        u8 RsaSha1HMAC[0x80];
        u32 Version;
        u32 EntrySize;
        u32 SupportedLanguages;
        u8 Unknown0[4];
        ConsoleRegion Region;
        char Serial[12];
        u8 Unknown1[3];
        u8 TitleIDLSBs[4];
    };
    u8 Bytes[164];
};

static_assert(sizeof(DSiSerialData) == 164, "DSiSerialData must be exactly 164 bytes");

}

#endif // DSI_NAND_H
