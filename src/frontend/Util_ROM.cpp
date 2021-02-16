/*
    Copyright 2016-2020 Arisotura

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

#ifdef ARCHIVE_SUPPORT_ENABLED
#include "ArchiveUtil.h"
#endif
#include "FrontendUtil.h"
#include "Config.h"
#include "SharedConfig.h"
#include "Platform.h"

#include "NDS.h"
#include "DSi.h"
#include "GBACart.h"

#include "AREngine.h"


namespace Frontend
{

char ROMPath     [ROMSlot_MAX][1024];
char SRAMPath    [ROMSlot_MAX][1024];
char PrevSRAMPath[ROMSlot_MAX][1024]; // for savestate 'undo load'

char NDSROMExtension[4];

bool SavestateLoaded;

ARCodeFile* CheatFile;
bool CheatsOn;


void Init_ROM()
{
    SavestateLoaded = false;

    memset(ROMPath[ROMSlot_NDS], 0, 1024);
    memset(ROMPath[ROMSlot_GBA], 0, 1024);
    memset(SRAMPath[ROMSlot_NDS], 0, 1024);
    memset(SRAMPath[ROMSlot_GBA], 0, 1024);
    memset(PrevSRAMPath[ROMSlot_NDS], 0, 1024);
    memset(PrevSRAMPath[ROMSlot_GBA], 0, 1024);

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
    strncpy(SRAMPath[slot], ROMPath[slot], 1023);
    SRAMPath[slot][1023] = '\0';
    strncpy(SRAMPath[slot] + strlen(ROMPath[slot]) - 3, "sav", 3);
}

int VerifyDSBIOS()
{
    FILE* f;
    long len;

    f = Platform::OpenLocalFile(Config::BIOS9Path, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        if (len != 0x1000)
        {
            fclose(f);
            return Load_BIOS9Bad;
        }

        fclose(f);
    }
    else
    {
        printf("Bios ARM9 not found. Proceeding with FreeBIOS.");
    }

    f = Platform::OpenLocalFile(Config::BIOS7Path, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        if (len != 0x4000)
        {
            fclose(f);
            return Load_BIOS7Bad;
        }

        fclose(f);
    }
    else
    {
        printf("Bios ARM7 not found. Proceeding with FreeBIOS.");
    }

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

    f = Platform::OpenLocalFile(Config::FirmwarePath, "rb");
    if (!f) return Load_FirmwareMissing;

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

    if (Config::DSiSDEnable)
    {
        f = Platform::OpenLocalFile(Config::DSiSDPath, "r+b");
        if (f)
            DSi::SDIOFile = f;
        else
            DSi::SDIOFile = Platform::OpenLocalFile(Config::DSiSDPath, "w+b");
    }

    return Load_OK;
}

void LoadCheats()
{
    if (CheatFile)
    {
        delete CheatFile;
        CheatFile = nullptr;
    }

    char filename[1024];
    if (ROMPath[ROMSlot_NDS][0] != '\0')
    {
        strncpy(filename, ROMPath[ROMSlot_NDS], 1023);
        filename[1023] = '\0';
        strncpy(filename + strlen(ROMPath[ROMSlot_NDS]) - 3, "mch", 3);
    }
    else
    {
        strncpy(filename, "firmware.mch", 1023);
    }

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

    ROMPath[ROMSlot_NDS][0] = '\0';
    SRAMPath[ROMSlot_NDS][0] = '\0';

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

    char oldpath[1024];
    char oldsram[1024];
    strncpy(oldpath, ROMPath[slot], 1024);
    strncpy(oldsram, SRAMPath[slot], 1024);

    strncpy(SRAMPath[slot], sramfilename, 1024);
    strncpy(ROMPath[slot], archivefilename, 1024);

    NDS::SetConsoleType(Config::ConsoleType);

    if (slot == ROMSlot_NDS && NDS::LoadROM(romdata, romlength, SRAMPath[slot], directboot))
    {
        SavestateLoaded = false;

        LoadCheats();

        // Reload the inserted GBA cartridge (if any)
        // TODO: report failure there??
        //if (ROMPath[ROMSlot_GBA][0] != '\0') NDS::LoadGBAROM(ROMPath[ROMSlot_GBA], SRAMPath[ROMSlot_GBA]);

        strncpy(PrevSRAMPath[slot], SRAMPath[slot], 1024); // safety
        return Load_OK;
    }
    else if (slot == ROMSlot_GBA && NDS::LoadGBAROM(romdata, romlength, romfilename, SRAMPath[slot]))
    {
        SavestateLoaded = false; // checkme??

        strncpy(PrevSRAMPath[slot], SRAMPath[slot], 1024); // safety
        return Load_OK;
    }
    else
    {
        strncpy(ROMPath[slot], oldpath, 1024);
        strncpy(SRAMPath[slot], oldsram, 1024);
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

    char oldpath[1024];
    char oldsram[1024];
    strncpy(oldpath, ROMPath[slot], 1024);
    strncpy(oldsram, SRAMPath[slot], 1024);

    strncpy(ROMPath[slot], file, 1023);
    ROMPath[slot][1023] = '\0';

    SetupSRAMPath(0);
    SetupSRAMPath(1);

    NDS::SetConsoleType(Config::ConsoleType);

    if (slot == ROMSlot_NDS && NDS::LoadROM(ROMPath[slot], SRAMPath[slot], directboot))
    {
        SavestateLoaded = false;

        LoadCheats();

        // Reload the inserted GBA cartridge (if any)
        // TODO: report failure there??
        if (ROMPath[ROMSlot_GBA][0] != '\0') NDS::LoadGBAROM(ROMPath[ROMSlot_GBA], SRAMPath[ROMSlot_GBA]);

        strncpy(PrevSRAMPath[slot], SRAMPath[slot], 1024); // safety
        return Load_OK;
    }
    else if (slot == ROMSlot_GBA && NDS::LoadGBAROM(ROMPath[slot], SRAMPath[slot]))
    {
        SavestateLoaded = false; // checkme??

        strncpy(PrevSRAMPath[slot], SRAMPath[slot], 1024); // safety
        return Load_OK;
    }
    else
    {
        strncpy(ROMPath[slot], oldpath, 1024);
        strncpy(SRAMPath[slot], oldsram, 1024);
        return Load_ROMLoadError;
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

    ROMPath[slot][0] = '\0';

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

    if (ROMPath[ROMSlot_NDS][0] == '\0')
    {
        NDS::LoadBIOS();
    }
    else
    {
        char ext[5] = {0}; int _len = strlen(ROMPath[ROMSlot_NDS]);
        strncpy(ext, ROMPath[ROMSlot_NDS] + _len - 4, 4);

        if(!strncmp(ext, ".nds", 4) || !strncmp(ext, ".srl", 4) || !strncmp(ext, ".dsi", 4))
        {
            SetupSRAMPath(0);
            if (!NDS::LoadROM(ROMPath[ROMSlot_NDS], SRAMPath[ROMSlot_NDS], directboot))
                return Load_ROMLoadError;
        }
#ifdef ARCHIVE_SUPPORT_ENABLED
        else
        {
            u8 *romdata = nullptr; u32 romlen;
            char romfilename[1024] = {0}, sramfilename[1024];
            strncpy(sramfilename, SRAMPath[ROMSlot_NDS], 1024); // Use existing SRAMPath

            int pos = strlen(sramfilename) - 1;
            while(pos > 0 && sramfilename[pos] != '/' && sramfilename[pos] != '\\')
                --pos;

            strncpy(romfilename, &sramfilename[pos + 1], 1024);
            strncpy(&romfilename[strlen(romfilename) - 3], NDSROMExtension, 3); // extension could be nds, srl or dsi
            printf("RESET loading from archive : %s\n", romfilename);
            romlen = Archive::ExtractFileFromArchive(ROMPath[ROMSlot_NDS], romfilename, &romdata);
            if(!romdata)
                return Load_ROMLoadError;

            bool ok = NDS::LoadROM(romdata, romlen, sramfilename, directboot);
            delete romdata;
            if(!ok)
                return Load_ROMLoadError;
        }
#endif
    }

    if (ROMPath[ROMSlot_GBA][0] != '\0')
    {
        char ext[5] = {0}; int _len = strlen(ROMPath[ROMSlot_GBA]);
        strncpy(ext, ROMPath[ROMSlot_NDS] + _len - 4, 4);

        if(!strncmp(ext, ".gba", 4))
        {
            SetupSRAMPath(1);
            if (!NDS::LoadGBAROM(ROMPath[ROMSlot_GBA], SRAMPath[ROMSlot_GBA]))
                return Load_ROMLoadError;
        }
#ifdef ARCHIVE_SUPPORT_ENABLED
        else
        {
            u8 *romdata = nullptr; u32 romlen;
            char romfilename[1024] = {0}, sramfilename[1024];
            strncpy(sramfilename, SRAMPath[ROMSlot_GBA], 1024); // Use existing SRAMPath

            int pos = strlen(sramfilename) - 1;
            while(pos > 0 && sramfilename[pos] != '/' && sramfilename[pos] != '\\')
                --pos;

            strncpy(romfilename, &sramfilename[pos + 1], 1024);
            strncpy(&romfilename[strlen(romfilename) - 3], "gba", 3);
            printf("RESET loading from archive : %s\n", romfilename);
            romlen = Archive::ExtractFileFromArchive(ROMPath[ROMSlot_GBA], romfilename, &romdata);
            if(!romdata)
                return Load_ROMLoadError;

            bool ok = NDS::LoadGBAROM(romdata, romlen, romfilename, SRAMPath[ROMSlot_GBA]);
            delete romdata;
            if(!ok)
                return Load_ROMLoadError;
        }
#endif
    }

    LoadCheats();

    return Load_OK;
}


// SAVESTATE TODO
// * configurable paths. not everyone wants their ROM directory to be polluted, I guess.

void GetSavestateName(int slot, char* filename, int len)
{
    int pos;

    if (ROMPath[ROMSlot_NDS][0] == '\0') // running firmware, no ROM
    {
        strcpy(filename, "firmware");
        pos = 8;
    }
    else
    {
        char *rompath;
        char ext[5] = {0}; int _len = strlen(ROMPath[ROMSlot_NDS]);
        strncpy(ext, ROMPath[ROMSlot_NDS] + _len - 4, 4);

        if(!strncmp(ext, ".nds", 4) || !strncmp(ext, ".srl", 4) || !strncmp(ext, ".dsi", 4))
            rompath = ROMPath[ROMSlot_NDS];
        else
            rompath = SRAMPath[ROMSlot_NDS]; // If archive, construct ssname from sram file

        int l = strlen(rompath);
        pos = l;
        while (rompath[pos] != '.' && pos > 0) pos--;
        if (pos == 0) pos = l;

        // avoid buffer overflow. shoddy
        if (pos > len-5) pos = len-5;

        strncpy(&filename[0], rompath, pos);
    }
    strcpy(&filename[pos], ".ml");
    filename[pos+3] = '0'+slot;
    filename[pos+4] = '\0';
}

bool SavestateExists(int slot)
{
    char ssfile[1024];
    GetSavestateName(slot, ssfile, 1024);
    return Platform::FileExists(ssfile);
}

bool LoadState(const char* filename)
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
        if (Config::SavestateRelocSRAM && ROMPath[ROMSlot_NDS][0]!='\0')
        {
            strncpy(PrevSRAMPath[ROMSlot_NDS], SRAMPath[0], 1024);

            strncpy(SRAMPath[ROMSlot_NDS], filename, 1019);
            int len = strlen(SRAMPath[ROMSlot_NDS]);
            strcpy(&SRAMPath[ROMSlot_NDS][len], ".sav");
            SRAMPath[ROMSlot_NDS][len+4] = '\0';

            NDS::RelocateSave(SRAMPath[ROMSlot_NDS], false);
        }

        bool loadedPartialGBAROM = false;

        // in case we have a GBA cart inserted, and the GBA ROM changes
        // due to having loaded a save state, we do not want to reload
        // the previous cartridge on reset, or commit writes to any
        // loaded save file. therefore, their paths are "nulled".
        if (GBACart::CartInserted && GBACart::CartCRC != oldGBACartCRC)
        {
            ROMPath[ROMSlot_GBA][0] = '\0';
            SRAMPath[ROMSlot_GBA][0] = '\0';
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

bool SaveState(const char* filename)
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

        if (Config::SavestateRelocSRAM && ROMPath[ROMSlot_NDS][0]!='\0')
        {
            strncpy(SRAMPath[ROMSlot_NDS], filename, 1019);
            int len = strlen(SRAMPath[ROMSlot_NDS]);
            strcpy(&SRAMPath[ROMSlot_NDS][len], ".sav");
            SRAMPath[ROMSlot_NDS][len+4] = '\0';

            NDS::RelocateSave(SRAMPath[ROMSlot_NDS], true);
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

    if (ROMPath[ROMSlot_NDS][0]!='\0')
    {
        strncpy(SRAMPath[ROMSlot_NDS], PrevSRAMPath[ROMSlot_NDS], 1024);
        NDS::RelocateSave(SRAMPath[ROMSlot_NDS], false);
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
