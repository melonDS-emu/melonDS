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
#include "fatfs/ff.h"
#include "NDS_Header.h"
#include "DSi_TMD.h"
#include "SPI_Firmware.h"
#include <array>
#include <memory>
#include <vector>
#include <string>

struct AES_ctx;

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
using DSiKey = std::array<u8, 16>;

class NANDImage
{
public:
    explicit NANDImage(Platform::FileHandle* nandfile, const DSiKey& es_keyY) noexcept;
    explicit NANDImage(Platform::FileHandle* nandfile, const u8* es_keyY) noexcept;
    ~NANDImage();
    NANDImage(const NANDImage&) = delete;
    NANDImage& operator=(const NANDImage&) = delete;

    NANDImage(NANDImage&& other) noexcept;
    NANDImage& operator=(NANDImage&& other) noexcept;

    Platform::FileHandle* GetFile() { return CurFile; }

    [[nodiscard]] const DSiKey& GetEMMCID() const noexcept { return eMMC_CID; }
    [[nodiscard]] u64 GetConsoleID() const noexcept { return ConsoleID; }
    [[nodiscard]] u64 GetLength() const noexcept { return Length; }

    explicit operator bool() const { return CurFile != nullptr; }
private:
    friend class NANDMount;
    void SetupFATCrypto(AES_ctx* ctx, u32 ctr);
    u32 ReadFATBlock(u64 addr, u32 len, u8* buf);
    u32 WriteFATBlock(u64 addr, u32 len, const u8* buf);
    bool ESEncrypt(u8* data, u32 len) const;
    bool ESDecrypt(u8* data, u32 len);
    Platform::FileHandle* CurFile = nullptr;
    DSiKey eMMC_CID;
    u64 ConsoleID;
    DSiKey FATIV;
    DSiKey FATKey;
    DSiKey ESKey;
    u64 Length;
};

class NANDMount
{
public:
    explicit NANDMount(NANDImage& nand) noexcept;
    ~NANDMount() noexcept;
    NANDMount(const NANDMount&) = delete;
    NANDMount& operator=(const NANDMount&) = delete;

    // Move constructor deleted so that the closure passed to FATFS can't be invalidated
    NANDMount(NANDMount&&) = delete;
    NANDMount& operator=(NANDMount&&) = delete;

    bool ReadSerialData(DSiSerialData& dataS);
    bool ReadHardwareInfoN(DSiHardwareInfoN& dataN);
    void ReadHardwareInfo(DSiSerialData& dataS, DSiHardwareInfoN& dataN);

    bool ReadUserData(DSiFirmwareSystemSettings& data);

    /// Saves the given system settings to the DSi NAND,
    /// to both TWLCFG0.dat and TWLCFG1.dat.
    bool ApplyUserData(const DSiFirmwareSystemSettings& data);

    void ListTitles(u32 category, std::vector<u32>& titlelist);
    bool TitleExists(u32 category, u32 titleid);
    void GetTitleInfo(u32 category, u32 titleid, u32& version, NDSHeader* header, NDSBanner* banner);
    bool ImportTitle(const char* appfile, const DSi_TMD::TitleMetadata& tmd, bool readonly);
    bool ImportTitle(const u8* app, size_t appLength, const DSi_TMD::TitleMetadata& tmd, bool readonly);
    void DeleteTitle(u32 category, u32 titleid);

    u32 GetTitleDataMask(u32 category, u32 titleid);
    bool ImportTitleData(u32 category, u32 titleid, int type, const char* file);
    bool ExportTitleData(u32 category, u32 titleid, int type, const char* file);

    bool ImportFile(const char* path, const u8* data, size_t len);
    bool ImportFile(const char* path, const char* in);
    bool ExportFile(const char* path, const char* out);
    void RemoveFile(const char* path);
    void RemoveDir(const char* path);

    explicit operator bool() const { return Image != nullptr && CurFS != nullptr; }
private:
    u32 GetTitleVersion(u32 category, u32 titleid);
    bool CreateTicket(const char* path, u32 titleid0, u32 titleid1, u8 version);
    bool CreateSaveFile(const char* path, u32 len);
    bool InitTitleFileStructure(const NDSHeader& header, const DSi_TMD::TitleMetadata& tmd, bool readonly);
    UINT FF_ReadNAND(BYTE* buf, LBA_t sector, UINT num);
    UINT FF_WriteNAND(const BYTE* buf, LBA_t sector, UINT num);

    NANDImage* Image;

    // We keep a pointer to CurFS because fatfs maintains a global pointer to it;
    // therefore if we embed the FATFS directly in the object,
    // we can't give it move semantics.
    std::unique_ptr<FATFS> CurFS;

};


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

/// Languages that the given NAND image supports.
/// @see https://problemkaputt.de/gbatek.htm#dsiregions
enum DSiSupportedLanguageMask : u32 {
    NoLanguagesSet = 0,
    JapaneseSupported = 1 << 0,
    EnglishSupported = 1 << 1,
    FrenchSupported = 1 << 2,
    GermanSupported = 1 << 3,
    ItalianSupported = 1 << 4,
    SpanishSupported = 1 << 5,
    ChineseSupported = 1 << 6,
    KoreanSupported = 1 << 7,

    JapanLanguages = JapaneseSupported,
    AmericaLanguages = EnglishSupported | FrenchSupported | SpanishSupported,
    EuropeLanguages = EnglishSupported | FrenchSupported | GermanSupported | ItalianSupported | SpanishSupported,
    AustraliaLanguages = EnglishSupported,

    // "Unknown (supposedly Chinese/Mandarin?, and maybe English or so)"
    ChinaLanguages = ChineseSupported | EnglishSupported,
    KoreaLanguages = KoreanSupported,
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
        DSiSupportedLanguageMask SupportedLanguages;
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
