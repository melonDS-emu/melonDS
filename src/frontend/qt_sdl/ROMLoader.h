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

#ifndef ROMLOADER_H
#define ROMLOADER_H

#include "types.h"

#include <string>
#include <vector>

namespace ROMLoader
{

enum
{
    ROMSlot_NDS = 0,
    ROMSlot_GBA,

    ROMSlot_MAX
};

enum
{
    Load_OK = 0,

    Load_BIOS9Missing,
    Load_BIOS9Bad,

    Load_BIOS7Missing,
    Load_BIOS7Bad,

    Load_FirmwareMissing,
    Load_FirmwareBad,
    Load_FirmwareNotBootable,

    Load_DSiBIOS9Missing,
    Load_DSiBIOS9Bad,

    Load_DSiBIOS7Missing,
    Load_DSiBIOS7Bad,

    Load_DSiNANDMissing,
    Load_DSiNANDBad,

    // TODO: more precise errors for ROM loading
    Load_ROMLoadError,
};

extern std::string ROMPath [ROMSlot_MAX];
extern std::string SRAMPath[ROMSlot_MAX];
extern bool SavestateLoaded;

// Stores type of nds rom i.e. nds/srl/dsi. Should be updated everytime an NDS rom is loaded from an archive
extern std::string NDSROMExtension;

// initialize the ROM handling utility
void Init_ROM();

// deinitialize the ROM handling utility
void DeInit_ROM();

// load the BIOS/firmware and boot from it
int LoadBIOS();

// load a ROM file to the specified cart slot
// note: loading a ROM to the NDS slot resets emulation
int LoadROM(const char* file, int slot);
int LoadROM(const u8 *romdata, u32 romlength, const char *archivefilename, const char *romfilename, const char *sramfilename, int slot);

// unload the ROM loaded in the specified cart slot
// simulating ejection of the cartridge
void UnloadROM(int slot);

void ROMIcon(u8 (&data)[512], u16 (&palette)[16], u32* iconRef);
void AnimatedROMIcon(u8 (&data)[8][512], u16 (&palette)[8][16], u16 (&sequence)[64], u32 (&animatedTexRef)[32 * 32 * 64], std::vector<int> &animatedSequenceRef);

// reset execution of the current ROM
int Reset();

// get the filename associated with the given savestate slot (1-8)
std::string GetSavestateName(int slot);

// determine whether the given savestate slot does contain a savestate
bool SavestateExists(int slot);

// load the given savestate file
// if successful, emulation will continue from the savestate's point
bool LoadState(std::string filename);

// save the current emulator state to the given file
bool SaveState(std::string filename);

// undo the latest savestate load
void UndoStateLoad();

// imports savedata from an external file. Returns the difference between the filesize and the SRAM size
int ImportSRAM(const char* filename);

// enable or disable cheats
void EnableCheats(bool enable);



}

#endif // ROMLOADER_H
