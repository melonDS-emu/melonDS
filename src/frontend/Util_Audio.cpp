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
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "FrontendUtil.h"

#include "NDS.h"

#include "mic_blow.h"


namespace Frontend
{

int AudioOut_Freq;
float AudioOut_SampleFrac;

s16* MicBuffer;
u32 MicBufferLength;
u32 MicBufferReadPos;


void Init_Audio(int outputfreq)
{
    AudioOut_Freq = outputfreq;
    AudioOut_SampleFrac = 0;

    MicBuffer = nullptr;
    MicBufferLength = 0;
    MicBufferReadPos = 0;
}


int AudioOut_GetNumSamples(int outlen)
{
    float f_len_in = (outlen * 32823.6328125) / (float)AudioOut_Freq;
    f_len_in += AudioOut_SampleFrac;
    int len_in = (int)floor(f_len_in);
    AudioOut_SampleFrac = f_len_in - len_in;

    return len_in;
}

void AudioOut_Resample(s16* inbuf, int inlen, s16* outbuf, int outlen, int volume)
{
    float res_incr = inlen / (float)outlen;
    float res_timer = 0;
    int res_pos = 0;

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


void Mic_FeedSilence()
{
    MicBufferReadPos = 0;
    NDS::MicInputFrame(NULL, 0);
}

void Mic_FeedNoise()
{
    int sample_len = sizeof(mic_blow) / sizeof(u16);
    static int sample_pos = 0;

    s16 tmp[735];

    for (int i = 0; i < 735; i++)
    {
        tmp[i] = mic_blow[sample_pos];
        sample_pos++;
        if (sample_pos >= sample_len) sample_pos = 0;
    }

    NDS::MicInputFrame(tmp, 735);
}

void Mic_FeedExternalBuffer()
{
    if (!MicBuffer) return Mic_FeedSilence();

    if ((MicBufferReadPos + 735) > MicBufferLength)
    {
        s16 tmp[735];
        u32 len1 = MicBufferLength - MicBufferReadPos;
        memcpy(&tmp[0], &MicBuffer[MicBufferReadPos], len1*sizeof(s16));
        memcpy(&tmp[len1], &MicBuffer[0], (735 - len1)*sizeof(s16));

        NDS::MicInputFrame(tmp, 735);
        MicBufferReadPos = 735 - len1;
    }
    else
    {
        NDS::MicInputFrame(&MicBuffer[MicBufferReadPos], 735);
        MicBufferReadPos += 735;
    }
}

void Mic_SetExternalBuffer(s16* buffer, u32 len)
{
    MicBuffer = buffer;
    MicBufferLength = len;
    MicBufferReadPos = 0;
}

}
