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

#include "FrontendUtil.h"
#include "Config.h"
#include "qt_sdl/PlatformConfig.h" // FIXME!!!
#include "Platform.h"

#include "NDS.h"
#include "GBACart.h"


namespace Frontend
{

char ROMPath     [ROMSlot_MAX][1024];
char SRAMPath    [ROMSlot_MAX][1024];
char PrevSRAMPath[ROMSlot_MAX][1024]; // for savestate 'undo load'

bool SavestateLoaded;


void Init_ROM()
{
    SavestateLoaded = false;

    memset(ROMPath[ROMSlot_NDS], 0, 1024);
    memset(ROMPath[ROMSlot_GBA], 0, 1024);
    memset(SRAMPath[ROMSlot_NDS], 0, 1024);
    memset(SRAMPath[ROMSlot_GBA], 0, 1024);
    memset(PrevSRAMPath[ROMSlot_NDS], 0, 1024);
    memset(PrevSRAMPath[ROMSlot_GBA], 0, 1024);
}

void SetupSRAMPath(int slot)
{
    strncpy(SRAMPath[slot], ROMPath[slot], 1023);
    SRAMPath[slot][1023] = '\0';
    strncpy(SRAMPath[slot] + strlen(ROMPath[slot]) - 3, "sav", 3);
}

bool LoadROM(const char* file, int slot)
{
    char oldpath[1024];
    char oldsram[1024];
    strncpy(oldpath, ROMPath[slot], 1024);
    strncpy(oldsram, SRAMPath[slot], 1024);

    strncpy(ROMPath[slot], file, 1023);
    ROMPath[slot][1023] = '\0';

    SetupSRAMPath(0);
    SetupSRAMPath(1);

    if (slot == ROMSlot_NDS && NDS::LoadROM(ROMPath[slot], SRAMPath[slot], Config::DirectBoot))
    {
        SavestateLoaded = false;

        // Reload the inserted GBA cartridge (if any)
        if (ROMPath[ROMSlot_GBA][0] != '\0') NDS::LoadGBAROM(ROMPath[ROMSlot_GBA], SRAMPath[ROMSlot_GBA]);

        strncpy(PrevSRAMPath[slot], SRAMPath[slot], 1024); // safety
        return true;
    }
    else if (slot == ROMSlot_GBA && NDS::LoadGBAROM(ROMPath[slot], SRAMPath[slot]))
    {
        SavestateLoaded = false;

        strncpy(PrevSRAMPath[slot], SRAMPath[slot], 1024); // safety
        return true;
    }
    else
    {
        strncpy(ROMPath[slot], oldpath, 1024);
        strncpy(SRAMPath[slot], oldsram, 1024);
        return false;
    }
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
        int l = strlen(ROMPath[ROMSlot_NDS]);
        pos = l;
        while (ROMPath[ROMSlot_NDS][pos] != '.' && pos > 0) pos--;
        if (pos == 0) pos = l;

        // avoid buffer overflow. shoddy
        if (pos > len-5) pos = len-5;

        strncpy(&filename[0], ROMPath[ROMSlot_NDS], pos);
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

    /*char msg[64];
    if (slot > 0) sprintf(msg, "State saved to slot %d", slot);
    else          sprintf(msg, "State saved to file");
    OSD::AddMessage(0, msg);*/
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

    //OSD::AddMessage(0, "State load undone");
}

}
