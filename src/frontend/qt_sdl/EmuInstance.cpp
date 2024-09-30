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
#include "Net.h"
#include "MPInterface.h"

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
extern Net net;


EmuInstance::EmuInstance(int inst) : deleting(false),
    instanceID(inst),
    globalCfg(Config::GetGlobalTable()),
    localCfg(Config::GetLocalTable(inst))
{
    consoleType = globalCfg.GetInt("Emu.ConsoleType");

    ndsSave = nullptr;
    cartType = -1;
    baseROMDir = "";
    baseROMName = "";
    baseAssetName = "";

    gbaSave = nullptr;
    gbaCartType = -1;
    baseGBAROMDir = "";
    baseGBAROMName = "";
    baseGBAAssetName = "";

    cheatFile = nullptr;
    cheatsOn = localCfg.GetBool("EnableCheats");

    doLimitFPS = globalCfg.GetBool("LimitFPS");

    double val = globalCfg.GetDouble("TargetFPS");
    if (val == 0.0)
    {
        Platform::Log(Platform::LogLevel::Error, "Target FPS in config invalid\n");
        targetFPS = 1.0 / 60.0;
    }
    else targetFPS = 1.0 / val;

    val = globalCfg.GetDouble("FastForwardFPS");
    if (val == 0.0)
    {
        Platform::Log(Platform::LogLevel::Error, "Fast-Forward FPS in config invalid\n");
        fastForwardFPS = 1.0 / 60.0;
    }
    else fastForwardFPS = 1.0 / val;

    val = globalCfg.GetDouble("SlowmoFPS");
    if (val == 0.0)
    {
        Platform::Log(Platform::LogLevel::Error, "Slow-Mo FPS in config invalid\n");
        slowmoFPS = 1.0 / 60.0;
    }
    else slowmoFPS = 1.0 / val;

    doAudioSync = globalCfg.GetBool("AudioSync");

    mpAudioMode = globalCfg.GetInt("MP.AudioMode");

    nds = nullptr;
    //updateConsole(nullptr, nullptr);

    audioInit();
    inputInit();

    net.RegisterInstance(instanceID);

    emuThread = new EmuThread(this);

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
    deleting = true;
    deleteAllWindows();

    MPInterface::Get().End(instanceID);

    emuThread->emuExit();
    emuThread->wait();
    delete emuThread;

    net.UnregisterInstance(instanceID);

    audioDeInit();
    inputDeInit();
}


std::string EmuInstance::instanceFileSuffix()
{
    if (instanceID == 0) return "";

    char suffix[16] = {0};
    snprintf(suffix, 15, ".%d", instanceID+1);
    return suffix;
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

    MainWindow* win = new MainWindow(id, this, topWindow);
    if (!topWindow) topWindow = win;
    if (!mainWindow) mainWindow = win;
    windowList[id] = win;
    numWindows++;

    emuThread->attachWindow(win);
}

void EmuInstance::deleteWindow(int id, bool close)
{
    if (id >= kMaxWindows) return;

    MainWindow* win = windowList[id];
    if (!win) return;

    if (win->hasOpenGL() && win == mainWindow)
    {
        // we intentionally don't unpause here
        emuThread->emuPause();
        emuThread->deinitContext();
    }

    emuThread->detachWindow(win);

    windowList[id] = nullptr;
    numWindows--;

    if (topWindow == win) topWindow = nullptr;
    if (mainWindow == win) mainWindow = nullptr;

    if (close)
        win->close();

    if ((!mainWindow) && (!deleting))
    {
        // if we closed this instance's main window, delete the instance
        deleteEmuInstance(instanceID);
    }
}

void EmuInstance::deleteAllWindows()
{
    for (int i = kMaxWindows-1; i >= 0; i--)
        deleteWindow(i, true);
}


void EmuInstance::osdAddMessage(unsigned int color, const char* fmt, ...)
{
    if (fmt == nullptr)
        return;

    char msg[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, 256, fmt, args);
    va_end(args);

    for (int i = 0; i < kMaxWindows; i++)
    {
        if (windowList[i])
            windowList[i]->osdAddMessage(color, msg);
    }
}


bool EmuInstance::emuIsActive()
{
    return emuThread->emuIsActive();
}

void EmuInstance::emuStop(StopReason reason)
{
    if (reason != StopReason::External)
        emuThread->emuStop(false);

    switch (reason)
    {
        case StopReason::GBAModeNotSupported:
            osdAddMessage(0xFFA0A0, "GBA mode not supported");
            break;
        case StopReason::BadExceptionRegion:
            osdAddMessage(0xFFA0A0, "Internal error");
            break;
        case StopReason::PowerOff:
        case StopReason::External:
            osdAddMessage(0xFFC040, "Shutdown");
        default:
            break;
    }
}


bool EmuInstance::usesOpenGL()
{
    return globalCfg.GetBool("Screen.UseGL") ||
           (globalCfg.GetInt("3D.Renderer") != renderer3D_Software);
}

void EmuInstance::initOpenGL()
{
    for (int i = 0; i < kMaxWindows; i++)
    {
        if (windowList[i])
            windowList[i]->initOpenGL();
    }

    setVSyncGL(true);
}

void EmuInstance::deinitOpenGL()
{
    for (int i = 0; i < kMaxWindows; i++)
    {
        if (windowList[i])
            windowList[i]->deinitOpenGL();
    }
}

void EmuInstance::setVSyncGL(bool vsync)
{
    int intv;

    vsync = vsync && globalCfg.GetBool("Screen.VSync");
    if (vsync)
        intv = globalCfg.GetInt("Screen.VSyncInterval");
    else
        intv = 0;

    for (int i = 0; i < kMaxWindows; i++)
    {
        if (windowList[i])
            windowList[i]->setGLSwapInterval(intv);
    }
}

void EmuInstance::makeCurrentGL()
{
    mainWindow->makeCurrentGL();
}

void EmuInstance::drawScreenGL()
{
    for (int i = 0; i < kMaxWindows; i++)
    {
        if (windowList[i])
            windowList[i]->drawScreenGL();
    }
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
        return GetLocalFilePath(kWifiSettingsPath);
    }
    if (consoleType == 1)
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
    firmwareSave = std::make_unique<SaveManager>(getEffectiveFirmwareSavePath() + instanceFileSuffix());
}

std::string EmuInstance::getSavestateName(int slot)
{
    std::string ext = ".ml";
    ext += (char)('0'+slot);
    return getAssetPath(false, globalCfg.GetString("SavestatePath"), ext);
}

bool EmuInstance::savestateExists(int slot)
{
    std::string ssfile = getSavestateName(slot);
    return Platform::FileExists(ssfile);
}

bool EmuInstance::loadState(const std::string& filename)
{
    Platform::FileHandle* file = Platform::OpenFile(filename, Platform::FileMode::Read);
    if (file == nullptr)
    { // If we couldn't open the state file...
        Platform::Log(Platform::LogLevel::Error, "Failed to open state file \"%s\"\n", filename.c_str());
        return false;
    }

    std::unique_ptr<Savestate> backup = std::make_unique<Savestate>(Savestate::DEFAULT_SIZE);
    if (backup->Error)
    { // If we couldn't allocate memory for the backup...
        Platform::Log(Platform::LogLevel::Error, "Failed to allocate memory for state backup\n");
        Platform::CloseFile(file);
        return false;
    }

    if (!nds->DoSavestate(backup.get()) || backup->Error)
    { // Back up the emulator's state. If that failed...
        Platform::Log(Platform::LogLevel::Error, "Failed to back up state, aborting load (from \"%s\")\n", filename.c_str());
        Platform::CloseFile(file);
        return false;
    }
    // We'll store the backup once we're sure that the state was loaded.
    // Now that we know the file and backup are both good, let's load the new state.

    // Get the size of the file that we opened
    size_t size = Platform::FileLength(file);

    // Allocate exactly as much memory as we need for the savestate
    std::vector<u8> buffer(size);
    if (Platform::FileRead(buffer.data(), size, 1, file) == 0)
    { // Read the state file into the buffer. If that failed...
        Platform::Log(Platform::LogLevel::Error, "Failed to read %u-byte state file \"%s\"\n", size, filename.c_str());
        Platform::CloseFile(file);
        return false;
    }
    Platform::CloseFile(file); // done with the file now

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

    if (globalCfg.GetBool("Savestate.RelocSRAM") && ndsSave)
    {
        previousSaveFile = ndsSave->GetPath();

        std::string savefile = filename.substr(lastSep(filename)+1);
        savefile = getAssetPath(false, globalCfg.GetString("SaveFilePath"), ".sav", savefile);
        savefile += instanceFileSuffix();
        ndsSave->SetPath(savefile, true);
    }

    savestateLoaded = true;

    return true;
}

bool EmuInstance::saveState(const std::string& filename)
{
    Platform::FileHandle* file = Platform::OpenFile(filename, Platform::FileMode::Write);

    if (file == nullptr)
    { // If the file couldn't be opened...
        return false;
    }

    Savestate state;
    if (state.Error)
    { // If there was an error creating the state (and allocating its memory)...
        Platform::CloseFile(file);
        return false;
    }

    // Write the savestate to the in-memory buffer
    nds->DoSavestate(&state);

    if (state.Error)
    {
        Platform::CloseFile(file);
        return false;
    }

    if (Platform::FileWrite(state.Buffer(), state.Length(), 1, file) == 0)
    { // Write the Savestate buffer to the file. If that fails...
        Platform::Log(Platform::Error,
                      "Failed to write %d-byte savestate to %s\n",
                      state.Length(),
                      filename.c_str()
        );
        Platform::CloseFile(file);
        return false;
    }

    Platform::CloseFile(file);

    if (globalCfg.GetBool("Savestate.RelocSRAM") && ndsSave)
    {
        std::string savefile = filename.substr(lastSep(filename)+1);
        savefile = getAssetPath(false, globalCfg.GetString("SaveFilePath"), ".sav", savefile);
        savefile += instanceFileSuffix();
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
    cheatFile = nullptr; // cleaned up by unique_ptr
    nds->AREngine.Cheats.clear();
}

void EmuInstance::loadCheats()
{
    unloadCheats();

    std::string filename = getAssetPath(false, globalCfg.GetString("CheatFilePath"), ".mch");

    // TODO: check for error (malformed cheat file, ...)
    cheatFile = std::make_unique<ARCodeFile>(filename);

    if (cheatsOn)
    {
        nds->AREngine.Cheats = cheatFile->GetCodes();
    }
    else
    {
        nds->AREngine.Cheats.clear();
    }
}

std::unique_ptr<ARM9BIOSImage> EmuInstance::loadARM9BIOS() noexcept
{
    if (!globalCfg.GetBool("Emu.ExternalBIOSEnable"))
    {
        return globalCfg.GetInt("Emu.ConsoleType") == 0 ? std::make_unique<ARM9BIOSImage>(bios_arm9_bin) : nullptr;
    }

    string path = globalCfg.GetString("DS.BIOS9Path");

    if (FileHandle* f = OpenLocalFile(path, Read))
    {
        std::unique_ptr<ARM9BIOSImage> bios = std::make_unique<ARM9BIOSImage>();
        FileRewind(f);
        FileRead(bios->data(), bios->size(), 1, f);
        CloseFile(f);
        Log(Info, "ARM9 BIOS loaded from %s\n", path.c_str());
        return bios;
    }

    Log(Warn, "ARM9 BIOS not found\n");
    return nullptr;
}

std::unique_ptr<ARM7BIOSImage> EmuInstance::loadARM7BIOS() noexcept
{
    if (!globalCfg.GetBool("Emu.ExternalBIOSEnable"))
    {
        return globalCfg.GetInt("Emu.ConsoleType") == 0 ? std::make_unique<ARM7BIOSImage>(bios_arm7_bin) : nullptr;
    }

    string path = globalCfg.GetString("DS.BIOS7Path");

    if (FileHandle* f = OpenLocalFile(path, Read))
    {
        std::unique_ptr<ARM7BIOSImage> bios = std::make_unique<ARM7BIOSImage>();
        FileRead(bios->data(), bios->size(), 1, f);
        CloseFile(f);
        Log(Info, "ARM7 BIOS loaded from %s\n", path.c_str());
        return bios;
    }

    Log(Warn, "ARM7 BIOS not found\n");
    return nullptr;
}

std::unique_ptr<DSiBIOSImage> EmuInstance::loadDSiARM9BIOS() noexcept
{
    string path = globalCfg.GetString("DSi.BIOS9Path");

    if (FileHandle* f = OpenLocalFile(path, Read))
    {
        std::unique_ptr<DSiBIOSImage> bios = std::make_unique<DSiBIOSImage>();
        FileRead(bios->data(), bios->size(), 1, f);
        CloseFile(f);

        if (!globalCfg.GetBool("DSi.FullBIOSBoot"))
        {
            // herp
            *(u32*)bios->data() = 0xEAFFFFFE; // overwrites the reset vector

            // TODO!!!!
            // hax the upper 32K out of the goddamn DSi
            // done that :)  -pcy
        }
        Log(Info, "ARM9i BIOS loaded from %s\n", path.c_str());
        return bios;
    }

    Log(Warn, "ARM9i BIOS not found\n");
    return nullptr;
}

std::unique_ptr<DSiBIOSImage> EmuInstance::loadDSiARM7BIOS() noexcept
{
    string path = globalCfg.GetString("DSi.BIOS7Path");

    if (FileHandle* f = OpenLocalFile(path, Read))
    {
        std::unique_ptr<DSiBIOSImage> bios = std::make_unique<DSiBIOSImage>();
        FileRead(bios->data(), bios->size(), 1, f);
        CloseFile(f);

        if (!globalCfg.GetBool("DSi.FullBIOSBoot"))
        {
            // herp
            *(u32*)bios->data() = 0xEAFFFFFE; // overwrites the reset vector

            // TODO!!!!
            // hax the upper 32K out of the goddamn DSi
            // done that :)  -pcy
        }
        Log(Info, "ARM7i BIOS loaded from %s\n", path.c_str());
        return bios;
    }

    Log(Warn, "ARM7i BIOS not found\n");
    return nullptr;
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

    customizeFirmware(firmware, true);

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

    customizeFirmware(firmware, localCfg.GetBool("Firmware.OverrideSettings"));

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
        if (localCfg.GetBool("Firmware.OverrideSettings"))
        {
            auto firmcfg = localCfg.GetTable("Firmware");

            // we store relevant strings as UTF-8, so we need to convert them to UTF-16
            auto converter = wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{};

            // setting up username
            std::u16string username = converter.from_bytes(firmcfg.GetString("Username"));
            size_t usernameLength = std::min(username.length(), (size_t) 10);
            memset(&settings.Nickname, 0, sizeof(settings.Nickname));
            memcpy(&settings.Nickname, username.data(), usernameLength * sizeof(char16_t));

            // setting language
            settings.Language = static_cast<Firmware::Language>(firmcfg.GetInt("Language"));

            // setting up color
            settings.FavoriteColor = firmcfg.GetInt("FavouriteColour");

            // setting up birthday
            settings.BirthdayMonth = firmcfg.GetInt("BirthdayMonth");
            settings.BirthdayDay = firmcfg.GetInt("BirthdayDay");

            // setup message
            std::u16string message = converter.from_bytes(firmcfg.GetString("Message"));
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
        nds->AREngine.Cheats = cheatFile->GetCodes();
    else
        nds->AREngine.Cheats.clear();
}

ARCodeFile* EmuInstance::getCheatFile()
{
    return cheatFile.get();
}

void EmuInstance::setBatteryLevels()
{
    if (consoleType == 1)
    {
        auto dsi = static_cast<DSi*>(nds);
        dsi->I2C.GetBPTWL()->SetBatteryLevel(localCfg.GetInt("DSi.Battery.Level"));
        dsi->I2C.GetBPTWL()->SetBatteryCharging(localCfg.GetBool("DSi.Battery.Charging"));
    }
    else
    {
        nds->SPI.GetPowerMan()->SetBatteryLevelOkay(localCfg.GetBool("DS.Battery.LevelOkay"));
    }
}

void EmuInstance::setDateTime()
{
    QDateTime hosttime = QDateTime::currentDateTime();
    QDateTime time = hosttime.addSecs(localCfg.GetInt64("RTC.Offset"));

    nds->RTC.SetDateTime(time.date().year(), time.date().month(), time.date().day(),
                         time.time().hour(), time.time().minute(), time.time().second());
}

bool EmuInstance::updateConsole(UpdateConsoleNDSArgs&& _ndsargs, UpdateConsoleGBAArgs&& _gbaargs) noexcept
{
    // Let's get the cart we want to use;
    // if we wnat to keep the cart, we'll eject it from the existing console first.
    std::unique_ptr<NDSCart::CartCommon> nextndscart;
    if (std::holds_alternative<Keep>(_ndsargs))
    { // If we want to keep the existing cart (if any)...
        nextndscart = nds ? nds->EjectCart() : nullptr;
        _ndsargs = {};
    }
    else if (const auto ptr = std::get_if<std::unique_ptr<NDSCart::CartCommon>>(&_ndsargs))
    {
        nextndscart = std::move(*ptr);
        _ndsargs = {};
    }

    if (auto* cartsd = dynamic_cast<NDSCart::CartSD*>(nextndscart.get()))
    {
        // LoadDLDISDCard will return nullopt if the SD card is disabled;
        // SetSDCard will accept nullopt, which means no SD card
        cartsd->SetSDCard(getSDCardArgs("DLDI"));
    }

    std::unique_ptr<GBACart::CartCommon> nextgbacart;
    if (std::holds_alternative<Keep>(_gbaargs))
    {
        nextgbacart = nds ? nds->EjectGBACart() : nullptr;
    }
    else if (const auto ptr = std::get_if<std::unique_ptr<GBACart::CartCommon>>(&_gbaargs))
    {
        nextgbacart = std::move(*ptr);
        _gbaargs = {};
    }


    int consoletype = globalCfg.GetInt("Emu.ConsoleType");

    auto arm9bios = loadARM9BIOS();
    if (!arm9bios)
        return false;

    auto arm7bios = loadARM7BIOS();
    if (!arm7bios)
        return false;

    auto firmware = loadFirmware(consoletype);
    if (!firmware)
        return false;

#ifdef JIT_ENABLED
    Config::Table jitopt = globalCfg.GetTable("JIT");
    JITArgs _jitargs {
            static_cast<unsigned>(jitopt.GetInt("MaxBlockSize")),
            jitopt.GetBool("LiteralOptimisations"),
            jitopt.GetBool("BranchOptimisations"),
            jitopt.GetBool("FastMemory"),
    };
    auto jitargs = jitopt.GetBool("Enable") ? std::make_optional(_jitargs) : std::nullopt;
#else
    std::optional<JITArgs> jitargs = std::nullopt;
#endif

#ifdef GDBSTUB_ENABLED
    Config::Table gdbopt = localCfg.GetTable("Gdb");
    GDBArgs _gdbargs {
            static_cast<u16>(gdbopt.GetInt("ARM7.Port")),
            static_cast<u16>(gdbopt.GetInt("ARM9.Port")),
            gdbopt.GetBool("ARM7.BreakOnStartup"),
            gdbopt.GetBool("ARM9.BreakOnStartup"),
    };
    auto gdbargs = gdbopt.GetBool("Enabled") ? std::make_optional(_gdbargs) : std::nullopt;
#else
    optional<GDBArgs> gdbargs = std::nullopt;
#endif

    NDSArgs ndsargs {
            std::move(nextndscart),
            std::move(nextgbacart),
            std::move(arm9bios),
            std::move(arm7bios),
            std::move(*firmware),
            jitargs,
            static_cast<AudioBitDepth>(globalCfg.GetInt("Audio.BitDepth")),
            static_cast<AudioInterpolation>(globalCfg.GetInt("Audio.Interpolation")),
            gdbargs,
    };
    NDSArgs* args = &ndsargs;

    std::optional<DSiArgs> dsiargs = std::nullopt;
    if (consoletype == 1)
    {
        ndsargs.GBAROM = nullptr;

        auto arm7ibios = loadDSiARM7BIOS();
        if (!arm7ibios)
            return false;

        auto arm9ibios = loadDSiARM9BIOS();
        if (!arm9ibios)
            return false;

        auto nand = loadNAND(*arm7ibios);
        if (!nand)
            return false;

        auto sdcard = loadSDCard("DSi.SD");

        DSiArgs _dsiargs {
                std::move(ndsargs),
                std::move(arm9ibios),
                std::move(arm7ibios),
                std::move(*nand),
                std::move(sdcard),
                globalCfg.GetBool("DSi.FullBIOSBoot"),
        };

        dsiargs = std::move(_dsiargs);
        args = &(*dsiargs);
    }


    if ((!nds) || (consoletype != nds->ConsoleType))
    {
        NDS::Current = nullptr;
        if (nds) delete nds;

        if (consoletype == 1)
            nds = new DSi(std::move(dsiargs.value()), this);
        else
            nds = new NDS(std::move(ndsargs), this);

        NDS::Current = nds;
        nds->Reset();
    }
    else
    {
        nds->SetARM7BIOS(*args->ARM7BIOS);
        nds->SetARM9BIOS(*args->ARM9BIOS);
        nds->SetFirmware(std::move(args->Firmware));
        nds->SetNDSCart(std::move(args->NDSROM));
        nds->SetGBACart(std::move(args->GBAROM));
        nds->SetJITArgs(args->JIT);
        // TODO GDB stub shit
        nds->SPU.SetInterpolation(args->Interpolation);
        nds->SPU.SetDegrade10Bit(args->BitDepth);

        if (consoletype == 1)
        {
            DSi* dsi = (DSi*)nds;
            DSiArgs& _dsiargs = *dsiargs;

            dsi->SetFullBIOSBoot(_dsiargs.FullBIOSBoot);
            dsi->ARM7iBIOS = *_dsiargs.ARM7iBIOS;
            dsi->ARM9iBIOS = *_dsiargs.ARM9iBIOS;
            dsi->SetNAND(std::move(_dsiargs.NANDImage));
            dsi->SetSDCard(std::move(_dsiargs.DSiSDCard));
            // We're moving the optional, not the card
            // (inserting std::nullopt here is okay, it means no card)

            dsi->EjectGBACart();
        }
    }

    return true;
}

void EmuInstance::reset()
{
    consoleType = globalCfg.GetInt("Emu.ConsoleType");
    
    updateConsole(Keep {}, Keep {});

    if (consoleType == 1) ejectGBACart();

    nds->Reset();
    setBatteryLevels();
    setDateTime();

    if ((cartType != -1) && ndsSave)
    {
        std::string oldsave = ndsSave->GetPath();
        std::string newsave = getAssetPath(false, globalCfg.GetString("SaveFilePath"), ".sav");
        newsave += instanceFileSuffix();
        if (oldsave != newsave)
            ndsSave->SetPath(newsave, false);
    }

    if ((gbaCartType != -1) && gbaSave)
    {
        std::string oldsave = gbaSave->GetPath();
        std::string newsave = getAssetPath(true, globalCfg.GetString("SaveFilePath"), ".sav");
        newsave += instanceFileSuffix();
        if (oldsave != newsave)
            gbaSave->SetPath(newsave, false);
    }

    initFirmwareSaveManager();
    if (firmwareSave)
    {
        std::string oldsave = firmwareSave->GetPath();
        string newsave;
        if (globalCfg.GetBool("Emu.ExternalBIOSEnable"))
        {
            if (consoleType == 1)
                newsave = globalCfg.GetString("DSi.FirmwarePath") + instanceFileSuffix();
            else
                newsave = globalCfg.GetString("DS.FirmwarePath") + instanceFileSuffix();
        }
        else
        {
            newsave = GetLocalFilePath(kWifiSettingsPath + instanceFileSuffix());
        }

        if (oldsave != newsave)
        { // If the player toggled the ConsoleType or ExternalBIOSEnable...
            firmwareSave->SetPath(newsave, true);
        }
    }

    if (!baseROMName.empty())
    {
        if (globalCfg.GetBool("Emu.DirectBoot") || nds->NeedsDirectBoot())
        {
            nds->SetupDirectBoot(baseROMName);
        }
    }

    nds->Start();
}


bool EmuInstance::bootToMenu()
{
    // Keep whatever cart is in the console, if any.
    if (!updateConsole(Keep {}, Keep {}))
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
    std::unique_ptr<Firmware> firmware = std::make_unique<Firmware>(consoleType);
    assert(firmware->Buffer() != nullptr);

    // Try to open the instanced Wi-fi settings, falling back to the regular Wi-fi settings if they don't exist.
    // We don't need to save the whole firmware, just the part that may actually change.
    std::string wfcsettingspath = kWifiSettingsPath;
    settingspath = wfcsettingspath + instanceFileSuffix();
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
                    Firmware::WifiAccessPoint(consoleType),
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
    const std::string mac_in = localCfg.GetString("Firmware.MAC");
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

void EmuInstance::customizeFirmware(Firmware& firmware, bool overridesettings) noexcept
{
    if (overridesettings)
    {
        auto &currentData = firmware.GetEffectiveUserData();

        auto firmcfg = localCfg.GetTable("Firmware");

        // setting up username
        std::string orig_username = firmcfg.GetString("Username");
        if (!orig_username.empty())
        { // If the frontend defines a username, take it. If not, leave the existing one.
            std::u16string username = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(
                    orig_username);
            size_t usernameLength = std::min(username.length(), (size_t) 10);
            currentData.NameLength = usernameLength;
            memcpy(currentData.Nickname, username.data(), usernameLength * sizeof(char16_t));
        }

        auto language = static_cast<Firmware::Language>(firmcfg.GetInt("Language"));
        if (language != Firmware::Language::Reserved)
        { // If the frontend specifies a language (rather than using the existing value)...
            currentData.Settings &= ~Firmware::Language::Reserved; // ..clear the existing language...
            currentData.Settings |= language; // ...and set the new one.
        }

        // setting up color
        u8 favoritecolor = firmcfg.GetInt("FavouriteColour");
        if (favoritecolor != 0xFF)
        {
            currentData.FavoriteColor = favoritecolor;
        }

        u8 birthmonth = firmcfg.GetInt("BirthdayMonth");
        if (birthmonth != 0)
        { // If the frontend specifies a birth month (rather than using the existing value)...
            currentData.BirthdayMonth = birthmonth;
        }

        u8 birthday = firmcfg.GetInt("BirthdayDay");
        if (birthday != 0)
        { // If the frontend specifies a birthday (rather than using the existing value)...
            currentData.BirthdayDay = birthday;
        }

        // setup message
        std::string orig_message = firmcfg.GetString("Message");
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
        rep = parseMacAddress(&configuredMac);
        rep &= (configuredMac != MacAddress());

        if (rep)
        {
            mac = configuredMac;
        }
    }

    if (instanceID > 0)
    {
        rep = true;
        mac[3] += instanceID;
        mac[4] += instanceID*0x44;
        mac[5] += instanceID*0x10;
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

    std::string savname = getAssetPath(false, globalCfg.GetString("SaveFilePath"), ".sav");
    std::string origsav = savname;
    savname += instanceFileSuffix();

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

    auto cart = NDSCart::ParseROM(std::move(filedata), filelen, this, std::move(cartargs));
    if (!cart)
    {
        // If we couldn't parse the ROM...
        QMessageBox::critical(mainWindow, "melonDS", "Failed to load the DS ROM.");
        return false;
    }

    if (reset)
    {
        if (!updateConsole(std::move(cart), Keep {}))
        {
            QMessageBox::critical(mainWindow, "melonDS", "Failed to load the DS ROM.");
            return false;
        }

        initFirmwareSaveManager();
        nds->Reset();

        if (globalCfg.GetBool("Emu.DirectBoot") || nds->NeedsDirectBoot())
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
    if (consoleType == 1)
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

    std::string savname = getAssetPath(true, globalCfg.GetString("SaveFilePath"), ".sav");
    std::string origsav = savname;
    savname += instanceFileSuffix();

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

    auto cart = GBACart::ParseROM(std::move(filedata), filelen, std::move(savedata), savelen, this);
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
    if (consoleType == 1) return;

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
    if (consoleType == 1) return "none (DSi)";

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
