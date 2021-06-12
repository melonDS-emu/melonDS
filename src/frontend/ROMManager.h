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

#pragma once

#include "FrontendUtil.h"

#include <array>
#include <memory>

class ARCodeFile;

namespace Frontend
{

class ROMManager {
  public:
    static ROMManager& Instance()
    {
        static ROMManager singleton;
        return singleton;
    }
    ROMManager(const ROMManager&) = delete;
    ROMManager& operator=(const ROMManager&) = delete;
    ROMManager(const ROMManager&&) = delete;
    ROMManager& operator=(const ROMManager&&) = delete;

    ARCodeFile* CheatFilePtr() { return CheatFile.get(); }

    // load the BIOS/firmware and boot from it
    int LoadBIOS();

    // load a ROM file to the specified cart slot
    // note: loading a ROM to the NDS slot resets emulation
    int LoadROM(const char* file, int slot);
    int LoadROM(const u8 *romdata, u32 romlength, const char *archivefilename, const char *romfilename, const char *sramfilename, int slot);

    // unload the ROM loaded in the specified cart slot
    // simulating ejection of the cartridge
    void UnloadROM(int slot);

    // reset execution of the current ROM
    int Reset();

    // get the filename associated with the given savestate slot (1-8)
    void GetSavestateName(int slot, char* filename, int len);

    // determine whether the given savestate slot does contain a savestate
    bool SavestateExists(int slot);

    // load the given savestate file
    // if successful, emulation will continue from the savestate's point
    bool LoadState(const char* filename);

    // save the current emulator state to the given file
    bool SaveState(const char* filename);

    // undo the latest savestate load
    void UndoStateLoad();

    // imports savedata from an external file. Returns the difference between the filesize and the SRAM size
    int ImportSRAM(const char* filename);

    // enable or disable cheats
    void EnableCheats(bool enable);

    // Stores type of nds rom i.e. nds/srl/dsi. Should be updated everytime an NDS rom is loaded from an archive
    char NDSROMExtension[4];

  private:
    ROMManager();
    ~ROMManager() = default;

    void SetupSRAMPath(int slot);
    int VerifyDSBIOS();
    int VerifyDSiBIOS();
    int VerifyDSFirmware();
    int VerifyDSiFirmware();
    int SetupDSiNAND();
    void LoadCheats();

    std::array<std::array<char, 1024>, ROMSlot_MAX> ROMPath;
    std::array<std::array<char, 1024>, ROMSlot_MAX> SRAMPath;
    std::array<std::array<char, 1024>, ROMSlot_MAX> PrevSRAMPath; // for savestate 'undo load'
    bool SavestateLoaded{false};
    std::unique_ptr<ARCodeFile> CheatFile;
    bool CheatsOn{false};
};

}
