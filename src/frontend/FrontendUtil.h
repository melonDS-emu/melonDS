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

#ifndef FRONTENDUTIL_H
#define FRONTENDUTIL_H

#include "types.h"

namespace Frontend
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

    // TODO: more precise errors for ROM loading
    Load_ROMLoadError,
};

extern char ROMPath [ROMSlot_MAX][1024];
extern char SRAMPath[ROMSlot_MAX][1024];
extern bool SavestateLoaded;


// initialize the ROM handling utility
void Init_ROM();

// load the BIOS/firmware and boot from it
int LoadBIOS();

// load a ROM file to the specified cart slot
// note: loading a ROM to the NDS slot resets emulation
int LoadROM(const char* file, int slot);

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


// initialize the audio utility
void Init_Audio(int outputfreq);

// get how many samples to read from the core audio output
// based on how many are needed by the frontend (outlen in samples)
int AudioOut_GetNumSamples(int outlen);

// resample audio from the core audio output to match the frontend's
// output frequency, and apply user-specified volume
// note: this assumes the output buffer is interleaved stereo
void AudioOut_Resample(s16* inbuf, int inlen, s16* outbuf, int outlen);

}

#endif // FRONTENDUTIL_H
