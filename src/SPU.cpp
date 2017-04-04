/*
    Copyright 2016-2017 StapleButter

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
#include "NDS.h"
#include "SPU.h"


namespace SPU
{

//SDL_AudioDeviceID device;
//


bool Init()
{
    /*SDL_AudioSpec whatIwant, whatIget;

    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = 32824; // 32823.6328125
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 2;
    whatIwant.samples = 2048;
    whatIwant.callback = zorp;
    device = SDL_OpenAudioDevice(NULL, 0, &whatIwant, &whatIget, 0);
    if (!device)
    {
        printf("Audio init failed: %s\n", SDL_GetError());
        return false;
    }*/

    return true;
}

void DeInit()
{
    //if (device) SDL_CloseAudioDevice(device);
}

void Reset()
{
    //
}

}
