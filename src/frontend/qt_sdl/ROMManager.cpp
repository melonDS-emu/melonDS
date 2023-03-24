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

#include <stdio.h>
#include <string.h>

#include <string>
#include <utility>
#include <fstream>

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
#include "DSi_I2C.h"

using namespace Platform;

namespace ROMManager
{

QStringList FullROMPath;

int CartType = -1;
std::string BaseROMDir = "";
std::string BaseROMName = "";
std::string BaseAssetName = "";

int GBACartType = -1;
std::string BaseGBAROMDir = "";
std::string BaseGBAROMName = "";
std::string BaseGBAAssetName = "";

SaveManager* NDSSave = nullptr;
SaveManager* GBASave = nullptr;

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

bool LoadState(const std::string& filename)
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

    if (!NDS::DoSavestate(backup.get()) || backup->Error)
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

    if (!NDS::DoSavestate(state.get()) || state->Error)
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

bool SaveState(const std::string& filename)
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
    NDS::DoSavestate(&state);

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

void UndoStateLoad()
{
    if (!SavestateLoaded || !BackupState) return;

    // Rewind the backup state and put it in load mode
    BackupState->Rewind(false);
    // pray that this works
    // what do we do if it doesn't???
    // but it should work.
    NDS::DoSavestate(BackupState.get());

    if (NDSSave && (!PreviousSaveFile.empty()))
    {
        NDSSave->SetPath(PreviousSaveFile, true);
    }
}


void UnloadCheats()
{
    if (CheatFile)
    {
        delete CheatFile;
        CheatFile = nullptr;
        AREngine::SetCodeFile(nullptr);
    }
}

void LoadCheats()
{
    UnloadCheats();

    std::string filename = GetAssetPath(false, Config::CheatFilePath, ".mch");

    // TODO: check for error (malformed cheat file, ...)
    CheatFile = new ARCodeFile(filename);

    AREngine::SetCodeFile(CheatsOn ? CheatFile : nullptr);
}

void EnableCheats(bool enable)
{
    CheatsOn = enable;
    if (CheatFile)
        AREngine::SetCodeFile(CheatsOn ? CheatFile : nullptr);
}

ARCodeFile* GetCheatFile()
{
    return CheatFile;
}


void SetBatteryLevels()
{
    if (NDS::ConsoleType == 1)
    {
        DSi_BPTWL::SetBatteryLevel(Config::DSiBatteryLevel);
        DSi_BPTWL::SetBatteryCharging(Config::DSiBatteryCharging);
    }
    else
    {
        SPI_Powerman::SetBatteryLevelOkay(Config::DSBatteryLevelOkay);
    }
}

void Reset()
{
    NDS::SetConsoleType(Config::ConsoleType);
    if (Config::ConsoleType == 1) EjectGBACart();
    NDS::Reset();
    SetBatteryLevels();

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

    if (!BaseROMName.empty())
    {
        if (Config::DirectBoot || NDS::NeedsDirectBoot())
        {
            NDS::SetupDirectBoot(BaseROMName);
        }
    }
}


bool LoadBIOS()
{
    NDS::SetConsoleType(Config::ConsoleType);

    if (NDS::NeedsDirectBoot())
        return false;

    /*if (NDSSave) delete NDSSave;
    NDSSave = nullptr;

    CartType = -1;
    BaseROMDir = "";
    BaseROMName = "";
    BaseAssetName = "";*/

    NDS::Reset();
    SetBatteryLevels();
    return true;
}

u32 DecompressROM(const u8* inContent, const u32 inSize, u8** outContent)
{
    u64 realSize = ZSTD_getFrameContentSize(inContent, inSize);
    const u32 maxSize = 0x40000000;

    if (realSize == ZSTD_CONTENTSIZE_ERROR || (realSize > maxSize && realSize != ZSTD_CONTENTSIZE_UNKNOWN))
    {
        return 0;
    }

    if (realSize != ZSTD_CONTENTSIZE_UNKNOWN)
    {
        u8* realContent = new u8[realSize];
        u64 decompressed = ZSTD_decompress(realContent, realSize, inContent, inSize);

        if (ZSTD_isError(decompressed))
        {
            delete[] realContent;
            return 0;
        }

        *outContent = realContent;
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

        ZSTD_freeDStream(dStream);
        *outContent = new u8[outBuf.pos];
        memcpy(*outContent, outBuf.dst, outBuf.pos);

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

bool LoadROM(QStringList filepath, bool reset)
{
    if (filepath.empty()) return false;

    u8* filedata;
    u32 filelen;

    std::string basepath;
    std::string romname;

    int num = filepath.count();
    if (num == 1)
    {
        // regular file

        std::string filename = filepath.at(0).toStdString();
        Platform::FileHandle* f = Platform::OpenFile(filename, FileMode::Read);
        if (!f) return false;

        long len = Platform::FileLength(f);
        if (len > 0x40000000)
        {
            Platform::CloseFile(f);
            delete[] filedata;
            return false;
        }

        Platform::FileRewind(f);
        filedata = new u8[len];
        size_t nread = Platform::FileRead(filedata, (size_t)len, 1, f);
        if (nread != 1)
        {
            Platform::CloseFile(f);
            delete[] filedata;
            return false;
        }

        Platform::CloseFile(f);
        filelen = (u32)len;

        if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".zst")
        {
            u8* outContent = nullptr;
            u32 decompressed = DecompressROM(filedata, len, &outContent);

            if (decompressed > 0)
            {
                delete[] filedata;
                filedata = outContent;
                filelen = decompressed;
                filename = filename.substr(0, filename.length() - 4);
            }
            else
            {
                delete[] filedata;
                return false;
            }
        }

        int pos = LastSep(filename);
        if(pos != -1)
            basepath = filename.substr(0, pos);
        romname = filename.substr(pos+1);
    }
#ifdef ARCHIVE_SUPPORT_ENABLED
    else if (num == 2)
    {
        // file inside archive

        s32 lenread = Archive::ExtractFileFromArchive(filepath.at(0), filepath.at(1), &filedata, &filelen);
        if (lenread < 0) return false;
        if (!filedata) return false;
        if (lenread != filelen)
        {
            delete[] filedata;
            return false;
        }

        std::string std_archivepath = filepath.at(0).toStdString();
        basepath = std_archivepath.substr(0, LastSep(std_archivepath));

        std::string std_romname = filepath.at(1).toStdString();
        romname = std_romname.substr(LastSep(std_romname)+1);
    }
#endif
    else
        return false;

    if (NDSSave) delete NDSSave;
    NDSSave = nullptr;

    FullROMPath = filepath;
    BaseROMDir = basepath;
    BaseROMName = romname;
    BaseAssetName = romname.substr(0, romname.rfind('.'));

    if (reset)
    {
        NDS::SetConsoleType(Config::ConsoleType);
        NDS::EjectCart();
        NDS::Reset();
        SetBatteryLevels();
    }

    u32 savelen = 0;
    u8* savedata = nullptr;

    std::string savname = GetAssetPath(false, Config::SaveFilePath, ".sav");
    std::string origsav = savname;
    savname += Platform::InstanceFileSuffix();

    FileHandle* sav = Platform::OpenFile(savname, FileMode::Read);
    if (!sav) sav = Platform::OpenFile(origsav, FileMode::Read);
    if (sav)
    {
        savelen = (u32)Platform::FileLength(sav);

        FileRewind(sav);
        savedata = new u8[savelen];
        FileRead(savedata, savelen, 1, sav);
        CloseFile(sav);
    }

    bool res = NDS::LoadCart(filedata, filelen, savedata, savelen);
    if (res && reset)
    {
        if (Config::DirectBoot || NDS::NeedsDirectBoot())
        {
            NDS::SetupDirectBoot(romname);
        }
    }

    if (res)
    {
        CartType = 0;
        NDSSave = new SaveManager(savname);

        LoadCheats();
    }

    if (savedata) delete[] savedata;
    delete[] filedata;
    return res;
}

void EjectCart()
{
    if (NDSSave) delete NDSSave;
    NDSSave = nullptr;

    UnloadCheats();

    NDS::EjectCart();

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


bool LoadGBAROM(QStringList filepath)
{
    if (Config::ConsoleType == 1) return false;
    if (filepath.empty()) return false;

    u8* filedata;
    u32 filelen;

    std::string basepath;
    std::string romname;

    int num = filepath.count();
    if (num == 1)
    {
        // regular file

        std::string filename = filepath.at(0).toStdString();
        FileHandle* f = Platform::OpenFile(filename, FileMode::Read);
        if (!f) return false;

        long len = FileLength(f);
        if (len > 0x40000000)
        {
            CloseFile(f);
            return false;
        }

        FileRewind(f);
        filedata = new u8[len];
        size_t nread = FileRead(filedata, (size_t)len, 1, f);
        if (nread != 1)
        {
            CloseFile(f);
            delete[] filedata;
            return false;
        }

        CloseFile(f);
        filelen = (u32)len;

        if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".zst")
        {
            u8* outContent = nullptr;
            u32 decompressed = DecompressROM(filedata, len, &outContent);

            if (decompressed > 0)
            {
                delete[] filedata;
                filedata = outContent;
                filelen = decompressed;
                filename = filename.substr(0, filename.length() - 4);
            }
            else
            {
                delete[] filedata;
                return false;
            }
        }

        int pos = LastSep(filename);
        basepath = filename.substr(0, pos);
        romname = filename.substr(pos+1);
    }
#ifdef ARCHIVE_SUPPORT_ENABLED
    else if (num == 2)
    {
        // file inside archive

        u32 lenread = Archive::ExtractFileFromArchive(filepath.at(0), filepath.at(1), &filedata, &filelen);
        if (lenread < 0) return false;
        if (!filedata) return false;
        if (lenread != filelen)
        {
            delete[] filedata;
            return false;
        }

        std::string std_archivepath = filepath.at(0).toStdString();
        basepath = std_archivepath.substr(0, LastSep(std_archivepath));

        std::string std_romname = filepath.at(1).toStdString();
        romname = std_romname.substr(LastSep(std_romname)+1);
    }
#endif
    else
        return false;

    if (GBASave) delete GBASave;
    GBASave = nullptr;

    BaseGBAROMDir = basepath;
    BaseGBAROMName = romname;
    BaseGBAAssetName = romname.substr(0, romname.rfind('.'));

    u32 savelen = 0;
    u8* savedata = nullptr;

    std::string savname = GetAssetPath(true, Config::SaveFilePath, ".sav");
    std::string origsav = savname;
    savname += Platform::InstanceFileSuffix();

    FileHandle* sav = Platform::OpenFile(savname, FileMode::Read);
    if (!sav) sav = Platform::OpenFile(origsav, FileMode::Read);
    if (sav)
    {
        savelen = (u32)FileLength(sav);

        FileRewind(sav);
        savedata = new u8[savelen];
        FileRead(savedata, savelen, 1, sav);
        CloseFile(sav);
    }

    bool res = NDS::LoadGBACart(filedata, filelen, savedata, savelen);

    if (res)
    {
        GBACartType = 0;
        GBASave = new SaveManager(savname);
    }

    if (savedata) delete[] savedata;
    delete[] filedata;
    return res;
}

void LoadGBAAddon(int type)
{
    if (Config::ConsoleType == 1) return;

    if (GBASave) delete GBASave;
    GBASave = nullptr;

    NDS::LoadGBAAddon(type);

    GBACartType = type;
    BaseGBAROMDir = "";
    BaseGBAROMName = "";
    BaseGBAAssetName = "";
}

void EjectGBACart()
{
    if (GBASave) delete GBASave;
    GBASave = nullptr;

    NDS::EjectGBACart();

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

    case NDS::GBAAddon_RAMExpansion:
        return "Memory expansion";
    }

    return "(none)";
}


void ROMIcon(const u8 (&data)[512], const u16 (&palette)[16], u32* iconRef)
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
                    row[l] = (a << 24) | (r << 16) | (g << 8) | b;
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

void AnimatedROMIcon(const u8 (&data)[8][512], const u16 (&palette)[8][16], const u16 (&sequence)[64], u32 (&animatedTexRef)[32 * 32 * 64], std::vector<int> &animatedSequenceRef)
{
    for (int i = 0; i < 64; i++)
    {
        if (!sequence[i])
            break;
        u32* frame = &animatedTexRef[32 * 32 * i];
        ROMIcon(data[SEQ_BMP(sequence[i])], palette[SEQ_PAL(sequence[i])], frame);

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
