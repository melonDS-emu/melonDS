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

#include "SPI_Firmware.h"
#include "SPI.h"

#include <string.h>

constexpr u8 BBINIT[0x69]
{
    0x03, 0x17, 0x40, 0x00, 0x1B, 0x6C, 0x48, 0x80, 0x38, 0x00, 0x35, 0x07, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC7, 0xBB, 0x01, 0x24, 0x7F,
    0x5A, 0x01, 0x3F, 0x01, 0x3F, 0x36, 0x1D, 0x00, 0x78, 0x35, 0x55, 0x12, 0x34, 0x1C, 0x00, 0x01,
    0x0E, 0x38, 0x03, 0x70, 0xC5, 0x2A, 0x0A, 0x08, 0x04, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFE,
    0xFE, 0xFE, 0xFE, 0xFC, 0xFC, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xF8, 0xF8, 0xF6, 0x00, 0x12, 0x14,
    0x12, 0x41, 0x23, 0x03, 0x04, 0x70, 0x35, 0x0E, 0x2C, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x12, 0x28, 0x1C
};

constexpr u8 RFINIT[0x29]
{
    0x31, 0x4C, 0x4F, 0x21, 0x00, 0x10, 0xB0, 0x08, 0xFA, 0x15, 0x26, 0xE6, 0xC1, 0x01, 0x0E, 0x50,
    0x05, 0x00, 0x6D, 0x12, 0x00, 0x00, 0x01, 0xFF, 0x0E, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x06,
    0x06, 0x00, 0x00, 0x00, 0x18, 0x00, 0x02, 0x00, 0x00
};

constexpr u8 CHANDATA[0x3C]
{
    0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x16,
    0x26, 0x1C, 0x1C, 0x1C, 0x1D, 0x1D, 0x1D, 0x1E, 0x1E, 0x1E, 0x1E, 0x1F, 0x1E, 0x1F, 0x18,
    0x01, 0x4B, 0x4B, 0x4B, 0x4B, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4D, 0x4D, 0x4D,
    0x02, 0x6C, 0x71, 0x76, 0x5B, 0x40, 0x45, 0x4A, 0x2F, 0x34, 0x39, 0x3E, 0x03, 0x08, 0x14
};

constexpr u8 DEFAULT_UNUSED3[6] { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

SPI_Firmware::WifiAccessPoint::WifiAccessPoint()
{
    memset(Bytes, 0, sizeof(Bytes));
    Status = AccessPointStatus::NotConfigured;
    ConnectionConfigured = 0x01;
    UpdateChecksum();
}

SPI_Firmware::WifiAccessPoint::WifiAccessPoint(int consoletype)
{
    memset(Bytes, 0, sizeof(Bytes));
    strncpy(SSID, DEFAULT_SSID, sizeof(SSID));
    if (consoletype == 1) Mtu = 1400;
    Status = AccessPointStatus::Normal;
    ConnectionConfigured = 0x01;
    UpdateChecksum();
}

void SPI_Firmware::WifiAccessPoint::UpdateChecksum()
{
    Checksum = CRC16(Bytes, 0xFE, 0x0000);
}

SPI_Firmware::ExtendedWifiAccessPoint::ExtendedWifiAccessPoint()
{
    Data.Base = WifiAccessPoint();

    UpdateChecksum();
}

void SPI_Firmware::ExtendedWifiAccessPoint::UpdateChecksum()
{
    Data.Base.UpdateChecksum();
    Data.ExtendedChecksum = CRC16(&Bytes[0x100], 0xFE, 0x0000);
}

SPI_Firmware::FirmwareHeader::FirmwareHeader(int consoletype)
{
    if (consoletype == 1)
    {
        ConsoleType = FirmwareConsoleType::DSi;
        WifiVersion = WifiVersion::W015;
        WifiBoard = WifiBoard::W015;
        WifiFlash = 0x20;

        // these need to be zero (part of the stage2 firmware signature!)
        memset(&Bytes[0x22], 0, 8);
    }
    else
    {
        ConsoleType = FirmwareConsoleType::DSLite; // TODO: make configurable?
        WifiVersion = WifiVersion::W006;
    }
    Identifier = GENERATED_FIRMWARE_IDENTIFIER;


    // wifi calibration
    WifiConfigLength = 0x138;
    Unused1 = 0;
    memcpy(&Unused3, DEFAULT_UNUSED3, sizeof(DEFAULT_UNUSED3));
    MacAddress = DEFAULT_MAC;
    EnabledChannels = 0x3FFE;
    memset(&Unknown2, 0xFF, sizeof(Unknown2));
    RFChipType = RFChipType::Type3;
    RFBitsPerEntry = 0x94;
    RFEntries = 0x29;
    Unknown3 = 0x02;
    *(u16*)&Bytes[0x44] = 0x0002;
    *(u16*)&Bytes[0x46] = 0x0017;
    *(u16*)&Bytes[0x48] = 0x0026;
    *(u16*)&Bytes[0x4A] = 0x1818;
    *(u16*)&Bytes[0x4C] = 0x0048;
    *(u16*)&Bytes[0x4E] = 0x4840;
    *(u16*)&Bytes[0x50] = 0x0058;
    *(u16*)&Bytes[0x52] = 0x0042;
    *(u16*)&Bytes[0x54] = 0x0146;
    *(u16*)&Bytes[0x56] = 0x8064;
    *(u16*)&Bytes[0x58] = 0xE6E6;
    *(u16*)&Bytes[0x5A] = 0x2443;
    *(u16*)&Bytes[0x5C] = 0x000E;
    *(u16*)&Bytes[0x5E] = 0x0001;
    *(u16*)&Bytes[0x60] = 0x0001;
    *(u16*)&Bytes[0x62] = 0x0402;
    memcpy(&InitialBBValues, BBINIT, sizeof(BBINIT));
    Unused4 = 0;
    memcpy(&Type3Config.InitialRFValues, RFINIT, sizeof(RFINIT));
    Type3Config.BBIndicesPerChannel = 0x02;
    memcpy(&Bytes[0xF8], CHANDATA, sizeof(CHANDATA));
    memset(&Type3Config.Unused0, 0xFF, sizeof(Type3Config.Unused0));

    UpdateChecksum();
}


void SPI_Firmware::FirmwareHeader::UpdateChecksum()
{
    WifiConfigChecksum = SPI_Firmware::CRC16(&Bytes[0x2C], WifiConfigLength, 0x0000);
}

SPI_Firmware::UserData::UserData()
{
    memset(Bytes, 0, 0x74);
    Version = 5;
    BirthdayMonth = 1;
    BirthdayDay = 1;
    Settings = Language::English | BacklightLevel::Max; // NOLINT(*-suspicious-enum-usage)
    memcpy(Nickname, DEFAULT_USERNAME.data(), DEFAULT_USERNAME.size() * sizeof(std::u16string_view::value_type));
    NameLength = DEFAULT_USERNAME.size();
    Checksum = CRC16(Bytes, 0x70, 0xFFFF);
}

void SPI_Firmware::UserData::UpdateChecksum()
{
    Checksum = CRC16(Bytes, 0x70, 0xFFFF);
    if (ExtendedSettings.Unknown0 == 0x01)
    {
        ExtendedSettings.Checksum = CRC16(&Bytes[0x74], 0x8A, 0xFFFF);
    }
}

SPI_Firmware::Firmware::Firmware(int consoletype)
{
    FirmwareBufferLength = DEFAULT_FIRMWARE_LENGTH;
    FirmwareBuffer = new u8[FirmwareBufferLength];
    memset(FirmwareBuffer, 0xFF, FirmwareBufferLength);
    FirmwareMask = FirmwareBufferLength - 1;

    memset(FirmwareBuffer, 0, 0x1D);
    FirmwareHeader& header = *reinterpret_cast<FirmwareHeader*>(FirmwareBuffer);
    header = FirmwareHeader(consoletype);
    FirmwareBuffer[0x2FF] = 0x80; // boot0: use NAND as stage2 medium

    // user data
    header.UserSettingsOffset = (0x7FE00 & FirmwareMask) >> 3;

    std::array<union UserData, 2>& settings = *reinterpret_cast<std::array<union UserData, 2>*>(UserDataPosition());
    settings = {
        SPI_Firmware::UserData(),
        SPI_Firmware::UserData(),
    };

    // wifi access points
    // TODO: WFC ID??

    std::array<WifiAccessPoint, 3>& accesspoints = *reinterpret_cast<std::array<WifiAccessPoint, 3>*>(WifiAccessPointPosition());

    accesspoints = {
        WifiAccessPoint(consoletype),
        WifiAccessPoint(),
        WifiAccessPoint(),
    };

    if (consoletype == 1)
    {
        std::array<ExtendedWifiAccessPoint, 3>& extendedaccesspoints = *reinterpret_cast<std::array<ExtendedWifiAccessPoint, 3>*>(ExtendedAccessPointPosition());

        extendedaccesspoints = {
            ExtendedWifiAccessPoint(),
            ExtendedWifiAccessPoint(),
            ExtendedWifiAccessPoint(),
        };
    }
}

SPI_Firmware::Firmware::Firmware(Platform::FileHandle* file) : FirmwareBuffer(nullptr), FirmwareBufferLength(0), FirmwareMask(0)
{
    if (file)
    {
        u64 length = Platform::FileLength(file);
        if (length > 0)
        {
            FirmwareBufferLength = FixFirmwareLength(length);
            FirmwareBuffer = new u8[FirmwareBufferLength];
            FirmwareMask = FirmwareBufferLength - 1;

            memset(FirmwareBuffer, 0, FirmwareBufferLength);
            Platform::FileRewind(file);
            if (!Platform::FileRead(FirmwareBuffer, length, 1, file))
            {
                delete[] FirmwareBuffer;
                FirmwareBuffer = nullptr;
                FirmwareBufferLength = 0;
                FirmwareMask = 0;
            }
        }

        Platform::FileRewind(file);
    }
}

SPI_Firmware::Firmware::Firmware(const u8* data, u32 length) : FirmwareBuffer(nullptr), FirmwareBufferLength(FixFirmwareLength(length))
{
    if (data)
    {
        FirmwareBuffer = new u8[FirmwareBufferLength];
        memcpy(FirmwareBuffer, data, FirmwareBufferLength);
        FirmwareMask = FirmwareBufferLength - 1;
    }
}

SPI_Firmware::Firmware::Firmware(const Firmware& other) : FirmwareBuffer(nullptr), FirmwareBufferLength(other.FirmwareBufferLength)
{
    FirmwareBuffer = new u8[FirmwareBufferLength];
    memcpy(FirmwareBuffer, other.FirmwareBuffer, FirmwareBufferLength);
    FirmwareMask = other.FirmwareMask;
}

SPI_Firmware::Firmware::Firmware(Firmware&& other) noexcept
{
    FirmwareBuffer = other.FirmwareBuffer;
    FirmwareBufferLength = other.FirmwareBufferLength;
    FirmwareMask = other.FirmwareMask;
    other.FirmwareBuffer = nullptr;
    other.FirmwareBufferLength = 0;
    other.FirmwareMask = 0;
}

SPI_Firmware::Firmware& SPI_Firmware::Firmware::operator=(const Firmware& other)
{
    if (this != &other)
    {
        delete[] FirmwareBuffer;
        FirmwareBufferLength = other.FirmwareBufferLength;
        FirmwareBuffer = new u8[other.FirmwareBufferLength];
        memcpy(FirmwareBuffer, other.FirmwareBuffer, other.FirmwareBufferLength);
        FirmwareMask = other.FirmwareMask;
    }

    return *this;
}

SPI_Firmware::Firmware& SPI_Firmware::Firmware::operator=(Firmware&& other) noexcept
{
    if (this != &other)
    {
        delete[] FirmwareBuffer;
        FirmwareBuffer = other.FirmwareBuffer;
        FirmwareBufferLength = other.FirmwareBufferLength;
        FirmwareMask = other.FirmwareMask;
        other.FirmwareBuffer = nullptr;
        other.FirmwareBufferLength = 0;
        other.FirmwareMask = 0;
    }

    return *this;
}

SPI_Firmware::Firmware::~Firmware()
{
    delete[] FirmwareBuffer;
}

bool SPI_Firmware::Firmware::IsBootable() const
{
    return
        FirmwareBufferLength != DEFAULT_FIRMWARE_LENGTH &&
        Header().Identifier != GENERATED_FIRMWARE_IDENTIFIER
    ;
}

const SPI_Firmware::UserData& SPI_Firmware::Firmware::EffectiveUserData() const {
    const std::array<union UserData, 2>& userdata = UserData();
    bool userdata0ChecksumOk = userdata[0].ChecksumValid();
    bool userdata1ChecksumOk = userdata[1].ChecksumValid();

    if (userdata0ChecksumOk && userdata1ChecksumOk)
    {
        return userdata[0].UpdateCounter > userdata[1].UpdateCounter ? userdata[0] : userdata[1];
    }
    else if (userdata0ChecksumOk)
    {
        return userdata[0];
    }
    else if (userdata1ChecksumOk)
    {
        return userdata[1];
    }
    else
    {
        return userdata[0];
    }
}

SPI_Firmware::UserData& SPI_Firmware::Firmware::EffectiveUserData() {
    std::array<union UserData, 2>& userdata = UserData();
    bool userdata0ChecksumOk = userdata[0].ChecksumValid();
    bool userdata1ChecksumOk = userdata[1].ChecksumValid();

    if (userdata0ChecksumOk && userdata1ChecksumOk)
    {
        return userdata[0].UpdateCounter > userdata[1].UpdateCounter ? userdata[0] : userdata[1];
    }
    else if (userdata0ChecksumOk)
    {
        return userdata[0];
    }
    else if (userdata1ChecksumOk)
    {
        return userdata[1];
    }
    else
    {
        return userdata[0];
    }
}

void SPI_Firmware::Firmware::UpdateChecksums()
{
    Header().UpdateChecksum();

    for (SPI_Firmware::WifiAccessPoint& ap : AccessPoints())
    {
        ap.UpdateChecksum();
    }

    if (Header().ConsoleType == FirmwareConsoleType::DSi)
    {
        for (SPI_Firmware::ExtendedWifiAccessPoint& eap : ExtendedAccessPoints())
        {
            eap.UpdateChecksum();
        }
    }

    for (SPI_Firmware::UserData& u : UserData())
    {
        u.UpdateChecksum();
    }
}