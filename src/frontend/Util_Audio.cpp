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
#include <math.h>

#include "FrontendUtil.h"
#include "Config.h"
#include "qt_sdl/PlatformConfig.h" // FIXME!!!
#include "Platform.h"

#include "NDS.h"
#include "GBACart.h"


namespace Frontend
{

int AudioOut_Freq;
float AudioOut_SampleFrac;


void Init_Audio(int outputfreq)
{
    AudioOut_Freq = outputfreq;
    AudioOut_SampleFrac = 0;
}

int AudioOut_GetNumSamples(int outlen)
{
    float f_len_in = (outlen * 32823.6328125) / (float)AudioOut_Freq;
    f_len_in += AudioOut_SampleFrac;
    int len_in = (int)floor(f_len_in);
    AudioOut_SampleFrac = f_len_in - len_in;

    return len_in;
}

void AudioOut_Resample(s16* inbuf, int inlen, s16* outbuf, int outlen)
{
    float res_incr = inlen / (float)outlen;
    float res_timer = 0;
    int res_pos = 0;

    int volume = Config::AudioVolume;

    for (int i = 0; i < outlen; i++)
    {
        outbuf[i*2  ] = (inbuf[res_pos*2  ] * volume) >> 8;
        outbuf[i*2+1] = (inbuf[res_pos*2+1] * volume) >> 8;

        res_timer += res_incr;
        while (res_timer >= 1.0)
        {
            res_timer -= 1.0;
            res_pos++;
        }
    }
}

}
