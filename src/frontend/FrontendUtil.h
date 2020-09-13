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

    Load_DSiBIOS9Missing,
    Load_DSiBIOS9Bad,

    Load_DSiBIOS7Missing,
    Load_DSiBIOS7Bad,

    Load_DSiNANDMissing,
    Load_DSiNANDBad,

    // TODO: more precise errors for ROM loading
    Load_ROMLoadError,
};

extern char ROMPath [ROMSlot_MAX][1024];
extern char SRAMPath[ROMSlot_MAX][1024];
extern bool SavestateLoaded;


// initialize the ROM handling utility
void Init_ROM();

// deinitialize the ROM handling utility
void DeInit_ROM();

// load the BIOS/firmware and boot from it
int LoadBIOS();

// load a ROM file to the specified cart slot
// note: loading a ROM to the NDS slot resets emulation
int LoadROM(const char* file, int slot);

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


// setup the display layout based on the provided display size and parameters
// * screenWidth/screenHeight: size of the host display
// * screenLayout: how the DS screens are laid out
//     0 = natural (top screen above bottom screen always)
//     1 = vertical
//     2 = horizontal
// * rotation: angle at which the DS screens are presented: 0/1/2/3 = 0/90/180/270
// * sizing: how the display size is shared between the two screens
//     0 = even (both screens get same size)
//     1 = emphasize top screen (make top screen as big as possible, fit bottom screen in remaining space)
//     2 = emphasize bottom screen
// * screenGap: size of the gap between the two screens
// * integerScale: force screens to be scaled up at integer scaling factors
void SetupScreenLayout(int screenWidth, int screenHeight, int screenLayout, int rotation, int sizing, int screenGap, bool integerScale);

// get a 2x3 transform matrix for each screen
// note: the transform assumes an origin point at the top left of the display,
// X going left and Y going down
// for each screen the source coordinates should be (0,0) and (256,192)
// 'top' and 'bot' should point each to an array of 6 floats
void GetScreenTransforms(float* top, float* bot);

// de-transform the provided host display coordinates to get coordinates
// on the bottom screen
void GetTouchCoords(int& x, int& y);


// initialize the audio utility
void Init_Audio(int outputfreq);

// get how many samples to read from the core audio output
// based on how many are needed by the frontend (outlen in samples)
int AudioOut_GetNumSamples(int outlen);

// resample audio from the core audio output to match the frontend's
// output frequency, and apply specified volume
// note: this assumes the output buffer is interleaved stereo
void AudioOut_Resample(s16* inbuf, int inlen, s16* outbuf, int outlen, int volume);

// feed silence to the microphone input
void Mic_FeedSilence();

// feed random noise to the microphone input
void Mic_FeedNoise();

// feed an external buffer to the microphone input
// buffer should be mono
void Mic_FeedExternalBuffer();
void Mic_SetExternalBuffer(s16* buffer, u32 len);

}

#endif // FRONTENDUTIL_H
