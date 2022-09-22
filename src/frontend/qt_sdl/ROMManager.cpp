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

SaveManager* NDSSave = nullptr;
SaveManager* GBASave = nullptr;

bool SavestateLoaded = false;
std::string PreviousSaveFile = "";

ARCodeFile* CheatFile = nullptr;
bool CheatsOn = false;


int LastSep(std::string path)
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

std::string GetAssetPath(bool gba, std::string configpath, std::string ext, std::string file="")
{
    if (configpath.empty())
        configpath = gba ? BaseGBAROMDir : BaseROMDir;

    if (file.empty())
    {
        file = gba ? BaseGBAAssetName : BaseAssetName;
        if (file.empty())
            file = "firmware";
    }

    for (;;)
    {
        int i = configpath.length() - 1;
        if (i < 0) break;
        if (configpath[i] == '/' || configpath[i] == '\\')
            configpath = configpath.substr(0, i);
        else
            break;
    }

    if (!configpath.empty())
        configpath += "/";

    return configpath + file + ext;
}


QString VerifyDSBIOS()
{
    FILE* f;
    long len;

    f = Platform::OpenLocalFile(Config::BIOS9Path, "rb");
    if (!f) return "DS ARM9 BIOS was not found or could not be accessed. Check your emu settings.";

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len != 0x1000)
    {
        fclose(f);
        return "DS ARM9 BIOS is not a valid BIOS dump.";
    }

    fclose(f);

    f = Platform::OpenLocalFile(Config::BIOS7Path, "rb");
    if (!f) return "DS ARM7 BIOS was not found or could not be accessed. Check your emu settings.";

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len != 0x4000)
    {
        fclose(f);
        return "DS ARM7 BIOS is not a valid BIOS dump.";
    }

    fclose(f);

    return "";
}

QString VerifyDSiBIOS()
{
    FILE* f;
    long len;

    // TODO: check the first 32 bytes

    f = Platform::OpenLocalFile(Config::DSiBIOS9Path, "rb");
    if (!f) return "DSi ARM9 BIOS was not found or could not be accessed. Check your emu settings.";

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len != 0x10000)
    {
        fclose(f);
        return "DSi ARM9 BIOS is not a valid BIOS dump.";
    }

    fclose(f);

    f = Platform::OpenLocalFile(Config::DSiBIOS7Path, "rb");
    if (!f) return "DSi ARM7 BIOS was not found or could not be accessed. Check your emu settings.";

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len != 0x10000)
    {
        fclose(f);
        return "DSi ARM7 BIOS is not a valid BIOS dump.";
    }

    fclose(f);

    return "";
}

QString VerifyDSFirmware()
{
    FILE* f;
    long len;

    f = Platform::OpenLocalFile(Config::FirmwarePath, "rb");
    if (!f) return "DS firmware was not found or could not be accessed. Check your emu settings.";

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len == 0x20000)
    {
        // 128KB firmware, not bootable
        fclose(f);
        // TODO report it somehow? detect in core?
        return "";
    }
    else if (len != 0x40000 && len != 0x80000)
    {
        fclose(f);
        return "DS firmware is not a valid firmware dump.";
    }

    fclose(f);

    return "";
}

QString VerifyDSiFirmware()
{
    FILE* f;
    long len;

    f = Platform::OpenLocalFile(Config::DSiFirmwarePath, "rb");
    if (!f) return "DSi firmware was not found or could not be accessed. Check your emu settings.";

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len != 0x20000)
    {
        // not 128KB
        // TODO: check whether those work
        fclose(f);
        return "DSi firmware is not a valid firmware dump.";
    }

    fclose(f);

    return "";
}

QString VerifyDSiNAND()
{
    FILE* f;
    long len;

    f = Platform::OpenLocalFile(Config::DSiNANDPath, "r+b");
    if (!f) return "DSi NAND was not found or could not be accessed. Check your emu settings.";

    // TODO: some basic checks
    // check that it has the nocash footer, and all

    fclose(f);

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

bool LoadState(std::string filename)
{
    // backup
    Savestate* backup = new Savestate("timewarp.mln", true);
    NDS::DoSavestate(backup);
    delete backup;

    bool failed = false;

    Savestate* state = new Savestate(filename, false);
    if (state->Error)
    {
        delete state;

        // current state might be crapoed, so restore from sane backup
        state = new Savestate("timewarp.mln", false);
        failed = true;
    }

    bool res = NDS::DoSavestate(state);
    delete state;

    if (!res)
    {
        failed = true;
        state = new Savestate("timewarp.mln", false);
        NDS::DoSavestate(state);
        delete state;
    }

    if (failed) return false;

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

bool SaveState(std::string filename)
{
    Savestate* state = new Savestate(filename, true);
    if (state->Error)
    {
        delete state;
        return false;
    }

    NDS::DoSavestate(state);
    delete state;

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
    if (!SavestateLoaded) return;

    // pray that this works
    // what do we do if it doesn't???
    // but it should work.
    Savestate* backup = new Savestate("timewarp.mln", false);
    NDS::DoSavestate(backup);
    delete backup;

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
        FILE* f = Platform::OpenFile(filename, "rb", true);
        if (!f) return false;

        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        if (len > 0x40000000)
        {
            fclose(f);
            return false;
        }

        fseek(f, 0, SEEK_SET);
        filedata = new u8[len];
        size_t nread = fread(filedata, (size_t)len, 1, f);
        if (nread != 1)
        {
            fclose(f);
            delete[] filedata;
            return false;
        }

        fclose(f);
        filelen = (u32)len;

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

    if (NDSSave) delete NDSSave;
    NDSSave = nullptr;

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

    FILE* sav = Platform::OpenFile(savname, "rb", true);
    if (!sav) sav = Platform::OpenFile(origsav, "rb", true);
    if (sav)
    {
        fseek(sav, 0, SEEK_END);
        savelen = (u32)ftell(sav);

        fseek(sav, 0, SEEK_SET);
        savedata = new u8[savelen];
        fread(savedata, savelen, 1, sav);
        fclose(sav);
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
        FILE* f = Platform::OpenFile(filename, "rb", true);
        if (!f) return false;

        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        if (len > 0x40000000)
        {
            fclose(f);
            return false;
        }

        fseek(f, 0, SEEK_SET);
        filedata = new u8[len];
        size_t nread = fread(filedata, (size_t)len, 1, f);
        if (nread != 1)
        {
            fclose(f);
            delete[] filedata;
            return false;
        }

        fclose(f);
        filelen = (u32)len;

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

    FILE* sav = Platform::OpenFile(savname, "rb", true);
    if (!sav) sav = Platform::OpenFile(origsav, "rb", true);
    if (sav)
    {
        fseek(sav, 0, SEEK_END);
        savelen = (u32)ftell(sav);

        fseek(sav, 0, SEEK_SET);
        savedata = new u8[savelen];
        fread(savedata, savelen, 1, sav);
        fclose(sav);
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


void ROMIcon(u8 (&data)[512], u16 (&palette)[16], u32* iconRef)
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

void AnimatedROMIcon(u8 (&data)[8][512], u16 (&palette)[8][16], u16 (&sequence)[64], u32 (&animatedTexRef)[32 * 32 * 64], std::vector<int> &animatedSequenceRef)
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
