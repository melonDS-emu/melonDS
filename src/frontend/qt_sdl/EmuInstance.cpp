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

void EmuInstance::setBatteryLevels()
{
    if (nds->ConsoleType == 1)
    {
        auto dsi = static_cast<DSi*>(nds);
        dsi->I2C.GetBPTWL()->SetBatteryLevel(Config::DSiBatteryLevel);
        dsi->I2C.GetBPTWL()->SetBatteryCharging(Config::DSiBatteryCharging);
    }
    else
    {
        nds->SPI.GetPowerMan()->SetBatteryLevelOkay(Config::DSBatteryLevelOkay);
    }
}

void EmuInstance::setDateTime()
{
    QDateTime hosttime = QDateTime::currentDateTime();
    QDateTime time = hosttime.addSecs(Config::RTCOffset);

    nds->RTC.SetDateTime(time.date().year(), time.date().month(), time.date().day(),
                         time.time().hour(), time.time().minute(), time.time().second());
}

/*std::unique_ptr<NDS> EmuInstance::createConsole(
        std::unique_ptr<melonDS::NDSCart::CartCommon>&& ndscart,
        std::unique_ptr<melonDS::GBACart::CartCommon>&& gbacart
) noexcept
{
    auto arm7bios = ROMManager::LoadARM7BIOS();
    if (!arm7bios)
        return nullptr;

    auto arm9bios = ROMManager::LoadARM9BIOS();
    if (!arm9bios)
        return nullptr;

    auto firmware = ROMManager::LoadFirmware(Config::ConsoleType);
    if (!firmware)
        return nullptr;

#ifdef JIT_ENABLED
    JITArgs jitargs {
            static_cast<unsigned>(Config::JIT_MaxBlockSize),
            Config::JIT_LiteralOptimisations,
            Config::JIT_BranchOptimisations,
            Config::JIT_FastMemory,
    };
#endif

#ifdef GDBSTUB_ENABLED
    GDBArgs gdbargs {
            static_cast<u16>(Config::GdbPortARM7),
            static_cast<u16>(Config::GdbPortARM9),
            Config::GdbARM7BreakOnStartup,
            Config::GdbARM9BreakOnStartup,
    };
#endif

    NDSArgs ndsargs {
            std::move(ndscart),
            std::move(gbacart),
            *arm9bios,
            *arm7bios,
            std::move(*firmware),
#ifdef JIT_ENABLED
            Config::JIT_Enable ? std::make_optional(jitargs) : std::nullopt,
#else
            std::nullopt,
#endif
            static_cast<AudioBitDepth>(Config::AudioBitDepth),
            static_cast<AudioInterpolation>(Config::AudioInterp),
#ifdef GDBSTUB_ENABLED
            Config::GdbEnabled ? std::make_optional(gdbargs) : std::nullopt,
#else
            std::nullopt,
#endif
    };

    if (Config::ConsoleType == 1)
    {
        auto arm7ibios = ROMManager::LoadDSiARM7BIOS();
        if (!arm7ibios)
            return nullptr;

        auto arm9ibios = ROMManager::LoadDSiARM9BIOS();
        if (!arm9ibios)
            return nullptr;

        auto nand = ROMManager::LoadNAND(*arm7ibios);
        if (!nand)
            return nullptr;

        auto sdcard = ROMManager::LoadDSiSDCard();
        DSiArgs args {
                std::move(ndsargs),
                *arm9ibios,
                *arm7ibios,
                std::move(*nand),
                std::move(sdcard),
                Config::DSiFullBIOSBoot,
        };

        args.GBAROM = nullptr;

        return std::make_unique<melonDS::DSi>(std::move(args));
    }

    return std::make_unique<melonDS::NDS>(std::move(ndsargs));
}*/

bool EmuInstance::updateConsole(UpdateConsoleNDSArgs&& ndsargs, UpdateConsoleGBAArgs&& gbaargs) noexcept
{
    // Let's get the cart we want to use;
    // if we wnat to keep the cart, we'll eject it from the existing console first.
    std::unique_ptr<NDSCart::CartCommon> nextndscart;
    if (std::holds_alternative<Keep>(ndsargs))
    { // If we want to keep the existing cart (if any)...
        nextndscart = nds ? nds->EjectCart() : nullptr;
        ndsargs = {};
    }
    else if (const auto ptr = std::get_if<std::unique_ptr<NDSCart::CartCommon>>(&ndsargs))
    {
        nextndscart = std::move(*ptr);
        ndsargs = {};
    }

    if (auto* cartsd = dynamic_cast<NDSCart::CartSD*>(nextndscart.get()))
    {
        // LoadDLDISDCard will return nullopt if the SD card is disabled;
        // SetSDCard will accept nullopt, which means no SD card
        cartsd->SetSDCard(getSDCardArgs("DLDI"));
    }

    std::unique_ptr<GBACart::CartCommon> nextgbacart;
    if (std::holds_alternative<Keep>(gbaargs))
    {
        nextgbacart = nds ? nds->EjectGBACart() : nullptr;
    }
    else if (const auto ptr = std::get_if<std::unique_ptr<GBACart::CartCommon>>(&gbaargs))
    {
        nextgbacart = std::move(*ptr);
        gbaargs = {};
    }

    int consoletype = globalCfg.GetInt("Emu.ConsoleType");

    if (!nds || nds->ConsoleType != consoletype)
    { // If we're switching between DS and DSi mode, or there's no console...
        // To ensure the destructor is called before a new one is created,
        // as the presence of global signal handlers still complicates things a bit
        /*NDS = nullptr;
        NDS::Current = nullptr;

        NDS = CreateConsole(std::move(nextndscart), std::move(nextgbacart));

        if (NDS == nullptr)
            return false;

        NDS->Reset();
        NDS::Current = NDS.get();

        return true;*/

        delete nds;

        if (consoletype == 1)
        {
            nds = new DSi();
        }
        else if (consoletype == 0)
        {
            nds = new NDS();
        }

        nds->Reset();
    }

    auto arm9bios = loadARM9BIOS();
    if (!arm9bios)
        return false;

    auto arm7bios = loadARM7BIOS();
    if (!arm7bios)
        return false;

    auto firmware = loadFirmware(consoletype);
    if (!firmware)
        return false;

    if (consoletype == 1)
    { // If the console we're updating is a DSi...
        DSi* dsi = static_cast<DSi*>(nds);

        auto arm9ibios = loadDSiARM9BIOS();
        if (!arm9ibios)
            return false;

        auto arm7ibios = loadDSiARM7BIOS();
        if (!arm7ibios)
            return false;

        auto nandimage = loadNAND(*arm7ibios);
        if (!nandimage)
            return false;

        auto dsisdcard = loadSDCard("DSi.SD");

        dsi->SetFullBIOSBoot(globalCfg.GetBool("DSi.FullBIOSBoot"));
        dsi->ARM7iBIOS = *arm7ibios;
        dsi->ARM9iBIOS = *arm9ibios;
        dsi->SetNAND(std::move(*nandimage));
        dsi->SetSDCard(std::move(dsisdcard));
        // We're moving the optional, not the card
        // (inserting std::nullopt here is okay, it means no card)

        dsi->EjectGBACart();
    }
    else if (consoletype == 0)
    {
        nds->SetGBACart(std::move(nextgbacart));
    }

#ifdef JIT_ENABLED
    Config::Table jitopt = globalCfg.GetTable("JIT");
    JITArgs jitargs {
            static_cast<unsigned>(jitopt.GetInt("MaxBlockSize")),
            jitopt.GetBool("LiteralOptimisations"),
            jitopt.GetBool("BranchOptimisations"),
            jitopt.GetBool("FastMemory"),
    };
    nds->SetJITArgs(jitopt.GetBool("Enable") ? std::make_optional(jitargs) : std::nullopt);
#endif

    // TODO GDB stub shit

    nds->SetARM7BIOS(*arm7bios);
    nds->SetARM9BIOS(*arm9bios);
    nds->SetFirmware(std::move(*firmware));
    nds->SetNDSCart(std::move(nextndscart));
    nds->SPU.SetInterpolation(static_cast<AudioInterpolation>(Config::AudioInterp));
    nds->SPU.SetDegrade10Bit(static_cast<AudioBitDepth>(Config::AudioBitDepth));

    NDS::Current = nds; // REMOVEME

    return true;
}

void EmuInstance::reset()
{
    updateConsole(Keep {}, Keep {});

    if (nds->ConsoleType == 1) ejectGBACart();

    nds->Reset();
    setBatteryLevels();
    setDateTime();

    if ((cartType != -1) && ndsSave)
    {
        std::string oldsave = ndsSave->GetPath();
        std::string newsave = getAssetPath(false, Config::SaveFilePath, ".sav");
        newsave += Platform::InstanceFileSuffix();
        if (oldsave != newsave)
            ndsSave->SetPath(newsave, false);
    }

    if ((gbaCartType != -1) && gbaSave)
    {
        std::string oldsave = gbaSave->GetPath();
        std::string newsave = getAssetPath(true, Config::SaveFilePath, ".sav");
        newsave += Platform::InstanceFileSuffix();
        if (oldsave != newsave)
            gbaSave->SetPath(newsave, false);
    }

    initFirmwareSaveManager();
    if (firmwareSave)
    {
        std::string oldsave = firmwareSave->GetPath();
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
            firmwareSave->SetPath(newsave, true);
        }
    }

    if (!baseROMName.empty())
    {
        if (Config::DirectBoot || nds->NeedsDirectBoot())
        {
            nds->SetupDirectBoot(baseROMName);
        }
    }
}


bool EmuInstance::bootToMenu()
{
    // Keep whatever cart is in the console, if any.
    if (!thread->UpdateConsole(Keep {}, Keep {}))
        // Try to update the console, but keep the existing cart. If that fails...
        return false;

    // BIOS and firmware files are loaded, patched, and installed in UpdateConsole
    if (nds->NeedsDirectBoot())
        return false;

    initFirmwareSaveManager();
    nds->Reset();
    setBatteryLevels();
    setDateTime();
    return true;
}

u32 EmuInstance::decompressROM(const u8* inContent, const u32 inSize, unique_ptr<u8[]>& outContent)
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

void EmuInstance::clearBackupState()
{
    if (backupState != nullptr)
    {
        backupState = nullptr;
    }
}

pair<unique_ptr<Firmware>, string> EmuInstance::generateDefaultFirmware()
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

bool EmuInstance::parseMacAddress(void* data)
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

void EmuInstance::customizeFirmware(Firmware& firmware) noexcept
{
    auto& currentData = firmware.GetEffectiveUserData();

    // setting up username
    std::string orig_username = Config::FirmwareUsername;
    if (!orig_username.empty())
    { // If the frontend defines a username, take it. If not, leave the existing one.
        std::u16string username = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(orig_username);
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
        std::u16string message = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(orig_message);
        size_t messageLength = std::min(message.length(), (size_t) 26);
        currentData.MessageLength = messageLength;
        memcpy(currentData.Message, message.data(), messageLength * sizeof(char16_t));
    }

    MacAddress mac;
    bool rep = false;
    auto& header = firmware.GetHeader();

    memcpy(&mac, header.MacAddr.data(), sizeof(MacAddress));


    MacAddress configuredMac;
    rep = parseMacAddress(&configuredMac);
    rep &= (configuredMac != MacAddress());

    if (rep)
    {
        mac = configuredMac;
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
bool EmuInstance::loadROMData(const QStringList& filepath, std::unique_ptr<u8[]>& filedata, u32& filelen, string& basepath, string& romname) noexcept
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
            filelen = decompressROM(filedata.get(), len, filedata);

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

        int pos = lastSep(filename);
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
        basepath = std_archivepath.substr(0, lastSep(std_archivepath));

        std::string std_romname = filepath.at(1).toStdString();
        romname = std_romname.substr(lastSep(std_romname)+1);
        return true;
    }
#endif
    else
        return false;
}

QString EmuInstance::getSavErrorString(std::string& filepath, bool gba)
{
    std::string console = gba ? "GBA" : "DS";
    std::string err1 = "Unable to write to ";
    std::string err2 = " save.\nPlease check file/folder write permissions.\n\nAttempted to Access:\n";

    err1 += console + err2 + filepath;

    return QString::fromStdString(err1);
}

bool EmuInstance::loadROM(QStringList filepath, bool reset)
{
    unique_ptr<u8[]> filedata = nullptr;
    u32 filelen;
    std::string basepath;
    std::string romname;

    if (!loadROMData(filepath, filedata, filelen, basepath, romname))
    {
        QMessageBox::critical(mainWindow, "melonDS", "Failed to load the DS ROM.");
        return false;
    }

    ndsSave = nullptr;

    baseROMDir = basepath;
    baseROMName = romname;
    baseAssetName = romname.substr(0, romname.rfind('.'));

    u32 savelen = 0;
    std::unique_ptr<u8[]> savedata = nullptr;

    std::string savname = getAssetPath(false, Config::SaveFilePath, ".sav");
    std::string origsav = savname;
    savname += Platform::InstanceFileSuffix();

    FileHandle* sav = Platform::OpenFile(savname, FileMode::Read);
    if (!sav)
    {
        if (!Platform::CheckFileWritable(origsav))
        {
            QMessageBox::critical(mainWindow, "melonDS", getSavErrorString(origsav, false));
            return false;
        }

        sav = Platform::OpenFile(origsav, FileMode::Read);
    }
    else if (!Platform::CheckFileWritable(savname))
    {
        QMessageBox::critical(mainWindow, "melonDS", getSavErrorString(savname, false));
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
            .SDCard = getSDCardArgs("DLDI"),
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

        initFirmwareSaveManager();
        nds->Reset();

        if (Config::DirectBoot || nds->NeedsDirectBoot())
        { // If direct boot is enabled or forced...
            nds->SetupDirectBoot(romname);
        }

        setBatteryLevels();
        setDateTime();
    }
    else
    {
        assert(nds != nullptr);
        nds->SetNDSCart(std::move(cart));
    }

    cartType = 0;
    ndsSave = std::make_unique<SaveManager>(savname);
    loadCheats();

    return true; // success
}

void EmuInstance::ejectCart()
{
    ndsSave = nullptr;

    unloadCheats();

    nds->EjectCart();

    cartType = -1;
    baseROMDir = "";
    baseROMName = "";
    baseAssetName = "";
}

bool EmuInstance::cartInserted()
{
    return cartType != -1;
}

QString EmuInstance::cartLabel()
{
    if (cartType == -1)
        return "(none)";

    QString ret = QString::fromStdString(baseROMName);

    int maxlen = 32;
    if (ret.length() > maxlen)
        ret = ret.left(maxlen-6) + "..." + ret.right(3);

    return ret;
}


bool EmuInstance::loadGBAROM(QStringList filepath)
{
    if (nds->ConsoleType == 1)
    {
        QMessageBox::critical(mainWindow, "melonDS", "The DSi doesn't have a GBA slot.");
        return false;
    }

    unique_ptr<u8[]> filedata = nullptr;
    u32 filelen;
    std::string basepath;
    std::string romname;

    if (!loadROMData(filepath, filedata, filelen, basepath, romname))
    {
        QMessageBox::critical(mainWindow, "melonDS", "Failed to load the GBA ROM.");
        return false;
    }

    gbaSave = nullptr;

    baseGBAROMDir = basepath;
    baseGBAROMName = romname;
    baseGBAAssetName = romname.substr(0, romname.rfind('.'));

    u32 savelen = 0;
    std::unique_ptr<u8[]> savedata = nullptr;

    std::string savname = getAssetPath(true, Config::SaveFilePath, ".sav");
    std::string origsav = savname;
    savname += Platform::InstanceFileSuffix();

    FileHandle* sav = Platform::OpenFile(savname, FileMode::Read);
    if (!sav)
    {
        if (!Platform::CheckFileWritable(origsav))
        {
            QMessageBox::critical(mainWindow, "melonDS", getSavErrorString(origsav, true));
            return false;
        }

        sav = Platform::OpenFile(origsav, FileMode::Read);
    }
    else if (!Platform::CheckFileWritable(savname))
    {
        QMessageBox::critical(mainWindow, "melonDS", getSavErrorString(savname, true));
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

    nds->SetGBACart(std::move(cart));
    gbaCartType = 0;
    gbaSave = std::make_unique<SaveManager>(savname);
    return true;
}

void EmuInstance::loadGBAAddon(int type)
{
    if (Config::ConsoleType == 1) return;

    gbaSave = nullptr;

    nds->LoadGBAAddon(type);

    gbaCartType = type;
    baseGBAROMDir = "";
    baseGBAROMName = "";
    baseGBAAssetName = "";
}

void EmuInstance::ejectGBACart()
{
    gbaSave = nullptr;

    nds->EjectGBACart();

    gbaCartType = -1;
    baseGBAROMDir = "";
    baseGBAROMName = "";
    baseGBAAssetName = "";
}

bool EmuInstance::gbaCartInserted()
{
    return gbaCartType != -1;
}

QString EmuInstance::gbaCartLabel()
{
    if (Config::ConsoleType == 1) return "none (DSi)";

    switch (gbaCartType)
    {
        case 0:
        {
            QString ret = QString::fromStdString(baseGBAROMName);

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


void EmuInstance::romIcon(const u8 (&data)[512], const u16 (&palette)[16], u32 (&iconRef)[32*32])
{
    int index = 0;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            for (int k = 0; k < 8; k++)
            {
                for (int l = 0; l < 8; l++)
                {
                    u8 pal_index = index % 2 ?  data[index/2] >> 4 : data[index/2] & 0x0F;
                    u8 r = ((palette[pal_index] >> 0)  & 0x1F) * 255 / 31;
                    u8 g = ((palette[pal_index] >> 5)  & 0x1F) * 255 / 31;
                    u8 b = ((palette[pal_index] >> 10) & 0x1F) * 255 / 31;
                    u8 a = pal_index ? 255: 0;
                    u32* row = &iconRef[256 * i + 32 * k + 8 * j];
                    row[l] = r | (g << 8) | (b << 16) | (a << 24);
                    index++;
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

void EmuInstance::animatedROMIcon(const u8 (&data)[8][512], const u16 (&palette)[8][16], const u16 (&sequence)[64], u32 (&animatedIconRef)[64][32*32], std::vector<int> &animatedSequenceRef)
{
    for (int i = 0; i < 64; i++)
    {
        if (!sequence[i])
            break;

        romIcon(data[SEQ_BMP(sequence[i])], palette[SEQ_PAL(sequence[i])], animatedIconRef[i]);
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
