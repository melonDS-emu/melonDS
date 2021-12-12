/*
    Copyright 2016-2021 Arisotura

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
#include "FrontendUtil.h"
#include "SharedConfig.h"
#include "Platform.h"

#include "NDS.h"
#include "DSi.h"
#include "GBACart.h"

#include "AREngine.h"


namespace Frontend
{

std::string ROMPath     [ROMSlot_MAX];
std::string SRAMPath    [ROMSlot_MAX];
std::string PrevSRAMPath[ROMSlot_MAX]; // for savestate 'undo load'

std::string NDSROMExtension;

bool SavestateLoaded;

ARCodeFile* CheatFile;
bool CheatsOn;


void Init_ROM()
{
    SavestateLoaded = false;

    ROMPath[ROMSlot_NDS] = "";
    ROMPath[ROMSlot_GBA] = "";
    SRAMPath[ROMSlot_NDS] = "";
    SRAMPath[ROMSlot_GBA] = "";
    PrevSRAMPath[ROMSlot_NDS] = "";
    PrevSRAMPath[ROMSlot_GBA] = "";

    CheatFile = nullptr;
    CheatsOn = false;
}

void DeInit_ROM()
{
    if (CheatFile)
    {
        delete CheatFile;
        CheatFile = nullptr;
    }
}

// TODO: currently, when failing to load a ROM for whatever reason, we attempt
// to revert to the previous state and resume execution; this may not be a very
// good thing, depending on what state the core was left in.
// should we do a better state revert (via the savestate system)? completely stop?

void SetupSRAMPath(int slot)
{
    SRAMPath[slot] = ROMPath[slot].substr(0, ROMPath[slot].length() - 3) + "sav";
}

int VerifyDSBIOS()
{
    FILE* f;
    long len;

    if (!Config::ExternalBIOSEnable) return Load_OK;

    f = Platform::OpenLocalFile(Config::BIOS9Path, "rb");
    if (!f) return Load_BIOS9Missing;

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len != 0x1000)
    {
        fclose(f);
        return Load_BIOS9Bad;
    }

    fclose(f);

    f = Platform::OpenLocalFile(Config::BIOS7Path, "rb");
    if (!f) return Load_BIOS7Missing;

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len != 0x4000)
    {
        fclose(f);
        return Load_BIOS7Bad;
    }

    fclose(f);

    return Load_OK;
}

int VerifyDSiBIOS()
{
    FILE* f;
    long len;

    // TODO: check the first 32 bytes

    f = Platform::OpenLocalFile(Config::DSiBIOS9Path, "rb");
    if (!f) return Load_DSiBIOS9Missing;

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len != 0x10000)
    {
        fclose(f);
        return Load_DSiBIOS9Bad;
    }

    fclose(f);

    f = Platform::OpenLocalFile(Config::DSiBIOS7Path, "rb");
    if (!f) return Load_DSiBIOS7Missing;

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len != 0x10000)
    {
        fclose(f);
        return Load_DSiBIOS7Bad;
    }

    fclose(f);

    return Load_OK;
}

int VerifyDSFirmware()
{
    FILE* f;
    long len;

    if (!Config::ExternalBIOSEnable) return Load_FirmwareNotBootable;

    f = Platform::OpenLocalFile(Config::FirmwarePath, "rb");
    if (!f) return Load_FirmwareNotBootable;

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len == 0x20000)
    {
        // 128KB firmware, not bootable
        fclose(f);
        return Load_FirmwareNotBootable;
    }
    else if (len != 0x40000 && len != 0x80000)
    {
        fclose(f);
        return Load_FirmwareBad;
    }

    fclose(f);

    return Load_OK;
}

int VerifyDSiFirmware()
{
    FILE* f;
    long len;

    f = Platform::OpenLocalFile(Config::DSiFirmwarePath, "rb");
    if (!f) return Load_FirmwareMissing;

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len != 0x20000)
    {
        // not 128KB
        // TODO: check whether those work
        fclose(f);
        return Load_FirmwareBad;
    }

    fclose(f);

    return Load_OK;
}

int SetupDSiNAND()
{
    FILE* f;
    long len;

    f = Platform::OpenLocalFile(Config::DSiNANDPath, "r+b");
    if (!f) return Load_DSiNANDMissing;

    // TODO: some basic checks
    // check that it has the nocash footer, and all

    DSi::SDMMCFile = f;

    return Load_OK;
}

void LoadCheats()
{
    if (CheatFile)
    {
        delete CheatFile;
        CheatFile = nullptr;
    }

    std::string filename;
    if (!ROMPath[ROMSlot_NDS].empty())
    {
        filename = ROMPath[ROMSlot_NDS].substr(0, ROMPath[ROMSlot_NDS].length() - 3) + "mch";
    }
    else
    {
        filename = "firmware.mch";
    }

    // TODO: add custom path here

    // TODO: check for error (malformed cheat file, ...)
    CheatFile = new ARCodeFile(filename);

    AREngine::SetCodeFile(CheatsOn ? CheatFile : nullptr);
}

int LoadBIOS()
{
    DSi::CloseDSiNAND();

    int res;

    res = VerifyDSBIOS();
    if (res != Load_OK) return res;

    if (Config::ConsoleType == 1)
    {
        res = VerifyDSiBIOS();
        if (res != Load_OK) return res;

        res = VerifyDSiFirmware();
        if (res != Load_OK) return res;

        res = SetupDSiNAND();
        if (res != Load_OK) return res;
    }
    else
    {
        res = VerifyDSFirmware();
        if (res != Load_OK) return res;
    }

    // TODO:
    // original code in the libui frontend called NDS::LoadGBAROM() if needed
    // should this be carried over here?
    // is that behavior consistent with that of LoadROM() below?

    ROMPath[ROMSlot_NDS] = "";
    SRAMPath[ROMSlot_NDS] = "";

    NDS::SetConsoleType(Config::ConsoleType);
    NDS::LoadBIOS();

    SavestateLoaded = false;

    LoadCheats();

    return Load_OK;
}

int LoadROM(const u8 *romdata, u32 romlength, const char *archivefilename, const char *romfilename, const char *sramfilename, int slot)
{
    int res;
    bool directboot = Config::DirectBoot != 0;

    if (Config::ConsoleType == 1 && slot == 1)
    {
        // cannot load a GBA ROM into a DSi
        return Load_ROMLoadError;
    }

    res = VerifyDSBIOS();
    if (res != Load_OK) return res;

    if (Config::ConsoleType == 1)
    {
        res = VerifyDSiBIOS();
        if (res != Load_OK) return res;

        res = VerifyDSiFirmware();
        if (res != Load_OK) return res;

        res = SetupDSiNAND();
        if (res != Load_OK) return res;

        GBACart::Eject();
        ROMPath[ROMSlot_GBA] = "";
    }
    else
    {
        res = VerifyDSFirmware();
        if (res != Load_OK)
        {
            if (res == Load_FirmwareNotBootable)
                directboot = true;
            else
                return res;
        }
    }

    std::string oldpath = ROMPath[slot];
    std::string oldsram = SRAMPath[slot];

    SRAMPath[slot] = sramfilename;
    ROMPath[slot] = archivefilename;

    NDS::SetConsoleType(Config::ConsoleType);

    if (slot == ROMSlot_NDS && NDS::LoadROM(romdata, romlength, SRAMPath[slot].c_str(), directboot))
    {
        SavestateLoaded = false;

        LoadCheats();

        // Reload the inserted GBA cartridge (if any)
        // TODO: report failure there??
        //if (!ROMPath[ROMSlot_GBA].empty()) NDS::LoadGBAROM(ROMPath[ROMSlot_GBA], SRAMPath[ROMSlot_GBA]);

        PrevSRAMPath[slot] = SRAMPath[slot]; // safety
        return Load_OK;
    }
    else if (slot == ROMSlot_GBA && NDS::LoadGBAROM(romdata, romlength, romfilename, SRAMPath[slot].c_str()))
    {
        SavestateLoaded = false; // checkme??

        PrevSRAMPath[slot] = SRAMPath[slot]; // safety
        return Load_OK;
    }
    else
    {
        ROMPath[slot] = oldpath;
        SRAMPath[slot] = oldsram;
        return Load_ROMLoadError;
    }
}

int LoadROM(const char* file, int slot)
{
    DSi::CloseDSiNAND();

    int res;
    bool directboot = Config::DirectBoot != 0;

    if (Config::ConsoleType == 1 && slot == 1)
    {
        // cannot load a GBA ROM into a DSi
        return Load_ROMLoadError;
    }

    res = VerifyDSBIOS();
    if (res != Load_OK) return res;

    if (Config::ConsoleType == 1)
    {
        res = VerifyDSiBIOS();
        if (res != Load_OK) return res;

        res = VerifyDSiFirmware();
        if (res != Load_OK) return res;

        res = SetupDSiNAND();
        if (res != Load_OK) return res;

        GBACart::Eject();
        ROMPath[ROMSlot_GBA] = "";
    }
    else
    {
        res = VerifyDSFirmware();
        if (res != Load_OK)
        {
            if (res == Load_FirmwareNotBootable)
                directboot = true;
            else
                return res;
        }
    }

    std::string oldpath = ROMPath[slot];
    std::string oldsram = SRAMPath[slot];

    ROMPath[slot] = file;

    SetupSRAMPath(0);
    SetupSRAMPath(1);

    NDS::SetConsoleType(Config::ConsoleType);

    if (slot == ROMSlot_NDS && NDS::LoadROM(ROMPath[slot].c_str(), SRAMPath[slot].c_str(), directboot))
    {
        SavestateLoaded = false;

        LoadCheats();

        // Reload the inserted GBA cartridge (if any)
        // TODO: report failure there??
        if (!ROMPath[ROMSlot_GBA].empty()) NDS::LoadGBAROM(ROMPath[ROMSlot_GBA].c_str(), SRAMPath[ROMSlot_GBA].c_str());

        PrevSRAMPath[slot] = SRAMPath[slot]; // safety
        return Load_OK;
    }
    else if (slot == ROMSlot_GBA && NDS::LoadGBAROM(ROMPath[slot].c_str(), SRAMPath[slot].c_str()))
    {
        SavestateLoaded = false; // checkme??

        PrevSRAMPath[slot] = SRAMPath[slot]; // safety
        return Load_OK;
    }
    else
    {
        ROMPath[slot] = oldpath;
        SRAMPath[slot] = oldsram;
        return Load_ROMLoadError;
    }
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

void UnloadROM(int slot)
{
    if (slot == ROMSlot_NDS)
    {
        // TODO!
    }
    else if (slot == ROMSlot_GBA)
    {
        GBACart::Eject();
    }

    ROMPath[slot] = "";

    DSi::CloseDSiNAND();
}

int Reset()
{
    DSi::CloseDSiNAND();

    int res;
    bool directboot = Config::DirectBoot != 0;

    res = VerifyDSBIOS();
    if (res != Load_OK) return res;

    if (Config::ConsoleType == 1)
    {
        res = VerifyDSiBIOS();
        if (res != Load_OK) return res;

        res = VerifyDSiFirmware();
        if (res != Load_OK) return res;

        res = SetupDSiNAND();
        if (res != Load_OK) return res;

        GBACart::Eject();
        ROMPath[ROMSlot_GBA][0] = '\0';
    }
    else
    {
        res = VerifyDSFirmware();
        if (res != Load_OK)
        {
            if (res == Load_FirmwareNotBootable)
                directboot = true;
            else
                return res;
        }
    }

    SavestateLoaded = false;

    NDS::SetConsoleType(Config::ConsoleType);

    if (ROMPath[ROMSlot_NDS].empty())
    {
        NDS::LoadBIOS();
    }
    else
    {
        std::string ext = ROMPath[ROMSlot_NDS].substr(ROMPath[ROMSlot_NDS].length() - 4);

        if (ext == ".nds" || ext == ".srl" || ext == ".dsi")
        {
            SetupSRAMPath(0);
            if (!NDS::LoadROM(ROMPath[ROMSlot_NDS].c_str(), SRAMPath[ROMSlot_NDS].c_str(), directboot))
                return Load_ROMLoadError;
        }
#ifdef ARCHIVE_SUPPORT_ENABLED
        else
        {
            // TODO!!!
            // THIS WILL BREAK IF CUSTOM SRAM PATHS ARE ADDED

            /*u8 *romdata = nullptr; u32 romlen;
            char romfilename[1024] = {0}, sramfilename[1024];
            strncpy(sramfilename, SRAMPath[ROMSlot_NDS], 1024); // Use existing SRAMPath

            int pos = strlen(sramfilename) - 1;
            while (pos > 0 && sramfilename[pos] != '/' && sramfilename[pos] != '\\')
                --pos;

            strncpy(romfilename, &sramfilename[pos + 1], 1024);
            strncpy(&romfilename[strlen(romfilename) - 3], NDSROMExtension, 3); // extension could be nds, srl or dsi
            printf("RESET loading from archive : %s\n", romfilename);
            romlen = Archive::ExtractFileFromArchive(ROMPath[ROMSlot_NDS], romfilename, &romdata);
            if (!romdata)
                return Load_ROMLoadError;

            bool ok = NDS::LoadROM(romdata, romlen, sramfilename, directboot);
            delete romdata;
            if (!ok)*/
                return Load_ROMLoadError;
        }
#endif
    }

    if (!ROMPath[ROMSlot_GBA].empty())
    {
        std::string ext = ROMPath[ROMSlot_GBA].substr(ROMPath[ROMSlot_GBA].length() - 4);

        if (ext == ".gba")
        {
            SetupSRAMPath(1);
            if (!NDS::LoadGBAROM(ROMPath[ROMSlot_GBA].c_str(), SRAMPath[ROMSlot_GBA].c_str()))
                return Load_ROMLoadError;
        }
#ifdef ARCHIVE_SUPPORT_ENABLED
        else
        {
            // TODO!! SAME AS ABOVE

            /*u8 *romdata = nullptr; u32 romlen;
            char romfilename[1024] = {0}, sramfilename[1024];
            strncpy(sramfilename, SRAMPath[ROMSlot_GBA], 1024); // Use existing SRAMPath

            int pos = strlen(sramfilename) - 1;
            while (pos > 0 && sramfilename[pos] != '/' && sramfilename[pos] != '\\')
                --pos;

            strncpy(romfilename, &sramfilename[pos + 1], 1024);
            strncpy(&romfilename[strlen(romfilename) - 3], "gba", 3);
            printf("RESET loading from archive : %s\n", romfilename);
            romlen = Archive::ExtractFileFromArchive(ROMPath[ROMSlot_GBA], romfilename, &romdata);
            if (!romdata)
                return Load_ROMLoadError;

            bool ok = NDS::LoadGBAROM(romdata, romlen, romfilename, SRAMPath[ROMSlot_GBA]);
            delete romdata;
            if (!ok)*/
                return Load_ROMLoadError;
        }
#endif
    }

    LoadCheats();

    return Load_OK;
}


// SAVESTATE TODO
// * configurable paths. not everyone wants their ROM directory to be polluted, I guess.

std::string GetSavestateName(int slot)
{
    std::string filename;

    if (ROMPath[ROMSlot_NDS].empty()) // running firmware, no ROM
    {
        filename = "firmware";
    }
    else
    {
        std::string rompath;
        std::string ext = ROMPath[ROMSlot_NDS].substr(ROMPath[ROMSlot_NDS].length() - 4);

        // TODO!!! MORE SHIT THAT IS GONNA ASPLODE
        if (ext == ".nds" || ext == ".srl" || ext == ".dsi")
            rompath = ROMPath[ROMSlot_NDS];
        else
            rompath = SRAMPath[ROMSlot_NDS]; // If archive, construct ssname from sram file

        filename = rompath.substr(0, rompath.rfind('.'));
    }

    filename += ".ml";
    filename += ('0'+slot);

    return filename;
}

bool SavestateExists(int slot)
{
    std::string ssfile = GetSavestateName(slot);
    return Platform::FileExists(ssfile);
}

bool LoadState(std::string filename)
{
    u32 oldGBACartCRC = GBACart::CartCRC;

    // backup
    Savestate* backup = new Savestate("timewarp.mln", true);
    NDS::DoSavestate(backup);
    delete backup;

    bool failed = false;

    Savestate* state = new Savestate(filename, false);
    if (state->Error)
    {
        delete state;

        //uiMsgBoxError(MainWindow, "Error", "Could not load savestate file.");

        // current state might be crapoed, so restore from sane backup
        state = new Savestate("timewarp.mln", false);
        failed = true;
    }

    NDS::DoSavestate(state);
    delete state;

    if (!failed)
    {
        if (Config::SavestateRelocSRAM && !ROMPath[ROMSlot_NDS].empty())
        {
            PrevSRAMPath[ROMSlot_NDS] = SRAMPath[ROMSlot_NDS];

            // TODO: how should this interact with custom paths?
            SRAMPath[ROMSlot_NDS] = filename + ".sav";

            NDS::RelocateSave(SRAMPath[ROMSlot_NDS].c_str(), false);
        }

        bool loadedPartialGBAROM = false;

        // in case we have a GBA cart inserted, and the GBA ROM changes
        // due to having loaded a save state, we do not want to reload
        // the previous cartridge on reset, or commit writes to any
        // loaded save file. therefore, their paths are "nulled".
        if (GBACart::CartInserted && GBACart::CartCRC != oldGBACartCRC)
        {
            ROMPath[ROMSlot_GBA] = "";
            SRAMPath[ROMSlot_GBA] = "";
            loadedPartialGBAROM = true;
        }

        // TODO forward this to user in a meaningful way!!
        /*char msg[64];
        if (slot > 0) sprintf(msg, "State loaded from slot %d%s",
                        slot, loadedPartialGBAROM ? " (GBA ROM header only)" : "");
        else          sprintf(msg, "State loaded from file%s",
                        loadedPartialGBAROM ? " (GBA ROM header only)" : "");
        OSD::AddMessage(0, msg);*/

        SavestateLoaded = true;
    }

    return !failed;
}

bool SaveState(std::string filename)
{
    Savestate* state = new Savestate(filename, true);
    if (state->Error)
    {
        delete state;
        return false;
    }
    else
    {
        NDS::DoSavestate(state);
        delete state;

        if (Config::SavestateRelocSRAM && !ROMPath[ROMSlot_NDS].empty())
        {
            // TODO: how should this interact with custom paths?
            SRAMPath[ROMSlot_NDS] = filename + ".sav";

            NDS::RelocateSave(SRAMPath[ROMSlot_NDS].c_str(), true);
        }
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

    if (!ROMPath[ROMSlot_NDS].empty())
    {
        SRAMPath[ROMSlot_NDS] = PrevSRAMPath[ROMSlot_NDS];
        NDS::RelocateSave(SRAMPath[ROMSlot_NDS].c_str(), false);
    }
}

int ImportSRAM(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    fseek(file, 0, SEEK_END);
    u32 size = ftell(file);
    u8* importData = new u8[size];
    rewind(file);
    fread(importData, size, 1, file);
    fclose(file);

    int diff = NDS::ImportSRAM(importData, size);
    delete[] importData;
    return diff;
}

void EnableCheats(bool enable)
{
    CheatsOn = enable;
    if (CheatFile)
        AREngine::SetCodeFile(CheatsOn ? CheatFile : nullptr);
}

}
