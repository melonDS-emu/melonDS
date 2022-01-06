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
        std::transform(ext.begin(), ext.end(), ext.begin(), tolower);

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

//            NDS::RelocateSave(SRAMPath[ROMSlot_NDS].c_str(), false);
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

     //       NDS::RelocateSave(SRAMPath[ROMSlot_NDS].c_str(), true);
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
//        NDS::RelocateSave(SRAMPath[ROMSlot_NDS].c_str(), false);
    }
}

int ImportSRAM(const char* filename)
{
    /*FILE* file = fopen(filename, "rb");
    fseek(file, 0, SEEK_END);
    u32 size = ftell(file);
    u8* importData = new u8[size];
    rewind(file);
    fread(importData, size, 1, file);
    fclose(file);

    int diff = NDS::ImportSRAM(importData, size);
    delete[] importData;
    return diff;*/
    return 0;
}

}
