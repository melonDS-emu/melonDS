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

#ifndef FRONTENDUTIL_H
#define FRONTENDUTIL_H

#include "types.h"

#include <string>
#include <vector>

namespace Frontend
{

// setup the display layout based on the provided display size and parameters
// * screenWidth/screenHeight: size of the host display
// * screenLayout: how the DS screens are laid out
//     0 = natural (top screen above bottom screen always)
//     1 = vertical
//     2 = horizontal
//     3 = hybrid
//     4 = flipped hybrid
// * rotation: angle at which the DS screens are presented: 0/1/2/3 = 0/90/180/270
// * sizing: how the display size is shared between the two screens
//     0 = even (both screens get same size)
//     1 = emphasize top screen (make top screen as big as possible, fit bottom screen in remaining space)
//     2 = emphasize bottom screen
//     4 = top only
//     5 = bottom only
// * screenGap: size of the gap between the two screens
// * integerScale: force screens to be scaled up at integer scaling factors
// * screenSwap: whether to swap the position of both screens
// * screenFocus: whether to swap the large screen in hybrid layout
// * topAspect/botAspect: ratio by which to scale the top and bottom screen respectively
void SetupScreenLayout(int screenWidth, int screenHeight,
    int screenLayout,
    int rotation,
    int sizing,
    int screenGap,
    bool integerScale,
    bool swapScreens,
    bool focusScreens,
    float topAspect, float botAspect);

const int MaxScreenTransforms = 3;

// get a 2x3 transform matrix for each screen and whether it's a top or bottom screen
// note: the transform assumes an origin point at the top left of the display,
// X going right and Y going down
// for each screen the source coordinates should be (0,0) and (256,192)
// 'out' should point to an array of 6*MaxScreenTransforms floats
// 'kind' should point to an array of MaxScreenTransforms ints
// (0 = indicates top screen, 1 = bottom screen)
// returns the amount of screens
int GetScreenTransforms(float* out, int* kind);

// de-transform the provided host display coordinates to get coordinates
// on the bottom screen
bool GetTouchCoords(int& x, int& y, bool clamp);


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
