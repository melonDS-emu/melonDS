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
#include "EmuInstance.h"
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


MainWindow* topWindow = nullptr;

const string kWifiSettingsPath = "wfcsettings.bin";


EmuInstance::EmuInstance(int inst) : instanceID(inst),
    globalCfg(Config::GetGlobalTable()),
    localCfg(Config::GetLocalTable(inst))
{
    emuThread = new EmuThread();

    numWindows = 0;
    mainWindow = nullptr;
    for (int i = 0; i < kMaxWindows; i++)
        windowList[i] = nullptr;

    if (inst == 0) topWindow = nullptr;
    createWindow();

    emuThread->start();
    emuThread->emuPause();
}

EmuInstance::~EmuInstance()
{
    // TODO window cleanup and shit?

    emuThread->emuStop();
    emuThread->wait();
    delete emuThread;
}


void EmuInstance::createWindow()
{
    if (numWindows >= kMaxWindows)
    {
        // TODO
        return;
    }

    int id = -1;
    for (int i = 0; i < kMaxWindows; i++)
    {
        if (windowList[i]) continue;
        id = i;
        break;
    }

    if (id == -1)
        return;

    MainWindow* win = new MainWindow(topWindow);
    if (!topWindow) topWindow = win;
    if (!mainWindow) mainWindow = win;
    windowList[id] = win;
    numWindows++;

    emuThread->attachWindow(win);
}


int EmuInstance::lastSep(const std::string& path)
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

string EmuInstance::getAssetPath(bool gba, const string& configpath, const string& ext, const string& file = "")
{
    string result;

    if (configpath.empty())
        result = gba ? baseGBAROMDir : baseROMDir;
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
        std::string& baseName = gba ? baseGBAAssetName : baseAssetName;
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


QString EmuInstance::verifyDSBIOS()
{
    FileHandle* f;
    long len;

    f = Platform::OpenLocalFile(globalCfg.GetString("DS.BIOS9Path"), FileMode::Read);
    if (!f) return "DS ARM9 BIOS was not found or could not be accessed. Check your emu settings.";

    len = FileLength(f);
    if (len != 0x1000)
    {
        CloseFile(f);
        return "DS ARM9 BIOS is not a valid BIOS dump.";
    }

    CloseFile(f);

    f = Platform::OpenLocalFile(globalCfg.GetString("DS.BIOS7Path"), FileMode::Read);
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

QString EmuInstance::verifyDSiBIOS()
{
    FileHandle* f;
    long len;

    // TODO: check the first 32 bytes

    f = Platform::OpenLocalFile(globalCfg.GetString("DSi.BIOS9Path"), FileMode::Read);
    if (!f) return "DSi ARM9 BIOS was not found or could not be accessed. Check your emu settings.";

    len = FileLength(f);
    if (len != 0x10000)
    {
        CloseFile(f);
        return "DSi ARM9 BIOS is not a valid BIOS dump.";
    }

    CloseFile(f);

    f = Platform::OpenLocalFile(globalCfg.GetString("DSi.BIOS7Path"), FileMode::Read);
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

QString EmuInstance::verifyDSFirmware()
{
    FileHandle* f;
    long len;

    std::string fwpath = globalCfg.GetString("DS.FirmwarePath");

    f = Platform::OpenLocalFile(fwpath, FileMode::Read);
    if (!f) return "DS firmware was not found or could not be accessed. Check your emu settings.";

    if (!Platform::CheckFileWritable(fwpath))
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

QString EmuInstance::verifyDSiFirmware()
{
    FileHandle* f;
    long len;

    std::string fwpath = globalCfg.GetString("DSi.FirmwarePath");

    f = Platform::OpenLocalFile(fwpath, FileMode::Read);
    if (!f) return "DSi firmware was not found or could not be accessed. Check your emu settings.";

    if (!Platform::CheckFileWritable(fwpath))
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

QString EmuInstance::verifyDSiNAND()
{
    FileHandle* f;
    long len;

    std::string nandpath = globalCfg.GetString("DSi.NANDPath");

    f = Platform::OpenLocalFile(nandpath, FileMode::ReadWriteExisting);
    if (!f) return "DSi NAND was not found or could not be accessed. Check your emu settings.";

    if (!Platform::CheckFileWritable(nandpath))
        return "DSi NAND is unable to be written to.\nPlease check file/folder write permissions.";

    // TODO: some basic checks
    // check that it has the nocash footer, and all

    CloseFile(f);

    return "";
}

QString EmuInstance::verifySetup()
{
    QString res;

    bool extbios = globalCfg.GetBool("Emu.ExternalBIOSEnable");
    int console = globalCfg.GetInt("Emu.ConsoleType");

    if (extbios)
    {
        res = verifyDSBIOS();
        if (!res.isEmpty()) return res;
    }

    if (console == 1)
    {
        res = verifyDSiBIOS();
        if (!res.isEmpty()) return res;

        if (extbios)
        {
            res = verifyDSiFirmware();
            if (!res.isEmpty()) return res;
        }

        res = verifyDSiNAND();
        if (!res.isEmpty()) return res;
    }
    else
    {
        if (extbios)
        {
            res = verifyDSFirmware();
            if (!res.isEmpty()) return res;
        }
    }

    return "";
}


std::string EmuInstance::getEffectiveFirmwareSavePath()
{
    if (!globalCfg.GetBool("Emu.ExternalBIOSEnable"))
    {
        return kWifiSettingsPath;
    }
    if (nds->ConsoleType == 1)
    {
        return globalCfg.GetString("DSi.FirmwarePath");
    }
    else
    {
        return globalCfg.GetString("DS.FirmwarePath");
    }
}

// Initializes the firmware save manager with the selected firmware image's path
// OR the path to the wi-fi settings.
void EmuInstance::initFirmwareSaveManager() noexcept
{
    firmwareSave = std::make_unique<SaveManager>(getEffectiveFirmwareSavePath());
}

std::string EmuInstance::getSavestateName(int slot)
{
    std::string ext = ".ml";
    ext += (char)('0'+slot);
    return getAssetPath(false, Config::SavestatePath, ext);
}

bool EmuInstance::savestateExists(int slot)
{
    std::string ssfile = getSavestateName(slot);
    return Platform::FileExists(ssfile);
}

bool EmuInstance::loadState(const std::string& filename)
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

    if (!nds->DoSavestate(backup.get()) || backup->Error)
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

    if (!nds->DoSavestate(state.get()) || state->Error)
    { // If we couldn't load the savestate from the buffer...
        Platform::Log(Platform::LogLevel::Error, "Failed to load state file \"%s\" into emulator\n", filename.c_str());
        return false;
    }

    // The backup was made and the state was loaded, so we can store the backup now.
    backupState = std::move(backup); // This will clean up any existing backup
    assert(backup == nullptr);

    if (Config::SavestateRelocSRAM && ndsSave)
    {
        previousSaveFile = ndsSave->GetPath();

        std::string savefile = filename.substr(lastSep(filename)+1);
        savefile = getAssetPath(false, Config::SaveFilePath, ".sav", savefile);
        savefile += Platform::InstanceFileSuffix();
        ndsSave->SetPath(savefile, true);
    }

    savestateLoaded = true;

    return true;
}

bool EmuInstance::saveState(const std::string& filename)
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
    nds->DoSavestate(&state);

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

    if (Config::SavestateRelocSRAM && ndsSave)
    {
        std::string savefile = filename.substr(lastSep(filename)+1);
        savefile = getAssetPath(false, Config::SaveFilePath, ".sav", savefile);
        savefile += Platform::InstanceFileSuffix();
        ndsSave->SetPath(savefile, false);
    }

    return true;
}

void EmuInstance::undoStateLoad()
{
    if (!savestateLoaded || !backupState) return;

    // Rewind the backup state and put it in load mode
    backupState->Rewind(false);
    // pray that this works
    // what do we do if it doesn't???
    // but it should work.
    nds->DoSavestate(backupState.get());

    if (ndsSave && (!previousSaveFile.empty()))
    {
        ndsSave->SetPath(previousSaveFile, true);
    }
}


void EmuInstance::unloadCheats()
{
    if (cheatFile)
    {
        delete cheatFile;
        cheatFile = nullptr;
        nds->AREngine.SetCodeFile(nullptr);
    }
}

void EmuInstance::loadCheats()
{
    unloadCheats();

    std::string filename = getAssetPath(false, Config::CheatFilePath, ".mch");

    // TODO: check for error (malformed cheat file, ...)
    cheatFile = new ARCodeFile(filename);

    nds->AREngine.SetCodeFile(cheatsOn ? cheatFile : nullptr);
}

std::optional<std::array<u8, ARM9BIOSSize>> EmuInstance::loadARM9BIOS() noexcept
{
    if (!globalCfg.GetBool("Emu.ExternalBIOSEnable"))
    {
        return globalCfg.GetInt("Emu.ConsoleType") == 0 ? std::make_optional(bios_arm9_bin) : std::nullopt;
    }

    string path = globalCfg.GetString("DS.BIOS9Path");

    if (FileHandle* f = OpenLocalFile(path, Read))
    {
        std::array<u8, ARM9BIOSSize> bios {};
        FileRewind(f);
        FileRead(bios.data(), sizeof(bios), 1, f);
        CloseFile(f);
        Log(Info, "ARM9 BIOS loaded from %s\n", path.c_str());
        return bios;
    }

    Log(Warn, "ARM9 BIOS not found\n");
    return std::nullopt;
}

std::optional<std::array<u8, ARM7BIOSSize>> EmuInstance::loadARM7BIOS() noexcept
{
    if (!globalCfg.GetBool("Emu.ExternalBIOSEnable"))
    {
        return globalCfg.GetInt("Emu.ConsoleType") == 0 ? std::make_optional(bios_arm7_bin) : std::nullopt;
    }

    string path = globalCfg.GetString("DS.BIOS7Path");

    if (FileHandle* f = OpenLocalFile(path, Read))
    {
        std::array<u8, ARM7BIOSSize> bios {};
        FileRead(bios.data(), sizeof(bios), 1, f);
        CloseFile(f);
        Log(Info, "ARM7 BIOS loaded from %s\n", path.c_str());
        return bios;
    }

    Log(Warn, "ARM7 BIOS not found\n");
    return std::nullopt;
}

std::optional<std::array<u8, DSiBIOSSize>> EmuInstance::loadDSiARM9BIOS() noexcept
{
    string path = globalCfg.GetString("DSi.BIOS9Path");

    if (FileHandle* f = OpenLocalFile(path, Read))
    {
        std::array<u8, DSiBIOSSize> bios {};
        FileRead(bios.data(), sizeof(bios), 1, f);
        CloseFile(f);

        if (!globalCfg.GetBool("DSi.FullBIOSBoot"))
        {
            // herp
            *(u32*)&bios[0] = 0xEAFFFFFE; // overwrites the reset vector

            // TODO!!!!
            // hax the upper 32K out of the goddamn DSi
            // done that :)  -pcy
        }
        Log(Info, "ARM9i BIOS loaded from %s\n", path.c_str());
        return bios;
    }

    Log(Warn, "ARM9i BIOS not found\n");
    return std::nullopt;
}

std::optional<std::array<u8, DSiBIOSSize>> EmuInstance::loadDSiARM7BIOS() noexcept
{
    string path = globalCfg.GetString("DSi.BIOS7Path");

    if (FileHandle* f = OpenLocalFile(path, Read))
    {
        std::array<u8, DSiBIOSSize> bios {};
        FileRead(bios.data(), sizeof(bios), 1, f);
        CloseFile(f);

        if (!globalCfg.GetBool("DSi.FullBIOSBoot"))
        {
            // herp
            *(u32*)&bios[0] = 0xEAFFFFFE; // overwrites the reset vector

            // TODO!!!!
            // hax the upper 32K out of the goddamn DSi
            // done that :)  -pcy
        }
        Log(Info, "ARM7i BIOS loaded from %s\n", path.c_str());
        return bios;
    }

    Log(Warn, "ARM7i BIOS not found\n");
    return std::nullopt;
}

Firmware EmuInstance::generateFirmware(int type) noexcept
{
    // Construct the default firmware...
    string settingspath;
    Firmware firmware = Firmware(type);
    assert(firmware.Buffer() != nullptr);

    // If using generated firmware, we keep the wi-fi settings on the host disk separately.
    // Wi-fi access point data includes Nintendo WFC settings,
    // and if we didn't keep them then the player would have to reset them in each session.
    // We don't need to save the whole firmware, just the part that may actually change.
    if (FileHandle* f = OpenLocalFile(kWifiSettingsPath, Read))
    {// If we have Wi-fi settings to load...
        constexpr unsigned TOTAL_WFC_SETTINGS_SIZE = 3 * (sizeof(Firmware::WifiAccessPoint) + sizeof(Firmware::ExtendedWifiAccessPoint));

        if (!FileRead(firmware.GetExtendedAccessPointPosition(), TOTAL_WFC_SETTINGS_SIZE, 1, f))
        { // If we couldn't read the Wi-fi settings from this file...
            Log(Warn, "Failed to read Wi-fi settings from \"%s\"; using defaults instead\n", kWifiSettingsPath.c_str());

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

    customizeFirmware(firmware);

    // If we don't have Wi-fi settings to load,
    // then the defaults will have already been populated by the constructor.
    return firmware;
}

std::optional<Firmware> EmuInstance::loadFirmware(int type) noexcept
{
    if (!globalCfg.GetBool("Emu.ExternalBIOSEnable"))
    { // If we're using built-in firmware...
        if (type == 1)
        {
            Log(Error, "DSi firmware: cannot use built-in firmware in DSi mode!\n");
            return std::nullopt;
        }

        return generateFirmware(type);
    }
    //const string& firmwarepath = type == 1 ? Config::DSiFirmwarePath : Config::FirmwarePath;
    string firmwarepath;
    if (type == 1)
        firmwarepath = globalCfg.GetString("DSi.FirmwarePath");
    else
        firmwarepath = globalCfg.GetString("DS.FirmwarePath");

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

    if (Config::FirmwareOverrideSettings)
    {
        customizeFirmware(firmware);
    }

    return firmware;
}


std::optional<DSi_NAND::NANDImage> EmuInstance::loadNAND(const std::array<u8, DSiBIOSSize>& arm7ibios) noexcept
{
    string path = globalCfg.GetString("DSi.NANDPath");

    FileHandle* nandfile = OpenLocalFile(path, ReadWriteExisting);
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

std::optional<FATStorageArgs> EmuInstance::getSDCardArgs(const string& key) noexcept
{
    // key = DSi.SD or DLDI
    Config::Table sdopt = globalCfg.GetTable(key);

    if (!sdopt.GetBool("Enable"))
        return std::nullopt;

    return FATStorageArgs {
            sdopt.GetString("ImagePath"),
            imgsizes[sdopt.GetInt("ImageSize")],
            sdopt.GetBool("ReadOnly"),
            sdopt.GetBool("FolderSync") ? std::make_optional(sdopt.GetString("FolderPath")) : std::nullopt
    };
}

std::optional<FATStorage> EmuInstance::loadSDCard(const string& key) noexcept
{
    auto args = getSDCardArgs(key);
    if (!args.has_value())
        return std::nullopt;

    return FATStorage(args.value());
}

void EmuInstance::enableCheats(bool enable)
{
    cheatsOn = enable;
    if (cheatFile)
        nds->AREngine.SetCodeFile(cheatsOn ? cheatFile : nullptr);
}

ARCodeFile* EmuInstance::getCheatFile()
{
    return cheatFile;
}
