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

#ifndef MELONDS_SPI_FIRMWARE_H
#define MELONDS_SPI_FIRMWARE_H

#include <array>
#include <string_view>
#include "types.h"

namespace SPI_Firmware
{

using MacAddress = std::array<u8, 6>;
using IpAddress = std::array<u8, 4>;

constexpr unsigned DEFAULT_FIRMWARE_LENGTH = 0x20000;
constexpr MacAddress DEFAULT_MAC = {0x00, 0x09, 0xBF, 0x11, 0x22, 0x33};
constexpr unsigned MAX_SSID_LENGTH = 32;
constexpr std::u16string_view  DEFAULT_USERNAME = u"melonDS";
constexpr const char* const DEFAULT_SSID = "melonDS";

enum class WepMode : u8
{
    None = 0,
    Hex5 = 1,
    Hex13 = 2,
    Hex16 = 3,
    Ascii5 = 5,
    Ascii13 = 6,
    Ascii16 = 7,
};

enum class WpaMode : u8
{
    Normal = 0,
    WPA_WPA2 = 0x10,
    WPS_WPA = 0x13,
    Unused = 0xff,
};

enum class WpaSecurity : u8
{
    None = 0,
    WPA_TKIP = 4,
    WPA2_TKIP = 5,
    WPA_AES = 6,
    WPA2_AES = 7,
};

enum class AccessPointStatus : u8
{
    Normal = 0,
    Aoss = 1,
    NotConfigured = 0xff
};

/**
 * @see https://problemkaputt.de/gbatek.htm#dsfirmwarewifiinternetaccesspoints
 */
union WifiAccessPoint
{
    /**
     * Constructs an unconfigured access point.
     */
    WifiAccessPoint();

    /**
     * Constructs an access point configured with melonDS's defaults.
     */
    explicit WifiAccessPoint(int consoletype);
    void UpdateChecksum();
    u8 Bytes[256];
    struct
    {
        char ProxyUsername[32];
        char ProxyPassword[32];
        char SSID[32];
        char SSIDWEP64[32];
        u8 WEPKey1[16];
        u8 WEPKey2[16];
        u8 WEPKey3[16];
        u8 WEPKey4[16];
        IpAddress Address;
        IpAddress Gateway;
        IpAddress PrimaryDns;
        IpAddress SecondaryDns;
        u8 SubnetMask;
        u8 Unknown0[21];
        WepMode WepMode; // todo union
        AccessPointStatus Status;
        u8 SSIDLength;
        u8 Unknown1;
        u16 Mtu;
        u8 Unknown2[3];
        u8 ConnectionConfigured;
        u8 NintendoWFCID[6];
        u8 Unknown3[8];
        u16 Checksum;
    };
};

static_assert(sizeof(WifiAccessPoint) == 256, "WifiAccessPoint should be 256 bytes");

union ExtendedWifiAccessPoint
{
    ExtendedWifiAccessPoint();
    void UpdateChecksum();
    u8 Bytes[512];
    struct
    {
        WifiAccessPoint Base;

        // DSi-specific entries now
        u8 PrecomputedPSK[32];
        char WPAPassword[64];
        char Unused0[33];
        WpaSecurity Security;
        bool ProxyEnabled;
        bool ProxyAuthentication;
        char ProxyName[48];
        u8 Unused1[52];
        u16 ProxyPort;
        u8 Unused2[20];
        u16 ExtendedChecksum;
    } Data;
};

static_assert(sizeof(ExtendedWifiAccessPoint) == 512, "WifiAccessPoint should be 512 bytes");


enum class FirmwareConsoleType : u8
{
    DS = 0xFF,
    DSLite = 0x20,
    DSi = 0x57,
    iQueDS = 0x43,
    iQueDSLite = 0x63,
};

enum class WifiVersion : u8
{
    V1_4 = 0,
    V5 = 3,
    V6_7 = 5,
    W006 = 6,
    W015 = 15,
    W024 = 24,
    N3DS = 34,
};

enum class RFChipType : u8
{
    Type2 = 0x2,
    Type3 = 0x3,
};

enum class WifiBoard : u8
{
    W015 = 0x1,
    W024 = 0x2,
    W028 = 0x3,
    Unused = 0xff,
};

enum Language : u8
{
    Japanese = 0,
    English = 1,
    French = 2,
    German = 3,
    Italian = 4,
    Spanish = 5,
    Chinese = 6,
    Reserved = 7,
};

enum GBAScreen : u8
{
    Upper = 0,
    Lower = (1 << 3),
};

enum BacklightLevel : u8
{
    Low = 0,
    Medium = 1 << 4,
    High = 2 << 4,
    Max = 3 << 4
};

enum BootMenu : u8
{
    Manual = 0,
    Autostart = 1 << 6,
};

using FirmwareIdentifier = std::array<u8, 4>;
using MacAddress = std::array<u8, 6>;

constexpr FirmwareIdentifier GENERATED_FIRMWARE_IDENTIFIER = {'M', 'E', 'L', 'N'};

/**
 * @note GBATek says the header is actually 511 bytes;
 * this header struct is 512 bytes due to padding,
 * but the last byte is just the first byte of the firmware's code.
 * It doesn't affect the offset of any of the fields,
 * so leaving that last byte in there is harmless.
 * @see https://problemkaputt.de/gbatek.htm#dsfirmwareheader
 * @see https://problemkaputt.de/gbatek.htm#dsfirmwarewificalibrationdata
*/
union FirmwareHeader
{
    FirmwareHeader(int consoletype);
    void UpdateChecksum();
    u8 Bytes[512];
    struct
    {
        u16 ARM9GUICodeOffset;
        u16 ARM7WifiCodeOffset;
        u16 GUIWifiCodeChecksum;
        u16 BootCodeChecksum;

        FirmwareIdentifier Identifier;

        u16 ARM9BootCodeROMAddress;
        u16 ARM9BootCodeRAMAddress;
        u16 ARM7BootCodeRAMAddress;
        u16 ARM7BootCodeROMAddress;
        u16 ShiftAmounts;
        u16 DataGfxRomAddress;

        u8 BuildMinute;
        u8 BuildHour;
        u8 BuildDay;
        u8 BuildMonth;
        u8 BuildYear;

        FirmwareConsoleType ConsoleType;

        u8 Unused0[2];

        u16 UserSettingsOffset;
        u8 Unknown0[2];
        u8 Unknown1[2];
        u16 DataGfxChecksum;
        u8 Unused2[2];

        // Begin wi-fi settings
        u16 WifiConfigChecksum;
        u16 WifiConfigLength;
        u8 Unused1;
        WifiVersion WifiVersion;

        u8 Unused3[6];

        MacAddress MacAddress;

        u16 EnabledChannels;

        u8 Unknown2[2];

        RFChipType RFChipType;
        u8 RFBitsPerEntry;
        u8 RFEntries;
        u8 Unknown3;

        u8 InitialValues[32];
        u8 InitialBBValues[105];
        u8 Unused4;
        union
        {
            struct
            {
                u8 InitialRFValues[36];
                u8 InitialRF56Values[84];
                u8 InitialBB1EValues[14];
                u8 InitialRf9Values[14];
            } Type2Config;

            struct
            {
                u8 InitialRFValues[41];
                u8 BBIndicesPerChannel;
                u8 BBIndex1;
                u8 BBData1[14];
                u8 BBIndex2;
                u8 BBData2[14];
                u8 RFIndex1;
                u8 RFData1[14];
                u8 RFIndex2;
                u8 RFData2[14];
                u8 Unused0[46];
            } Type3Config;
        };

        u8 Unknown4;
        u8 Unused5;
        u8 Unused6[153];
        WifiBoard WifiBoard;
        u8 WifiFlash;
        u8 Unused7;
    };
};

static_assert(sizeof(FirmwareHeader) == 512, "FirmwareHeader should be 512 bytes");

struct ExtendedUserSettings
{
    char ID[8];
    u16 Checksum;
    u16 ChecksumLength;
    u8 Version;
    u8 UpdateCount;
    u8 BootMenuFlags;
    u8 GBABorder;
    u16 TemperatureCalibration0;
    u16 TemperatureCalibration1;
    u16 TemperatureCalibrationDegrees;
    u8 TemperatureFlags;
    u8 BacklightIntensity;
    u32 DateCenturyOffset;
    u8 DateMonthRecovery;
    u8 DateDayRecovery;
    u8 DateYearRecovery;
    u8 DateTimeFlags;
    char DateSeparator;
    char TimeSeparator;
    char DecimalSeparator;
    char ThousandsSeparator;
    u8 DaylightSavingsTimeNth;
    u8 DaylightSavingsTimeDay;
    u8 DaylightSavingsTimeOfMonth;
    u8 DaylightSavingsTimeFlags;
};

static_assert(sizeof(ExtendedUserSettings) == 0x28, "ExtendedUserSettings should be 40 bytes");

union UserData
{
    UserData();
    u8 Bytes[256];
    struct
    {
        u16 Version;
        u8 FavoriteColor;
        u8 BirthdayMonth;
        u8 BirthdayDay;
        u8 Unused0;
        char16_t Nickname[10];
        u16 NameLength;
        char16_t Message[26];
        u16 MessageLength;
        u8 AlarmHour;
        u8 AlarmMinute;
        u8 Unknown0[2];
        u8 AlarmFlags;
        u8 Unused1;
        u16 TouchCalibrationADC1[2];
        u8 TouchCalibrationPixel1[2];
        u16 TouchCalibrationADC2[2];
        u8 TouchCalibrationPixel2[2];
        u16 Settings;
        u8 Year;
        u8 Unknown1;
        u32 RTCOffset;
        u8 Unused2[4];
        u16 UpdateCounter;
        u16 Checksum;
        union
        {
            u8 Unused3[0x8C];
            struct
            {
                u8 Unknown0;
                Language ExtendedLanguage; // padded
                u16 SupportedLanguageMask;
                u8 Unused0[0x86];
                u16 ExtendedChecksum;
            } ExtendedSettings;
        };
    };
};
static_assert(sizeof(UserData) == 256, "UserData should be 256 bytes");

class Firmware
{
public:
    /**
     * Constructs a default firmware blob
     * filled with data necessary for booting and configuring NDS games.
     * Will not contain executable code.
     * @param consoletype Console type to use. 1 for DSi, 0 for NDS.
     */
    Firmware(int consoletype);

    /**
     * Constructs a firmware blob from a copy of the given data.
     * @param data Buffer containing the firmware data.
     * @param length Length of the buffer in bytes.
     * If too short, the firmware will be padded with zeroes.
     * If too long, the extra data will be ignored.
     */
    Firmware(const u8* data, u32 length);
    Firmware(const Firmware& other);
    Firmware(Firmware&& other) noexcept;
    Firmware& operator=(const Firmware& other);
    Firmware& operator=(Firmware&& other) noexcept;
    ~Firmware();

    [[nodiscard]] FirmwareHeader& Header() { return reinterpret_cast<FirmwareHeader&>(FirmwareBuffer); }
    [[nodiscard]] const FirmwareHeader& Header() const { return reinterpret_cast<const FirmwareHeader&>(FirmwareBuffer); }

    [[nodiscard]] const std::array<WifiAccessPoint, 3>& AccessPoints() const;
    [[nodiscard]] std::array<WifiAccessPoint, 3>& AccessPoints();

    [[nodiscard]] const std::array<ExtendedWifiAccessPoint, 3>& ExtendedAccessPoints() const;
    [[nodiscard]] std::array<ExtendedWifiAccessPoint, 3>& ExtendedAccessPoints();

    [[nodiscard]] u8* Buffer() { return FirmwareBuffer; }
    [[nodiscard]] const u8* Buffer() const { return FirmwareBuffer; }

    [[nodiscard]] u32 Length() const { return FirmwareBufferLength; }

    [[nodiscard]] const std::array<UserData, 2>& UserData() const;
    [[nodiscard]] std::array<union UserData, 2>& UserData();

    /**
     * @return Reference to whichever of the two user data sections
     * has the highest update counter.
     */
    [[nodiscard]] const union UserData& EffectiveUserData() const {
        return UserData()[0].UpdateCounter > UserData()[1].UpdateCounter ? UserData()[0] : UserData()[1];
    }

    /**
     * @return Reference to whichever of the two user data sections
     * has the highest update counter.
     */
    [[nodiscard]] union UserData& EffectiveUserData() {
        return UserData()[0].UpdateCounter > UserData()[1].UpdateCounter ? UserData()[0] : UserData()[1];
    }

    u32 Hold;
    u8 CurCmd;
    u32 DataPos;
    u8 Data;

    u8 StatusReg;
    u32 Addr;

private:
    u8* FirmwareBuffer;
    u32 FirmwareBufferLength;
    u32 FirmwareMask;
};
}

#endif //MELONDS_SPI_FIRMWARE_H