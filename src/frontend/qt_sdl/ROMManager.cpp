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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <codecvt>
#include <locale>
#include <memory>
#include <tuple>
#include <string>
#include <utility>
#include <fstream>

#include <QDateTime>
#include <QMessageBox>

#include <zstd.h>
#ifdef ARCHIVE_SUPPORT_ENABLED
#include "ArchiveUtil.h"
#endif
#include "ROMManager.h"
#include "Config.h"
#include "Platform.h"

#include "NDS.h"
#include "DSi.h"
#include "SPI.h"
#include "RTC.h"
#include "DSi_I2C.h"
#include "FreeBIOS.h"
#include "main.h"

using std::make_unique;
using std::pair;
using std::string;
using std::tie;
using std::unique_ptr;
using std::wstring_convert;
using namespace melonDS;
using namespace melonDS::Platform;

namespace ROMManager
{

int CartType = -1;
std::string BaseROMDir = "";
std::string BaseROMName = "";
std::string BaseAssetName = "";

int GBACartType = -1;
std::string BaseGBAROMDir = "";
std::string BaseGBAROMName = "";
std::string BaseGBAAssetName = "";

std::unique_ptr<SaveManager> NDSSave = nullptr;
std::unique_ptr<SaveManager> GBASave = nullptr;
std::unique_ptr<SaveManager> FirmwareSave = nullptr;

std::unique_ptr<Savestate> BackupState = nullptr;
bool SavestateLoaded = false;
std::string PreviousSaveFile = "";

ARCodeFile* CheatFile = nullptr;
bool CheatsOn = false;


int LastSep(const std::string& path)
{
    int i = path.length() - 1;
    while (i >= 0)
    {
        if (path[i] == '/' || path[i] == '\\')
            return i;

        i--;
    }

    return -1;
}

std::string GetAssetPath(bool gba, const std::string& configpath, const std::string& ext, const std::string& file = "")
{
    std::string result;

    if (configpath.empty())
        result = gba ? BaseGBAROMDir : BaseROMDir;
    else
        result = configpath;

    // cut off trailing slashes
    for (;;)
    {
        int i = result.length() - 1;
        if (i < 0) break;
        if (result[i] == '/' || result[i] == '\\')
            result.resize(i);
        else
            break;
    }

    if (!result.empty())
        result += '/';

    if (file.empty())
    {
        std::string& baseName = gba ? BaseGBAAssetName : BaseAssetName;
        if (baseName.empty())
            result += "firmware";
        else
            result += baseName;
    }
    else
    {
        result += file;
    }

    result += ext;

    return result;
}


QString VerifyDSBIOS()
{
    FileHandle* f;
    long len;

    f = Platform::OpenLocalFile(Config::BIOS9Path, FileMode::Read);
    if (!f) return "DS ARM9 BIOS was not found or could not be accessed. Check your emu settings.";

    len = FileLength(f);
    if (len != 0x1000)
    {
        CloseFile(f);
        return "DS ARM9 BIOS is not a valid BIOS dump.";
    }

    CloseFile(f);

    f = Platform::OpenLocalFile(Config::BIOS7Path, FileMode::Read);
    if (!f) return "DS ARM7 BIOS was not found or could not be accessed. Check your emu settings.";

    len = FileLength(f);
    if (len != 0x4000)
    {
        CloseFile(f);
        return "DS ARM7 BIOS is not a valid BIOS dump.";
    }

    CloseFile(f);

    return "";
}

QString VerifyDSiBIOS()
{
    FileHandle* f;
    long len;

    // TODO: check the first 32 bytes

    f = Platform::OpenLocalFile(Config::DSiBIOS9Path, FileMode::Read);
    if (!f) return "DSi ARM9 BIOS was not found or could not be accessed. Check your emu settings.";

    len = FileLength(f);
    if (len != 0x10000)
    {
        CloseFile(f);
        return "DSi ARM9 BIOS is not a valid BIOS dump.";
    }

    CloseFile(f);

    f = Platform::OpenLocalFile(Config::DSiBIOS7Path, FileMode::Read);
    if (!f) return "DSi ARM7 BIOS was not found or could not be accessed. Check your emu settings.";

    len = FileLength(f);
    if (len != 0x10000)
    {
        CloseFile(f);
        return "DSi ARM7 BIOS is not a valid BIOS dump.";
    }

    CloseFile(f);

    return "";
}

QString VerifyDSFirmware()
{
    FileHandle* f;
    long len;

    f = Platform::OpenLocalFile(Config::FirmwarePath, FileMode::Read);
    if (!f) return "DS firmware was not found or could not be accessed. Check your emu settings.";

    if (!Platform::CheckFileWritable(Config::FirmwarePath))
        return "DS firmware is unable to be written to.\nPlease check file/folder write permissions.";

    len = FileLength(f);
    if (len == 0x20000)
    {
        // 128KB firmware, not bootable
        CloseFile(f);
        // TODO report it somehow? detect in core?
        return "";
    }
    else if (len != 0x40000 && len != 0x80000)
    {
        CloseFile(f);
        return "DS firmware is not a valid firmware dump.";
    }

    CloseFile(f);

    return "";
}

QString VerifyDSiFirmware()
{
    FileHandle* f;
    long len;

    f = Platform::OpenLocalFile(Config::DSiFirmwarePath, FileMode::Read);
    if (!f) return "DSi firmware was not found or could not be accessed. Check your emu settings.";

    if (!Platform::CheckFileWritable(Config::FirmwarePath))
        return "DSi firmware is unable to be written to.\nPlease check file/folder write permissions.";

    len = FileLength(f);
    if (len != 0x20000)
    {
        // not 128KB
        // TODO: check whether those work
        CloseFile(f);
        return "DSi firmware is not a valid firmware dump.";
    }

    CloseFile(f);

    return "";
}

QString VerifyDSiNAND()
{
    FileHandle* f;
    long len;

    f = Platform::OpenLocalFile(Config::DSiNANDPath, FileMode::ReadWriteExisting);
    if (!f) return "DSi NAND was not found or could not be accessed. Check your emu settings.";

    if (!Platform::CheckFileWritable(Config::FirmwarePath))
        return "DSi NAND is unable to be written to.\nPlease check file/folder write permissions.";

    // TODO: some basic checks
    // check that it has the nocash footer, and all

    CloseFile(f);

    return "";
}

QString VerifySetup()
{
    QString res;

    if (Config::ExternalBIOSEnable)
    {
        res = VerifyDSBIOS();
        if (!res.isEmpty()) return res;
    }

    if (Config::ConsoleType == 1)
    {
        res = VerifyDSiBIOS();
        if (!res.isEmpty()) return res;

        if (Config::ExternalBIOSEnable)
        {
            res = VerifyDSiFirmware();
            if (!res.isEmpty()) return res;
        }

        res = VerifyDSiNAND();
        if (!res.isEmpty()) return res;
    }
    else
    {
        if (Config::ExternalBIOSEnable)
        {
            res = VerifyDSFirmware();
            if (!res.isEmpty()) return res;
        }
    }

    return "";
}

std::string GetEffectiveFirmwareSavePath(EmuThread* thread)
{
    if (!Config::ExternalBIOSEnable)
    {
        return Config::WifiSettingsPath;
    }
    if (thread->NDS->ConsoleType == 1)
    {
        return Config::DSiFirmwarePath;
    }
    else
    {
        return Config::FirmwarePath;
    }
}

// Initializes the firmware save manager with the selected firmware image's path
// OR the path to the wi-fi settings.
void InitFirmwareSaveManager(EmuThread* thread) noexcept
{
    FirmwareSave = std::make_unique<SaveManager>(GetEffectiveFirmwareSavePath(thread));
}

std::string GetSavestateName(int slot)
{
    std::string ext = ".ml";
    ext += (char)('0'+slot);
    return GetAssetPath(false, Config::SavestatePath, ext);
}

bool SavestateExists(int slot)
{
    std::string ssfile = GetSavestateName(slot);
    return Platform::FileExists(ssfile);
}

bool LoadState(NDS& nds, const std::string& filename)
{
    FILE* file = fopen(filename.c_str(), "rb");
    if (file == nullptr)
    { // If we couldn't open the state file...
        Platform::Log(Platform::LogLevel::Error, "Failed to open state file \"%s\"\n", filename.c_str());
        return false;
    }

    std::unique_ptr<Savestate> backup = std::make_unique<Savestate>(Savestate::DEFAULT_SIZE);
    if (backup->Error)
    { // If we couldn't allocate memory for the backup...
        Platform::Log(Platform::LogLevel::Error, "Failed to allocate memory for state backup\n");
        fclose(file);
        return false;
    }

    if (!nds.DoSavestate(backup.get()) || backup->Error)
    { // Back up the emulator's state. If that failed...
        Platform::Log(Platform::LogLevel::Error, "Failed to back up state, aborting load (from \"%s\")\n", filename.c_str());
        fclose(file);
        return false;
    }
    // We'll store the backup once we're sure that the state was loaded.
    // Now that we know the file and backup are both good, let's load the new state.

    // Get the size of the file that we opened
    if (fseek(file, 0, SEEK_END) != 0)
    {
        Platform::Log(Platform::LogLevel::Error, "Failed to seek to end of state file \"%s\"\n", filename.c_str());
        fclose(file);
        return false;
    }
    size_t size = ftell(file);
    rewind(file); // reset the filebuf's position

    // Allocate exactly as much memory as we need for the savestate
    std::vector<u8> buffer(size);
    if (fread(buffer.data(), size, 1, file) == 0)
    { // Read the state file into the buffer. If that failed...
        Platform::Log(Platform::LogLevel::Error, "Failed to read %u-byte state file \"%s\"\n", size, filename.c_str());
        fclose(file);
        return false;
    }
    fclose(file); // done with the file now

    // Get ready to load the state from the buffer into the emulator
    std::unique_ptr<Savestate> state = std::make_unique<Savestate>(buffer.data(), size, false);

    if (!nds.DoSavestate(state.get()) || state->Error)
    { // If we couldn't load the savestate from the buffer...
        Platform::Log(Platform::LogLevel::Error, "Failed to load state file \"%s\" into emulator\n", filename.c_str());
        return false;
    }

    // The backup was made and the state was loaded, so we can store the backup now.
    BackupState = std::move(backup); // This will clean up any existing backup
    assert(backup == nullptr);

    if (Config::SavestateRelocSRAM && NDSSave)
    {
        PreviousSaveFile = NDSSave->GetPath();

        std::string savefile = filename.substr(LastSep(filename)+1);
        savefile = GetAssetPath(false, Config::SaveFilePath, ".sav", savefile);
        savefile += Platform::InstanceFileSuffix();
        NDSSave->SetPath(savefile, true);
    }

    SavestateLoaded = true;

    return true;
}

bool SaveState(NDS& nds, const std::string& filename)
{
    FILE* file = fopen(filename.c_str(), "wb");

    if (file == nullptr)
    { // If the file couldn't be opened...
        return false;
    }

    Savestate state;
    if (state.Error)
    { // If there was an error creating the state (and allocating its memory)...
        fclose(file);
        return false;
    }

    // Write the savestate to the in-memory buffer
    nds.DoSavestate(&state);

    if (state.Error)
    {
        fclose(file);
        return false;
    }

    if (fwrite(state.Buffer(), state.Length(), 1, file) == 0)
    { // Write the Savestate buffer to the file. If that fails...
        Platform::Log(Platform::Error,
            "Failed to write %d-byte savestate to %s\n",
            state.Length(),
            filename.c_str()
        );
        fclose(file);
        return false;
    }

    fclose(file);

    if (Config::SavestateRelocSRAM && NDSSave)
    {
        std::string savefile = filename.substr(LastSep(filename)+1);
        savefile = GetAssetPath(false, Config::SaveFilePath, ".sav", savefile);
        savefile += Platform::InstanceFileSuffix();
        NDSSave->SetPath(savefile, false);
    }

    return true;
}

void UndoStateLoad(NDS& nds)
{
    if (!SavestateLoaded || !BackupState) return;

    // Rewind the backup state and put it in load mode
    BackupState->Rewind(false);
    // pray that this works
    // what do we do if it doesn't???
    // but it should work.
    nds.DoSavestate(BackupState.get());

    if (NDSSave && (!PreviousSaveFile.empty()))
    {
        NDSSave->SetPath(PreviousSaveFile, true);
    }
}


void UnloadCheats(NDS& nds)
{
    if (CheatFile)
    {
        delete CheatFile;
        CheatFile = nullptr;
        nds.AREngine.SetCodeFile(nullptr);
    }
}

void LoadCheats(NDS& nds)
{
    UnloadCheats(nds);

    std::string filename = GetAssetPath(false, Config::CheatFilePath, ".mch");

    // TODO: check for error (malformed cheat file, ...)
    CheatFile = new ARCodeFile(filename);

    nds.AREngine.SetCodeFile(CheatsOn ? CheatFile : nullptr);
}

std::unique_ptr<ARM9BIOSImage> LoadARM9BIOS() noexcept
{
    if (!Config::ExternalBIOSEnable)
    {
        return Config::ConsoleType == 0 ? std::make_unique<ARM9BIOSImage>(bios_arm9_bin) : nullptr;
    }

    if (FileHandle* f = OpenLocalFile(Config::BIOS9Path, Read))
    {
        std::unique_ptr<ARM9BIOSImage> bios = std::make_unique<ARM9BIOSImage>();
        FileRewind(f);
        FileRead(bios->data(), bios->size(), 1, f);
        CloseFile(f);
        Log(Info, "ARM9 BIOS loaded from %s\n", Config::BIOS9Path.c_str());
        return bios;
    }

    Log(Warn, "ARM9 BIOS not found\n");
    return nullptr;
}

std::unique_ptr<ARM7BIOSImage> LoadARM7BIOS() noexcept
{
    if (!Config::ExternalBIOSEnable)
    {
        return Config::ConsoleType == 0 ? std::make_unique<ARM7BIOSImage>(bios_arm7_bin) : nullptr;
    }

    if (FileHandle* f = OpenLocalFile(Config::BIOS7Path, Read))
    {
        std::unique_ptr<ARM7BIOSImage> bios = std::make_unique<ARM7BIOSImage>();
        FileRead(bios->data(), bios->size(), 1, f);
        CloseFile(f);
        Log(Info, "ARM7 BIOS loaded from %s\n", Config::BIOS7Path.c_str());
        return bios;
    }

    Log(Warn, "ARM7 BIOS not found\n");
    return nullptr;
}

std::unique_ptr<DSiBIOSImage> LoadDSiARM9BIOS() noexcept
{
    if (FileHandle* f = OpenLocalFile(Config::DSiBIOS9Path, Read))
    {
        std::unique_ptr<DSiBIOSImage> bios = std::make_unique<DSiBIOSImage>();
        FileRead(bios->data(), bios->size(), 1, f);
        CloseFile(f);

        if (!Config::DSiFullBIOSBoot)
        {
            // herp
            *(u32*)bios->data() = 0xEAFFFFFE; // overwrites the reset vector

            // TODO!!!!
            // hax the upper 32K out of the goddamn DSi
            // done that :)  -pcy
        }
        Log(Info, "ARM9i BIOS loaded from %s\n", Config::DSiBIOS9Path.c_str());
        return bios;
    }

    Log(Warn, "ARM9i BIOS not found\n");
    return nullptr;
}

std::unique_ptr<DSiBIOSImage> LoadDSiARM7BIOS() noexcept
{
    if (FileHandle* f = OpenLocalFile(Config::DSiBIOS7Path, Read))
    {
        std::unique_ptr<DSiBIOSImage> bios = std::make_unique<DSiBIOSImage>();
        FileRead(bios->data(), bios->size(), 1, f);
        CloseFile(f);

        if (!Config::DSiFullBIOSBoot)
        {
            // herp
            *(u32*)bios->data() = 0xEAFFFFFE; // overwrites the reset vector

            // TODO!!!!
            // hax the upper 32K out of the goddamn DSi
            // done that :)  -pcy
        }
        Log(Info, "ARM7i BIOS loaded from %s\n", Config::DSiBIOS7Path.c_str());
        return bios;
    }

    Log(Warn, "ARM7i BIOS not found\n");
    return nullptr;
}

Firmware GenerateFirmware(int type) noexcept
{
    // Construct the default firmware...
    string settingspath;
    Firmware firmware = Firmware(type);
    assert(firmware.Buffer() != nullptr);

    // If using generated firmware, we keep the wi-fi settings on the host disk separately.
    // Wi-fi access point data includes Nintendo WFC settings,
    // and if we didn't keep them then the player would have to reset them in each session.
    // We don't need to save the whole firmware, just the part that may actually change.
    if (FileHandle* f = OpenLocalFile(Config::WifiSettingsPath, Read))
    {// If we have Wi-fi settings to load...
        constexpr unsigned TOTAL_WFC_SETTINGS_SIZE = 3 * (sizeof(Firmware::WifiAccessPoint) + sizeof(Firmware::ExtendedWifiAccessPoint));

        if (!FileRead(firmware.GetExtendedAccessPointPosition(), TOTAL_WFC_SETTINGS_SIZE, 1, f))
        { // If we couldn't read the Wi-fi settings from this file...
            Log(Warn, "Failed to read Wi-fi settings from \"%s\"; using defaults instead\n", Config::WifiSettingsPath.c_str());

            // The access point and extended access point segments might
            // be in different locations depending on the firmware revision,
            // but our generated firmware always keeps them next to each other.
            // (Extended access points first, then regular ones.)
            firmware.GetAccessPoints() = {
                Firmware::WifiAccessPoint(type),
                Firmware::WifiAccessPoint(),
                Firmware::WifiAccessPoint(),
            };

            firmware.GetExtendedAccessPoints() = {
                Firmware::ExtendedWifiAccessPoint(),
                Firmware::ExtendedWifiAccessPoint(),
                Firmware::ExtendedWifiAccessPoint(),
            };
            firmware.UpdateChecksums();
            CloseFile(f);
        }
    }

    CustomizeFirmware(firmware, true);

    // If we don't have Wi-fi settings to load,
    // then the defaults will have already been populated by the constructor.
    return firmware;
}

std::optional<Firmware> LoadFirmware(int type) noexcept
{
    if (!Config::ExternalBIOSEnable)
    { // If we're using built-in firmware...
        if (type == 1)
        {
            Log(Error, "DSi firmware: cannot use built-in firmware in DSi mode!\n");
            return std::nullopt;
        }

        return GenerateFirmware(type);
    }
    const string& firmwarepath = type == 1 ? Config::DSiFirmwarePath : Config::FirmwarePath;

    Log(Debug, "SPI firmware: loading from file %s\n", firmwarepath.c_str());

    FileHandle* file = OpenLocalFile(firmwarepath, Read);

    if (!file)
    {
        Log(Error, "SPI firmware: couldn't open firmware file!\n");
        return std::nullopt;
    }
    Firmware firmware(file);
    CloseFile(file);

    if (!firmware.Buffer())
    {
        Log(Error, "SPI firmware: couldn't read firmware file!\n");
        return std::nullopt;
    }

    CustomizeFirmware(firmware, Config::FirmwareOverrideSettings);

    return firmware;
}


std::optional<DSi_NAND::NANDImage> LoadNAND(const std::array<u8, DSiBIOSSize>& arm7ibios) noexcept
{
    FileHandle* nandfile = OpenLocalFile(Config::DSiNANDPath, ReadWriteExisting);
    if (!nandfile)
        return std::nullopt;

    DSi_NAND::NANDImage nandImage(nandfile, &arm7ibios[0x8308]);
    if (!nandImage)
    {
        Log(Error, "Failed to parse DSi NAND\n");
        return std::nullopt;
        // the NANDImage takes ownership of the FileHandle, no need to clean it up here
    }

    // scoped so that mount isn't alive when we move the NAND image to DSi::NANDImage
    {
        auto mount = DSi_NAND::NANDMount(nandImage);
        if (!mount)
        {
            Log(Error, "Failed to mount DSi NAND\n");
            return std::nullopt;
        }

        DSi_NAND::DSiFirmwareSystemSettings settings {};
        if (!mount.ReadUserData(settings))
        {
            Log(Error, "Failed to read DSi NAND user data\n");
            return std::nullopt;
        }

        // override user settings, if needed
        if (Config::FirmwareOverrideSettings)
        {
            // we store relevant strings as UTF-8, so we need to convert them to UTF-16
            auto converter = wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{};

            // setting up username
            std::u16string username = converter.from_bytes(Config::FirmwareUsername);
            size_t usernameLength = std::min(username.length(), (size_t) 10);
            memset(&settings.Nickname, 0, sizeof(settings.Nickname));
            memcpy(&settings.Nickname, username.data(), usernameLength * sizeof(char16_t));

            // setting language
            settings.Language = static_cast<Firmware::Language>(Config::FirmwareLanguage);

            // setting up color
            settings.FavoriteColor = Config::FirmwareFavouriteColour;

            // setting up birthday
            settings.BirthdayMonth = Config::FirmwareBirthdayMonth;
            settings.BirthdayDay = Config::FirmwareBirthdayDay;

            // setup message
            std::u16string message = converter.from_bytes(Config::FirmwareMessage);
            size_t messageLength = std::min(message.length(), (size_t) 26);
            memset(&settings.Message, 0, sizeof(settings.Message));
            memcpy(&settings.Message, message.data(), messageLength * sizeof(char16_t));

            // TODO: make other items configurable?
        }

        // fix touchscreen coords
        settings.TouchCalibrationADC1 = {0, 0};
        settings.TouchCalibrationPixel1 = {0, 0};
        settings.TouchCalibrationADC2 = {255 << 4, 191 << 4};
        settings.TouchCalibrationPixel2 = {255, 191};

        settings.UpdateHash();

        if (!mount.ApplyUserData(settings))
        {
            Log(LogLevel::Error, "Failed to write patched DSi NAND user data\n");
            return std::nullopt;
        }
    }

    return nandImage;
}

constexpr u64 MB(u64 i)
{
    return i * 1024 * 1024;
}

constexpr u64 imgsizes[] = {0, MB(256), MB(512), MB(1024), MB(2048), MB(4096)};
std::optional<FATStorageArgs> GetDSiSDCardArgs() noexcept
{
    if (!Config::DSiSDEnable)
        return std::nullopt;

    return FATStorageArgs {
        Config::DSiSDPath,
        imgsizes[Config::DSiSDSize],
        Config::DSiSDReadOnly,
        Config::DSiSDFolderSync ? std::make_optional(Config::DSiSDFolderPath) : std::nullopt
    };
}

std::optional<FATStorage> LoadDSiSDCard() noexcept
{
    if (!Config::DSiSDEnable)
        return std::nullopt;

    return FATStorage(
        Config::DSiSDPath,
        imgsizes[Config::DSiSDSize],
        Config::DSiSDReadOnly,
        Config::DSiSDFolderSync ? std::make_optional(Config::DSiSDFolderPath) : std::nullopt
    );
}

std::optional<FATStorageArgs> GetDLDISDCardArgs() noexcept
{
    if (!Config::DLDIEnable)
        return std::nullopt;

    return FATStorageArgs{
        Config::DLDISDPath,
        imgsizes[Config::DLDISize],
        Config::DLDIReadOnly,
        Config::DLDIFolderSync ? std::make_optional(Config::DLDIFolderPath) : std::nullopt
    };
}

std::optional<FATStorage> LoadDLDISDCard() noexcept
{
    if (!Config::DLDIEnable)
        return std::nullopt;

    return FATStorage(*GetDLDISDCardArgs());
}

void EnableCheats(NDS& nds, bool enable)
{
    CheatsOn = enable;
    if (CheatFile)
        nds.AREngine.SetCodeFile(CheatsOn ? CheatFile : nullptr);
}

ARCodeFile* GetCheatFile()
{
    return CheatFile;
}


void SetBatteryLevels(NDS& nds)
{
    if (nds.ConsoleType == 1)
    {
        auto& dsi = static_cast<DSi&>(nds);
        dsi.I2C.GetBPTWL()->SetBatteryLevel(Config::DSiBatteryLevel);
        dsi.I2C.GetBPTWL()->SetBatteryCharging(Config::DSiBatteryCharging);
    }
    else
    {
        nds.SPI.GetPowerMan()->SetBatteryLevelOkay(Config::DSBatteryLevelOkay);
    }
}

void SetDateTime(NDS& nds)
{
    QDateTime hosttime = QDateTime::currentDateTime();
    QDateTime time = hosttime.addSecs(Config::RTCOffset);

    nds.RTC.SetDateTime(time.date().year(), time.date().month(), time.date().day(),
                          time.time().hour(), time.time().minute(), time.time().second());
}

void Reset(EmuThread* thread)
{
    thread->UpdateConsole(Keep {}, Keep {});

    if (Config::ConsoleType == 1) EjectGBACart(*thread->NDS);

    thread->NDS->Reset();
    SetBatteryLevels(*thread->NDS);
    SetDateTime(*thread->NDS);

    if ((CartType != -1) && NDSSave)
    {
        std::string oldsave = NDSSave->GetPath();
        std::string newsave = GetAssetPath(false, Config::SaveFilePath, ".sav");
        newsave += Platform::InstanceFileSuffix();
        if (oldsave != newsave)
            NDSSave->SetPath(newsave, false);
    }

    if ((GBACartType != -1) && GBASave)
    {
        std::string oldsave = GBASave->GetPath();
        std::string newsave = GetAssetPath(true, Config::SaveFilePath, ".sav");
        newsave += Platform::InstanceFileSuffix();
        if (oldsave != newsave)
            GBASave->SetPath(newsave, false);
    }

    InitFirmwareSaveManager(thread);
    if (FirmwareSave)
    {
        std::string oldsave = FirmwareSave->GetPath();
        string newsave;
        if (Config::ExternalBIOSEnable)
        {
            if (Config::ConsoleType == 1)
                newsave = Config::DSiFirmwarePath + Platform::InstanceFileSuffix();
            else
                newsave = Config::FirmwarePath + Platform::InstanceFileSuffix();
        }
        else
        {
            newsave = Config::WifiSettingsPath + Platform::InstanceFileSuffix();
        }

        if (oldsave != newsave)
        { // If the player toggled the ConsoleType or ExternalBIOSEnable...
            FirmwareSave->SetPath(newsave, true);
        }
    }

    if (!BaseROMName.empty())
    {
        if (Config::DirectBoot || thread->NDS->NeedsDirectBoot())
        {
            thread->NDS->SetupDirectBoot(BaseROMName);
        }
    }

    thread->NDS->Start();
}


bool BootToMenu(EmuThread* thread)
{
    // Keep whatever cart is in the console, if any.
    if (!thread->UpdateConsole(Keep {}, Keep {}))
        // Try to update the console, but keep the existing cart. If that fails...
        return false;

    // BIOS and firmware files are loaded, patched, and installed in UpdateConsole
    if (thread->NDS->NeedsDirectBoot())
        return false;

    InitFirmwareSaveManager(thread);
    thread->NDS->Reset();
    SetBatteryLevels(*thread->NDS);
    SetDateTime(*thread->NDS);
    return true;
}

u32 DecompressROM(const u8* inContent, const u32 inSize, unique_ptr<u8[]>& outContent)
{
    u64 realSize = ZSTD_getFrameContentSize(inContent, inSize);
    const u32 maxSize = 0x40000000;

    if (realSize == ZSTD_CONTENTSIZE_ERROR || (realSize > maxSize && realSize != ZSTD_CONTENTSIZE_UNKNOWN))
    {
        return 0;
    }

    if (realSize != ZSTD_CONTENTSIZE_UNKNOWN)
    {
        auto newOutContent = make_unique<u8[]>(realSize);
        u64 decompressed = ZSTD_decompress(newOutContent.get(), realSize, inContent, inSize);

        if (ZSTD_isError(decompressed))
        {
            outContent = nullptr;
            return 0;
        }

        outContent = std::move(newOutContent);
        return realSize;
    }
    else
    {
        ZSTD_DStream* dStream = ZSTD_createDStream();
        ZSTD_initDStream(dStream);

        ZSTD_inBuffer inBuf = {
            .src = inContent,
            .size = inSize,
            .pos = 0
        };

        const u32 startSize = 1024 * 1024 * 16;
        u8* partialOutContent = (u8*) malloc(startSize);

        ZSTD_outBuffer outBuf = {
                .dst = partialOutContent,
                .size = startSize,
                .pos = 0
        };

        size_t result;

        do
        {
            result = ZSTD_decompressStream(dStream, &outBuf, &inBuf);

            if (ZSTD_isError(result))
            {
                ZSTD_freeDStream(dStream);
                free(outBuf.dst);
                return 0;
            }

            // if result == 0 and not inBuf.pos < inBuf.size, go again to let zstd flush everything.
            if (result == 0)
                continue;

            if (outBuf.pos == outBuf.size)
            {
                outBuf.size *= 2;

                if (outBuf.size > maxSize)
                {
                    ZSTD_freeDStream(dStream);
                    free(outBuf.dst);
                    return 0;
                }

                outBuf.dst = realloc(outBuf.dst, outBuf.size);
            }
        } while (inBuf.pos < inBuf.size);

        outContent = make_unique<u8[]>(outBuf.pos);
        memcpy(outContent.get(), outBuf.dst, outBuf.pos);

        ZSTD_freeDStream(dStream);
        free(outBuf.dst);

        return outBuf.size;
    }
}

void ClearBackupState()
{
    if (BackupState != nullptr)
    {
        BackupState = nullptr;
    }
}

pair<unique_ptr<Firmware>, string> GenerateDefaultFirmware()
{
    // Construct the default firmware...
    string settingspath;
    std::unique_ptr<Firmware> firmware = std::make_unique<Firmware>(Config::ConsoleType);
    assert(firmware->Buffer() != nullptr);

    // Try to open the instanced Wi-fi settings, falling back to the regular Wi-fi settings if they don't exist.
    // We don't need to save the whole firmware, just the part that may actually change.
    std::string wfcsettingspath = Config::WifiSettingsPath;
    settingspath = wfcsettingspath + Platform::InstanceFileSuffix();
    FileHandle* f = Platform::OpenLocalFile(settingspath, FileMode::Read);
    if (!f)
    {
        settingspath = wfcsettingspath;
        f = Platform::OpenLocalFile(settingspath, FileMode::Read);
    }

    // If using generated firmware, we keep the wi-fi settings on the host disk separately.
    // Wi-fi access point data includes Nintendo WFC settings,
    // and if we didn't keep them then the player would have to reset them in each session.
    if (f)
    { // If we have Wi-fi settings to load...
        constexpr unsigned TOTAL_WFC_SETTINGS_SIZE = 3 * (sizeof(Firmware::WifiAccessPoint) + sizeof(Firmware::ExtendedWifiAccessPoint));

        // The access point and extended access point segments might
        // be in different locations depending on the firmware revision,
        // but our generated firmware always keeps them next to each other.
        // (Extended access points first, then regular ones.)

        if (!FileRead(firmware->GetExtendedAccessPointPosition(), TOTAL_WFC_SETTINGS_SIZE, 1, f))
        { // If we couldn't read the Wi-fi settings from this file...
            Platform::Log(Platform::LogLevel::Warn, "Failed to read Wi-fi settings from \"%s\"; using defaults instead\n", wfcsettingspath.c_str());

            firmware->GetAccessPoints() = {
                Firmware::WifiAccessPoint(Config::ConsoleType),
                Firmware::WifiAccessPoint(),
                Firmware::WifiAccessPoint(),
            };

            firmware->GetExtendedAccessPoints() = {
                Firmware::ExtendedWifiAccessPoint(),
                Firmware::ExtendedWifiAccessPoint(),
                Firmware::ExtendedWifiAccessPoint(),
            };
        }

        firmware->UpdateChecksums();

        CloseFile(f);
    }

    // If we don't have Wi-fi settings to load,
    // then the defaults will have already been populated by the constructor.
    return std::make_pair(std::move(firmware), std::move(wfcsettingspath));
}

bool ParseMacAddress(void* data)
{
    const std::string& mac_in = Config::FirmwareMAC;
    u8* mac_out = (u8*)data;

    int o = 0;
    u8 tmp = 0;
    for (int i = 0; i < 18; i++)
    {
        char c = mac_in[i];
        if (c == '\0') break;

        int n;
        if      (c >= '0' && c <= '9') n = c - '0';
        else if (c >= 'a' && c <= 'f') n = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') n = c - 'A' + 10;
        else continue;

        if (!(o & 1))
            tmp = n;
        else
            mac_out[o >> 1] = n | (tmp << 4);

        o++;
        if (o >= 12) return true;
    }

    return false;
}

void CustomizeFirmware(Firmware& firmware, bool overridesettings) noexcept
{
    if (overridesettings)
    {
        auto &currentData = firmware.GetEffectiveUserData();

        // setting up username
        std::string orig_username = Config::FirmwareUsername;
        if (!orig_username.empty())
        { // If the frontend defines a username, take it. If not, leave the existing one.
            std::u16string username = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(
                    orig_username);
            size_t usernameLength = std::min(username.length(), (size_t) 10);
            currentData.NameLength = usernameLength;
            memcpy(currentData.Nickname, username.data(), usernameLength * sizeof(char16_t));
        }

        auto language = static_cast<Firmware::Language>(Config::FirmwareLanguage);
        if (language != Firmware::Language::Reserved)
        { // If the frontend specifies a language (rather than using the existing value)...
            currentData.Settings &= ~Firmware::Language::Reserved; // ..clear the existing language...
            currentData.Settings |= language; // ...and set the new one.
        }

        // setting up color
        u8 favoritecolor = Config::FirmwareFavouriteColour;
        if (favoritecolor != 0xFF)
        {
            currentData.FavoriteColor = favoritecolor;
        }

        u8 birthmonth = Config::FirmwareBirthdayMonth;
        if (birthmonth != 0)
        { // If the frontend specifies a birth month (rather than using the existing value)...
            currentData.BirthdayMonth = birthmonth;
        }

        u8 birthday = Config::FirmwareBirthdayDay;
        if (birthday != 0)
        { // If the frontend specifies a birthday (rather than using the existing value)...
            currentData.BirthdayDay = birthday;
        }

        // setup message
        std::string orig_message = Config::FirmwareMessage;
        if (!orig_message.empty())
        {
            std::u16string message = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(
                    orig_message);
            size_t messageLength = std::min(message.length(), (size_t) 26);
            currentData.MessageLength = messageLength;
            memcpy(currentData.Message, message.data(), messageLength * sizeof(char16_t));
        }
    }

    MacAddress mac;
    bool rep = false;
    auto& header = firmware.GetHeader();

    memcpy(&mac, header.MacAddr.data(), sizeof(MacAddress));

    if (overridesettings)
    {
        MacAddress configuredMac;
        rep = ParseMacAddress(&configuredMac);
        rep &= (configuredMac != MacAddress());

        if (rep)
        {
            mac = configuredMac;
        }
    }

    int inst = Platform::InstanceID();
    if (inst > 0)
    {
        rep = true;
        mac[3] += inst;
        mac[4] += inst*0x44;
        mac[5] += inst*0x10;
    }

    if (rep)
    {
        mac[0] &= 0xFC; // ensure the MAC isn't a broadcast MAC
        header.MacAddr = mac;
        header.UpdateChecksum();
    }

    firmware.UpdateChecksums();
}

// Loads ROM data without parsing it. Works for GBA and NDS ROMs.
bool LoadROMData(const QStringList& filepath, std::unique_ptr<u8[]>& filedata, u32& filelen, string& basepath, string& romname) noexcept
{
    if (filepath.empty()) return false;

    if (int num = filepath.count(); num == 1)
    {
        // regular file

        std::string filename = filepath.at(0).toStdString();
        Platform::FileHandle* f = Platform::OpenFile(filename, FileMode::Read);
        if (!f) return false;

        long len = Platform::FileLength(f);
        if (len > 0x40000000)
        {
            Platform::CloseFile(f);
            return false;
        }

        Platform::FileRewind(f);
        filedata = make_unique<u8[]>(len);
        size_t nread = Platform::FileRead(filedata.get(), (size_t)len, 1, f);
        Platform::CloseFile(f);
        if (nread != 1)
        {
            filedata = nullptr;
            return false;
        }

        filelen = (u32)len;

        if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".zst")
        {
            filelen = DecompressROM(filedata.get(), len, filedata);

            if (filelen > 0)
            {
                filename = filename.substr(0, filename.length() - 4);
            }
            else
            {
                filedata = nullptr;
                filelen = 0;
                basepath = "";
                romname = "";
                return false;
            }
        }

        int pos = LastSep(filename);
        if(pos != -1)
            basepath = filename.substr(0, pos);

        romname = filename.substr(pos+1);
        return true;
    }
#ifdef ARCHIVE_SUPPORT_ENABLED
    else if (num == 2)
    {
        // file inside archive

        s32 lenread = Archive::ExtractFileFromArchive(filepath.at(0), filepath.at(1), filedata, &filelen);
        if (lenread < 0) return false;
        if (!filedata) return false;
        if (lenread != filelen)
        {
            filedata = nullptr;
            return false;
        }

        std::string std_archivepath = filepath.at(0).toStdString();
        basepath = std_archivepath.substr(0, LastSep(std_archivepath));

        std::string std_romname = filepath.at(1).toStdString();
        romname = std_romname.substr(LastSep(std_romname)+1);
        return true;
    }
#endif
    else
        return false;
}

QString GetSavErrorString(std::string& filepath, bool gba)
{
    std::string console = gba ? "GBA" : "DS";
    std::string err1 = "Unable to write to ";
    std::string err2 = " save.\nPlease check file/folder write permissions.\n\nAttempted to Access:\n";

    err1 += console + err2 + filepath;

    return QString::fromStdString(err1);
}

bool LoadROM(QMainWindow* mainWindow, EmuThread* emuthread, QStringList filepath, bool reset)
{
    unique_ptr<u8[]> filedata = nullptr;
    u32 filelen;
    std::string basepath;
    std::string romname;

    if (!LoadROMData(filepath, filedata, filelen, basepath, romname))
    {
        QMessageBox::critical(mainWindow, "melonDS", "Failed to load the DS ROM.");
        return false;
    }

    NDSSave = nullptr;

    BaseROMDir = basepath;
    BaseROMName = romname;
    BaseAssetName = romname.substr(0, romname.rfind('.'));

    u32 savelen = 0;
    std::unique_ptr<u8[]> savedata = nullptr;

    std::string savname = GetAssetPath(false, Config::SaveFilePath, ".sav");
    std::string origsav = savname;
    savname += Platform::InstanceFileSuffix();

    FileHandle* sav = Platform::OpenFile(savname, FileMode::Read);
    if (!sav)
    {
        if (!Platform::CheckFileWritable(origsav))
        {
            QMessageBox::critical(mainWindow, "melonDS", GetSavErrorString(origsav, false));
            return false;
        }

        sav = Platform::OpenFile(origsav, FileMode::Read);
    }
    else if (!Platform::CheckFileWritable(savname))
    {
        QMessageBox::critical(mainWindow, "melonDS", GetSavErrorString(savname, false));
        return false;
    }

    if (sav)
    {
        savelen = (u32)Platform::FileLength(sav);

        FileRewind(sav);
        savedata = std::make_unique<u8[]>(savelen);
        FileRead(savedata.get(), savelen, 1, sav);
        CloseFile(sav);
    }

    NDSCart::NDSCartArgs cartargs {
        // Don't load the SD card itself yet, because we don't know if
        // the ROM is homebrew or not.
        // So this is the card we *would* load if the ROM were homebrew.
        .SDCard = GetDLDISDCardArgs(),
        .SRAM = std::move(savedata),
        .SRAMLength = savelen,
    };

    auto cart = NDSCart::ParseROM(std::move(filedata), filelen, std::move(cartargs));
    if (!cart)
    {
        // If we couldn't parse the ROM...
        QMessageBox::critical(mainWindow, "melonDS", "Failed to load the DS ROM.");
        return false;
    }

    if (reset)
    {
        if (!emuthread->UpdateConsole(std::move(cart), Keep {}))
        {
            QMessageBox::critical(mainWindow, "melonDS", "Failed to load the DS ROM.");
            return false;
        }

        InitFirmwareSaveManager(emuthread);
        emuthread->NDS->Reset();

        if (Config::DirectBoot || emuthread->NDS->NeedsDirectBoot())
        { // If direct boot is enabled or forced...
            emuthread->NDS->SetupDirectBoot(romname);
        }

        SetBatteryLevels(*emuthread->NDS);
        SetDateTime(*emuthread->NDS);
    }
    else
    {
        assert(emuthread->NDS != nullptr);
        emuthread->NDS->SetNDSCart(std::move(cart));
    }

    CartType = 0;
    NDSSave = std::make_unique<SaveManager>(savname);
    LoadCheats(*emuthread->NDS);

    return true; // success
}

void EjectCart(NDS& nds)
{
    NDSSave = nullptr;

    UnloadCheats(nds);

    nds.EjectCart();

    CartType = -1;
    BaseROMDir = "";
    BaseROMName = "";
    BaseAssetName = "";
}

bool CartInserted()
{
    return CartType != -1;
}

QString CartLabel()
{
    if (CartType == -1)
        return "(none)";

    QString ret = QString::fromStdString(BaseROMName);

    int maxlen = 32;
    if (ret.length() > maxlen)
        ret = ret.left(maxlen-6) + "..." + ret.right(3);

    return ret;
}


bool LoadGBAROM(QMainWindow* mainWindow, NDS& nds, QStringList filepath)
{
    if (nds.ConsoleType == 1)
    {
        QMessageBox::critical(mainWindow, "melonDS", "The DSi doesn't have a GBA slot.");
        return false;
    }

    unique_ptr<u8[]> filedata = nullptr;
    u32 filelen;
    std::string basepath;
    std::string romname;

    if (!LoadROMData(filepath, filedata, filelen, basepath, romname))
    {
        QMessageBox::critical(mainWindow, "melonDS", "Failed to load the GBA ROM.");
        return false;
    }

    GBASave = nullptr;

    BaseGBAROMDir = basepath;
    BaseGBAROMName = romname;
    BaseGBAAssetName = romname.substr(0, romname.rfind('.'));

    u32 savelen = 0;
    std::unique_ptr<u8[]> savedata = nullptr;

    std::string savname = GetAssetPath(true, Config::SaveFilePath, ".sav");
    std::string origsav = savname;
    savname += Platform::InstanceFileSuffix();

    FileHandle* sav = Platform::OpenFile(savname, FileMode::Read);
    if (!sav)
    {
        if (!Platform::CheckFileWritable(origsav))
        {
            QMessageBox::critical(mainWindow, "melonDS", GetSavErrorString(origsav, true));
            return false;
        }

        sav = Platform::OpenFile(origsav, FileMode::Read);
    }
    else if (!Platform::CheckFileWritable(savname))
    {
        QMessageBox::critical(mainWindow, "melonDS", GetSavErrorString(savname, true));
        return false;
    }

    if (sav)
    {
        savelen = (u32)FileLength(sav);

        if (savelen > 0)
        {
            FileRewind(sav);
            savedata = std::make_unique<u8[]>(savelen);
            FileRead(savedata.get(), savelen, 1, sav);
        }
        CloseFile(sav);
    }

    auto cart = GBACart::ParseROM(std::move(filedata), filelen, std::move(savedata), savelen);
    if (!cart)
    {
        QMessageBox::critical(mainWindow, "melonDS", "Failed to load the GBA ROM.");
        return false;
    }

    nds.SetGBACart(std::move(cart));
    GBACartType = 0;
    GBASave = std::make_unique<SaveManager>(savname);
    return true;
}

void LoadGBAAddon(NDS& nds, int type)
{
    if (Config::ConsoleType == 1) return;

    GBASave = nullptr;

    nds.LoadGBAAddon(type);

    GBACartType = type;
    BaseGBAROMDir = "";
    BaseGBAROMName = "";
    BaseGBAAssetName = "";
}

void EjectGBACart(NDS& nds)
{
    GBASave = nullptr;

    nds.EjectGBACart();

    GBACartType = -1;
    BaseGBAROMDir = "";
    BaseGBAROMName = "";
    BaseGBAAssetName = "";
}

bool GBACartInserted()
{
    return GBACartType != -1;
}

QString GBACartLabel()
{
    if (Config::ConsoleType == 1) return "none (DSi)";

    switch (GBACartType)
    {
    case 0:
        {
            QString ret = QString::fromStdString(BaseGBAROMName);

            int maxlen = 32;
            if (ret.length() > maxlen)
                ret = ret.left(maxlen-6) + "..." + ret.right(3);

            return ret;
        }

    case GBAAddon_RAMExpansion:
        return "Memory expansion";
    }

    return "(none)";
}


void ROMIcon(const u8 (&data)[512], const u16 (&palette)[16], u32 (&iconRef)[32*32])
{
    u32 paletteRGBA[16];
    for (int i = 0; i < 16; i++)
    {
        u8 r = ((palette[i] >> 0)  & 0x1F) * 255 / 31;
        u8 g = ((palette[i] >> 5)  & 0x1F) * 255 / 31;
        u8 b = ((palette[i] >> 10) & 0x1F) * 255 / 31;
        u8 a = i ? 255 : 0;
        paletteRGBA[i] = r | (g << 8) | (b << 16) | (a << 24);
    }

    int count = 0;
    for (int ytile = 0; ytile < 4; ytile++)
    {
        for (int xtile = 0; xtile < 4; xtile++)
        {
            for (int ypixel = 0; ypixel < 8; ypixel++)
            {
                for (int xpixel = 0; xpixel < 8; xpixel++)
                {
                    u8 pal_index = count % 2 ? data[count/2] >> 4 : data[count/2] & 0x0F;
                    iconRef[ytile*256 + ypixel*32 + xtile*8 + xpixel] = paletteRGBA[pal_index];
                    count++;
                }
            }
        }
    }
}

#define SEQ_FLIPV(i) ((i & 0b1000000000000000) >> 15)
#define SEQ_FLIPH(i) ((i & 0b0100000000000000) >> 14)
#define SEQ_PAL(i) ((i & 0b0011100000000000) >> 11)
#define SEQ_BMP(i) ((i & 0b0000011100000000) >> 8)
#define SEQ_DUR(i) ((i & 0b0000000011111111) >> 0)

void AnimatedROMIcon(const u8 (&data)[8][512], const u16 (&palette)[8][16], const u16 (&sequence)[64], u32 (&animatedIconRef)[64][32*32], std::vector<int> &animatedSequenceRef)
{
    for (int i = 0; i < 64; i++)
    {
        if (!sequence[i])
            break;

        ROMIcon(data[SEQ_BMP(sequence[i])], palette[SEQ_PAL(sequence[i])], animatedIconRef[i]);
        u32* frame = animatedIconRef[i];

        if (SEQ_FLIPH(sequence[i]))
        {
            for (int x = 0; x < 32; x++)
            {
                for (int y = 0; y < 32/2; y++)
                {
                    std::swap(frame[x * 32 + y], frame[x * 32 + (32 - 1 - y)]);
                }
            }
        }
        if (SEQ_FLIPV(sequence[i]))
        {
            for (int x = 0; x < 32/2; x++)
            {
                for (int y = 0; y < 32; y++)
                {
                    std::swap(frame[x * 32 + y], frame[(32 - 1 - x) * 32 + y]);
                }
            }
        }

        for (int j = 0; j < SEQ_DUR(sequence[i]); j++)
            animatedSequenceRef.push_back(i);
    }
}

}
